/* Class TorcMediaMaster
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torcevent.h"
#include "torcmediamaster.h"

TorcMediaMaster *gTorcMediaMaster = NULL;

TorcMediaMaster::TorcMediaMaster()
  : QAbstractListModel()
{
}

TorcMediaMaster::~TorcMediaMaster()
{
    while (!m_media.isEmpty())
        delete m_media.takeLast();
}

QVariant TorcMediaMaster::data(const QModelIndex &Index, int Role) const
{
    if (!Index.isValid())
        return QVariant();

    int position = Index.row();

    if (position < 0 || position >= m_media.size() || Role != Qt::DisplayRole)
        return QVariant();

    return QVariant::fromValue(m_media.at(position));
}

TorcMedia* TorcMediaMaster::GetChildByIndex(int Index) const
{
    if (Index < 0 || Index >= m_media.size())
        return NULL;

    return m_media.at(Index);
}

QHash<int,QByteArray> TorcMediaMaster::roleNames(void) const
{
    QHash<int,QByteArray> roles;
    roles.insert(Qt::DisplayRole, "name");
    roles.insert(Qt::DisplayRole, "url");
    roles.insert(Qt::DisplayRole, "type");
    roles.insert(Qt::DisplayRole, "source");
    roles.insert(Qt::DisplayRole, "metadata");
    return roles;
}

int TorcMediaMaster::rowCount(const QModelIndex &Parent) const
{
    return m_media.size();
}

bool TorcMediaMaster::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent *event = static_cast<TorcEvent*>(Event);

        if (event && event->GetEvent() == Torc::MediaAdded)
        {
            if (event->Data().contains("files"))
            {
                QVariantList items = static_cast<QVariantList>(event->Data().value("files").toList());

                if (!items.isEmpty())
                {
                    QList<TorcMedia*> newfiles;

                    for (int i = 0; i < items.size(); ++i)
                    {
                        TorcMediaDescription media = items[i].value<TorcMediaDescription>();

                        if (!m_mediaMap.contains(media.url))
                        {
                            TorcMedia* newmedia = new TorcMedia(media.name, media.url, media.type, media.source, media.metadata);
                            newfiles.append(newmedia);
                            m_mediaMap.insert(media.url, newmedia);
                        }
                    }

                    if (!newfiles.isEmpty())
                    {
                        beginInsertRows(QModelIndex(), m_media.size(), m_media.size() + newfiles.size() - 1);
                        m_media.append(newfiles);
                        endInsertRows();
                    }
                }
            }
        }
        else if (event && event->GetEvent() == Torc::MediaRemoved)
        {
            if (event->Data().contains("files"))
            {
                QVariantList items = event->Data().value("files").toList();

                if (!items.isEmpty())
                {
                    for (int i = 0; i < items.size(); ++i)
                    {
                        TorcMediaDescription media = items[i].value<TorcMediaDescription>();

                        if (m_mediaMap.contains(media.url))
                        {
                            TorcMedia *oldmedia = m_mediaMap.take(media.url);
                            int position = m_media.indexOf(oldmedia);
                            beginRemoveRows(QModelIndex(), position, position);
                            m_media.removeAt(position);
                            endRemoveRows();
                        }
                    }
                }
            }
        }
    }

    return false;
}

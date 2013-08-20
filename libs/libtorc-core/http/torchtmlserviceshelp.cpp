/* Class TorcHTMLServicesHelp
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
#include <QCoreApplication>
#include <QObject>

// Torc
#include "torclocalcontext.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlserviceshelp.h"

TorcHTMLServicesHelp::TorcHTMLServicesHelp(TorcHTTPServer *Server)
  : TorcHTTPService(this, "", tr("Services"), TorcHTMLServicesHelp::staticMetaObject),
    m_server(Server)
{
}

void TorcHTMLServicesHelp::ProcessHTTPRequest(TorcHTTPServer* Server, TorcHTTPRequest *Request, TorcHTTPConnection* Connection)
{
    if (!Request || !Server)
        return;

    // handle own service
    if (!Request->GetMethod().isEmpty())
    {
        TorcHTTPService::ProcessHTTPRequest(Server, Request, Connection);
        return;
    }

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseDefault);
        Request->SetResponseContent(NULL);
        return;
    }

    QByteArray *result = new QByteArray();
    QTextStream stream(result);

    QMap<QString,QString> services = Server->GetServiceHandlers();

    stream << "<html><head><title>" << QCoreApplication::applicationName() << "</title></head>";
    stream << "<body><h1><a href='/'>" << QCoreApplication::applicationName();
    stream << "<a> " << m_name << "</a></h1>";

    if (services.isEmpty())
    {
        stream << "<h3>" << QObject::tr("No services are registered") << "</h3>";
    }
    else
    {
        stream << "<h3>" << QObject::tr("Available services") << "</h3>";
        QMap<QString,QString>::iterator it = services.begin();
        for ( ; it != services.end(); ++it)
            stream << it.value() << " <a href='" << it.key() + "help" << "'>" << it.key() << "</a><br>";
    }

    stream << "</body></html>";
    stream.flush();
    Request->SetStatus(HTTP_OK);
    Request->SetResponseType(HTTPResponseHTML);
    Request->SetResponseContent(result);
}

QVariantMap TorcHTMLServicesHelp::GetServiceList(void)
{
    QVariantMap results;

    QMap<QString,QString> services =  m_server->GetServiceHandlers();
    QMap<QString,QString>::const_iterator it = services.begin();
    for ( ; it != services.end(); ++it)
        results.insert(it.value(), QVariant(it.key()));

    return results;
}

qint64 TorcHTMLServicesHelp::GetStartTime(void)
{
    return gLocalContext->GetStartTime();
}

int TorcHTMLServicesHelp::GetPriority(void)
{
    return gLocalContext->GetPriority();
}

QString TorcHTMLServicesHelp::GetUuid(void)
{
    return gLocalContext->GetUuid();
}

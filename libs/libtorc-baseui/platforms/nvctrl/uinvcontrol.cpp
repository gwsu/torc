/* UINVControl
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
* You should have received a copy of the GNU General Public License
*
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QMap>

// Torc
#include "torclogging.h"
#include "uinvcontrol.h"

extern "C" {
#include "NVCtrl.h"
#include "NVCtrlLib.h"
}

QMutex* gNVCtrlLock = new QMutex(QMutex::Recursive);
QMap<int, double> gMetaModeMap;

static QString DisplayDeviceName(int Mask)
{
    switch (Mask)
    {
    case (1 <<  0): return QString("CRT-0"); break;
    case (1 <<  1): return QString("CRT-1"); break;
    case (1 <<  2): return QString("CRT-2"); break;
    case (1 <<  3): return QString("CRT-3"); break;
    case (1 <<  4): return QString("CRT-4"); break;
    case (1 <<  5): return QString("CRT-5"); break;
    case (1 <<  6): return QString("CRT-6"); break;
    case (1 <<  7): return QString("CRT-7"); break;
    case (1 <<  8): return QString("TV-0"); break;
    case (1 <<  9): return QString("TV-1"); break;
    case (1 << 10): return QString("TV-2"); break;
    case (1 << 11): return QString("TV-3"); break;
    case (1 << 12): return QString("TV-4"); break;
    case (1 << 13): return QString("TV-5"); break;
    case (1 << 14): return QString("TV-6"); break;
    case (1 << 15): return QString("TV-7"); break;
    case (1 << 16): return QString("DFP-0"); break;
    case (1 << 17): return QString("DFP-1"); break;
    case (1 << 18): return QString("DFP-2"); break;
    case (1 << 19): return QString("DFP-3"); break;
    case (1 << 20): return QString("DFP-4"); break;
    case (1 << 21): return QString("DFP-5"); break;
    case (1 << 22): return QString("DFP-6"); break;
    case (1 << 23): return QString("DFP-7"); break;
    default:        return QString("Unknown");
    }
}

int ListDisplays(int Displays)
{
    int count = 0;

    for (int i = 1; i < (1 << 24); i <<= 1)
    {
        if (Displays & i)
        {
            count++;
            LOG(VB_GENERAL, LOG_INFO, QString("Connected display: %1").arg(DisplayDeviceName(i)));
        }
    }

    return count;
}

bool UINVControl::NVControlAvailable(Display *XDisplay)
{
    QMutexLocker locker(gNVCtrlLock);

    static bool available = false;
    static bool checked   = false;

    if (checked || !XDisplay)
        return available;

    checked = true;

    int event = 0;
    int error = 0;
    Bool ok = XNVCTRLQueryExtension(XDisplay, &event, &error);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension not available on display '%1'").arg(XDisplayName(NULL)));
        return available;
    }

    int major = 0;
    int minor = 0;
    ok = XNVCTRLQueryVersion(XDisplay, &major, &minor);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension not available on display '%1'").arg(XDisplayName(NULL)));
        return available;
    }

    available = true;

    LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension version %1.%2 available on display '%3'")
        .arg(major).arg(minor).arg(XDisplayName(NULL)));

    return available;
}

QByteArray UINVControl::GetNVEDID(Display *XDisplay, int Screen)
{
    if (!NVControlAvailable(XDisplay))
        return QByteArray();

    QMutexLocker locker(gNVCtrlLock);
    QByteArray result;

    if (!XNVCTRLIsNvScreen(XDisplay, Screen))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("NV-CONTROL is not available on screen %1 of display '%2'")
            .arg(Screen).arg(XDisplayName(NULL)));
        return result;
    }

    int displays = 0;
    Bool ok = XNVCTRLQueryAttribute(XDisplay, Screen, 0, NV_CTRL_CONNECTED_DISPLAYS, &displays);
    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve display list");
        return result;
    }

    int displaycount = ListDisplays(displays);
    if (displaycount != 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, "There is more than one physical display attached to this screen. Ignoring EDID");
        return result;
    }

    int edid = NV_CTRL_EDID_AVAILABLE_FALSE;
    ok = XNVCTRLQueryAttribute(XDisplay, Screen, displays, NV_CTRL_EDID_AVAILABLE, &edid);
    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to check EDID_AVAILABLE attribute");
        return result;
    }

    if (edid != NV_CTRL_EDID_AVAILABLE_TRUE)
    {
        LOG(VB_GENERAL, LOG_INFO, "EDID not available");
        return result;
    }

    unsigned char* data = NULL;
    int datalength = 0;

    ok = XNVCTRLQueryBinaryData(XDisplay, Screen, displays, NV_CTRL_BINARY_DATA_EDID, &data, &datalength);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("EDID not available on screen %1 of display '%2'")
                .arg(Screen).arg(XDisplayName(NULL)));
        return result;
    }

    result = QByteArray((const char*)data, datalength);
    delete data;
    return result;
}

void UINVControl::InitialiseMetaModes(Display *XDisplay, int Screen)
{
    if (!NVControlAvailable(XDisplay))
        return;

    QMutexLocker locker(gNVCtrlLock);

    gMetaModeMap.clear();

    if (!XNVCTRLIsNvScreen(XDisplay, Screen))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("NV-CONTROL is not available on screen %1 of display '%2'")
            .arg(Screen).arg(XDisplayName(NULL)));
        return;
    }

    int displays = 0;
    Bool ok = XNVCTRLQueryAttribute(XDisplay, Screen, 0, NV_CTRL_CONNECTED_DISPLAYS, &displays);
    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve display list");
        return;
    }

    int displaycount = ListDisplays(displays);
    if (displaycount != 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, "There is more than one physical display attached to this screen. Ignoring metamodes");
        return result;
    }

    // retrieve a list of refresh rates by mode name
    QMap<QString,double> rates;

    unsigned char* modelines = NULL;
    int modelinelength = 0;
    if (XNVCTRLQueryBinaryData(XDisplay, Screen, displays, NV_CTRL_BINARY_DATA_MODELINES, &modelines, &modelinelength))
    {
        QByteArray data((const char*)modelines, modelinelength);
        QList<QByteArray> lines = data.split('\0');
        foreach (QByteArray line, lines)
        {
            QString modeline = QString::fromLatin1(line.data()).simplified();

            LOG(VB_GUI, LOG_DEBUG, QString("Modeline: %1").arg(modeline);

            QStringList parts = modeline.split("::", QString::SkipEmptyParts);
            if (parts.size() < 1)
                continue;

            modeline = parts.last();
            parts = modeline.split(" ", QString::SkipEmptyParts);
            if (parts.size() < 10)
                continue;

            QString name = parts[0].replace("\"", "");
            double rate = parts[5].toInt() * parts[9].toInt();
            double clock = parts[1].toDouble();
            if (rate > 0.0 && clock > 0.0f)
            {
                rate = (clock * 1000000) / rate;
                if (modeline.contains("interlace", Qt::CaseInsensitive))
                    rate *= 2.0f;
                if (rate > 20.0f && rate < 121.0f)
                    rates.insert(name, rate);
            }
        }
    }

    if (rates.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to parse any valid modelines");
        return;
    }

    // retrieve the list of metamodes
    unsigned char* metamodes = NULL;
    int metamodelength = 0;
    if (XNVCTRLQueryBinaryData(XDisplay, Screen, 0, NV_CTRL_BINARY_DATA_METAMODES, &metamodes, &metamodelength))
    {
        QByteArray data((const char*)metamodes, metamodelength);
        QList<QByteArray> lines = data.split('\0');
        foreach (QByteArray line, lines)
        {
            QString metamode = QString::fromLatin1(line.data());

            LOG(VB_GUI, LOG_DEBUG, QString("Metamode: %1").arg(metamode));

            QStringList parts = metamode.split("::", QString::SkipEmptyParts);
            if (parts.size() < 2)
                continue;

            QStringList ids = parts[0].split(",");
            if (ids.size() < 1)
                continue;

            QStringList rates = ids[0].split("=");
            if (rates.size() < 2)
                continue;

            if (rates[0].trimmed().toLower() != "id")
                continue;

            int rate = rates[1].toInt();
            if (rate <= 0)
                continue;

            QStringList descriptions = parts[1].split(" ", QString::SkipEmptyParts);
            if (descriptions.size() < 2)
                continue;

            QString name = descriptions[1].trimmed();

            // match the name to a known real rate
            if (rates.contains(name))
                gMetaModeMap.insert(rate, rates.value(name));
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Found %1 metamode rates").arg(gMetaModeMap.size()));

    if (gLogLevel & LOG_DEBUG)
    {
        QMap<int,double>::iterator it = gMetaModeMap.begin();
        for (int i = 1; ; it != gMetaModeMap.end(); ++it, ++i)
            LOG(VB_GUI, LOG_DEBUG, QString("Metamode #%1: metarate %2 real rate %3").arg(i).arg(it.key()).arg(it.value()));
    }
}

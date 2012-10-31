/* Class TorcHTTPRequest
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
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QTextStream>
#include <QStringList>
#include <QDateTime>
#include <QRegExp>
#include <QUrl>

// Torc
#include "version.h"
#include "torclogging.h"
#include "torchttpserver.h"
#include "torchttprequest.h"

/*! \class TorcHTTPRequest
 *  \brief A class to encapsulte an incoming HTTP request.
 *
 * TorcHTTPRequest validates an incoming HTTP request and prepares the appropriate
 * headers for the response.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
*/

QRegExp gRegExp = QRegExp("[ \r\n][ \r\n]*");

TorcHTTPRequest::TorcHTTPRequest(const QString &Method, QMap<QString,QString> *Headers, QByteArray *Content)
  : m_type(HTTPRequest),
    m_requestType(HTTPUnknownType),
    m_protocol(HTTPUnknownProtocol),
    m_keepAlive(false),
    m_headers(Headers),
    m_content(Content),
    m_responseType(HTTPResponseUnknown),
    m_responseStatus(HTTP_NotFound),
    m_responseContent(NULL)
{
    QStringList items = Method.split(gRegExp, QString::SkipEmptyParts);
    QString item;

    if (!items.isEmpty())
    {
        item = items.takeFirst();

        if (item == "HTTP/")
        {
            m_type = HTTPResponse;
        }
        else
        {
            m_type = HTTPRequest;
            m_requestType = RequestTypeFromString(item);
        }
    }

    if (!items.isEmpty())
    {
        QUrl url  = QUrl::fromEncoded(items.takeFirst().toUtf8());
        m_path    = url.path();
        m_fullUrl = url.toString();

        int index = m_path.lastIndexOf("/");
        if (index > -1)
        {
            m_method = m_path.mid(index + 1);
            m_path   = m_path.left(index + 1);
        }

        if (url.hasQuery())
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            QList<QPair<QString, QString> > pairs = url.queryItems();
            for (int i = 0; i < pairs.size(); ++i)
                m_headers->insert(pairs[i].first, pairs[i].second);
#else
            QStringList pairs = url.query().split('&');
            foreach (QString pair, pairs)
            {
                int index = pair.indexOf('=');
                QString key = pair.left(index);
                QString val = pair.mid(index + 1);
                m_headers->insert(key, val);
            }
#endif
        }
    }

    if (!items.isEmpty())
        m_protocol = ProtocolFromString(items.takeFirst());

    m_keepAlive = m_protocol > HTTPOneDotZero;

    QString connection = m_headers->value("connection").toLower();

    if (connection == "keep-alive")
        m_keepAlive = true;
    else if (connection == "close")
        m_keepAlive = false;
}

TorcHTTPRequest::~TorcHTTPRequest()
{
    delete m_headers;
    delete m_content;
    delete m_responseContent;
}

bool TorcHTTPRequest::KeepAlive(void)
{
    return m_keepAlive;
}

void TorcHTTPRequest::SetStatus(HTTPStatus Status)
{
    m_responseStatus = Status;
}

void TorcHTTPRequest::SetResponseType(HTTPResponseType Type)
{
    m_responseType = Type;
}

void TorcHTTPRequest::SetResponseContent(QByteArray *Content)
{
    delete m_responseContent;
    m_responseContent = Content;
}

HTTPType TorcHTTPRequest::GetHTTPType(void)
{
    return m_type;
}

QString TorcHTTPRequest::GetPath(void)
{
    return m_path;
}

QPair<QByteArray*,QByteArray*> TorcHTTPRequest::Respond(void)
{
    if (m_responseType == HTTPResponseUnknown)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown HTTP response");
        m_responseStatus = HTTP_InternalServerError;
        m_responseType   = HTTPResponseDefault;
        m_keepAlive      = false;
    }

    QByteArray *buffer = new QByteArray();
    QTextStream response(buffer);

    QString contenttype = ResponseTypeToString(m_responseType);

    response << TorcHTTPRequest::ProtocolToString(m_protocol) << " " << TorcHTTPRequest::StatusToString(m_responseStatus) << "\r\n";
    response << "Date: " << QDateTime::currentDateTimeUtc().toString("d MMM yyyy hh:mm:ss 'GMT'") << "\r\n";
    response << "Server: " << TorcHTTPServer::PlatformName() << ", Torc " << TORC_SOURCE_VERSION << "\r\n";
    response << "Connection: " << (m_keepAlive ? QString("keep-alive") : QString("close")) << "\r\n";
    response << "Accept-Ranges: bytes\r\n";
    response << "Content-Length: " << (m_responseContent ? QString::number(m_responseContent->size()) : "0") << "\r\n";
    if (!contenttype.isEmpty())
        response << "Content-Type: " << contenttype << "\r\n";
    response << "\r\n";

    return QPair<QByteArray*,QByteArray*>(buffer, m_responseContent);
}

HTTPRequestType TorcHTTPRequest::RequestTypeFromString(const QString &Type)
{
    if (Type == "GET")  return HTTPGet;
    if (Type == "HEAD") return HTTPHead;
    if (Type == "POST") return HTTPPost;

    return HTTPUnknownType;
}

HTTPProtocol TorcHTTPRequest::ProtocolFromString(const QString &Protocol)
{
    if (Protocol.startsWith("HTTP"))
    {
        if (Protocol.endsWith("1.1")) return HTTPOneDotOne;
        if (Protocol.endsWith("1.0")) return HTTPOneDotZero;
        if (Protocol.endsWith("0.9")) return HTTPZeroDotNine;
    }

    return HTTPUnknownProtocol;
}

QString TorcHTTPRequest::ProtocolToString(HTTPProtocol Protocol)
{
    switch (Protocol)
    {
        case HTTPOneDotOne:       return QString("HTTP/1.1");
        case HTTPOneDotZero:      return QString("HTTP/1.0");
        case HTTPZeroDotNine:     return QString("HTTP/0.9");
        case HTTPUnknownProtocol: return QString("Error");
    }

    return QString("Error");
}

QString TorcHTTPRequest::StatusToString(HTTPStatus Status)
{
    switch (Status)
    {
        case HTTP_OK:                  return QString("200 OK");
        case HTTP_BadRequest:          return QString("400 Bad Request");
        case HTTP_Unauthorized:        return QString("401 Unauthorized");
        case HTTP_Forbidden:           return QString("403 Forbidden");
        case HTTP_NotFound:            return QString("404 Not Found");
        case HTTP_MethodNotAllowed:    return QString("405 Method Not Allowed");
        case HTTP_InternalServerError: return QString("500 Internal Server Error");
    }

    return QString("Error");
}

QString TorcHTTPRequest::ResponseTypeToString(HTTPResponseType Response)
{
    switch (Response)
    {
        case HTTPResponseXML:  return QString("text/xml; charset=\"UTF-8\"");
        case HTTPResponseHTML: return QString("text/html; charset=\"UTF-8\"");
        default: break;
    }

    return QString("text/plain");
}

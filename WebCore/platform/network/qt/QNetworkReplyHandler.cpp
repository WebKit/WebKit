/*
    Copyright (C) 2007 Trolltech ASA
    Copyright (C) 2007 Staikos Computing Services Inc.  <info@staikos.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#include "config.h"
#include "QNetworkReplyHandler.h"

#if QT_VERSION >= 0x040400

#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include <QNetworkReply>
#include <QNetworkCookie>
#include <qwebframe.h>
#include <qwebpage.h>

namespace WebCore {

QNetworkReplyHandler::QNetworkReplyHandler(ResourceHandle *handle)
    : QObject(0)
      , m_resourceHandle(handle)
      , m_reply(0)
      , m_redirected(false)
      , m_responseSent(false)
{
    const ResourceRequest &r = m_resourceHandle->request();

    if (r.httpMethod() == "GET")
        m_method = QNetworkAccessManager::GetOperation;
    else if (r.httpMethod() == "HEAD")
        m_method = QNetworkAccessManager::HeadOperation;
    else if (r.httpMethod() == "POST")
        m_method = QNetworkAccessManager::PostOperation;
    else if (r.httpMethod() == "PUT")
        m_method = QNetworkAccessManager::PutOperation;
    else
        m_method = QNetworkAccessManager::UnknownOperation;

    m_request = r.toNetworkRequest();

    start();
}

void QNetworkReplyHandler::abort()
{
    m_resourceHandle = 0;
    if (m_reply) {
        disconnect(m_reply, 0, this, 0);
        m_reply->abort();
        deleteLater();
        m_reply = 0;
    }
}

void QNetworkReplyHandler::finish()
{
    sendResponseIfNeeded();

    if (!m_resourceHandle)
        return;
    ResourceHandleClient* client = m_resourceHandle->client();
    m_reply->deleteLater();
    if (!client)
        return;
    if (m_redirected) {
        m_redirected = false;
        start();
    } else if (m_reply->error() != QNetworkReply::NoError) {
        QUrl url = m_reply->url();
        ResourceError error(url.host(), m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                            url.toString(), m_reply->errorString());
        client->didFail(m_resourceHandle, error);
    } else {
        client->didFinishLoading(m_resourceHandle);
    }
}

void QNetworkReplyHandler::sendResponseIfNeeded()
{
    if (m_responseSent || !m_resourceHandle)
        return;
    m_responseSent = true;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    WebCore::String contentType = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
    WebCore::String encoding = extractCharsetFromMediaType(contentType);
    WebCore::String mimeType = extractMIMETypeFromMediaType(contentType);

    if (mimeType.isEmpty()) {
        // let's try to guess from the extension
        QString extension = m_reply->url().path();
        int index = extension.lastIndexOf(QLatin1Char('.'));
        if (index > 0) {
            extension = extension.mid(index + 1);
            mimeType = MIMETypeRegistry::getMIMETypeForExtension(extension);
        }
    }

    KURL url(m_reply->url());
    String contentDisposition = QString::fromAscii(m_reply->rawHeader("Content-Disposition"));

    ResourceResponse response(url, mimeType,
                              m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(),
                              encoding,
                              filenameFromHTTPContentDisposition(contentDisposition));

    int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_reply->url().scheme() != QLatin1String("file"))
        response.setHTTPStatusCode(statusCode);
    else if (m_reply->error() == QNetworkReply::ContentNotFoundError)
        response.setHTTPStatusCode(404);


    /* Fill in the other fields */
    foreach (QByteArray headerName, m_reply->rawHeaderList())
        response.setHTTPHeaderField(QString::fromAscii(headerName), QString::fromAscii(m_reply->rawHeader(headerName)));

    QUrl redirection = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirection.isValid()) {
        QUrl newUrl = m_reply->url().resolved(redirection);
        ResourceRequest newRequest = m_resourceHandle->request();
        newRequest.setURL(newUrl);
        client->willSendRequest(m_resourceHandle, newRequest, response);

        if (statusCode >= 301 && statusCode <= 303 && m_method == QNetworkAccessManager::PostOperation)
            m_method = QNetworkAccessManager::GetOperation;
        m_redirected = true;
        m_responseSent = false;

        m_request.setUrl(newUrl);
    } else {
        client->didReceiveResponse(m_resourceHandle, response);
    }
}

void QNetworkReplyHandler::forwardData()
{
    sendResponseIfNeeded();

    // don't emit the "Document has moved here" type of HTML
    if (m_redirected)
        return;

    if (!m_resourceHandle)
        return;

    QByteArray data = m_reply->read(m_reply->bytesAvailable());

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    if (!data.isEmpty())
        client->didReceiveData(m_resourceHandle, data.constData(), data.length(), data.length() /*FixMe*/);
}

void QNetworkReplyHandler::start()
{
    ResourceHandleInternal* d = m_resourceHandle->getInternal();

    QNetworkAccessManager* manager = d->m_frame->page()->networkAccessManager();

    // Post requests on files don't really make sense, but for
    // fast/forms/form-post-urlencoded.html we still need to retrieve the file,
    // which means we map it to a Get instead.
    if (m_method == QNetworkAccessManager::PostOperation
        && !m_request.url().toLocalFile().isEmpty())
        m_method = QNetworkAccessManager::GetOperation;

    switch (m_method) {
        case QNetworkAccessManager::GetOperation:
            m_reply = manager->get(m_request);
            break;
        case QNetworkAccessManager::PostOperation: {
            Vector<char> bytes;
            if (d->m_request.httpBody())
                d->m_request.httpBody()->flatten(bytes);
            m_reply = manager->post(m_request, QByteArray(bytes.data(), bytes.size()));
            break;
        }
        case QNetworkAccessManager::HeadOperation:
            m_reply = manager->head(m_request);
            break;
        case QNetworkAccessManager::PutOperation: {
            // ### data?
            Vector<char> bytes;
            if (d->m_request.httpBody())
                d->m_request.httpBody()->flatten(bytes);
            m_reply = manager->put(m_request, QByteArray(bytes.data(), bytes.size()));
            break;
        }
        case QNetworkAccessManager::UnknownOperation:
            break; // eh?
    }

    m_reply->setParent(this);

    connect(m_reply, SIGNAL(finished()),
            this, SLOT(finish()));

    // For http(s) we know that the headers are complete upon metaDataChanged() emission, so we
    // can send the response as early as possible
    QString scheme = m_request.url().scheme();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        connect(m_reply, SIGNAL(metaDataChanged()),
                this, SLOT(sendResponseIfNeeded()));

    connect(m_reply, SIGNAL(readyRead()),
            this, SLOT(forwardData()));
}

}

#include "moc_QNetworkReplyHandler.cpp"

#endif

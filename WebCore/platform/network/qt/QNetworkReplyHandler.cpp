/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.  <info@staikos.net>
    Copyright (C) 2008 Holger Hans Peter Freyther

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
#include <QDateTime>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <qwebframe.h>
#include <qwebpage.h>

#include <QDebug>
#include <QCoreApplication>

namespace WebCore {

// Take a deep copy of the FormDataElement
FormDataIODevice::FormDataIODevice(FormData* data)
    : m_formElements(data ? data->elements() : Vector<FormDataElement>())
    , m_currentFile(0)
    , m_currentDelta(0)
{
    setOpenMode(FormDataIODevice::ReadOnly);
}

FormDataIODevice::~FormDataIODevice()
{
    delete m_currentFile;
}

void FormDataIODevice::moveToNextElement()
{
    if (m_currentFile)
        m_currentFile->close();
    m_currentDelta = 0;

    m_formElements.remove(0);

    if (m_formElements.isEmpty() || m_formElements[0].m_type == FormDataElement::data)
        return;

    if (!m_currentFile)
        m_currentFile = new QFile;

    m_currentFile->setFileName(m_formElements[0].m_filename);
    m_currentFile->open(QFile::ReadOnly);
}

// m_formElements[0] is the current item. If the destination buffer is
// big enough we are going to read from more than one FormDataElement
qint64 FormDataIODevice::readData(char* destination, qint64 size)
{
    if (m_formElements.isEmpty())
        return -1;

    qint64 copied = 0;
    while (copied < size && !m_formElements.isEmpty()) {
        const FormDataElement& element = m_formElements[0];
        const qint64 available = size-copied;

        if (element.m_type == FormDataElement::data) {
            const qint64 toCopy = qMin<qint64>(available, element.m_data.size() - m_currentDelta);
            memcpy(destination+copied, element.m_data.data()+m_currentDelta, toCopy); 
            m_currentDelta += toCopy;
            copied += toCopy;

            if (m_currentDelta == element.m_data.size())
                moveToNextElement();
        } else {
            const QByteArray data = m_currentFile->read(available);
            memcpy(destination+copied, data.constData(), data.size());
            copied += data.size();

            if (m_currentFile->atEnd() || !m_currentFile->isOpen())
                moveToNextElement();
        }
    }

    return copied;
}

qint64 FormDataIODevice::writeData(const char*, qint64)
{
    return -1;
}

void FormDataIODevice::setParent(QNetworkReply* reply)
{
    QIODevice::setParent(reply);

    connect(reply, SIGNAL(finished()), SLOT(slotFinished()), Qt::QueuedConnection);
}

bool FormDataIODevice::isSequential() const
{
    return true;
}

void FormDataIODevice::slotFinished()
{
    deleteLater();
}

QNetworkReplyHandler::QNetworkReplyHandler(ResourceHandle* handle, LoadMode loadMode)
    : QObject(0)
    , m_reply(0)
    , m_resourceHandle(handle)
    , m_redirected(false)
    , m_responseSent(false)
    , m_responseDataSent(false)
    , m_loadMode(loadMode)
    , m_shouldStart(true)
    , m_shouldFinish(false)
    , m_shouldSendResponse(false)
    , m_shouldForwardData(false)
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

    if (m_loadMode == LoadNormal)
        start();
}

void QNetworkReplyHandler::setLoadMode(LoadMode mode)
{
    // https://bugs.webkit.org/show_bug.cgi?id=26556
    // We cannot call sendQueuedItems() from here, because the signal that 
    // caused us to get into deferred mode, might not be processed yet.
    switch (mode) {
    case LoadNormal:
        m_loadMode = LoadResuming;
        emit processQueuedItems();
        break;
    case LoadDeferred:
        m_loadMode = LoadDeferred;
        break;
    case LoadResuming:
        Q_ASSERT(0); // should never happen
        break;
    };
}

void QNetworkReplyHandler::abort()
{
    m_resourceHandle = 0;
    if (m_reply) {
        QNetworkReply* reply = release();
        reply->abort();
        reply->deleteLater();
        deleteLater();
    }
}

QNetworkReply* QNetworkReplyHandler::release()
{
    QNetworkReply* reply = m_reply;
    if (m_reply) {
        disconnect(m_reply, 0, this, 0);
        // We have queued connections to the QNetworkReply. Make sure any
        // posted meta call events that were the result of a signal emission
        // don't reach the slots in our instance.
        QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
        m_reply = 0;
    }
    return reply;
}

static bool ignoreHttpError(QNetworkReply* reply, bool receivedData)
{
    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (httpStatusCode == 401 || httpStatusCode == 407)
        return true;

    if (receivedData && (httpStatusCode >= 400 && httpStatusCode < 600))
        return true;

    return false;
}

void QNetworkReplyHandler::finish()
{
    m_shouldFinish = (m_loadMode != LoadNormal);
    if (m_shouldFinish)
        return;

    // FIXME: Investigate if this check should be moved into sendResponseIfNeeded()
    if (!m_reply->error())
        sendResponseIfNeeded();

    if (!m_resourceHandle)
        return;
    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client) {
        m_reply->deleteLater();
        m_reply = 0;
        return;
    }
    QNetworkReply* oldReply = m_reply;
    if (m_redirected) {
        resetState();
        start();
    } else if (!m_reply->error() || ignoreHttpError(m_reply, m_responseDataSent)) {
        client->didFinishLoading(m_resourceHandle);
    } else {
        int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QUrl url = m_reply->url();

        if (code) {
            ResourceError error("HTTP", code, url.toString(), m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
            client->didFail(m_resourceHandle, error);
        } else {
            ResourceError error("QtNetwork", m_reply->error(), url.toString(), m_reply->errorString());
            client->didFail(m_resourceHandle, error);
        }
    }

    oldReply->deleteLater();
    if (oldReply == m_reply)
        m_reply = 0;
}

void QNetworkReplyHandler::sendResponseIfNeeded()
{
    m_shouldSendResponse = (m_loadMode != LoadNormal);
    if (m_shouldSendResponse)
        return;

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
    String suggestedFilename = filenameFromHTTPContentDisposition(QString::fromAscii(m_reply->rawHeader("Content-Disposition")));

    if (suggestedFilename.isEmpty())
        suggestedFilename = url.lastPathComponent();

    ResourceResponse response(url, mimeType,
                              m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(),
                              encoding,
                              suggestedFilename);

    const bool isLocalFileReply = (m_reply->url().scheme() == QLatin1String("file"));
    int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!isLocalFileReply) {
        response.setHTTPStatusCode(statusCode);
        response.setHTTPStatusText(m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray().constData());
    }
    else if (m_reply->error() == QNetworkReply::ContentNotFoundError)
        response.setHTTPStatusCode(404);


    /* Fill in the other fields
     * For local file requests remove the content length and the last-modified
     * headers as required by fast/dom/xmlhttprequest-get.xhtml
     */
    foreach (const QByteArray& headerName, m_reply->rawHeaderList()) {
        if (isLocalFileReply
            && (headerName == "Content-Length" || headerName == "Last-Modified"))
            continue;

        response.setHTTPHeaderField(QString::fromAscii(headerName), QString::fromAscii(m_reply->rawHeader(headerName)));
    }

    if (isLocalFileReply)
        response.setHTTPHeaderField(QString::fromAscii("Cache-Control"), QString::fromAscii("no-cache"));

    QUrl redirection = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirection.isValid()) {
        QUrl newUrl = m_reply->url().resolved(redirection);
        ResourceRequest newRequest = m_resourceHandle->request();
        newRequest.setURL(newUrl);

        if (((statusCode >= 301 && statusCode <= 303) || statusCode == 307) && m_method == QNetworkAccessManager::PostOperation) {
            m_method = QNetworkAccessManager::GetOperation;
            newRequest.setHTTPMethod("GET");
        }

        client->willSendRequest(m_resourceHandle, newRequest, response);
        m_redirected = true;
        m_request = newRequest.toNetworkRequest();
    } else {
        client->didReceiveResponse(m_resourceHandle, response);
    }
}

void QNetworkReplyHandler::forwardData()
{
    m_shouldForwardData = (m_loadMode != LoadNormal);
    if (m_shouldForwardData)
        return;

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

    if (!data.isEmpty()) {
        m_responseDataSent = true;
        client->didReceiveData(m_resourceHandle, data.constData(), data.length(), data.length() /*FixMe*/);
    }
}

void QNetworkReplyHandler::start()
{
    m_shouldStart = false;

    ResourceHandleInternal* d = m_resourceHandle->getInternal();

    QNetworkAccessManager* manager = d->m_frame->page()->networkAccessManager();

    const QUrl url = m_request.url();
    const QString scheme = url.scheme();
    // Post requests on files and data don't really make sense, but for
    // fast/forms/form-post-urlencoded.html and for fast/forms/button-state-restore.html
    // we still need to retrieve the file/data, which means we map it to a Get instead.
    if (m_method == QNetworkAccessManager::PostOperation
        && (!url.toLocalFile().isEmpty() || url.scheme() == QLatin1String("data")))
        m_method = QNetworkAccessManager::GetOperation;

    switch (m_method) {
        case QNetworkAccessManager::GetOperation:
            m_reply = manager->get(m_request);
            break;
        case QNetworkAccessManager::PostOperation: {
            FormDataIODevice* postDevice = new FormDataIODevice(d->m_request.httpBody()); 
            m_reply = manager->post(m_request, postDevice);
            postDevice->setParent(m_reply);
            break;
        }
        case QNetworkAccessManager::HeadOperation:
            m_reply = manager->head(m_request);
            break;
        case QNetworkAccessManager::PutOperation: {
            FormDataIODevice* putDevice = new FormDataIODevice(d->m_request.httpBody()); 
            m_reply = manager->put(m_request, putDevice);
            putDevice->setParent(m_reply);
            break;
        }
        case QNetworkAccessManager::UnknownOperation: {
            m_reply = 0;
            ResourceHandleClient* client = m_resourceHandle->client();
            if (client) {
                ResourceError error(url.host(), 400 /*bad request*/,
                                    url.toString(),
                                    QCoreApplication::translate("QWebPage", "Bad HTTP request"));
                client->didFail(m_resourceHandle, error);
            }
            return;
        }
    }

    m_reply->setParent(this);

    connect(m_reply, SIGNAL(finished()),
            this, SLOT(finish()), Qt::QueuedConnection);

    // For http(s) we know that the headers are complete upon metaDataChanged() emission, so we
    // can send the response as early as possible
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        connect(m_reply, SIGNAL(metaDataChanged()),
                this, SLOT(sendResponseIfNeeded()), Qt::QueuedConnection);

    connect(m_reply, SIGNAL(readyRead()),
            this, SLOT(forwardData()), Qt::QueuedConnection);
    connect(this, SIGNAL(processQueuedItems()),
            this, SLOT(sendQueuedItems()), Qt::QueuedConnection);
}

void QNetworkReplyHandler::resetState()
{
    m_redirected = false;
    m_responseSent = false;
    m_responseDataSent = false;
    m_shouldStart = true;
    m_shouldFinish = false;
    m_shouldSendResponse = false;
    m_shouldForwardData = false;
}

void QNetworkReplyHandler::sendQueuedItems()
{
    if (m_loadMode != LoadResuming)
        return;
    m_loadMode = LoadNormal;

    if (m_shouldStart)
        start();

    if (m_shouldSendResponse)
        sendResponseIfNeeded();

    if (m_shouldForwardData)
        forwardData();

    if (m_shouldFinish)
        finish(); 
}

}

#include "moc_QNetworkReplyHandler.cpp"

#endif

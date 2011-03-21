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

#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <qwebframe.h>
#include <qwebpage.h>

#include <wtf/text/CString.h>

#include <QDebug>
#include <QCoreApplication>

// In Qt 4.8, the attribute for sending a request synchronously will be made public,
// for now, use this hackish solution for setting the internal attribute.
const QNetworkRequest::Attribute gSynchronousNetworkRequestAttribute = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::HttpPipeliningWasUsedAttribute + 7);

static const int gMaxRedirections = 10;

namespace WebCore {

// Take a deep copy of the FormDataElement
FormDataIODevice::FormDataIODevice(FormData* data)
    : m_formElements(data ? data->elements() : Vector<FormDataElement>())
    , m_currentFile(0)
    , m_currentDelta(0)
    , m_fileSize(0)
    , m_dataSize(0)
{
    setOpenMode(FormDataIODevice::ReadOnly);

    if (!m_formElements.isEmpty() && m_formElements[0].m_type == FormDataElement::encodedFile)
        openFileForCurrentElement();
    computeSize();
}

FormDataIODevice::~FormDataIODevice()
{
    delete m_currentFile;
}

qint64 FormDataIODevice::computeSize() 
{
    for (int i = 0; i < m_formElements.size(); ++i) {
        const FormDataElement& element = m_formElements[i];
        if (element.m_type == FormDataElement::data) 
            m_dataSize += element.m_data.size();
        else {
            QFileInfo fi(element.m_filename);
            m_fileSize += fi.size();
        }
    }
    return m_dataSize + m_fileSize;
}

void FormDataIODevice::moveToNextElement()
{
    if (m_currentFile)
        m_currentFile->close();
    m_currentDelta = 0;

    m_formElements.remove(0);

    if (m_formElements.isEmpty() || m_formElements[0].m_type == FormDataElement::data)
        return;

    openFileForCurrentElement();
}

void FormDataIODevice::openFileForCurrentElement()
{
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

bool FormDataIODevice::isSequential() const
{
    return true;
}

String QNetworkReplyHandler::httpMethod() const
{
    switch (m_method) {
    case QNetworkAccessManager::GetOperation:
        return "GET";
    case QNetworkAccessManager::HeadOperation:
        return "HEAD";
    case QNetworkAccessManager::PostOperation:
        return "POST";
    case QNetworkAccessManager::PutOperation:
        return "PUT";
    case QNetworkAccessManager::DeleteOperation:
        return "DELETE";
    case QNetworkAccessManager::CustomOperation:
        return m_resourceHandle->firstRequest().httpMethod();
    default:
        ASSERT_NOT_REACHED();
        return "GET";
    }
}

QNetworkReplyHandler::QNetworkReplyHandler(ResourceHandle* handle, LoadType loadType, bool deferred)
    : QObject(0)
    , m_reply(0)
    , m_resourceHandle(handle)
    , m_loadType(loadType)
    , m_deferred(deferred)
    , m_redirectionTries(gMaxRedirections)
{
    resetState();

    const ResourceRequest &r = m_resourceHandle->firstRequest();

    if (r.httpMethod() == "GET")
        m_method = QNetworkAccessManager::GetOperation;
    else if (r.httpMethod() == "HEAD")
        m_method = QNetworkAccessManager::HeadOperation;
    else if (r.httpMethod() == "POST")
        m_method = QNetworkAccessManager::PostOperation;
    else if (r.httpMethod() == "PUT")
        m_method = QNetworkAccessManager::PutOperation;
    else if (r.httpMethod() == "DELETE")
        m_method = QNetworkAccessManager::DeleteOperation;
    else
        m_method = QNetworkAccessManager::CustomOperation;

    QObject* originatingObject = 0;
    if (m_resourceHandle->getInternal()->m_context)
        originatingObject = m_resourceHandle->getInternal()->m_context->originatingObject();

    m_request = r.toNetworkRequest(originatingObject);

    if (!m_deferred)
        start();
}

void QNetworkReplyHandler::resetState()
{
    m_redirected = false;
    m_responseSent = false;
    m_responseContainsData = false;
    m_hasStarted = false;
    m_callFinishOnResume = false;
    m_callSendResponseIfNeededOnResume = false;
    m_callForwardDataOnResume = false;

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = 0;
    }
}

void QNetworkReplyHandler::setLoadingDeferred(bool deferred)
{
    m_deferred = deferred;

    if (!deferred)
        resumeDeferredLoad();
}

void QNetworkReplyHandler::resumeDeferredLoad()
{
    if (!m_hasStarted) {
        ASSERT(!m_callSendResponseIfNeededOnResume);
        ASSERT(!m_callForwardDataOnResume);
        ASSERT(!m_callFinishOnResume);
        start();
        return;
    }

    if (m_callSendResponseIfNeededOnResume)
        sendResponseIfNeeded();

    if (m_callForwardDataOnResume)
        forwardData();

    if (m_callFinishOnResume)
        finish();
}

void QNetworkReplyHandler::abort()
{
    m_resourceHandle = 0;
    if (QNetworkReply* reply = release()) {
        reply->abort();
        reply->deleteLater();
    }
    deleteLater();
}

QNetworkReply* QNetworkReplyHandler::release()
{
    if (!m_reply)
        return 0;

    QNetworkReply* reply = m_reply;
    m_reply = 0;
    disconnect(reply, 0, this, 0);
    reply->setParent(0);
    return reply;
}

static bool shouldIgnoreHttpError(QNetworkReply* reply, bool receivedData)
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
    ASSERT(m_hasStarted);

    m_callFinishOnResume = m_deferred;
    if (m_deferred)
        return;

    if (!m_reply)
        return;

    sendResponseIfNeeded();

    if (wasAborted())
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client) {
        m_reply->deleteLater();
        m_reply = 0;
        return;
    }

    if (m_redirected) {
        resetState();
        start();
        return;
    }

    if (!m_reply->error() || shouldIgnoreHttpError(m_reply, m_responseContainsData))
        client->didFinishLoading(m_resourceHandle, 0);
    else {
        QUrl url = m_reply->url();
        int httpStatusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (httpStatusCode) {
            ResourceError error("HTTP", httpStatusCode, url.toString(), m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
            client->didFail(m_resourceHandle, error);
        } else {
            ResourceError error("QtNetwork", m_reply->error(), url.toString(), m_reply->errorString());
            client->didFail(m_resourceHandle, error);
        }
    }

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = 0;
    }
}

void QNetworkReplyHandler::sendResponseIfNeeded()
{
    ASSERT(m_hasStarted);

    m_callSendResponseIfNeededOnResume = m_deferred;
    if (m_deferred)
        return;

    if (!m_reply)
        return;

    if (m_reply->error() && !shouldIgnoreHttpError(m_reply, m_responseContainsData))
        return;

    if (wasAborted())
        return;

    if (m_responseSent)
        return;
    m_responseSent = true;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    WTF::String contentType = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
    WTF::String encoding = extractCharsetFromMediaType(contentType);
    WTF::String mimeType = extractMIMETypeFromMediaType(contentType);

    if (mimeType.isEmpty()) {
        // let's try to guess from the extension
        mimeType = MIMETypeRegistry::getMIMETypeForPath(m_reply->url().path());
    }

    KURL url(m_reply->url());
    ResourceResponse response(url, mimeType.lower(),
                              m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(),
                              encoding, String());

    if (url.isLocalFile()) {
        client->didReceiveResponse(m_resourceHandle, response);
        return;
    }

    // The status code is equal to 0 for protocols not in the HTTP family.
    int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (url.protocolInHTTPFamily()) {
        String suggestedFilename = filenameFromHTTPContentDisposition(QString::fromAscii(m_reply->rawHeader("Content-Disposition")));

        if (!suggestedFilename.isEmpty())
            response.setSuggestedFilename(suggestedFilename);
        else
            response.setSuggestedFilename(url.lastPathComponent());

        response.setHTTPStatusCode(statusCode);
        response.setHTTPStatusText(m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toByteArray().constData());

        // Add remaining headers.
        foreach (const QNetworkReply::RawHeaderPair& pair, m_reply->rawHeaderPairs())
            response.setHTTPHeaderField(QString::fromAscii(pair.first), QString::fromAscii(pair.second));
    }

    QUrl redirection = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirection.isValid()) {
        redirect(response, redirection);
        return;
    }

    client->didReceiveResponse(m_resourceHandle, response);
}

void QNetworkReplyHandler::redirect(ResourceResponse& response, const QUrl& redirection)
{
    QUrl newUrl = m_reply->url().resolved(redirection);

    ResourceHandleClient* client = m_resourceHandle->client();
    ASSERT(client);

    int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    m_redirectionTries--;
    if (!m_redirectionTries) {
        ResourceError error(newUrl.host(), 400 /*bad request*/,
                            newUrl.toString(),
                            QCoreApplication::translate("QWebPage", "Redirection limit reached"));
        client->didFail(m_resourceHandle, error);
        return;
    }
    m_redirected = true;

    //  Status Code 301 (Moved Permanently), 302 (Moved Temporarily), 303 (See Other):
    //    - If original request is POST convert to GET and redirect automatically
    //  Status Code 307 (Temporary Redirect) and all other redirect status codes:
    //    - Use the HTTP method from the previous request
    if ((statusCode >= 301 && statusCode <= 303) && m_resourceHandle->firstRequest().httpMethod() == "POST")
        m_method = QNetworkAccessManager::GetOperation;

    ResourceRequest newRequest = m_resourceHandle->firstRequest();
    newRequest.setHTTPMethod(httpMethod());
    newRequest.setURL(newUrl);

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (!newRequest.url().protocolIs("https") && protocolIs(newRequest.httpReferrer(), "https"))
        newRequest.clearHTTPReferrer();

    client->willSendRequest(m_resourceHandle, newRequest, response);
    if (wasAborted()) // Network error cancelled the request.
        return;

    QObject* originatingObject = 0;
    if (m_resourceHandle->getInternal()->m_context)
        originatingObject = m_resourceHandle->getInternal()->m_context->originatingObject();

    m_request = newRequest.toNetworkRequest(originatingObject);
}

void QNetworkReplyHandler::forwardData()
{
    ASSERT(m_hasStarted);

    m_callForwardDataOnResume = m_deferred;
    if (m_deferred)
        return;

    if (m_reply->bytesAvailable())
        m_responseContainsData = true;

    sendResponseIfNeeded();

    // don't emit the "Document has moved here" type of HTML
    if (m_redirected)
        return;

    if (wasAborted())
        return;

    QByteArray data = m_reply->read(m_reply->bytesAvailable());

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=19793
    // -1 means we do not provide any data about transfer size to inspector so it would use
    // Content-Length headers or content size to show transfer size.
    if (!data.isEmpty())
        client->didReceiveData(m_resourceHandle, data.constData(), data.length(), -1);
}

void QNetworkReplyHandler::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (wasAborted())
        return;

    ResourceHandleClient* client = m_resourceHandle->client();
    if (!client)
        return;

    client->didSendData(m_resourceHandle, bytesSent, bytesTotal);
}

void QNetworkReplyHandler::start()
{
    ASSERT(!m_hasStarted);
    m_hasStarted = true;

    if (m_loadType == SynchronousLoad)
        m_request.setAttribute(gSynchronousNetworkRequestAttribute, true);

    ResourceHandleInternal* d = m_resourceHandle->getInternal();

    QNetworkAccessManager* manager = 0;
    if (d->m_context)
        manager = d->m_context->networkAccessManager();

    if (!manager)
        return;

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
            FormDataIODevice* postDevice = new FormDataIODevice(d->m_firstRequest.httpBody()); 
            // We may be uploading files so prevent QNR from buffering data
            m_request.setHeader(QNetworkRequest::ContentLengthHeader, postDevice->getFormDataSize());
            m_request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, QVariant(true));
            m_reply = manager->post(m_request, postDevice);
            postDevice->setParent(m_reply);
            break;
        }
        case QNetworkAccessManager::HeadOperation:
            m_reply = manager->head(m_request);
            break;
        case QNetworkAccessManager::PutOperation: {
            FormDataIODevice* putDevice = new FormDataIODevice(d->m_firstRequest.httpBody()); 
            // We may be uploading files so prevent QNR from buffering data
            m_request.setHeader(QNetworkRequest::ContentLengthHeader, putDevice->getFormDataSize());
            m_request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, QVariant(true));
            m_reply = manager->put(m_request, putDevice);
            putDevice->setParent(m_reply);
            break;
        }
        case QNetworkAccessManager::DeleteOperation: {
            m_reply = manager->deleteResource(m_request);
            break;
        }
        case QNetworkAccessManager::CustomOperation:
            m_reply = manager->sendCustomRequest(m_request, m_resourceHandle->firstRequest().httpMethod().latin1().data());
            break;
        case QNetworkAccessManager::UnknownOperation:
            ASSERT_NOT_REACHED();
            return;
    }

    m_reply->setParent(this);

    if (m_loadType == SynchronousLoad && m_reply->isFinished()) {
        // If supported, a synchronous request will be finished at this point, no need to hook up the signals.
        return;
    }

    connect(m_reply, SIGNAL(finished()), this, SLOT(finish()));

    // For http(s) we know that the headers are complete upon metaDataChanged() emission, so we
    // can send the response as early as possible
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        connect(m_reply, SIGNAL(metaDataChanged()), this, SLOT(sendResponseIfNeeded()));

    connect(m_reply, SIGNAL(readyRead()), this, SLOT(forwardData()));

    if (m_resourceHandle->firstRequest().reportUploadProgress())
        connect(m_reply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(uploadProgress(qint64, qint64)));
}

}

#include "moc_QNetworkReplyHandler.cpp"

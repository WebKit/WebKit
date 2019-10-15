/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ServiceWorkerFetchTask.h"

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include <WebCore/CrossOriginAccessControl.h>

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerFetchTask::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerFetchTask::" fmt, this, ##__VA_ARGS__)

using namespace WebCore;

namespace WebKit {

ServiceWorkerFetchTask::ServiceWorkerFetchTask(PAL::SessionID sessionID, NetworkResourceLoader& loader, SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier)
    : m_sessionID(sessionID)
    , m_loader(loader)
    , m_fetchIdentifier(WebCore::FetchIdentifier::generate())
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_timeoutTimer(*this, &ServiceWorkerFetchTask::timeoutTimerFired)
{
    m_timeoutTimer.startOneShot(loader.connectionToWebProcess().networkProcess().serviceWorkerFetchTimeout());
}

ServiceWorkerFetchTask::~ServiceWorkerFetchTask()
{
    if (m_serviceWorkerConnection)
        m_serviceWorkerConnection->unregisterFetch(*this);
}

template<typename Message> bool ServiceWorkerFetchTask::sendToServiceWorker(Message&& message)
{
    return m_serviceWorkerConnection ? m_serviceWorkerConnection->ipcConnection().send(std::forward<Message>(message), 0) : false;
}

template<typename Message> bool ServiceWorkerFetchTask::sendToClient(Message&& message)
{
    return m_loader.connectionToWebProcess().connection().send(std::forward<Message>(message), m_loader.identifier());
}

void ServiceWorkerFetchTask::start(WebSWServerToContextConnection& serviceWorkerConnection)
{
    m_serviceWorkerConnection = makeWeakPtr(serviceWorkerConnection);
    serviceWorkerConnection.registerFetch(*this);

    startFetch(ResourceRequest { m_loader.originalRequest() }, serviceWorkerConnection);
}

void ServiceWorkerFetchTask::startFetch(ResourceRequest&& request, WebSWServerToContextConnection& serviceWorkerConnection)
{
    m_currentRequest = WTFMove(request);

    auto& options = m_loader.parameters().options;
    auto referrer = m_currentRequest.httpReferrer();

    // We are intercepting fetch calls after going through the HTTP layer, which may add some specific headers.
    cleanHTTPRequestHeadersForAccessControl(m_currentRequest, m_loader.parameters().httpHeadersToKeep);

    bool isSent = sendToServiceWorker(Messages::WebSWContextManagerConnection::StartFetch { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier, m_currentRequest, options, IPC::FormDataReference { m_currentRequest.httpBody() }, referrer });
    ASSERT_UNUSED(isSent, isSent);
}

void ServiceWorkerFetchTask::didReceiveRedirectResponse(ResourceResponse&& response)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveRedirectResponse: %s", m_fetchIdentifier.loggingString().utf8().data());
    m_wasHandled = true;

    response.setSource(ResourceResponse::Source::ServiceWorker);
    auto newRequest = m_currentRequest.redirectedRequest(response, m_loader.parameters().shouldClearReferrerOnHTTPSToHTTPRedirect);

    sendToClient(Messages::WebResourceLoader::WillSendRequest { newRequest, response });
}

void ServiceWorkerFetchTask::didReceiveResponse(ResourceResponse&& response, bool needsContinueDidReceiveResponseMessage)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: %s", m_fetchIdentifier.loggingString().utf8().data());
    m_timeoutTimer.stop();
    m_wasHandled = true;

    response.setSource(ResourceResponse::Source::ServiceWorker);
    sendToClient(Messages::WebResourceLoader::DidReceiveResponse { response, needsContinueDidReceiveResponseMessage });
}

void ServiceWorkerFetchTask::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    ASSERT(!m_timeoutTimer.isActive());
    sendToClient(Messages::WebResourceLoader::DidReceiveData { IPC::SharedBufferDataReference { data.data(), data.size() }, encodedDataLength });
}

void ServiceWorkerFetchTask::didReceiveFormData(const IPC::FormDataReference& formData)
{
    ASSERT(!m_timeoutTimer.isActive());
    // FIXME: Allow WebResourceLoader to receive form data.
}

void ServiceWorkerFetchTask::didFinish()
{
    ASSERT(!m_timeoutTimer.isActive());
    RELEASE_LOG_IF_ALLOWED("didFinishFetch: fetchIdentifier: %s", m_fetchIdentifier.loggingString().utf8().data());
    m_timeoutTimer.stop();
    sendToClient(Messages::WebResourceLoader::DidFinishResourceLoad { { } });
}

void ServiceWorkerFetchTask::didFail(const ResourceError& error)
{
    m_timeoutTimer.stop();
    RELEASE_LOG_ERROR_IF_ALLOWED("didFailFetch: fetchIdentifier: %s", m_fetchIdentifier.loggingString().utf8().data());
    m_loader.didFailLoading(error);
}

void ServiceWorkerFetchTask::didNotHandle()
{
    RELEASE_LOG_IF_ALLOWED("didNotHandleFetch: fetchIdentifier: %s", m_fetchIdentifier.loggingString().utf8().data());
    m_timeoutTimer.stop();
    m_loader.serviceWorkerDidNotHandle();
}

void ServiceWorkerFetchTask::cancelFromClient()
{
    RELEASE_LOG_IF_ALLOWED("cancelFromClient: fetchIdentifier: %s", m_fetchIdentifier.loggingString().utf8().data());
    sendToServiceWorker(Messages::WebSWContextManagerConnection::CancelFetch { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueDidReceiveFetchResponse()
{
    sendToServiceWorker(Messages::WebSWContextManagerConnection::ContinueDidReceiveFetchResponse { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueFetchTaskWith(ResourceRequest&& request)
{
    if (!m_serviceWorkerConnection) {
        m_loader.serviceWorkerDidNotHandle();
        return;
    }
    startFetch(WTFMove(request), *m_serviceWorkerConnection);
}

void ServiceWorkerFetchTask::timeoutTimerFired()
{
    RELEASE_LOG_IF_ALLOWED("timeoutTimerFired: fetchIdentifier: %s", m_fetchIdentifier.loggingString().utf8().data());
    if (m_serviceWorkerConnection)
        m_serviceWorkerConnection->fetchTaskTimedOut(serviceWorkerIdentifier());
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)

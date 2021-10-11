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
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include "WebSWServerToContextConnection.h"
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/SWServerRegistration.h>

#define SWFETCH_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - [fetchIdentifier=%" PRIu64 "] ServiceWorkerFetchTask::" fmt, this, m_fetchIdentifier.toUInt64(), ##__VA_ARGS__)
#define SWFETCH_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - [fetchIdentifier=%" PRIu64 "] ServiceWorkerFetchTask::" fmt, this, m_fetchIdentifier.toUInt64(), ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

ServiceWorkerFetchTask::ServiceWorkerFetchTask(WebSWServerConnection& swServerConnection, NetworkResourceLoader& loader, ResourceRequest&& request, SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, ServiceWorkerRegistrationIdentifier serviceWorkerRegistrationIdentifier, bool shouldSoftUpdate)
    : m_swServerConnection(makeWeakPtr(swServerConnection))
    , m_loader(loader)
    , m_fetchIdentifier(WebCore::FetchIdentifier::generate())
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_currentRequest(WTFMove(request))
    , m_timeoutTimer(*this, &ServiceWorkerFetchTask::timeoutTimerFired)
    , m_serviceWorkerRegistrationIdentifier(serviceWorkerRegistrationIdentifier)
    , m_shouldSoftUpdate(shouldSoftUpdate)
{
    SWFETCH_RELEASE_LOG("ServiceWorkerFetchTask: (serverConnectionIdentifier=%" PRIu64 ", serviceWorkerRegistrationIdentifier=%" PRIu64 ", serviceWorkerIdentifier=%" PRIu64 ")", m_serverConnectionIdentifier.toUInt64(), m_serviceWorkerRegistrationIdentifier.toUInt64(), m_serviceWorkerIdentifier.toUInt64());
    m_timeoutTimer.startOneShot(loader.connectionToWebProcess().networkProcess().serviceWorkerFetchTimeout());
}

ServiceWorkerFetchTask::~ServiceWorkerFetchTask()
{
    SWFETCH_RELEASE_LOG("~ServiceWorkerFetchTask:");
    if (m_serviceWorkerConnection)
        m_serviceWorkerConnection->unregisterFetch(*this);
}

template<typename Message> bool ServiceWorkerFetchTask::sendToServiceWorker(Message&& message)
{
    return m_serviceWorkerConnection ? m_serviceWorkerConnection->ipcConnection().send(std::forward<Message>(message), 0) : false;
}

template<typename Message> bool ServiceWorkerFetchTask::sendToClient(Message&& message)
{
    return m_loader.connectionToWebProcess().connection().send(std::forward<Message>(message), m_loader.coreIdentifier());
}

void ServiceWorkerFetchTask::start(WebSWServerToContextConnection& serviceWorkerConnection)
{
    SWFETCH_RELEASE_LOG("start:");
    m_serviceWorkerConnection = makeWeakPtr(serviceWorkerConnection);
    serviceWorkerConnection.registerFetch(*this);

    startFetch();
}

void ServiceWorkerFetchTask::contextClosed()
{
    SWFETCH_RELEASE_LOG("contextClosed: (m_isDone=%d, m_wasHandled=%d)", m_isDone, m_wasHandled);
    m_serviceWorkerConnection = nullptr;
    if (m_isDone)
        return;

    if (m_wasHandled) {
        didFail(ResourceError { errorDomainWebKitInternal, 0, { }, "Service Worker context closed"_s });
        return;
    }
    cannotHandle();
}

void ServiceWorkerFetchTask::startFetch()
{
    SWFETCH_RELEASE_LOG("startFetch");
    m_loader.consumeSandboxExtensionsIfNeeded();
    auto& options = m_loader.parameters().options;
    auto referrer = m_currentRequest.httpReferrer();

    // We are intercepting fetch calls after going through the HTTP layer, which may add some specific headers.
    auto request = m_currentRequest;
    cleanHTTPRequestHeadersForAccessControl(request, m_loader.parameters().httpHeadersToKeep);

    bool isSent = sendToServiceWorker(Messages::WebSWContextManagerConnection::StartFetch { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier, request, options, IPC::FormDataReference { m_currentRequest.httpBody() }, referrer });
    ASSERT_UNUSED(isSent, isSent);
}

void ServiceWorkerFetchTask::didReceiveRedirectResponse(ResourceResponse&& response)
{
    if (m_isDone)
        return;

    SWFETCH_RELEASE_LOG("didReceiveRedirectResponse:");
    m_wasHandled = true;
    m_timeoutTimer.stop();
    softUpdateIfNeeded();

    response.setSource(ResourceResponse::Source::ServiceWorker);
    auto newRequest = m_currentRequest.redirectedRequest(response, m_loader.parameters().shouldClearReferrerOnHTTPSToHTTPRedirect);

    sendToClient(Messages::WebResourceLoader::WillSendRequest { newRequest, IPC::FormDataReference { newRequest.httpBody() }, response });
}

void ServiceWorkerFetchTask::didReceiveResponse(ResourceResponse&& response, bool needsContinueDidReceiveResponseMessage)
{
    if (m_isDone)
        return;

    SWFETCH_RELEASE_LOG("didReceiveResponse: (httpStatusCode=%d, MIMEType=%" PUBLIC_LOG_STRING ", expectedContentLength=%" PRId64 ", needsContinueDidReceiveResponseMessage=%d, source=%u)", response.httpStatusCode(), response.mimeType().utf8().data(), response.expectedContentLength(), needsContinueDidReceiveResponseMessage, static_cast<unsigned>(response.source()));
    m_wasHandled = true;
    m_timeoutTimer.stop();
    softUpdateIfNeeded();

    if (m_loader.parameters().options.mode == FetchOptions::Mode::Navigate) {
        if (auto parentOrigin = m_loader.parameters().parentOrigin()) {
            if (auto error = validateCrossOriginResourcePolicy(m_loader.parameters().parentCrossOriginEmbedderPolicy.value, *parentOrigin, m_currentRequest.url(), response, ForNavigation::Yes)) {
                didFail(*error);
                return;
            }
        }
    }
    if (m_loader.parameters().options.mode == FetchOptions::Mode::NoCors) {
        if (auto error = validateCrossOriginResourcePolicy(m_loader.parameters().crossOriginEmbedderPolicy.value, *m_loader.parameters().sourceOrigin, m_currentRequest.url(), response, ForNavigation::No)) {
            didFail(*error);
            return;
        }
    }

    response.setSource(ResourceResponse::Source::ServiceWorker);
    sendToClient(Messages::WebResourceLoader::DidReceiveResponse { response, needsContinueDidReceiveResponseMessage });
    if (needsContinueDidReceiveResponseMessage)
        m_loader.setResponse(WTFMove(response));
}

void ServiceWorkerFetchTask::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    if (m_isDone)
        return;

    ASSERT(!m_timeoutTimer.isActive());
    sendToClient(Messages::WebResourceLoader::DidReceiveData { data, encodedDataLength });
}

void ServiceWorkerFetchTask::didReceiveFormData(const IPC::FormDataReference& formData)
{
    if (m_isDone)
        return;

    ASSERT(!m_timeoutTimer.isActive());
    // FIXME: Allow WebResourceLoader to receive form data.
}

void ServiceWorkerFetchTask::didFinish()
{
    ASSERT(!m_timeoutTimer.isActive());
    SWFETCH_RELEASE_LOG("didFinish:");

    m_isDone = true;
    m_timeoutTimer.stop();
    sendToClient(Messages::WebResourceLoader::DidFinishResourceLoad { { } });
}

void ServiceWorkerFetchTask::didFail(const ResourceError& error)
{
    m_isDone = true;
    if (m_timeoutTimer.isActive()) {
        m_timeoutTimer.stop();
        softUpdateIfNeeded();
    }
    SWFETCH_RELEASE_LOG_ERROR("didFail: (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d)", error.domain().utf8().data(), error.errorCode());
    m_loader.didFailLoading(error);
}

void ServiceWorkerFetchTask::didNotHandle()
{
    if (m_isDone)
        return;

    SWFETCH_RELEASE_LOG("didNotHandle:");
    m_isDone = true;
    m_timeoutTimer.stop();
    softUpdateIfNeeded();

    m_loader.serviceWorkerDidNotHandle(this);
}

void ServiceWorkerFetchTask::cannotHandle()
{
    SWFETCH_RELEASE_LOG("cannotHandle:");
    // Make sure we call didNotHandle asynchronously because failing synchronously would get the NetworkResourceLoader in a bad state.
    RunLoop::main().dispatch([weakThis = makeWeakPtr(this)] {
        if (weakThis)
            weakThis->didNotHandle();
    });
}

void ServiceWorkerFetchTask::cancelFromClient()
{
    SWFETCH_RELEASE_LOG("cancelFromClient:");
    sendToServiceWorker(Messages::WebSWContextManagerConnection::CancelFetch { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueDidReceiveFetchResponse()
{
    SWFETCH_RELEASE_LOG("continueDidReceiveFetchResponse:");
    sendToServiceWorker(Messages::WebSWContextManagerConnection::ContinueDidReceiveFetchResponse { m_serverConnectionIdentifier, m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueFetchTaskWith(ResourceRequest&& request)
{
    SWFETCH_RELEASE_LOG("continueFetchTaskWith: (hasServiceWorkerConnection=%d)", !!m_serviceWorkerConnection);
    if (!m_serviceWorkerConnection) {
        m_loader.serviceWorkerDidNotHandle(this);
        return;
    }
    m_timeoutTimer.startOneShot(m_loader.connectionToWebProcess().networkProcess().serviceWorkerFetchTimeout());
    m_currentRequest = WTFMove(request);
    startFetch();
}

void ServiceWorkerFetchTask::timeoutTimerFired()
{
    ASSERT(!m_isDone);
    ASSERT(!m_wasHandled);
    SWFETCH_RELEASE_LOG_ERROR("timeoutTimerFired: (hasServiceWorkerConnection=%d)", !!m_serviceWorkerConnection);

    softUpdateIfNeeded();

    cannotHandle();

    if (m_swServerConnection)
        m_swServerConnection->fetchTaskTimedOut(serviceWorkerIdentifier());
}

void ServiceWorkerFetchTask::softUpdateIfNeeded()
{
    SWFETCH_RELEASE_LOG("softUpdateIfNeeded: (m_shouldSoftUpdate=%d)", m_shouldSoftUpdate);
    if (!m_shouldSoftUpdate)
        return;
    if (auto* registration = m_loader.connectionToWebProcess().swConnection().server().getRegistration(m_serviceWorkerRegistrationIdentifier))
        registration->scheduleSoftUpdate(m_loader.isAppInitiated() ? WebCore::IsAppInitiated::Yes : WebCore::IsAppInitiated::No);
}

} // namespace WebKit

#undef SWFETCH_RELEASE_LOG
#undef SWFETCH_RELEASE_LOG_ERROR

#endif // ENABLE(SERVICE_WORKER)

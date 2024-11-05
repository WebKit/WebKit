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

#include "Connection.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkLoad.h"
#include "NetworkOriginAccessPatterns.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "PrivateRelayed.h"
#include "ServiceWorkerNavigationPreloader.h"
#include "SharedBufferReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include "WebSWServerToContextConnection.h"
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/SWServerRegistration.h>
#include <wtf/TZoneMallocInlines.h>

#define SWFETCH_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - [fetchIdentifier=%" PRIu64 "] ServiceWorkerFetchTask::" fmt, this, m_fetchIdentifier.toUInt64(), ##__VA_ARGS__)
#define SWFETCH_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - [fetchIdentifier=%" PRIu64 "] ServiceWorkerFetchTask::" fmt, this, m_fetchIdentifier.toUInt64(), ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ServiceWorkerFetchTask);

Ref<ServiceWorkerFetchTask> ServiceWorkerFetchTask::create(WebSWServerConnection& connection, NetworkResourceLoader& loader, WebCore::ResourceRequest&& request, WebCore::SWServerConnectionIdentifier connectionIdentifier, WebCore::ServiceWorkerIdentifier workerIdentifier, WebCore::SWServerRegistration& registration, NetworkSession* session, bool isWorkerReady)
{
    return adoptRef(*new ServiceWorkerFetchTask(connection, loader, WTFMove(request), connectionIdentifier, workerIdentifier, registration, session, isWorkerReady));
}

Ref<ServiceWorkerFetchTask> ServiceWorkerFetchTask::create(WebSWServerConnection& connection, NetworkResourceLoader& loader, std::unique_ptr<ServiceWorkerNavigationPreloader>&& preloader)
{
    return adoptRef(*new ServiceWorkerFetchTask(connection, loader, WTFMove(preloader)));
}

RefPtr<ServiceWorkerFetchTask> ServiceWorkerFetchTask::fromNavigationPreloader(WebSWServerConnection& swServerConnection, NetworkResourceLoader& loader, const WebCore::ResourceRequest& request, NetworkSession* session)
{
    if (!loader.parameters().navigationPreloadIdentifier)
        return nullptr;

    RefPtr task = session ? session->navigationPreloaderTaskFromFetchIdentifier(*loader.parameters().navigationPreloadIdentifier) : nullptr;
    if (!task || !task->m_preloader || task->m_isLoadingFromPreloader) {
        RELEASE_LOG_ERROR(ServiceWorker, "Unable to retrieve preloader, load will go to the network");
        return nullptr;
    }

    auto preload = std::exchange(task->m_preloader, { });
    return ServiceWorkerFetchTask::create(swServerConnection, loader, WTFMove(preload));
}

ServiceWorkerFetchTask::ServiceWorkerFetchTask(WebSWServerConnection& swServerConnection, NetworkResourceLoader& loader, std::unique_ptr<ServiceWorkerNavigationPreloader>&& preloader)
    : m_swServerConnection(swServerConnection)
    , m_loader(loader)
    , m_fetchIdentifier(WebCore::FetchIdentifier::generate())
    , m_preloader(WTFMove(preloader))
{
    callOnMainRunLoop([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->loadResponseFromPreloader();
    });
}

ServiceWorkerFetchTask::ServiceWorkerFetchTask(WebSWServerConnection& swServerConnection, NetworkResourceLoader& loader, ResourceRequest&& request, SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, SWServerRegistration& registration, NetworkSession* session, bool isWorkerReady)
    : m_swServerConnection(swServerConnection)
    , m_loader(loader)
    , m_fetchIdentifier(WebCore::FetchIdentifier::generate())
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_serviceWorkerIdentifier(serviceWorkerIdentifier)
    , m_currentRequest(WTFMove(request))
    , m_serviceWorkerRegistrationIdentifier(registration.identifier())
    , m_shouldSoftUpdate(registration.shouldSoftUpdate(loader.parameters().options))
{
    SWFETCH_RELEASE_LOG("ServiceWorkerFetchTask: (serverConnectionIdentifier=%" PRIu64 ", serviceWorkerRegistrationIdentifier=%" PRIu64 ", serviceWorkerIdentifier=%" PRIu64 ", %d)", m_serverConnectionIdentifier->toUInt64(), m_serviceWorkerRegistrationIdentifier->toUInt64(), m_serviceWorkerIdentifier->toUInt64(), isWorkerReady);

    // We only do the timeout logic for main document navigations because it is not Web-compatible to do so for subresources.
    if (loader.parameters().request.requester() == WebCore::ResourceRequestRequester::Main) {
        m_timeoutTimer = makeUnique<Timer>(*this, &ServiceWorkerFetchTask::timeoutTimerFired);
        m_timeoutTimer->startOneShot(loader.connectionToWebProcess().networkProcess().serviceWorkerFetchTimeout());
    }

    bool shouldDoNavigationPreload = session && isNavigationRequest(loader.parameters().options.destination) && m_currentRequest.httpMethod() == "GET"_s;

    if (shouldDoNavigationPreload && (!isWorkerReady || registration.navigationPreloadState().enabled)) {
        NetworkLoadParameters parameters = loader.parameters();
        parameters.request = m_currentRequest;
        m_preloader = makeUnique<ServiceWorkerNavigationPreloader>(*session, WTFMove(parameters), registration.navigationPreloadState(), loader.shouldCaptureExtraNetworkLoadMetrics());
        session->addNavigationPreloaderTask(*this);

        m_preloader->waitForResponse([weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->preloadResponseIsReady();
        });
    }

    loader.setWorkerStart(MonotonicTime::now());
}

ServiceWorkerFetchTask::~ServiceWorkerFetchTask()
{
    SWFETCH_RELEASE_LOG("~ServiceWorkerFetchTask:");
    if (RefPtr serviceWorkerConnection = m_serviceWorkerConnection.get())
        serviceWorkerConnection->unregisterFetch(*this);

    cancelPreloadIfNecessary();
}

template<typename Message> bool ServiceWorkerFetchTask::sendToServiceWorker(Message&& message)
{
    if (!m_serviceWorkerConnection)
        return false;

    return m_serviceWorkerConnection->protectedIPCConnection()->send(std::forward<Message>(message), 0) == IPC::Error::NoError;
}

template<typename Message> bool ServiceWorkerFetchTask::sendToClient(Message&& message)
{
    Ref loader = *m_loader;
    return loader->protectedConnectionToWebProcess()->protectedConnection()->send(std::forward<Message>(message), loader->coreIdentifier()) == IPC::Error::NoError;
}

void ServiceWorkerFetchTask::start(WebSWServerToContextConnection& serviceWorkerConnection)
{
    SWFETCH_RELEASE_LOG("start:");
    m_serviceWorkerConnection = serviceWorkerConnection;
    serviceWorkerConnection.registerFetch(*this);

    startFetch();
}

void ServiceWorkerFetchTask::workerClosed()
{
    if (RefPtr serviceWorkerConnection = m_serviceWorkerConnection.get())
        serviceWorkerConnection->unregisterFetch(*this);
    contextClosed();
}

void ServiceWorkerFetchTask::contextClosed()
{
    SWFETCH_RELEASE_LOG("contextClosed: (m_isDone=%d, m_wasHandled=%d)", m_isDone, m_wasHandled);
    m_serviceWorkerConnection = nullptr;
    if (m_isDone)
        return;

    if (m_wasHandled && !m_isLoadingFromPreloader) {
        didFail(ResourceError { errorDomainWebKitInternal, 0, { }, "Service Worker context closed"_s });
        return;
    }
    cannotHandle();
}

void ServiceWorkerFetchTask::startFetch()
{
    SWFETCH_RELEASE_LOG("startFetch");
    Ref loader = *m_loader;
    loader->consumeSandboxExtensionsIfNeeded();
    auto& options = loader->parameters().options;
    auto referrer = m_currentRequest.httpReferrer();

    // We are intercepting fetch calls after going through the HTTP layer, which may add some specific headers.
    auto request = m_currentRequest;
    cleanHTTPRequestHeadersForAccessControl(request, loader->parameters().httpHeadersToKeep);

    String clientIdentifier;
    if (loader->parameters().options.mode != FetchOptions::Mode::Navigate) {
        if (auto identifier = loader->parameters().options.clientIdentifier)
            clientIdentifier = identifier->toString();
    }
    String resultingClientIdentifier;
    if (auto& identifier = loader->parameters().options.resultingClientIdentifier)
        resultingClientIdentifier = identifier->toString();

    bool isSent = sendToServiceWorker(Messages::WebSWContextManagerConnection::StartFetch { *m_serverConnectionIdentifier, *m_serviceWorkerIdentifier, m_fetchIdentifier, request, options, IPC::FormDataReference { m_currentRequest.httpBody() }, referrer, m_preloader && m_preloader->isServiceWorkerNavigationPreloadEnabled(), clientIdentifier, resultingClientIdentifier });
    ASSERT_UNUSED(isSent, isSent);

    if (m_preloader && m_preloader->didReceiveResponseOrError())
        sendNavigationPreloadUpdate();
}

void ServiceWorkerFetchTask::didReceiveRedirectResponse(WebCore::ResourceResponse&& response)
{
    cancelPreloadIfNecessary();

    processRedirectResponse(WTFMove(response), ShouldSetSource::Yes);
}

void ServiceWorkerFetchTask::processRedirectResponse(ResourceResponse&& response, ShouldSetSource shouldSetSource)
{
    if (m_isDone)
        return;

    SWFETCH_RELEASE_LOG("processRedirectResponse:");
    m_wasHandled = true;
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
    softUpdateIfNeeded();

    if (shouldSetSource == ShouldSetSource::Yes)
        response.setSource(ResourceResponse::Source::ServiceWorker);
    Ref loader = *m_loader;
    auto newRequest = m_currentRequest.redirectedRequest(response, loader->parameters().shouldClearReferrerOnHTTPSToHTTPRedirect, ResourceRequest::ShouldSetHash::Yes);
    loader->willSendServiceWorkerRedirectedRequest(ResourceRequest(m_currentRequest), WTFMove(newRequest), WTFMove(response));
}

void ServiceWorkerFetchTask::didReceiveResponse(WebCore::ResourceResponse&& response, bool needsContinueDidReceiveResponseMessage)
{
    if (m_preloader && !m_preloader->isServiceWorkerNavigationPreloadEnabled())
        cancelPreloadIfNecessary();

    processResponse(WTFMove(response), needsContinueDidReceiveResponseMessage, ShouldSetSource::Yes);
}

void ServiceWorkerFetchTask::processResponse(ResourceResponse&& response, bool needsContinueDidReceiveResponseMessage, ShouldSetSource shouldSetSource)
{
    if (m_isDone)
        return;

    Ref loader = *m_loader;
#if ENABLE(CONTENT_FILTERING)
    if (!loader->continueAfterServiceWorkerReceivedResponse(response))
        return;
#endif

    SWFETCH_RELEASE_LOG("processResponse: (httpStatusCode=%d, MIMEType=%" PUBLIC_LOG_STRING ", expectedContentLength=%lld, needsContinueDidReceiveResponseMessage=%d, source=%u)", response.httpStatusCode(), response.mimeType().utf8().data(), response.expectedContentLength(), needsContinueDidReceiveResponseMessage, static_cast<unsigned>(response.source()));
    m_wasHandled = true;
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
    softUpdateIfNeeded();

    if (loader->parameters().options.mode == FetchOptions::Mode::Navigate) {
        if (auto parentOrigin = loader->parameters().parentOrigin()) {
            if (auto error = validateCrossOriginResourcePolicy(loader->parameters().parentCrossOriginEmbedderPolicy.value, *parentOrigin, m_currentRequest.url(), response, ForNavigation::Yes, loader->connectionToWebProcess().originAccessPatterns())) {
                didFail(*error);
                return;
            }
        }
    }
    if (loader->parameters().options.mode == FetchOptions::Mode::NoCors) {
        Ref sourceOrigin = *loader->parameters().sourceOrigin;
        if (auto error = validateCrossOriginResourcePolicy(loader->parameters().crossOriginEmbedderPolicy.value, sourceOrigin, m_currentRequest.url(), response, ForNavigation::No, loader->connectionToWebProcess().originAccessPatterns())) {
            didFail(*error);
            return;
        }
    }

    if (auto error = loader->doCrossOriginOpenerHandlingOfResponse(response)) {
        didFail(*error);
        return;
    }

    if (shouldSetSource == ShouldSetSource::Yes)
        response.setSource(ResourceResponse::Source::ServiceWorker);
    loader->sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup(response, PrivateRelayed::No, needsContinueDidReceiveResponseMessage);
    if (needsContinueDidReceiveResponseMessage)
        loader->setResponse(WTFMove(response));
}

void ServiceWorkerFetchTask::didReceiveData(const IPC::SharedBufferReference& data, uint64_t encodedDataLength)
{
    if (m_isDone)
        return;

    ASSERT(!m_timeoutTimer || !m_timeoutTimer->isActive());

#if ENABLE(CONTENT_FILTERING)
    RefPtr buffer = data.unsafeBuffer();
    if (!buffer)
        return;
    if (!protectedLoader()->continueAfterServiceWorkerReceivedData(*buffer, encodedDataLength))
        return;
#endif
    sendToClient(Messages::WebResourceLoader::DidReceiveData { IPC::SharedBufferReference(data), encodedDataLength });
}

void ServiceWorkerFetchTask::didReceiveDataFromPreloader(const WebCore::FragmentedSharedBuffer& data, uint64_t encodedDataLength)
{
    if (m_isDone)
        return;

    ASSERT(!m_timeoutTimer || !m_timeoutTimer->isActive());

#if ENABLE(CONTENT_FILTERING)
    RefPtr buffer = data.makeContiguous();
    if (!buffer)
        return;
    if (!protectedLoader()->continueAfterServiceWorkerReceivedData(*buffer, encodedDataLength))
        return;
#endif
    sendToClient(Messages::WebResourceLoader::DidReceiveData { IPC::SharedBufferReference(data), encodedDataLength });
}

void ServiceWorkerFetchTask::didReceiveFormData(const IPC::FormDataReference& formData)
{
    if (m_isDone)
        return;

    ASSERT(!m_timeoutTimer || !m_timeoutTimer->isActive());
    // FIXME: Allow WebResourceLoader to receive form data.
}

void ServiceWorkerFetchTask::didFinish(const NetworkLoadMetrics& networkLoadMetrics)
{
    ASSERT(!m_timeoutTimer || !m_timeoutTimer->isActive());
    SWFETCH_RELEASE_LOG("didFinish:");

    m_isDone = true;
    if (m_timeoutTimer)
        m_timeoutTimer->stop();

#if ENABLE(CONTENT_FILTERING)
    protectedLoader()->serviceWorkerDidFinish();
#endif

    sendToClient(Messages::WebResourceLoader::DidFinishResourceLoad { networkLoadMetrics });

    cancelPreloadIfNecessary();
}

void ServiceWorkerFetchTask::didFail(const ResourceError& error)
{
    m_isDone = true;
    if (m_timeoutTimer && m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
        softUpdateIfNeeded();
    }
    cancelPreloadIfNecessary();

    SWFETCH_RELEASE_LOG_ERROR("didFail: (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d)", error.domain().utf8().data(), error.errorCode());
    protectedLoader()->didFailLoading(error);
}

void ServiceWorkerFetchTask::didNotHandle()
{
    if (m_isDone)
        return;

    SWFETCH_RELEASE_LOG("didNotHandle:");
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
    softUpdateIfNeeded();

    if (m_preloader && !m_preloader->isServiceWorkerNavigationPreloadEnabled()) {
        loadResponseFromPreloader();
        return;
    }

    m_isDone = true;
    protectedLoader()->serviceWorkerDidNotHandle(this);
}

void ServiceWorkerFetchTask::usePreload()
{
    if (m_isDone)
        return;

    ASSERT(m_preloader);
    if (m_preloader) {
        loadResponseFromPreloader();
        return;
    }

    m_isDone = true;
    protectedLoader()->serviceWorkerDidNotHandle(this);
}

void ServiceWorkerFetchTask::cannotHandle()
{
    SWFETCH_RELEASE_LOG("cannotHandle:");
    // Make sure we call didNotHandle asynchronously because failing synchronously would get the NetworkResourceLoader in a bad state.
    RunLoop::protectedMain()->dispatch([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didNotHandle();
    });
}

void ServiceWorkerFetchTask::cancelFromClient()
{
    SWFETCH_RELEASE_LOG("cancelFromClient: isDone=%d", m_isDone);
    if (m_isDone)
        return;

    if (m_isLoadingFromPreloader) {
        cancelPreloadIfNecessary();
        return;
    }

    sendToServiceWorker(Messages::WebSWContextManagerConnection::CancelFetch { *m_serverConnectionIdentifier, *m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueDidReceiveFetchResponse()
{
    SWFETCH_RELEASE_LOG("continueDidReceiveFetchResponse:");
    if (m_isLoadingFromPreloader) {
        loadBodyFromPreloader();
        return;
    }
    sendToServiceWorker(Messages::WebSWContextManagerConnection::ContinueDidReceiveFetchResponse { *m_serverConnectionIdentifier, *m_serviceWorkerIdentifier, m_fetchIdentifier });
}

void ServiceWorkerFetchTask::continueFetchTaskWith(ResourceRequest&& request)
{
    SWFETCH_RELEASE_LOG("continueFetchTaskWith: (hasServiceWorkerConnection=%d)", !!m_serviceWorkerConnection);
    Ref loader = *m_loader;
    if (!m_serviceWorkerConnection) {
        loader->serviceWorkerDidNotHandle(this);
        return;
    }
    if (m_timeoutTimer)
        m_timeoutTimer->startOneShot(loader->connectionToWebProcess().networkProcess().serviceWorkerFetchTimeout());
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

    if (CheckedPtr swServerConnection = m_swServerConnection.get())
        swServerConnection->fetchTaskTimedOut(*serviceWorkerIdentifier());
}

void ServiceWorkerFetchTask::softUpdateIfNeeded()
{
    SWFETCH_RELEASE_LOG("softUpdateIfNeeded: (m_shouldSoftUpdate=%d)", m_shouldSoftUpdate);
    if (!m_shouldSoftUpdate)
        return;
    Ref loader = *m_loader;
    CheckedPtr swConnection = loader->connectionToWebProcess().swConnection();
    if (!swConnection)
        return;
    if (RefPtr registration = swConnection->protectedServer()->getRegistration(*m_serviceWorkerRegistrationIdentifier))
        registration->scheduleSoftUpdate(loader->isAppInitiated() ? WebCore::IsAppInitiated::Yes : WebCore::IsAppInitiated::No);
}

void ServiceWorkerFetchTask::loadResponseFromPreloader()
{
    SWFETCH_RELEASE_LOG("loadResponseFromPreloader");

    if (m_isLoadingFromPreloader)
        return;

    m_isLoadingFromPreloader = true;
    m_preloader->waitForResponse([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->preloadResponseIsReady();
    });
}

void ServiceWorkerFetchTask::preloadResponseIsReady()
{
    if (!m_isLoadingFromPreloader) {
        if (m_preloader && m_preloader->isServiceWorkerNavigationPreloadEnabled() && m_serviceWorkerConnection)
            sendNavigationPreloadUpdate();
        return;
    }

    if (!m_preloader->error().isNull()) {
        // Let's copy the error as calling didFail might destroy m_preloader.
        didFail(ResourceError { m_preloader->error() });
        return;
    }

    auto response = m_preloader->response();
    if (response.isRedirection() && response.httpHeaderFields().contains(HTTPHeaderName::Location)) {
        processRedirectResponse(WTFMove(response), ShouldSetSource::No);
        return;
    }

    bool needsContinueDidReceiveResponseMessage = true;
    processResponse(WTFMove(response), needsContinueDidReceiveResponseMessage, ShouldSetSource::No);
}

void ServiceWorkerFetchTask::sendNavigationPreloadUpdate()
{
    ASSERT(!!m_serviceWorkerConnection);

    if (!m_preloader->error().isNull())
        sendToServiceWorker(Messages::WebSWContextManagerConnection::NavigationPreloadFailed { *m_serverConnectionIdentifier, *m_serviceWorkerIdentifier, m_fetchIdentifier, m_preloader->error() });
    else
        sendToServiceWorker(Messages::WebSWContextManagerConnection::NavigationPreloadIsReady { *m_serverConnectionIdentifier, *m_serviceWorkerIdentifier, m_fetchIdentifier, m_preloader->response() });
}

void ServiceWorkerFetchTask::loadBodyFromPreloader()
{
    SWFETCH_RELEASE_LOG("loadBodyFromPreloader");

    ASSERT(m_isLoadingFromPreloader);
    if (!m_preloader) {
        SWFETCH_RELEASE_LOG_ERROR("loadBodyFromPreloader preloader is null");
        didFail(ResourceError(errorDomainWebKitInternal, 0, m_currentRequest.url(), "Request canceled from preloader"_s, ResourceError::Type::Cancellation));
        return;
    }

    m_preloader->waitForBody([this, weakThis = WeakPtr { *this }](auto&& chunk, uint64_t length) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!m_preloader->error().isNull()) {
            // Let's copy the error as calling didFail might destroy m_preloader.
            didFail(ResourceError { m_preloader->error() });
            return;
        }
        if (!chunk) {
            didFinish(m_preloader->networkLoadMetrics());
            return;
        }
        didReceiveDataFromPreloader(const_cast<WebCore::FragmentedSharedBuffer&>(*chunk), length);
    });
}

void ServiceWorkerFetchTask::cancelPreloadIfNecessary()
{
    if (!m_preloader)
        return;

    if (auto* session = this->session())
        session->removeNavigationPreloaderTask(*this);

    m_preloader->cancel();
    m_preloader = nullptr;
}

NetworkSession* ServiceWorkerFetchTask::session()
{
    return m_swServerConnection ? m_swServerConnection->session() : nullptr;
}

bool ServiceWorkerFetchTask::convertToDownload(DownloadManager& manager, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    if (m_preloader)
        return m_preloader->convertToDownload(manager, downloadID, request, response);

    CheckedPtr session = this->session();
    if (!session || !m_serviceWorkerConnection)
        return false;

    m_isDone = true;

    // FIXME: We might want to keep the service worker alive until the download ends.
    RefPtr<ServiceWorkerDownloadTask> serviceWorkerDownloadTask;
    auto serviceWorkerDownloadLoad = NetworkLoad::create(*protectedLoader(), *session, [&](auto& client) {
        serviceWorkerDownloadTask =  ServiceWorkerDownloadTask::create(*session, client, *m_serviceWorkerConnection, *m_serviceWorkerIdentifier, *m_serverConnectionIdentifier, m_fetchIdentifier, request, response, downloadID);
        return serviceWorkerDownloadTask.copyRef();
    });

    ResponseCompletionHandler completionHandler = [serviceWorkerDownloadTask = WTFMove(serviceWorkerDownloadTask)](auto policy) {
        if (policy != PolicyAction::Download) {
            serviceWorkerDownloadTask->stop();
            return;
        }
        serviceWorkerDownloadTask->start();
    };

    manager.convertNetworkLoadToDownload(downloadID, WTFMove(serviceWorkerDownloadLoad), WTFMove(completionHandler), { }, request, response);
    return true;
}

MonotonicTime ServiceWorkerFetchTask::startTime() const
{
    return m_preloader ? m_preloader->startTime() : MonotonicTime { };
}

RefPtr<NetworkResourceLoader> ServiceWorkerFetchTask::protectedLoader() const
{
    return m_loader.get();
}

} // namespace WebKit

#undef SWFETCH_RELEASE_LOG
#undef SWFETCH_RELEASE_LOG_ERROR

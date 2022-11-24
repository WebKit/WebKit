/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "WebSWContextManagerConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessMessages.h"
#include "RemoteWebLockRegistry.h"
#include "RemoteWorkerFrameLoaderClient.h"
#include "RemoteWorkerInitializationData.h"
#include "RemoteWorkerLibWebRTCProvider.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCacheStorageProvider.h"
#include "WebCompiledContentRuleListData.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseProvider.h"
#include "WebDocumentLoader.h"
#include "WebFrameLoaderClient.h"
#include "WebMessagePortChannelProvider.h"
#include "WebNotificationClient.h"
#include "WebPage.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebServiceWorkerFetchTaskClient.h"
#include "WebSocketProvider.h"
#include "WebUserContentController.h"
#include <WebCore/EditorClient.h>
#include <WebCore/EmptyClients.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/NotificationData.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientQueryOptions.h>
#include <WebCore/ServiceWorkerJobDataIdentifier.h>
#include <WebCore/UserAgent.h>
#include <WebCore/UserContentURLPattern.h>
#include <wtf/ProcessID.h>

#if USE(QUICK_LOOK)
#include <WebCore/LegacyPreviewLoaderClient.h>
#endif

namespace WebKit {
using namespace PAL;
using namespace WebCore;

WebSWContextManagerConnection::WebSWContextManagerConnection(Ref<IPC::Connection>&& connection, RegistrableDomain&& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, RemoteWorkerInitializationData&& initializationData)
    : m_connectionToNetworkProcess(WTFMove(connection))
    , m_registrableDomain(WTFMove(registrableDomain))
    , m_serviceWorkerPageIdentifier(serviceWorkerPageIdentifier)
    , m_pageGroupID(pageGroupID)
    , m_webPageProxyID(webPageProxyID)
    , m_pageID(pageID)
#if PLATFORM(COCOA)
    , m_userAgent(standardUserAgentWithApplicationName({ }))
#else
    , m_userAgent(standardUserAgent())
#endif
    , m_userContentController(WebUserContentController::getOrCreate(initializationData.userContentControllerIdentifier))
    , m_queue(WorkQueue::create("WebSWContextManagerConnection queue", WorkQueue::QOS::UserInitiated))
{
#if ENABLE(CONTENT_EXTENSIONS)
    m_userContentController->addContentRuleLists(WTFMove(initializationData.contentRuleLists));
#endif

    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = store;

    WebProcess::singleton().disableTermination();
}

WebSWContextManagerConnection::~WebSWContextManagerConnection()
{
    auto callbacks = WTFMove(m_skipWaitingCallbacks);
    for (auto& callback : callbacks.values())
        callback();
}

void WebSWContextManagerConnection::establishConnection(CompletionHandler<void()>&& completionHandler)
{
    m_connectionToNetworkProcess->addWorkQueueMessageReceiver(Messages::WebSWContextManagerConnection::messageReceiverName(), m_queue.get(), *this);
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::EstablishSWContextConnection { m_webPageProxyID, m_registrableDomain, m_serviceWorkerPageIdentifier }, WTFMove(completionHandler), 0);
}

void WebSWContextManagerConnection::stop()
{
    ASSERT(isMainRunLoop());

    m_connectionToNetworkProcess->removeWorkQueueMessageReceiver(Messages::WebSWContextManagerConnection::messageReceiverName());
}

void WebSWContextManagerConnection::updatePreferencesStore(WebPreferencesStore&& store)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, store = WTFMove(store).isolatedCopy()]() mutable {
            protectedThis->updatePreferencesStore(WTFMove(store));
        });
        return;
    }

    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = WTFMove(store);
}

void WebSWContextManagerConnection::updateAppInitiatedValue(ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    ASSERT(isMainRunLoop());

    if (auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(lastNavigationWasAppInitiated == WebCore::LastNavigationWasAppInitiated::Yes);
}

void WebSWContextManagerConnection::installServiceWorker(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, String&& userAgent, WorkerThreadMode workerThreadMode)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, contextData = WTFMove(contextData).isolatedCopy(), workerData = WTFMove(workerData).isolatedCopy(), userAgent = WTFMove(userAgent).isolatedCopy(), workerThreadMode]() mutable {
    auto pageConfiguration = pageConfigurationWithEmptyClients(WebProcess::singleton().sessionID());
        pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroupID);
        pageConfiguration.socketProvider = WebSocketProvider::create(m_webPageProxyID);
        pageConfiguration.broadcastChannelRegistry = WebProcess::singleton().broadcastChannelRegistry();
        pageConfiguration.userContentProvider = m_userContentController;
#if ENABLE(WEB_RTC)
        pageConfiguration.webRTCProvider = makeUniqueRef<RemoteWorkerLibWebRTCProvider>();
#endif
        if (auto* serviceWorkerPage = m_serviceWorkerPageIdentifier ? Page::serviceWorkerPage(*m_serviceWorkerPageIdentifier) : nullptr)
            pageConfiguration.corsDisablingPatterns = serviceWorkerPage->corsDisablingPatterns();

        auto effectiveUserAgent =  WTFMove(userAgent);
        if (effectiveUserAgent.isNull())
            effectiveUserAgent = m_userAgent;

        auto loaderClientForMainFrame = makeUniqueRef<RemoteWorkerFrameLoaderClient>(m_webPageProxyID, m_pageID, effectiveUserAgent);
        if (contextData.serviceWorkerPageIdentifier)
            loaderClientForMainFrame->setServiceWorkerPageIdentifier(*contextData.serviceWorkerPageIdentifier);
        pageConfiguration.loaderClientForMainFrame = WTFMove(loaderClientForMainFrame);

#if !RELEASE_LOG_DISABLED
        auto serviceWorkerIdentifier = contextData.serviceWorkerIdentifier;
#endif

        auto lastNavigationWasAppInitiated = contextData.lastNavigationWasAppInitiated;
        auto page = makeUniqueRef<Page>(WTFMove(pageConfiguration));
        if (m_preferencesStore) {
            WebPage::updateSettingsGenerated(*m_preferencesStore, page->settings());
            page->settings().setStorageBlockingPolicy(static_cast<StorageBlockingPolicy>(m_preferencesStore->getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey())));
        }
        page->setupForRemoteWorker(contextData.scriptURL, contextData.registration.key.topOrigin(), contextData.referrerPolicy);

        std::unique_ptr<WebCore::NotificationClient> notificationClient;
#if ENABLE(NOTIFICATIONS)
        notificationClient = makeUnique<WebNotificationClient>(nullptr);
#endif

        auto serviceWorkerThreadProxy = ServiceWorkerThreadProxy::create(WTFMove(page), WTFMove(contextData), WTFMove(workerData), WTFMove(effectiveUserAgent), workerThreadMode, WebProcess::singleton().cacheStorageProvider(), WTFMove(notificationClient));

        if (lastNavigationWasAppInitiated)
            serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(lastNavigationWasAppInitiated == WebCore::LastNavigationWasAppInitiated::Yes);

        SWContextManager::singleton().registerServiceWorkerThreadForInstall(WTFMove(serviceWorkerThreadProxy));

        RELEASE_LOG(ServiceWorker, "Created service worker %" PRIu64 " in process PID %i", serviceWorkerIdentifier.toUInt64(), getCurrentProcessID());
    });
}

void WebSWContextManagerConnection::setUserAgent(String&& userAgent)
{
    if (!isMainThread()) {
        callOnMainRunLoop([protectedThis = Ref { *this }, userAgent = WTFMove(userAgent).isolatedCopy()]() mutable {
            protectedThis->setUserAgent(WTFMove(userAgent));
        });
        return;
    }
    m_userAgent = WTFMove(userAgent);
}

void WebSWContextManagerConnection::serviceWorkerStarted(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool doesHandleFetch)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextStarted { jobDataIdentifier, serviceWorkerIdentifier, doesHandleFetch }, 0);
}

void WebSWContextManagerConnection::serviceWorkerFailedToStart(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& exceptionMessage)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextFailedToStart { jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage }, 0);
}

void WebSWContextManagerConnection::cancelFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->cancelFetch(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::continueDidReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->continueDidReceiveFetchResponse(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::startFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData, String&& referrer, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    assertIsCurrent(m_queue.get());

    auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier);
    if (!serviceWorkerThreadProxy) {
        m_connectionToNetworkProcess->send(Messages::ServiceWorkerFetchTask::DidNotHandle { }, fetchIdentifier);
        return;
    }

    callOnMainRunLoop([serviceWorkerThreadProxy, isAppInitiated = request.isAppInitiated()]() mutable {
        serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(isAppInitiated);
    });

    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToNetworkProcess.copyRef(), serviceWorkerIdentifier, serverConnectionIdentifier, fetchIdentifier, request.requester() == ResourceRequestRequester::Main);

    request.setHTTPBody(formData.takeData());
    serviceWorkerThreadProxy->startFetch(serverConnectionIdentifier, fetchIdentifier, WTFMove(client), WTFMove(request), WTFMove(referrer), WTFMove(options), isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
}

void WebSWContextManagerConnection::postMessageToServiceWorker(ServiceWorkerIdentifier serviceWorkerIdentifier, MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->fireMessageEvent(WTFMove(message), WTFMove(sourceData));
}

void WebSWContextManagerConnection::fireInstallEvent(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([identifier] {
        SWContextManager::singleton().fireInstallEvent(identifier);
    });
}

void WebSWContextManagerConnection::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([identifier] {
        SWContextManager::singleton().fireActivateEvent(identifier);
    });
}

void WebSWContextManagerConnection::firePushEvent(ServiceWorkerIdentifier identifier, const std::optional<IPC::DataReference>& ipcData, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(m_queue.get());

    std::optional<Vector<uint8_t>> data;
    if (ipcData)
        data = Vector<uint8_t> { ipcData->data(), ipcData->size() };

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result) mutable {
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    };

    callOnMainRunLoop([identifier, data = WTFMove(data), callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().firePushEvent(identifier, WTFMove(data), WTFMove(callback));
    });
}

void WebSWContextManagerConnection::fireNotificationEvent(ServiceWorkerIdentifier identifier, NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    assertIsCurrent(m_queue.get());

    auto inQueueCallback = [queue = m_queue, callback = WTFMove(callback)](bool result) mutable {
        queue->dispatch([result, callback = WTFMove(callback)]() mutable {
            callback(result);
        });
    };
    callOnMainRunLoop([identifier, data = WTFMove(data), eventType, callback = WTFMove(inQueueCallback)]() mutable {
        SWContextManager::singleton().fireNotificationEvent(identifier, WTFMove(data), eventType, WTFMove(callback));
    });
}

void WebSWContextManagerConnection::terminateWorker(ServiceWorkerIdentifier identifier)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([identifier] {
        SWContextManager::singleton().terminateWorker(identifier, SWContextManager::workerTerminationTimeout, nullptr);
    });
}

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
void WebSWContextManagerConnection::didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->didSaveScriptsToDisk(WTFMove(script), WTFMove(importedScripts));
}
#endif

void WebSWContextManagerConnection::convertFetchToDownload(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->convertFetchToDownload(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::navigationPreloadIsReady(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceResponse&& response)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->navigationPreloadIsReady(serverConnectionIdentifier, fetchIdentifier, WTFMove(response));
}

void WebSWContextManagerConnection::navigationPreloadFailed(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceError&& error)
{
    assertIsCurrent(m_queue.get());

    if (auto serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxyFromBackgroundThread(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->navigationPreloadFailed(serverConnectionIdentifier, fetchIdentifier, WTFMove(error));
}

void WebSWContextManagerConnection::postMessageToServiceWorkerClient(const ScriptExecutionContextIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    for (auto& port : message.transferredPorts)
        WebMessagePortChannelProvider::singleton().messagePortSentToRemote(port.first);

    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::PostMessageToServiceWorkerClient(destinationIdentifier, message, sourceIdentifier, sourceOrigin), 0);
}

void WebSWContextManagerConnection::didFinishInstall(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFinishInstall(jobDataIdentifier, serviceWorkerIdentifier, wasSuccessful), 0);
}

void WebSWContextManagerConnection::didFinishActivation(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFinishActivation(serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetServiceWorkerHasPendingEvents(serviceWorkerIdentifier, hasPendingEvents), 0);
}

void WebSWContextManagerConnection::skipWaiting(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& callback)
{
    auto requestIdentifier = ++m_previousRequestIdentifier;
    m_skipWaitingCallbacks.add(requestIdentifier, WTFMove(callback));
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SkipWaiting(requestIdentifier, serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::skipWaitingCompleted(uint64_t requestIdentifier)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([protectedThis = Ref { *this }, requestIdentifier]() mutable {
        if (auto callback = protectedThis->m_skipWaitingCallbacks.take(requestIdentifier))
            callback();
    });
}

void WebSWContextManagerConnection::setScriptResource(ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, const ServiceWorkerContextData::ImportedScript& script)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetScriptResource { serviceWorkerIdentifier, url, script }, 0);
}

void WebSWContextManagerConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::WorkerTerminated(serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::findClientByVisibleIdentifier(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const String& clientIdentifier, FindClientByIdentifierCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::FindClientByVisibleIdentifier { serviceWorkerIdentifier, clientIdentifier }, WTFMove(callback));
}

void WebSWContextManagerConnection::matchAll(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const ServiceWorkerClientQueryOptions& options, ServiceWorkerClientsMatchAllCallback&& callback)
{
    auto requestIdentifier = ++m_previousRequestIdentifier;
    m_matchAllRequests.add(requestIdentifier, WTFMove(callback));
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::MatchAll { requestIdentifier, serviceWorkerIdentifier, options }, 0);
}

void WebSWContextManagerConnection::matchAllCompleted(uint64_t requestIdentifier, Vector<ServiceWorkerClientData>&& clientsData)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([protectedThis = Ref { *this }, requestIdentifier, clientsData = crossThreadCopy(WTFMove(clientsData))]() mutable {
        if (auto callback = protectedThis->m_matchAllRequests.take(requestIdentifier))
            callback(WTFMove(clientsData));
    });
}

void WebSWContextManagerConnection::openWindow(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, OpenWindowCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::OpenWindow { serviceWorkerIdentifier, url }, [callback = WTFMove(callback)] (auto&& result) mutable {
        if (!result.has_value()) {
            callback(result.error().toException());
            return;
        }
        callback(WTFMove(result.value()));
    });
}

void WebSWContextManagerConnection::claim(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(ExceptionOr<void>&&)>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Claim { serviceWorkerIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        callback(result ? result->toException() : ExceptionOr<void> { });
    });
}

void WebSWContextManagerConnection::navigate(ScriptExecutionContextIdentifier clientIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, NavigateCallback&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Navigate { clientIdentifier, serviceWorkerIdentifier, url }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value()) {
            callback(WTFMove(result).error().toException());
            return;
        }
        callback(WTFMove(result).value());
    });
}

void WebSWContextManagerConnection::focus(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Focus { clientIdentifier }, WTFMove(callback));
}

void WebSWContextManagerConnection::close()
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedThis = Ref { *this }] {
            protectedThis->close();
        });
        return;
    }

    RELEASE_LOG(ServiceWorker, "Service worker process is requested to stop all service workers (already stopped = %d)", isClosed());
    if (isClosed())
        return;

    setAsClosed();

    m_connectionToNetworkProcess->send(Messages::NetworkConnectionToWebProcess::CloseSWContextConnection { }, 0);
    SWContextManager::singleton().stopAllServiceWorkers();
    WebProcess::singleton().enableTermination();
}

void WebSWContextManagerConnection::setThrottleState(bool isThrottleable)
{
    assertIsCurrent(m_queue.get());

    callOnMainRunLoop([protectedThis = Ref { *this }, isThrottleable] {
        RELEASE_LOG(ServiceWorker, "Service worker throttleable state is set to %d", isThrottleable);
        protectedThis->m_isThrottleable = isThrottleable;
        WebProcess::singleton().setProcessSuppressionEnabled(isThrottleable);
    });
}

bool WebSWContextManagerConnection::isThrottleable() const
{
    return m_isThrottleable;
}

void WebSWContextManagerConnection::didFailHeartBeatCheck(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFailHeartBeatCheck { serviceWorkerIdentifier }, 0);
}

void WebSWContextManagerConnection::setAsInspected(WebCore::ServiceWorkerIdentifier identifier, bool isInspected)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetAsInspected { identifier, isInspected }, 0);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

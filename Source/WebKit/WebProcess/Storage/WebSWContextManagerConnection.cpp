/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <WebCore/PageConfiguration.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientQueryOptions.h>
#include <WebCore/ServiceWorkerJobDataIdentifier.h>
#include <WebCore/UserAgent.h>
#include <wtf/ProcessID.h>

#if USE(QUICK_LOOK)
#include <WebCore/LegacyPreviewLoaderClient.h>
#endif

namespace WebKit {
using namespace PAL;
using namespace WebCore;

WebSWContextManagerConnection::WebSWContextManagerConnection(Ref<IPC::Connection>&& connection, WebCore::RegistrableDomain&& registrableDomain, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PageGroupIdentifier pageGroupID, WebPageProxyIdentifier webPageProxyID, PageIdentifier pageID, const WebPreferencesStore& store, RemoteWorkerInitializationData&& initializationData)
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
{
#if ENABLE(CONTENT_EXTENSIONS)
    m_userContentController->addContentRuleLists(WTFMove(initializationData.contentRuleLists));
#endif

    updatePreferencesStore(store);
    WebProcess::singleton().disableTermination();
}

WebSWContextManagerConnection::~WebSWContextManagerConnection() = default;

void WebSWContextManagerConnection::establishConnection(CompletionHandler<void()>&& completionHandler)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::EstablishSWContextConnection { m_webPageProxyID, m_registrableDomain, m_serviceWorkerPageIdentifier }, WTFMove(completionHandler), 0);
}

void WebSWContextManagerConnection::updatePreferencesStore(const WebPreferencesStore& store)
{
    WebPage::updatePreferencesGenerated(store);
    m_preferencesStore = store;
}

void WebSWContextManagerConnection::updateAppInitiatedValue(ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    if (auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(lastNavigationWasAppInitiated == WebCore::LastNavigationWasAppInitiated::Yes);
}

void WebSWContextManagerConnection::installServiceWorker(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, String&& userAgent, WorkerThreadMode workerThreadMode)
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(WebProcess::singleton().sessionID());

    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroupID);
    pageConfiguration.socketProvider = WebSocketProvider::create(m_webPageProxyID);
    pageConfiguration.broadcastChannelRegistry = WebProcess::singleton().broadcastChannelRegistry();
    pageConfiguration.webLockRegistry = WebProcess::singleton().webLockRegistry();
    pageConfiguration.userContentProvider = m_userContentController;
#if ENABLE(WEB_RTC)
    pageConfiguration.libWebRTCProvider = makeUniqueRef<RemoteWorkerLibWebRTCProvider>();
#endif

    auto effectiveUserAgent =  WTFMove(userAgent);
    if (effectiveUserAgent.isNull())
        effectiveUserAgent = m_userAgent;

    pageConfiguration.loaderClientForMainFrame = makeUniqueRef<RemoteWorkerFrameLoaderClient>(m_webPageProxyID, m_pageID, FrameIdentifier::generate(), effectiveUserAgent);

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
}

void WebSWContextManagerConnection::setUserAgent(String&& userAgent)
{
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

static inline bool isValidFetch(const ResourceRequest& request, const FetchOptions& options, const URL& serviceWorkerURL, const String& referrer)
{
    // For exotic service workers, do not enforce checks.
    if (!serviceWorkerURL.protocolIsInHTTPFamily())
        return true;

    if (options.mode == FetchOptions::Mode::Navigate) {
        if (!protocolHostAndPortAreEqual(request.url(), serviceWorkerURL)) {
            RELEASE_LOG_ERROR(ServiceWorker, "Should not intercept a navigation load that is not same-origin as the service worker URL");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().host() == serviceWorkerURL.host(), "Hosts do not match");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().protocol() == serviceWorkerURL.protocol(), "Protocols do not match");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().port() == serviceWorkerURL.port(), "Ports do not match");
            return false;
        }
        return true;
    }

    String origin = request.httpOrigin();
    URL url { URL(), origin.isEmpty() ? referrer : origin };
    if (url.protocolIsInHTTPFamily() && !protocolHostAndPortAreEqual(url, serviceWorkerURL)) {
        RELEASE_LOG_ERROR(ServiceWorker, "Should not intercept a non navigation load that is not originating from a same-origin context as the service worker URL");
        ASSERT(url.host() == serviceWorkerURL.host());
        ASSERT(url.protocol() == serviceWorkerURL.protocol());
        ASSERT(url.port() == serviceWorkerURL.port());
        return false;
    }
    return true;
}

void WebSWContextManagerConnection::cancelFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    if (auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->cancelFetch(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::continueDidReceiveFetchResponse(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier);
    RELEASE_LOG(ServiceWorker, "WebSWContextManagerConnection::continueDidReceiveFetchResponse for service worker %llu, fetch identifier %llu, has service worker %d", serviceWorkerIdentifier.toUInt64(), fetchIdentifier.toUInt64(), !!serviceWorkerThreadProxy);

    if (serviceWorkerThreadProxy)
        serviceWorkerThreadProxy->continueDidReceiveFetchResponse(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::startFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData, String&& referrer, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier);
    if (!serviceWorkerThreadProxy) {
        m_connectionToNetworkProcess->send(Messages::ServiceWorkerFetchTask::DidNotHandle { }, fetchIdentifier);
        return;
    }

    serviceWorkerThreadProxy->setLastNavigationWasAppInitiated(request.isAppInitiated());

    if (!isValidFetch(request, options, serviceWorkerThreadProxy->scriptURL(), referrer)) {
        m_connectionToNetworkProcess->send(Messages::ServiceWorkerFetchTask::DidNotHandle { }, fetchIdentifier);
        return;
    }

    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToNetworkProcess.copyRef(), serviceWorkerIdentifier, serverConnectionIdentifier, fetchIdentifier, request.requester() == ResourceRequest::Requester::Main);

    request.setHTTPBody(formData.takeData());
    serviceWorkerThreadProxy->startFetch(serverConnectionIdentifier, fetchIdentifier, WTFMove(client), WTFMove(request), WTFMove(referrer), WTFMove(options), isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
}

void WebSWContextManagerConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    SWContextManager::singleton().postMessageToServiceWorker(destinationIdentifier, WTFMove(message), WTFMove(sourceData));
}

void WebSWContextManagerConnection::fireInstallEvent(ServiceWorkerIdentifier identifier)
{
    SWContextManager::singleton().fireInstallEvent(identifier);
}

void WebSWContextManagerConnection::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    SWContextManager::singleton().fireActivateEvent(identifier);
}

void WebSWContextManagerConnection::firePushEvent(ServiceWorkerIdentifier identifier, const std::optional<IPC::DataReference>& ipcData, CompletionHandler<void(bool)>&& callback)
{
    std::optional<Vector<uint8_t>> data;
    if (ipcData)
        data = Vector<uint8_t> { ipcData->data(), ipcData->size() };
    SWContextManager::singleton().firePushEvent(identifier, WTFMove(data), WTFMove(callback));
}

void WebSWContextManagerConnection::terminateWorker(ServiceWorkerIdentifier identifier)
{
    SWContextManager::singleton().terminateWorker(identifier, SWContextManager::workerTerminationTimeout, nullptr);
}

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
void WebSWContextManagerConnection::didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier identifier, ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    SWContextManager::singleton().didSaveScriptsToDisk(identifier, WTFMove(script), WTFMove(importedScripts));
}
#endif

void WebSWContextManagerConnection::postMessageToServiceWorkerClient(const ScriptExecutionContextIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
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
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::SkipWaiting(serviceWorkerIdentifier), WTFMove(callback));
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
    if (auto callback = m_matchAllRequests.take(requestIdentifier))
        callback(WTFMove(clientsData));
}

void WebSWContextManagerConnection::claim(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(ExceptionOr<void>&&)>&& callback)
{
    m_connectionToNetworkProcess->sendWithAsyncReply(Messages::WebSWServerToContextConnection::Claim { serviceWorkerIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        callback(result ? result->toException() : ExceptionOr<void> { });
    });
}

void WebSWContextManagerConnection::close()
{
    RELEASE_LOG(ServiceWorker, "Service worker process is requested to stop all service workers");
    setAsClosed();

    m_connectionToNetworkProcess->send(Messages::NetworkConnectionToWebProcess::CloseSWContextConnection { }, 0);
    SWContextManager::singleton().stopAllServiceWorkers();
    WebProcess::singleton().enableTermination();
}

void WebSWContextManagerConnection::setThrottleState(bool isThrottleable)
{
    RELEASE_LOG(ServiceWorker, "Service worker throttleable state is set to %d", isThrottleable);
    m_isThrottleable = isThrottleable;
    WebProcess::singleton().setProcessSuppressionEnabled(isThrottleable);
}

bool WebSWContextManagerConnection::isThrottleable() const
{
    return m_isThrottleable;
}

void WebSWContextManagerConnection::convertFetchToDownload(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier)
{
    if (auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier))
        serviceWorkerThreadProxy->convertFetchToDownload(serverConnectionIdentifier, fetchIdentifier);
}

void WebSWContextManagerConnection::didFailHeartBeatCheck(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::DidFailHeartBeatCheck { serviceWorkerIdentifier }, 0);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

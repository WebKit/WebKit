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
#include "NetworkProcessMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseProvider.h"
#include "WebDocumentLoader.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebServiceWorkerFetchTaskClient.h"
#include "WebSocketProvider.h"
#include <WebCore/EditorClient.h>
#include <WebCore/EmptyClients.h>
#include <WebCore/EmptyFrameLoaderClient.h>
#include <WebCore/LibWebRTCProvider.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/PageConfiguration.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/ServiceWorkerClientQueryOptions.h>
#include <WebCore/ServiceWorkerJobDataIdentifier.h>
#include <WebCore/UserAgent.h>
#include <pal/SessionID.h>
#include <wtf/ProcessID.h>

#if USE(QUICK_LOOK)
#include <WebCore/PreviewLoaderClient.h>
#endif

namespace WebKit {
using namespace PAL;
using namespace WebCore;

static const Seconds asyncWorkerTerminationTimeout { 10_s };
static const Seconds syncWorkerTerminationTimeout { 100_ms }; // Only used by layout tests.

class ServiceWorkerFrameLoaderClient final : public EmptyFrameLoaderClient {
public:
    ServiceWorkerFrameLoaderClient(WebSWContextManagerConnection& connection, PAL::SessionID sessionID, uint64_t pageID, uint64_t frameID, const String& userAgent)
        : m_connection(connection)
        , m_sessionID(sessionID)
        , m_pageID(pageID)
        , m_frameID(frameID)
        , m_userAgent(userAgent)
    {
    }

    void setUserAgent(String&& userAgent) { m_userAgent = WTFMove(userAgent); }

private:
    Ref<DocumentLoader> createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData) final
    {
        return WebDocumentLoader::create(request, substituteData);
    }

    void frameLoaderDestroyed() final { m_connection.removeFrameLoaderClient(*this); }

    bool shouldUseCredentialStorage(DocumentLoader*, unsigned long) final { return true; }

    PAL::SessionID sessionID() const final { return m_sessionID; }
    Optional<uint64_t> pageID() const final { return m_pageID; }
    Optional<uint64_t> frameID() const final { return m_frameID; }
    String userAgent(const URL&) final { return m_userAgent; }

    WebSWContextManagerConnection& m_connection;
    PAL::SessionID m_sessionID;
    uint64_t m_pageID { 0 };
    uint64_t m_frameID { 0 };
    String m_userAgent;
};

WebSWContextManagerConnection::WebSWContextManagerConnection(Ref<IPC::Connection>&& connection, uint64_t pageGroupID, uint64_t pageID, const WebPreferencesStore& store)
    : m_connectionToNetworkProcess(WTFMove(connection))
    , m_pageGroupID(pageGroupID)
    , m_pageID(pageID)
#if PLATFORM(COCOA)
    , m_userAgent(standardUserAgentWithApplicationName({ }))
#else
    , m_userAgent(standardUserAgent())
#endif
{
    updatePreferencesStore(store);
}

WebSWContextManagerConnection::~WebSWContextManagerConnection() = default;

void WebSWContextManagerConnection::updatePreferencesStore(const WebPreferencesStore& store)
{
    RuntimeEnabledFeatures::sharedFeatures().setServiceWorkerEnabled(true);
    RuntimeEnabledFeatures::sharedFeatures().setCacheAPIEnabled(store.getBoolValueForKey(WebPreferencesKey::cacheAPIEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setFetchAPIEnabled(store.getBoolValueForKey(WebPreferencesKey::fetchAPIEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setUserTimingEnabled(store.getBoolValueForKey(WebPreferencesKey::userTimingEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setResourceTimingEnabled(store.getBoolValueForKey(WebPreferencesKey::resourceTimingEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setFetchAPIKeepAliveEnabled(store.getBoolValueForKey(WebPreferencesKey::fetchAPIKeepAliveEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setRestrictedHTTPResponseAccess(store.getBoolValueForKey(WebPreferencesKey::restrictedHTTPResponseAccessKey()));
    RuntimeEnabledFeatures::sharedFeatures().setServerTimingEnabled(store.getBoolValueForKey(WebPreferencesKey::serverTimingEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setIsSecureContextAttributeEnabled(store.getBoolValueForKey(WebPreferencesKey::isSecureContextAttributeEnabledKey()));

    m_storageBlockingPolicy = static_cast<SecurityOrigin::StorageBlockingPolicy>(store.getUInt32ValueForKey(WebPreferencesKey::storageBlockingPolicyKey()));
}

void WebSWContextManagerConnection::installServiceWorker(const ServiceWorkerContextData& data, SessionID sessionID, String&& userAgent)
{
    LOG(ServiceWorker, "WebSWContextManagerConnection::installServiceWorker for worker %s", data.serviceWorkerIdentifier.loggingString().utf8().data());

    auto pageConfiguration = pageConfigurationWithEmptyClients();

#if ENABLE(INDEXED_DATABASE)
    pageConfiguration.databaseProvider = WebDatabaseProvider::getOrCreate(m_pageGroupID);
#endif

    auto effectiveUserAgent =  WTFMove(userAgent);
    if (effectiveUserAgent.isNull())
        effectiveUserAgent = m_userAgent;

    // FIXME: This method should be moved directly to WebCore::SWContextManager::Connection
    // If it weren't for ServiceWorkerFrameLoaderClient's dependence on WebDocumentLoader, this could already happen.
    auto frameLoaderClient = std::make_unique<ServiceWorkerFrameLoaderClient>(*this, sessionID, m_pageID, ++m_previousServiceWorkerID, effectiveUserAgent);
    pageConfiguration.loaderClientForMainFrame = frameLoaderClient.get();
    m_loaders.add(WTFMove(frameLoaderClient));

    auto serviceWorkerThreadProxy = ServiceWorkerThreadProxy::create(WTFMove(pageConfiguration), data, sessionID, WTFMove(effectiveUserAgent), WebProcess::singleton().cacheStorageProvider(), m_storageBlockingPolicy);
    SWContextManager::singleton().registerServiceWorkerThreadForInstall(WTFMove(serviceWorkerThreadProxy));

    LOG(ServiceWorker, "Context process PID: %i created worker thread\n", getCurrentProcessID());
}

void WebSWContextManagerConnection::setUserAgent(String&& userAgent)
{
    m_userAgent = WTFMove(userAgent);
}

void WebSWContextManagerConnection::removeFrameLoaderClient(ServiceWorkerFrameLoaderClient& client)
{
    auto result = m_loaders.remove(&client);
    ASSERT_UNUSED(result, result);
}

void WebSWContextManagerConnection::serviceWorkerStartedWithMessage(Optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& exceptionMessage)
{
    if (exceptionMessage.isEmpty())
        m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextStarted(jobDataIdentifier, serviceWorkerIdentifier), 0);
    else
        m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::ScriptContextFailedToStart(jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage), 0);
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

void WebSWContextManagerConnection::startFetch(SWServerConnectionIdentifier serverConnectionIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, FetchIdentifier fetchIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData, String&& referrer)
{
    auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(serviceWorkerIdentifier);
    if (!serviceWorkerThreadProxy) {
        m_connectionToNetworkProcess->send(Messages::NetworkProcess::DidNotHandleFetch { serverConnectionIdentifier, fetchIdentifier }, 0);
        return;
    }

    if (!isValidFetch(request, options, serviceWorkerThreadProxy->scriptURL(), referrer)) {
        m_connectionToNetworkProcess->send(Messages::NetworkProcess::DidNotHandleFetch { serverConnectionIdentifier, fetchIdentifier }, 0);
        return;
    }

    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToNetworkProcess.copyRef(), serviceWorkerIdentifier, serverConnectionIdentifier, fetchIdentifier);
    Optional<ServiceWorkerClientIdentifier> clientId;
    if (options.clientIdentifier)
        clientId = ServiceWorkerClientIdentifier { serverConnectionIdentifier, options.clientIdentifier.value() };

    request.setHTTPBody(formData.takeData());
    serviceWorkerThreadProxy->startFetch(serverConnectionIdentifier, fetchIdentifier, WTFMove(client), WTFMove(clientId), WTFMove(request), WTFMove(referrer), WTFMove(options));
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

void WebSWContextManagerConnection::terminateWorker(ServiceWorkerIdentifier identifier)
{
    SWContextManager::singleton().terminateWorker(identifier, asyncWorkerTerminationTimeout, nullptr);
}

void WebSWContextManagerConnection::syncTerminateWorker(ServiceWorkerIdentifier identifier, Messages::WebSWContextManagerConnection::SyncTerminateWorker::DelayedReply&& reply)
{
    SWContextManager::singleton().terminateWorker(identifier, syncWorkerTerminationTimeout, WTFMove(reply));
}

void WebSWContextManagerConnection::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, MessageWithMessagePorts&& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    // FIXME: Temporarily pipe the SW postMessage messages via the UIProcess since this is where the MessagePort registry lives
    // and this avoids races.
    WebProcess::singleton().send(Messages::WebProcessPool::PostMessageToServiceWorkerClient(destinationIdentifier, WTFMove(message), sourceIdentifier, sourceOrigin), 0);
}

void WebSWContextManagerConnection::didFinishInstall(Optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
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

void WebSWContextManagerConnection::skipWaiting(ServiceWorkerIdentifier serviceWorkerIdentifier, WTF::Function<void()>&& callback)
{
    auto callbackID = ++m_previousRequestIdentifier;
    m_skipWaitingRequests.add(callbackID, WTFMove(callback));
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SkipWaiting(serviceWorkerIdentifier, callbackID), 0);
}

void WebSWContextManagerConnection::setScriptResource(ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, const ServiceWorkerContextData::ImportedScript& script)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::SetScriptResource { serviceWorkerIdentifier, url, script.script, script.responseURL, script.mimeType }, 0);
}

void WebSWContextManagerConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::WorkerTerminated(serviceWorkerIdentifier), 0);
}

void WebSWContextManagerConnection::findClientByIdentifier(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, ServiceWorkerClientIdentifier clientIdentifier, FindClientByIdentifierCallback&& callback)
{
    auto requestIdentifier = ++m_previousRequestIdentifier;
    m_findClientByIdentifierRequests.add(requestIdentifier, WTFMove(callback));
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::FindClientByIdentifier { requestIdentifier, serviceWorkerIdentifier, clientIdentifier }, 0);
}

void WebSWContextManagerConnection::findClientByIdentifierCompleted(uint64_t requestIdentifier, Optional<ServiceWorkerClientData>&& clientData, bool hasSecurityError)
{
    if (auto callback = m_findClientByIdentifierRequests.take(requestIdentifier)) {
        if (hasSecurityError) {
            callback(Exception { SecurityError });
            return;
        }
        callback(WTFMove(clientData));
    }
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

void WebSWContextManagerConnection::claim(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& callback)
{
    auto requestIdentifier = ++m_previousRequestIdentifier;
    m_claimRequests.add(requestIdentifier, WTFMove(callback));
    m_connectionToNetworkProcess->send(Messages::WebSWServerToContextConnection::Claim { requestIdentifier, serviceWorkerIdentifier }, 0);
}

void WebSWContextManagerConnection::claimCompleted(uint64_t claimRequestIdentifier)
{
    if (auto callback = m_claimRequests.take(claimRequestIdentifier))
        callback();
}

void WebSWContextManagerConnection::didFinishSkipWaiting(uint64_t callbackID)
{
    if (auto callback = m_skipWaitingRequests.take(callbackID))
        callback();
}

NO_RETURN void WebSWContextManagerConnection::terminateProcess()
{
    RELEASE_LOG(ServiceWorker, "Service worker process is exiting because it is no longer needed");
    _exit(EXIT_SUCCESS);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

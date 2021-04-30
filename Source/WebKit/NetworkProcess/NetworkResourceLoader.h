/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#pragma once

#include "DownloadID.h"
#include "MessageSender.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkConnectionToWebProcessMessagesReplies.h"
#include "NetworkLoadClient.h"
#include "NetworkResourceLoadIdentifier.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/ContentSecurityPolicyClient.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityPolicyViolationEvent.h>
#include <WebCore/Timer.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class BlobDataFileReference;
class FormData;
class NetworkStorageSession;
class ResourceRequest;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkLoad;
class NetworkLoadChecker;
class ServiceWorkerFetchTask;
class WebSWServerConnection;

enum class NegotiatedLegacyTLS : bool;

struct ResourceLoadInfo;

namespace NetworkCache {
class Entry;
}

class NetworkResourceLoader final
    : public RefCounted<NetworkResourceLoader>
    , public NetworkLoadClient
    , public IPC::MessageSender
    , public WebCore::ContentSecurityPolicyClient
    , public WebCore::CrossOriginAccessControlCheckDisabler
    , public CanMakeWeakPtr<NetworkResourceLoader> {
public:
    static Ref<NetworkResourceLoader> create(NetworkResourceLoadParameters&& parameters, NetworkConnectionToWebProcess& connection, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoadDelayedReply&& reply = nullptr)
    {
        return adoptRef(*new NetworkResourceLoader(WTFMove(parameters), connection, WTFMove(reply)));
    }
    virtual ~NetworkResourceLoader();

    const WebCore::ResourceRequest& originalRequest() const { return m_parameters.request; }

    NetworkLoad* networkLoad() const { return m_networkLoad.get(); }

    void start();
    void abort();

    // Message handlers.
    void didReceiveNetworkResourceLoaderMessage(IPC::Connection&, IPC::Decoder&);

    void continueWillSendRequest(WebCore::ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials);

    const WebCore::ResourceResponse& response() const { return m_response; }

    NetworkConnectionToWebProcess& connectionToWebProcess() const { return m_connection; }
    PAL::SessionID sessionID() const { return m_connection->sessionID(); }
    ResourceLoadIdentifier identifier() const { return m_parameters.identifier; }
    WebCore::FrameIdentifier frameID() const { return m_parameters.webFrameID; }
    WebCore::PageIdentifier pageID() const { return m_parameters.webPageID; }
    const NetworkResourceLoadParameters& parameters() const { return m_parameters; }

    NetworkCache::GlobalFrameID globalFrameID() { return { m_parameters.webPageProxyID, pageID(), frameID() }; }

    struct SynchronousLoadData;

    // NetworkLoadClient.
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final;
    bool isSynchronous() const final;
    bool isAllowedToAskUserForCredentials() const final { return m_isAllowedToAskUserForCredentials; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(Ref<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;
    void didBlockAuthenticationChallenge() final;
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&) final;
    bool shouldCaptureExtraNetworkLoadMetrics() const final;

    // CrossOriginAccessControlCheckDisabler
    bool crossOriginAccessControlCheckEnabled() const override;
        
    void convertToDownload(DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    bool isMainResource() const { return m_parameters.request.requester() == WebCore::ResourceRequest::Requester::Main; }
    bool isMainFrameLoad() const { return isMainResource() && m_parameters.frameAncestorOrigins.isEmpty(); }
    bool isCrossOriginPrefetch() const;

    bool isAlwaysOnLoggingAllowed() const;

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    static bool shouldLogCookieInformation(NetworkConnectionToWebProcess&, const PAL::SessionID&);
    static void logCookieInformation(NetworkConnectionToWebProcess&, const String& label, const void* loggedObject, const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, const String& referrer, Optional<WebCore::FrameIdentifier>, Optional<WebCore::PageIdentifier>, Optional<uint64_t> identifier);
#endif

    void disableExtraNetworkLoadMetricsCapture() { m_shouldCaptureExtraNetworkLoadMetrics = false; }

    bool isKeptAlive() const { return m_isKeptAlive; }

    void consumeSandboxExtensionsIfNeeded();

#if ENABLE(SERVICE_WORKER)
    void startWithServiceWorker();
    void serviceWorkerDidNotHandle(ServiceWorkerFetchTask*);
#endif

    bool isAppBound();

private:
    NetworkResourceLoader(NetworkResourceLoadParameters&&, NetworkConnectionToWebProcess&, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoadDelayedReply&&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override { return m_parameters.identifier; }

    bool canUseCache(const WebCore::ResourceRequest&) const;
    bool canUseCachedRedirect(const WebCore::ResourceRequest&) const;

    void tryStoreAsCacheEntry();
    void retrieveCacheEntry(const WebCore::ResourceRequest&);
    void retrieveCacheEntryInternal(std::unique_ptr<NetworkCache::Entry>&&, WebCore::ResourceRequest&&);
    void didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry>);
    void sendResultForCacheEntry(std::unique_ptr<NetworkCache::Entry>);
    void validateCacheEntry(std::unique_ptr<NetworkCache::Entry>);
    void dispatchWillSendRequestForCacheEntry(WebCore::ResourceRequest&&, std::unique_ptr<NetworkCache::Entry>&&);

    bool shouldInterruptLoadForXFrameOptions(const String&, const URL&);
    bool shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(const WebCore::ResourceResponse&);

    enum class FirstLoad { No, Yes };
    void startNetworkLoad(WebCore::ResourceRequest&&, FirstLoad);
    void restartNetworkLoad(WebCore::ResourceRequest&&);
    void continueDidReceiveResponse();
    void didReceiveMainResourceResponse(const WebCore::ResourceResponse&);

    enum class LoadResult {
        Unknown,
        Success,
        Failure,
        Cancel
    };
    void cleanup(LoadResult);
    
    void platformDidReceiveResponse(const WebCore::ResourceResponse&);

    void startBufferingTimerIfNeeded();
    void bufferingTimerFired();
    void sendBuffer(WebCore::SharedBuffer&, size_t encodedDataLength);

    void consumeSandboxExtensions();
    void invalidateSandboxExtensions();

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    void logCookieInformation() const;
#endif

    void continueWillSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&, Optional<WebCore::PrivateClickMeasurement::AttributionTriggerData>&&);
    void didFinishWithRedirectResponse(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&);
    WebCore::ResourceResponse sanitizeResponseIfPossible(WebCore::ResourceResponse&&, WebCore::ResourceResponse::SanitizationType);

    // ContentSecurityPolicyClient
    void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long requestIdentifier = 0) final;
    void sendCSPViolationReport(URL&&, Ref<WebCore::FormData>&&) final;
    void enqueueSecurityPolicyViolationEvent(WebCore::SecurityPolicyViolationEvent::Init&&) final;

    void logSlowCacheRetrieveIfNeeded(const NetworkCache::Cache::RetrieveInfo&);

    void handlePrivateClickMeasurementConversion(WebCore::PrivateClickMeasurement::AttributionTriggerData&&, const URL&, const WebCore::ResourceRequest&);

    Optional<Seconds> validateCacheEntryForMaxAgeCapValidation(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse&);

    ResourceLoadInfo resourceLoadInfo();

    const NetworkResourceLoadParameters m_parameters;

    Ref<NetworkConnectionToWebProcess> m_connection;

    std::unique_ptr<NetworkLoad> m_networkLoad;

    WebCore::ResourceResponse m_response;

    size_t m_bufferedDataEncodedDataLength { 0 };
    RefPtr<WebCore::SharedBuffer> m_bufferedData;
    unsigned m_redirectCount { 0 };

    std::unique_ptr<SynchronousLoadData> m_synchronousLoadData;
    Vector<RefPtr<WebCore::BlobDataFileReference>> m_fileReferences;

    bool m_wasStarted { false };
    bool m_didConsumeSandboxExtensions { false };
    bool m_isAllowedToAskUserForCredentials { false };
    size_t m_numBytesReceived { 0 };

    unsigned m_retrievedDerivedDataCount { 0 };

    WebCore::Timer m_bufferingTimer;
    RefPtr<NetworkCache::Cache> m_cache;
    RefPtr<WebCore::SharedBuffer> m_bufferedDataForCache;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForValidation;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForMaxAgeCapValidation;
    bool m_isWaitingContinueWillSendRequestForCachedRedirect { false };
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryWaitingForContinueDidReceiveResponse;
    std::unique_ptr<NetworkLoadChecker> m_networkLoadChecker;
    bool m_shouldRestartLoad { false };
    ResponseCompletionHandler m_responseCompletionHandler;
    bool m_shouldCaptureExtraNetworkLoadMetrics { false };
    bool m_isKeptAlive { false };

    Optional<NetworkActivityTracker> m_networkActivityTracker;
#if ENABLE(SERVICE_WORKER)
    std::unique_ptr<ServiceWorkerFetchTask> m_serviceWorkerFetchTask;
#endif
    NetworkResourceLoadIdentifier m_resourceLoadID;
    WebCore::ResourceResponse m_redirectResponse;
};

} // namespace WebKit

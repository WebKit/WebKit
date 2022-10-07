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
#include "PrivateRelayed.h"
#include <WebCore/ContentFilterClient.h>
#include <WebCore/ContentFilterUnblockHandler.h>
#include <WebCore/ContentSecurityPolicyClient.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/ReportingClient.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SWServerRegistration.h>
#include <WebCore/SecurityPolicyViolationEvent.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/Timer.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class BlobDataFileReference;
class ContentFilter;
class FormData;
class NetworkStorageSession;
class Report;
class ResourceRequest;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkLoad;
class NetworkLoadChecker;
class ServiceWorkerFetchTask;
class WebSWServerConnection;

enum class NegotiatedLegacyTLS : bool;
enum class ViolationReportType : uint8_t;

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
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    , public WebCore::ContentFilterClient
#endif
    , public WebCore::ReportingClient
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

    void transferToNewWebProcess(NetworkConnectionToWebProcess&, const NetworkResourceLoadParameters&);

    // Message handlers.
    void didReceiveNetworkResourceLoaderMessage(IPC::Connection&, IPC::Decoder&);

    void continueWillSendRequest(WebCore::ResourceRequest&&, bool isAllowedToAskUserForCredentials);

    void setResponse(WebCore::ResourceResponse&& response) { m_response = WTFMove(response); }
    const WebCore::ResourceResponse& response() const { return m_response; }

    NetworkConnectionToWebProcess& connectionToWebProcess() const { return m_connection; }
    PAL::SessionID sessionID() const { return m_connection->sessionID(); }
    WebCore::ResourceLoaderIdentifier coreIdentifier() const { return m_parameters.identifier; }
    WebCore::FrameIdentifier frameID() const { return m_parameters.webFrameID; }
    WebCore::PageIdentifier pageID() const { return m_parameters.webPageID; }
    const NetworkResourceLoadParameters& parameters() const { return m_parameters; }
    NetworkResourceLoadIdentifier identifier() const { return m_resourceLoadID; }
    const URL& firstResponseURL() const { return m_firstResponseURL; }

    NetworkCache::GlobalFrameID globalFrameID() { return { m_parameters.webPageProxyID, pageID(), frameID() }; }

    struct SynchronousLoadData;

    // NetworkLoadClient.
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final;
    bool isSynchronous() const final;
    bool isAllowedToAskUserForCredentials() const final { return m_isAllowedToAskUserForCredentials; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&, int reportedEncodedDataLength) final;
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

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
    static bool shouldLogCookieInformation(NetworkConnectionToWebProcess&, PAL::SessionID);
    static void logCookieInformation(NetworkConnectionToWebProcess&, ASCIILiteral label, const void* loggedObject, const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, const String& referrer, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, std::optional<WebCore::ResourceLoaderIdentifier>);
#endif

    void disableExtraNetworkLoadMetricsCapture() { m_shouldCaptureExtraNetworkLoadMetrics = false; }

    bool isKeptAlive() const { return m_isKeptAlive; }

    void consumeSandboxExtensionsIfNeeded();

#if ENABLE(SERVICE_WORKER)
    void startWithServiceWorker();
    void serviceWorkerDidNotHandle(ServiceWorkerFetchTask*);
    void setResultingClientIdentifier(String&& identifier) { m_resultingClientIdentifier = WTFMove(identifier); }
    const String& resultingClientIdentifier() const { return m_resultingClientIdentifier; }
    void setServiceWorkerRegistration(WebCore::SWServerRegistration& serviceWorkerRegistration) { m_serviceWorkerRegistration = serviceWorkerRegistration; }
    void setWorkerStart(MonotonicTime);
    MonotonicTime workerStart() const { return m_workerStart; }
#endif

    std::optional<WebCore::ResourceError> doCrossOriginOpenerHandlingOfResponse(const WebCore::ResourceResponse&);
    void sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup(const WebCore::ResourceResponse&, PrivateRelayed, bool needsContinueDidReceiveResponseMessage);

    bool isAppInitiated();

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    void ref() const final { RefCounted<NetworkResourceLoader>::ref(); }
    void deref() const final { RefCounted<NetworkResourceLoader>::deref(); }
#endif

    void willSendServiceWorkerRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&);

private:
    NetworkResourceLoader(NetworkResourceLoadParameters&&, NetworkConnectionToWebProcess&, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoadDelayedReply&&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override { return m_parameters.identifier.toUInt64(); }

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    // ContentFilterClient
    void dataReceivedThroughContentFilter(const WebCore::SharedBuffer&, size_t) final;
    WebCore::ResourceError contentFilterDidBlock(WebCore::ContentFilterUnblockHandler, String&& unblockRequestDeniedScript) final;
    void cancelMainResourceLoadForContentFilter(const WebCore::ResourceError&) final;
    void handleProvisionalLoadFailureFromContentFilter(const URL& blockedPageURL, WebCore::SubstituteData&) final;
#endif

    void processClearSiteDataHeader(const WebCore::ResourceResponse&, CompletionHandler<void()>&&);

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
    bool shouldInterruptNavigationForCrossOriginEmbedderPolicy(const WebCore::ResourceResponse&);
    bool shouldInterruptWorkerLoadForCrossOriginEmbedderPolicy(const WebCore::ResourceResponse&);

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
    void sendBuffer(const WebCore::FragmentedSharedBuffer&, size_t encodedDataLength);

    void consumeSandboxExtensions();
    void invalidateSandboxExtensions();

#if ENABLE(TRACKING_PREVENTION) && !RELEASE_LOG_DISABLED
    void logCookieInformation() const;
#endif

    void continueWillSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&, std::optional<WebCore::PCM::AttributionTriggerData>&&);
    void didFinishWithRedirectResponse(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&);
    WebCore::ResourceResponse sanitizeResponseIfPossible(WebCore::ResourceResponse&&, WebCore::ResourceResponse::SanitizationType);

    // ContentSecurityPolicyClient
    void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long requestIdentifier = 0) final;
    void enqueueSecurityPolicyViolationEvent(WebCore::SecurityPolicyViolationEventInit&&) final;

    void logSlowCacheRetrieveIfNeeded(const NetworkCache::Cache::RetrieveInfo&);

    std::optional<Seconds> validateCacheEntryForMaxAgeCapValidation(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse&);

    ResourceLoadInfo resourceLoadInfo();

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    bool startContentFiltering(WebCore::ResourceRequest&);
#endif

    // ReportingClient
    void notifyReportObservers(Ref<WebCore::Report>&&) final;
    String endpointURIForToken(const String&) const final;
    void sendReportToEndpoints(const URL& baseURL, const Vector<String>& endpointURIs, const Vector<String>& endpointTokens, Ref<WebCore::FormData>&& report, WebCore::ViolationReportType) final;
    String httpUserAgent() const final { return originalRequest().httpUserAgent(); }
    void initializeReportingEndpoints(const WebCore::ResourceResponse&);
    WebCore::FrameIdentifier frameIdentifierForReport() const;

    enum class IsFromServiceWorker : bool { No, Yes };
    void willSendRedirectedRequestInternal(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&, IsFromServiceWorker);
    std::optional<WebCore::NetworkLoadMetrics> computeResponseMetrics(const WebCore::ResourceResponse&) const;

    NetworkResourceLoadParameters m_parameters;

    Ref<NetworkConnectionToWebProcess> m_connection;

    std::unique_ptr<NetworkLoad> m_networkLoad;

    WebCore::ResourceResponse m_response;

    size_t m_bufferedDataEncodedDataLength { 0 };
    WebCore::SharedBufferBuilder m_bufferedData;
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
    WebCore::SharedBufferBuilder m_bufferedDataForCache;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForValidation;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForMaxAgeCapValidation;
    bool m_isWaitingContinueWillSendRequestForCachedRedirect { false };
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryWaitingForContinueDidReceiveResponse;
    std::unique_ptr<NetworkLoadChecker> m_networkLoadChecker;
    bool m_shouldRestartLoad { false };
    ResponseCompletionHandler m_responseCompletionHandler;
    bool m_shouldCaptureExtraNetworkLoadMetrics { false };
    bool m_isKeptAlive { false };

    std::optional<NetworkActivityTracker> m_networkActivityTracker;
#if ENABLE(SERVICE_WORKER)
    std::unique_ptr<ServiceWorkerFetchTask> m_serviceWorkerFetchTask;
    String m_resultingClientIdentifier;
    WeakPtr<WebCore::SWServerRegistration> m_serviceWorkerRegistration;
    MonotonicTime m_workerStart;
#endif
    NetworkResourceLoadIdentifier m_resourceLoadID;
    WebCore::ResourceResponse m_redirectResponse;
    URL m_firstResponseURL; // First URL in response's URL list (https://fetch.spec.whatwg.org/#concept-response-url-list).
    std::optional<WebCore::CrossOriginOpenerPolicyEnforcementResult> m_currentCoopEnforcementResult;

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    std::unique_ptr<WebCore::ContentFilter> m_contentFilter;
    WebCore::ContentFilterUnblockHandler m_unblockHandler;
    String m_unblockRequestDeniedScript;
#endif

    PrivateRelayed m_privateRelayed { PrivateRelayed::No };
    MemoryCompactRobinHoodHashMap<String, String> m_reportingEndpoints;
};

} // namespace WebKit

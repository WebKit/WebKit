/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "AppPrivacyReport.h"
#include "DataTaskIdentifier.h"
#include "NavigatingToAppBoundDomain.h"
#include "NetworkNotificationManager.h"
#include "NetworkResourceLoadIdentifier.h"
#include "PrefetchCache.h"
#include "PrivateClickMeasurementManagerInterface.h"
#include "SandboxExtension.h"
#include "ServiceWorkerSoftUpdateLoader.h"
#include "WebPageProxyIdentifier.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/DNS.h>
#include <WebCore/FetchIdentifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SWServerDelegate.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <pal/SessionID.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/LazyUniqueRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class CertificateInfo;
class NetworkStorageSession;
class ResourceMonitorThrottlerHolder;
class ResourceRequest;
class ResourceError;
class SWServer;
class SecurityOriginData;
enum class AdvancedPrivacyProtections : uint16_t;
enum class IncludeHttpOnlyCookies : bool;
enum class ShouldSample : bool;
enum class IsInitiatedByDedicatedWorker : bool;
struct ClientOrigin;
}

namespace WTF {
enum class Critical : bool;
}

namespace WebKit {
class BackgroundFetchStoreImpl;
class NetworkBroadcastChannelRegistry;
class NetworkDataTask;
class NetworkLoadScheduler;
class NetworkProcess;
class NetworkConnectionToWebProcess;
class NetworkResourceLoader;
struct NetworkResourceLoadParameters;
class NetworkSocketChannel;
class NetworkStorageManager;
class ServiceWorkerFetchTask;
class WebPageNetworkParameters;
class WebResourceLoadStatisticsStore;
class WebSharedWorkerServer;
class WebSocketTask;
class WebSWOriginStore;
class WebSWServerConnection;
struct BackgroundFetchState;
struct NetworkSessionCreationParameters;
struct SessionSet;

enum class WebsiteDataType : uint32_t;

namespace CacheStorage {
class Engine;
}

namespace NetworkCache {
class Cache;
}

class NetworkSession : public WebCore::SWServerDelegate, public CanMakeCheckedPtr<NetworkSession> {
    WTF_MAKE_TZONE_ALLOCATED(NetworkSession);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(NetworkSession);
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, const NetworkSessionCreationParameters&);
    virtual ~NetworkSession();

    virtual void invalidateAndCancel();
    bool isInvalidated() const { return m_isInvalidated; }
    virtual bool shouldLogCookieInformation() const { return false; }
    virtual Vector<WebCore::SecurityOriginData> hostNamesWithAlternativeServices() const { return { }; }
    virtual void deleteAlternativeServicesForHostNames(const Vector<String>&) { }
    virtual void clearAlternativeServices(WallTime) { }
    virtual HashSet<WebCore::SecurityOriginData> originsWithCredentials() { return { }; }
    virtual void removeCredentialsForOrigins(const Vector<WebCore::SecurityOriginData>&) { }
    virtual void clearCredentials(WallTime) { }
    virtual void loadImageForDecoding(WebCore::ResourceRequest&&, WebPageProxyIdentifier, size_t, CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)>&&) { ASSERT_NOT_REACHED(); }

    // CanMakeCheckedPtr.
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    PAL::SessionID sessionID() const { return m_sessionID; }
    NetworkProcess& networkProcess() { return m_networkProcess; }
    WebCore::NetworkStorageSession* NODELETE networkStorageSession() const;

    void registerNetworkDataTask(NetworkDataTask&);
    void unregisterNetworkDataTask(NetworkDataTask&);

    void destroyPrivateClickMeasurementStore(CompletionHandler<void()>&&);

    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void setTrackingPreventionEnabled(bool);
    bool NODELETE isTrackingPreventionEnabled() const;
    static WebCore::IsKnownCrossSiteTracker isRequestToKnownCrossSiteTracker(const WebCore::ResourceRequest&);
    static WebCore::IsKnownCrossSiteTracker isResourceFromKnownCrossSiteTracker(const URL& firstParty, const URL& resource);
    static bool isRequestBlockable(const WebCore::ResourceRequest&);
    void deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    bool enableResourceLoadStatisticsLogTestingEvent() const { return m_enableResourceLoadStatisticsLogTestingEvent; }
    void setResourceLoadStatisticsLogTestingEvent(bool log) { m_enableResourceLoadStatisticsLogTestingEvent = log; }
    virtual bool hasIsolatedSession(const WebCore::RegistrableDomain&) const { return false; }
    virtual void clearIsolatedSessions() { }
    void NODELETE setShouldDowngradeReferrerForTesting(bool);
    bool NODELETE shouldDowngradeReferrer() const;
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode);
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const { return m_thirdPartyCookieBlockingMode; }
    void setShouldEnbleSameSiteStrictEnforcement(WebCore::SameSiteStrictEnforcementEnabled);
    void setFirstPartyHostCNAMEDomain(String&& firstPartyHost, WebCore::RegistrableDomain&& cnameDomain);
    std::optional<WebCore::RegistrableDomain> firstPartyHostCNAMEDomain(const String& firstPartyHost);
    void setFirstPartyHostIPAddress(const String& firstPartyHost, const String& addressString);
    std::optional<WebCore::IPAddress> firstPartyHostIPAddress(const String& firstPartyHost);
    void setThirdPartyCNAMEDomainForTesting(WebCore::RegistrableDomain&& domain) { m_thirdPartyCNAMEDomainForTesting = WTF::move(domain); };
    std::optional<WebCore::RegistrableDomain> thirdPartyCNAMEDomainForTesting() const { return m_thirdPartyCNAMEDomainForTesting; }
    void resetFirstPartyDNSData();
    void destroyResourceLoadStatistics(CompletionHandler<void()>&&);
    
#if ENABLE(APP_BOUND_DOMAINS)
    virtual bool hasAppBoundSession() const { return false; }
    virtual void clearAppBoundSession() { }
#endif

    void storePrivateClickMeasurement(WebCore::PrivateClickMeasurement&&);
    virtual void donateToSKAdNetwork(WebCore::PrivateClickMeasurement&&) { }
    virtual void notifyAdAttributionKitOfSessionTermination() { }
    void handlePrivateClickMeasurementConversion(WebCore::PCM::AttributionTriggerData&&, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest, String&& attributedBundleIdentifier);
    void simulatePrivateClickMeasurementConversion(int priority, int triggerData, const URL& sourceURL, const URL& destinationURL);
    void dumpPrivateClickMeasurement(CompletionHandler<void(String)>&&);
    void clearPrivateClickMeasurement(CompletionHandler<void()>&&);
    void clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementOverrideTimerForTesting(bool value);
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenPublicKeyURLForTesting(URL&&);
    void setPrivateClickMeasurementTokenSignatureURLForTesting(URL&&);
    void setPrivateClickMeasurementAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL);
    void markPrivateClickMeasurementsAsExpiredForTesting();
    void NODELETE setPrivateClickMeasurementEphemeralMeasurementForTesting(bool);
    void setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID);
    void firePrivateClickMeasurementTimerImmediatelyForTesting();
    void allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo&);
    void setPrivateClickMeasurementAppBundleIDForTesting(String&&);

    void addKeptAliveLoad(Ref<NetworkResourceLoader>&&);
    void removeKeptAliveLoad(NetworkResourceLoader&);

    void addLoaderAwaitingWebProcessTransfer(Ref<NetworkResourceLoader>&&);
    void setParkedLoaderDestinationAndResolvePendingClaims(NetworkResourceLoadIdentifier, WebCore::ProcessIdentifier destinationWebProcess);
    void removeLoaderWaitingWebProcessTransfer(NetworkResourceLoadIdentifier);

    enum class LoaderAwaitingWebProcessTransferOutcome : uint8_t {
        Success, // loader returned in claim.loader
        NotFound, // no parked loader for this identifier (legitimate fallthrough to fresh load)
        Pending, // parked loader exists, destination not yet known from UIProcess (caller should queue)
        WrongCaller, // parked loader exists, destination known, caller is not it (call site MESSAGE_CHECKs)
    };
    struct LoaderAwaitingWebProcessTransferClaim {
        RefPtr<NetworkResourceLoader> loader;
        LoaderAwaitingWebProcessTransferOutcome outcome { LoaderAwaitingWebProcessTransferOutcome::NotFound };
    };
    LoaderAwaitingWebProcessTransferClaim takeLoaderAwaitingWebProcessTransfer(NetworkResourceLoadIdentifier, WebCore::ProcessIdentifier callerWebProcess);

    // Unchecked take for the trusted in-NetworkProcess Enhanced Security return-to-sender path, where a
    // declined process swap resumes the load in the original process. There is no untrusted caller to
    // validate here (and the declined loader never had a destination recorded), so this bypasses the
    // ownership check used by the ScheduleResourceLoad IPC path above.
    RefPtr<NetworkResourceLoader> takeParkedLoaderForOriginalProcess(NetworkResourceLoadIdentifier);

    // Returns false if the per-identifier pending-claim queue is full (caller should MESSAGE_CHECK kill).
    bool queuePendingLoaderClaim(NetworkResourceLoadIdentifier, WeakPtr<NetworkConnectionToWebProcess>, NetworkResourceLoadParameters&&);

#if ENABLE(IPC_TESTING_API)
    // Insert a synthetic parked entry without a real NetworkResourceLoader. Returns false if an entry
    // for `identifier` is already parked. Used by tests to deterministically drive the bind-to-claimant
    // ownership check in takeLoaderAwaitingWebProcessTransfer.
    bool addSyntheticLoaderAwaitingWebProcessTransferForTesting(NetworkResourceLoadIdentifier, std::optional<WebCore::ProcessIdentifier> destination);
    void removeSyntheticLoaderAwaitingWebProcessTransferForTesting(NetworkResourceLoadIdentifier);
#endif

    NetworkCache::Cache* cache() { return m_cache.get(); }

    PrefetchCache& NODELETE prefetchCache();
    void clearPrefetchCache() { m_prefetchCache->clear(); }

    virtual RefPtr<WebSocketTask> createWebSocketTask(WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, WebCore::StoredCredentialsPolicy, WebCore::IsInitiatedByDedicatedWorker);
    virtual void removeWebSocketTask(SessionSet&, WebSocketTask&) { }
    virtual void addWebSocketTask(WebPageProxyIdentifier, WebSocketTask&) { }

    WebCore::BlobRegistryImpl& blobRegistry() LIFETIME_BOUND { return m_blobRegistry; }
    NetworkBroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry; }

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }
    bool allowsServerPreconnect() const { return m_allowsServerPreconnect; }
    bool shouldRunServiceWorkersOnMainThreadForTesting() const { return m_shouldRunServiceWorkersOnMainThreadForTesting; }
    std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue() const { return m_overrideServiceWorkerRegistrationCountTestingValue; }
    bool isStaleWhileRevalidateEnabled() const { return m_isStaleWhileRevalidateEnabled; }

    void lowMemoryHandler(WTF::Critical);

    void removeSoftUpdateLoader(ServiceWorkerSoftUpdateLoader* loader) { m_softUpdateLoaders.remove(loader); }
    void addNavigationPreloaderTask(ServiceWorkerFetchTask&);
    ServiceWorkerFetchTask* NODELETE navigationPreloaderTaskFromFetchIdentifier(WebCore::FetchIdentifier);
    void removeNavigationPreloaderTask(ServiceWorkerFetchTask&);

    WebCore::SWServer* swServer() { return m_swServer.get(); }
    WebCore::SWServer& ensureSWServer();
    void registerSWServerConnection(WebSWServerConnection&);
    void unregisterSWServerConnection(WebSWServerConnection&);

    bool NODELETE hasServiceWorkerDatabasePath() const;

    void getAllBackgroundFetchIdentifiers(CompletionHandler<void(Vector<String>&&)>&&);
    void getBackgroundFetchState(const String&, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&&);
    void abortBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void pauseBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void resumeBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void clickBackgroundFetch(const String&, CompletionHandler<void()>&&);

    WebSharedWorkerServer* sharedWorkerServer() { return const_cast<WebSharedWorkerServer*>(m_sharedWorkerServer.getIfExists()); }
    WebSharedWorkerServer& ensureSharedWorkerServer() { return const_cast<WebSharedWorkerServer&>(m_sharedWorkerServer.get(*this)); }

    NetworkStorageManager& storageManager() { return m_storageManager.get(); }
    void clearCacheEngine();

    NetworkLoadScheduler& networkLoadScheduler();

    PCM::ManagerInterface& privateClickMeasurement() { return m_privateClickMeasurement.get(); }
    void setPrivateClickMeasurementDebugMode(bool);
    bool privateClickMeasurementDebugModeEnabled() const { return m_privateClickMeasurementDebugModeEnabled; }

    void NODELETE setShouldSendPrivateTokenIPCForTesting(bool);
    bool shouldSendPrivateTokenIPCForTesting() const { return m_shouldSendPrivateTokenIPCForTesting; }
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    void setOptInCookiePartitioningEnabled(bool);
#endif

#if PLATFORM(COCOA)
    AppPrivacyReportTestingData& appPrivacyReportTestingData() LIFETIME_BOUND { return m_appPrivacyReportTestingData; }
#endif

    virtual void removeNetworkWebsiteData(std::optional<WallTime>, std::optional<HashSet<WebCore::RegistrableDomain>>&&, CompletionHandler<void()>&& completionHandler) { completionHandler(); }

    virtual void dataTaskWithRequest(WebPageProxyIdentifier, WebCore::ResourceRequest&&, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(DataTaskIdentifier)>&&) { }
    virtual void cancelDataTask(DataTaskIdentifier) { }
    virtual void addWebPageNetworkParameters(WebPageProxyIdentifier, WebPageNetworkParameters&&) { }
    virtual void removeWebPageNetworkParameters(WebPageProxyIdentifier) { }
    virtual size_t countNonDefaultSessionSets() const { return 0; }

    String NODELETE attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier) const;

#if ENABLE(NETWORK_ISSUE_REPORTING)
    void reportNetworkIssue(WebPageProxyIdentifier, const URL&);
#endif

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    NetworkNotificationManager& notificationManager() { return m_notificationManager.get(); }
#endif

    const Vector<WebCore::SecurityOriginData>& mockPushSubscriptionOriginsForTesting() const { return m_mockPushSubscriptionOriginsForTesting; }

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    std::optional<int64_t> bytesPerSecondLimit() const { return m_bytesPerSecondLimit; }
    void setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit);
#endif

#if HAVE(NW_PROXY_CONFIG)
    virtual void clearProxyConfigData() { }
    virtual void setProxyConfigData(const Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>&) { };
#endif

    virtual bool canPrefetchDNS() const { return true; }

    void setInspectionForServiceWorkersAllowed(bool);
    void setPersistedDomains(HashSet<WebCore::RegistrableDomain>&&);

    void recordHTTPSConnectionTiming(const WebCore::NetworkLoadMetrics&);
    double currentHTTPSConnectionAverageTiming() const { return m_recentHTTPSConnectionTiming.currentMovingAverage; }

    virtual bool isNetworkSessionCocoa() const { return false; }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    bool isDeclarativeWebPushEnabled() const { return m_isDeclarativeWebPushEnabled; }
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ResourceMonitorThrottlerHolder& resourceMonitorThrottler();

    void clearResourceMonitorThrottlerData(CompletionHandler<void()>&&);
#endif

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    String webContentRestrictionsConfigurationFile() const { return m_webContentRestrictionsConfigurationFile; }
#endif

    std::optional<WTF::UUID> dataStoreIdentifier() const { return m_dataStoreIdentifier; }

protected:
    NetworkSession(NetworkProcess&, const NetworkSessionCreationParameters&);

    void forwardResourceLoadStatisticsSettings();
    WebSWOriginStore* NODELETE swOriginStore() const LIFETIME_BOUND;

    // SWServerDelegate
    void softUpdate(WebCore::ServiceWorkerJobData&&, bool shouldRefreshCache, WebCore::ResourceRequest&&, CompletionHandler<void(WebCore::WorkerFetchResult&&)>&&) final;
    void createContextConnection(const WebCore::Site&, std::optional<WebCore::ProcessIdentifier>, std::optional<WebCore::ScriptExecutionContextIdentifier>, WebCore::CrossOriginEmbedderPolicyValue, CompletionHandler<void()>&&) final;
    void appBoundDomains(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&) final;
    void addAllowedFirstPartyForCookies(WebCore::ProcessIdentifier, std::optional<WebCore::ProcessIdentifier>, WebCore::RegistrableDomain&&) final;
    RefPtr<WebCore::SWRegistrationStore> createRegistrationStore(WebCore::SWServer&) final;
    void requestBackgroundFetchPermission(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&) final;
    RefPtr<WebCore::BackgroundFetchRecordLoader> createBackgroundFetchRecordLoader(WebCore::BackgroundFetchRecordLoaderClient&, const WebCore::BackgroundFetchRequest&, size_t responseDataSize, const WebCore::ClientOrigin&) final;
    Ref<WebCore::BackgroundFetchStore> createBackgroundFetchStore() final;

    BackgroundFetchStoreImpl& ensureBackgroundFetchStore();

    PAL::SessionID m_sessionID;
    const Ref<NetworkProcess> m_networkProcess;
    ThreadSafeWeakHashSet<NetworkDataTask> m_dataTaskSet;
    String m_resourceLoadStatisticsDirectory;
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;
    ShouldIncludeLocalhost m_shouldIncludeLocalhostInResourceLoadStatistics { ShouldIncludeLocalhost::Yes };
    EnableResourceLoadStatisticsDebugMode m_enableResourceLoadStatisticsDebugMode { EnableResourceLoadStatisticsDebugMode::No };
    WebCore::RegistrableDomain m_resourceLoadStatisticsManualPrevalentResource;
    bool m_enableResourceLoadStatisticsLogTestingEvent;
    bool m_downgradeReferrer { true };
    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    WebCore::SameSiteStrictEnforcementEnabled m_sameSiteStrictEnforcementEnabled { WebCore::SameSiteStrictEnforcementEnabled::No };
    WebCore::FirstPartyWebsiteDataRemovalMode m_firstPartyWebsiteDataRemovalMode { WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies };
    WebCore::RegistrableDomain m_standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> m_persistedDomains;
    HashMap<String, WebCore::RegistrableDomain> m_firstPartyHostCNAMEDomains;
    HashMap<String, WebCore::IPAddress> m_firstPartyHostIPAddresses;
    std::optional<WebCore::RegistrableDomain> m_thirdPartyCNAMEDomainForTesting;
    bool m_isStaleWhileRevalidateEnabled { false };
    const Ref<PCM::ManagerInterface> m_privateClickMeasurement;
    bool m_privateClickMeasurementDebugModeEnabled { false };
    std::optional<WebCore::PrivateClickMeasurement> m_ephemeralMeasurement;
    bool m_isRunningEphemeralMeasurementTest { false };

    HashSet<Ref<NetworkResourceLoader>> m_keptAliveLoads;

    class CachedNetworkResourceLoader : public RefCountedAndCanMakeWeakPtr<CachedNetworkResourceLoader> {
        WTF_MAKE_TZONE_ALLOCATED(CachedNetworkResourceLoader);
    public:
        static Ref<CachedNetworkResourceLoader> create(Ref<NetworkResourceLoader>&&);
#if ENABLE(IPC_TESTING_API)
        static Ref<CachedNetworkResourceLoader> createForTesting();
#endif
        ~CachedNetworkResourceLoader();
        RefPtr<NetworkResourceLoader> takeLoader();

        std::optional<WebCore::ProcessIdentifier> destinationWebProcess() const { return m_destinationWebProcess; }
        void setDestinationWebProcess(WebCore::ProcessIdentifier destination) { m_destinationWebProcess = destination; }

        struct PendingClaim;
        // Cap the number of pending claims to prevent the WebContent process from
        // allocating many claims in the NetworkProcess. This limit on claims covers
        // claims from all processes.
        static constexpr size_t maxPendingClaims = 4;
        bool addPendingClaim(WeakPtr<NetworkConnectionToWebProcess>, NetworkResourceLoadParameters&&);
        Vector<std::unique_ptr<PendingClaim>> takePendingClaims();

    private:
        explicit CachedNetworkResourceLoader(Ref<NetworkResourceLoader>&&);
#if ENABLE(IPC_TESTING_API)
        CachedNetworkResourceLoader();
#endif
        void expirationTimerFired();

        WebCore::Timer m_expirationTimer;
        RefPtr<NetworkResourceLoader> m_loader;
        std::optional<WebCore::ProcessIdentifier> m_destinationWebProcess;
        Vector<std::unique_ptr<PendingClaim>> m_pendingClaims;
    };
    HashMap<NetworkResourceLoadIdentifier, Ref<CachedNetworkResourceLoader>> m_loadersAwaitingWebProcessTransfer;

    const UniqueRef<PrefetchCache> m_prefetchCache;

    bool m_isInvalidated { false };
    RefPtr<NetworkCache::Cache> m_cache;
    const RefPtr<NetworkLoadScheduler> m_networkLoadScheduler;
    WebCore::BlobRegistryImpl m_blobRegistry;
    const Ref<NetworkBroadcastChannelRegistry> m_broadcastChannelRegistry;
    unsigned m_testSpeedMultiplier { 1 };
    bool m_allowsServerPreconnect { true };
    bool m_shouldRunServiceWorkersOnMainThreadForTesting { false };
    bool m_shouldSendPrivateTokenIPCForTesting { false };
    std::optional<unsigned> m_overrideServiceWorkerRegistrationCountTestingValue;
    HashSet<Ref<ServiceWorkerSoftUpdateLoader>> m_softUpdateLoaders;
    HashMap<WebCore::FetchIdentifier, WeakRef<ServiceWorkerFetchTask>> m_navigationPreloaders;

    struct ServiceWorkerInfo {
        String databasePath;
        bool processTerminationDelayEnabled { true };
    };
    std::optional<ServiceWorkerInfo> m_serviceWorkerInfo;
    RefPtr<WebCore::SWServer> m_swServer;
    const RefPtr<BackgroundFetchStoreImpl> m_backgroundFetchStore;
    bool m_inspectionForServiceWorkersAllowed { true };
    const LazyUniqueRef<NetworkSession, WebSharedWorkerServer> m_sharedWorkerServer;

    struct RecentHTTPSConnectionTiming {
        static constexpr unsigned maxEntries { 25 };
        Deque<Seconds, maxEntries> recentConnectionTimings;
        double currentMovingAverage { 0 };
    } m_recentHTTPSConnectionTiming;

    const Ref<NetworkStorageManager> m_storageManager;
    String m_cacheStorageDirectory;

#if PLATFORM(COCOA)
    AppPrivacyReportTestingData m_appPrivacyReportTestingData;
#endif

    HashMap<WebPageProxyIdentifier, String> m_attributedBundleIdentifierFromPageIdentifiers;

    Vector<WebCore::SecurityOriginData> m_mockPushSubscriptionOriginsForTesting;
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    const Ref<NetworkNotificationManager> m_notificationManager;
#endif
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    std::optional<int64_t> m_bytesPerSecondLimit;
#endif
#if ENABLE(DECLARATIVE_WEB_PUSH)
    bool m_isDeclarativeWebPushEnabled { false };
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    const RefPtr<WebCore::ResourceMonitorThrottlerHolder> m_resourceMonitorThrottler;
    String m_resourceMonitorThrottlerDirectory;
#endif
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    String m_webContentRestrictionsConfigurationFile;
#endif
    Markable<WTF::UUID> m_dataStoreIdentifier;
};

} // namespace WebKit

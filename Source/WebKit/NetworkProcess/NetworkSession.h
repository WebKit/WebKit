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
#include "DataReference.h"
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
#include <pal/SessionID.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/UUID.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class CertificateInfo;
class NetworkStorageSession;
class ResourceRequest;
class ResourceError;
class SWServer;
enum class IncludeHttpOnlyCookies : bool;
enum class NetworkConnectionIntegrity : uint8_t;
enum class ShouldSample : bool;
struct ClientOrigin;
struct SecurityOriginData;
}

namespace WTF {
enum class Critical : bool;
}

namespace WebKit {

class NetworkBroadcastChannelRegistry;
class NetworkDataTask;
class NetworkLoadScheduler;
class NetworkProcess;
class NetworkResourceLoader;
class NetworkSocketChannel;
class NetworkStorageManager;
class ServiceWorkerFetchTask;
class WebPageNetworkParameters;
class WebResourceLoadStatisticsStore;
class WebSharedWorkerServer;
class WebSocketTask;
class WebSWOriginStore;
class WebSWServerConnection;
struct NetworkSessionCreationParameters;
struct SessionSet;

enum class WebsiteDataType : uint32_t;

namespace CacheStorage {
class Engine;
}

namespace NetworkCache {
class Cache;
}

class NetworkSession : public CanMakeWeakPtr<NetworkSession> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, const NetworkSessionCreationParameters&);
    virtual ~NetworkSession();

    virtual void invalidateAndCancel();
    virtual void clearCredentials() { };
    virtual bool shouldLogCookieInformation() const { return false; }
    virtual Vector<WebCore::SecurityOriginData> hostNamesWithAlternativeServices() const { return { }; }
    virtual void deleteAlternativeServicesForHostNames(const Vector<String>&) { }
    virtual void clearAlternativeServices(WallTime) { }

    PAL::SessionID sessionID() const { return m_sessionID; }
    std::optional<UUID> dataStoreIdentifier() const { return m_dataStoreIdentifier; }
    NetworkProcess& networkProcess() { return m_networkProcess; }
    WebCore::NetworkStorageSession* networkStorageSession() const;

    void registerNetworkDataTask(NetworkDataTask&);

    void destroyPrivateClickMeasurementStore(CompletionHandler<void()>&&);

#if ENABLE(TRACKING_PREVENTION)
    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void setTrackingPreventionEnabled(bool);
    bool isTrackingPreventionEnabled() const;
    void notifyResourceLoadStatisticsProcessed();
    void deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, bool shouldNotifyPage, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, WebCore::ShouldSample);
    bool enableResourceLoadStatisticsLogTestingEvent() const { return m_enableResourceLoadStatisticsLogTestingEvent; }
    void setResourceLoadStatisticsLogTestingEvent(bool log) { m_enableResourceLoadStatisticsLogTestingEvent = log; }
    virtual bool hasIsolatedSession(const WebCore::RegistrableDomain&) const { return false; }
    virtual void clearIsolatedSessions() { }
    void setShouldDowngradeReferrerForTesting(bool);
    bool shouldDowngradeReferrer() const;
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode);
    void setShouldEnbleSameSiteStrictEnforcement(WebCore::SameSiteStrictEnforcementEnabled);
    void setFirstPartyHostCNAMEDomain(String&& firstPartyHost, WebCore::RegistrableDomain&& cnameDomain);
    std::optional<WebCore::RegistrableDomain> firstPartyHostCNAMEDomain(const String& firstPartyHost);
    void setFirstPartyHostIPAddress(const String& firstPartyHost, const String& addressString);
    std::optional<WebCore::IPAddress> firstPartyHostIPAddress(const String& firstPartyHost);
    void setThirdPartyCNAMEDomainForTesting(WebCore::RegistrableDomain&& domain) { m_thirdPartyCNAMEDomainForTesting = WTFMove(domain); };
    std::optional<WebCore::RegistrableDomain> thirdPartyCNAMEDomainForTesting() const { return m_thirdPartyCNAMEDomainForTesting; }
    void resetFirstPartyDNSData();
    void destroyResourceLoadStatistics(CompletionHandler<void()>&&);
#endif
    
#if ENABLE(APP_BOUND_DOMAINS)
    virtual bool hasAppBoundSession() const { return false; }
    virtual void clearAppBoundSession() { }
#endif

    void storePrivateClickMeasurement(WebCore::PrivateClickMeasurement&&);
    virtual void donateToSKAdNetwork(WebCore::PrivateClickMeasurement&&) { }
    void handlePrivateClickMeasurementConversion(WebCore::PCM::AttributionTriggerData&&, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest, String&& attributedBundleIdentifier);
    void dumpPrivateClickMeasurement(CompletionHandler<void(String)>&&);
    void clearPrivateClickMeasurement(CompletionHandler<void()>&&);
    void clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementOverrideTimerForTesting(bool value);
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenPublicKeyURLForTesting(URL&&);
    void setPrivateClickMeasurementTokenSignatureURLForTesting(URL&&);
    void setPrivateClickMeasurementAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL);
    void markPrivateClickMeasurementsAsExpiredForTesting();
    void setPrivateClickMeasurementEphemeralMeasurementForTesting(bool);
    void setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID);
    void firePrivateClickMeasurementTimerImmediatelyForTesting();
    void allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo&);
    void setPrivateClickMeasurementAppBundleIDForTesting(String&&);

    void addKeptAliveLoad(Ref<NetworkResourceLoader>&&);
    void removeKeptAliveLoad(NetworkResourceLoader&);

    void addLoaderAwaitingWebProcessTransfer(Ref<NetworkResourceLoader>&&);
    void removeLoaderWaitingWebProcessTransfer(NetworkResourceLoadIdentifier);
    RefPtr<NetworkResourceLoader> takeLoaderAwaitingWebProcessTransfer(NetworkResourceLoadIdentifier);

    NetworkCache::Cache* cache() { return m_cache.get(); }

    PrefetchCache& prefetchCache() { return m_prefetchCache; }
    void clearPrefetchCache() { m_prefetchCache.clear(); }

    virtual std::unique_ptr<WebSocketTask> createWebSocketTask(WebPageProxyIdentifier, NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::NetworkConnectionIntegrity> networkConnectionIntegrityPolicy);
    virtual void removeWebSocketTask(SessionSet&, WebSocketTask&) { }
    virtual void addWebSocketTask(WebPageProxyIdentifier, WebSocketTask&) { }

    WebCore::BlobRegistryImpl& blobRegistry() { return m_blobRegistry; }
    NetworkBroadcastChannelRegistry& broadcastChannelRegistry() { return m_broadcastChannelRegistry; }

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }
    bool allowsServerPreconnect() const { return m_allowsServerPreconnect; }
    bool shouldRunServiceWorkersOnMainThreadForTesting() const { return m_shouldRunServiceWorkersOnMainThreadForTesting; }
    std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue() const { return m_overrideServiceWorkerRegistrationCountTestingValue; }
    bool isStaleWhileRevalidateEnabled() const { return m_isStaleWhileRevalidateEnabled; }

    void lowMemoryHandler(WTF::Critical);

#if ENABLE(SERVICE_WORKER)
    void addSoftUpdateLoader(std::unique_ptr<ServiceWorkerSoftUpdateLoader>&& loader) { m_softUpdateLoaders.add(WTFMove(loader)); }
    void removeSoftUpdateLoader(ServiceWorkerSoftUpdateLoader* loader) { m_softUpdateLoaders.remove(loader); }
    void addNavigationPreloaderTask(ServiceWorkerFetchTask&);
    ServiceWorkerFetchTask* navigationPreloaderTaskFromFetchIdentifier(WebCore::FetchIdentifier);
    void removeNavigationPreloaderTask(ServiceWorkerFetchTask&);

    WebCore::SWServer* swServer() { return m_swServer.get(); }
    WebCore::SWServer& ensureSWServer();
    WebSWOriginStore* swOriginStore() const; // FIXME: Can be private?
    void registerSWServerConnection(WebSWServerConnection&);
    void unregisterSWServerConnection(WebSWServerConnection&);

    bool hasServiceWorkerDatabasePath() const;
#endif

    WebSharedWorkerServer* sharedWorkerServer() { return m_sharedWorkerServer.get(); }
    WebSharedWorkerServer& ensureSharedWorkerServer();

    NetworkStorageManager& storageManager() { return m_storageManager.get(); }
    CacheStorage::Engine& ensureCacheEngine();
    void clearCacheEngine();

    NetworkLoadScheduler& networkLoadScheduler();
    PCM::ManagerInterface& privateClickMeasurement() { return m_privateClickMeasurement.get(); }
    void setPrivateClickMeasurementDebugMode(bool);
    bool privateClickMeasurementDebugModeEnabled() const { return m_privateClickMeasurementDebugModeEnabled; }

#if PLATFORM(COCOA)
    AppPrivacyReportTestingData& appPrivacyReportTestingData() { return m_appPrivacyReportTestingData; }
#endif

    virtual void removeNetworkWebsiteData(std::optional<WallTime>, std::optional<HashSet<WebCore::RegistrableDomain>>&&, CompletionHandler<void()>&& completionHandler) { completionHandler(); }

    virtual void dataTaskWithRequest(WebPageProxyIdentifier, WebCore::ResourceRequest&&, CompletionHandler<void(DataTaskIdentifier)>&&) { }
    virtual void cancelDataTask(DataTaskIdentifier) { }
    virtual void addWebPageNetworkParameters(WebPageProxyIdentifier, WebPageNetworkParameters&&) { }
    virtual void removeWebPageNetworkParameters(WebPageProxyIdentifier) { }
    virtual size_t countNonDefaultSessionSets() const { return 0; }

    String attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier) const;

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    NetworkNotificationManager& notificationManager() { return m_notificationManager; }
#endif
    
#if !HAVE(NSURLSESSION_WEBSOCKET)
    bool shouldAcceptInsecureCertificatesForWebSockets() const { return m_shouldAcceptInsecureCertificatesForWebSockets; }
#endif

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    void setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit);
#endif

    static bool needsAdditionalNetworkConnectionIntegritySettings(const WebCore::ResourceRequest&);

protected:
    NetworkSession(NetworkProcess&, const NetworkSessionCreationParameters&);

#if ENABLE(TRACKING_PREVENTION)
    void forwardResourceLoadStatisticsSettings();
#endif

    PAL::SessionID m_sessionID;
    std::optional<UUID> m_dataStoreIdentifier;
    Ref<NetworkProcess> m_networkProcess;
    ThreadSafeWeakHashSet<NetworkDataTask> m_dataTaskSet;
#if ENABLE(TRACKING_PREVENTION)
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
    HashMap<String, WebCore::RegistrableDomain> m_firstPartyHostCNAMEDomains;
    HashMap<String, WebCore::IPAddress> m_firstPartyHostIPAddresses;
    std::optional<WebCore::RegistrableDomain> m_thirdPartyCNAMEDomainForTesting;
#endif
    bool m_isStaleWhileRevalidateEnabled { false };
    UniqueRef<PCM::ManagerInterface> m_privateClickMeasurement;
    bool m_privateClickMeasurementDebugModeEnabled { false };
    std::optional<WebCore::PrivateClickMeasurement> m_ephemeralMeasurement;
    bool m_isRunningEphemeralMeasurementTest { false };

    HashSet<Ref<NetworkResourceLoader>> m_keptAliveLoads;

    class CachedNetworkResourceLoader {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit CachedNetworkResourceLoader(Ref<NetworkResourceLoader>&&);
        RefPtr<NetworkResourceLoader> takeLoader();
    private:
        void expirationTimerFired();
        WebCore::Timer m_expirationTimer;
        RefPtr<NetworkResourceLoader> m_loader;
    };
    HashMap<NetworkResourceLoadIdentifier, std::unique_ptr<CachedNetworkResourceLoader>> m_loadersAwaitingWebProcessTransfer;

    PrefetchCache m_prefetchCache;

#if ASSERT_ENABLED
    bool m_isInvalidated { false };
#endif
    RefPtr<NetworkCache::Cache> m_cache;
    std::unique_ptr<NetworkLoadScheduler> m_networkLoadScheduler;
    WebCore::BlobRegistryImpl m_blobRegistry;
    UniqueRef<NetworkBroadcastChannelRegistry> m_broadcastChannelRegistry;
    unsigned m_testSpeedMultiplier { 1 };
    bool m_allowsServerPreconnect { true };
    bool m_shouldRunServiceWorkersOnMainThreadForTesting { false };
    std::optional<unsigned> m_overrideServiceWorkerRegistrationCountTestingValue;
#if ENABLE(SERVICE_WORKER)
    HashSet<std::unique_ptr<ServiceWorkerSoftUpdateLoader>> m_softUpdateLoaders;
    HashMap<WebCore::FetchIdentifier, WeakPtr<ServiceWorkerFetchTask>> m_navigationPreloaders;

    struct ServiceWorkerInfo {
        String databasePath;
        bool processTerminationDelayEnabled { true };
    };
    std::optional<ServiceWorkerInfo> m_serviceWorkerInfo;
    std::unique_ptr<WebCore::SWServer> m_swServer;
#endif
    std::unique_ptr<WebSharedWorkerServer> m_sharedWorkerServer;

    Ref<NetworkStorageManager> m_storageManager;
    String m_cacheStorageDirectory;
    RefPtr<CacheStorage::Engine> m_cacheEngine;
    Vector<Function<void(CacheStorage::Engine&)>> m_cacheStorageParametersCallbacks;

#if PLATFORM(COCOA)
    AppPrivacyReportTestingData m_appPrivacyReportTestingData;
#endif

    HashMap<WebPageProxyIdentifier, String> m_attributedBundleIdentifierFromPageIdentifiers;

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    NetworkNotificationManager m_notificationManager;
#endif
#if !HAVE(NSURLSESSION_WEBSOCKET)
    bool m_shouldAcceptInsecureCertificatesForWebSockets { false };
#endif
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    std::optional<int64_t> m_bytesPerSecondLimit;
#endif
};

} // namespace WebKit

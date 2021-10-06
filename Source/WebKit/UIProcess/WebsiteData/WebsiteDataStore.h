/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "LocalStorageDatabaseTracker.h"
#include "NetworkSessionCreationParameters.h"
#include "WebDeviceOrientationAndMotionAccessController.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageProxyIdentifier.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreConfiguration.h"
#include <WebCore/Cookie.h>
#include <WebCore/DeviceOrientationOrMotionPermissionState.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Identified.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if USE(CURL)
#include <WebCore/CurlProxySettings.h>
#endif

#if USE(SOUP)
#include "SoupCookiePersistentStorageType.h"
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/SoupNetworkProxySettings.h>
#endif

namespace API {
class HTTPCookieStore;
}

namespace WebCore {
class CertificateInfo;
class RegistrableDomain;
class SecurityOrigin;

struct MockWebAuthenticationConfiguration;
}

namespace WebKit {

class AuthenticatorManager;
class SecKeyProxyStore;
class DeviceIdHashSaltStorage;
class NetworkProcessProxy;
class SOAuthorizationCoordinator;
class WebPageProxy;
class WebProcessPool;
class WebProcessProxy;
class WebResourceLoadStatisticsStore;
enum class CacheModel : uint8_t;
enum class WebsiteDataFetchOption : uint8_t;
enum class WebsiteDataType : uint32_t;

struct NetworkProcessConnectionInfo;
struct WebsiteDataRecord;
struct WebsiteDataStoreParameters;

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
enum class ShouldGrandfatherStatistics : bool;
enum class StorageAccessStatus : uint8_t;
enum class StorageAccessPromptStatus;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
struct PluginModuleInfo;
#endif

class WebsiteDataStore : public API::ObjectImpl<API::Object::Type::WebsiteDataStore>, public Identified<WebsiteDataStore>, public CanMakeWeakPtr<WebsiteDataStore>  {
public:
    static Ref<WebsiteDataStore> defaultDataStore();
    static bool defaultDataStoreExists();
    static void deleteDefaultDataStoreForTesting();
    
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Ref<WebsiteDataStoreConfiguration>&&, PAL::SessionID);

    WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&&, PAL::SessionID);
    ~WebsiteDataStore();

    static void forEachWebsiteDataStore(Function<void(WebsiteDataStore&)>&&);
    
    NetworkProcessProxy& networkProcess() const;
    NetworkProcessProxy& networkProcess();
    NetworkProcessProxy* networkProcessIfExists() { return m_networkProcess.get(); }

    static WebsiteDataStore* existingDataStoreForSessionID(PAL::SessionID);

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    PAL::SessionID sessionID() const { return m_sessionID; }
    
    void registerProcess(WebProcessProxy&);
    void unregisterProcess(WebProcessProxy&);
    
    const WeakHashSet<WebProcessProxy>& processes() const { return m_processes; }

    enum class ShouldRetryOnFailure : bool { No, Yes };
    void getNetworkProcessConnection(WebProcessProxy&, CompletionHandler<void(const NetworkProcessConnectionInfo&)>&&, ShouldRetryOnFailure = ShouldRetryOnFailure::Yes);
    void terminateNetworkProcess();
    void sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&&);
    void sendNetworkProcessWillSuspendImminentlyForTesting();
    void sendNetworkProcessDidResume();
    void networkProcessDidTerminate(NetworkProcessProxy&);
    static void makeNextNetworkProcessLaunchFailForTesting();
    static bool shouldMakeNextNetworkProcessLaunchFailForTesting();

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsDebugMode() const;
    void setResourceLoadStatisticsDebugMode(bool);
    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);
    void isResourceLoadStatisticsEphemeral(CompletionHandler<void(bool)>&&) const;

    void setPrivateClickMeasurementDebugMode(bool);

    uint64_t perOriginStorageQuota() const { return m_resolvedConfiguration->perOriginStorageQuota(); }
    uint64_t perThirdPartyOriginStorageQuota() const;
    const String& cacheStorageDirectory() const { return m_resolvedConfiguration->cacheStorageDirectory(); }
    const String& generalStorageDirectory() const { return m_resolvedConfiguration->generalStorageDirectory(); }

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    void clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&&);
#endif

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, WallTime modifiedSince, Function<void()>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, Function<void()>&& completionHandler);

    void getLocalStorageDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&&);
    void setCacheModelSynchronouslyForTesting(CacheModel);
    void setServiceWorkerTimeoutForTesting(Seconds);
    void resetServiceWorkerTimeoutForTesting();

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    void fetchDataForRegistrableDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Vector<WebCore::RegistrableDomain>&&, CompletionHandler<void(Vector<WebsiteDataRecord>&&, HashSet<WebCore::RegistrableDomain>&&)>&&);
    void clearPrevalentResource(const URL&, CompletionHandler<void()>&&);
    void clearUserInteraction(const URL&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&&);
    void logTestingEvent(const String&);
    void logUserInteraction(const URL&, CompletionHandler<void()>&&);
    void getAllStorageAccessEntries(WebPageProxyIdentifier, CompletionHandler<void(Vector<String>&& domains)>&&);
    void hasHadUserInteraction(const URL&, CompletionHandler<void(bool)>&&);
    void isRelationshipOnlyInDatabaseOnce(const URL& subUrl, const URL& topUrl, CompletionHandler<void(bool)>&&);
    void isPrevalentResource(const URL&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const URL& hostRedirectedFrom, const URL& hostRedirectedTo, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const URL& subresource, const URL& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const URL& subFrame, const URL& topFrame, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const URL&, CompletionHandler<void(bool)>&&);
    void resetParametersToDefaultValues(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void getResourceLoadStatisticsDataSummary(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&&);
    void scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&&);
    void setGrandfathered(const URL&, bool, CompletionHandler<void()>&&);
    void isGrandfathered(const URL&, CompletionHandler<void(bool)>&&);
    void setGrandfatheringTime(Seconds, CompletionHandler<void()>&&);
    void setLastSeen(const URL&, Seconds, CompletionHandler<void()>&&);
    void domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&&);
    void statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&&);
    void mergeStatisticForTesting(const URL&, const URL& topFrameUrl1, const URL& topFrameUrl2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&&);
    void insertExpiredStatisticForTesting(const URL&, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool, CompletionHandler<void()>&&);
    void setIsRunningResourceLoadStatisticsTest(bool, CompletionHandler<void()>&&);
    void setPruneEntriesDownTo(size_t, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(const URL& subframe, const URL& topFrame, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(const URL& subresource, const URL& topFrame, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(const URL& subresource, const URL& hostNameRedirectedFrom, CompletionHandler<void()>&&);
    void setTimeToLiveUserInteraction(Seconds, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(const URL& topFrameHostName, const URL& hostNameRedirectedTo, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(const URL& topFrameHostName, const URL& hostNameRedirectedFrom, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(size_t, CompletionHandler<void()>&&);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds, CompletionHandler<void()>&&);
    void setPrevalentResource(const URL&, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(const URL&, CompletionHandler<void()>&&);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool, CompletionHandler<void()>&&);
    void setStatisticsTestingCallback(Function<void(const String&)>&&);
    bool hasStatisticsTestingCallback() const { return !!m_statisticsTestingCallback; }
    void setVeryPrevalentResource(const URL&, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(const URL& subframe, const URL& topFrame);
    void setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, CompletionHandler<void()>&&);
    void resetCrossSiteLoadsWithLinkDecorationForTesting(CompletionHandler<void()>&&);
    void deleteCookiesForTesting(const URL&, bool includeHttpOnlyCookies, CompletionHandler<void()>&&);
    void hasLocalStorageForTesting(const URL&, CompletionHandler<void(bool)>&&) const;
    void hasIsolatedSessionForTesting(const URL&, CompletionHandler<void(bool)>&&) const;
    void setResourceLoadStatisticsShouldDowngradeReferrerForTesting(bool, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(bool enabled, bool onlyOnSitesWithoutUserInteraction, CompletionHandler<void()>&&);
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(bool enabled, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(bool enabled, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(const URL&, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(const URL& firstPartyURL, const URL& cnameURL, CompletionHandler<void()>&&);
    void setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(const URL&, CompletionHandler<void()>&&);
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const;
    bool isItpStateExplicitlySet() const { return m_isItpStateExplicitlySet; }
    void useExplicitITPState() { m_isItpStateExplicitlySet = true; }
#endif
    void syncLocalStorage(CompletionHandler<void()>&&);
    void setCacheMaxAgeCapForPrevalentResources(Seconds, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&&);
    void resolveDirectoriesIfNecessary();
    const String& applicationCacheFlatFileSubdirectoryName() const { return m_configuration->applicationCacheFlatFileSubdirectoryName(); }
    const String& resolvedApplicationCacheDirectory() const { return m_resolvedConfiguration->applicationCacheDirectory(); }
    const String& resolvedLocalStorageDirectory() const { return m_resolvedConfiguration->localStorageDirectory(); }
    const String& resolvedNetworkCacheDirectory() const { return m_resolvedConfiguration->networkCacheDirectory(); }
    const String& resolvedAlternativeServicesStorageDirectory() const { return m_resolvedConfiguration->alternativeServicesDirectory(); }
    const String& resolvedMediaCacheDirectory() const { return m_resolvedConfiguration->mediaCacheDirectory(); }
    const String& resolvedMediaKeysDirectory() const { return m_resolvedConfiguration->mediaKeysStorageDirectory(); }
    const String& resolvedDatabaseDirectory() const { return m_resolvedConfiguration->webSQLDatabaseDirectory(); }
    const String& resolvedJavaScriptConfigurationDirectory() const { return m_resolvedConfiguration->javaScriptConfigurationDirectory(); }
    const String& resolvedCookieStorageFile() const { return m_resolvedConfiguration->cookieStorageFile(); }
    const String& resolvedIndexedDatabaseDirectory() const { return m_resolvedConfiguration->indexedDBDatabaseDirectory(); }
    const String& resolvedServiceWorkerRegistrationDirectory() const { return m_resolvedConfiguration->serviceWorkerRegistrationDirectory(); }
    const String& resolvedResourceLoadStatisticsDirectory() const { return m_resolvedConfiguration->resourceLoadStatisticsDirectory(); }
    const String& resolvedHSTSStorageDirectory() const { return m_resolvedConfiguration->hstsStorageDirectory(); }
#if ENABLE(ARKIT_INLINE_PREVIEW)
    const String& resolvedModelElementCacheDirectory() const { return m_resolvedConfiguration->modelElementCacheDirectory(); }
#endif

    static void setCachedProcessSuspensionDelayForTesting(Seconds);

    void allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo&, const String& host);
    void allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo&);

    DeviceIdHashSaltStorage& deviceIdHashSaltStorage() { return m_deviceIdHashSaltStorage.get(); }

    WebsiteDataStoreParameters parameters();
    static Vector<WebsiteDataStoreParameters> parametersFromEachWebsiteDataStore();

    void flushCookies(CompletionHandler<void()>&&);
    void clearCachedCredentials();

    void setAllowsAnySSLCertificateForWebSocket(bool);

    void dispatchOnQueue(Function<void()>&&);

#if PLATFORM(COCOA)
    void sendNetworkProcessXPCEndpointToWebProcess(WebProcessProxy&);
    void sendNetworkProcessXPCEndpointToAllWebProcesses();
    
    static bool useNetworkLoader();
#endif

#if USE(CURL)
    void setNetworkProxySettings(WebCore::CurlProxySettings&&);
    const WebCore::CurlProxySettings& networkProxySettings() const { return m_proxySettings; }
#endif

#if USE(SOUP)
    void setPersistentCredentialStorageEnabled(bool);
    bool persistentCredentialStorageEnabled() const { return m_persistentCredentialStorageEnabled && isPersistent(); }
    void setIgnoreTLSErrors(bool);
    bool ignoreTLSErrors() const { return m_ignoreTLSErrors; }
    void setNetworkProxySettings(WebCore::SoupNetworkProxySettings&&);
    const WebCore::SoupNetworkProxySettings& networkProxySettings() const { return m_networkProxySettings; }
    void setCookiePersistentStorage(const String&, SoupCookiePersistentStorageType);
    void setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy);
#endif

    static void allowWebsiteDataRecordsForAllOrigins();

#if HAVE(SEC_KEY_PROXY)
    void addSecKeyProxyStore(Ref<SecKeyProxyStore>&&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorManager& authenticatorManager() { return m_authenticatorManager.get(); }
    void setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&&);
#endif

    const WebsiteDataStoreConfiguration& configuration() { return m_configuration.get(); }

    WebsiteDataStoreClient& client() { return m_client.get(); }
    void setClient(UniqueRef<WebsiteDataStoreClient>&& client) { m_client = WTFMove(client); }

    API::HTTPCookieStore& cookieStore();

    void renameOriginInWebsiteData(URL&&, URL&&, OptionSet<WebsiteDataType>, CompletionHandler<void()>&&);

    bool networkProcessHasEntitlementForTesting(const String&);

#if ENABLE(DEVICE_ORIENTATION)
    WebDeviceOrientationAndMotionAccessController& deviceOrientationAndMotionAccessController() { return m_deviceOrientationAndMotionAccessController; }
#endif

#if HAVE(APP_SSO)
    SOAuthorizationCoordinator& soAuthorizationCoordinator() { return m_soAuthorizationCoordinator.get(); }
#endif

    static WTF::String defaultServiceWorkerRegistrationDirectory();
    static WTF::String defaultLocalStorageDirectory();
    static WTF::String defaultResourceLoadStatisticsDirectory();
    static WTF::String defaultNetworkCacheDirectory();
    static WTF::String defaultAlternativeServicesDirectory();
    static WTF::String defaultApplicationCacheDirectory();
    static WTF::String defaultWebSQLDatabaseDirectory();
#if USE(GLIB)
    static WTF::String defaultHSTSDirectory();
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    static WTF::String defaultModelElementCacheDirectory();
#endif
    static WTF::String defaultIndexedDBDatabaseDirectory();
    static WTF::String defaultCacheStorageDirectory();
    static WTF::String defaultGeneralStorageDirectory();
    static WTF::String defaultMediaCacheDirectory();
    static WTF::String defaultMediaKeysStorageDirectory();
    static WTF::String defaultDeviceIdHashSaltsStorageDirectory();
    static WTF::String defaultJavaScriptConfigurationDirectory();
    static bool http3Enabled();

    void resetQuota(CompletionHandler<void()>&&);
    void clearStorage(CompletionHandler<void()>&&);

#if ENABLE(APP_BOUND_DOMAINS)
    void hasAppBoundSession(CompletionHandler<void(bool)>&&) const;
    void clearAppBoundSession(CompletionHandler<void()>&&);
    void beginAppBoundDomainCheck(const String& host, const String& protocol, WebFramePolicyListenerProxy&);
    void getAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&&) const;
    void getAppBoundSchemes(CompletionHandler<void(const HashSet<String>&)>&&) const;
    void ensureAppBoundDomains(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&, const HashSet<String>&)>&&) const;
    void reinitializeAppBoundDomains();
    static void setAppBoundDomainsForTesting(HashSet<WebCore::RegistrableDomain>&&, CompletionHandler<void()>&&);
#endif
    void updateBundleIdentifierInNetworkProcess(const String&, CompletionHandler<void()>&&);
    void clearBundleIdentifierInNetworkProcess(CompletionHandler<void()>&&);

    void countNonDefaultSessionSets(CompletionHandler<void(size_t)>&&);

private:
    enum class ForceReinitialization : bool { No, Yes };
#if ENABLE(APP_BOUND_DOMAINS)
    void initializeAppBoundDomains(ForceReinitialization = ForceReinitialization::No);
    void addTestDomains() const;
#endif

    void fetchDataAndApply(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Ref<WorkQueue>&&, Function<void(Vector<WebsiteDataRecord>)>&& apply);

    void platformInitialize();
    void platformDestroy();
    static void platformRemoveRecentSearches(WallTime);

    void platformSetNetworkParameters(WebsiteDataStoreParameters&);

    WebsiteDataStore();

    enum class ShouldCreateDirectory { No, Yes };
    static String tempDirectoryFileSystemRepresentation(const String& directoryName, ShouldCreateDirectory = ShouldCreateDirectory::Yes);
    static String cacheDirectoryFileSystemRepresentation(const String& directoryName, ShouldCreateDirectory = ShouldCreateDirectory::Yes);
    static String websiteDataDirectoryFileSystemRepresentation(const String& directoryName);

    HashSet<RefPtr<WebProcessPool>> processPools(size_t limit = std::numeric_limits<size_t>::max()) const;

    // Will create a temporary process pool is none exists yet.
    HashSet<RefPtr<WebProcessPool>> ensureProcessPools() const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins() const;
#endif

    static Vector<WebCore::SecurityOriginData> mediaKeyOrigins(const String& mediaKeysStorageDirectory);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>&);

    void registerWithSessionIDMap();

#if ENABLE(APP_BOUND_DOMAINS)
    static std::optional<HashSet<WebCore::RegistrableDomain>> appBoundDomainsIfInitialized();
    constexpr static const std::atomic<bool> isAppBoundITPRelaxationEnabled = false;
    static void forwardAppBoundDomainsToITPIfInitialized(CompletionHandler<void()>&&);
    void setAppBoundDomainsForITP(const HashSet<WebCore::RegistrableDomain>&, CompletionHandler<void()>&&);
#endif

    const PAL::SessionID m_sessionID;

    Ref<WebsiteDataStoreConfiguration> m_resolvedConfiguration;
    Ref<const WebsiteDataStoreConfiguration> m_configuration;
    bool m_hasResolvedDirectories { false };

    const Ref<DeviceIdHashSaltStorage> m_deviceIdHashSaltStorage;

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    bool m_resourceLoadStatisticsDebugMode { false };
    bool m_resourceLoadStatisticsEnabled { false };
    Function<void(const String&)> m_statisticsTestingCallback;
#endif

    Ref<WorkQueue> m_queue;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_uiProcessCookieStorageIdentifier;
    RetainPtr<CFHTTPCookieStorageRef> m_cfCookieStorage;
#endif

#if USE(CURL)
    WebCore::CurlProxySettings m_proxySettings;
#endif

#if USE(SOUP)
    bool m_persistentCredentialStorageEnabled { true };
    bool m_ignoreTLSErrors { true };
    WebCore::SoupNetworkProxySettings m_networkProxySettings;
    String m_cookiePersistentStoragePath;
    SoupCookiePersistentStorageType m_cookiePersistentStorageType { SoupCookiePersistentStorageType::SQLite };
    WebCore::HTTPCookieAcceptPolicy m_cookieAcceptPolicy { WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain };
#endif

    WeakHashSet<WebProcessProxy> m_processes;

    bool m_isItpStateExplicitlySet { false };

#if HAVE(SEC_KEY_PROXY)
    Vector<Ref<SecKeyProxyStore>> m_secKeyProxyStores;
#endif

#if ENABLE(WEB_AUTHN)
    UniqueRef<AuthenticatorManager> m_authenticatorManager;
#endif

#if ENABLE(DEVICE_ORIENTATION)
    WebDeviceOrientationAndMotionAccessController m_deviceOrientationAndMotionAccessController;
#endif

    UniqueRef<WebsiteDataStoreClient> m_client;

    RefPtr<API::HTTPCookieStore> m_cookieStore;
    RefPtr<NetworkProcessProxy> m_networkProcess;

#if HAVE(APP_SSO)
    UniqueRef<SOAuthorizationCoordinator> m_soAuthorizationCoordinator;
#endif
#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    mutable std::optional<WebCore::ThirdPartyCookieBlockingMode> m_thirdPartyCookieBlockingMode; // Lazily computed.
#endif
};

}

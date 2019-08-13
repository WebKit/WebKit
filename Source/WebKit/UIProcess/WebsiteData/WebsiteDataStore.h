/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#endif

#if USE(CURL)
#include <WebCore/CurlProxySettings.h>
#endif

namespace API {
class HTTPCookieStore;
}

namespace WebCore {
class RegistrableDomain;
class SecurityOrigin;
}

namespace WebKit {

class AuthenticatorManager;
class SecKeyProxyStore;
class DeviceIdHashSaltStorage;
class SOAuthorizationCoordinator;
class WebPageProxy;
class WebProcessPool;
class WebProcessProxy;
class WebResourceLoadStatisticsStore;
enum class WebsiteDataFetchOption;
enum class WebsiteDataType;
struct MockWebAuthenticationConfiguration;
struct WebsiteDataRecord;
struct WebsiteDataStoreParameters;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
enum class ShouldGrandfatherStatistics : bool;
enum class StorageAccessStatus : uint8_t;
enum class StorageAccessPromptStatus;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
struct PluginModuleInfo;
#endif

class WebsiteDataStore : public RefCounted<WebsiteDataStore>, public Identified<WebsiteDataStore>, public CanMakeWeakPtr<WebsiteDataStore>  {
public:
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Ref<WebsiteDataStoreConfiguration>&&, PAL::SessionID);
    virtual ~WebsiteDataStore();

    static WebsiteDataStore* existingNonDefaultDataStoreForSessionID(PAL::SessionID);

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    PAL::SessionID sessionID() const { return m_sessionID; }
    
    void registerProcess(WebProcessProxy&);
    void unregisterProcess(WebProcessProxy&);
    
    const WeakHashSet<WebProcessProxy>& processes() const { return m_processes; }

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsDebugMode() const;
    void setResourceLoadStatisticsDebugMode(bool);
    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);

    uint64_t perOriginStorageQuota() const { return m_resolvedConfiguration->perOriginStorageQuota(); }
    uint64_t perThirdPartyOriginStorageQuota() const;
    void setPerOriginStorageQuota(uint64_t quota) { m_resolvedConfiguration->setPerOriginStorageQuota(quota); }
    const String& cacheStorageDirectory() const { return m_resolvedConfiguration->cacheStorageDirectory(); }
    void setCacheStorageDirectory(String&& directory) { m_resolvedConfiguration->setCacheStorageDirectory(WTFMove(directory)); }
    const String& serviceWorkerRegistrationDirectory() const { return m_resolvedConfiguration->serviceWorkerRegistrationDirectory(); }
    void setServiceWorkerRegistrationDirectory(String&& directory) { m_resolvedConfiguration->setServiceWorkerRegistrationDirectory(WTFMove(directory)); }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&&);
#endif

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, WallTime modifiedSince, Function<void()>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, Function<void()>&& completionHandler);

    void getLocalStorageDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&&);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void fetchDataForRegistrableDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<WebCore::RegistrableDomain>&, CompletionHandler<void(Vector<WebsiteDataRecord>&&, HashSet<WebCore::RegistrableDomain>&&)>&&);
    void clearPrevalentResource(const URL&, CompletionHandler<void()>&&);
    void clearUserInteraction(const URL&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&&);
    void logTestingEvent(const String&);
    void logUserInteraction(const URL&, CompletionHandler<void()>&&);
    void getAllStorageAccessEntries(WebCore::PageIdentifier, CompletionHandler<void(Vector<String>&& domains)>&&);
    void hasHadUserInteraction(const URL&, CompletionHandler<void(bool)>&&);
    void isPrevalentResource(const URL&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const URL& hostRedirectedFrom, const URL& hostRedirectedTo, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const URL& subresource, const URL& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const URL& subFrame, const URL& topFrame, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const URL&, CompletionHandler<void(bool)>&&);
    void resetParametersToDefaultValues(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&&);
    void submitTelemetry();
    void setGrandfathered(const URL&, bool, CompletionHandler<void()>&&);
    void setGrandfatheringTime(Seconds, CompletionHandler<void()>&&);
    void setLastSeen(const URL&, Seconds, CompletionHandler<void()>&&);
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
    void setNotifyPagesWhenTelemetryWasCaptured(bool, CompletionHandler<void()>&&);
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
#endif
    void setCacheMaxAgeCapForPrevalentResources(Seconds, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&&);
    void resolveDirectoriesIfNecessary();
    const String& applicationCacheFlatFileSubdirectoryName() const { return m_configuration->applicationCacheFlatFileSubdirectoryName(); }
    const String& resolvedApplicationCacheDirectory() const { return m_resolvedConfiguration->applicationCacheDirectory(); }
    const String& resolvedLocalStorageDirectory() const { return m_resolvedConfiguration->localStorageDirectory(); }
    const String& resolvedNetworkCacheDirectory() const { return m_resolvedConfiguration->networkCacheDirectory(); }
    const String& resolvedMediaCacheDirectory() const { return m_resolvedConfiguration->mediaCacheDirectory(); }
    const String& resolvedMediaKeysDirectory() const { return m_resolvedConfiguration->mediaKeysStorageDirectory(); }
    const String& resolvedDatabaseDirectory() const { return m_resolvedConfiguration->webSQLDatabaseDirectory(); }
    const String& resolvedJavaScriptConfigurationDirectory() const { return m_resolvedConfiguration->javaScriptConfigurationDirectory(); }
    const String& resolvedCookieStorageFile() const { return m_resolvedConfiguration->cookieStorageFile(); }
    const String& resolvedIndexedDatabaseDirectory() const { return m_resolvedConfiguration->indexedDBDatabaseDirectory(); }
    const String& resolvedServiceWorkerRegistrationDirectory() const { return m_resolvedConfiguration->serviceWorkerRegistrationDirectory(); }
    const String& resolvedResourceLoadStatisticsDirectory() const { return m_resolvedConfiguration->resourceLoadStatisticsDirectory(); }

    DeviceIdHashSaltStorage& deviceIdHashSaltStorage() { return m_deviceIdHashSaltStorage.get(); }

    WebProcessPool* processPoolForCookieStorageOperations();
    bool isAssociatedProcessPool(WebProcessPool&) const;

    WebsiteDataStoreParameters parameters();

    Vector<WebCore::Cookie> pendingCookies() const;
    void addPendingCookie(const WebCore::Cookie&);
    void removePendingCookie(const WebCore::Cookie&);
    void clearPendingCookies();

    void setBoundInterfaceIdentifier(String&& identifier) { m_boundInterfaceIdentifier = WTFMove(identifier); }
    const String& boundInterfaceIdentifier() { return m_boundInterfaceIdentifier; }

    const String& sourceApplicationBundleIdentifier() const { return m_sourceApplicationBundleIdentifier; }
    bool setSourceApplicationBundleIdentifier(String&&);

    const String& sourceApplicationSecondaryIdentifier() const { return m_sourceApplicationSecondaryIdentifier; }
    bool setSourceApplicationSecondaryIdentifier(String&&);

    bool allowsTLSFallback() const { return m_allowsTLSFallback; }
    bool setAllowsTLSFallback(bool);

    void networkingHasBegun() { m_networkingHasBegun = true; }
    
    void setAllowsCellularAccess(AllowsCellularAccess allows) { m_allowsCellularAccess = allows; }
    AllowsCellularAccess allowsCellularAccess() { return m_allowsCellularAccess; }

#if PLATFORM(COCOA)
    void setProxyConfiguration(CFDictionaryRef configuration) { m_proxyConfiguration = configuration; }
    CFDictionaryRef proxyConfiguration() { return m_proxyConfiguration.get(); }
#endif

#if USE(CURL)
    void setNetworkProxySettings(WebCore::CurlProxySettings&&);
    const WebCore::CurlProxySettings& networkProxySettings() const { return m_proxySettings; }
#endif

    static void allowWebsiteDataRecordsForAllOrigins();

#if HAVE(SEC_KEY_PROXY)
    void addSecKeyProxyStore(Ref<SecKeyProxyStore>&&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorManager& authenticatorManager() { return m_authenticatorManager.get(); }
    void setMockWebAuthenticationConfiguration(MockWebAuthenticationConfiguration&&);
#endif

    void didCreateNetworkProcess();

    const WebsiteDataStoreConfiguration& configuration() { return m_configuration.get(); }

    WebsiteDataStoreClient& client() { return m_client.get(); }
    void setClient(UniqueRef<WebsiteDataStoreClient>&& client) { m_client = WTFMove(client); }

    API::HTTPCookieStore& cookieStore();

#if ENABLE(DEVICE_ORIENTATION)
    WebDeviceOrientationAndMotionAccessController& deviceOrientationAndMotionAccessController() { return m_deviceOrientationAndMotionAccessController; }
#endif

#if HAVE(APP_SSO)
    SOAuthorizationCoordinator& soAuthorizationCoordinator() { return m_soAuthorizationCoordinator.get(); }
#endif

private:
    explicit WebsiteDataStore(PAL::SessionID);
    explicit WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&&, PAL::SessionID);

    void fetchDataAndApply(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, RefPtr<WorkQueue>&&, Function<void(Vector<WebsiteDataRecord>)>&& apply);

    void platformInitialize();
    void platformDestroy();
    static void platformRemoveRecentSearches(WallTime);

#if USE(CURL) || USE(SOUP)
    void platformSetNetworkParameters(WebsiteDataStoreParameters&);
#endif

    HashSet<RefPtr<WebProcessPool>> processPools(size_t count = std::numeric_limits<size_t>::max(), bool ensureAPoolExists = true) const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins() const;
#endif

    static Vector<WebCore::SecurityOriginData> mediaKeyOrigins(const String& mediaKeysStorageDirectory);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>&);

    void maybeRegisterWithSessionIDMap();

    const PAL::SessionID m_sessionID;

    Ref<WebsiteDataStoreConfiguration> m_resolvedConfiguration;
    Ref<const WebsiteDataStoreConfiguration> m_configuration;
    bool m_hasResolvedDirectories { false };

    const Ref<DeviceIdHashSaltStorage> m_deviceIdHashSaltStorage;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;
    bool m_resourceLoadStatisticsDebugMode { false };
    bool m_resourceLoadStatisticsEnabled { false };
    WTF::Function<void(const String&)> m_statisticsTestingCallback;
#endif

    Ref<WorkQueue> m_queue;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_uiProcessCookieStorageIdentifier;
    RetainPtr<CFHTTPCookieStorageRef> m_cfCookieStorage;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
#endif

#if USE(CURL)
    WebCore::CurlProxySettings m_proxySettings;
#endif

    HashSet<WebCore::Cookie> m_pendingCookies;
    
    WeakHashSet<WebProcessProxy> m_processes;

    String m_boundInterfaceIdentifier;
    AllowsCellularAccess m_allowsCellularAccess { AllowsCellularAccess::Yes };
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    bool m_allowsTLSFallback { true };
    bool m_networkingHasBegun { false };

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

#if HAVE(APP_SSO)
    UniqueRef<SOAuthorizationCoordinator> m_soAuthorizationCoordinator;
#endif
};

}

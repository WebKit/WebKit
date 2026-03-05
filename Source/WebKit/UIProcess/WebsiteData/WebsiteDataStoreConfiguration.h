/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <wtf/Markable.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class UnifiedOriginStorageLevel : uint8_t;

namespace WebPushD {
struct WebPushDaemonConnectionConfiguration;
}

enum class IsPersistent : bool { No, Yes };

class WebsiteDataStoreConfiguration : public API::ObjectImpl<API::Object::Type::WebsiteDataStoreConfiguration> {
public:
    enum class ShouldInitializePaths : bool { No, Yes };
    static Ref<WebsiteDataStoreConfiguration> create(IsPersistent isPersistent) { return adoptRef(*new WebsiteDataStoreConfiguration(isPersistent, ShouldInitializePaths::Yes)); }
    WebsiteDataStoreConfiguration(IsPersistent, ShouldInitializePaths = ShouldInitializePaths::Yes);
    WebsiteDataStoreConfiguration(const String& baseCacheDirectory, const String& baseDataDirectory);

#if PLATFORM(COCOA)
    static Ref<WebsiteDataStoreConfiguration> create(const WTF::UUID& identifier) { return adoptRef(*new WebsiteDataStoreConfiguration(identifier)); }
    WebsiteDataStoreConfiguration(const WTF::UUID&);
#endif

#if !PLATFORM(COCOA)
    // All cache and data directories are initialized relative to baseCacheDirectory and
    // baseDataDirectory, respectively, if provided. On Cocoa ports, these are always null.
    static Ref<WebsiteDataStoreConfiguration> createWithBaseDirectories(const String& baseCacheDirectory, const String& baseDataDirectory) { return adoptRef(*new WebsiteDataStoreConfiguration(baseCacheDirectory, baseDataDirectory)); }
#endif

    Ref<WebsiteDataStoreConfiguration> copy() const;

    bool isPersistent() const { return m_isPersistent == IsPersistent::Yes; }
    std::optional<WTF::UUID> identifier() const { return m_identifier; }

    uint64_t perOriginStorageQuota() const { return m_perOriginStorageQuota; }
    void setPerOriginStorageQuota(uint64_t quota) { m_perOriginStorageQuota = quota; }

    std::optional<double> originQuotaRatio() const { return m_originQuotaRatio; }
    void setOriginQuotaRatio(std::optional<double> ratio) { m_originQuotaRatio = ratio; }

    std::optional<double> totalQuotaRatio() const { return m_totalQuotaRatio; }
    void setTotalQuotaRatio(std::optional<double> ratio) { m_totalQuotaRatio = ratio; }

    std::optional<uint64_t> standardVolumeCapacity() const { return m_standardVolumeCapacity; }
    void setStandardVolumeCapacity(std::optional<uint64_t> capacity) { m_standardVolumeCapacity = capacity; }

    std::optional<uint64_t> volumeCapacityOverride() const { return m_volumeCapacityOverride; }
    void setVolumeCapacityOverride(std::optional<uint64_t> capacity) { m_volumeCapacityOverride = capacity; }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    bool isDeclarativeWebPushEnabled() const { return m_isDeclarativeWebPushEnabled; }
    void setIsDeclarativeWebPushEnabled(bool enabled) { m_isDeclarativeWebPushEnabled = enabled; }
#endif
    
    const String& mediaCacheDirectory() const LIFETIME_BOUND { return m_directories.mediaCacheDirectory; }
    void setMediaCacheDirectory(String&& directory) { m_directories.mediaCacheDirectory = WTF::move(directory); }
    
    const String& mediaKeysStorageDirectory() const LIFETIME_BOUND { return m_directories.mediaKeysStorageDirectory; }
    void setMediaKeysStorageDirectory(String&& directory) { m_directories.mediaKeysStorageDirectory = WTF::move(directory); }
    
    const String& alternativeServicesDirectory() const LIFETIME_BOUND { return m_directories.alternativeServicesDirectory; }
    void setAlternativeServicesDirectory(String&& directory) { m_directories.alternativeServicesDirectory = WTF::move(directory); }

    const String& javaScriptConfigurationDirectory() const LIFETIME_BOUND { return m_directories.javaScriptConfigurationDirectory; }
    void setJavaScriptConfigurationDirectory(String&& directory) { m_directories.javaScriptConfigurationDirectory = WTF::move(directory); }

    const String& searchFieldHistoryDirectory() const LIFETIME_BOUND { return m_directories.searchFieldHistoryDirectory; }
    void setSearchFieldHistoryDirectory(String&& directory) { m_directories.searchFieldHistoryDirectory = WTF::move(directory); }

    // indexedDBDatabaseDirectory is sort of deprecated. Data is migrated from here to
    // generalStoragePath unless useCustomStoragePaths is true.
    const String& indexedDBDatabaseDirectory() const LIFETIME_BOUND { return m_directories.indexedDBDatabaseDirectory; }
    void setIndexedDBDatabaseDirectory(String&& directory) { m_directories.indexedDBDatabaseDirectory = WTF::move(directory); }

    const String& webSQLDatabaseDirectory() const LIFETIME_BOUND { return m_directories.webSQLDatabaseDirectory; }
    void setWebSQLDatabaseDirectory(String&& directory) { m_directories.webSQLDatabaseDirectory = WTF::move(directory); }

    const String& hstsStorageDirectory() const LIFETIME_BOUND { return m_directories.hstsStorageDirectory; }
    void setHSTSStorageDirectory(String&& directory) { m_directories.hstsStorageDirectory = WTF::move(directory); }

    // localStorageDirectory is sort of deprecated. Data is migrated from here to
    // generalStoragePath unless useCustomStoragePaths is true.
    const String& localStorageDirectory() const LIFETIME_BOUND { return m_directories.localStorageDirectory; }
    void setLocalStorageDirectory(String&& directory) { m_directories.localStorageDirectory = WTF::move(directory); }

#if ENABLE(ARKIT_INLINE_PREVIEW)
    const String& modelElementCacheDirectory() const LIFETIME_BOUND { return m_directories.modelElementCacheDirectory; }
    void setModelElementCacheDirectory(String&& directory) { m_directories.modelElementCacheDirectory = WTF::move(directory); }
#endif

    const String& boundInterfaceIdentifier() const LIFETIME_BOUND { return m_boundInterfaceIdentifier; }
    void setBoundInterfaceIdentifier(String&& identifier) { m_boundInterfaceIdentifier = WTF::move(identifier); }

    bool allowsCellularAccess() const { return m_allowsCellularAccess; }
    void setAllowsCellularAccess(bool allows) { m_allowsCellularAccess = allows; }

    bool legacyTLSEnabled() const { return m_legacyTLSEnabled; }
    void setLegacyTLSEnabled(bool enabled) { m_legacyTLSEnabled = enabled; }

    bool fastServerTrustEvaluationEnabled() const { return m_fastServerTrustEvaluationEnabled; }
    void setFastServerTrustEvaluationEnabled(bool enabled) { m_fastServerTrustEvaluationEnabled = enabled; }

    bool networkCacheSpeculativeValidationEnabled() const { return m_networkCacheSpeculativeValidationEnabled; }
    void setNetworkCacheSpeculativeValidationEnabled(bool enabled) { m_networkCacheSpeculativeValidationEnabled = enabled; }

    bool testingSessionEnabled() const { return m_testingSessionEnabled; }
    void setTestingSessionEnabled(bool enabled) { m_testingSessionEnabled = enabled; }

    bool staleWhileRevalidateEnabled() const { return m_staleWhileRevalidateEnabled; }
    void setStaleWhileRevalidateEnabled(bool enabled) { m_staleWhileRevalidateEnabled = enabled; }

    bool resourceLoadStatisticsDebugModeEnabled() const { return m_trackingPreventionDebugModeEnabled; }
    void setResourceLoadStatisticsDebugModeEnabled(bool enabled) { m_trackingPreventionDebugModeEnabled = enabled; }

    std::optional<bool> defaultTrackingPreventionEnabledOverride() const { return m_defaultTrackingPreventionEnabledOverride; }
    void setDefaultTrackingPreventionEnabledOverride(std::optional<bool> enabled) { m_defaultTrackingPreventionEnabledOverride = enabled; }

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }
    void setTestSpeedMultiplier(unsigned multiplier) { m_testSpeedMultiplier = multiplier; }

#if PLATFORM(COCOA)
    CFDictionaryRef proxyConfiguration() const { return m_proxyConfiguration.get(); }
    void setProxyConfiguration(CFDictionaryRef configuration) { m_proxyConfiguration = configuration; }
#endif
    
    const String& deviceIdHashSaltsStorageDirectory() const LIFETIME_BOUND { return m_directories.deviceIdHashSaltsStorageDirectory; }
    void setDeviceIdHashSaltsStorageDirectory(String&& directory) { m_directories.deviceIdHashSaltsStorageDirectory = WTF::move(directory); }

#if ENABLE(ENCRYPTED_MEDIA)
    const String& mediaKeysHashSaltsStorageDirectory() const LIFETIME_BOUND { return m_directories.mediaKeysHashSaltsStorageDirectory; }
    void setMediaKeysHashSaltsStorageDirectory(String&& directory) { m_directories.mediaKeysHashSaltsStorageDirectory = WTF::move(directory); }
#endif

    const String& cookieStorageFile() const LIFETIME_BOUND { return m_directories.cookieStorageFile; }
    void setCookieStorageFile(String&& directory) { m_directories.cookieStorageFile = WTF::move(directory); }
    
    const String& resourceLoadStatisticsDirectory() const LIFETIME_BOUND { return m_directories.resourceLoadStatisticsDirectory; }
    void setResourceLoadStatisticsDirectory(String&& directory) { m_directories.resourceLoadStatisticsDirectory = WTF::move(directory); }

    const String& networkCacheDirectory() const LIFETIME_BOUND { return m_directories.networkCacheDirectory; }
    void setNetworkCacheDirectory(String&& directory) { m_directories.networkCacheDirectory = WTF::move(directory); }
    
    const String& cacheStorageDirectory() const LIFETIME_BOUND { return m_directories.cacheStorageDirectory; }
    void setCacheStorageDirectory(String&& directory) { m_directories.cacheStorageDirectory = WTF::move(directory); }

    const String& generalStorageDirectory() const LIFETIME_BOUND { return m_directories.generalStorageDirectory; }
    void setGeneralStorageDirectory(String&& directory) { m_directories.generalStorageDirectory = WTF::move(directory); }

    UnifiedOriginStorageLevel unifiedOriginStorageLevel() const { return m_unifiedOriginStorageLevel; }
    void setUnifiedOriginStorageLevel(UnifiedOriginStorageLevel level) { m_unifiedOriginStorageLevel = level; }

    const String& webPushPartitionString() const LIFETIME_BOUND { return m_webPushPartitionString; }
    void setWebPushPartitionString(String&& string) { m_webPushPartitionString = WTF::move(string); }
    
    const String& serviceWorkerRegistrationDirectory() const LIFETIME_BOUND { return m_directories.serviceWorkerRegistrationDirectory; }
    void setServiceWorkerRegistrationDirectory(String&& directory) { m_directories.serviceWorkerRegistrationDirectory = WTF::move(directory); }
    
    bool serviceWorkerProcessTerminationDelayEnabled() const { return m_serviceWorkerProcessTerminationDelayEnabled; }
    void setServiceWorkerProcessTerminationDelayEnabled(bool enabled) { m_serviceWorkerProcessTerminationDelayEnabled = enabled; }

    const String& sourceApplicationBundleIdentifier() const LIFETIME_BOUND { return m_sourceApplicationBundleIdentifier; }
    void setSourceApplicationBundleIdentifier(String&& identifier) { m_sourceApplicationBundleIdentifier = WTF::move(identifier); }

    const String& sourceApplicationSecondaryIdentifier() const LIFETIME_BOUND { return m_sourceApplicationSecondaryIdentifier; }
    void setSourceApplicationSecondaryIdentifier(String&& identifier) { m_sourceApplicationSecondaryIdentifier = WTF::move(identifier); }
    
#if ENABLE(CONTENT_EXTENSIONS)
    const String& resourceMonitorThrottlerDirectory() const LIFETIME_BOUND { return m_directories.resourceMonitorThrottlerDirectory; }
    void setResourceMonitorThrottlerDirectory(String&& directory) { m_directories.resourceMonitorThrottlerDirectory = WTF::move(directory); }
#endif

    const URL& httpProxy() const LIFETIME_BOUND { return m_httpProxy; }
    void setHTTPProxy(URL&& proxy) { m_httpProxy = WTF::move(proxy); }

    const URL& httpsProxy() const LIFETIME_BOUND { return m_httpsProxy; }
    void setHTTPSProxy(URL&& proxy) { m_httpsProxy = WTF::move(proxy); }

    bool deviceManagementRestrictionsEnabled() const { return m_deviceManagementRestrictionsEnabled; }
    void setDeviceManagementRestrictionsEnabled(bool enabled) { m_deviceManagementRestrictionsEnabled = enabled; }

    bool allLoadsBlockedByDeviceManagementRestrictionsForTesting() const { return m_allLoadsBlockedByDeviceManagementRestrictionsForTesting; }
    void setAllLoadsBlockedByDeviceManagementRestrictionsForTesting(bool blocked) { m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = blocked; }

    WebPushD::WebPushDaemonConnectionConfiguration webPushDaemonConnectionConfiguration() const;

    const String& dataConnectionServiceType() const { return m_dataConnectionServiceType; }
    void setDataConnectionServiceType(String&& type) { m_dataConnectionServiceType = WTF::move(type); }
    
    bool suppressesConnectionTerminationOnSystemChange() const { return m_suppressesConnectionTerminationOnSystemChange; }
    void setSuppressesConnectionTerminationOnSystemChange(bool suppresses) { m_suppressesConnectionTerminationOnSystemChange = suppresses; }

    bool allowsServerPreconnect() const { return m_allowsServerPreconnect; }
    void setAllowsServerPreconnect(bool allows) { m_allowsServerPreconnect = allows; }

    bool preventsSystemHTTPProxyAuthentication() const { return m_preventsSystemHTTPProxyAuthentication; }
    void setPreventsSystemHTTPProxyAuthentication(bool prevents) { m_preventsSystemHTTPProxyAuthentication = prevents; }

    bool requiresSecureHTTPSProxyConnection() const { return m_requiresSecureHTTPSProxyConnection; };
    void setRequiresSecureHTTPSProxyConnection(bool requiresSecureProxy) { m_requiresSecureHTTPSProxyConnection = requiresSecureProxy; }

    bool shouldRunServiceWorkersOnMainThreadForTesting() const { return m_shouldRunServiceWorkersOnMainThreadForTesting; }
    void setShouldRunServiceWorkersOnMainThreadForTesting(bool shouldRunOnMainThread) { m_shouldRunServiceWorkersOnMainThreadForTesting = shouldRunOnMainThread; }
    std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue() const { return m_overrideServiceWorkerRegistrationCountTestingValue; }
    void setOverrideServiceWorkerRegistrationCountTestingValue(unsigned count) { m_overrideServiceWorkerRegistrationCountTestingValue = count; }

    const URL& standaloneApplicationURL() const LIFETIME_BOUND { return m_standaloneApplicationURL; }
    void setStandaloneApplicationURL(URL&& url) { m_standaloneApplicationURL = WTF::move(url); }

    bool enableInAppBrowserPrivacyForTesting() const { return m_enableInAppBrowserPrivacyForTesting; }
    void setEnableInAppBrowserPrivacyForTesting(bool value) { m_enableInAppBrowserPrivacyForTesting = value; }
    
    bool allowsHSTSWithUntrustedRootCertificate() const { return m_allowsHSTSWithUntrustedRootCertificate; }
    void setAllowsHSTSWithUntrustedRootCertificate(bool allows) { m_allowsHSTSWithUntrustedRootCertificate = allows; }
    
    void setPCMMachServiceName(String&& name) { m_pcmMachServiceName = WTF::move(name); }
    const String& pcmMachServiceName() const LIFETIME_BOUND { return m_pcmMachServiceName; }

    void setWebPushMachServiceName(String&& name) { m_webPushMachServiceName = WTF::move(name); }
    const String& webPushMachServiceName() const LIFETIME_BOUND { return m_webPushMachServiceName; }

    void setMemoryFootprintNotificationThresholds(Vector<size_t>&& thresholds) { m_memoryFootprintNotificationThresholds = WTF::move(thresholds); }
    const Vector<size_t>& memoryFootprintNotificationThresholds() LIFETIME_BOUND { return m_memoryFootprintNotificationThresholds; }

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    void setWebContentRestrictionsConfigurationFile(String&& file) { m_webContentRestrictionsConfigurationFile = WTF::move(file); }
    const String& webContentRestrictionsConfigurationFile() const LIFETIME_BOUND { return m_webContentRestrictionsConfigurationFile; }
#endif

    void setAdditionalDomainsWithUserInteractionForTesting(String&& domains) { m_additionalDomainsWithUserInteractionForTesting = WTF::move(domains); }
    const String& additionalDomainsWithUserInteractionForTesting() const LIFETIME_BOUND { return m_additionalDomainsWithUserInteractionForTesting; }

    const String& enhancedSecurityDirectory() const LIFETIME_BOUND { return m_directories.enhancedSecurityDirectory; }
    void setEnhancedSecurityDirectory(String&& directory) { m_directories.enhancedSecurityDirectory = WTF::move(directory); }

    struct Directories {
        String alternativeServicesDirectory;
        String cacheStorageDirectory;
        String cookieStorageFile;
        String deviceIdHashSaltsStorageDirectory;
#if ENABLE(ENCRYPTED_MEDIA)
        String mediaKeysHashSaltsStorageDirectory;
#endif
        String generalStorageDirectory;
        String hstsStorageDirectory;
        String indexedDBDatabaseDirectory;
        String javaScriptConfigurationDirectory;
        String localStorageDirectory;
        String mediaCacheDirectory;
        String mediaKeysStorageDirectory;
        String networkCacheDirectory;
        String resourceLoadStatisticsDirectory;
        String searchFieldHistoryDirectory;
        String serviceWorkerRegistrationDirectory;
        String webSQLDatabaseDirectory;
#if ENABLE(ARKIT_INLINE_PREVIEW)
        String modelElementCacheDirectory;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
        String resourceMonitorThrottlerDirectory;
#endif
        String enhancedSecurityDirectory;
        Directories isolatedCopy() const&;
        Directories isolatedCopy() &&;
    };
    const Directories& directories() const LIFETIME_BOUND { return m_directories; }

private:
    static Ref<WebsiteDataStoreConfiguration> create(IsPersistent isPersistent, ShouldInitializePaths shouldInitializePaths) { return adoptRef(*new WebsiteDataStoreConfiguration(isPersistent, shouldInitializePaths)); }

    void initializePaths();

    IsPersistent m_isPersistent { IsPersistent::No };

    UnifiedOriginStorageLevel m_unifiedOriginStorageLevel;
    Markable<WTF::UUID> m_identifier;
    String m_baseCacheDirectory;
    String m_baseDataDirectory;
    Directories m_directories;
    uint64_t m_perOriginStorageQuota;
    std::optional<double> m_originQuotaRatio;
    std::optional<double> m_totalQuotaRatio;
    std::optional<uint64_t> m_standardVolumeCapacity;
    std::optional<uint64_t> m_volumeCapacityOverride;
#if USE(GLIB)
    bool m_networkCacheSpeculativeValidationEnabled { true };
#else
    bool m_networkCacheSpeculativeValidationEnabled { false };
#endif
    bool m_staleWhileRevalidateEnabled { true };
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    String m_boundInterfaceIdentifier;
    String m_dataConnectionServiceType;
    URL m_httpProxy;
    URL m_httpsProxy;
    bool m_deviceManagementRestrictionsEnabled { false };
    bool m_allLoadsBlockedByDeviceManagementRestrictionsForTesting { false };
    bool m_allowsCellularAccess { true };
#if ENABLE(TLS_1_2_DEFAULT_MINIMUM)
    bool m_legacyTLSEnabled { false };
#else
    bool m_legacyTLSEnabled { true };
#endif
    bool m_fastServerTrustEvaluationEnabled { false };
    bool m_serviceWorkerProcessTerminationDelayEnabled { true };
    bool m_testingSessionEnabled { false };
    bool m_suppressesConnectionTerminationOnSystemChange { false };
    bool m_allowsServerPreconnect { true };
    bool m_preventsSystemHTTPProxyAuthentication { false };
    bool m_requiresSecureHTTPSProxyConnection { false };
    bool m_shouldRunServiceWorkersOnMainThreadForTesting { false };
    std::optional<unsigned> m_overrideServiceWorkerRegistrationCountTestingValue;
    unsigned m_testSpeedMultiplier { 1 };
    URL m_standaloneApplicationURL;
    bool m_enableInAppBrowserPrivacyForTesting { false };
    bool m_allowsHSTSWithUntrustedRootCertificate { false };
    bool m_trackingPreventionDebugModeEnabled { false };
#if ENABLE(DECLARATIVE_WEB_PUSH)
    bool m_isDeclarativeWebPushEnabled { false };
#endif
    String m_pcmMachServiceName;
    String m_webPushMachServiceName;
    String m_webPushPartitionString;
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
#endif
    Vector<size_t> m_memoryFootprintNotificationThresholds;
    std::optional<bool> m_defaultTrackingPreventionEnabledOverride;
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    String m_webContentRestrictionsConfigurationFile;
#endif
    String m_additionalDomainsWithUserInteractionForTesting;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebsiteDataStoreConfiguration)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::WebsiteDataStoreConfiguration; }
SPECIALIZE_TYPE_TRAITS_END()

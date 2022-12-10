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
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

namespace WebPushD {
struct WebPushDaemonConnectionConfiguration;
}

enum class IsPersistent : bool { No, Yes };

class WebsiteDataStoreConfiguration : public API::ObjectImpl<API::Object::Type::WebsiteDataStoreConfiguration> {
public:
    enum class ShouldInitializePaths : bool { No, Yes };
    static Ref<WebsiteDataStoreConfiguration> create(IsPersistent isPersistent) { return adoptRef(*new WebsiteDataStoreConfiguration(isPersistent, ShouldInitializePaths::Yes)); }
    WebsiteDataStoreConfiguration(IsPersistent, ShouldInitializePaths = ShouldInitializePaths::Yes);

#if PLATFORM(COCOA)
    WebsiteDataStoreConfiguration(const UUID&);
#endif

#if !PLATFORM(COCOA)
    // All cache and data directories are initialized relative to baseCacheDirectory and
    // baseDataDirectory, respectively, if provided. On Cocoa ports, these are always null.
    static Ref<WebsiteDataStoreConfiguration> createWithBaseDirectories(const String& baseCacheDirectory, const String& baseDataDirectory) { return adoptRef(*new WebsiteDataStoreConfiguration(baseCacheDirectory, baseDataDirectory)); }
#endif

    Ref<WebsiteDataStoreConfiguration> copy() const;

    bool isPersistent() const { return m_isPersistent == IsPersistent::Yes; }
    std::optional<UUID> identifier() const { return m_identifier; }

    uint64_t perOriginStorageQuota() const { return m_perOriginStorageQuota; }
    void setPerOriginStorageQuota(uint64_t quota) { m_perOriginStorageQuota = quota; }

    const String& applicationCacheDirectory() const { return m_applicationCacheDirectory; }
    void setApplicationCacheDirectory(String&& directory) { m_applicationCacheDirectory = WTFMove(directory); }
    
    const String& mediaCacheDirectory() const { return m_mediaCacheDirectory; }
    void setMediaCacheDirectory(String&& directory) { m_mediaCacheDirectory = WTFMove(directory); }
    
    const String& mediaKeysStorageDirectory() const { return m_mediaKeysStorageDirectory; }
    void setMediaKeysStorageDirectory(String&& directory) { m_mediaKeysStorageDirectory = WTFMove(directory); }
    
    const String& alternativeServicesDirectory() const { return m_alternativeServicesDirectory; }
    void setAlternativeServicesDirectory(String&& directory) { m_alternativeServicesDirectory = WTFMove(directory); }

    const String& javaScriptConfigurationDirectory() const { return m_javaScriptConfigurationDirectory; }
    void setJavaScriptConfigurationDirectory(String&& directory) { m_javaScriptConfigurationDirectory = WTFMove(directory); }

    // indexedDBDatabaseDirectory is sort of deprecated. Data is migrated from here to
    // generalStoragePath unless useCustomStoragePaths is true.
    const String& indexedDBDatabaseDirectory() const { return m_indexedDBDatabaseDirectory; }
    void setIndexedDBDatabaseDirectory(String&& directory) { m_indexedDBDatabaseDirectory = WTFMove(directory); }

    const String& webSQLDatabaseDirectory() const { return m_webSQLDatabaseDirectory; }
    void setWebSQLDatabaseDirectory(String&& directory) { m_webSQLDatabaseDirectory = WTFMove(directory); }

    const String& hstsStorageDirectory() const { return m_hstsStorageDirectory; }
    void setHSTSStorageDirectory(String&& directory) { m_hstsStorageDirectory = WTFMove(directory); }

    // localStorageDirectory is sort of deprecated. Data is migrated from here to
    // generalStoragePath unless useCustomStoragePaths is true.
    const String& localStorageDirectory() const { return m_localStorageDirectory; }
    void setLocalStorageDirectory(String&& directory) { m_localStorageDirectory = WTFMove(directory); }

#if ENABLE(ARKIT_INLINE_PREVIEW)
    const String& modelElementCacheDirectory() const { return m_modelElementCacheDirectory; }
    void setModelElementCacheDirectory(String&& directory) { m_modelElementCacheDirectory = WTFMove(directory); }
#endif

    const String& boundInterfaceIdentifier() const { return m_boundInterfaceIdentifier; }
    void setBoundInterfaceIdentifier(String&& identifier) { m_boundInterfaceIdentifier = WTFMove(identifier); }

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

    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }
    void setTestSpeedMultiplier(unsigned multiplier) { m_testSpeedMultiplier = multiplier; }

#if PLATFORM(COCOA)
    CFDictionaryRef proxyConfiguration() const { return m_proxyConfiguration.get(); }
    void setProxyConfiguration(CFDictionaryRef configuration) { m_proxyConfiguration = configuration; }
#endif
    
    const String& deviceIdHashSaltsStorageDirectory() const { return m_deviceIdHashSaltsStorageDirectory; }
    void setDeviceIdHashSaltsStorageDirectory(String&& directory) { m_deviceIdHashSaltsStorageDirectory = WTFMove(directory); }

    const String& cookieStorageFile() const { return m_cookieStorageFile; }
    void setCookieStorageFile(String&& directory) { m_cookieStorageFile = WTFMove(directory); }
    
    const String& resourceLoadStatisticsDirectory() const { return m_resourceLoadStatisticsDirectory; }
    void setResourceLoadStatisticsDirectory(String&& directory) { m_resourceLoadStatisticsDirectory = WTFMove(directory); }

    const String& networkCacheDirectory() const { return m_networkCacheDirectory; }
    void setNetworkCacheDirectory(String&& directory) { m_networkCacheDirectory = WTFMove(directory); }
    
    const String& cacheStorageDirectory() const { return m_cacheStorageDirectory; }
    void setCacheStorageDirectory(String&& directory) { m_cacheStorageDirectory = WTFMove(directory); }

    const String& generalStorageDirectory() const { return m_generalStorageDirectory; }
    void setGeneralStorageDirectory(String&& directory) { m_generalStorageDirectory = WTFMove(directory); }

    // If true, store data in localStorageDirectory and indexedDBDatabaseDirectory. Otherwise,
    // migrate data from these locations to generalStorageDirectory.
    bool shouldUseCustomStoragePaths() const { return m_shouldUseCustomStoragePaths; }
    void setShouldUseCustomStoragePaths(bool use) { m_shouldUseCustomStoragePaths = use; }

    const String& webPushPartitionString() const { return m_webPushPartitionString; }
    void setWebPushPartitionString(String&& string) { m_webPushPartitionString = WTFMove(string); }

    const String& applicationCacheFlatFileSubdirectoryName() const { return m_applicationCacheFlatFileSubdirectoryName; }
    void setApplicationCacheFlatFileSubdirectoryName(String&& directory) { m_applicationCacheFlatFileSubdirectoryName = WTFMove(directory); }
    
    const String& serviceWorkerRegistrationDirectory() const { return m_serviceWorkerRegistrationDirectory; }
    void setServiceWorkerRegistrationDirectory(String&& directory) { m_serviceWorkerRegistrationDirectory = WTFMove(directory); }
    
    bool serviceWorkerProcessTerminationDelayEnabled() const { return m_serviceWorkerProcessTerminationDelayEnabled; }
    void setServiceWorkerProcessTerminationDelayEnabled(bool enabled) { m_serviceWorkerProcessTerminationDelayEnabled = enabled; }

    const String& sourceApplicationBundleIdentifier() const { return m_sourceApplicationBundleIdentifier; }
    void setSourceApplicationBundleIdentifier(String&& identifier) { m_sourceApplicationBundleIdentifier = WTFMove(identifier); }

    const String& sourceApplicationSecondaryIdentifier() const { return m_sourceApplicationSecondaryIdentifier; }
    void setSourceApplicationSecondaryIdentifier(String&& identifier) { m_sourceApplicationSecondaryIdentifier = WTFMove(identifier); }
    
    const URL& httpProxy() const { return m_httpProxy; }
    void setHTTPProxy(URL&& proxy) { m_httpProxy = WTFMove(proxy); }

    const URL& httpsProxy() const { return m_httpsProxy; }
    void setHTTPSProxy(URL&& proxy) { m_httpsProxy = WTFMove(proxy); }

    bool deviceManagementRestrictionsEnabled() const { return m_deviceManagementRestrictionsEnabled; }
    void setDeviceManagementRestrictionsEnabled(bool enabled) { m_deviceManagementRestrictionsEnabled = enabled; }

    bool allLoadsBlockedByDeviceManagementRestrictionsForTesting() const { return m_allLoadsBlockedByDeviceManagementRestrictionsForTesting; }
    void setAllLoadsBlockedByDeviceManagementRestrictionsForTesting(bool blocked) { m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = blocked; }

    bool webPushDaemonUsesMockBundlesForTesting() const { return m_webPushDaemonUsesMockBundlesForTesting; }
    void setWebPushDaemonUsesMockBundlesForTesting(bool usesMockBundles) { m_webPushDaemonUsesMockBundlesForTesting = usesMockBundles; }
    WebPushD::WebPushDaemonConnectionConfiguration webPushDaemonConnectionConfiguration() const;

    const String& dataConnectionServiceType() const { return m_dataConnectionServiceType; }
    void setDataConnectionServiceType(String&& type) { m_dataConnectionServiceType = WTFMove(type); }
    
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

    const URL& standaloneApplicationURL() const { return m_standaloneApplicationURL; }
    void setStandaloneApplicationURL(URL&& url) { m_standaloneApplicationURL = WTFMove(url); }

    bool enableInAppBrowserPrivacyForTesting() const { return m_enableInAppBrowserPrivacyForTesting; }
    void setEnableInAppBrowserPrivacyForTesting(bool value) { m_enableInAppBrowserPrivacyForTesting = value; }
    
    bool allowsHSTSWithUntrustedRootCertificate() const { return m_allowsHSTSWithUntrustedRootCertificate; }
    void setAllowsHSTSWithUntrustedRootCertificate(bool allows) { m_allowsHSTSWithUntrustedRootCertificate = allows; }
    
    void setPCMMachServiceName(String&& name) { m_pcmMachServiceName = WTFMove(name); }
    const String& pcmMachServiceName() const { return m_pcmMachServiceName; }

    void setWebPushMachServiceName(String&& name) { m_webPushMachServiceName = WTFMove(name); }
    const String& webPushMachServiceName() const { return m_webPushMachServiceName; }

#if !HAVE(NSURLSESSION_WEBSOCKET)
    bool shouldAcceptInsecureCertificatesForWebSockets() const { return m_shouldAcceptInsecureCertificatesForWebSockets; }
    void setShouldAcceptInsecureCertificatesForWebSockets(bool accept) { m_shouldAcceptInsecureCertificatesForWebSockets = accept; }
#endif

private:
    WebsiteDataStoreConfiguration(const String& baseCacheDirectory, const String& baseDataDirectory);
    static Ref<WebsiteDataStoreConfiguration> create(IsPersistent isPersistent, ShouldInitializePaths shouldInitializePaths) { return adoptRef(*new WebsiteDataStoreConfiguration(isPersistent, shouldInitializePaths)); }

    void initializePaths();

    IsPersistent m_isPersistent { IsPersistent::No };

    bool m_shouldUseCustomStoragePaths;
    std::optional<UUID> m_identifier;
    String m_baseCacheDirectory;
    String m_baseDataDirectory;
    String m_cacheStorageDirectory;
    String m_generalStorageDirectory;
    uint64_t m_perOriginStorageQuota;
    String m_networkCacheDirectory;
    String m_applicationCacheDirectory;
    String m_applicationCacheFlatFileSubdirectoryName { "Files"_s };
    String m_mediaCacheDirectory;
    String m_indexedDBDatabaseDirectory;
    String m_serviceWorkerRegistrationDirectory;
    String m_webSQLDatabaseDirectory;
    String m_hstsStorageDirectory;
#if ENABLE(ARKIT_INLINE_PREVIEW)
    String m_modelElementCacheDirectory;
#endif
#if USE(GLIB)
    bool m_networkCacheSpeculativeValidationEnabled { true };
#else
    bool m_networkCacheSpeculativeValidationEnabled { false };
#endif
    bool m_staleWhileRevalidateEnabled { true };
    String m_localStorageDirectory;
    String m_mediaKeysStorageDirectory;
    String m_alternativeServicesDirectory;
    String m_deviceIdHashSaltsStorageDirectory;
    String m_resourceLoadStatisticsDirectory;
    String m_javaScriptConfigurationDirectory;
    String m_cookieStorageFile;
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    String m_boundInterfaceIdentifier;
    String m_dataConnectionServiceType;
    URL m_httpProxy;
    URL m_httpsProxy;
    bool m_deviceManagementRestrictionsEnabled { false };
    bool m_allLoadsBlockedByDeviceManagementRestrictionsForTesting { false };
    bool m_webPushDaemonUsesMockBundlesForTesting { false };
    bool m_allowsCellularAccess { true };
    bool m_legacyTLSEnabled { true };
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
    String m_pcmMachServiceName;
    String m_webPushMachServiceName;
    String m_webPushPartitionString;
#if !HAVE(NSURLSESSION_WEBSOCKET)
    bool m_shouldAcceptInsecureCertificatesForWebSockets { false };
#endif
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
#endif
};

}

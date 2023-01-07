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

#include "config.h"
#include "WebsiteDataStoreConfiguration.h"

#include "UnifiedOriginStorageLevel.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebsiteDataStore.h"

namespace WebKit {

WebsiteDataStoreConfiguration::WebsiteDataStoreConfiguration(IsPersistent isPersistent, ShouldInitializePaths shouldInitializePaths)
    : m_isPersistent(isPersistent)
    , m_unifiedOriginStorageLevel(WebsiteDataStore::defaultUnifiedOriginStorageLevel())
    , m_perOriginStorageQuota(WebsiteDataStore::defaultPerOriginQuota())
{
    if (isPersistent == IsPersistent::Yes && shouldInitializePaths == ShouldInitializePaths::Yes) {
#if PLATFORM(GTK) || PLATFORM(WPE)
        // Storing data outside the permitted base cache and data directories is a serious error.
        // Use WebsiteDataStore::createWithBaseDirectories instead.
        RELEASE_ASSERT_NOT_REACHED();
#else
        // Other ports do not require use of the base directories.
        initializePaths();
#endif

#if PLATFORM(IOS)
        setPCMMachServiceName("com.apple.webkit.adattributiond.service"_s);
#endif
    }
}

#if PLATFORM(COCOA)

WebsiteDataStoreConfiguration::WebsiteDataStoreConfiguration(const UUID& identifier)
    : m_isPersistent(IsPersistent::Yes)
    , m_unifiedOriginStorageLevel(WebsiteDataStore::defaultUnifiedOriginStorageLevel())
    , m_identifier(identifier)
    , m_baseCacheDirectory(WebsiteDataStore::defaultWebsiteDataStoreDirectory(identifier))
    , m_baseDataDirectory(WebsiteDataStore::defaultWebsiteDataStoreDirectory(identifier))
    , m_perOriginStorageQuota(WebsiteDataStore::defaultPerOriginQuota())
#if PLATFORM(IOS)
    , m_pcmMachServiceName("com.apple.webkit.adattributiond.service"_s)
#endif
{
    ASSERT(m_identifier);

    initializePaths();
}

#endif

#if !PLATFORM(COCOA)
WebsiteDataStoreConfiguration::WebsiteDataStoreConfiguration(const String& baseCacheDirectory, const String& baseDataDirectory)
    : m_isPersistent(IsPersistent::Yes)
    , m_unifiedOriginStorageLevel(WebsiteDataStore::defaultUnifiedOriginStorageLevel())
    , m_baseCacheDirectory(baseCacheDirectory)
    , m_baseDataDirectory(baseDataDirectory)
    , m_perOriginStorageQuota(WebsiteDataStore::defaultPerOriginQuota())
{
    initializePaths();
}
#endif

void WebsiteDataStoreConfiguration::initializePaths()
{
    setApplicationCacheDirectory(WebsiteDataStore::defaultApplicationCacheDirectory(m_baseCacheDirectory));
    setCacheStorageDirectory(WebsiteDataStore::defaultCacheStorageDirectory(m_baseCacheDirectory));
    setGeneralStorageDirectory(WebsiteDataStore::defaultGeneralStorageDirectory(m_baseCacheDirectory));
    setNetworkCacheDirectory(WebsiteDataStore::defaultNetworkCacheDirectory(m_baseCacheDirectory));
    setMediaCacheDirectory(WebsiteDataStore::defaultMediaCacheDirectory(m_baseCacheDirectory));
#if USE(GLIB) || PLATFORM(COCOA)
    setHSTSStorageDirectory(WebsiteDataStore::defaultHSTSStorageDirectory(m_baseCacheDirectory));
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    setModelElementCacheDirectory(WebsiteDataStore::defaultModelElementCacheDirectory());
#endif

    setAlternativeServicesDirectory(WebsiteDataStore::defaultAlternativeServicesDirectory(m_baseDataDirectory));
    setIndexedDBDatabaseDirectory(WebsiteDataStore::defaultIndexedDBDatabaseDirectory(m_baseDataDirectory));
    setServiceWorkerRegistrationDirectory(WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(m_baseDataDirectory));
    setWebSQLDatabaseDirectory(WebsiteDataStore::defaultWebSQLDatabaseDirectory(m_baseDataDirectory));
    setLocalStorageDirectory(WebsiteDataStore::defaultLocalStorageDirectory(m_baseDataDirectory));
    setMediaKeysStorageDirectory(WebsiteDataStore::defaultMediaKeysStorageDirectory(m_baseDataDirectory));
    setResourceLoadStatisticsDirectory(WebsiteDataStore::defaultResourceLoadStatisticsDirectory(m_baseDataDirectory));
    setDeviceIdHashSaltsStorageDirectory(WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory(m_baseDataDirectory));
    setJavaScriptConfigurationDirectory(WebsiteDataStore::defaultJavaScriptConfigurationDirectory(m_baseDataDirectory));
#if PLATFORM(COCOA)
    setCookieStorageFile(WebsiteDataStore::defaultCookieStorageFile(m_baseDataDirectory));
#endif
}

Ref<WebsiteDataStoreConfiguration> WebsiteDataStoreConfiguration::copy() const
{
    auto copy = WebsiteDataStoreConfiguration::create(m_isPersistent, ShouldInitializePaths::No);

    copy->m_baseCacheDirectory = this->m_baseCacheDirectory;
    copy->m_baseDataDirectory = this->m_baseDataDirectory;
    copy->m_serviceWorkerProcessTerminationDelayEnabled = this->m_serviceWorkerProcessTerminationDelayEnabled;
    copy->m_fastServerTrustEvaluationEnabled = this->m_fastServerTrustEvaluationEnabled;
    copy->m_networkCacheSpeculativeValidationEnabled = this->m_networkCacheSpeculativeValidationEnabled;
    copy->m_staleWhileRevalidateEnabled = this->m_staleWhileRevalidateEnabled;
    copy->m_cacheStorageDirectory = this->m_cacheStorageDirectory;
    copy->m_generalStorageDirectory = this->m_generalStorageDirectory;
    copy->m_unifiedOriginStorageLevel = this->m_unifiedOriginStorageLevel;
    copy->m_perOriginStorageQuota = this->m_perOriginStorageQuota;
    copy->m_networkCacheDirectory = this->m_networkCacheDirectory;
    copy->m_applicationCacheDirectory = this->m_applicationCacheDirectory;
    copy->m_applicationCacheFlatFileSubdirectoryName = this->m_applicationCacheFlatFileSubdirectoryName;
    copy->m_mediaCacheDirectory = this->m_mediaCacheDirectory;
    copy->m_indexedDBDatabaseDirectory = this->m_indexedDBDatabaseDirectory;
    copy->m_serviceWorkerRegistrationDirectory = this->m_serviceWorkerRegistrationDirectory;
    copy->m_webSQLDatabaseDirectory = this->m_webSQLDatabaseDirectory;
    copy->m_hstsStorageDirectory = this->m_hstsStorageDirectory;
    copy->m_localStorageDirectory = this->m_localStorageDirectory;
    copy->m_mediaKeysStorageDirectory = this->m_mediaKeysStorageDirectory;
    copy->m_alternativeServicesDirectory = this->m_alternativeServicesDirectory;
    copy->m_deviceIdHashSaltsStorageDirectory = this->m_deviceIdHashSaltsStorageDirectory;
    copy->m_resourceLoadStatisticsDirectory = this->m_resourceLoadStatisticsDirectory;
    copy->m_javaScriptConfigurationDirectory = this->m_javaScriptConfigurationDirectory;
    copy->m_cookieStorageFile = this->m_cookieStorageFile;
    copy->m_sourceApplicationBundleIdentifier = this->m_sourceApplicationBundleIdentifier;
    copy->m_sourceApplicationSecondaryIdentifier = this->m_sourceApplicationSecondaryIdentifier;
    copy->m_httpProxy = this->m_httpProxy;
    copy->m_httpsProxy = this->m_httpsProxy;
    copy->m_deviceManagementRestrictionsEnabled = this->m_deviceManagementRestrictionsEnabled;
    copy->m_allLoadsBlockedByDeviceManagementRestrictionsForTesting = this->m_allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    copy->m_webPushDaemonUsesMockBundlesForTesting = this->m_webPushDaemonUsesMockBundlesForTesting;
    copy->m_boundInterfaceIdentifier = this->m_boundInterfaceIdentifier;
    copy->m_allowsCellularAccess = this->m_allowsCellularAccess;
    copy->m_legacyTLSEnabled = this->m_legacyTLSEnabled;
    copy->m_dataConnectionServiceType = this->m_dataConnectionServiceType;
    copy->m_testingSessionEnabled = this->m_testingSessionEnabled;
    copy->m_testSpeedMultiplier = this->m_testSpeedMultiplier;
    copy->m_suppressesConnectionTerminationOnSystemChange = this->m_suppressesConnectionTerminationOnSystemChange;
    copy->m_allowsServerPreconnect = this->m_allowsServerPreconnect;
    copy->m_requiresSecureHTTPSProxyConnection = this->m_requiresSecureHTTPSProxyConnection;
    copy->m_shouldRunServiceWorkersOnMainThreadForTesting = this->m_shouldRunServiceWorkersOnMainThreadForTesting;
    copy->m_overrideServiceWorkerRegistrationCountTestingValue = this->m_overrideServiceWorkerRegistrationCountTestingValue;
    copy->m_preventsSystemHTTPProxyAuthentication = this->m_preventsSystemHTTPProxyAuthentication;
    copy->m_standaloneApplicationURL = this->m_standaloneApplicationURL;
    copy->m_enableInAppBrowserPrivacyForTesting = this->m_enableInAppBrowserPrivacyForTesting;
    copy->m_allowsHSTSWithUntrustedRootCertificate = this->m_allowsHSTSWithUntrustedRootCertificate;
    copy->m_pcmMachServiceName = this->m_pcmMachServiceName;
    copy->m_webPushMachServiceName = this->m_webPushMachServiceName;
    copy->m_webPushPartitionString = this->m_webPushPartitionString;
    copy->m_trackingPreventionDebugModeEnabled = this->m_trackingPreventionDebugModeEnabled;
    copy->m_identifier = m_identifier;
#if PLATFORM(COCOA)
    if (m_proxyConfiguration)
        copy->m_proxyConfiguration = adoptCF(CFDictionaryCreateCopy(nullptr, this->m_proxyConfiguration.get()));
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    copy->m_modelElementCacheDirectory = this->m_modelElementCacheDirectory;
#endif
#if !HAVE(NSURLSESSION_WEBSOCKET)
    copy->m_shouldAcceptInsecureCertificatesForWebSockets = this->m_shouldAcceptInsecureCertificatesForWebSockets;
#endif

    return copy;
}

WebPushD::WebPushDaemonConnectionConfiguration WebsiteDataStoreConfiguration::webPushDaemonConnectionConfiguration() const
{
    return { m_webPushDaemonUsesMockBundlesForTesting, { }, m_webPushPartitionString, m_identifier };
}

} // namespace WebKit

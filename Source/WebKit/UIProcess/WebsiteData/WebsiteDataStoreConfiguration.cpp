/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "WebsiteDataStore.h"

namespace WebKit {

WebsiteDataStoreConfiguration::WebsiteDataStoreConfiguration(IsPersistent isPersistent)
    : m_isPersistent(isPersistent)
{
    if (isPersistent == IsPersistent::Yes) {
        setApplicationCacheDirectory(WebsiteDataStore::defaultApplicationCacheDirectory());
        setCacheStorageDirectory(WebsiteDataStore::defaultCacheStorageDirectory());
        setNetworkCacheDirectory(WebsiteDataStore::defaultNetworkCacheDirectory());
        setAlternativeServicesDirectory(WebsiteDataStore::defaultAlternativeServicesDirectory());
        setMediaCacheDirectory(WebsiteDataStore::defaultMediaCacheDirectory());
        setIndexedDBDatabaseDirectory(WebsiteDataStore::defaultIndexedDBDatabaseDirectory());
        setServiceWorkerRegistrationDirectory(WebsiteDataStore::defaultServiceWorkerRegistrationDirectory());
        setWebSQLDatabaseDirectory(WebsiteDataStore::defaultWebSQLDatabaseDirectory());
        setLocalStorageDirectory(WebsiteDataStore::defaultLocalStorageDirectory());
        setMediaKeysStorageDirectory(WebsiteDataStore::defaultMediaKeysStorageDirectory());
        setResourceLoadStatisticsDirectory(WebsiteDataStore::defaultResourceLoadStatisticsDirectory());
        setDeviceIdHashSaltsStorageDirectory(WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory());
        setJavaScriptConfigurationDirectory(WebsiteDataStore::defaultJavaScriptConfigurationDirectory());
    }
}

Ref<WebsiteDataStoreConfiguration> WebsiteDataStoreConfiguration::copy() const
{
    auto copy = WebsiteDataStoreConfiguration::create(m_isPersistent);

    copy->m_serviceWorkerProcessTerminationDelayEnabled = this->m_serviceWorkerProcessTerminationDelayEnabled;
    copy->m_fastServerTrustEvaluationEnabled = this->m_fastServerTrustEvaluationEnabled;
    copy->m_networkCacheSpeculativeValidationEnabled = this->m_networkCacheSpeculativeValidationEnabled;
    copy->m_staleWhileRevalidateEnabled = this->m_staleWhileRevalidateEnabled;
    copy->m_cacheStorageDirectory = this->m_cacheStorageDirectory;
    copy->m_perOriginStorageQuota = this->m_perOriginStorageQuota;
    copy->m_networkCacheDirectory = this->m_networkCacheDirectory;
    copy->m_applicationCacheDirectory = this->m_applicationCacheDirectory;
    copy->m_applicationCacheFlatFileSubdirectoryName = this->m_applicationCacheFlatFileSubdirectoryName;
    copy->m_mediaCacheDirectory = this->m_mediaCacheDirectory;
    copy->m_indexedDBDatabaseDirectory = this->m_indexedDBDatabaseDirectory;
    copy->m_serviceWorkerRegistrationDirectory = this->m_serviceWorkerRegistrationDirectory;
    copy->m_webSQLDatabaseDirectory = this->m_webSQLDatabaseDirectory;
#if USE(GLIB)
    copy->m_hstsStorageDirectory = this->m_hstsStorageDirectory;
#endif
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
    copy->m_boundInterfaceIdentifier = this->m_boundInterfaceIdentifier;
    copy->m_allowsCellularAccess = this->m_allowsCellularAccess;
    copy->m_legacyTLSEnabled = this->m_legacyTLSEnabled;
    copy->m_dataConnectionServiceType = this->m_dataConnectionServiceType;
    copy->m_testingSessionEnabled = this->m_testingSessionEnabled;
    copy->m_testSpeedMultiplier = this->m_testSpeedMultiplier;
    copy->m_suppressesConnectionTerminationOnSystemChange = this->m_suppressesConnectionTerminationOnSystemChange;
    copy->m_allowsServerPreconnect = this->m_allowsServerPreconnect;
    copy->m_requiresSecureHTTPSProxyConnection = this->m_requiresSecureHTTPSProxyConnection;
    copy->m_preventsSystemHTTPProxyAuthentication = this->m_preventsSystemHTTPProxyAuthentication;
    copy->m_standaloneApplicationURL = this->m_standaloneApplicationURL;
#if PLATFORM(COCOA)
    if (m_proxyConfiguration)
        copy->m_proxyConfiguration = adoptCF(CFDictionaryCreateCopy(nullptr, this->m_proxyConfiguration.get()));
#endif

    return copy;
}

} // namespace WebKit

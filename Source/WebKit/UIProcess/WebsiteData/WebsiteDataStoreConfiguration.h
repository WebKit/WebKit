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

#pragma once

#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebsiteDataStoreConfiguration : public API::ObjectImpl<API::Object::Type::WebsiteDataStoreConfiguration> {
public:
    static Ref<WebsiteDataStoreConfiguration> create() { return adoptRef(*new WebsiteDataStoreConfiguration); }

    Ref<WebsiteDataStoreConfiguration> copy();

    uint64_t cacheStoragePerOriginQuota() { return m_cacheStoragePerOriginQuota; }
    void setCacheStoragePerOriginQuota(uint64_t quota) { m_cacheStoragePerOriginQuota = quota; }

    const String& applicationCacheDirectory() const { return m_applicationCacheDirectory; }
    void setApplicationCacheDirectory(String&& directory) { m_applicationCacheDirectory = WTFMove(directory); }
    
    const String& mediaCacheDirectory() const { return m_mediaCacheDirectory; }
    void setMediaCacheDirectory(String&& directory) { m_mediaCacheDirectory = WTFMove(directory); }
    
    const String& mediaKeysStorageDirectory() const { return m_mediaKeysStorageDirectory; }
    void setMediaKeysStorageDirectory(String&& directory) { m_mediaKeysStorageDirectory = WTFMove(directory); }
    
    const String& javaScriptConfigurationDirectory() const { return m_javaScriptConfigurationDirectory; }
    void setJavaScriptConfigurationDirectory(String&& directory) { m_javaScriptConfigurationDirectory = WTFMove(directory); }
    
    const String& webStorageDirectory() const { return m_webStorageDirectory; }
    void setWebStorageDirectory(String&& directory) { m_webStorageDirectory = WTFMove(directory); }

    const String& indexedDBDatabaseDirectory() const { return m_indexedDBDatabaseDirectory; }
    void setIndexedDBDatabaseDirectory(String&& directory) { m_indexedDBDatabaseDirectory = WTFMove(directory); }

    const String& webSQLDatabaseDirectory() const { return m_webSQLDatabaseDirectory; }
    void setWebSQLDatabaseDirectory(String&& directory) { m_webSQLDatabaseDirectory = WTFMove(directory); }

    const String& localStorageDirectory() const { return m_localStorageDirectory; }
    void setLocalStorageDirectory(String&& directory) { m_localStorageDirectory = WTFMove(directory); }
    
    const String& cookieStorageFile() const { return m_cookieStorageFile; }
    void setCookieStorageFile(String&& directory) { m_cookieStorageFile = WTFMove(directory); }
    
    const String& resourceLoadStatisticsDirectory() const { return m_resourceLoadStatisticsDirectory; }
    void setResourceLoadStatisticsDirectory(String&& directory) { m_resourceLoadStatisticsDirectory = WTFMove(directory); }

    const String& networkCacheDirectory() const { return m_networkCacheDirectory; }
    void setNetworkCacheDirectory(String&& directory) { m_networkCacheDirectory = WTFMove(directory); }
    
    const String& cacheStorageDirectory() const { return m_cacheStorageDirectory; }
    void setCacheStorageDirectory(String&& directory) { m_cacheStorageDirectory = WTFMove(directory); }
    
    const String& applicationCacheFlatFileSubdirectoryName() const { return m_applicationCacheFlatFileSubdirectoryName; }
    void setApplicationCacheFlatFileSubdirectoryName(String&& directory) { m_applicationCacheFlatFileSubdirectoryName = WTFMove(directory); }
    
    const String& serviceWorkerRegistrationDirectory() const { return m_serviceWorkerRegistrationDirectory; }
    void setServiceWorkerRegistrationDirectory(String&& directory) { m_serviceWorkerRegistrationDirectory = WTFMove(directory); }
    
    const String& sourceApplicationBundleIdentifier() const { return m_sourceApplicationBundleIdentifier; }
    void setSourceApplicationBundleIdentifier(String&& identifier) { m_sourceApplicationBundleIdentifier = WTFMove(identifier); }

    const String& sourceApplicationSecondaryIdentifier() const { return m_sourceApplicationSecondaryIdentifier; }
    void setSourceApplicationSecondaryIdentifier(String&& identifier) { m_sourceApplicationSecondaryIdentifier = WTFMove(identifier); }

    constexpr static uint64_t defaultCacheStoragePerOriginQuota = 50 * 1024 * 1024;

private:
    WebsiteDataStoreConfiguration();

    String m_cacheStorageDirectory;
    uint64_t m_cacheStoragePerOriginQuota { defaultCacheStoragePerOriginQuota };
    String m_networkCacheDirectory;
    String m_applicationCacheDirectory;
    String m_applicationCacheFlatFileSubdirectoryName;
    String m_webStorageDirectory;
    String m_mediaCacheDirectory;
    String m_indexedDBDatabaseDirectory;
    String m_serviceWorkerRegistrationDirectory;
    String m_webSQLDatabaseDirectory;
    String m_localStorageDirectory;
    String m_mediaKeysStorageDirectory;
    String m_deviceIdHashSaltsStorageDirectory;
    String m_resourceLoadStatisticsDirectory;
    String m_javaScriptConfigurationDirectory;
    String m_cookieStorageFile;
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
};

}

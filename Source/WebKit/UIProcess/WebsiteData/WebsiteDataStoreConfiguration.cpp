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

namespace WebKit {

WebsiteDataStoreConfiguration::WebsiteDataStoreConfiguration()
    : m_resourceLoadStatisticsDirectory(API::WebsiteDataStore::defaultResourceLoadStatisticsDirectory())
{
}

Ref<WebsiteDataStoreConfiguration> WebsiteDataStoreConfiguration::copy()
{
    auto copy = WebsiteDataStoreConfiguration::create();

    copy->m_cacheStorageDirectory = this->m_cacheStorageDirectory;
    copy->m_cacheStoragePerOriginQuota = this->m_cacheStoragePerOriginQuota;
    copy->m_networkCacheDirectory = this->m_networkCacheDirectory;
    copy->m_applicationCacheDirectory = this->m_applicationCacheDirectory;
    copy->m_applicationCacheFlatFileSubdirectoryName = this->m_applicationCacheFlatFileSubdirectoryName;
    copy->m_webStorageDirectory = this->m_webStorageDirectory;
    copy->m_mediaCacheDirectory = this->m_mediaCacheDirectory;
    copy->m_indexedDBDatabaseDirectory = this->m_indexedDBDatabaseDirectory;
    copy->m_serviceWorkerRegistrationDirectory = this->m_serviceWorkerRegistrationDirectory;
    copy->m_webSQLDatabaseDirectory = this->m_webSQLDatabaseDirectory;
    copy->m_localStorageDirectory = this->m_localStorageDirectory;
    copy->m_mediaKeysStorageDirectory = this->m_mediaKeysStorageDirectory;
    copy->m_deviceIdHashSaltsStorageDirectory = this->m_deviceIdHashSaltsStorageDirectory;
    copy->m_resourceLoadStatisticsDirectory = this->m_resourceLoadStatisticsDirectory;
    copy->m_javaScriptConfigurationDirectory = this->m_javaScriptConfigurationDirectory;
    copy->m_cookieStorageFile = this->m_cookieStorageFile;
    copy->m_sourceApplicationBundleIdentifier = this->m_sourceApplicationBundleIdentifier;
    copy->m_sourceApplicationSecondaryIdentifier = this->m_sourceApplicationSecondaryIdentifier;

    return copy;
}

} // namespace WebKit

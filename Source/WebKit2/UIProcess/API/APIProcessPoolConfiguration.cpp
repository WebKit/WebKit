/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "APIProcessPoolConfiguration.h"

#include "APIWebsiteDataStore.h"
#include "WebProcessPool.h"

namespace API {

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::create()
{
    return adoptRef(*new ProcessPoolConfiguration);
}

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::createWithLegacyOptions()
{
    auto configuration = ProcessPoolConfiguration::create();

    configuration->m_shouldHaveLegacyDataStore = true;
    configuration->m_maximumProcessCount = 1;
    configuration->m_cacheModel = WebKit::CacheModelDocumentViewer;

    configuration->m_applicationCacheDirectory = WebKit::WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory();
    configuration->m_applicationCacheFlatFileSubdirectoryName = "ApplicationCache";
    configuration->m_diskCacheDirectory = WebKit::WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory();
    configuration->m_mediaCacheDirectory = WebKit::WebProcessPool::legacyPlatformDefaultMediaCacheDirectory();
    configuration->m_indexedDBDatabaseDirectory = WebKit::WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory();
    configuration->m_localStorageDirectory = WebKit::WebProcessPool::legacyPlatformDefaultLocalStorageDirectory();
    configuration->m_mediaKeysStorageDirectory = WebKit::WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory();
    configuration->m_webSQLDatabaseDirectory = WebKit::WebProcessPool::legacyPlatformDefaultWebSQLDatabaseDirectory();

    return configuration;
}

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::createWithWebsiteDataStoreConfiguration(const WebKit::WebsiteDataStore::Configuration& legacyConfiguration)
{
    auto configuration = ProcessPoolConfiguration::create();

    configuration->m_applicationCacheDirectory = legacyConfiguration.applicationCacheDirectory;
    configuration->m_applicationCacheFlatFileSubdirectoryName = legacyConfiguration.applicationCacheFlatFileSubdirectoryName;
    configuration->m_diskCacheDirectory = legacyConfiguration.networkCacheDirectory;
    configuration->m_mediaCacheDirectory = legacyConfiguration.mediaCacheDirectory;
    configuration->m_indexedDBDatabaseDirectory = WebKit::WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory();
    configuration->m_localStorageDirectory = legacyConfiguration.localStorageDirectory;
    configuration->m_mediaKeysStorageDirectory = legacyConfiguration.mediaKeysStorageDirectory;
    configuration->m_webSQLDatabaseDirectory = legacyConfiguration.webSQLDatabaseDirectory;

    return configuration;
}

ProcessPoolConfiguration::ProcessPoolConfiguration()
    : m_applicationCacheDirectory(WebsiteDataStore::defaultApplicationCacheDirectory())
    , m_applicationCacheFlatFileSubdirectoryName("Files")
    , m_diskCacheDirectory(WebsiteDataStore::defaultNetworkCacheDirectory())
    , m_mediaCacheDirectory(WebsiteDataStore::defaultMediaCacheDirectory())
    , m_indexedDBDatabaseDirectory(WebsiteDataStore::defaultIndexedDBDatabaseDirectory())
    , m_localStorageDirectory(WebsiteDataStore::defaultLocalStorageDirectory())
    , m_webSQLDatabaseDirectory(WebsiteDataStore::defaultWebSQLDatabaseDirectory())
    , m_mediaKeysStorageDirectory(WebsiteDataStore::defaultMediaKeysStorageDirectory())
{
}

ProcessPoolConfiguration::~ProcessPoolConfiguration()
{
}

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::copy()
{
    auto copy = this->create();

    copy->m_shouldHaveLegacyDataStore = this->m_shouldHaveLegacyDataStore;
    copy->m_maximumProcessCount = this->m_maximumProcessCount;
    copy->m_cacheModel = this->m_cacheModel;
    copy->m_diskCacheSpeculativeValidationEnabled = this->m_diskCacheSpeculativeValidationEnabled;
    copy->m_unresponsiveBackgroundProcessesTerminationEnabled = this->m_unresponsiveBackgroundProcessesTerminationEnabled;
    copy->m_diskCacheSizeOverride = this->m_diskCacheSizeOverride;
    copy->m_applicationCacheDirectory = this->m_applicationCacheDirectory;
    copy->m_applicationCacheFlatFileSubdirectoryName = this->m_applicationCacheFlatFileSubdirectoryName;
    copy->m_diskCacheDirectory = this->m_diskCacheDirectory;
    copy->m_mediaCacheDirectory = this->m_mediaCacheDirectory;
    copy->m_indexedDBDatabaseDirectory = this->m_indexedDBDatabaseDirectory;
    copy->m_injectedBundlePath = this->m_injectedBundlePath;
    copy->m_localStorageDirectory = this->m_localStorageDirectory;
    copy->m_mediaKeysStorageDirectory = this->m_mediaKeysStorageDirectory;
    copy->m_webSQLDatabaseDirectory = this->m_webSQLDatabaseDirectory;
    copy->m_cachePartitionedURLSchemes = this->m_cachePartitionedURLSchemes;
    copy->m_alwaysRevalidatedURLSchemes = this->m_alwaysRevalidatedURLSchemes;
    copy->m_fullySynchronousModeIsAllowedForTesting = this->m_fullySynchronousModeIsAllowedForTesting;
    copy->m_ignoreSynchronousMessagingTimeoutsForTesting = this->m_ignoreSynchronousMessagingTimeoutsForTesting;
    copy->m_overrideLanguages = this->m_overrideLanguages;
    copy->m_sourceApplicationBundleIdentifier = this->m_sourceApplicationBundleIdentifier;
    copy->m_sourceApplicationSecondaryIdentifier = this->m_sourceApplicationSecondaryIdentifier;
    copy->m_alwaysRunsAtBackgroundPriority = this->m_alwaysRunsAtBackgroundPriority;
#if PLATFORM(IOS)
    copy->m_ctDataConnectionServiceType = this->m_ctDataConnectionServiceType;
#endif
    
    return copy;
}

} // namespace API

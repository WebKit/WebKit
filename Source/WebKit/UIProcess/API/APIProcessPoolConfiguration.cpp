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
    auto configuration = ProcessPoolConfiguration::createWithWebsiteDataStoreConfiguration(WebsiteDataStore::legacyDefaultDataStoreConfiguration());

    configuration->m_shouldHaveLegacyDataStore = true;
    configuration->m_cacheModel = WebKit::CacheModel::DocumentViewer;

    return configuration;
}

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::createWithWebsiteDataStoreConfiguration(const WebKit::WebsiteDataStoreConfiguration& legacyConfiguration)
{
    auto configuration = ProcessPoolConfiguration::create();

    configuration->m_applicationCacheDirectory = legacyConfiguration.applicationCacheDirectory();
    configuration->m_applicationCacheFlatFileSubdirectoryName = legacyConfiguration.applicationCacheFlatFileSubdirectoryName();
    configuration->m_diskCacheDirectory = legacyConfiguration.networkCacheDirectory();
    configuration->m_mediaCacheDirectory = legacyConfiguration.mediaCacheDirectory();
    configuration->m_indexedDBDatabaseDirectory = WebsiteDataStore::legacyDefaultIndexedDBDatabaseDirectory();
    configuration->m_localStorageDirectory = legacyConfiguration.localStorageDirectory();
    configuration->m_deviceIdHashSaltsStorageDirectory = legacyConfiguration.deviceIdHashSaltsStorageDirectory();
    configuration->m_mediaKeysStorageDirectory = legacyConfiguration.mediaKeysStorageDirectory();
    configuration->m_resourceLoadStatisticsDirectory = legacyConfiguration.resourceLoadStatisticsDirectory();
    configuration->m_webSQLDatabaseDirectory = legacyConfiguration.webSQLDatabaseDirectory();
    configuration->m_javaScriptConfigurationDirectory = legacyConfiguration.javaScriptConfigurationDirectory();

    return configuration;
}

ProcessPoolConfiguration::ProcessPoolConfiguration()
    : m_applicationCacheDirectory(WebsiteDataStore::defaultApplicationCacheDirectory())
    , m_applicationCacheFlatFileSubdirectoryName("Files")
    , m_diskCacheDirectory(WebsiteDataStore::defaultNetworkCacheDirectory())
    , m_mediaCacheDirectory(WebsiteDataStore::defaultMediaCacheDirectory())
    , m_indexedDBDatabaseDirectory(WebsiteDataStore::defaultIndexedDBDatabaseDirectory())
    , m_localStorageDirectory(WebsiteDataStore::defaultLocalStorageDirectory())
    , m_deviceIdHashSaltsStorageDirectory(WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory())
    , m_webSQLDatabaseDirectory(WebsiteDataStore::defaultWebSQLDatabaseDirectory())
    , m_mediaKeysStorageDirectory(WebsiteDataStore::defaultMediaKeysStorageDirectory())
    , m_resourceLoadStatisticsDirectory(WebsiteDataStore::defaultResourceLoadStatisticsDirectory())
    , m_javaScriptConfigurationDirectory(WebsiteDataStore::defaultJavaScriptConfigurationDirectory())
{
}

ProcessPoolConfiguration::~ProcessPoolConfiguration()
{
}

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::copy()
{
    auto copy = this->create();

    copy->m_shouldHaveLegacyDataStore = this->m_shouldHaveLegacyDataStore;
    copy->m_cacheModel = this->m_cacheModel;
    copy->m_diskCacheDirectory = this->m_diskCacheDirectory;
    copy->m_diskCacheSpeculativeValidationEnabled = this->m_diskCacheSpeculativeValidationEnabled;
    copy->m_applicationCacheDirectory = this->m_applicationCacheDirectory;
    copy->m_applicationCacheFlatFileSubdirectoryName = this->m_applicationCacheFlatFileSubdirectoryName;
    copy->m_mediaCacheDirectory = this->m_mediaCacheDirectory;
    copy->m_indexedDBDatabaseDirectory = this->m_indexedDBDatabaseDirectory;
    copy->m_injectedBundlePath = this->m_injectedBundlePath;
    copy->m_localStorageDirectory = this->m_localStorageDirectory;
    copy->m_deviceIdHashSaltsStorageDirectory = this->m_deviceIdHashSaltsStorageDirectory;
    copy->m_mediaKeysStorageDirectory = this->m_mediaKeysStorageDirectory;
    copy->m_resourceLoadStatisticsDirectory = this->m_resourceLoadStatisticsDirectory;
    copy->m_javaScriptConfigurationDirectory = this->m_javaScriptConfigurationDirectory;
    copy->m_webSQLDatabaseDirectory = this->m_webSQLDatabaseDirectory;
    copy->m_cachePartitionedURLSchemes = this->m_cachePartitionedURLSchemes;
    copy->m_alwaysRevalidatedURLSchemes = this->m_alwaysRevalidatedURLSchemes;
    copy->m_additionalReadAccessAllowedPaths = this->m_additionalReadAccessAllowedPaths;
    copy->m_fullySynchronousModeIsAllowedForTesting = this->m_fullySynchronousModeIsAllowedForTesting;
    copy->m_ignoreSynchronousMessagingTimeoutsForTesting = this->m_ignoreSynchronousMessagingTimeoutsForTesting;
    copy->m_attrStyleEnabled = this->m_attrStyleEnabled;
    copy->m_overrideLanguages = this->m_overrideLanguages;
    copy->m_alwaysRunsAtBackgroundPriority = this->m_alwaysRunsAtBackgroundPriority;
    copy->m_shouldTakeUIBackgroundAssertion = this->m_shouldTakeUIBackgroundAssertion;
    copy->m_shouldCaptureAudioInUIProcess = this->m_shouldCaptureAudioInUIProcess;
    copy->m_shouldCaptureDisplayInUIProcess = this->m_shouldCaptureDisplayInUIProcess;
    copy->m_isJITEnabled = this->m_isJITEnabled;
#if PLATFORM(IOS_FAMILY)
    copy->m_ctDataConnectionServiceType = this->m_ctDataConnectionServiceType;
#endif
    copy->m_presentingApplicationPID = this->m_presentingApplicationPID;
    copy->m_processSwapsOnNavigationFromClient = this->m_processSwapsOnNavigationFromClient;
    copy->m_processSwapsOnNavigationFromExperimentalFeatures = this->m_processSwapsOnNavigationFromExperimentalFeatures;
    copy->m_alwaysKeepAndReuseSwappedProcesses = this->m_alwaysKeepAndReuseSwappedProcesses;
    copy->m_processSwapsOnWindowOpenWithOpener = this->m_processSwapsOnWindowOpenWithOpener;
    copy->m_isAutomaticProcessWarmingEnabledByClient = this->m_isAutomaticProcessWarmingEnabledByClient;
#if ENABLE(PROXIMITY_NETWORKING)
    copy->m_wirelessContextIdentifier = this->m_wirelessContextIdentifier;
#endif
#if PLATFORM(COCOA)
    copy->m_suppressesConnectionTerminationOnSystemChange = this->m_suppressesConnectionTerminationOnSystemChange;
#endif
    copy->m_customWebContentServiceBundleIdentifier = this->m_customWebContentServiceBundleIdentifier;
    copy->m_usesSingleWebProcess = m_usesSingleWebProcess;

    return copy;
}

} // namespace API

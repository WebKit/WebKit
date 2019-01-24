/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "APIWebsiteDataStore.h"

#include <wtf/FileSystem.h>

namespace API {

String WebsiteDataStore::defaultApplicationCacheDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "ApplicationCache");
}

String WebsiteDataStore::defaultCacheStorageDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "CacheStorage");
}

String WebsiteDataStore::defaultNetworkCacheDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "NetworkCache");
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "IndexedDB");
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "ServiceWorkers");
}

String WebsiteDataStore::defaultLocalStorageDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "LocalStorage");
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "MediaKeyStorage");
}

String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory()
{
    // Not Implemented.
    return String();
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "WebSQL");
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "ResourceLoadStatistics");
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName)
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), directoryName);
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName)
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), directoryName);
}

String WebsiteDataStore::legacyDefaultApplicationCacheDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "ApplicationCache");
}

String WebsiteDataStore::legacyDefaultNetworkCacheDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "NetworkCache");
}

String WebsiteDataStore::legacyDefaultWebSQLDatabaseDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "WebSQL");
}

String WebsiteDataStore::legacyDefaultIndexedDBDatabaseDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "IndexedDB");
}

String WebsiteDataStore::legacyDefaultLocalStorageDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "LocalStorage");
}

String WebsiteDataStore::legacyDefaultMediaCacheDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "MediaCache");
}

String WebsiteDataStore::legacyDefaultMediaKeysStorageDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "MediaKeyStorage");
}

String WebsiteDataStore::legacyDefaultDeviceIdHashSaltsStorageDirectory()
{
    // Not Implemented.
    return String();
}

String WebsiteDataStore::legacyDefaultJavaScriptConfigurationDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "JavaScriptCoreDebug");
}

Ref<WebKit::WebsiteDataStoreConfiguration> WebsiteDataStore::defaultDataStoreConfiguration()
{
    auto configuration = WebKit::WebsiteDataStoreConfiguration::create();

    configuration->setApplicationCacheDirectory(defaultApplicationCacheDirectory());
    configuration->setNetworkCacheDirectory(defaultNetworkCacheDirectory());
    configuration->setWebSQLDatabaseDirectory(defaultWebSQLDatabaseDirectory());
    configuration->setLocalStorageDirectory(defaultLocalStorageDirectory());
    configuration->setMediaKeysStorageDirectory(defaultMediaKeysStorageDirectory());
    configuration->setResourceLoadStatisticsDirectory(defaultResourceLoadStatisticsDirectory());

    return configuration;
}

} // namespace API

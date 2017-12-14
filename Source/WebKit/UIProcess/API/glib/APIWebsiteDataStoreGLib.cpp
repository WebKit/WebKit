/*
 * Copyright (C) 2015-2017 Igalia S.L.
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

#include <WebCore/FileSystem.h>

#if PLATFORM(GTK)
#define BASE_DIRECTORY "webkitgtk"
#elif PLATFORM(WPE)
#define BASE_DIRECTORY "wpe"
#endif

namespace API {

String WebsiteDataStore::defaultApplicationCacheDirectory()
{
    return cacheDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "applications");
}

// FIXME: The other directories in this file are shared between all applications using WebKitGTK+.
// Why is only this directory namespaced to a particular application?
String WebsiteDataStore::defaultNetworkCacheDirectory()
{
    return cacheDirectoryFileSystemRepresentation(WebCore::FileSystem::pathByAppendingComponent(WebCore::FileSystem::stringFromFileSystemRepresentation(g_get_prgname()), "WebKitCache"));
}

String WebsiteDataStore::defaultCacheStorageDirectory()
{
    return cacheDirectoryFileSystemRepresentation(WebCore::FileSystem::pathByAppendingComponent(WebCore::FileSystem::stringFromFileSystemRepresentation(g_get_prgname()), "CacheStorage"));
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "databases" G_DIR_SEPARATOR_S "indexeddb");
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory()
{
    // FIXME: Implement
    return { };
}

String WebsiteDataStore::defaultLocalStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "localstorage");
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "mediakeys");
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "databases");
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "ResourceLoadStatistics");
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName)
{
    return WebCore::FileSystem::pathByAppendingComponent(WebCore::FileSystem::stringFromFileSystemRepresentation(g_get_user_cache_dir()), directoryName);
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName)
{
    return WebCore::FileSystem::pathByAppendingComponent(WebCore::FileSystem::stringFromFileSystemRepresentation(g_get_user_data_dir()), directoryName);
}

WebKit::WebsiteDataStore::Configuration WebsiteDataStore::defaultDataStoreConfiguration()
{
    WebKit::WebsiteDataStore::Configuration configuration;

    configuration.applicationCacheDirectory = defaultApplicationCacheDirectory();
    configuration.networkCacheDirectory = defaultNetworkCacheDirectory();

    configuration.indexedDBDatabaseDirectory = defaultIndexedDBDatabaseDirectory();
    configuration.serviceWorkerRegistrationDirectory = defaultServiceWorkerRegistrationDirectory();
    configuration.webSQLDatabaseDirectory = defaultWebSQLDatabaseDirectory();
    configuration.localStorageDirectory = defaultLocalStorageDirectory();
    configuration.mediaKeysStorageDirectory = defaultMediaKeysStorageDirectory();
    configuration.resourceLoadStatisticsDirectory = defaultResourceLoadStatisticsDirectory();

    return configuration;
}

} // namespace API

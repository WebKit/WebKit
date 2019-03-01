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

#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
#define BASE_DIRECTORY "webkitgtk"
#elif PLATFORM(WPE)
#define BASE_DIRECTORY "wpe"
#endif

namespace API {

WTF::String WebsiteDataStore::defaultApplicationCacheDirectory()
{
    return cacheDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "applications");
}

// FIXME: The other directories in this file are shared between all applications using WebKitGTK+.
// Why is only this directory namespaced to a particular application?
WTF::String WebsiteDataStore::defaultNetworkCacheDirectory()
{
    return cacheDirectoryFileSystemRepresentation(FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_prgname()), "WebKitCache"));
}

WTF::String WebsiteDataStore::defaultCacheStorageDirectory()
{
    return cacheDirectoryFileSystemRepresentation(FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_prgname()), "CacheStorage"));
}

WTF::String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "databases" G_DIR_SEPARATOR_S "indexeddb");
}

WTF::String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "serviceworkers");
}

WTF::String WebsiteDataStore::defaultLocalStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "localstorage");
}

WTF::String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "mediakeys");
}

String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "deviceidhashsalts");
}

WTF::String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "databases");
}

WTF::String WebsiteDataStore::defaultResourceLoadStatisticsDirectory()
{
    return websiteDataDirectoryFileSystemRepresentation(BASE_DIRECTORY G_DIR_SEPARATOR_S "ResourceLoadStatistics");
}

WTF::String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const WTF::String& directoryName)
{
    return FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_user_cache_dir()), directoryName);
}

WTF::String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const WTF::String& directoryName)
{
    return FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_user_data_dir()), directoryName);
}

WTF::String WebsiteDataStore::legacyDefaultApplicationCacheDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "appcache", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(cacheDirectory.get());
#endif
    return defaultApplicationCacheDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultNetworkCacheDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<char> diskCacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "cache", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(diskCacheDirectory.get());
#endif
    return defaultNetworkCacheDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultWebSQLDatabaseDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> databaseDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "databases", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(databaseDirectory.get());
#endif
    return defaultWebSQLDatabaseDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultIndexedDBDatabaseDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> indexedDBDatabaseDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "databases", "indexeddb", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(indexedDBDatabaseDirectory.get());
#endif
    return defaultIndexedDBDatabaseDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultLocalStorageDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> storageDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "localstorage", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(storageDirectory.get());
#endif
    return defaultLocalStorageDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultMediaCacheDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "mediacache", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(cacheDirectory.get());
#endif
    return defaultMediaCacheDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultMediaKeysStorageDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> mediaKeysStorageDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "mediakeys", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(mediaKeysStorageDirectory.get());
#endif
    return defaultMediaKeysStorageDirectory();
}

String WebsiteDataStore::legacyDefaultDeviceIdHashSaltsStorageDirectory()
{
#if PLATFORM(WPE)
    GUniquePtr<gchar> deviceIdHashSaltsStorageDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "deviceidhashsalts", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(deviceIdHashSaltsStorageDirectory.get());
#endif
    return defaultDeviceIdHashSaltsStorageDirectory();
}

WTF::String WebsiteDataStore::legacyDefaultJavaScriptConfigurationDirectory()
{
    GUniquePtr<gchar> javaScriptCoreConfigDirectory(g_build_filename(g_get_user_data_dir(), BASE_DIRECTORY, "JavaScriptCoreDebug", nullptr));
    return FileSystem::stringFromFileSystemRepresentation(javaScriptCoreConfigDirectory.get());
}

} // namespace API

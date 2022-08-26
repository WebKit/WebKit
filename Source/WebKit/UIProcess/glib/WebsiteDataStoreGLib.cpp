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
#include "WebsiteDataStore.h"

#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

String WebsiteDataStore::defaultApplicationCacheDirectory(const String& baseCacheDirectory)
{
    return cacheDirectoryFileSystemRepresentation("applications"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultNetworkCacheDirectory(const String& baseCacheDirectory)
{
    return cacheDirectoryFileSystemRepresentation("WebKitCache"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultCacheStorageDirectory(const String& baseCacheDirectory)
{
    return cacheDirectoryFileSystemRepresentation("CacheStorage"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultHSTSStorageDirectory(const String& baseCacheDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation(""_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultGeneralStorageDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("storage"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation(String::fromUTF8("databases" G_DIR_SEPARATOR_S "indexeddb"), baseDataDirectory);
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("serviceworkers"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultLocalStorageDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("localstorage"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("mediakeys"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("deviceidhashsalts"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("databases"_s, baseDataDirectory);
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory(const String& baseDataDirectory)
{
    return websiteDataDirectoryFileSystemRepresentation("itp"_s, baseDataDirectory);
}

static String programName()
{
    if (auto* prgname = g_get_prgname())
        return String::fromUTF8(prgname);

#if PLATFORM(GTK)
    return "webkitgtk"_s;
#elif PLATFORM(WPE)
    return "wpe"_s;
#else
    return "WebKit"_s;
#endif
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName, const String& baseCacheDirectory, ShouldCreateDirectory)
{
    if (!baseCacheDirectory.isNull())
        return FileSystem::pathByAppendingComponent(baseCacheDirectory, directoryName);
    return FileSystem::pathByAppendingComponents(FileSystem::userCacheDirectory(), { programName(), directoryName });
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName, const String& baseDataDirectory, ShouldCreateDirectory)
{
    if (!baseDataDirectory.isNull())
        return FileSystem::pathByAppendingComponent(baseDataDirectory, directoryName);
    return FileSystem::pathByAppendingComponents(FileSystem::userDataDirectory(), { programName(), directoryName });
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime)
{
    notImplemented();
}

} // namespace API

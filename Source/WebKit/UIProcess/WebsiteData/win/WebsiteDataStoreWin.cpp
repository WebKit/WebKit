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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebsiteDataStore.h"

#include <WebCore/NotImplemented.h>
#include <wtf/FileSystem.h>

namespace WebKit {

void WebsiteDataStore::platformInitialize()
{
    notImplemented();
}

void WebsiteDataStore::platformDestroy()
{
    notImplemented();
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime)
{
    notImplemented();
}

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

String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "WebSQL");
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory()
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "ResourceLoadStatistics");
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName, ShouldCreateDirectory)
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), directoryName);
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName)
{
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), directoryName);
}

} // namespace WebKit

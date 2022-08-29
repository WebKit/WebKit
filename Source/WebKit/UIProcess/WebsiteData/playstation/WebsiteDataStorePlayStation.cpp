/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

String WebsiteDataStore::defaultApplicationCacheDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultCacheStorageDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultGeneralStorageDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultNetworkCacheDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultLocalStorageDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory(const String&)
{
    return { };
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String&, const String&, ShouldCreateDirectory)
{
    return { };
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String&, const String&, ShouldCreateDirectory)
{
    return { };
}

} // namespace WebKit

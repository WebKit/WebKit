/*
 * Copyright (C) 2015 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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

#include <Efreet.h>
#include <wtf/text/StringBuilder.h>

namespace API {

String WebsiteDataStore::defaultApplicationCacheDirectory()
{
    return String::fromUTF8(efreet_cache_home_get()) + "/WebKitEfl/Applications";
}

String WebsiteDataStore::defaultNetworkCacheDirectory()
{
#if ENABLE(NETWORK_CACHE)
    static const char networkCacheSubdirectory[] = "WebKitCache";
#else
    static const char networkCacheSubdirectory[] = "webkit";
#endif

    StringBuilder diskCacheDirectory;
    diskCacheDirectory.append(efreet_cache_home_get());
    diskCacheDirectory.appendLiteral("/");
    diskCacheDirectory.append(networkCacheSubdirectory);

    return diskCacheDirectory.toString();
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/IndexedDB";
}

String WebsiteDataStore::defaultLocalStorageDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/LocalStorage";
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/MediaKeys";
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/Databases";
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName)
{
    return String::fromUTF8(efreet_cache_home_get()) + directoryName;
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName)
{
    return String::fromUTF8(efreet_data_home_get()) + directoryName;
}

WebKit::WebsiteDataStore::Configuration WebsiteDataStore::defaultDataStoreConfiguration()
{
    WebKit::WebsiteDataStore::Configuration configuration;

    configuration.applicationCacheDirectory = defaultApplicationCacheDirectory();
    configuration.networkCacheDirectory = defaultNetworkCacheDirectory();

    configuration.webSQLDatabaseDirectory = defaultWebSQLDatabaseDirectory();
    configuration.localStorageDirectory = defaultLocalStorageDirectory();
    configuration.mediaKeysStorageDirectory = defaultMediaKeysStorageDirectory();

    return configuration;
}

} // namespace API

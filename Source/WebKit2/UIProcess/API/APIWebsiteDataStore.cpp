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
#include "APIWebsiteDataStore.h"

#include "WebsiteDataStore.h"

namespace API {

RefPtr<WebsiteDataStore> WebsiteDataStore::defaultDataStore()
{
    static WebsiteDataStore* defaultDataStore = adoptRef(new WebsiteDataStore(defaultDataStoreConfiguration())).leakRef();

    return defaultDataStore;
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistentDataStore()
{
    return adoptRef(*new WebsiteDataStore);
}

Ref<WebsiteDataStore> WebsiteDataStore::create(WebKit::WebsiteDataStore::Configuration configuration)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration)));
}

WebsiteDataStore::WebsiteDataStore()
    : m_websiteDataStore(WebKit::WebsiteDataStore::createNonPersistent())
{
}

WebsiteDataStore::WebsiteDataStore(WebKit::WebsiteDataStore::Configuration configuration)
    : m_websiteDataStore(WebKit::WebsiteDataStore::create(WTFMove(configuration)))
{
}

WebsiteDataStore::~WebsiteDataStore()
{
}

bool WebsiteDataStore::isPersistent()
{
    return m_websiteDataStore->isPersistent();
}

#if !PLATFORM(COCOA) && !PLATFORM(EFL) && !PLATFORM(GTK)
WebKit::WebsiteDataStore::Configuration WebsiteDataStore::defaultDataStoreConfiguration()
{
    // FIXME: Fill everything in.
    WebKit::WebsiteDataStore::Configuration configuration;

    return configuration;
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String&)
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultLocalStorageDirectory()
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory()
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultNetworkCacheDirectory()
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultApplicationCacheDirectory()
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory()
{
    // FIXME: Implement.
    return String();
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory()
{
    // FIXME: Implement.
    return String();
}

#endif

}

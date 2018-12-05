/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "WebKit2Initialize.h"
#include "WebsiteDataStore.h"

namespace API {

static RefPtr<WebsiteDataStore>& globalDefaultDataStore()
{
    static NeverDestroyed<RefPtr<WebsiteDataStore>> globalDefaultDataStore;
    return globalDefaultDataStore.get();
}


Ref<WebsiteDataStore> WebsiteDataStore::defaultDataStore()
{
    WebKit::InitializeWebKit2();

    auto& store = globalDefaultDataStore();
    if (!store)
        store = adoptRef(new WebsiteDataStore(defaultDataStoreConfiguration(), PAL::SessionID::defaultSessionID()));

    return *store;
}

void WebsiteDataStore::deleteDefaultDataStoreForTesting()
{
    globalDefaultDataStore() = nullptr;
}

bool WebsiteDataStore::defaultDataStoreExists()
{
    return !!globalDefaultDataStore();
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistentDataStore()
{
    return adoptRef(*new WebsiteDataStore);
}

Ref<WebsiteDataStore> WebsiteDataStore::createLegacy(Ref<WebKit::WebsiteDataStoreConfiguration>&& configuration)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration), PAL::SessionID::defaultSessionID()));
}

WebsiteDataStore::WebsiteDataStore()
    : m_websiteDataStore(WebKit::WebsiteDataStore::createNonPersistent())
{
}

WebsiteDataStore::WebsiteDataStore(Ref<WebKit::WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
    : m_websiteDataStore(WebKit::WebsiteDataStore::create(WTFMove(configuration), sessionID))
{
}

WebsiteDataStore::~WebsiteDataStore()
{
}

HTTPCookieStore& WebsiteDataStore::httpCookieStore()
{
    if (!m_apiHTTPCookieStore)
        m_apiHTTPCookieStore = HTTPCookieStore::create(*this);

    return *m_apiHTTPCookieStore;
}

bool WebsiteDataStore::isPersistent()
{
    return m_websiteDataStore->isPersistent();
}

bool WebsiteDataStore::resourceLoadStatisticsEnabled() const
{
    return m_websiteDataStore->resourceLoadStatisticsEnabled();
}

void WebsiteDataStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    m_websiteDataStore->setResourceLoadStatisticsEnabled(enabled);
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
    return m_websiteDataStore->resourceLoadStatisticsDebugMode();
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    m_websiteDataStore->setResourceLoadStatisticsDebugMode(enabled);
}

#if !PLATFORM(COCOA)
WTF::String WebsiteDataStore::defaultMediaCacheDirectory()
{
    // FIXME: Implement. https://bugs.webkit.org/show_bug.cgi?id=156369 and https://bugs.webkit.org/show_bug.cgi?id=156370
    return WTF::String();
}

WTF::String WebsiteDataStore::defaultJavaScriptConfigurationDirectory()
{
    // FIXME: Implement.
    return WTF::String();
}
#endif

Ref<WebKit::WebsiteDataStoreConfiguration> WebsiteDataStore::legacyDefaultDataStoreConfiguration()
{
    auto configuration = defaultDataStoreConfiguration();

    configuration->setApplicationCacheDirectory(legacyDefaultApplicationCacheDirectory());
    configuration->setApplicationCacheFlatFileSubdirectoryName("ApplicationCache");
    configuration->setNetworkCacheDirectory(legacyDefaultNetworkCacheDirectory());
    configuration->setMediaCacheDirectory(legacyDefaultMediaCacheDirectory());
    configuration->setMediaKeysStorageDirectory(legacyDefaultMediaKeysStorageDirectory());
    configuration->setIndexedDBDatabaseDirectory(legacyDefaultIndexedDBDatabaseDirectory());
    configuration->setWebSQLDatabaseDirectory(legacyDefaultWebSQLDatabaseDirectory());
    configuration->setLocalStorageDirectory(legacyDefaultLocalStorageDirectory());
    configuration->setJavaScriptConfigurationDirectory(legacyDefaultJavaScriptConfigurationDirectory());
    
    return configuration;
}

} // namespace API

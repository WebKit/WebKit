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

#pragma once

#include "APIHTTPCookieStore.h"
#include "WebsiteDataStore.h"
#include <pal/SessionID.h>
#include <wtf/text/WTFString.h>

namespace API {

class WebsiteDataStore final : public ObjectImpl<Object::Type::WebsiteDataStore> {
public:
    static Ref<WebsiteDataStore> defaultDataStore();
    static bool defaultDataStoreExists();
    static void deleteDefaultDataStoreForTesting();

    static Ref<WebsiteDataStore> createNonPersistentDataStore();
    static Ref<WebsiteDataStore> createLegacy(Ref<WebKit::WebsiteDataStoreConfiguration>&&);

    explicit WebsiteDataStore(Ref<WebKit::WebsiteDataStoreConfiguration>&&, PAL::SessionID);
    virtual ~WebsiteDataStore();

    bool isPersistent();

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsDebugMode() const;
    void setResourceLoadStatisticsDebugMode(bool);

    WebKit::WebsiteDataStore& websiteDataStore() { return m_websiteDataStore.get(); }
    HTTPCookieStore& httpCookieStore();

    WTF::String indexedDBDatabaseDirectory();

    static WTF::String defaultApplicationCacheDirectory();
    static WTF::String defaultCacheStorageDirectory();
    static WTF::String defaultNetworkCacheDirectory();
    static WTF::String defaultMediaCacheDirectory();
    static WTF::String defaultIndexedDBDatabaseDirectory();
    static WTF::String defaultServiceWorkerRegistrationDirectory();
    static WTF::String defaultLocalStorageDirectory();
    static WTF::String defaultMediaKeysStorageDirectory();
    static WTF::String defaultDeviceIdHashSaltsStorageDirectory();
    static WTF::String defaultWebSQLDatabaseDirectory();
    static WTF::String defaultResourceLoadStatisticsDirectory();
    static WTF::String defaultJavaScriptConfigurationDirectory();

    static Ref<WebKit::WebsiteDataStoreConfiguration> defaultDataStoreConfiguration();

    static WTF::String legacyDefaultApplicationCacheDirectory();
    static WTF::String legacyDefaultNetworkCacheDirectory();
    static WTF::String legacyDefaultLocalStorageDirectory();
    static WTF::String legacyDefaultIndexedDBDatabaseDirectory();
    static WTF::String legacyDefaultWebSQLDatabaseDirectory();
    static WTF::String legacyDefaultMediaKeysStorageDirectory();
    static WTF::String legacyDefaultDeviceIdHashSaltsStorageDirectory();
    static WTF::String legacyDefaultMediaCacheDirectory();
    static WTF::String legacyDefaultJavaScriptConfigurationDirectory();

    static Ref<WebKit::WebsiteDataStoreConfiguration> legacyDefaultDataStoreConfiguration();

private:
    enum ShouldCreateDirectory { CreateDirectory, DontCreateDirectory };

    WebsiteDataStore();

    static WTF::String tempDirectoryFileSystemRepresentation(const WTF::String& directoryName, ShouldCreateDirectory = CreateDirectory);
    static WTF::String cacheDirectoryFileSystemRepresentation(const WTF::String& directoryName);
    static WTF::String websiteDataDirectoryFileSystemRepresentation(const WTF::String& directoryName);

    Ref<WebKit::WebsiteDataStore> m_websiteDataStore;
};

}

/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef APIWebsiteDataStore_h
#define APIWebsiteDataStore_h

#include "APIObject.h"
#include "WebsiteDataStore.h"
#include <WebCore/SessionID.h>
#include <wtf/text/WTFString.h>

namespace API {

class WebsiteDataStore final : public ObjectImpl<Object::Type::WebsiteDataStore> {
public:
    static RefPtr<WebsiteDataStore> defaultDataStore();
    static Ref<WebsiteDataStore> createNonPersistentDataStore();
    static Ref<WebsiteDataStore> create(WebKit::WebsiteDataStore::Configuration);
    virtual ~WebsiteDataStore();

    bool isPersistent();

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);

    WebKit::WebsiteDataStore& websiteDataStore() { return *m_websiteDataStore; }

    static String defaultApplicationCacheDirectory();
    static String defaultNetworkCacheDirectory();
    static String defaultMediaCacheDirectory();

    static String defaultIndexedDBDatabaseDirectory();
    static String defaultLocalStorageDirectory();
    static String defaultMediaKeysStorageDirectory();
    static String defaultWebSQLDatabaseDirectory();
    static String defaultResourceLoadStatisticsDirectory();

private:
    WebsiteDataStore(WebKit::WebsiteDataStore::Configuration);
    WebsiteDataStore();

    static String tempDirectoryFileSystemRepresentation(const String& directoryName);
    static String cacheDirectoryFileSystemRepresentation(const String& directoryName);
    static String websiteDataDirectoryFileSystemRepresentation(const String& directoryName);

    static WebKit::WebsiteDataStore::Configuration defaultDataStoreConfiguration();

    RefPtr<WebKit::WebsiteDataStore> m_websiteDataStore;
};

}

#endif // APIWebsiteDataStore_h

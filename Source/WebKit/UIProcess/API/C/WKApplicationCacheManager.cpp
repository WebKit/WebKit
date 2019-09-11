/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WKApplicationCacheManager.h"

#include "APIArray.h"
#include "APIWebsiteDataStore.h"
#include "WKAPICast.h"
#include "WebsiteDataRecord.h"

WKTypeID WKApplicationCacheManagerGetTypeID()
{
    return WebKit::toAPI(API::WebsiteDataStore::APIType);
}

void WKApplicationCacheManagerGetApplicationCacheOrigins(WKApplicationCacheManagerRef applicationCacheManager, void* context, WKApplicationCacheManagerGetApplicationCacheOriginsFunction callback)
{
    auto& websiteDataStore = WebKit::toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();
    websiteDataStore.fetchData(WebKit::WebsiteDataType::OfflineWebApplicationCache, { }, [context, callback](auto dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(origin.securityOrigin()));
        }

        callback(WebKit::toAPI(API::Array::create(WTFMove(securityOrigins)).ptr()), nullptr, context);
    });
}

void WKApplicationCacheManagerDeleteEntriesForOrigin(WKApplicationCacheManagerRef applicationCacheManager, WKSecurityOriginRef origin)
{
    auto& websiteDataStore = WebKit::toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();

    WebKit::WebsiteDataRecord dataRecord;
    dataRecord.add(WebKit::WebsiteDataType::OfflineWebApplicationCache, WebKit::toImpl(origin)->securityOrigin().data());

    websiteDataStore.removeData(WebKit::WebsiteDataType::OfflineWebApplicationCache, { dataRecord }, [] { });
}

void WKApplicationCacheManagerDeleteAllEntries(WKApplicationCacheManagerRef applicationCacheManager)
{
    auto& websiteDataStore = WebKit::toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();
    websiteDataStore.removeData(WebKit::WebsiteDataType::OfflineWebApplicationCache, -WallTime::infinity(), [] { });
}

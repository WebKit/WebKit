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
#include "WKResourceCacheManager.h"

#include "APIArray.h"
#include "APIWebsiteDataStore.h"
#include "WKAPICast.h"
#include "WebsiteDataRecord.h"

using namespace WebKit;

WKTypeID WKResourceCacheManagerGetTypeID()
{
    return toAPI(API::WebsiteDataStore::APIType);
}

static OptionSet<WebsiteDataType> toWebsiteDataTypes(WKResourceCachesToClear cachesToClear)
{
    OptionSet<WebsiteDataType> websiteDataTypes { WebsiteDataType::MemoryCache };

    if (cachesToClear == WKResourceCachesToClearAll)
        websiteDataTypes.add(WebsiteDataType::DiskCache);

    return websiteDataTypes;
}

void WKResourceCacheManagerGetCacheOrigins(WKResourceCacheManagerRef cacheManager, void* context, WKResourceCacheManagerGetCacheOriginsFunction callback)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(cacheManager))->websiteDataStore();
    websiteDataStore.fetchData(toWebsiteDataTypes(WKResourceCachesToClearAll), { }, [context, callback](auto dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(origin.securityOrigin()));
        }

        callback(toAPI(API::Array::create(WTFMove(securityOrigins)).ptr()), nullptr, context);
    });
}

void WKResourceCacheManagerClearCacheForOrigin(WKResourceCacheManagerRef cacheManager, WKSecurityOriginRef origin, WKResourceCachesToClear cachesToClear)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(cacheManager))->websiteDataStore();

    Vector<WebsiteDataRecord> dataRecords;

    {
        WebsiteDataRecord dataRecord;
        dataRecord.add(WebsiteDataType::MemoryCache, toImpl(origin)->securityOrigin().data());

        dataRecords.append(dataRecord);
    }

    if (cachesToClear == WKResourceCachesToClearAll) {
        WebsiteDataRecord dataRecord;
        dataRecord.add(WebsiteDataType::DiskCache, toImpl(origin)->securityOrigin().data());

        dataRecords.append(dataRecord);
    }

    websiteDataStore.removeData(toWebsiteDataTypes(cachesToClear), dataRecords, [] { });
}

void WKResourceCacheManagerClearCacheForAllOrigins(WKResourceCacheManagerRef cacheManager, WKResourceCachesToClear cachesToClear)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(cacheManager))->websiteDataStore();
    websiteDataStore.removeData(toWebsiteDataTypes(cachesToClear), -WallTime::infinity(), [] { });
}

/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WKOriginDataManager.h"

#include "APIWebsiteDataStore.h"
#include "WKAPICast.h"
#include "WebsiteDataRecord.h"

using namespace WebKit;

WKTypeID WKOriginDataManagerGetTypeID()
{
    return toAPI(API::WebsiteDataStore::APIType);
}

void WKOriginDataManagerGetOrigins(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, void* context, WKOriginDataManagerGetOriginsFunction callback)
{
    // This is the only type supported.
    ASSERT_UNUSED(types, types == kWKIndexedDatabaseData);

    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(originDataManager))->websiteDataStore();
    websiteDataStore.fetchData(WebsiteDataTypes::WebsiteDataTypeIndexedDBDatabases, [context, callback](Vector<WebsiteDataRecord> dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(*origin));
        }

        callback(toAPI(API::Array::create(WTF::move(securityOrigins)).ptr()), nullptr, context);
    });
}

void WKOriginDataManagerDeleteEntriesForOrigin(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, WKSecurityOriginRef origin, void* context, WKOriginDataManagerDeleteEntriesCallbackFunction callback)
{
    // This is the only type supported.
    ASSERT_UNUSED(types, types == kWKIndexedDatabaseData);

    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(originDataManager))->websiteDataStore();

    WebsiteDataRecord dataRecord;
    dataRecord.add(WebsiteDataTypes::WebsiteDataTypeIndexedDBDatabases, &toImpl(origin)->securityOrigin());

    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypeIndexedDBDatabases, { dataRecord }, [context, callback] {
        callback(nullptr, context);
    });
}

void WKOriginDataManagerDeleteEntriesModifiedBetweenDates(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, double startDate, double endDate, void* context, WKOriginDataManagerDeleteEntriesCallbackFunction callback)
{
    // This is the only type supported.
    ASSERT_UNUSED(types, types == kWKIndexedDatabaseData);
    UNUSED_PARAM(endDate);

    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(originDataManager))->websiteDataStore();
    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypeIndexedDBDatabases, std::chrono::system_clock::from_time_t(startDate), [context, callback] {
        callback(nullptr, context);
    });
}

void WKOriginDataManagerDeleteAllEntries(WKOriginDataManagerRef originDataManager, WKOriginDataTypes types, void* context, WKOriginDataManagerDeleteEntriesCallbackFunction callback)
{
    // This is the only type supported.
    ASSERT_UNUSED(types, types == kWKIndexedDatabaseData);

    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(originDataManager))->websiteDataStore();
    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypeIndexedDBDatabases, std::chrono::system_clock::time_point::min(), [context, callback] {
        callback(nullptr, context);
    });
}

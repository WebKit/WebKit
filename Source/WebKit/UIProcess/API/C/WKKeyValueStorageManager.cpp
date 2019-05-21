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
#include "WKKeyValueStorageManager.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APIWebsiteDataStore.h"
#include "StorageManager.h"
#include "WKAPICast.h"
#include "WebsiteDataRecord.h"
#include "WebsiteDataStore.h"
#include <wtf/RunLoop.h>

using namespace WebKit;

WKTypeID WKKeyValueStorageManagerGetTypeID()
{
    return toAPI(API::WebsiteDataStore::APIType);
}

WKStringRef WKKeyValueStorageManagerGetOriginKey()
{
    static API::String& key = API::String::create("WebKeyValueStorageManagerStorageDetailsOriginKey").leakRef();
    return toAPI(&key);
}

WKStringRef WKKeyValueStorageManagerGetCreationTimeKey()
{
    static API::String& key = API::String::create("WebKeyValueStorageManagerStorageDetailsCreationTimeKey").leakRef();
    return toAPI(&key);
}

WKStringRef WKKeyValueStorageManagerGetModificationTimeKey()
{
    static API::String& key = API::String::create("WebKeyValueStorageManagerStorageDetailsModificationTimeKey").leakRef();
    return toAPI(&key);
}

void WKKeyValueStorageManagerGetKeyValueStorageOrigins(WKKeyValueStorageManagerRef keyValueStorageManager, void* context, WKKeyValueStorageManagerGetKeyValueStorageOriginsFunction callback)

{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore();
    websiteDataStore.fetchData({ WebsiteDataType::LocalStorage, WebsiteDataType::SessionStorage }, { }, [context, callback](auto dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(origin.securityOrigin()));
        }

        callback(toAPI(API::Array::create(WTFMove(securityOrigins)).ptr()), nullptr, context);
    });
}

void WKKeyValueStorageManagerGetStorageDetailsByOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, void* context, WKKeyValueStorageManagerGetStorageDetailsByOriginFunction callback)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore();
    websiteDataStore.getLocalStorageDetails([context, callback](auto&& details) {
        Vector<RefPtr<API::Object>> result;
        result.reserveInitialCapacity(details.size());

        for (const auto& detail : details) {
            HashMap<String, RefPtr<API::Object>> detailMap;
            RefPtr<API::Object> origin = API::SecurityOrigin::create(WebCore::SecurityOriginData::fromDatabaseIdentifier(detail.originIdentifier)->securityOrigin());

            detailMap.set(toImpl(WKKeyValueStorageManagerGetOriginKey())->string(), origin);
            if (detail.creationTime)
                detailMap.set(toImpl(WKKeyValueStorageManagerGetCreationTimeKey())->string(), API::Double::create(detail.creationTime->secondsSinceEpoch().value()));
            if (detail.modificationTime)
                detailMap.set(toImpl(WKKeyValueStorageManagerGetModificationTimeKey())->string(), API::Double::create(detail.modificationTime->secondsSinceEpoch().value()));

            result.uncheckedAppend(API::Dictionary::create(WTFMove(detailMap)));
        }

        callback(toAPI(API::Array::create(WTFMove(result)).ptr()), nullptr, context);
    });
}

void WKKeyValueStorageManagerDeleteEntriesForOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, WKSecurityOriginRef origin)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore();
    WebsiteDataRecord dataRecord;
    dataRecord.add(WebsiteDataType::LocalStorage, toImpl(origin)->securityOrigin().data());
    dataRecord.add(WebsiteDataType::SessionStorage, toImpl(origin)->securityOrigin().data());
    websiteDataStore.removeData({ WebsiteDataType::LocalStorage, WebsiteDataType::SessionStorage }, { dataRecord }, [] { });
}

void WKKeyValueStorageManagerDeleteAllEntries(WKKeyValueStorageManagerRef keyValueStorageManager)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore();
    websiteDataStore.removeData({ WebsiteDataType::LocalStorage, WebsiteDataType::SessionStorage }, -WallTime::infinity(), [] { });
}

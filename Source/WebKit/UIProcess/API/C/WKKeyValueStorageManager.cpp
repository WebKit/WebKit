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
    StorageManager* storageManager = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore().storageManager();
    if (!storageManager) {
        RunLoop::main().dispatch([context, callback] {
            callback(toAPI(API::Array::create().ptr()), nullptr, context);
        });
        return;
    }

    storageManager->getLocalStorageOrigins([context, callback](auto&& securityOrigins) {
        Vector<RefPtr<API::Object>> webSecurityOrigins;
        webSecurityOrigins.reserveInitialCapacity(securityOrigins.size());
        for (auto& origin : securityOrigins)
            webSecurityOrigins.uncheckedAppend(API::SecurityOrigin::create(origin.securityOrigin()));

        callback(toAPI(API::Array::create(WTFMove(webSecurityOrigins)).ptr()), nullptr, context);
    });
}

void WKKeyValueStorageManagerGetStorageDetailsByOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, void* context, WKKeyValueStorageManagerGetStorageDetailsByOriginFunction callback)
{
    StorageManager* storageManager = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore().storageManager();
    if (!storageManager) {
        RunLoop::main().dispatch([context, callback] {
            callback(toAPI(API::Array::create().ptr()), nullptr, context);
        });
        return;
    }

    storageManager->getLocalStorageOriginDetails([context, callback](auto storageDetails) {
        HashMap<String, RefPtr<API::Object>> detailsMap;
        Vector<RefPtr<API::Object>> result;
        result.reserveInitialCapacity(storageDetails.size());

        for (const auto& originDetails : storageDetails) {
            HashMap<String, RefPtr<API::Object>> detailsMap;

            RefPtr<API::Object> origin = API::SecurityOrigin::create(WebCore::SecurityOriginData::fromDatabaseIdentifier(originDetails.originIdentifier)->securityOrigin());

            detailsMap.set(toImpl(WKKeyValueStorageManagerGetOriginKey())->string(), origin);
            if (originDetails.creationTime)
                detailsMap.set(toImpl(WKKeyValueStorageManagerGetCreationTimeKey())->string(), API::Double::create(originDetails.creationTime->secondsSinceEpoch().value()));
            if (originDetails.modificationTime)
                detailsMap.set(toImpl(WKKeyValueStorageManagerGetModificationTimeKey())->string(), API::Double::create(originDetails.modificationTime->secondsSinceEpoch().value()));

            result.uncheckedAppend(API::Dictionary::create(WTFMove(detailsMap)));
        }

        callback(toAPI(API::Array::create(WTFMove(result)).ptr()), nullptr, context);
    });
}

void WKKeyValueStorageManagerDeleteEntriesForOrigin(WKKeyValueStorageManagerRef keyValueStorageManager, WKSecurityOriginRef origin)
{
    StorageManager* storageManager = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore().storageManager();
    if (!storageManager)
        return;

    storageManager->deleteLocalStorageEntriesForOrigin(toImpl(origin)->securityOrigin().data());
}

void WKKeyValueStorageManagerDeleteAllEntries(WKKeyValueStorageManagerRef keyValueStorageManager)
{
    StorageManager* storageManager = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(keyValueStorageManager))->websiteDataStore().storageManager();
    if (!storageManager)
        return;

    storageManager->deleteLocalStorageOriginsModifiedSince(-WallTime::infinity(), [] { });
}

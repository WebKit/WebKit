/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "WebKeyValueStorageManager.h"

#include "APIArray.h"
#include "APISecurityOrigin.h"
#include "LocalStorageDetails.h"
#include "SecurityOriginData.h"
#include "StorageManager.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

const char* WebKeyValueStorageManager::supplementName()
{
    return "WebKeyValueStorageManager";
}

String WebKeyValueStorageManager::originKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebKeyValueStorageManagerStorageDetailsOriginKey"));
    return key;
}

String WebKeyValueStorageManager::creationTimeKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebKeyValueStorageManagerStorageDetailsCreationTimeKey"));
    return key;
}

String WebKeyValueStorageManager::modificationTimeKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebKeyValueStorageManagerStorageDetailsModificationTimeKey"));
    return key;
}

PassRefPtr<WebKeyValueStorageManager> WebKeyValueStorageManager::create(WebProcessPool* processPool)
{
    return adoptRef(new WebKeyValueStorageManager(processPool));
}

WebKeyValueStorageManager::WebKeyValueStorageManager(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
{
}

WebKeyValueStorageManager::~WebKeyValueStorageManager()
{
}

// WebContextSupplement

void WebKeyValueStorageManager::refWebContextSupplement()
{
    API::Object::ref();
}

void WebKeyValueStorageManager::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebKeyValueStorageManager::getKeyValueStorageOrigins(std::function<void (API::Array*, CallbackBase::Error)> callbackFunction)
{
    StorageManager* storageManager = processPool()->websiteDataStore().storageManager();
    if (!storageManager) {
        RunLoop::main().dispatch([callbackFunction] {
            callbackFunction(API::Array::create().get(), CallbackBase::Error::None);
        });
        return;
    }

    storageManager->getOrigins([callbackFunction](Vector<RefPtr<SecurityOrigin>> securityOrigins) {
        Vector<RefPtr<API::Object>> webSecurityOrigins;
        webSecurityOrigins.reserveInitialCapacity(securityOrigins.size());
        for (auto& origin : securityOrigins)
            webSecurityOrigins.uncheckedAppend(API::SecurityOrigin::create(WTF::move(origin)));

        callbackFunction(API::Array::create(WTF::move(webSecurityOrigins)).get(), CallbackBase::Error::None);
    });
}

void WebKeyValueStorageManager::getStorageDetailsByOrigin(std::function<void (API::Array*, CallbackBase::Error)> callbackFunction)
{
    StorageManager* storageManager = processPool()->websiteDataStore().storageManager();
    if (!storageManager) {
        RunLoop::main().dispatch([callbackFunction] {
            callbackFunction(API::Array::create().get(), CallbackBase::Error::None);
        });
        return;
    }

    storageManager->getStorageDetailsByOrigin([callbackFunction](Vector<LocalStorageDetails> storageDetails) {
        HashMap<String, RefPtr<API::Object>> detailsMap;
        Vector<RefPtr<API::Object>> result;
        result.reserveInitialCapacity(storageDetails.size());

        for (const LocalStorageDetails& originDetails : storageDetails) {
            HashMap<String, RefPtr<API::Object>> detailsMap;

            RefPtr<API::Object> origin = API::SecurityOrigin::create(SecurityOrigin::createFromDatabaseIdentifier(originDetails.originIdentifier));

            detailsMap.set(WebKeyValueStorageManager::originKey(), origin);
            if (originDetails.creationTime)
                detailsMap.set(WebKeyValueStorageManager::creationTimeKey(), API::Double::create(originDetails.creationTime.valueOr(0)));
            if (originDetails.modificationTime)
                detailsMap.set(WebKeyValueStorageManager::modificationTimeKey(), API::Double::create(originDetails.modificationTime.valueOr(0)));

            result.uncheckedAppend(API::Dictionary::create(WTF::move(detailsMap)));
        }

        callbackFunction(API::Array::create(WTF::move(result)).get(), CallbackBase::Error::None);
    });
}

void WebKeyValueStorageManager::deleteEntriesForOrigin(API::SecurityOrigin* origin)
{
    StorageManager* storageManager = processPool()->websiteDataStore().storageManager();
    if (!storageManager)
        return;

    storageManager->deleteEntriesForOrigin(origin->securityOrigin());
}

void WebKeyValueStorageManager::deleteAllEntries()
{
    StorageManager* storageManager = processPool()->websiteDataStore().storageManager();
    if (!storageManager)
        return;

    storageManager->deleteAllEntries();
}

} // namespace WebKit

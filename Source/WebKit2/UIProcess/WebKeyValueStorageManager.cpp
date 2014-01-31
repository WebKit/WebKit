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
#include "LocalStorageDetails.h"
#include "SecurityOriginData.h"
#include "WebContext.h"
#include "WebSecurityOrigin.h"
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

PassRefPtr<WebKeyValueStorageManager> WebKeyValueStorageManager::create(WebContext* context)
{
    return adoptRef(new WebKeyValueStorageManager(context));
}

WebKeyValueStorageManager::WebKeyValueStorageManager(WebContext* context)
    : WebContextSupplement(context)
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

static void didGetKeyValueStorageOrigins(const Vector<RefPtr<SecurityOrigin>>& securityOrigins, void* context)
{
    RefPtr<ArrayCallback> callback = adoptRef(static_cast<ArrayCallback*>(context));

    Vector<RefPtr<API::Object>> webSecurityOrigins;
    webSecurityOrigins.reserveInitialCapacity(securityOrigins.size());

    for (unsigned i = 0; i < securityOrigins.size(); ++i)
        webSecurityOrigins.uncheckedAppend(WebSecurityOrigin::create(securityOrigins[i]));

    callback->performCallbackWithReturnValue(API::Array::create(std::move(webSecurityOrigins)).get());
}

void WebKeyValueStorageManager::getKeyValueStorageOrigins(PassRefPtr<ArrayCallback> prpCallback)
{
    context()->storageManager().getOrigins(RunLoop::main(), prpCallback.leakRef(), didGetKeyValueStorageOrigins);
}

static void didGetStorageDetailsByOrigin(const Vector<LocalStorageDetails>& storageDetails, void* context)
{
    RefPtr<ArrayCallback> callback = adoptRef(static_cast<ArrayCallback*>(context));

    Vector<RefPtr<API::Object>> result;
    result.reserveInitialCapacity(storageDetails.size());

    for (const LocalStorageDetails& originDetails : storageDetails) {
        HashMap<String, RefPtr<API::Object>> detailsMap;

        RefPtr<API::Object> origin = WebSecurityOrigin::createFromDatabaseIdentifier(originDetails.originIdentifier);

        detailsMap.set(WebKeyValueStorageManager::originKey(), origin);
        if (originDetails.creationTime)
            detailsMap.set(WebKeyValueStorageManager::creationTimeKey(), API::Double::create(originDetails.creationTime));
        if (originDetails.modificationTime)
            detailsMap.set(WebKeyValueStorageManager::modificationTimeKey(), API::Double::create(originDetails.modificationTime));

        result.uncheckedAppend(ImmutableDictionary::create(std::move(detailsMap)));
    }

    callback->performCallbackWithReturnValue(API::Array::create(std::move(result)).get());
}

void WebKeyValueStorageManager::getStorageDetailsByOrigin(PassRefPtr<ArrayCallback> prpCallback)
{
    context()->storageManager().getStorageDetailsByOrigin(RunLoop::main(), prpCallback.leakRef(), didGetStorageDetailsByOrigin);
}

void WebKeyValueStorageManager::deleteEntriesForOrigin(WebSecurityOrigin* origin)
{
    context()->storageManager().deleteEntriesForOrigin(origin->securityOrigin());
}

void WebKeyValueStorageManager::deleteAllEntries()
{
    context()->storageManager().deleteAllEntries();
}

} // namespace WebKit

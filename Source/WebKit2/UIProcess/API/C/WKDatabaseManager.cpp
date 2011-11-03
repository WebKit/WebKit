/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WKDatabaseManager.h"

#include "WebDatabaseManagerProxy.h"
#include "WKAPICast.h"

#ifdef __BLOCKS__
#include <Block.h>
#endif

using namespace WebKit;

WKTypeID WKDatabaseManagerGetTypeID()
{
    return toAPI(WebDatabaseManagerProxy::APIType);
}

WKStringRef WKDatabaseManagerGetOriginKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::originKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetOriginQuotaKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::originQuotaKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetOriginUsageKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::originUsageKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::databaseDetailsKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsNameKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::databaseDetailsNameKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsDisplayNameKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::databaseDetailsDisplayNameKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsExpectedUsageKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::databaseDetailsExpectedUsageKey()).leakRef();
    return toAPI(key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsCurrentUsageKey()
{
    static WebString* key = WebString::create(WebDatabaseManagerProxy::databaseDetailsCurrentUsageKey()).leakRef();
    return toAPI(key);
}

void WKDatabaseManagerSetClient(WKDatabaseManagerRef databaseManagerRef, const WKDatabaseManagerClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(databaseManagerRef)->initializeClient(wkClient);
}

void WKDatabaseManagerGetDatabasesByOrigin(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabasesByOriginFunction callback)
{
    toImpl(databaseManagerRef)->getDatabasesByOrigin(ArrayCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callGetDatabasesByOriginBlockAndDispose(WKArrayRef resultValue, WKErrorRef errorRef, void* context)
{
    WKDatabaseManagerGetDatabasesByOriginBlock block = (WKDatabaseManagerGetDatabasesByOriginBlock)context;
    block(resultValue, errorRef);
    Block_release(block);
}

void WKDatabaseManagerGetDatabasesByOrigin_b(WKDatabaseManagerRef databaseManagerRef, WKDatabaseManagerGetDatabasesByOriginBlock block)
{
    WKDatabaseManagerGetDatabasesByOrigin(databaseManagerRef, Block_copy(block), callGetDatabasesByOriginBlockAndDispose);
}
#endif

void WKDatabaseManagerGetDatabaseOrigins(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabaseOriginsFunction callback)
{
    toImpl(databaseManagerRef)->getDatabaseOrigins(ArrayCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callGetDatabaseOriginsBlockBlockAndDispose(WKArrayRef resultValue, WKErrorRef errorRef, void* context)
{
    WKDatabaseManagerGetDatabaseOriginsBlock block = (WKDatabaseManagerGetDatabaseOriginsBlock)context;
    block(resultValue, errorRef);
    Block_release(block);
}

void WKDatabaseManagerGetDatabaseOrigins_b(WKDatabaseManagerRef databaseManagerRef, WKDatabaseManagerGetDatabaseOriginsBlock block)
{
    WKDatabaseManagerGetDatabaseOrigins(databaseManagerRef, Block_copy(block), callGetDatabaseOriginsBlockBlockAndDispose);
}
#endif

void WKDatabaseManagerDeleteDatabasesWithNameForOrigin(WKDatabaseManagerRef databaseManagerRef, WKStringRef databaseNameRef, WKSecurityOriginRef originRef)
{
    toImpl(databaseManagerRef)->deleteDatabaseWithNameForOrigin(toWTFString(databaseNameRef), toImpl(originRef));
}

void WKDatabaseManagerDeleteDatabasesForOrigin(WKDatabaseManagerRef databaseManagerRef, WKSecurityOriginRef originRef)
{
    toImpl(databaseManagerRef)->deleteDatabasesForOrigin(toImpl(originRef));
}

void WKDatabaseManagerDeleteAllDatabases(WKDatabaseManagerRef databaseManagerRef)
{
    toImpl(databaseManagerRef)->deleteAllDatabases();
}

void WKDatabaseManagerSetQuotaForOrigin(WKDatabaseManagerRef databaseManagerRef, WKSecurityOriginRef originRef, uint64_t quota)
{
    toImpl(databaseManagerRef)->setQuotaForOrigin(toImpl(originRef), quota);
}

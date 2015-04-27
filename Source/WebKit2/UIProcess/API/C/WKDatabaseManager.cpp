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

using namespace WebKit;

WKTypeID WKDatabaseManagerGetTypeID()
{
    return toAPI(WebDatabaseManagerProxy::APIType);
}

WKStringRef WKDatabaseManagerGetOriginKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::originKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetOriginQuotaKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::originQuotaKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetOriginUsageKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::originUsageKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsNameKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsNameKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsDisplayNameKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsDisplayNameKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsExpectedUsageKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsExpectedUsageKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsCurrentUsageKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsCurrentUsageKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsCreationTimeKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsCreationTimeKey()).leakRef();
    return toAPI(&key);
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsModificationTimeKey()
{
    static API::String& key = API::String::create(WebDatabaseManagerProxy::databaseDetailsModificationTimeKey()).leakRef();
    return toAPI(&key);
}

void WKDatabaseManagerSetClient(WKDatabaseManagerRef databaseManagerRef, const WKDatabaseManagerClientBase* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(databaseManagerRef)->initializeClient(wkClient);
}

void WKDatabaseManagerGetDatabasesByOrigin(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabasesByOriginFunction callback)
{
    toImpl(databaseManagerRef)->getDatabasesByOrigin(toGenericCallbackFunction(context, callback));
}

void WKDatabaseManagerGetDatabaseOrigins(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabaseOriginsFunction callback)
{
    toImpl(databaseManagerRef)->getDatabaseOrigins(toGenericCallbackFunction(context, callback));
}

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

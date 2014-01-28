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
#if ENABLE(SQL_DATABASE)
    return toAPI(WebDatabaseManagerProxy::APIType);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetOriginKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::originKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetOriginQuotaKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::originQuotaKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetOriginUsageKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::originUsageKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsNameKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsNameKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsDisplayNameKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsDisplayNameKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsExpectedUsageKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsExpectedUsageKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsCurrentUsageKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsCurrentUsageKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsCreationTimeKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsCreationTimeKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

WKStringRef WKDatabaseManagerGetDatabaseDetailsModificationTimeKey()
{
#if ENABLE(SQL_DATABASE)
    static API::String* key = API::String::create(WebDatabaseManagerProxy::databaseDetailsModificationTimeKey()).leakRef();
    return toAPI(key);
#else
    return 0;
#endif
}

void WKDatabaseManagerSetClient(WKDatabaseManagerRef databaseManagerRef, const WKDatabaseManagerClientBase* wkClient)
{
#if ENABLE(SQL_DATABASE)
    if (wkClient && wkClient->version)
        return;
    toImpl(databaseManagerRef)->initializeClient(wkClient);
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(wkClient);
#endif
}

void WKDatabaseManagerGetDatabasesByOrigin(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabasesByOriginFunction callback)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->getDatabasesByOrigin(ArrayCallback::create(context, callback));
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKDatabaseManagerGetDatabaseOrigins(WKDatabaseManagerRef databaseManagerRef, void* context, WKDatabaseManagerGetDatabaseOriginsFunction callback)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->getDatabaseOrigins(ArrayCallback::create(context, callback));
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
#endif
}

void WKDatabaseManagerDeleteDatabasesWithNameForOrigin(WKDatabaseManagerRef databaseManagerRef, WKStringRef databaseNameRef, WKSecurityOriginRef originRef)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->deleteDatabaseWithNameForOrigin(toWTFString(databaseNameRef), toImpl(originRef));
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(databaseNameRef);
    UNUSED_PARAM(originRef);
#endif
}

void WKDatabaseManagerDeleteDatabasesForOrigin(WKDatabaseManagerRef databaseManagerRef, WKSecurityOriginRef originRef)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->deleteDatabasesForOrigin(toImpl(originRef));
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(originRef);
#endif
}

void WKDatabaseManagerDeleteAllDatabases(WKDatabaseManagerRef databaseManagerRef)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->deleteAllDatabases();
#else
    UNUSED_PARAM(databaseManagerRef);
#endif
}

void WKDatabaseManagerSetQuotaForOrigin(WKDatabaseManagerRef databaseManagerRef, WKSecurityOriginRef originRef, uint64_t quota)
{
#if ENABLE(SQL_DATABASE)
    toImpl(databaseManagerRef)->setQuotaForOrigin(toImpl(originRef), quota);
#else
    UNUSED_PARAM(databaseManagerRef);
    UNUSED_PARAM(originRef);
    UNUSED_PARAM(quota);
#endif
}

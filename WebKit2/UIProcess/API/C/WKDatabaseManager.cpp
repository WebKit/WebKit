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

void WKDatabaseManagerGetDatabaseOrigins(WKDatabaseManagerRef databaseManager, void* context, WKDatabaseManagerGetDatabaseOriginsFunction callback)
{
    toImpl(databaseManager)->getDatabaseOrigins(DatabaseOriginsCallback::create(context, callback));
}

#ifdef __BLOCKS__
static void callGetDatabaseOriginsBlockBlockAndDispose(WKArrayRef resultValue, WKErrorRef error, void* context)
{
    WKDatabaseManagerGetDatabaseOriginsBlock block = (WKDatabaseManagerGetDatabaseOriginsBlock)context;
    block(resultValue, error);
    Block_release(block);
}

void WKDatabaseManagerGetDatabaseOrigins_b(WKDatabaseManagerRef databaseManager, WKDatabaseManagerGetDatabaseOriginsBlock block)
{
    WKDatabaseManagerGetDatabaseOrigins(databaseManager, Block_copy(block), callGetDatabaseOriginsBlockBlockAndDispose);
}
#endif

void WKDatabaseManagerDeleteDatabasesForOrigin(WKDatabaseManagerRef databaseManager, WKSecurityOriginRef origin)
{
    toImpl(databaseManager)->deleteDatabasesForOrigin(toImpl(origin));
}

void WKDatabaseManagerDeleteAllDatabases(WKDatabaseManagerRef databaseManager)
{
    toImpl(databaseManager)->deleteAllDatabases();
}


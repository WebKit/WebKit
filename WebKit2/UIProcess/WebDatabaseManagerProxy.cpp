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

#include "WebDatabaseManagerProxy.h"

#include "ImmutableArray.h"
#include "WebDatabaseManagerMessages.h"
#include "WebContext.h"
#include "WebSecurityOrigin.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebDatabaseManagerProxy> WebDatabaseManagerProxy::create(WebContext* webContext)
{
    return adoptRef(new WebDatabaseManagerProxy(webContext));
}

WebDatabaseManagerProxy::WebDatabaseManagerProxy(WebContext* webContext)
    : m_webContext(webContext)
{
}

WebDatabaseManagerProxy::~WebDatabaseManagerProxy()
{
}

void WebDatabaseManagerProxy::invalidate()
{
    invalidateCallbackMap(m_databaseOriginsCallbacks);

    m_webContext = 0;
}

void WebDatabaseManagerProxy::getDatabaseOrigins(PassRefPtr<DatabaseOriginsCallback> prpCallback)
{
    RefPtr<DatabaseOriginsCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_databaseOriginsCallbacks.set(callbackID, callback.release());
    m_webContext->process()->send(Messages::WebDatabaseManager::GetDatabaseOrigins(callbackID), 0);
}

void WebDatabaseManagerProxy::didGetDatabaseOrigins(const Vector<String>& originIdentifiers, uint64_t callbackID)
{
    RefPtr<DatabaseOriginsCallback> callback = m_databaseOriginsCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    size_t originIdentifiersCount = originIdentifiers.size();
    Vector<RefPtr<APIObject> > securityOrigins(originIdentifiersCount);

    for (size_t i = 0; i < originIdentifiersCount; ++i)
        securityOrigins[i] = WebSecurityOrigin::create(originIdentifiers[i]);

    callback->performCallbackWithReturnValue(ImmutableArray::adopt(securityOrigins).get());
}

void WebDatabaseManagerProxy::deleteDatabasesForOrigin(WebSecurityOrigin* origin)
{
    if (!origin)
        return;

    m_webContext->process()->send(Messages::WebDatabaseManager::DeleteDatabasesForOrigin(origin->databaseIdentifier()), 0);
}

void WebDatabaseManagerProxy::deleteAllDatabases()
{
    m_webContext->process()->send(Messages::WebDatabaseManager::DeleteAllDatabases(), 0);
}

} // namespace WebKit


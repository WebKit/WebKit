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
#include "WebDatabaseManagerProxy.h"

#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "WebDatabaseManagerMessages.h"
#include "WebContext.h"
#include "WebSecurityOrigin.h"

using namespace WebCore;

namespace WebKit {

String WebDatabaseManagerProxy::originKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerOriginKey"));
    return key;
}

String WebDatabaseManagerProxy::originQuotaKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerOriginQuotaKey"));
    return key;
}

String WebDatabaseManagerProxy::originUsageKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerOriginUsageKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerDatabaseDetailsKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsNameKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerDatabaseDetailsNameKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsDisplayNameKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerDatabaseDetailsDisplayNameKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsExpectedUsageKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerDatabaseDetailsExpectedUsageKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsCurrentUsageKey()
{
    DEFINE_STATIC_LOCAL(String, key, ("WebDatabaseManagerDatabaseDetailsCurrentUsageKey"));
    return key;
}

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
    invalidateCallbackMap(m_arrayCallbacks);
}

bool WebDatabaseManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebDatabaseManagerProxy::initializeClient(const WKDatabaseManagerClient* client)
{
    m_client.initialize(client);
}

void WebDatabaseManagerProxy::getDatabasesByOrigin(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::GetDatabasesByOrigin(callbackID));
}

void WebDatabaseManagerProxy::didGetDatabasesByOrigin(const Vector<OriginAndDatabases>& originAndDatabasesVector, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    size_t originAndDatabasesCount = originAndDatabasesVector.size();
    Vector<RefPtr<APIObject> > result(originAndDatabasesCount);

    for (size_t i = 0; i < originAndDatabasesCount; ++i) {
        const OriginAndDatabases& originAndDatabases = originAndDatabasesVector[i];
    
        RefPtr<APIObject> origin = WebSecurityOrigin::create(originAndDatabases.originIdentifier);
    
        size_t databasesCount = originAndDatabases.databases.size();
        Vector<RefPtr<APIObject> > databases(databasesCount);
    
        for (size_t j = 0; j < databasesCount; ++j) {
            const DatabaseDetails& details = originAndDatabases.databases[i];
            HashMap<String, RefPtr<APIObject> > detailsMap;

            detailsMap.set(databaseDetailsNameKey(), WebString::create(details.name()));
            detailsMap.set(databaseDetailsDisplayNameKey(), WebString::create(details.displayName()));
            detailsMap.set(databaseDetailsExpectedUsageKey(), WebUInt64::create(details.expectedUsage()));
            detailsMap.set(databaseDetailsCurrentUsageKey(), WebUInt64::create(details.currentUsage()));
            databases.append(ImmutableDictionary::adopt(detailsMap));
        }

        HashMap<String, RefPtr<APIObject> > originAndDatabasesMap;
        originAndDatabasesMap.set(originKey(), origin);
        originAndDatabasesMap.set(originQuotaKey(), WebUInt64::create(originAndDatabases.originQuota));
        originAndDatabasesMap.set(originUsageKey(), WebUInt64::create(originAndDatabases.originUsage));
        originAndDatabasesMap.set(databaseDetailsKey(), ImmutableArray::adopt(databases));

        result.append(ImmutableDictionary::adopt(originAndDatabasesMap));
    }

    RefPtr<ImmutableArray> resultArray = ImmutableArray::adopt(result);
    callback->performCallbackWithReturnValue(resultArray.get());
}

void WebDatabaseManagerProxy::getDatabaseOrigins(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::GetDatabaseOrigins(callbackID));
}

void WebDatabaseManagerProxy::didGetDatabaseOrigins(const Vector<String>& originIdentifiers, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
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

void WebDatabaseManagerProxy::deleteDatabaseWithNameForOrigin(const String& databaseIdentifier, WebSecurityOrigin* origin)
{
    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::DeleteDatabaseWithNameForOrigin(databaseIdentifier, origin->databaseIdentifier()));
}

void WebDatabaseManagerProxy::deleteDatabasesForOrigin(WebSecurityOrigin* origin)
{
    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::DeleteDatabasesForOrigin(origin->databaseIdentifier()));
}

void WebDatabaseManagerProxy::deleteAllDatabases()
{
    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::DeleteAllDatabases());
}

void WebDatabaseManagerProxy::setQuotaForOrigin(WebSecurityOrigin* origin, uint64_t quota)
{
    // FIXME (Multi-WebProcess): Databases shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebDatabaseManager::SetQuotaForOrigin(origin->databaseIdentifier(), quota));
}

void WebDatabaseManagerProxy::didModifyOrigin(const String& originIdentifier)
{
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(originIdentifier);
    m_client.didModifyOrigin(this, origin.get());
}

void WebDatabaseManagerProxy::didModifyDatabase(const String& originIdentifier, const String& databaseIdentifier)
{
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(originIdentifier);
    m_client.didModifyDatabase(this, origin.get(), databaseIdentifier);
}

} // namespace WebKit


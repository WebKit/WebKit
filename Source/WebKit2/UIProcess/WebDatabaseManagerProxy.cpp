/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(SQL_DATABASE)

#include "APIArray.h"
#include "ImmutableDictionary.h"
#include "WebContext.h"
#include "WebDatabaseManagerMessages.h"
#include "WebDatabaseManagerProxyMessages.h"
#include "WebSecurityOrigin.h"
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

const char* WebDatabaseManagerProxy::supplementName()
{
    return "WebDatabaseManagerProxy";
}

String WebDatabaseManagerProxy::originKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerOriginKey"));
    return key;
}

String WebDatabaseManagerProxy::originQuotaKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerOriginQuotaKey"));
    return key;
}

String WebDatabaseManagerProxy::originUsageKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerOriginUsageKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsNameKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsNameKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsDisplayNameKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsDisplayNameKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsExpectedUsageKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsExpectedUsageKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsCurrentUsageKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsCurrentUsageKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsCreationTimeKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsCreationTimeKey"));
    return key;
}

String WebDatabaseManagerProxy::databaseDetailsModificationTimeKey()
{
    static NeverDestroyed<String> key(ASCIILiteral("WebDatabaseManagerDatabaseDetailsModificationTimeKey"));
    return key;
}

PassRefPtr<WebDatabaseManagerProxy> WebDatabaseManagerProxy::create(WebContext* webContext)
{
    return adoptRef(new WebDatabaseManagerProxy(webContext));
}

WebDatabaseManagerProxy::WebDatabaseManagerProxy(WebContext* webContext)
    : WebContextSupplement(webContext)
{
    WebContextSupplement::context()->addMessageReceiver(Messages::WebDatabaseManagerProxy::messageReceiverName(), *this);
}

WebDatabaseManagerProxy::~WebDatabaseManagerProxy()
{
}

void WebDatabaseManagerProxy::initializeClient(const WKDatabaseManagerClientBase* client)
{
    m_client.initialize(client);
}

// WebContextSupplement

void WebDatabaseManagerProxy::contextDestroyed()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

void WebDatabaseManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks);
}

bool WebDatabaseManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebDatabaseManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebDatabaseManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebDatabaseManagerProxy::getDatabasesByOrigin(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    context()->sendToOneProcess(Messages::WebDatabaseManager::GetDatabasesByOrigin(callbackID));
}

void WebDatabaseManagerProxy::didGetDatabasesByOrigin(const Vector<OriginAndDatabases>& originAndDatabasesVector, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    Vector<RefPtr<API::Object>> result;
    result.reserveInitialCapacity(originAndDatabasesVector.size());

    for (const auto& originAndDatabases : originAndDatabasesVector) {
        RefPtr<API::Object> origin = WebSecurityOrigin::createFromDatabaseIdentifier(originAndDatabases.originIdentifier);

        Vector<RefPtr<API::Object>> databases;
        databases.reserveInitialCapacity(originAndDatabases.databases.size());

        for (const auto& databaseDetails : originAndDatabases.databases) {
            HashMap<String, RefPtr<API::Object>> detailsMap;

            detailsMap.set(databaseDetailsNameKey(), API::String::create(databaseDetails.name()));
            detailsMap.set(databaseDetailsDisplayNameKey(), API::String::create(databaseDetails.displayName()));
            detailsMap.set(databaseDetailsExpectedUsageKey(), API::UInt64::create(databaseDetails.expectedUsage()));
            detailsMap.set(databaseDetailsCurrentUsageKey(), API::UInt64::create(databaseDetails.currentUsage()));
            if (databaseDetails.creationTime())
                detailsMap.set(databaseDetailsCreationTimeKey(), API::Double::create(databaseDetails.creationTime()));
            if (databaseDetails.modificationTime())
                detailsMap.set(databaseDetailsModificationTimeKey(), API::Double::create(databaseDetails.modificationTime()));

            databases.uncheckedAppend(ImmutableDictionary::create(std::move(detailsMap)));
        }

        HashMap<String, RefPtr<API::Object>> originAndDatabasesMap;
        originAndDatabasesMap.set(originKey(), origin);
        originAndDatabasesMap.set(originQuotaKey(), API::UInt64::create(originAndDatabases.originQuota));
        originAndDatabasesMap.set(originUsageKey(), API::UInt64::create(originAndDatabases.originUsage));
        originAndDatabasesMap.set(databaseDetailsKey(), API::Array::create(std::move(databases)));

        result.uncheckedAppend(ImmutableDictionary::create(std::move(originAndDatabasesMap)));
    }

    callback->performCallbackWithReturnValue(API::Array::create(std::move(result)).get());
}

void WebDatabaseManagerProxy::getDatabaseOrigins(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    context()->sendToOneProcess(Messages::WebDatabaseManager::GetDatabaseOrigins(callbackID));
}

void WebDatabaseManagerProxy::didGetDatabaseOrigins(const Vector<String>& originIdentifiers, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    Vector<RefPtr<API::Object>> securityOrigins;
    securityOrigins.reserveInitialCapacity(originIdentifiers.size());

    for (const auto& originIdentifier : originIdentifiers)
        securityOrigins.uncheckedAppend(WebSecurityOrigin::createFromDatabaseIdentifier(originIdentifier));

    callback->performCallbackWithReturnValue(API::Array::create(std::move(securityOrigins)).get());
}

void WebDatabaseManagerProxy::deleteDatabaseWithNameForOrigin(const String& databaseIdentifier, WebSecurityOrigin* origin)
{
    context()->sendToOneProcess(Messages::WebDatabaseManager::DeleteDatabaseWithNameForOrigin(databaseIdentifier, origin->databaseIdentifier()));
}

void WebDatabaseManagerProxy::deleteDatabasesForOrigin(WebSecurityOrigin* origin)
{
    context()->sendToOneProcess(Messages::WebDatabaseManager::DeleteDatabasesForOrigin(origin->databaseIdentifier()));
}

void WebDatabaseManagerProxy::deleteAllDatabases()
{
    context()->sendToOneProcess(Messages::WebDatabaseManager::DeleteAllDatabases());
}

void WebDatabaseManagerProxy::setQuotaForOrigin(WebSecurityOrigin* origin, uint64_t quota)
{
    context()->sendToOneProcess(Messages::WebDatabaseManager::SetQuotaForOrigin(origin->databaseIdentifier(), quota));
}

void WebDatabaseManagerProxy::didModifyOrigin(const String& originIdentifier)
{
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::createFromDatabaseIdentifier(originIdentifier);
    m_client.didModifyOrigin(this, origin.get());
}

void WebDatabaseManagerProxy::didModifyDatabase(const String& originIdentifier, const String& databaseIdentifier)
{
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::createFromDatabaseIdentifier(originIdentifier);
    m_client.didModifyDatabase(this, origin.get(), databaseIdentifier);
}

} // namespace WebKit

#endif // ENABLE(SQL_DATABASE)

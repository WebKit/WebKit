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
#include "WebOriginDataManagerProxy.h"

#include "SecurityOriginData.h"
#include "WebContext.h"
#include "WebOriginDataManagerMessages.h"
#include "WebOriginDataManagerProxyMessages.h"
#include "WebSecurityOrigin.h"
#include <WebCore/SecurityOrigin.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

const char* WebOriginDataManagerProxy::supplementName()
{
    return "WebOriginDataManagerProxy";
}

PassRefPtr<WebOriginDataManagerProxy> WebOriginDataManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebOriginDataManagerProxy(context));
}

WebOriginDataManagerProxy::WebOriginDataManagerProxy(WebContext* context)
    : WebContextSupplement(context)
{
    context->addMessageReceiver(Messages::WebOriginDataManagerProxy::messageReceiverName(), *this);
}

WebOriginDataManagerProxy::~WebOriginDataManagerProxy()
{
}


void WebOriginDataManagerProxy::contextDestroyed()
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::OwnerWasInvalidated);
}

void WebOriginDataManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::ProcessExited);
}

bool WebOriginDataManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebOriginDataManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebOriginDataManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebOriginDataManagerProxy::getOrigins(WKOriginDataTypes types, std::function<void (API::Array*, CallbackBase::Error)> callbackFunction)
{
    // FIXME: Right now we only support IndexedDatabase data so we know that we're only sending this request to the DatabaseProcess.
    // That's why having one single callback works.
    // In the future when we message N-processes we'll have to wait for all N replies before responding to the client.
    if (!(types & kWKIndexedDatabaseData)) {
        callbackFunction(nullptr, CallbackBase::Error::None);
        return;
    }

    RefPtr<ArrayCallback> callback = ArrayCallback::create(WTF::move(callbackFunction));

    if (!context()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    context()->sendToDatabaseProcessRelaunchingIfNecessary(Messages::WebOriginDataManager::GetOrigins(types, callbackID));
}

void WebOriginDataManagerProxy::didGetOrigins(IPC::Connection* connection, const Vector<SecurityOriginData>& originIdentifiers, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, connection);

    Vector<RefPtr<API::Object>> securityOrigins;
    securityOrigins.reserveInitialCapacity(originIdentifiers.size());

    for (const auto& originIdentifier : originIdentifiers)
        securityOrigins.uncheckedAppend(WebSecurityOrigin::create(originIdentifier.securityOrigin()));

    callback->performCallbackWithReturnValue(API::Array::create(WTF::move(securityOrigins)).get());
}

void WebOriginDataManagerProxy::deleteEntriesForOrigin(WKOriginDataTypes types, WebSecurityOrigin* origin, std::function<void (CallbackBase::Error)> callbackFunction)
{
    // FIXME: Right now we only support IndexedDatabase data so we know that we're only sending this request to the DatabaseProcess.
    // That's why having one single callback works.
    // In the future when we message N-processes we'll have to wait for all N replies before responding to the client.
    if (!(types & kWKIndexedDatabaseData)) {
        callbackFunction(CallbackBase::Error::None);
        return;
    }

    RefPtr<VoidCallback> callback = VoidCallback::create(WTF::move(callbackFunction));

    if (!context()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_voidCallbacks.set(callbackID, callback.release());

    SecurityOriginData securityOriginData;
    securityOriginData.protocol = origin->securityOrigin().protocol();
    securityOriginData.host = origin->securityOrigin().host();
    securityOriginData.port = origin->securityOrigin().port();

    context()->sendToDatabaseProcessRelaunchingIfNecessary(Messages::WebOriginDataManager::DeleteEntriesForOrigin(types, securityOriginData, callbackID));
}

void WebOriginDataManagerProxy::deleteEntriesModifiedBetweenDates(WKOriginDataTypes types, double startDate, double endDate, std::function<void (CallbackBase::Error)> callbackFunction)
{
    // FIXME: Right now we only support IndexedDatabase data so we know that we're only sending this request to the DatabaseProcess.
    // That's why having one single callback works.
    // In the future when we message N-processes we'll have to wait for all N replies before responding to the client.
    if (!(types & kWKIndexedDatabaseData)) {
        callbackFunction(CallbackBase::Error::None);
        return;
    }

    RefPtr<VoidCallback> callback = VoidCallback::create(WTF::move(callbackFunction));

    if (!context()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_voidCallbacks.set(callbackID, callback.release());

    context()->sendToDatabaseProcessRelaunchingIfNecessary(Messages::WebOriginDataManager::DeleteEntriesModifiedBetweenDates(types, startDate, endDate, callbackID));
}

void WebOriginDataManagerProxy::didDeleteEntries(IPC::Connection* connection, uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, connection);
    callback->performCallback();
}

void WebOriginDataManagerProxy::deleteAllEntries(WKOriginDataTypes types, std::function<void (CallbackBase::Error)> callbackFunction)
{
    // FIXME: Right now we only support IndexedDatabase data so we know that we're only sending this request to the DatabaseProcess.
    // That's why having one single callback works.
    // In the future when we message N-processes we'll have to wait for all N replies before responding to the client.
    if (!(types & kWKIndexedDatabaseData)) {
        callbackFunction(CallbackBase::Error::None);
        return;
    }

    RefPtr<VoidCallback> callback = VoidCallback::create(WTF::move(callbackFunction));

    if (!context()) {
        callback->invalidate();
        return;
    }

    uint64_t callbackID = callback->callbackID();
    m_voidCallbacks.set(callbackID, callback.release());

    context()->sendToDatabaseProcessRelaunchingIfNecessary(Messages::WebOriginDataManager::DeleteAllEntries(types, callbackID));
}

void WebOriginDataManagerProxy::didDeleteAllEntries(IPC::Connection* connection, uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, connection);
    callback->performCallback();
}

} // namespace WebKit

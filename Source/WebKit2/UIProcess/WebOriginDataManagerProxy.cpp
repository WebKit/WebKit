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
    invalidateCallbackMap(m_arrayCallbacks);
}

void WebOriginDataManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks);
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

void WebOriginDataManagerProxy::getOrigins(WKOriginDataTypes types, PassRefPtr<ArrayCallback> prpCallback)
{
    if (!context())
        return;

    RefPtr<ArrayCallback> callback = prpCallback;

    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Make manipulating cache information work with per-tab WebProcess.
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebOriginDataManager::GetOrigins(types, callbackID));
}

void WebOriginDataManagerProxy::didGetOrigins(const Vector<SecurityOriginData>& originDatas, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    performAPICallbackWithSecurityOriginDataVector(originDatas, callback.get());
}

void WebOriginDataManagerProxy::deleteEntriesForOrigin(WKOriginDataTypes types, WebSecurityOrigin* origin)
{
    if (!context())
        return;

    SecurityOriginData securityOriginData;
    securityOriginData.protocol = origin->protocol();
    securityOriginData.host = origin->host();
    securityOriginData.port = origin->port();

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Make manipulating cache information work with per-tab WebProcess.
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebOriginDataManager::DeleteEntriesForOrigin(types, securityOriginData));
}

void WebOriginDataManagerProxy::deleteAllEntries(WKOriginDataTypes types)
{
    if (!context())
        return;

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Make manipulating cache information work with per-tab WebProcess.
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebOriginDataManager::DeleteAllEntries(types));
}

void WebOriginDataManagerProxy::startObservingChanges(WKOriginDataTypes types)
{
    if (!context())
        return;

    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebOriginDataManager::StartObservingChanges(types));
}

void WebOriginDataManagerProxy::stopObservingChanges(WKOriginDataTypes types)
{
    if (!context())
        return;

    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebOriginDataManager::StartObservingChanges(types));
}

void WebOriginDataManagerProxy::setChangeClient(const WKOriginDataManagerChangeClientBase *client)
{
    m_client.initialize(client);
}

void WebOriginDataManagerProxy::didChange()
{
    m_client.didChange(this);
}

} // namespace WebKit

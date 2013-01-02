/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebKeyValueStorageManagerProxy.h"

#include "SecurityOriginData.h"
#include "WebContext.h"
#include "WebKeyValueStorageManagerMessages.h"
#include "WebKeyValueStorageManagerProxyMessages.h"
#include "WebSecurityOrigin.h"

namespace WebKit {

const AtomicString& WebKeyValueStorageManagerProxy::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("WebKeyValueStorageManagerProxy", AtomicString::ConstructFromLiteral));
    return name;
}

PassRefPtr<WebKeyValueStorageManagerProxy> WebKeyValueStorageManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebKeyValueStorageManagerProxy(context));
}

WebKeyValueStorageManagerProxy::WebKeyValueStorageManagerProxy(WebContext* context)
    : WebContextSupplement(context)
{
    WebContextSupplement::context()->addMessageReceiver(Messages::WebKeyValueStorageManagerProxy::messageReceiverName(), this);
}

WebKeyValueStorageManagerProxy::~WebKeyValueStorageManagerProxy()
{
}

// WebContextSupplement

void WebKeyValueStorageManagerProxy::contextDestroyed()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

void WebKeyValueStorageManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks);
}

bool WebKeyValueStorageManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebKeyValueStorageManagerProxy::refWebContextSupplement()
{
    APIObject::ref();
}

void WebKeyValueStorageManagerProxy::derefWebContextSupplement()
{
    APIObject::deref();
}

// CoreIPC::MessageReceiver

void WebKeyValueStorageManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveWebKeyValueStorageManagerProxyMessage(connection, messageID, decoder);
}

void WebKeyValueStorageManagerProxy::getKeyValueStorageOrigins(PassRefPtr<ArrayCallback> prpCallback)
{
    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Should key-value storage be handled in the web process?
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebKeyValueStorageManager::GetKeyValueStorageOrigins(callbackID));
}
    
void WebKeyValueStorageManagerProxy::didGetKeyValueStorageOrigins(const Vector<SecurityOriginData>& originDatas, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    performAPICallbackWithSecurityOriginDataVector(originDatas, callback.get());
}

void WebKeyValueStorageManagerProxy::deleteEntriesForOrigin(WebSecurityOrigin* origin)
{
    SecurityOriginData securityOriginData;
    securityOriginData.protocol = origin->protocol();
    securityOriginData.host = origin->host();
    securityOriginData.port = origin->port();

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Should key-value storage be handled in the web process?
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebKeyValueStorageManager::DeleteEntriesForOrigin(securityOriginData));
}

void WebKeyValueStorageManagerProxy::deleteAllEntries()
{
    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Should key-value storage be handled in the web process?
    context()->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebKeyValueStorageManager::DeleteAllEntries());
}

} // namespace WebKit

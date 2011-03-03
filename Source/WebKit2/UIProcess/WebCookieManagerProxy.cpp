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
#include "WebCookieManagerProxy.h"

#include "SecurityOriginData.h"
#include "WebCookieManagerMessages.h"
#include "WebContext.h"
#include "WebSecurityOrigin.h"

namespace WebKit {

PassRefPtr<WebCookieManagerProxy> WebCookieManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebCookieManagerProxy(context));
}

WebCookieManagerProxy::WebCookieManagerProxy(WebContext* context)
    : m_webContext(context)
{
}

WebCookieManagerProxy::~WebCookieManagerProxy()
{
}

void WebCookieManagerProxy::invalidate()
{
    invalidateCallbackMap(m_arrayCallbacks);
}

void WebCookieManagerProxy::initializeClient(const WKCookieManagerClient* client)
{
    m_client.initialize(client);
}

void WebCookieManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebCookieManagerProxyMessage(connection, messageID, arguments);
}

void WebCookieManagerProxy::getHostnamesWithCookies(PassRefPtr<ArrayCallback> prpCallback)
{
    ASSERT(m_webContext);

    RefPtr<ArrayCallback> callback = prpCallback;
    if (!m_webContext->hasValidProcess()) {
        callback->invalidate();
        return;
    }
    
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());
    m_webContext->process()->send(Messages::WebCookieManager::GetHostnamesWithCookies(callbackID), 0);
}
    
void WebCookieManagerProxy::didGetHostnamesWithCookies(const Vector<String>& hostnameList, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    size_t hostnameCount = hostnameList.size();
    Vector<RefPtr<APIObject> > hostnames(hostnameCount);

    for (size_t i = 0; i < hostnameCount; ++i)
        hostnames[i] = WebString::create(hostnameList[i]);

    callback->performCallbackWithReturnValue(ImmutableArray::adopt(hostnames).get());
}

void WebCookieManagerProxy::deleteCookiesForHostname(const String& hostname)
{
    ASSERT(m_webContext);
    if (!m_webContext->hasValidProcess())
        return;
    m_webContext->process()->send(Messages::WebCookieManager::DeleteCookiesForHostname(hostname), 0);
}

void WebCookieManagerProxy::deleteAllCookies()
{
    ASSERT(m_webContext);
    if (!m_webContext->hasValidProcess())
        return;
    m_webContext->process()->send(Messages::WebCookieManager::DeleteAllCookies(), 0);
}

void WebCookieManagerProxy::startObservingCookieChanges()
{
    ASSERT(m_webContext);
    if (!m_webContext->hasValidProcess())
        return;
    m_webContext->process()->send(Messages::WebCookieManager::StartObservingCookieChanges(), 0);
}

void WebCookieManagerProxy::stopObservingCookieChanges()
{
    ASSERT(m_webContext);
    if (!m_webContext->hasValidProcess())
        return;
    m_webContext->process()->send(Messages::WebCookieManager::StopObservingCookieChanges(), 0);
}

void WebCookieManagerProxy::cookiesDidChange()
{
    m_client.cookiesDidChange(this);
}

} // namespace WebKit

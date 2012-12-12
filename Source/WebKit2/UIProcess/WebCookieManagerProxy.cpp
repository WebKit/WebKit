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
#include "WebContext.h"
#include "WebCookieManagerMessages.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebSecurityOrigin.h"

namespace WebKit {

PassRefPtr<WebCookieManagerProxy> WebCookieManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebCookieManagerProxy(context));
}

WebCookieManagerProxy::WebCookieManagerProxy(WebContext* context)
    : m_webContext(context)
#if USE(SOUP)
    , m_cookiePersistentStorageType(SoupCookiePersistentStorageSQLite)
#endif
{
    m_webContext->addMessageReceiver(Messages::WebCookieManagerProxy::messageReceiverName(), this);
}

WebCookieManagerProxy::~WebCookieManagerProxy()
{
}

void WebCookieManagerProxy::invalidate()
{
    invalidateCallbackMap(m_arrayCallbacks);
    invalidateCallbackMap(m_httpCookieAcceptPolicyCallbacks);
}

bool WebCookieManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty() && m_httpCookieAcceptPolicyCallbacks.isEmpty();
}

void WebCookieManagerProxy::initializeClient(const WKCookieManagerClient* client)
{
    m_client.initialize(client);
}

void WebCookieManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveWebCookieManagerProxyMessage(connection, messageID, decoder);
}

void WebCookieManagerProxy::getHostnamesWithCookies(PassRefPtr<ArrayCallback> prpCallback)
{
    ASSERT(m_webContext);

    RefPtr<ArrayCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Cookies shouldn't be stored in the web process.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::GetHostnamesWithCookies(callbackID));
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
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::DeleteCookiesForHostname(hostname));
}

void WebCookieManagerProxy::deleteAllCookies()
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::DeleteAllCookies());
}

void WebCookieManagerProxy::startObservingCookieChanges()
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::StartObservingCookieChanges());
}

void WebCookieManagerProxy::stopObservingCookieChanges()
{
    ASSERT(m_webContext);
    m_webContext->sendToAllProcesses(Messages::WebCookieManager::StopObservingCookieChanges());
}

void WebCookieManagerProxy::cookiesDidChange()
{
    m_client.cookiesDidChange(this);
}

void WebCookieManagerProxy::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    ASSERT(m_webContext);
#if PLATFORM(MAC)
    persistHTTPCookieAcceptPolicy(policy);
#endif
#if USE(SOUP)
    m_webContext->setInitialHTTPCookieAcceptPolicy(policy);
#endif

    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(policy));
}

void WebCookieManagerProxy::getHTTPCookieAcceptPolicy(PassRefPtr<HTTPCookieAcceptPolicyCallback> prpCallback)
{
    ASSERT(m_webContext);

    RefPtr<HTTPCookieAcceptPolicyCallback> callback = prpCallback;

    uint64_t callbackID = callback->callbackID();
    m_httpCookieAcceptPolicyCallbacks.set(callbackID, callback.release());

    // FIXME (Multi-WebProcess): <rdar://problem/12240189> Make cookie policies work with per-tab WebProcess.
    // We can't ber asking all web processes for this information.
    m_webContext->sendToAllProcessesRelaunchingThemIfNecessary(Messages::WebCookieManager::GetHTTPCookieAcceptPolicy(callbackID));
}

void WebCookieManagerProxy::didGetHTTPCookieAcceptPolicy(uint32_t policy, uint64_t callbackID)
{
    RefPtr<HTTPCookieAcceptPolicyCallback> callback = m_httpCookieAcceptPolicyCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(policy);
}

} // namespace WebKit

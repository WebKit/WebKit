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
#include "WebCookieManagerProxy.h"

#include "APIArray.h"
#include "APISecurityOrigin.h"
#include "WebCookieManagerMessages.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebProcessPool.h"
#include <WebCore/SecurityOriginData.h>

namespace WebKit {

const char* WebCookieManagerProxy::supplementName()
{
    return "WebCookieManagerProxy";
}

PassRefPtr<WebCookieManagerProxy> WebCookieManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(new WebCookieManagerProxy(processPool));
}

WebCookieManagerProxy::WebCookieManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
#if USE(SOUP)
    , m_cookiePersistentStorageType(SoupCookiePersistentStorageSQLite)
#endif
{
    WebContextSupplement::processPool()->addMessageReceiver(Messages::WebCookieManagerProxy::messageReceiverName(), *this);
}

WebCookieManagerProxy::~WebCookieManagerProxy()
{
}

void WebCookieManagerProxy::initializeClient(const WKCookieManagerClientBase* client)
{
    m_client.initialize(client);
}

// WebContextSupplement

void WebCookieManagerProxy::processPoolDestroyed()
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::OwnerWasInvalidated);
    invalidateCallbackMap(m_httpCookieAcceptPolicyCallbacks, CallbackBase::Error::OwnerWasInvalidated);
}

void WebCookieManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::ProcessExited);
    invalidateCallbackMap(m_httpCookieAcceptPolicyCallbacks, CallbackBase::Error::ProcessExited);
}

void WebCookieManagerProxy::processDidClose(NetworkProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::ProcessExited);
    invalidateCallbackMap(m_httpCookieAcceptPolicyCallbacks, CallbackBase::Error::ProcessExited);
}

void WebCookieManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebCookieManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

void WebCookieManagerProxy::getHostnamesWithCookies(WebCore::SessionID sessionID, std::function<void (API::Array*, CallbackBase::Error)> callbackFunction)
{
    auto callback = ArrayCallback::create(WTFMove(callbackFunction));
    uint64_t callbackID = callback->callbackID();
    m_arrayCallbacks.set(callbackID, WTFMove(callback));

    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::GetHostnamesWithCookies(sessionID, callbackID));
}

void WebCookieManagerProxy::didGetHostnamesWithCookies(const Vector<String>& hostnames, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(API::Array::createStringArray(hostnames).ptr());
}

void WebCookieManagerProxy::deleteCookiesForHostname(WebCore::SessionID sessionID, const String& hostname)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::DeleteCookiesForHostname(sessionID, hostname));
}

void WebCookieManagerProxy::deleteAllCookies(WebCore::SessionID sessionID)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::DeleteAllCookies(sessionID));
}

void WebCookieManagerProxy::deleteAllCookiesModifiedSince(WebCore::SessionID sessionID, std::chrono::system_clock::time_point time)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::DeleteAllCookiesModifiedSince(sessionID, time));
}

void WebCookieManagerProxy::addCookie(WebCore::SessionID sessionID, const WebCore::Cookie& cookie, const String& hostname)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::AddCookie(sessionID, cookie, hostname));
}

void WebCookieManagerProxy::startObservingCookieChanges(WebCore::SessionID sessionID)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::StartObservingCookieChanges(sessionID));
}

void WebCookieManagerProxy::stopObservingCookieChanges(WebCore::SessionID sessionID)
{
    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::StopObservingCookieChanges(sessionID));
}

void WebCookieManagerProxy::setCookieObserverCallback(WebCore::SessionID sessionID, std::function<void ()>&& callback)
{
    if (callback)
        m_cookieObservers.set(sessionID, WTFMove(callback));
    else
        m_cookieObservers.remove(sessionID);
}

void WebCookieManagerProxy::cookiesDidChange(WebCore::SessionID sessionID)
{
    m_client.cookiesDidChange(this);
    if (auto callback = m_cookieObservers.get(sessionID))
        callback();
}

void WebCookieManagerProxy::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
#if PLATFORM(COCOA)
    if (!processPool()->isUsingTestingNetworkSession())
        persistHTTPCookieAcceptPolicy(policy);
#endif
#if USE(SOUP)
    processPool()->setInitialHTTPCookieAcceptPolicy(policy);
#endif

    // The policy is not sent to newly created processes (only Soup does that via setInitialHTTPCookieAcceptPolicy()). This is not a serious problem, because:
    // - When testing, we only have one WebProcess and one NetworkProcess, and WebKitTestRunner never restarts them;
    // - When not testing, Cocoa has the policy persisted, and thus new processes use it (even for ephemeral sessions).
    processPool()->sendToAllProcesses(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(policy));
    processPool()->sendToNetworkingProcess(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(policy));
}

void WebCookieManagerProxy::getHTTPCookieAcceptPolicy(std::function<void (HTTPCookieAcceptPolicy, CallbackBase::Error)> callbackFunction)
{
    auto callback = HTTPCookieAcceptPolicyCallback::create(WTFMove(callbackFunction));

    uint64_t callbackID = callback->callbackID();
    m_httpCookieAcceptPolicyCallbacks.set(callbackID, WTFMove(callback));

    processPool()->sendToNetworkingProcessRelaunchingIfNecessary(Messages::WebCookieManager::GetHTTPCookieAcceptPolicy(callbackID));
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

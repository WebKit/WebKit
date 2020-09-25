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
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "OptionalCallbackID.h"
#include "WebCookieManagerMessages.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebProcessPool.h"
#include <WebCore/Cookie.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

WebCookieManagerProxy::WebCookieManagerProxy(NetworkProcessProxy& networkProcess)
    : m_networkProcess(makeWeakPtr(networkProcess))
{
    networkProcess.addMessageReceiver(Messages::WebCookieManagerProxy::messageReceiverName(), *this);
}

WebCookieManagerProxy::~WebCookieManagerProxy()
{
    if (m_networkProcess)
        m_networkProcess->removeMessageReceiver(Messages::WebCookieManagerProxy::messageReceiverName());
    ASSERT(m_cookieObservers.isEmpty());
}

void WebCookieManagerProxy::initializeClient(const WKCookieManagerClientBase* client)
{
    m_client.initialize(client);
}

void WebCookieManagerProxy::getHostnamesWithCookies(PAL::SessionID sessionID, CompletionHandler<void(Vector<String>&&)>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetHostnamesWithCookies(sessionID), WTFMove(callbackFunction));
    else
        callbackFunction({ });
}

void WebCookieManagerProxy::deleteCookiesForHostnames(PAL::SessionID sessionID, const Vector<String>& hostnames)
{
    if (m_networkProcess)
        m_networkProcess->send(Messages::WebCookieManager::DeleteCookiesForHostnames(sessionID, hostnames), 0);
}

void WebCookieManagerProxy::deleteAllCookies(PAL::SessionID sessionID)
{
    if (m_networkProcess)
        m_networkProcess->send(Messages::WebCookieManager::DeleteAllCookies(sessionID), 0);
}

void WebCookieManagerProxy::deleteCookie(PAL::SessionID sessionID, const Cookie& cookie, CompletionHandler<void()>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteCookie(sessionID, cookie), WTFMove(callbackFunction));
    else
        callbackFunction();
}

void WebCookieManagerProxy::deleteAllCookiesModifiedSince(PAL::SessionID sessionID, WallTime time, CompletionHandler<void()>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteAllCookiesModifiedSince(sessionID, time), WTFMove(callbackFunction));
    else
        callbackFunction();
}

void WebCookieManagerProxy::setCookies(PAL::SessionID sessionID, const Vector<Cookie>& cookies, CompletionHandler<void()>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetCookie(sessionID, cookies), WTFMove(callbackFunction));
    else
        callbackFunction();
}

void WebCookieManagerProxy::setCookies(PAL::SessionID sessionID, const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL, CompletionHandler<void()>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetCookies(sessionID, cookies, url, mainDocumentURL), WTFMove(callbackFunction));
    else
        callbackFunction();
}

void WebCookieManagerProxy::getAllCookies(PAL::SessionID sessionID, CompletionHandler<void(Vector<Cookie>&&)>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetAllCookies(sessionID), WTFMove(callbackFunction));
    else
        callbackFunction({ });
}

void WebCookieManagerProxy::getCookies(PAL::SessionID sessionID, const URL& url, CompletionHandler<void(Vector<Cookie>&&)>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetCookies(sessionID, url), WTFMove(callbackFunction));
    else
        callbackFunction({ });
}

void WebCookieManagerProxy::startObservingCookieChanges(PAL::SessionID sessionID)
{
    if (m_networkProcess)
        m_networkProcess->send(Messages::WebCookieManager::StartObservingCookieChanges(sessionID), 0);
}

void WebCookieManagerProxy::stopObservingCookieChanges(PAL::SessionID sessionID)
{
    if (m_networkProcess)
        m_networkProcess->send(Messages::WebCookieManager::StopObservingCookieChanges(sessionID), 0);
}

void WebCookieManagerProxy::setCookieObserverCallback(PAL::SessionID sessionID, WTF::Function<void ()>&& callback)
{
    if (callback)
        m_legacyCookieObservers.set(sessionID, WTFMove(callback));
    else
        m_legacyCookieObservers.remove(sessionID);
}

void WebCookieManagerProxy::registerObserver(PAL::SessionID sessionID, Observer& observer)
{
    auto result = m_cookieObservers.set(sessionID, HashSet<Observer*>());
    result.iterator->value.add(&observer);

    if (result.isNewEntry)
        startObservingCookieChanges(sessionID);
}

void WebCookieManagerProxy::unregisterObserver(PAL::SessionID sessionID, Observer& observer)
{
    auto iterator = m_cookieObservers.find(sessionID);
    if (iterator == m_cookieObservers.end())
        return;

    iterator->value.remove(&observer);
    if (!iterator->value.isEmpty())
        return;

    m_cookieObservers.remove(iterator);
    stopObservingCookieChanges(sessionID);
}

void WebCookieManagerProxy::cookiesDidChange(PAL::SessionID sessionID)
{
    m_client.cookiesDidChange(this);
    auto legacyIterator = m_legacyCookieObservers.find(sessionID);
    if (legacyIterator != m_legacyCookieObservers.end())
        ((*legacyIterator).value)();

    auto iterator = m_cookieObservers.find(sessionID);
    if (iterator == m_cookieObservers.end())
        return;

    for (auto* observer : iterator->value)
        observer->cookiesDidChange();
}

void WebCookieManagerProxy::setHTTPCookieAcceptPolicy(PAL::SessionID, HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(policy), WTFMove(callbackFunction));
    else
        callbackFunction();
}

void WebCookieManagerProxy::getHTTPCookieAcceptPolicy(PAL::SessionID sessionID, CompletionHandler<void(HTTPCookieAcceptPolicy)>&& callbackFunction)
{
    if (m_networkProcess)
        m_networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetHTTPCookieAcceptPolicy(sessionID), WTFMove(callbackFunction));
    else
        callbackFunction(HTTPCookieAcceptPolicy::Never);
}

} // namespace WebKit

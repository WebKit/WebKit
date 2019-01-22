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
#include "WebCookieManager.h"

#include "ChildProcess.h"
#include "WebCookieManagerMessages.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/Cookie.h>
#include <WebCore/CookieStorage.h>
#include <WebCore/NetworkStorageSession.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

const char* WebCookieManager::supplementName()
{
    return "WebCookieManager";
}

WebCookieManager::WebCookieManager(NetworkProcess& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::WebCookieManager::messageReceiverName(), *this);
}

WebCookieManager::~WebCookieManager() = default;

void WebCookieManager::getHostnamesWithCookies(PAL::SessionID sessionID, CallbackID callbackID)
{
    HashSet<String> hostnames;
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->getHostnamesWithCookies(hostnames);

    m_process.send(Messages::WebCookieManagerProxy::DidGetHostnamesWithCookies(copyToVector(hostnames), callbackID), 0);
}

void WebCookieManager::deleteCookiesForHostname(PAL::SessionID sessionID, const String& hostname)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->deleteCookiesForHostnames({ hostname });
}


void WebCookieManager::deleteAllCookies(PAL::SessionID sessionID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->deleteAllCookies();
}

void WebCookieManager::deleteCookie(PAL::SessionID sessionID, const Cookie& cookie, CallbackID callbackID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->deleteCookie(cookie);

    m_process.send(Messages::WebCookieManagerProxy::DidDeleteCookies(callbackID), 0);
}

void WebCookieManager::deleteAllCookiesModifiedSince(PAL::SessionID sessionID, WallTime time, CallbackID callbackID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->deleteAllCookiesModifiedSince(time);

    m_process.send(Messages::WebCookieManagerProxy::DidDeleteCookies(callbackID), 0);
}

void WebCookieManager::getAllCookies(PAL::SessionID sessionID, CallbackID callbackID)
{
    Vector<Cookie> cookies;
    if (auto* storageSession = m_process.storageSession(sessionID))
        cookies = storageSession->getAllCookies();

    m_process.send(Messages::WebCookieManagerProxy::DidGetCookies(cookies, callbackID), 0);
}

void WebCookieManager::getCookies(PAL::SessionID sessionID, const URL& url, CallbackID callbackID)
{
    Vector<Cookie> cookies;
    if (auto* storageSession = m_process.storageSession(sessionID))
        cookies = storageSession->getCookies(url);

    m_process.send(Messages::WebCookieManagerProxy::DidGetCookies(cookies, callbackID), 0);
}

void WebCookieManager::setCookie(PAL::SessionID sessionID, const Cookie& cookie, CallbackID callbackID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->setCookie(cookie);

    m_process.send(Messages::WebCookieManagerProxy::DidSetCookies(callbackID), 0);
}

void WebCookieManager::setCookies(PAL::SessionID sessionID, const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL, CallbackID callbackID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        storageSession->setCookies(cookies, url, mainDocumentURL);

    m_process.send(Messages::WebCookieManagerProxy::DidSetCookies(callbackID), 0);
}

void WebCookieManager::notifyCookiesDidChange(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());
    m_process.send(Messages::WebCookieManagerProxy::CookiesDidChange(sessionID), 0);
}

void WebCookieManager::startObservingCookieChanges(PAL::SessionID sessionID)
{
    if (auto* storageSession = m_process.storageSession(sessionID)) {
        WebCore::startObservingCookieChanges(*storageSession, [this, sessionID] {
            notifyCookiesDidChange(sessionID);
        });
    }
}

void WebCookieManager::stopObservingCookieChanges(PAL::SessionID sessionID)
{
    if (auto* storageSession = m_process.storageSession(sessionID))
        WebCore::stopObservingCookieChanges(*storageSession);
}

void WebCookieManager::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy, OptionalCallbackID callbackID)
{
    platformSetHTTPCookieAcceptPolicy(policy);

    if (callbackID)
        m_process.send(Messages::WebCookieManagerProxy::DidSetHTTPCookieAcceptPolicy(callbackID.callbackID()), 0);
}

void WebCookieManager::getHTTPCookieAcceptPolicy(CallbackID callbackID)
{
    m_process.send(Messages::WebCookieManagerProxy::DidGetHTTPCookieAcceptPolicy(platformGetHTTPCookieAcceptPolicy(), callbackID), 0);
}

} // namespace WebKit

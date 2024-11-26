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

#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebCookieManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/Cookie.h>
#include <WebCore/CookieStorage.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/NetworkStorageSession.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCookieManager);

ASCIILiteral WebCookieManager::supplementName()
{
    return "WebCookieManager"_s;
}

WebCookieManager::WebCookieManager(NetworkProcess& process)
    : m_process(process)
{
    process.addMessageReceiver(Messages::WebCookieManager::messageReceiverName(), *this);
}

WebCookieManager::~WebCookieManager() = default;

void WebCookieManager::ref() const
{
    m_process->ref();
}

void WebCookieManager::deref() const
{
    m_process->deref();
}

Ref<NetworkProcess> WebCookieManager::protectedProcess()
{
    ASSERT(RunLoop::isMain());
    return m_process.get();
}

void WebCookieManager::getHostnamesWithCookies(PAL::SessionID sessionID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    HashSet<String> hostnames;
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->getHostnamesWithCookies(hostnames);
    completionHandler(copyToVector(hostnames));
}

void WebCookieManager::deleteCookiesForHostnames(PAL::SessionID sessionID, const Vector<String>& hostnames, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->deleteCookiesForHostnames(hostnames, WTFMove(completionHandler));
    else
        completionHandler();
}

void WebCookieManager::deleteAllCookies(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->deleteAllCookies(WTFMove(completionHandler));
    else
        completionHandler();
}

void WebCookieManager::deleteCookie(PAL::SessionID sessionID, const Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->deleteCookie(cookie, WTFMove(completionHandler));
    else
        completionHandler();
}

void WebCookieManager::deleteAllCookiesModifiedSince(PAL::SessionID sessionID, WallTime time, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->deleteAllCookiesModifiedSince(time, WTFMove(completionHandler));
    else
        completionHandler();
}

void WebCookieManager::getAllCookies(PAL::SessionID sessionID, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    Vector<Cookie> cookies;
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        cookies = storageSession->getAllCookies();
    completionHandler(WTFMove(cookies));
}

void WebCookieManager::getCookies(PAL::SessionID sessionID, const URL& url, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    Vector<Cookie> cookies;
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        cookies = storageSession->getCookies(url);
    completionHandler(WTFMove(cookies));
}

void WebCookieManager::setCookie(PAL::SessionID sessionID, const Vector<Cookie>& cookies, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID)) {
        for (auto& cookie : cookies)
            storageSession->setCookie(cookie);
    }
    completionHandler();
}

void WebCookieManager::setCookies(PAL::SessionID sessionID, const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL, CompletionHandler<void()>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->setCookies(cookies, url, mainDocumentURL);
    completionHandler();
}

void WebCookieManager::notifyCookiesDidChange(PAL::SessionID sessionID)
{
    ASSERT(RunLoop::isMain());
    protectedProcess()->send(Messages::NetworkProcessProxy::CookiesDidChange(sessionID), 0);
}

void WebCookieManager::startObservingCookieChanges(PAL::SessionID sessionID)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID)) {
        WebCore::startObservingCookieChanges(*storageSession, [this, sessionID] {
            notifyCookiesDidChange(sessionID);
        });
    }
}

void WebCookieManager::stopObservingCookieChanges(PAL::SessionID sessionID)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        WebCore::stopObservingCookieChanges(*storageSession);
}

void WebCookieManager::setHTTPCookieAcceptPolicy(PAL::SessionID sessionID, HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(Storage, "WebCookieManager::setHTTPCookieAcceptPolicy set policy %d for session %" PRIu64, static_cast<int>(policy), sessionID.toUInt64());
    platformSetHTTPCookieAcceptPolicy(sessionID, policy, [policy, process = protectedProcess(), completionHandler = WTFMove(completionHandler)] () mutable {
        process->cookieAcceptPolicyChanged(policy);
        completionHandler();
    });
}

void WebCookieManager::getHTTPCookieAcceptPolicy(PAL::SessionID sessionID, CompletionHandler<void(HTTPCookieAcceptPolicy)>&& completionHandler)
{
    if (auto* storageSession = protectedProcess()->storageSession(sessionID))
        completionHandler(storageSession->cookieAcceptPolicy());
    else
        completionHandler(HTTPCookieAcceptPolicy::Never);
}

} // namespace WebKit

/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "APIHTTPCookieStore.h"

#include "NetworkProcessMessages.h"
#include "WebCookieManagerMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/Cookie.h>
#include <WebCore/CookieStorage.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/NetworkStorageSession.h>
#include <wtf/CallbackAggregator.h>

#if PLATFORM(IOS_FAMILY)
#include "DefaultWebBrowserChecks.h"
#endif

using namespace WebKit;

namespace API {

HTTPCookieStore::HTTPCookieStore(WebKit::WebsiteDataStore& websiteDataStore)
    : m_sessionID(websiteDataStore.sessionID())
    , m_owningDataStore(websiteDataStore)
{
}

HTTPCookieStore::~HTTPCookieStore()
{
    ASSERT(m_observers.computesEmpty());
}

void HTTPCookieStore::filterAppBoundCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (!m_owningDataStore)
        return completionHandler({ });
    m_owningDataStore->getAppBoundDomains([cookies = WTFMove(cookies), completionHandler = WTFMove(completionHandler)] (auto& domains) mutable {
        Vector<WebCore::Cookie> appBoundCookies;
        if (!domains.isEmpty() && !isFullWebBrowser()) {
            for (auto& cookie : WTFMove(cookies)) {
                if (domains.contains(WebCore::RegistrableDomain::uncheckedCreateFromHost(cookie.domain)))
                    appBoundCookies.append(WTFMove(cookie));
            }
        } else
            appBoundCookies = WTFMove(cookies);
        completionHandler(WTFMove(appBoundCookies));
    });
#else
    completionHandler(WTFMove(cookies));
#endif
}

void HTTPCookieStore::cookies(CompletionHandler<void(const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists()) {
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetAllCookies(m_sessionID), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
            filterAppBoundCookies(WTFMove(cookies), WTFMove(completionHandler));
        });
    } else
        completionHandler({ });
}

void HTTPCookieStore::cookiesForURL(WTF::URL&& url, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists()) {
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetCookies(m_sessionID, url), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
            filterAppBoundCookies(WTFMove(cookies), WTFMove(completionHandler));
        });
    } else
        completionHandler({ });
}

void HTTPCookieStore::setCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void()>&& completionHandler)
{
    filterAppBoundCookies(WTFMove(cookies), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (auto&& appBoundCookies) mutable {
        if (auto* networkProcess = networkProcessLaunchingIfNecessary())
            networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetCookie(m_sessionID, appBoundCookies), WTFMove(completionHandler));
        else
            completionHandler();
    });
}

void HTTPCookieStore::deleteCookie(const WebCore::Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteCookie(m_sessionID, cookie), WTFMove(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    if (m_owningDataStore) {
        for (auto& processPool : m_owningDataStore->processPools()) {
            processPool->forEachProcessForSession(m_sessionID, [&](auto& process) {
                if (!process.canSendMessage())
                    return;
                process.sendWithAsyncReply(Messages::WebProcess::DeleteAllCookies(), [callbackAggregator] { });
            });
        }
    }
    if (auto* networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteAllCookies(m_sessionID), [callbackAggregator] { });
}

void HTTPCookieStore::deleteCookiesForHostnames(const Vector<WTF::String>& hostnames, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteCookiesForHostnames(m_sessionID, hostnames), WTFMove(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(policy), WTFMove(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::getHTTPCookieAcceptPolicy(CompletionHandler<void(const WebCore::HTTPCookieAcceptPolicy&)>&& completionHandler)
{
    if (auto* networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetHTTPCookieAcceptPolicy(m_sessionID), WTFMove(completionHandler));
    else
        completionHandler({ });
}

void HTTPCookieStore::flushCookies(CompletionHandler<void()>&& completionHandler)
{
    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::NetworkProcess::FlushCookies(m_sessionID), WTFMove(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::registerObserver(Observer& observer)
{
    bool wasObserving = !m_observers.computesEmpty();
    m_observers.add(observer);
    if (wasObserving)
        return;

    if (auto* networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->send(Messages::WebCookieManager::StartObservingCookieChanges(m_sessionID), 0);
}

void HTTPCookieStore::unregisterObserver(Observer& observer)
{
    m_observers.remove(observer);
    if (!m_observers.computesEmpty())
        return;

    if (auto* networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::WebCookieManager::StopObservingCookieChanges(m_sessionID), 0);
}

void HTTPCookieStore::cookiesDidChange()
{
    for (auto& observer : m_observers)
        observer.cookiesDidChange(*this);
}

WebKit::NetworkProcessProxy* HTTPCookieStore::networkProcessIfExists()
{
    if (!m_owningDataStore)
        return nullptr;
    return m_owningDataStore->networkProcessIfExists();
}

WebKit::NetworkProcessProxy* HTTPCookieStore::networkProcessLaunchingIfNecessary()
{
    if (!m_owningDataStore)
        return nullptr;
    return &m_owningDataStore->networkProcess();
}

} // namespace API

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
#include <WebCore/Cookie.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>

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
    ASSERT(m_observers.isEmptyIgnoringNullReferences());
}

#if ENABLE(APP_BOUND_DOMAINS)
static Vector<WebCore::Cookie> filterCookiesByAppBoundDomains(Vector<WebCore::Cookie>&& cookies, const HashSet<WebCore::RegistrableDomain>& appBoundDomains)
{
    if (appBoundDomains.isEmpty())
        return cookies;

    return WTF::compactMap(WTF::move(cookies), [&](auto&& cookie) -> std::optional<WebCore::Cookie> {
        if (appBoundDomains.contains(WebCore::RegistrableDomain::uncheckedCreateFromHost(cookie.domain)))
            return WTF::move(cookie);
        return std::nullopt;
    });
}
#endif

void HTTPCookieStore::filterAppBoundCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (!m_owningDataStore)
        return completionHandler({ });
    protect(m_owningDataStore)->getAppBoundDomains([cookies = WTF::move(cookies), completionHandler = WTF::move(completionHandler)] (auto& domains) mutable {
        HashSet<WebCore::RegistrableDomain> appBoundDomains;
        if (!isFullWebBrowserOrRunningTest())
            appBoundDomains = domains;
        completionHandler(filterCookiesByAppBoundDomains(WTF::move(cookies), appBoundDomains));
    });
#else
    completionHandler(WTF::move(cookies));
#endif
}

template<typename Message>
void HTTPCookieStore::fetchCookies(Message&& message, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler, RefPtr<WorkQueue> replyQueue)
{
    RefPtr networkProcess = networkProcessLaunchingIfNecessary();
    if (!networkProcess) {
        if (replyQueue)
            replyQueue->dispatch([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler({ }); });
        else
            completionHandler({ });
        return;
    }

    if (!replyQueue) {
        // Existing main-thread path: reply lands on main, then filter on main.
        networkProcess->sendWithAsyncReply(std::forward<Message>(message), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
            filterAppBoundCookies(WTF::move(cookies), WTF::move(completionHandler));
        });
        return;
    }

#if ENABLE(APP_BOUND_DOMAINS)
    // Resolve app-bound domains on the main thread first, then send the IPC with the reply
    // dispatched on replyQueue. The filtering itself happens on replyQueue using the captured set.
    RefPtr dataStore = m_owningDataStore;
    if (!dataStore) {
        replyQueue->dispatch([completionHandler = WTF::move(completionHandler)]() mutable { completionHandler({ }); });
        return;
    }
    dataStore->getAppBoundDomains([message = std::forward<Message>(message), completionHandler = WTF::move(completionHandler), replyQueue = WTF::move(replyQueue), networkProcess = WTF::move(networkProcess)] (auto& domains) mutable {
        HashSet<WebCore::RegistrableDomain> appBoundDomains;
        if (!domains.isEmpty() && !isFullWebBrowserOrRunningTest())
            appBoundDomains = crossThreadCopy(domains);
        networkProcess->sendWithAsyncReplyOnDispatcher(WTF::move(message), *replyQueue, [completionHandler = WTF::move(completionHandler), appBoundDomains = WTF::move(appBoundDomains), replyQueue = protect(*replyQueue)] (Vector<WebCore::Cookie>&& cookies) mutable {
            assertIsCurrent(replyQueue);
            completionHandler(filterCookiesByAppBoundDomains(WTF::move(cookies), appBoundDomains));
        });
    });
#else
    // No app-bound filtering needed. Send IPC with reply directly on replyQueue.
    networkProcess->sendWithAsyncReplyOnDispatcher(std::forward<Message>(message), *replyQueue, WTF::move(completionHandler));
#endif
}

void HTTPCookieStore::cookies(CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler, RefPtr<WorkQueue> replyQueue)
{
    fetchCookies(Messages::WebCookieManager::GetAllCookies(m_sessionID), WTF::move(completionHandler), WTF::move(replyQueue));
}

void HTTPCookieStore::cookiesForURL(WTF::URL&& url, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler, RefPtr<WorkQueue> replyQueue)
{
    fetchCookies(Messages::WebCookieManager::GetCookies(m_sessionID, WTF::move(url)), WTF::move(completionHandler), WTF::move(replyQueue));
}

void HTTPCookieStore::setCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void()>&& completionHandler)
{
    filterAppBoundCookies(WTF::move(cookies), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (auto&& appBoundCookies) mutable {
        if (RefPtr dataStore = m_owningDataStore.get())
            dataStore->setCookies(WTF::move(appBoundCookies), WTF::move(completionHandler));
        else
            completionHandler();
    });
}

void HTTPCookieStore::deleteCookie(const WebCore::Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteCookie(m_sessionID, cookie), WTF::move(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTF::move(completionHandler));

    if (RefPtr dataStore = m_owningDataStore.get()) {
        for (auto& processPool : dataStore->processPools()) {
            processPool->forEachProcessForSession(m_sessionID, [&](auto& process) {
                if (!process.canSendMessage())
                    return;
                process.sendWithAsyncReply(Messages::WebProcess::DeleteAllCookies(), [callbackAggregator] { });
            });
        }
    }
    if (RefPtr networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteAllCookies(m_sessionID), [callbackAggregator] { });
}

void HTTPCookieStore::deleteCookiesForHostnames(const Vector<WTF::String>& hostnames, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::DeleteCookiesForHostnames(m_sessionID, hostnames), WTF::move(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::SetHTTPCookieAcceptPolicy(m_sessionID, policy), WTF::move(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::getHTTPCookieAcceptPolicy(CompletionHandler<void(const WebCore::HTTPCookieAcceptPolicy&)>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->sendWithAsyncReply(Messages::WebCookieManager::GetHTTPCookieAcceptPolicy(m_sessionID), WTF::move(completionHandler));
    else
        completionHandler({ });
}

void HTTPCookieStore::flushCookies(CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->sendWithAsyncReply(Messages::NetworkProcess::FlushCookies(m_sessionID), WTF::move(completionHandler));
    else
        completionHandler();
}

void HTTPCookieStore::registerObserver(HTTPCookieStoreObserver& observer)
{
    bool wasObserving = !m_observers.isEmptyIgnoringNullReferences();
    m_observers.add(observer);
    if (wasObserving)
        return;

    if (RefPtr networkProcess = networkProcessLaunchingIfNecessary())
        networkProcess->send(Messages::WebCookieManager::StartObservingCookieChanges(m_sessionID), 0);
}

void HTTPCookieStore::unregisterObserver(HTTPCookieStoreObserver& observer)
{
    m_observers.remove(observer);
    if (!m_observers.isEmptyIgnoringNullReferences())
        return;

    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::WebCookieManager::StopObservingCookieChanges(m_sessionID), 0);
}

bool HTTPCookieStore::isOptInCookiePartitioningEnabled() const
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    if (RefPtr dataStore = m_owningDataStore.get())
        return dataStore->computeIsOptInCookiePartitioningEnabled();
#endif
    return false;
}

void HTTPCookieStore::cookiesDidChange()
{
    for (Ref observer : m_observers)
        observer->cookiesDidChange(*this);
}

WebKit::NetworkProcessProxy* HTTPCookieStore::networkProcessIfExists()
{
    if (auto* dataStore = m_owningDataStore.get())
        return dataStore->networkProcessIfExists();
    return nullptr;
}

WebKit::NetworkProcessProxy* HTTPCookieStore::networkProcessLaunchingIfNecessary()
{
    if (RefPtr dataStore = m_owningDataStore.get())
        return &dataStore->networkProcess();
    return nullptr;
}

} // namespace API

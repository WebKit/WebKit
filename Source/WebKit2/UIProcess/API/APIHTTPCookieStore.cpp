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

#include "APIWebsiteDataStore.h"
#include "WebCookieManagerProxy.h"
#include "WebProcessPool.h"
#include <WebCore/Cookie.h>

using namespace WebKit;

namespace API {

HTTPCookieStore::HTTPCookieStore(WebsiteDataStore& websiteDataStore)
    : m_owningDataStore(websiteDataStore)
{
}

HTTPCookieStore::~HTTPCookieStore()
{
    ASSERT(m_observers.isEmpty());
    ASSERT(!m_observedCookieManagerProxy);
    ASSERT(!m_cookieManagerProxyObserver);

    unregisterForNewProcessPoolNotifications();
}

void HTTPCookieStore::cookies(Function<void (const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->getAllCookies(dataStore.sessionID(), [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](const Vector<WebCore::Cookie>& cookies, CallbackBase::Error error) {
        completionHandler(cookies);
    });
}

void HTTPCookieStore::setCookie(const WebCore::Cookie& cookie, Function<void ()>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->setCookie(dataStore.sessionID(), cookie, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](CallbackBase::Error error) {
        completionHandler();
    });
}

void HTTPCookieStore::deleteCookie(const WebCore::Cookie& cookie, Function<void ()>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->deleteCookie(dataStore.sessionID(), cookie, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](CallbackBase::Error error) {
        completionHandler();
    });
}

class APIWebCookieManagerProxyObserver : public WebCookieManagerProxy::Observer {
public:
    explicit APIWebCookieManagerProxyObserver(API::HTTPCookieStore& cookieStore)
        : m_cookieStore(cookieStore)
    {
    }

private:
    void cookiesDidChange() final
    {
        m_cookieStore.cookiesDidChange();
    }

    void managerDestroyed() final
    {
        m_cookieStore.cookieManagerDestroyed();
    }

    API::HTTPCookieStore& m_cookieStore;
};

void HTTPCookieStore::registerObserver(Observer& observer)
{
    m_observers.add(&observer);

    if (m_cookieManagerProxyObserver)
        return;

    ASSERT(!m_observedCookieManagerProxy);

    m_cookieManagerProxyObserver = std::make_unique<APIWebCookieManagerProxyObserver>(*this);

    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto* pool = dataStore.processPoolForCookieStorageNotifications();

    if (!pool) {
        registerForNewProcessPoolNotifications();
        return;
    }

    m_observedCookieManagerProxy = pool->supplement<WebKit::WebCookieManagerProxy>();
    m_observedCookieManagerProxy->registerObserver(dataStore.sessionID(), *m_cookieManagerProxyObserver);
}

void HTTPCookieStore::unregisterObserver(Observer& observer)
{
    m_observers.remove(&observer);

    if (!m_observers.isEmpty())
        return;

    if (m_observedCookieManagerProxy)
        m_observedCookieManagerProxy->unregisterObserver(m_owningDataStore.websiteDataStore().sessionID(), *m_cookieManagerProxyObserver);

    if (m_processPoolCreationListenerIdentifier)
        WebProcessPool::unregisterProcessPoolCreationListener(m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = 0;
    m_observedCookieManagerProxy = nullptr;
    m_cookieManagerProxyObserver = nullptr;
}

void HTTPCookieStore::cookiesDidChange()
{
    for (auto* observer : m_observers)
        observer->cookiesDidChange(*this);
}

void HTTPCookieStore::cookieManagerDestroyed()
{
    auto& dataStore = m_owningDataStore.websiteDataStore();

    m_observedCookieManagerProxy->unregisterObserver(dataStore.sessionID(), *m_cookieManagerProxyObserver);
    m_observedCookieManagerProxy = nullptr;

    auto* pool = dataStore.processPoolForCookieStorageNotifications();

    if (!pool) {
        registerForNewProcessPoolNotifications();
        return;
    }

    m_observedCookieManagerProxy = pool->supplement<WebKit::WebCookieManagerProxy>();
    m_observedCookieManagerProxy->registerObserver(dataStore.sessionID(), *m_cookieManagerProxyObserver);
}

void HTTPCookieStore::registerForNewProcessPoolNotifications()
{
    ASSERT(!m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = WebProcessPool::registerProcessPoolCreationListener([this](WebProcessPool& newProcessPool) {
        ASSERT(m_cookieManagerProxyObserver);

        if (!m_owningDataStore.websiteDataStore().isAssociatedProcessPool(newProcessPool))
            return;

        m_observedCookieManagerProxy = newProcessPool.supplement<WebKit::WebCookieManagerProxy>();
        m_observedCookieManagerProxy->registerObserver(m_owningDataStore.websiteDataStore().sessionID(), *m_cookieManagerProxyObserver);
        unregisterForNewProcessPoolNotifications();
    });
}

void HTTPCookieStore::unregisterForNewProcessPoolNotifications()
{
    if (m_processPoolCreationListenerIdentifier)
        WebProcessPool::unregisterProcessPoolCreationListener(m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = 0;
}

} // namespace API

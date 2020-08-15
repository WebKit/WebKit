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

#if PLATFORM(IOS_FAMILY)
#include "DefaultWebBrowserChecks.h"
#endif

#include "WebCookieManagerProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/Cookie.h>
#include <WebCore/CookieStorage.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/NetworkStorageSession.h>

using namespace WebKit;

namespace API {

HTTPCookieStore::HTTPCookieStore(WebKit::WebsiteDataStore& websiteDataStore)
    : m_owningDataStore(websiteDataStore)
{
    if (!m_owningDataStore->processPoolForCookieStorageOperations())
        registerForNewProcessPoolNotifications();
}

HTTPCookieStore::~HTTPCookieStore()
{
    ASSERT(m_observers.isEmpty());
    ASSERT(!m_observedCookieManagerProxy);
    ASSERT(!m_cookieManagerProxyObserver);

    unregisterForNewProcessPoolNotifications();
}

void HTTPCookieStore::filterAppBoundCookies(const Vector<WebCore::Cookie>& cookies, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    Vector<WebCore::Cookie> appBoundCookies;
#if PLATFORM(IOS_FAMILY)
    m_owningDataStore->getAppBoundDomains([cookies, appBoundCookies = WTFMove(appBoundCookies), completionHandler = WTFMove(completionHandler)] (auto& domains) mutable {
        if (!domains.isEmpty() && !isFullWebBrowser()) {
            for (auto& cookie : cookies) {
                if (domains.contains(WebCore::RegistrableDomain::uncheckedCreateFromHost(cookie.domain)))
                    appBoundCookies.append(cookie);
            }
        } else
            appBoundCookies = cookies;
        completionHandler(WTFMove(appBoundCookies));
    });
#else
    appBoundCookies = cookies;
    completionHandler(WTFMove(appBoundCookies));
#endif
}

void HTTPCookieStore::cookies(CompletionHandler<void(const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
    if (!pool) {
        Vector<WebCore::Cookie> allCookies;
        if (m_owningDataStore->sessionID() == PAL::SessionID::defaultSessionID())
            allCookies = getAllDefaultUIProcessCookieStoreCookies();
        allCookies.appendVector(m_owningDataStore->pendingCookies());

        RunLoop::main().dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), allCookies] () mutable {
            filterAppBoundCookies(allCookies, WTFMove(completionHandler));
        });
        return;
    }

    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
    cookieManager->getAllCookies(m_owningDataStore->sessionID(), [this, protectedThis = makeRef(*this), pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)] (const Vector<WebCore::Cookie>& cookies) mutable {
        filterAppBoundCookies(cookies, WTFMove(completionHandler));
    });
}

void HTTPCookieStore::cookiesForURL(WTF::URL&& url, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
    if (!pool)
        return completionHandler({ });

    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
    cookieManager->getCookies(m_owningDataStore->sessionID(), url, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
        filterAppBoundCookies(cookies, WTFMove(completionHandler));
    });
}

void HTTPCookieStore::setCookies(const Vector<WebCore::Cookie>& cookies, CompletionHandler<void()>&& completionHandler)
{
    filterAppBoundCookies(cookies, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (auto&& appBoundCookies) mutable {
        auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
        if (!pool) {
            for (auto& cookie : appBoundCookies) {
                // FIXME: pendingCookies used for defaultSession because session cookies cannot be propagated to Network Process with uiProcessCookieStorageIdentifier.
                if (m_owningDataStore->sessionID() == PAL::SessionID::defaultSessionID() && !cookie.session)
                    setCookieInDefaultUIProcessCookieStore(cookie);
                else
                    m_owningDataStore->addPendingCookie(cookie);
            }

            RunLoop::main().dispatch(WTFMove(completionHandler));
            return;
        }

        auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
        cookieManager->setCookies(m_owningDataStore->sessionID(), appBoundCookies, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler();
        });
    });
}

void HTTPCookieStore::deleteCookie(const WebCore::Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
    if (!pool) {
        if (m_owningDataStore->sessionID() == PAL::SessionID::defaultSessionID() && !cookie.session)
            deleteCookieFromDefaultUIProcessCookieStore(cookie);
        else
            m_owningDataStore->removePendingCookie(cookie);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler();
        });
        return;
    }

    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
    cookieManager->deleteCookie(m_owningDataStore->sessionID(), cookie, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void HTTPCookieStore::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
    if (!pool) {
        if (!m_owningDataStore->sessionID().isEphemeral())
            deleteCookiesInDefaultUIProcessCookieStore();
        RunLoop::main().dispatch(WTFMove(completionHandler));
        return;
    }

    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
    cookieManager->deleteAllCookies(m_owningDataStore->sessionID());
    // FIXME: The CompletionHandler should be passed to WebCookieManagerProxy::deleteAllCookies.
    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void HTTPCookieStore::setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();
    if (!pool) {
        if (!m_owningDataStore->sessionID().isEphemeral())
            setHTTPCookieAcceptPolicyInDefaultUIProcessCookieStore(policy);
        RunLoop::main().dispatch(WTFMove(completionHandler));
        return;
    }

    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();
    cookieManager->setHTTPCookieAcceptPolicy(m_owningDataStore->sessionID(), policy, [completionHandler = WTFMove(completionHandler)] () mutable {
        completionHandler();
    });
}

class APIWebCookieManagerProxyObserver : public WebCookieManagerProxy::Observer {
    WTF_MAKE_FAST_ALLOCATED;
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

    m_cookieManagerProxyObserver = makeUnique<APIWebCookieManagerProxyObserver>(*this);

    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();

    if (!pool) {
        ASSERT(!m_observingUIProcessCookies);

        // Listen for cookie notifications in the UIProcess in the meantime.
        startObservingChangesToDefaultUIProcessCookieStore([this] () {
            cookiesDidChange();
        });

        m_observingUIProcessCookies = true;
        return;
    }

    m_observedCookieManagerProxy = pool->supplement<WebKit::WebCookieManagerProxy>();
    m_observedCookieManagerProxy->registerObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);
}

void HTTPCookieStore::unregisterObserver(Observer& observer)
{
    m_observers.remove(&observer);

    if (!m_observers.isEmpty())
        return;

    if (m_observedCookieManagerProxy)
        m_observedCookieManagerProxy->unregisterObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);

    if (m_observingUIProcessCookies)
        stopObservingChangesToDefaultUIProcessCookieStore();

    if (m_processPoolCreationListenerIdentifier)
        WebProcessPool::unregisterProcessPoolCreationListener(m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = 0;
    m_observedCookieManagerProxy = nullptr;
    m_cookieManagerProxyObserver = nullptr;
    m_observingUIProcessCookies = false;
}

void HTTPCookieStore::cookiesDidChange()
{
    for (auto* observer : m_observers)
        observer->cookiesDidChange(*this);
}

void HTTPCookieStore::cookieManagerDestroyed()
{
    m_observedCookieManagerProxy->unregisterObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);
    m_observedCookieManagerProxy = nullptr;

    auto* pool = m_owningDataStore->processPoolForCookieStorageOperations();

    if (!pool)
        return;

    m_observedCookieManagerProxy = pool->supplement<WebKit::WebCookieManagerProxy>();
    m_observedCookieManagerProxy->registerObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);
}

void HTTPCookieStore::registerForNewProcessPoolNotifications()
{
    ASSERT(!m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = WebProcessPool::registerProcessPoolCreationListener([this](WebProcessPool& newProcessPool) {
        if (!m_owningDataStore->isAssociatedProcessPool(newProcessPool))
            return;

        // Now that an associated process pool exists, we need to flush the UI process cookie store
        // to make sure any changes are reflected within the new process pool.
        flushDefaultUIProcessCookieStore();
        newProcessPool.ensureNetworkProcess();

        if (m_cookieManagerProxyObserver) {
            m_observedCookieManagerProxy = newProcessPool.supplement<WebKit::WebCookieManagerProxy>();
            m_observedCookieManagerProxy->registerObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);
        }
        unregisterForNewProcessPoolNotifications();
    });
}

void HTTPCookieStore::unregisterForNewProcessPoolNotifications()
{
    if (m_processPoolCreationListenerIdentifier)
        WebProcessPool::unregisterProcessPoolCreationListener(m_processPoolCreationListenerIdentifier);

    m_processPoolCreationListenerIdentifier = 0;
}

#if !PLATFORM(COCOA)
void HTTPCookieStore::flushDefaultUIProcessCookieStore() { }
Vector<WebCore::Cookie> HTTPCookieStore::getAllDefaultUIProcessCookieStoreCookies() { return { }; }
void HTTPCookieStore::setCookieInDefaultUIProcessCookieStore(const WebCore::Cookie&) { }
void HTTPCookieStore::deleteCookieFromDefaultUIProcessCookieStore(const WebCore::Cookie&) { }
void HTTPCookieStore::startObservingChangesToDefaultUIProcessCookieStore(Function<void()>&&) { }
void HTTPCookieStore::stopObservingChangesToDefaultUIProcessCookieStore() { }
void HTTPCookieStore::deleteCookiesInDefaultUIProcessCookieStore() { }
void HTTPCookieStore::setHTTPCookieAcceptPolicyInDefaultUIProcessCookieStore(WebCore::HTTPCookieAcceptPolicy) { }
#endif
    
} // namespace API

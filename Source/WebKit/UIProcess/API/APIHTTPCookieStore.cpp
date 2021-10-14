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
}

HTTPCookieStore::~HTTPCookieStore()
{
    ASSERT(m_observers.computesEmpty());
    ASSERT(!m_observedCookieManagerProxy);
    ASSERT(!m_cookieManagerProxyObserver);
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
    if (!m_owningDataStore)
        return completionHandler({ });
    auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
    cookieManager.getAllCookies(m_owningDataStore->sessionID(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
        filterAppBoundCookies(WTFMove(cookies), WTFMove(completionHandler));
    });
}

void HTTPCookieStore::cookiesForURL(WTF::URL&& url, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    if (!m_owningDataStore)
        return completionHandler({ });
    auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
    cookieManager.getCookies(m_owningDataStore->sessionID(), url, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<WebCore::Cookie>&& cookies) mutable {
        filterAppBoundCookies(WTFMove(cookies), WTFMove(completionHandler));
    });
}

void HTTPCookieStore::setCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void()>&& completionHandler)
{
    filterAppBoundCookies(WTFMove(cookies), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (auto&& appBoundCookies) mutable {
        if (!m_owningDataStore)
            return;
        auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
        cookieManager.setCookies(m_owningDataStore->sessionID(), appBoundCookies, WTFMove(completionHandler));
    });
}

void HTTPCookieStore::deleteCookie(const WebCore::Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    if (!m_owningDataStore)
        return completionHandler();
    auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
    cookieManager.deleteCookie(m_owningDataStore->sessionID(), cookie, WTFMove(completionHandler));
}

void HTTPCookieStore::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    if (!m_owningDataStore)
        return completionHandler();
    auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
    cookieManager.deleteAllCookies(m_owningDataStore->sessionID());
    // FIXME: The CompletionHandler should be passed to WebCookieManagerProxy::deleteAllCookies.
    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void HTTPCookieStore::setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    if (!m_owningDataStore)
        return completionHandler();
    auto& cookieManager = m_owningDataStore->networkProcess().cookieManager();
    cookieManager.setHTTPCookieAcceptPolicy(m_owningDataStore->sessionID(), policy, WTFMove(completionHandler));
}

void HTTPCookieStore::flushCookies(CompletionHandler<void()>&& completionHandler)
{
    if (!m_owningDataStore)
        return completionHandler();
    m_owningDataStore->flushCookies(WTFMove(completionHandler));
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

    API::HTTPCookieStore& m_cookieStore;
};

void HTTPCookieStore::registerObserver(Observer& observer)
{
    m_observers.add(observer);

    if (m_cookieManagerProxyObserver || !m_owningDataStore)
        return;

    ASSERT(!m_observedCookieManagerProxy);

    m_cookieManagerProxyObserver = makeUnique<APIWebCookieManagerProxyObserver>(*this);

    m_observedCookieManagerProxy = m_owningDataStore->networkProcess().cookieManager();
    m_observedCookieManagerProxy->registerObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);
}

void HTTPCookieStore::unregisterObserver(Observer& observer)
{
    m_observers.remove(observer);

    if (!m_observers.computesEmpty())
        return;

    if (m_observedCookieManagerProxy && m_owningDataStore)
        m_observedCookieManagerProxy->unregisterObserver(m_owningDataStore->sessionID(), *m_cookieManagerProxyObserver);

    m_observedCookieManagerProxy = nullptr;
    m_cookieManagerProxyObserver = nullptr;
}

void HTTPCookieStore::cookiesDidChange()
{
    for (auto& observer : m_observers)
        observer.cookiesDidChange(*this);
}
    
} // namespace API

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

void HTTPCookieStore::cookies(const WebCore::URL& url, Function<void (const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->getCookies(dataStore.sessionID(), url, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](const Vector<WebCore::Cookie>& cookies, CallbackBase::Error error) {
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

void HTTPCookieStore::setCookies(const Vector<WebCore::Cookie>& cookies, const WebCore::URL& url, const WebCore::URL& mainDocumentURL, Function<void ()>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->setCookies(dataStore.sessionID(), cookies, url, mainDocumentURL, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](CallbackBase::Error error) {
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

void HTTPCookieStore::removeCookiesSinceDate(std::chrono::system_clock::time_point date, Function<void ()>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->deleteAllCookiesModifiedSince(dataStore.sessionID(), date, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](CallbackBase::Error error) {
        completionHandler();
    });
}

void HTTPCookieStore::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy, Function<void ()>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->setHTTPCookieAcceptPolicy(dataStore.sessionID(), policy, [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](CallbackBase::Error error) {
        completionHandler();
    });
}

void HTTPCookieStore::getHTTPCookieAcceptPolicy(Function<void (HTTPCookieAcceptPolicy)>&& completionHandler)
{
    auto& dataStore = m_owningDataStore.websiteDataStore();
    auto pool = dataStore.processPoolForCookieStorageOperations();
    auto* cookieManager = pool->supplement<WebKit::WebCookieManagerProxy>();

    cookieManager->getHTTPCookieAcceptPolicy(dataStore.sessionID(), [pool = WTFMove(pool), completionHandler = WTFMove(completionHandler)](HTTPCookieAcceptPolicy policy, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None)
            policy = HTTPCookieAcceptPolicyNever;

        completionHandler(policy);
    });
}

} // namespace API

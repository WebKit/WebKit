/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "BidiStorageAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"

namespace WebKit {

using namespace Inspector;
using PartitionKey = Inspector::Protocol::BidiStorage::PartitionKey;
using PartialCookie = Inspector::Protocol::BidiStorage::PartialCookie;
using CookieFilter = Inspector::Protocol::BidiStorage::CookieFilter;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiStorageAgent);

BidiStorageAgent::BidiStorageAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_storageDomainDispatcher(BidiStorageBackendDispatcher::create(backendDispatcher, this))
{
}

BidiStorageAgent::~BidiStorageAgent() = default;

static Ref<PartialCookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    auto partialCookie = PartialCookie::create()
        .setValue(cookie.value)
        .setName(cookie.name)
        .setDomain(cookie.domain)
        .release();

    partialCookie->setPath(cookie.path);
    partialCookie->setHttpOnly(cookie.httpOnly);
    partialCookie->setSecure(cookie.secure);
    return partialCookie;
}

static Ref<JSON::ArrayOf<PartialCookie>> buildArrayForCookies(const Vector<WebCore::Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<PartialCookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

static bool cookieMatchesFilter(const WebCore::Cookie& cookie, const RefPtr<JSON::Object> optionalFilter)
{

    String optionalFilterName = optionalFilter->getString("name"_s);
    if (!optionalFilterName.isEmpty()) {
        if (cookie.name != optionalFilterName)
            return false;
    }

    auto optionalFilterValue = optionalFilter->getString("value"_s);
    if (!optionalFilterValue.isEmpty()) {
        if (cookie.value != optionalFilterValue)
            return false;
    }

    auto optionalFilterDomain = optionalFilter->getString("domain"_s);
    if (!optionalFilterDomain.isEmpty()) {
        if (cookie.domain != optionalFilterDomain)
            return false;
    }

    auto optionalFilterPath = optionalFilter->getString("path"_s);
    if (!optionalFilterPath.isEmpty()) {
        if (cookie.path != optionalFilterPath)
            return false;
    }

    auto optionalFilterHttpOnly = optionalFilter->getBoolean("httpOnly"_s);
    if (optionalFilterHttpOnly) {
        if (cookie.httpOnly != optionalFilterHttpOnly.value())
            return false;
    }

    auto optionalFilterSecure = optionalFilter->getBoolean("secure"_s);
    if (optionalFilterSecure) {
        if (cookie.secure != optionalFilterSecure.value())
            return false;
    }

    return true;
}

Inspector::Protocol::ErrorStringOr<Ref<PartitionKey>> BidiStorageAgent::makePartitionKey(RefPtr<JSON::Object> partitionDescriptor)
{
    RefPtr session = m_session.get();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto partitionKey = PartitionKey::create().release();
    if (!partitionDescriptor)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);

    auto type = partitionDescriptor->getString("type"_s);
    if (type == "context"_s) {
        auto browsingContextHandle = partitionDescriptor->getString("context"_s);
        if (browsingContextHandle.isEmpty())
            SYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);
        auto page = session->webPageProxyForHandle(browsingContextHandle);

        if (!page)
            SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

        URL pageURL = URL({ }, page->currentURL());
        if (pageURL.isValid())
            partitionKey->setSourceOrigin(pageURL.string());

    } else if (type == "storageKey"_s) {
        // FIXME: support storage key https://bugs.webkit.org/show_bug.cgi?id=292393
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
    } else {
        LOG(Automation, "partitionDescriptor invalid structure or unknown type");
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);
    }
    return { WTF::move(partitionKey) };
}

Inspector::Protocol::ErrorStringOr<Ref<API::HTTPCookieStore>> BidiStorageAgent::cookieStoreForPartition(RefPtr<JSON::Object> partitionDescriptor)
{
    RefPtr session = m_session.get();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    if (!partitionDescriptor) {
        auto page = session->webPageProxyForHandle("default"_s);
        if (!page)
            SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
        return { protect(page->websiteDataStore())->cookieStore() };
    }

    auto type = partitionDescriptor->getString("type"_s);
    if (type == "context"_s) {
        auto browsingContextHandle = partitionDescriptor->getString("context"_s);
        if (browsingContextHandle.isEmpty())
            SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

        auto page = session->webPageProxyForHandle(browsingContextHandle);
        if (!page)
            SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

        return { protect(page->websiteDataStore())->cookieStore() };
    }

    SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
}

void BidiStorageAgent::getCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallbackOf<Ref<JSON::ArrayOf<Inspector::Protocol::BidiStorage::PartialCookie>>, Ref<Inspector::Protocol::BidiStorage::PartitionKey>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto parsedPartitionKey = makePartitionKey(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedPartitionKey, InternalError);
    auto partitionKey = parsedPartitionKey.value();

    auto parsedCookieStore = cookieStoreForPartition(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedCookieStore, InternalError);
    Ref<API::HTTPCookieStore> resolvedCookieStore = parsedCookieStore.value();

    if (!optionalFilter)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);

    resolvedCookieStore->cookies([callback = WTF::move(callback), filter = WTF::move(optionalFilter), partitionKey = WTF::move(partitionKey)](Vector<WebCore::Cookie>&& cookiesList) mutable {
        Vector<WebCore::Cookie> matchingCookies;
        matchingCookies.reserveInitialCapacity(cookiesList.size());

        for (const auto& cookie : cookiesList) {
            if (cookieMatchesFilter(cookie, filter))
                matchingCookies.append(cookie);
        }

        callback({ { buildArrayForCookies(matchingCookies), partitionKey } });
    });
}

void BidiStorageAgent::setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<PartitionKey>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto parsedPartitionKey = makePartitionKey(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedPartitionKey, InternalError);
    auto partitionKey = parsedPartitionKey.value();

    auto parsedCookieStore = cookieStoreForPartition(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedCookieStore, InternalError);
    auto cookieStore = parsedCookieStore.value();

    String name = cookie->getString("name"_s);
    String value = cookie->getString("value"_s);
    String domain = cookie->getString("domain"_s);

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(name.isEmpty(), InternalError);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(value.isEmpty(), InternalError);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(domain.isEmpty(), InternalError);

    WebCore::Cookie webCoreCookie;
    webCoreCookie.name = name;
    webCoreCookie.value = value;
    webCoreCookie.domain = domain;
    webCoreCookie.path = cookie->getString("path"_s);

    auto secureOptional = cookie->getBoolean("secure"_s);
    webCoreCookie.secure = secureOptional.value_or(false);

    auto httpOnlyOptional = cookie->getBoolean("httpOnly"_s);
    webCoreCookie.httpOnly = httpOnlyOptional.value_or(false);

    Vector<WebCore::Cookie> cookiesToSet;
    cookiesToSet.append(WTF::move(webCoreCookie));

    cookieStore->setCookies(WTF::move(cookiesToSet), [callback = WTF::move(callback), partitionKey = WTF::move(partitionKey)]() mutable {
        callback({ WTF::move(partitionKey) });
    });
};

static void deleteCookiesSequentially(RefPtr<API::HTTPCookieStore> store, Vector<WebCore::Cookie> cookies, size_t index, Ref<PartitionKey> partitionKey, Inspector::CommandCallback<Ref<PartitionKey>> callback)
{
    if (index >= cookies.size()) {
        callback({ partitionKey });
        return;
    }
    store->deleteCookie(cookies[index], [store, cookies = WTF::move(cookies), index, partitionKey = WTF::move(partitionKey), callback = WTF::move(callback)]() mutable {
        deleteCookiesSequentially(store, WTF::move(cookies), index + 1, partitionKey, WTF::move(callback));
    });
}

void BidiStorageAgent::deleteCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<PartitionKey>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto parsedPartitionKey = makePartitionKey(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedPartitionKey, InternalError);
    auto partitionKey = parsedPartitionKey.value();

    auto parsedCookieStore = cookieStoreForPartition(optionalPartition);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedCookieStore, InternalError);
    Ref<API::HTTPCookieStore> cookieStore = parsedCookieStore.value();

    if (!optionalFilter)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);

    cookieStore->cookies([callback = WTF::move(callback), filter = WTF::move(optionalFilter), partitionKey = WTF::move(partitionKey), cookieStore](Vector<WebCore::Cookie>&& fetchedCookies) mutable {
        Vector<WebCore::Cookie> toDelete;
        toDelete.reserveInitialCapacity(fetchedCookies.size());

        if (filter) {
            for (auto& cookie : fetchedCookies) {
                if (cookieMatchesFilter(cookie, filter))
                    toDelete.append(cookie);
            }
        }

        if (toDelete.isEmpty()) {
            callback({ partitionKey });
            return;
        }

        LOG(Automation, "deleteCookies: %zu cookies matched; deleting one-by-one.", toDelete.size());
        deleteCookiesSequentially(WTF::move(cookieStore), WTF::move(toDelete), 0, partitionKey, WTF::move(callback));
    });
}

}

#endif // ENABLE(WEBDRIVER_BIDI)

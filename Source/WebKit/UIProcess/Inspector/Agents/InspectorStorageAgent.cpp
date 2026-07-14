/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "InspectorStorageAgent.h"

#include "APIHTTPCookieStore.h"
#include "InspectorCookieStoreHelpers.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <WebCore/Cookie.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

using namespace Inspector;
using Cookie = Inspector::Protocol::Storage::Cookie;
using CookieSameSitePolicy = Inspector::Protocol::Storage::CookieSameSitePolicy;
using PartitionKey = Inspector::Protocol::Storage::PartitionKey;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorStorageAgent);

static CookieSameSitePolicy toProtocol(WebCore::Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case WebCore::Cookie::SameSitePolicy::None:
        return CookieSameSitePolicy::None;
    case WebCore::Cookie::SameSitePolicy::Lax:
        return CookieSameSitePolicy::Lax;
    case WebCore::Cookie::SameSitePolicy::Strict:
        return CookieSameSitePolicy::Strict;
    }
    ASSERT_NOT_REACHED();
    return CookieSameSitePolicy::None;
}

static WebCore::Cookie::SameSitePolicy fromProtocol(const String& sameSite)
{
    if (sameSite == "Strict"_s)
        return WebCore::Cookie::SameSitePolicy::Strict;
    if (sameSite == "Lax"_s)
        return WebCore::Cookie::SameSitePolicy::Lax;
    return WebCore::Cookie::SameSitePolicy::None;
}

static Ref<Cookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    auto result = Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires.value_or(0))
        .setSession(cookie.session)
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSameSite(toProtocol(cookie.sameSite))
        .release();

    if (!cookie.partitionKey.isEmpty())
        result->setPartitionKey(cookie.partitionKey);

    return result;
}

static Ref<JSON::ArrayOf<Cookie>> buildArrayForCookies(const Vector<WebCore::Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Cookie>::create();
    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));
    return cookies;
}

InspectorStorageAgent::InspectorStorageAgent(WebPageAgentContext& context)
    : InspectorAgentBase("Storage"_s, context)
    , m_backendDispatcher(Inspector::StorageBackendDispatcher::create(context.backendDispatcher.get(), this))
    , m_inspectedPage(context.inspectedPage)
{
}

InspectorStorageAgent::~InspectorStorageAgent() = default;

void InspectorStorageAgent::didCreateFrontendAndBackend()
{
}

void InspectorStorageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
}

Inspector::Protocol::ErrorStringOr<void> InspectorStorageAgent::enable()
{
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorStorageAgent::disable()
{
    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<API::HTTPCookieStore>> InspectorStorageAgent::cookieStoreForPartition(const RefPtr<JSON::Object>& partition)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return makeUnexpected("Inspected page is gone"_s);

    if (partition) {
        auto type = partition->getString("type"_s);
        if (type != "context"_s)
            return makeUnexpected("Invalid partition descriptor"_s);
    }

    return Ref { protect(inspectedPage->websiteDataStore())->cookieStore() };
}

Inspector::Protocol::ErrorStringOr<Ref<PartitionKey>> InspectorStorageAgent::makePartitionKey(const RefPtr<JSON::Object>& partition)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return makeUnexpected("Inspected page is gone"_s);

    if (partition) {
        auto type = partition->getString("type"_s);
        if (type != "context"_s)
            return makeUnexpected("Invalid partition descriptor"_s);
    }

    // All-optional builder: create() returns a Builder, release() yields the Ref.
    auto partitionKey = PartitionKey::create().release();

    URL pageURL { inspectedPage->currentURL() };
    if (pageURL.isValid())
        partitionKey->setSourceOrigin(WebCore::SecurityOriginData::fromURL(pageURL).toString());

    return partitionKey;
}

// Not scoped to the inspected page's origins: under Site Isolation cookies span processes a single
// web process can't reach, so per-origin grouping is the frontend's job. See the commit message.
void InspectorStorageAgent::getCookies(RefPtr<JSON::Object>&& filter, RefPtr<JSON::Object>&& partition, Ref<GetCookiesCallback>&& callback)
{
    auto partitionKey = makePartitionKey(partition);
    if (!partitionKey) {
        callback->sendFailure(partitionKey.error());
        return;
    }

    auto cookieStore = cookieStoreForPartition(partition);
    if (!cookieStore) {
        callback->sendFailure(cookieStore.error());
        return;
    }

    Ref<API::HTTPCookieStore> store = cookieStore.value();
    store->cookies([callback = WTF::move(callback), filter = CookieFilter::fromProtocol(filter), partitionKey = partitionKey.value()](Vector<WebCore::Cookie>&& cookiesList) mutable {
        Vector<WebCore::Cookie> matchingCookies;
        matchingCookies.reserveInitialCapacity(cookiesList.size());
        for (const auto& cookie : cookiesList) {
            if (filter.matches(cookie))
                matchingCookies.append(cookie);
        }
        callback->sendSuccess(buildArrayForCookies(matchingCookies), WTF::move(partitionKey));
    });
}

void InspectorStorageAgent::setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& partition, Ref<SetCookieCallback>&& callback)
{
    auto partitionKey = makePartitionKey(partition);
    if (!partitionKey) {
        callback->sendFailure(partitionKey.error());
        return;
    }

    auto cookieStore = cookieStoreForPartition(partition);
    if (!cookieStore) {
        callback->sendFailure(cookieStore.error());
        return;
    }

    auto name = cookie->getString("name"_s);
    auto value = cookie->getString("value"_s);
    auto domain = cookie->getString("domain"_s);
    if (name.isEmpty() || domain.isEmpty()) {
        callback->sendFailure("Cookie must have a name and a domain"_s);
        return;
    }

    WebCore::Cookie webCoreCookie;
    webCoreCookie.name = name;
    webCoreCookie.value = value;
    webCoreCookie.domain = domain;
    webCoreCookie.path = cookie->getString("path"_s);
    webCoreCookie.secure = cookie->getBoolean("secure"_s).value_or(false);
    webCoreCookie.httpOnly = cookie->getBoolean("httpOnly"_s).value_or(false);
    webCoreCookie.session = cookie->getBoolean("session"_s).value_or(false);
    // A session cookie has no expiry, but getCookies must emit the required `expires` field, so it
    // serializes a session cookie's absent expiry as 0. Echoing that back must not set expires to 0
    // (the 1970 epoch, i.e. already expired). Treat session (and any non-positive timestamp) as "no
    // expiry", matching WI.Cookie's own `(!session && expires)` rule.
    if (!webCoreCookie.session) {
        if (auto expires = cookie->getDouble("expires"_s); expires && *expires > 0)
            webCoreCookie.expires = *expires;
    }
    if (auto sameSite = cookie->getString("sameSite"_s); !sameSite.isEmpty())
        webCoreCookie.sameSite = fromProtocol(sameSite);

    // Only partition when the caller asks; a synthesized key would break the set/get/delete round-trip.
    if (auto explicitPartitionKey = cookie->getString("partitionKey"_s); !explicitPartitionKey.isEmpty())
        webCoreCookie.partitionKey = explicitPartitionKey;

    auto resolvedPartitionKey = PartitionKey::create().release();
    if (!webCoreCookie.partitionKey.isEmpty())
        resolvedPartitionKey->setSourceOrigin(webCoreCookie.partitionKey);

    Vector<WebCore::Cookie> cookiesToSet;
    cookiesToSet.append(WTF::move(webCoreCookie));

    Ref<API::HTTPCookieStore> store = cookieStore.value();
    store->setCookies(WTF::move(cookiesToSet), [callback = WTF::move(callback), partitionKey = WTF::move(resolvedPartitionKey)]() mutable {
        callback->sendSuccess(WTF::move(partitionKey));
    });
}

static void deleteCookiesSequentially(Ref<API::HTTPCookieStore> store, Vector<WebCore::Cookie> cookies, size_t index, Ref<PartitionKey> partitionKey, Ref<InspectorStorageAgent::DeleteCookiesCallback> callback)
{
    if (index >= cookies.size()) {
        callback->sendSuccess(WTF::move(partitionKey));
        return;
    }
    // Copy the target cookie out before the lambda moves `cookies` into its capture (unsequenced-read hazard).
    auto cookieToDelete = cookies[index];
    store->deleteCookie(cookieToDelete, [store, cookies = WTF::move(cookies), index, partitionKey = WTF::move(partitionKey), callback = WTF::move(callback)]() mutable {
        deleteCookiesSequentially(WTF::move(store), WTF::move(cookies), index + 1, WTF::move(partitionKey), WTF::move(callback));
    });
}

void InspectorStorageAgent::deleteCookies(RefPtr<JSON::Object>&& filter, RefPtr<JSON::Object>&& partition, Ref<DeleteCookiesCallback>&& callback)
{
    // Require an explicit filter: an empty request would clear the entire store (matches BidiStorageAgent).
    if (!filter) {
        callback->sendFailure("A filter is required to delete cookies"_s);
        return;
    }

    auto partitionKey = makePartitionKey(partition);
    if (!partitionKey) {
        callback->sendFailure(partitionKey.error());
        return;
    }

    auto cookieStore = cookieStoreForPartition(partition);
    if (!cookieStore) {
        callback->sendFailure(cookieStore.error());
        return;
    }

    Ref<API::HTTPCookieStore> store = cookieStore.value();
    store->cookies([store, callback = WTF::move(callback), filter = CookieFilter::fromProtocol(filter), partitionKey = partitionKey.value()](Vector<WebCore::Cookie>&& fetchedCookies) mutable {
        Vector<WebCore::Cookie> toDelete;
        toDelete.reserveInitialCapacity(fetchedCookies.size());
        for (auto& cookie : fetchedCookies) {
            if (filter.matches(cookie))
                toDelete.append(cookie);
        }

        if (toDelete.isEmpty()) {
            callback->sendSuccess(WTF::move(partitionKey));
            return;
        }

        deleteCookiesSequentially(WTF::move(store), WTF::move(toDelete), 0, WTF::move(partitionKey), WTF::move(callback));
    });
}

} // namespace WebKit

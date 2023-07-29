/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CookieStore.h"

#include "Cookie.h"
#include "CookieInit.h"
#include "CookieJar.h"
#include "CookieListItem.h"
#include "CookieStoreDeleteOptions.h"
#include "CookieStoreGetOptions.h"
#include "Document.h"
#include "JSCookieListItem.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CookieStore);

Ref<CookieStore> CookieStore::create(Document* document)
{
    auto cookieStore = adoptRef(*new CookieStore(document));
    cookieStore->suspendIfNeeded();
    return cookieStore;
}

CookieStore::CookieStore(Document* document)
    : ActiveDOMObject(document)
{
}

CookieStore::~CookieStore() = default;

void CookieStore::get(String&& name, Ref<DeferredPromise>&& promise)
{
    get(CookieStoreGetOptions { WTFMove(name), { } }, WTFMove(promise));
}

void CookieStore::get(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise->reject(SecurityError);
        return;
    }

    auto* origin = context->securityOrigin();
    if (!origin) {
        promise->reject(SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { SecurityError, "The origin is opaque"_s });
        return;
    }

    auto& document = *downcast<Document>(context);
    auto* page = document.page();
    if (!page) {
        promise->reject(SecurityError);
        return;
    }

    auto& url = document.url();
    auto& cookieJar = page->cookieJar();
    auto completionHandler = [promise = WTFMove(promise)] (std::optional<Vector<Cookie>>&& cookies) {
        if (!cookies) {
            promise->reject(TypeError);
            return;
        }

        auto& cookiesVector = *cookies;
        if (cookiesVector.isEmpty()) {
            promise->resolveWithJSValue(JSC::jsNull());
            return;
        }

        promise->resolve<IDLDictionary<CookieListItem>>(CookieListItem(WTFMove(cookiesVector[0])));
    };

    cookieJar.getCookiesAsync(document, url, options, WTFMove(completionHandler));
}

void CookieStore::getAll(String&& name, Ref<DeferredPromise>&& promise)
{
    getAll(CookieStoreGetOptions { WTFMove(name), { } }, WTFMove(promise));
}

void CookieStore::getAll(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise->reject(SecurityError);
        return;
    }

    auto* origin = context->securityOrigin();
    if (!origin) {
        promise->reject(SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { SecurityError, "The origin is opaque"_s });
        return;
    }

    auto& document = *downcast<Document>(context);
    auto* page = document.page();
    if (!page) {
        promise->reject(SecurityError);
        return;
    }

    auto url = document.url();
    if (!options.url.isNull()) {
        auto parsed = document.completeURL(options.url);
        if (scriptExecutionContext()->isDocument() && parsed != url) {
            promise->reject(TypeError);
            return;
        }
        if (!origin->isSameOriginDomain(SecurityOrigin::create(parsed))) {
            promise->reject(TypeError);
            return;
        }
        url = WTFMove(parsed);
    }

    auto& cookieJar = page->cookieJar();
    auto completionHandler = [promise = WTFMove(promise)] (std::optional<Vector<Cookie>>&& cookies) {
        if (!cookies) {
            promise->reject(TypeError);
            return;
        }

        promise->resolve<IDLSequence<IDLDictionary<CookieListItem>>>(WTF::map(WTFMove(*cookies), [](auto&& cookie) {
            return CookieListItem { WTFMove(cookie) };
        }));
    };

    cookieJar.getCookiesAsync(document, url, options, WTFMove(completionHandler));
}

void CookieStore::set(String&& name, String&& value, Ref<DeferredPromise>&& promise)
{
    set(CookieInit { WTFMove(name), WTFMove(value) }, WTFMove(promise));
}

void CookieStore::set(CookieInit&& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise->reject(SecurityError);
        return;
    }

    auto* origin = context->securityOrigin();
    if (!origin) {
        promise->reject(SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { SecurityError, "The origin is opaque"_s });
        return;
    }

    auto& document = *downcast<Document>(context);
    auto& url = document.url();
    auto* page = document.page();
    if (!page) {
        promise->reject(SecurityError);
        return;
    }

    // The maximum attribute value size is specified at https://wicg.github.io/cookie-store/#cookie-maximum-attribute-value-size.
    static constexpr auto maximumAttributeValueSize = 1024;

    Cookie cookie;
    cookie.name = WTFMove(options.name);
    cookie.value = WTFMove(options.value);
    cookie.created = WallTime::now().secondsSinceEpoch().milliseconds();

    cookie.domain = options.domain.isNull() ? document.domain() : WTFMove(options.domain);
    if (!cookie.domain.isNull()) {
        if (cookie.domain.startsWith('.')) {
            promise->reject(Exception { TypeError, "The domain must not begin with a '.'"_s });
            return;
        }

        auto host = url.host();
        if (!host.endsWith(cookie.domain) || (host.length() > cookie.domain.length() && !StringView(host).substring(0, host.length() - cookie.domain.length()).endsWith('.'))) {
            promise->reject(Exception { TypeError, "The domain must be a part of the current host"_s });
            return;
        }

        // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
        if (cookie.domain.utf8().length() > maximumAttributeValueSize) {
            promise->reject(Exception { TypeError, makeString("The size of the domain must not be greater than ", maximumAttributeValueSize, " bytes") });
            return;
        }
    }

    cookie.path = WTFMove(options.path);
    if (!cookie.path.isNull()) {
        if (!cookie.path.startsWith('/')) {
            promise->reject(Exception { TypeError, "The path must begin with a '/'"_s });
            return;
        }

        if (!cookie.path.endsWith('/'))
            cookie.path = cookie.path + '/';

        // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
        if (cookie.path.utf8().length() > maximumAttributeValueSize) {
            promise->reject(Exception { TypeError, makeString("The size of the path must not be greater than ", maximumAttributeValueSize, " bytes") });
            return;
        }
    }

    if (options.expires)
        cookie.expires = *options.expires;

    switch (options.sameSite) {
    case CookieSameSite::Strict:
        cookie.sameSite = Cookie::SameSitePolicy::Strict;
        break;
    case CookieSameSite::Lax:
        cookie.sameSite = Cookie::SameSitePolicy::Lax;
        break;
    case CookieSameSite::None:
        cookie.sameSite = Cookie::SameSitePolicy::None;
        break;
    }

    auto& cookieJar = page->cookieJar();
    auto completionHandler = [promise = WTFMove(promise)] (bool setSuccessfully) {
        if (!setSuccessfully)
            promise->reject(TypeError);
        else
            promise->resolve();
    };

    cookieJar.setCookieAsync(document, url, cookie, WTFMove(completionHandler));
}

void CookieStore::remove(String&& name, Ref<DeferredPromise>&& promise)
{
    remove(CookieStoreDeleteOptions { WTFMove(name), { } }, WTFMove(promise));
}

void CookieStore::remove(CookieStoreDeleteOptions&& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise->reject(SecurityError);
        return;
    }

    auto* origin = context->securityOrigin();
    if (!origin) {
        promise->reject(SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { SecurityError, "The origin is opaque"_s });
        return;
    }

    CookieInit initOptions;
    initOptions.name = WTFMove(options.name);
    initOptions.value = emptyString();
    initOptions.domain = WTFMove(options.domain);
    initOptions.path = WTFMove(options.path);
    initOptions.expires = (WallTime::now() - 24_h).secondsSinceEpoch().milliseconds();

    set(WTFMove(initOptions), WTFMove(promise));
}

const char* CookieStore::activeDOMObjectName() const
{
    return "CookieStore";
}

EventTargetInterface CookieStore::eventTargetInterface() const
{
    return CookieStoreEventTargetInterfaceType;
}

ScriptExecutionContext* CookieStore::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

}

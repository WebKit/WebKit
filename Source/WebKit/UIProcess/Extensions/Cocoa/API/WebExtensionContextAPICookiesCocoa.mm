/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIHTTPCookieStore.h"
#import "CocoaHelpers.h"
#import "WKWebViewConfiguration.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionCookieParameters.h"
#import "WebExtensionUtilities.h"
#import "WebsiteDataStore.h"
#import <WebCore/Cookie.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/SessionID.h>
#import <pal/spi/cf/CFNetworkSPI.h>

namespace WebKit {

static inline bool domainsMatch(StringView a, StringView b)
{
    if (equalIgnoringASCIICase(a, b))
        return true;
    if (a.endsWith(makeString('.', b)))
        return true;
    return false;
}

static inline URL toURL(const WebCore::Cookie& cookie)
{
    // Remove leading dot.
    auto domain = cookie.domain.startsWith('.') ? cookie.domain.substring(1) : cookie.domain;

    // Add IPv6 brackets.
    if (domain.contains(':'))
        domain = makeString('[', domain, ']');

    return URL { makeString(cookie.secure ? "https"_s : "http"_s, "://"_s, domain, cookie.path) };
}

void WebExtensionContext::fetchCookies(WebsiteDataStore& dataStore, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Vector<WebExtensionCookieParameters>&&, ErrorString)>&& completionHandler)
{
    if (url.isValid() && !hasPermission(url)) {
        completionHandler({ }, std::nullopt);
        return;
    }

    auto internalCompletionHandler = [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), filterParameters, dataStore = Ref { dataStore }](Vector<WebCore::Cookie>&& cookies) mutable {
        auto result = WTF::compactMap(cookies, [&](auto& cookie) -> std::optional<WebExtensionCookieParameters> {
            if (filterParameters.name && cookie.name != filterParameters.name.value())
                return std::nullopt;

            if (filterParameters.domain && !domainsMatch(cookie.domain, filterParameters.domain.value()))
                return std::nullopt;

            if (filterParameters.path && cookie.path != filterParameters.path.value())
                return std::nullopt;

            if (filterParameters.secure && cookie.secure != filterParameters.secure.value())
                return std::nullopt;

            if (filterParameters.session && cookie.session != filterParameters.session.value())
                return std::nullopt;

            if (!hasPermission(toURL(cookie)))
                return std::nullopt;

            return WebExtensionCookieParameters { dataStore->sessionID(), cookie };
        });

        completionHandler(WTFMove(result), std::nullopt);
    };

    if (url.isValid())
        dataStore.cookieStore().cookiesForURL(url.isolatedCopy(), WTFMove(internalCompletionHandler));
    else
        dataStore.cookieStore().cookies(WTFMove(internalCompletionHandler));
}

void WebExtensionContext::cookiesGet(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(std::nullopt, toErrorString(@"cookies.get()", nil, @"cookie store not found"));
        return;
    }

    WebExtensionCookieFilterParameters filterParameters;
    filterParameters.name = name;

    fetchCookies(*dataStore, url, filterParameters, [completionHandler = WTFMove(completionHandler), dataStore, name = name.isolatedCopy()](Vector<WebExtensionCookieParameters>&& cookies, ErrorString error) mutable {
        if (cookies.isEmpty() || error) {
            completionHandler(std::nullopt, error);
            return;
        }

        ASSERT(cookies.size() == 1);
        auto& cookieParameters = cookies[0];

        completionHandler(cookieParameters, std::nullopt);
    });
}

void WebExtensionContext::cookiesGetAll(std::optional<PAL::SessionID> sessionID, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Vector<WebExtensionCookieParameters>&&, ErrorString)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler({ }, toErrorString(@"cookies.getAll()", nil, @"cookie store not found"));
        return;
    }

    fetchCookies(*dataStore, url, filterParameters, WTFMove(completionHandler));
}

void WebExtensionContext::cookiesSet(std::optional<PAL::SessionID> sessionID, const WebExtensionCookieParameters& cookieParameters, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler({ }, toErrorString(@"cookies.set()", nil, @"cookie store not found"));
        return;
    }

    if (!hasPermission(toURL(cookieParameters.cookie))) {
        completionHandler({ }, toErrorString(@"cookies.set()", nil, @"host permissions are missing or not granted"));
        return;
    }

    dataStore->cookieStore().setCookies({ cookieParameters.cookie }, [completionHandler = WTFMove(completionHandler), sessionID = dataStore->sessionID().isolatedCopy(), cookie = cookieParameters.cookie.isolatedCopy()]() mutable {
        completionHandler({ { sessionID, cookie } }, std::nullopt);
    });
}

void WebExtensionContext::cookiesRemove(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(std::nullopt, toErrorString(@"cookies.remove()", nil, @"cookie store not found"));
        return;
    }

    if (!hasPermission(url)) {
        completionHandler({ }, toErrorString(@"cookies.remove()", nil, @"host permissions are missing or not granted"));
        return;
    }

    WebExtensionCookieFilterParameters filterParameters;
    filterParameters.name = name;

    fetchCookies(*dataStore, url, filterParameters, [completionHandler = WTFMove(completionHandler), dataStore](Vector<WebExtensionCookieParameters>&& cookies, ErrorString error) mutable {
        if (cookies.isEmpty() || error) {
            completionHandler(std::nullopt, error);
            return;
        }

        ASSERT(cookies.size() == 1);
        auto& cookieParameters = cookies[0];

        dataStore->cookieStore().deleteCookie(cookieParameters.cookie, [completionHandler = WTFMove(completionHandler), cookieParameters]() mutable {
            completionHandler(cookieParameters, std::nullopt);
        });
    });
}

void WebExtensionContext::cookiesGetAllCookieStores(CompletionHandler<void(HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>&&, ErrorString)>&& completionHandler)
{
    HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>> stores;

    auto defaultSessionID = extensionController()->configuration().defaultWebsiteDataStore().sessionID();
    stores.set(defaultSessionID, Vector<WebExtensionTabIdentifier> { });

    for (Ref tab : openTabs()) {
        if (!tab->extensionHasAccess())
            continue;

        for (WKWebView *webView in tab->webViews()) {
            auto sessionID = webView.configuration.websiteDataStore->_websiteDataStore.get()->sessionID();

            auto& tabsVector = stores.ensure(sessionID, [] {
                return Vector<WebExtensionTabIdentifier> { };
            }).iterator->value;

            tabsVector.append(tab->identifier());
        }
    }

    completionHandler(WTFMove(stores), std::nullopt);
}

void WebExtensionContext::fireCookiesChangedEventIfNeeded()
{
    constexpr auto type = WebExtensionEventListenerType::CookiesOnChanged;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchCookiesChangedEvent());
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

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
#import "WKWebViewPrivate.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionCookieParameters.h"
#import "WebExtensionUtilities.h"
#import "WebsiteDataStore.h"
#import <WebCore/Cookie.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/SessionID.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/text/MakeString.h>

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

bool WebExtensionContext::isCookiesMessageAllowed()
{
    return isLoaded() && hasPermission(WKWebExtensionPermissionCookies);
}

void WebExtensionContext::fetchCookies(WebsiteDataStore& dataStore, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    if (url.isValid() && !hasPermission(url)) {
        completionHandler({ });
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

        completionHandler(WTFMove(result));
    };

    if (url.isValid())
        dataStore.protectedCookieStore()->cookiesForURL(url.isolatedCopy(), WTFMove(internalCompletionHandler));
    else
        dataStore.protectedCookieStore()->cookies(WTFMove(internalCompletionHandler));
}

void WebExtensionContext::cookiesGet(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(toWebExtensionError(@"cookies.get()", nil, @"cookie store not found"));
        return;
    }

    WebExtensionCookieFilterParameters filterParameters;
    filterParameters.name = name;

    requestPermissionToAccessURLs({ url }, nullptr, [this, protectedThis = Ref { *this }, dataStore, name, url, filterParameters = WTFMove(filterParameters), completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        fetchCookies(*dataStore, url, filterParameters, [completionHandler = WTFMove(completionHandler), dataStore, name](Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(result.error()));
                return;
            }

            auto& cookies = result.value();
            if (cookies.isEmpty()) {
                completionHandler({ });
                return;
            }

            ASSERT(cookies.size() == 1);
            auto& cookieParameters = cookies[0];

            completionHandler({ WTFMove(cookieParameters) });
        });
    });
}

void WebExtensionContext::cookiesGetAll(std::optional<PAL::SessionID> sessionID, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(toWebExtensionError(@"cookies.getAll()", nil, @"cookie store not found"));
        return;
    }

    requestPermissionToAccessURLs({ url }, nullptr, [this, protectedThis = Ref { *this }, dataStore, url, filterParameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        fetchCookies(*dataStore, url, filterParameters, WTFMove(completionHandler));
    });
}

void WebExtensionContext::cookiesSet(std::optional<PAL::SessionID> sessionID, const WebExtensionCookieParameters& cookieParameters, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(toWebExtensionError(@"cookies.set()", nil, @"cookie store not found"));
        return;
    }

    auto url = toURL(cookieParameters.cookie);

    requestPermissionToAccessURLs({ url }, nullptr, [this, protectedThis = Ref { *this }, dataStore, url, cookieParameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!hasPermission(url)) {
            completionHandler(toWebExtensionError(@"cookies.set()", nil, @"host permissions are missing or not granted"));
            return;
        }

        dataStore->cookieStore().setCookies({ cookieParameters.cookie }, [completionHandler = WTFMove(completionHandler), sessionID = dataStore->sessionID(), cookie = cookieParameters.cookie]() mutable {
            completionHandler({ WebExtensionCookieParameters { WTFMove(sessionID), WTFMove(cookie) } });
        });
    });
}

void WebExtensionContext::cookiesRemove(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        completionHandler(toWebExtensionError(@"cookies.remove()", nil, @"cookie store not found"));
        return;
    }

    requestPermissionToAccessURLs({ url }, nullptr, [this, protectedThis = Ref { *this }, dataStore, name, url, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!hasPermission(url)) {
            completionHandler(toWebExtensionError(@"cookies.remove()", nil, @"host permissions are missing or not granted"));
            return;
        }

        WebExtensionCookieFilterParameters filterParameters;
        filterParameters.name = name;

        fetchCookies(*dataStore, url, filterParameters, [completionHandler = WTFMove(completionHandler), dataStore](Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(result.error()));
                return;
            }

            auto& cookies = result.value();
            if (cookies.isEmpty()) {
                completionHandler({ });
                return;
            }

            ASSERT(cookies.size() == 1);
            auto& cookieParameters = cookies[0];

            dataStore->cookieStore().deleteCookie(cookieParameters.cookie, [completionHandler = WTFMove(completionHandler), cookieParameters]() mutable {
                completionHandler({ WTFMove(cookieParameters) });
            });
        });
    });
}

void WebExtensionContext::cookiesGetAllCookieStores(CompletionHandler<void(Expected<HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>, WebExtensionError>&&)>&& completionHandler)
{
    HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>> stores;

    auto defaultSessionID = extensionController()->protectedConfiguration()->defaultWebsiteDataStore().sessionID();
    stores.set(defaultSessionID, Vector<WebExtensionTabIdentifier> { });

    for (Ref tab : openTabs()) {
        if (WKWebView *webView = tab->webView()) {
            auto sessionID = webView.configuration.websiteDataStore->_websiteDataStore.get()->sessionID();

            auto& tabsVector = stores.ensure(sessionID, [] {
                return Vector<WebExtensionTabIdentifier> { };
            }).iterator->value;

            tabsVector.append(tab->identifier());
        }
    }

    completionHandler(WTFMove(stores));
}

void WebExtensionContext::fireCookiesChangedEventIfNeeded()
{
    constexpr auto type = WebExtensionEventListenerType::CookiesOnChanged;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchCookiesChangedEvent());
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

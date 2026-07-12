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
#import "WebExtensionPermission.h"
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

bool WebExtensionContext::isCookiesMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::cookies());
}

// Free non-enforced wrapper for callers that still pass a plain CompletionHandler.
void WebExtensionContext::fetchCookies(WebsiteDataStore& dataStore, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&)>&& completionHandler)
{
    fetchCookies(dataStore, url, filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&), true>(WTF::move(completionHandler)));
}

CompletionHandlerCalledToken WebExtensionContext::fetchCookies(WebsiteDataStore& dataStore, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&), true>&& completionHandler)
{
    if (url.isValid() && !hasPermission(url))
        return completionHandler({ });

    // Genuine leaf: the result handler crosses into APIHTTPCookieStore::cookies/cookiesForURL,
    // which dispatch over a WorkQueue / app-bound-domain resolution and take a non-enforced
    // CompletionHandler. Keep a single deferUnchecked at that boundary.
    return CompletionHandlerCalledToken::deferUnchecked(completionHandler, [this, &dataStore, url, filterParameters](auto& completionHandler, auto deferred) mutable -> CompletionHandlerCalledToken {
        auto internalCompletionHandler = [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), filterParameters, dataStore = Ref { dataStore }](Vector<WebCore::Cookie>&& cookies) mutable {
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

            completionHandler(WTF::move(result));
        };

        if (url.isValid())
            protect(dataStore.cookieStore())->cookiesForURL(url.isolatedCopy(), WTF::move(internalCompletionHandler));
        else
            protect(dataStore.cookieStore())->cookies(WTF::move(internalCompletionHandler));

        return deferred;
    });
}

CompletionHandlerCalledToken WebExtensionContext::cookiesGet(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&), true>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        return completionHandler(toWebExtensionError(@"cookies.get()", nullString(), @"cookie store not found"));
    }

    WebExtensionCookieFilterParameters filterParameters;
    filterParameters.name = name;

    return CompletionHandlerCalledToken::defer(WTF::move(completionHandler), [&](auto completionHandler) -> CompletionHandlerCalledToken {
        return requestPermissionToAccessURLs({ url }, nullptr, CompletionHandler<void(URLSet&&, URLSet&&, WallTime), true>([this, protectedThis = Ref { *this }, dataStore, name, url, filterParameters = WTF::move(filterParameters), completionHandler = WTF::move(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable -> CompletionHandlerCalledToken {
            return fetchCookies(*dataStore, url, filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&), true>([completionHandler = WTF::move(completionHandler), dataStore, name](Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&& result) mutable -> CompletionHandlerCalledToken {
                if (!result)
                    return completionHandler(makeUnexpected(result.error()));

                auto& cookies = result.value();
                if (cookies.isEmpty())
                    return completionHandler({ });

                ASSERT(cookies.size() == 1);
                auto& cookieParameters = cookies[0];

                return completionHandler({ WTF::move(cookieParameters) });
            }));
        }));
    });

}

CompletionHandlerCalledToken WebExtensionContext::cookiesGetAll(std::optional<PAL::SessionID> sessionID, const URL& url, const WebExtensionCookieFilterParameters& filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&), true>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        return completionHandler(toWebExtensionError(@"cookies.getAll()", nullString(), @"cookie store not found"));
    }

    return CompletionHandlerCalledToken::defer(WTF::move(completionHandler), [&](auto completionHandler) -> CompletionHandlerCalledToken {
        return requestPermissionToAccessURLs({ url }, nullptr, CompletionHandler<void(URLSet&&, URLSet&&, WallTime), true>([this, protectedThis = Ref { *this }, dataStore, url, filterParameters, completionHandler = WTF::move(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable -> CompletionHandlerCalledToken {
            return fetchCookies(*dataStore, url, filterParameters, WTF::move(completionHandler));
        }));
    });

}

CompletionHandlerCalledToken WebExtensionContext::cookiesSet(std::optional<PAL::SessionID> sessionID, const WebExtensionCookieParameters& cookieParameters, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&), true>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        return completionHandler(toWebExtensionError(@"cookies.set()", nullString(), @"cookie store not found"));
    }

    auto url = toURL(cookieParameters.cookie);

    return CompletionHandlerCalledToken::defer(WTF::move(completionHandler), [&](auto completionHandler) -> CompletionHandlerCalledToken {
        return requestPermissionToAccessURLs({ url }, nullptr, CompletionHandler<void(URLSet&&, URLSet&&, WallTime), true>([this, protectedThis = Ref { *this }, dataStore, url, cookieParameters, completionHandler = WTF::move(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable -> CompletionHandlerCalledToken {
            if (!hasPermission(url))
                return completionHandler(toWebExtensionError(@"cookies.set()", nullString(), @"host permissions are missing or not granted"));

            return dataStore->cookieStore().setCookies({ cookieParameters.cookie }, CompletionHandler<void(), true>([completionHandler = WTF::move(completionHandler), sessionID = dataStore->sessionID(), cookie = cookieParameters.cookie]() mutable -> CompletionHandlerCalledToken {
                return completionHandler({ WebExtensionCookieParameters { WTF::move(sessionID), WTF::move(cookie) } });
            }));
        }));
    });

}

CompletionHandlerCalledToken WebExtensionContext::cookiesRemove(std::optional<PAL::SessionID> sessionID, const String& name, const URL& url, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&), true>&& completionHandler)
{
    RefPtr dataStore = websiteDataStore(sessionID);
    if (!dataStore) {
        return completionHandler(toWebExtensionError(@"cookies.remove()", nullString(), @"cookie store not found"));
    }

    return CompletionHandlerCalledToken::defer(WTF::move(completionHandler), [&](auto completionHandler) -> CompletionHandlerCalledToken {
        return requestPermissionToAccessURLs({ url }, nullptr, CompletionHandler<void(URLSet&&, URLSet&&, WallTime), true>([this, protectedThis = Ref { *this }, dataStore, name, url, completionHandler = WTF::move(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable -> CompletionHandlerCalledToken {
            if (!hasPermission(url))
                return completionHandler(toWebExtensionError(@"cookies.remove()", nullString(), @"host permissions are missing or not granted"));

            WebExtensionCookieFilterParameters filterParameters;
            filterParameters.name = name;

            return fetchCookies(*dataStore, url, filterParameters, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&), true>([completionHandler = WTF::move(completionHandler), dataStore](Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&& result) mutable -> CompletionHandlerCalledToken {
                if (!result)
                    return completionHandler(makeUnexpected(result.error()));

                auto& cookies = result.value();
                if (cookies.isEmpty())
                    return completionHandler({ });

                ASSERT(cookies.size() == 1);
                auto& cookieParameters = cookies[0];

                return dataStore->cookieStore().deleteCookie(cookieParameters.cookie, CompletionHandler<void(), true>([completionHandler = WTF::move(completionHandler), cookieParameters]() mutable -> CompletionHandlerCalledToken {
                    return completionHandler({ WTF::move(cookieParameters) });
                }));
            }));
        }));
    });

}

CompletionHandlerCalledToken WebExtensionContext::cookiesGetAllCookieStores(CompletionHandler<void(Expected<HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>, WebExtensionError>&&), true>&& completionHandler)
{
    HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>> stores;

    auto defaultSessionID = protect(extensionController()->configuration())->defaultWebsiteDataStore().sessionID();
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

    return completionHandler(WTF::move(stores));
}

void WebExtensionContext::fireCookiesChangedEventIfNeeded()
{
    constexpr auto type = WebExtensionEventListenerType::CookiesOnChanged;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, Function<void()>([=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchCookiesChangedEvent());
    }));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

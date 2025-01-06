/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WebExtensionAPICookies.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionCookieParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

static NSString * const domainKey = @"domain";
static NSString * const expirationDateKey = @"expirationDate";
static NSString * const hostOnlyKey = @"hostOnly";
static NSString * const httpOnlyKey = @"httpOnly";
static NSString * const idKey = @"id";
static NSString * const incognitoKey = @"incognito";
static NSString * const nameKey = @"name";
static NSString * const pathKey = @"path";
static NSString * const sameSiteKey = @"sameSite";
static NSString * const secureKey = @"secure";
static NSString * const sessionKey = @"session";
static NSString * const storeIdKey = @"storeId";
static NSString * const tabIdsKey = @"tabIds";
static NSString * const urlKey = @"url";
static NSString * const valueKey = @"value";

static NSString * const noRestrictionKey = @"no_restriction";
static NSString * const laxKey = @"lax";
static NSString * const strictKey = @"strict";

static NSString * const ephemeralPrefix = @"ephemeral-";
static NSString * const persistentPrefix = @"persistent-";

static inline std::optional<PAL::SessionID> toImpl(NSString *storeID)
{
    ASSERT(storeID.length);

    auto *scanner = [NSScanner scannerWithString:storeID];

    uint64_t sessionMask = 0;
    if ([scanner scanString:ephemeralPrefix intoString:nullptr])
        sessionMask = PAL::SessionID::EphemeralSessionMask;
    else if (![scanner scanString:persistentPrefix intoString:nullptr])
        return std::nullopt;

    uint64_t value;
    if ([scanner scanUnsignedLongLong:&value]) {
        auto result = PAL::SessionID(value | sessionMask);
        if (!result.isValid())
            return std::nullopt;
        return result;
    }

    return std::nullopt;
}

static inline NSString *toWebAPI(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());

    auto maskedValue = sessionID.toUInt64() & ~PAL::SessionID::EphemeralSessionMask;
    auto *prefix = sessionID.isEphemeral() ? ephemeralPrefix : persistentPrefix;
    return [NSString stringWithFormat:@"%@%llu", prefix, maskedValue];
}

static inline NSString *toWebAPI(WebCore::Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case WebCore::Cookie::SameSitePolicy::None:
        return noRestrictionKey;
    case WebCore::Cookie::SameSitePolicy::Lax:
        return laxKey;
    case WebCore::Cookie::SameSitePolicy::Strict:
        return strictKey;
    }
}

static inline NSDictionary *toWebAPI(const WebExtensionCookieParameters& cookieParameters)
{
    ASSERT(cookieParameters.sessionIdentifier);

    auto& cookie = cookieParameters.cookie;

    NSMutableDictionary *result = [@{
        domainKey: cookie.domain,
        hostOnlyKey: @(!cookie.domain.startsWith('.')),
        httpOnlyKey: @(cookie.httpOnly),
        nameKey: cookie.name,
        pathKey: cookie.path,
        sameSiteKey: toWebAPI(cookie.sameSite),
        secureKey: @(cookie.secure),
        sessionKey: @(cookie.session),
        storeIdKey: toWebAPI(cookieParameters.sessionIdentifier.value()),
        valueKey: cookie.value,
    } mutableCopy];

    if (cookie.expires)
        result[expirationDateKey] = @(Seconds::fromMilliseconds(cookie.expires.value()).seconds());

    return [result copy];
}

static inline NSArray *toWebAPI(const HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>& stores)
{
    auto *result = [NSMutableArray arrayWithCapacity:stores.size()];

    for (const auto& entry : stores) {
        auto *tabIdentifiers = createNSArray(entry.value, [](const auto& tabIdentifier) {
            return @(toWebAPI(tabIdentifier));
        }).get();

        [result addObject:@{ idKey: toWebAPI(entry.key), tabIdsKey: tabIdentifiers, incognitoKey: @(entry.key.isEphemeral()) }];
    }

    return [result copy];
}

std::optional<WebExtensionAPICookies::ParsedDetails> WebExtensionAPICookies::parseCookieDetails(NSDictionary *details, NSArray *requiredKeys, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
        storeIdKey: NSString.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return std::nullopt;

    String name = details[nameKey];
    if (!name.isNull() && name.isEmpty()) {
        *outExceptionString = toErrorString(nil, nameKey, @"it must not be empty");
        return std::nullopt;
    }

    URL url;
    if (NSString *urlString = details[urlKey]) {
        if (!urlString.length) {
            *outExceptionString = toErrorString(nil, urlKey, @"it must not be empty");
            return std::nullopt;
        }

        url = URL { urlString };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", urlString);
            return std::nullopt;
        }
    }

    std::optional<PAL::SessionID> sessionID;
    if (NSString *storeID = details[storeIdKey]) {
        if (!storeID.length) {
            *outExceptionString = toErrorString(nil, storeIdKey, @"it must not be empty");
            return std::nullopt;
        }

        sessionID = toImpl(storeID);
        if (!sessionID) {
            *outExceptionString = toErrorString(nil, storeIdKey, @"'%@' is not a valid cookie store identifier", storeID);
            return std::nullopt;
        }
    }

    return { { sessionID, name, url } };
}

void WebExtensionAPICookies::get(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/get

    auto parsedDetails = parseCookieDetails(details, @[ nameKey, urlKey ], outExceptionString);
    if (!parsedDetails)
        return;

    auto [sessionID, name, url] = WTFMove(parsedDetails.value());

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CookiesGet(sessionID, name, url), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

static inline String normalizeDomain(const String& domain)
{
    auto normalizedDomain = domain;

    // Remove leading dot, subdomain matching is assumed.
    if (normalizedDomain.startsWith('.'))
        normalizedDomain = normalizedDomain.substring(1);

    // Remove IPv6 brackets.
    if (normalizedDomain.contains(':') && normalizedDomain.startsWith('[') && normalizedDomain.endsWith(']'))
        normalizedDomain = normalizedDomain.substring(1, normalizedDomain.length() - 2);

    return normalizedDomain;
}

void WebExtensionAPICookies::getAll(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/getAll

    auto parsedDetails = parseCookieDetails(details, nil, outExceptionString);
    if (!parsedDetails)
        return;

    auto [sessionID, name, url] = WTFMove(parsedDetails.value());

    static NSDictionary<NSString *, id> *types = @{
        domainKey: NSString.class,
        pathKey: NSString.class,
        secureKey: @YES.class,
        sessionKey: @YES.class,
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return;

    WebExtensionCookieFilterParameters filterParameters;

    if (!name.isNull())
        filterParameters.name = name;

    if (details[domainKey])
        filterParameters.domain = normalizeDomain(details[domainKey]);

    if (details[pathKey])
        filterParameters.path = details[pathKey];

    if (details[secureKey])
        filterParameters.secure = objectForKey<NSNumber>(details, secureKey).boolValue;

    if (details[sessionKey])
        filterParameters.session = objectForKey<NSNumber>(details, sessionKey).boolValue;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CookiesGetAll(sessionID, url, WTFMove(filterParameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPICookies::set(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/set

    auto parsedDetails = parseCookieDetails(details, @[ urlKey ], outExceptionString);
    if (!parsedDetails)
        return;

    auto [sessionID, name, url] = WTFMove(parsedDetails.value());

    static NSDictionary<NSString *, id> *types = @{
        domainKey: NSString.class,
        expirationDateKey: NSNumber.class,
        httpOnlyKey: @YES.class,
        pathKey: NSString.class,
        sameSiteKey: NSString.class,
        secureKey: @YES.class,
        valueKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return;

    auto cookieParameters = WebExtensionCookieParameters { sessionID, { } };
    auto& cookie = cookieParameters.cookie;

    cookie.name = details[nameKey] ?: @"";
    cookie.value = details[valueKey] ?: @"";
    cookie.secure = details[secureKey] ? objectForKey<NSNumber>(details, secureKey).boolValue : false;
    cookie.domain = details[domainKey] ? String(details[domainKey]) : normalizeDomain(url.host().toString());
    cookie.path = details[pathKey] ? String(details[pathKey]) : url.path().toString();
    cookie.httpOnly = objectForKey<NSNumber>(details, httpOnlyKey).boolValue;
    cookie.created = WallTime::now().secondsSinceEpoch().milliseconds();

    if (auto *expirationNumber = objectForKey<NSNumber>(details, expirationDateKey); expirationNumber.doubleValue > 0)
        cookie.expires = WallTime::fromRawSeconds(expirationNumber.doubleValue).secondsSinceEpoch().milliseconds();
    else
        cookie.session = true;

    if (NSString *sameSiteString = details[sameSiteKey]) {
        if ([sameSiteString isEqualToString:noRestrictionKey])
            cookie.sameSite = WebCore::Cookie::SameSitePolicy::None;
        else if ([sameSiteString isEqualToString:laxKey])
            cookie.sameSite = WebCore::Cookie::SameSitePolicy::Lax;
        else if ([sameSiteString isEqualToString:strictKey])
            cookie.sameSite = WebCore::Cookie::SameSitePolicy::Strict;
        else {
            *outExceptionString = toErrorString(nil, sameSiteKey, @"it must specify either 'no_restriction', 'lax', or 'strict'");
            return;
        }
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CookiesSet(sessionID, cookieParameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPICookies::remove(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/remove

    auto parsedDetails = parseCookieDetails(details, @[ nameKey, urlKey ], outExceptionString);
    if (!parsedDetails)
        return;

    auto [sessionID, name, url] = WTFMove(parsedDetails.value());

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CookiesRemove(sessionID, name, url), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPICookies::getAllCookieStores(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/getAllCookieStores

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CookiesGetAllCookieStores(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPICookies::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/onChanged

    if (!m_onChanged)
        m_onChanged = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::CookiesOnChanged);

    return *m_onChanged;
}

void WebExtensionContextProxy::dispatchCookiesChangedEvent()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/onChanged

    // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.cookies().onChanged().invokeListeners();
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

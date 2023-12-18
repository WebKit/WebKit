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
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

namespace WebKit {

static NSString * const domainKey = @"domain";
static NSString * const expirationDateKey = @"expirationDate";
static NSString * const httpOnlyKey = @"httpOnly";
static NSString * const nameKey = @"name";
static NSString * const partitionKeyKey = @"partitionKey";
static NSString * const pathKey = @"path";
static NSString * const sameSiteKey = @"sameSite";
static NSString * const secureKey = @"secure";
static NSString * const sessionKey = @"session";
static NSString * const storeIdKey = @"storeId";
static NSString * const topLevelSiteKey = @"topLevelSite";
static NSString * const urlKey = @"url";
static NSString * const valueKey = @"value";

void WebExtensionAPICookies::get(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/get

    static NSArray<NSString *> *requiredKeys = @[
        nameKey,
        urlKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
        partitionKeyKey: NSDictionary.class,
        storeIdKey: NSString.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    if (NSDictionary *partitionKey = details[partitionKeyKey]) {
        if (!validateDictionary(partitionKey, partitionKeyKey, nil, @{ topLevelSiteKey: NSString.class }, outExceptionString))
            return;
    }

    if (NSString *name = details[nameKey]; !name.length) {
        *outExceptionString = toErrorString(nil, nameKey, @"it must not be empty");
        return;
    }

    if (NSString *urlString = details[urlKey]) {
        if (!urlString.length) {
            *outExceptionString = toErrorString(nil, urlKey, @"it must not be empty");
            return;
        }

        auto url = URL { urlString };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", urlString);
            return;
        }
    }

    // FIXME: <https://webkit.org/b/261771> Implement.
    callback->call();
}

void WebExtensionAPICookies::getAll(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/getAll

    static NSDictionary<NSString *, id> *types = @{
        domainKey: NSString.class,
        nameKey: NSString.class,
        partitionKeyKey: NSDictionary.class,
        pathKey: NSString.class,
        secureKey: @YES.class,
        sessionKey: @YES.class,
        storeIdKey: NSString.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return;

    if (NSDictionary *partitionKey = details[partitionKeyKey]) {
        if (!validateDictionary(partitionKey, partitionKeyKey, nil, @{ topLevelSiteKey: NSString.class }, outExceptionString))
            return;
    }

    if (NSString *name = details[nameKey]; name && !name.length) {
        *outExceptionString = toErrorString(nil, nameKey, @"it must not be empty");
        return;
    }

    if (NSString *urlString = details[urlKey]) {
        if (!urlString.length) {
            *outExceptionString = toErrorString(nil, urlKey, @"it must not be empty");
            return;
        }

        auto url = URL { urlString };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", urlString);
            return;
        }
    }

    // FIXME: <https://webkit.org/b/261771> Implement.
    callback->call(@[ ]);
}

void WebExtensionAPICookies::set(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/set

    static NSDictionary<NSString *, id> *types = @{
        domainKey: NSString.class,
        expirationDateKey: NSNumber.class,
        httpOnlyKey: @YES.class,
        nameKey: NSString.class,
        partitionKeyKey: NSDictionary.class,
        pathKey: NSString.class,
        sameSiteKey: NSString.class,
        secureKey: @YES.class,
        storeIdKey: NSString.class,
        urlKey: NSString.class,
        valueKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return;

    if (NSDictionary *partitionKey = details[partitionKeyKey]) {
        if (!validateDictionary(partitionKey, partitionKeyKey, nil, @{ topLevelSiteKey: NSString.class }, outExceptionString))
            return;
    }

    if (NSString *name = details[nameKey]; name && !name.length) {
        *outExceptionString = toErrorString(nil, nameKey, @"it must not be empty");
        return;
    }

    if (NSString *urlString = details[urlKey]) {
        if (!urlString.length) {
            *outExceptionString = toErrorString(nil, urlKey, @"it must not be empty");
            return;
        }

        auto url = URL { urlString };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", urlString);
            return;
        }
    }

    // FIXME: <https://webkit.org/b/261771> Implement.
    callback->call();
}

void WebExtensionAPICookies::remove(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/remove

    static NSArray<NSString *> *requiredKeys = @[
        nameKey,
        urlKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
        partitionKeyKey: NSDictionary.class,
        storeIdKey: NSString.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    if (NSDictionary *partitionKey = details[partitionKeyKey]) {
        if (!validateDictionary(partitionKey, partitionKeyKey, nil, @{ topLevelSiteKey: NSString.class }, outExceptionString))
            return;
    }

    if (NSString *name = details[nameKey]; !name.length) {
        *outExceptionString = toErrorString(nil, nameKey, @"it must not be empty");
        return;
    }

    if (NSString *urlString = details[urlKey]) {
        if (!urlString.length) {
            *outExceptionString = toErrorString(nil, urlKey, @"it must not be empty");
            return;
        }

        auto url = URL { urlString };
        if (!url.isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", urlString);
            return;
        }
    }

    // FIXME: Implement.
    callback->call();
}

void WebExtensionAPICookies::getAllCookieStores(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/getAllCookieStores

    // FIXME: <https://webkit.org/b/261771> Implement.
    callback->call(@[ ]);
}

WebExtensionAPIEvent& WebExtensionAPICookies::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies/onChanged

    if (!m_onChanged)
        m_onChanged = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::CookiesOnChanged);

    return *m_onChanged;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

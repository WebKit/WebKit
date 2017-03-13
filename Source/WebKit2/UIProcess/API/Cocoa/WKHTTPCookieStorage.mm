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

#import "config.h"
#import "WKHTTPCookieStorageInternal.h"

#if WK_API_ENABLED

#include "HTTPCookieAcceptPolicy.h"
#include <WebCore/CFNetworkSPI.h>
#include <WebCore/Cookie.h>
#include <WebCore/URL.h>
#include <wtf/RetainPtr.h>

static NSArray<NSHTTPCookie *> *coreCookiesToNSCookies(const Vector<WebCore::Cookie>& coreCookies)
{
    NSMutableArray<NSHTTPCookie *> *nsCookies = [NSMutableArray arrayWithCapacity:coreCookies.size()];

    for (auto& cookie : coreCookies)
        [nsCookies addObject:(NSHTTPCookie *)cookie];

    return nsCookies;
}

@implementation WKHTTPCookieStorage

- (void)dealloc
{
    _cookieStorage->API::HTTPCookieStorage::~HTTPCookieStorage();

    [super dealloc];
}

- (void)fetchCookies:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler
{
    _cookieStorage->cookies([handler = adoptNS([completionHandler copy])](const Vector<WebCore::Cookie>& cookies) {
        auto rawHandler = (void (^)(NSArray<NSHTTPCookie *> *))handler.get();
        rawHandler(coreCookiesToNSCookies(cookies));
    });
}

- (void)fetchCookiesForURL:(NSURL *)url completionHandler:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler
{
    _cookieStorage->cookies(url, [handler = adoptNS([completionHandler copy])](const Vector<WebCore::Cookie>& cookies) {
        auto rawHandler = (void (^)(NSArray<NSHTTPCookie *> *))handler.get();
        rawHandler(coreCookiesToNSCookies(cookies));
    });
}

- (void)setCookie:(NSHTTPCookie *)cookie completionHandler:(void (^)())completionHandler
{
    _cookieStorage->setCookie(cookie, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });

}

- (void)deleteCookie:(NSHTTPCookie *)cookie completionHandler:(void (^)())completionHandler
{
    _cookieStorage->deleteCookie(cookie, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });
}

- (void)setCookies:(NSArray<NSHTTPCookie *> *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL completionHandler:(void (^)())completionHandler
{
    Vector<WebCore::Cookie> coreCookies;
    coreCookies.reserveInitialCapacity(cookies.count);
    for (NSHTTPCookie *cookie : cookies)
        coreCookies.uncheckedAppend(cookie);

    _cookieStorage->setCookies(coreCookies, URL, mainDocumentURL, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });
}

- (void)removeCookiesSinceDate:(NSDate *)date completionHandler:(void (^)())completionHandler
{
    auto systemClockTime = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(date.timeIntervalSince1970)));

    _cookieStorage->removeCookiesSinceDate(systemClockTime, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });
}

- (void)setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy completionHandler:(void (^)())completionHandler
{
    _cookieStorage->setHTTPCookieAcceptPolicy(policy, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });
}

static NSHTTPCookieAcceptPolicy kitCookiePolicyToNSCookiePolicy(WebKit::HTTPCookieAcceptPolicy kitPolicy)
{
    switch (kitPolicy) {
    case WebKit::HTTPCookieAcceptPolicyAlways:
        return NSHTTPCookieAcceptPolicyAlways;
    case WebKit::HTTPCookieAcceptPolicyNever:
        return NSHTTPCookieAcceptPolicyNever;
    case WebKit::HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    case WebKit::HTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain:
        // Cast required because of CFNetworkSPI.
        return static_cast<NSHTTPCookieAcceptPolicy>(NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain);
    default:
        ASSERT_NOT_REACHED();
    }

    return NSHTTPCookieAcceptPolicyNever;
}

- (void)fetchCookieAcceptPolicy:(void (^)(NSHTTPCookieAcceptPolicy))completionHandler
{
    _cookieStorage->getHTTPCookieAcceptPolicy([handler = adoptNS([completionHandler copy])](WebKit::HTTPCookieAcceptPolicy policy) {
        auto rawHandler = (void (^)(NSHTTPCookieAcceptPolicy))handler.get();
        rawHandler(kitCookiePolicyToNSCookiePolicy(policy));
    });
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_cookieStorage;
}

@end

#endif

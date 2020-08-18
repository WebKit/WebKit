/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "APIHTTPCookieStore.h"

#import "WebsiteDataStore.h"
#import <WebCore/Cookie.h>
#import <WebCore/CookieStorageObserver.h>
#import <WebCore/HTTPCookieAcceptPolicy.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

namespace API {

void HTTPCookieStore::flushDefaultUIProcessCookieStore()
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] _saveCookies];
}

Vector<WebCore::Cookie> HTTPCookieStore::getAllDefaultUIProcessCookieStoreCookies()
{
    NSArray<NSHTTPCookie *> *cookiesArray = [NSHTTPCookieStorage sharedHTTPCookieStorage].cookies;
    Vector<WebCore::Cookie> cookiesVector;
    cookiesVector.reserveInitialCapacity(cookiesArray.count);
    for (NSHTTPCookie *cookie in cookiesArray)
        cookiesVector.uncheckedAppend({ cookie });
    return cookiesVector;
}

void HTTPCookieStore::setCookieInDefaultUIProcessCookieStore(const WebCore::Cookie& cookie)
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookie:cookie];
}

void HTTPCookieStore::deleteCookieFromDefaultUIProcessCookieStore(const WebCore::Cookie& cookie)
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:cookie];
}

void HTTPCookieStore::startObservingChangesToDefaultUIProcessCookieStore(Function<void()>&& function)
{
    stopObservingChangesToDefaultUIProcessCookieStore();
    m_defaultUIProcessObserver = makeUnique<WebCore::CookieStorageObserver>([NSHTTPCookieStorage sharedHTTPCookieStorage]);
    m_defaultUIProcessObserver->startObserving(WTFMove(function));
}

void HTTPCookieStore::stopObservingChangesToDefaultUIProcessCookieStore()
{
    if (auto observer = std::exchange(m_defaultUIProcessObserver, nullptr))
        observer->stopObserving();
}

void HTTPCookieStore::deleteCookiesInDefaultUIProcessCookieStore()
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] removeCookiesSinceDate:[NSDate distantPast]];
}

static NSHTTPCookieAcceptPolicy toNSHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case WebCore::HTTPCookieAcceptPolicy::AlwaysAccept:
        return NSHTTPCookieAcceptPolicyAlways;
    case WebCore::HTTPCookieAcceptPolicy::Never:
        return NSHTTPCookieAcceptPolicyNever;
    case WebCore::HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
        return NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    case WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        return (NSHTTPCookieAcceptPolicy)NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain;
    }
    ASSERT_NOT_REACHED();
    return NSHTTPCookieAcceptPolicyAlways;
}

void HTTPCookieStore::setHTTPCookieAcceptPolicyInDefaultUIProcessCookieStore(WebCore::HTTPCookieAcceptPolicy policy)
{
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookieAcceptPolicy:toNSHTTPCookieAcceptPolicy(policy)];
}

} // namespace API

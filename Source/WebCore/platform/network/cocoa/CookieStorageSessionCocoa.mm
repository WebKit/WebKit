/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#import "CookieStorageSession.h"

#import "Cookie.h"
#import "CookieRequestHeaderFieldProxy.h"
#import "HTTPCookieAcceptPolicyCocoa.h"
#import "SameSiteInfo.h"
#import <optional>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/StringView.h>
#import <wtf/text/cf/StringConcatenateCF.h>

namespace WebCore {

RetainPtr<NSHTTPCookieStorage> CookieStorageSession::nsCookieStorage() const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    auto cfCookieStorage = cookieStorage();
    ASSERT(cfCookieStorage || !m_isInMemoryCookieStore);
    if (!m_isInMemoryCookieStore && (!cfCookieStorage || [NSHTTPCookieStorage sharedHTTPCookieStorage]._cookieStorage == cfCookieStorage))
        return [NSHTTPCookieStorage sharedHTTPCookieStorage];

    return adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cfCookieStorage.get()]);
}

RetainPtr<CFURLStorageSessionRef> createPrivateStorageSession(CFStringRef identifier, std::optional<HTTPCookieAcceptPolicy> cookieAcceptPolicy, CookieStorageSession::ShouldDisableCFURLCache shouldDisableCFURLCache)
{
    const void* sessionPropertyKeys[] = { _kCFURLStorageSessionIsPrivate };
    const void* sessionPropertyValues[] = { kCFBooleanTrue };
    RetainPtr<CFDictionaryRef> sessionProperties = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, sessionPropertyKeys, sessionPropertyValues, sizeof(sessionPropertyKeys) / sizeof(*sessionPropertyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CFURLStorageSessionRef> storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, sessionProperties.get()));

    if (!storageSession)
        return nullptr;

    if (shouldDisableCFURLCache == CookieStorageSession::ShouldDisableCFURLCache::Yes)
        _CFURLStorageSessionDisableCache(storageSession.get());

    // The private storage session should have the same properties as the default storage session,
    // with the exception that it should be in-memory only storage.

    // FIXME 9199649: If any of the storages do not exist, do no use the storage session.
    // This could occur if there is an issue figuring out where to place a storage on disk (e.g. the
    // sandbox does not allow CFNetwork access).

    if (shouldDisableCFURLCache == CookieStorageSession::ShouldDisableCFURLCache::No) {
        RetainPtr<CFURLCacheRef> cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
        if (!cache)
            return nullptr;

        CFURLCacheSetMemoryCapacity(cache.get(), [[NSURLCache sharedURLCache] memoryCapacity]);
    }

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    NSHTTPCookieAcceptPolicy nsCookieAcceptPolicy;
    if (cookieAcceptPolicy)
        nsCookieAcceptPolicy = toNSHTTPCookieAcceptPolicy(*cookieAcceptPolicy);
    else
        nsCookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    // FIXME: Use _CFHTTPCookieStorageGetDefault when USE(CFNETWORK) is defined in WebKit for consistency.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), nsCookieAcceptPolicy);

    return storageSession;
}

void CookieStorageSession::deleteHTTPCookie(CFHTTPCookieStorageRef cookieStorage, NSHTTPCookie *cookie, CompletionHandler<void()>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    auto work = [completionHandler = WTF::move(completionHandler), cookieStorage = RetainPtr { cookieStorage }, cookie = RetainPtr { cookie }, isInMemoryCookieStore = m_isInMemoryCookieStore] () mutable {
        if (!cookieStorage) {
            RELEASE_ASSERT(!isInMemoryCookieStore);
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:cookie.get()];
        } else
            CFHTTPCookieStorageDeleteCookie(cookieStorage.get(), [cookie _GetInternalCFHTTPCookie]);
        ensureOnMainThread(WTF::move(completionHandler));
    };

    if (m_isInMemoryCookieStore)
        return work();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr(WTF::move(work)).get());
}

RetainPtr<NSDictionary> CookieStorageSession::policyProperties(const SameSiteInfo& sameSiteInfo, NSURL *url, NSString *partition, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision)
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    BOOL shouldAllowOnlyPartitioned = thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::AllExceptPartitioned;
    RetainPtr policyProperties = adoptNS([[NSMutableDictionary alloc] init]);
    policyProperties.get()[@"_kCFHTTPCookiePolicyPropertySiteForCookies"] = RetainPtr { sameSiteInfo.isSameSite ? url : URL::emptyNSURL() };
    policyProperties.get()[@"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation"] = [NSNumber numberWithBool:sameSiteInfo.isTopSite];
    policyProperties.get()[@"_kCFHTTPCookiePolicyPropertyAllowOnlyPartitionedCookies"] = @(shouldAllowOnlyPartitioned);
    if (partition)
        policyProperties.get()[@"_kCFHTTPCookiePolicyPropertyStoragePartitionIdentifier"] = partition;
#else
    UNUSED_PARAM(partition);
    UNUSED_PARAM(thirdPartyCookieBlockingDecision);
    NSDictionary *policyProperties = @{
        @"_kCFHTTPCookiePolicyPropertySiteForCookies": sameSiteInfo.isSameSite ? url : URL::emptyNSURL(),
        @"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation": [NSNumber numberWithBool:sameSiteInfo.isTopSite],
    };
#endif
    return policyProperties;
}

RetainPtr<NSArray> CookieStorageSession::cookiesForURLFromStorage(NSHTTPCookieStorage *storage, NSURL *url, NSURL *mainDocumentURL, const std::optional<SameSiteInfo>& sameSiteInfo, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, NSString *partition)
{
    ASSERT(thirdPartyCookieBlockingDecision != ThirdPartyCookieBlockingDecision::All);

    // The _getCookiesForURL: method calls the completionHandler synchronously. We use std::optional<> to check this invariant and crash if it's not met.
    std::optional<RetainPtr<NSArray>> cookiesPtr;
    auto completionHandler = [&cookiesPtr] (NSArray *cookies) {
        cookiesPtr = retainPtr(cookies);
    };
    [storage _getCookiesForURL:url mainDocumentURL:mainDocumentURL partition:partition policyProperties:sameSiteInfo ? policyProperties(sameSiteInfo.value(), url, partition, thirdPartyCookieBlockingDecision).get() : nullptr completionHandler:completionHandler];
    RELEASE_ASSERT(!!cookiesPtr);

    // _getCookiesForURL returns only unpartitioned cookies if partition is nil, and it returns both
    // unpartitioned cookies plus cookies in the specified partition if partition is not nil. Return the
    // array of cookies the partition was nil, or if we should return both partitioned and unpartitioned
    // cookies
    if (!partition || thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::None)
        return WTF::move(*cookiesPtr);

    // Filter all cookies that aren't in the specified partition.
    RetainPtr<NSMutableArray<NSHTTPCookie *>> partitionedCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:[cookiesPtr->get() count]]);
    for (NSHTTPCookie *nsCookie in cookiesPtr->get()) {
        if (![nsCookie._storagePartition isEqualToString:partition])
            continue;
        [partitionedCookies.get() addObject:nsCookie];
    }
    return WTF::move(partitionedCookies);
}

void CookieStorageSession::setHTTPCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSArray *cookies, NSURL *url, NSURL *mainDocumentURL, NSString *partition, const SameSiteInfo& sameSiteInfo, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    if (!cookieStorage) {
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url, partition, thirdPartyCookieBlockingDecision).get()];
        return;
    }

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // CookieStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
    [nsCookieStorage _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url, partition, thirdPartyCookieBlockingDecision).get()];
}

RetainPtr<NSArray> CookieStorageSession::httpCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSURL *firstParty, const std::optional<SameSiteInfo>& sameSiteInfo, NSURL *url, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, NSString *partition) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        cookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    }

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // CookieStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
    return cookiesForURLFromStorage(nsCookieStorage.get(), url, firstParty, sameSiteInfo, thirdPartyCookieBlockingDecision, partition);
}

RetainPtr<NSHTTPCookie> CookieStorageSession::capExpiryOfPersistentCookie(NSHTTPCookie *cookie, Seconds cap)
{
    if ([cookie isSessionOnly])
        return cookie;

    if (!cookie.expiresDate || cookie.expiresDate.timeIntervalSinceNow > cap.seconds()) {
        RetainPtr<NSMutableDictionary> properties = adoptNS([[cookie properties] mutableCopy]);
        RetainPtr<NSDate> date = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:cap.seconds()]);
        [properties setObject:date.get() forKey:NSHTTPCookieExpires];
        return adoptNS([[NSHTTPCookie alloc] initWithProperties:properties.get()]);
    }
    return cookie;
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
NSHTTPCookie *CookieStorageSession::setCookiePartition(NSHTTPCookie *cookie, NSString* partitionKey)
{
    if (!cookie)
        return cookie;

    if (!partitionKey)
        return cookie;

    if (cookie._storagePartition) {
        ASSERT(cookie._storagePartition == partitionKey);
        return cookie;
    }

    RetainPtr<NSMutableDictionary> properties = adoptNS([[cookie properties] mutableCopy]);
    [properties setObject:partitionKey forKey:@"StoragePartition"];
    return [NSHTTPCookie cookieWithProperties:properties.get()];
}
#endif

RetainPtr<NSArray> CookieStorageSession::cookiesForURL(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    if (thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All)
        return nil;
    return httpCookiesForURL(cookieStorage().get(), firstParty.createNSURL().get(), sameSiteInfo, url.createNSURL().get(), thirdPartyCookieBlockingDecision, nsStringNilIfEmpty(partition).get());
}

Vector<Cookie> CookieStorageSession::nsCookiesToCookieVector(NSArray *nsCookies, NOESCAPE const Function<bool(NSHTTPCookie *)>& filter)
{
    Vector<Cookie> cookies;
    cookies.reserveInitialCapacity(nsCookies.count);
    for (NSHTTPCookie *nsCookie in nsCookies) {
        @autoreleasepool {
            if (!filter || filter(nsCookie))
                cookies.append(nsCookie);
        }
    }
    if (filter)
        cookies.shrinkToFit();
    return cookies;
}

bool CookieStorageSession::shouldIncludeCookie(NSHTTPCookie *cookie, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, bool& didAccessSecureCookies)
{
    if (![[cookie name] length])
        return false;
    if (cookiesFor == CookiesFor::Dom && [cookie isHTTPOnly])
        return false;
    if ([cookie isSecure]) {
        didAccessSecureCookies = true;
        if (includeSecureCookies == IncludeSecureCookies::No)
            return false;
    }
    return true;
}

std::pair<String, bool> CookieStorageSession::cookiesForSession(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto cookies = cookiesForURL(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, partition);
    if (![cookies count])
        return { String(), false }; // Return a null string; StringBuilder below would create an empty one.

    StringBuilder cookiesBuilder;
    bool didAccessSecureCookies = false;
    for (NSHTTPCookie *cookie in cookies.get()) {
        if (!shouldIncludeCookie(cookie, cookiesFor, includeSecureCookies, didAccessSecureCookies))
            continue;
        cookiesBuilder.append(cookiesBuilder.isEmpty() ? ""_s : "; "_s, [cookie name], '=', [cookie value]);
    }
    return { cookiesBuilder.toString(), didAccessSecureCookies };

    END_BLOCK_OBJC_EXCEPTIONS
    return { String(), false };
}

std::optional<Vector<Cookie>> CookieStorageSession::cookiesForSessionAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition, const String& cookieName) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto cookies = cookiesForURL(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, partition);
    if (![cookies count])
        return Vector<Cookie> { };

    RetainPtr name = cookieName.createNSString();
    bool didAccessSecureCookies = false;
    return nsCookiesToCookieVector(cookies.get(), [&](NSHTTPCookie *cookie) {
        if (!shouldIncludeCookie(cookie, cookiesFor, includeSecureCookies, didAccessSecureCookies))
            return false;
        return cookieName.isNull() || [[cookie name] isEqualToString:name.get()];
    });

    END_BLOCK_OBJC_EXCEPTIONS
    return std::nullopt;
}

std::pair<String, bool> CookieStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, CookiesFor::Dom, includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

std::pair<String, bool> CookieStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, CookiesFor::Http, includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

std::pair<String, bool> CookieStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, CookiesFor::Http, headerFieldProxy.includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

RetainPtr<NSHTTPCookie> CookieStorageSession::adjustScriptWrittenCookie(NSHTTPCookie *initialCookie, std::optional<Seconds> cappedLifetime)
{
    if (!initialCookie)
        return nil;

#if ENABLE(JS_COOKIE_CHECKING)
    RetainPtr mutableProperties = adoptNS([[initialCookie properties] mutableCopy]);
    [mutableProperties.get() setValue:@1 forKey:@"SetInJavaScript"];
    RetainPtr cookie = adoptNS([[NSHTTPCookie alloc] initWithProperties:mutableProperties.get()]);
#else
    RetainPtr cookie = initialCookie;
#endif

    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =". We have a workaround in setCookies() to prevent
    // that, but we also need to avoid sending cookies that were previously stored, and
    // there's no harm to doing this check because such a cookie is never valid.
    if (![[cookie name] length])
        return nil;

    if ([cookie isHTTPOnly])
        return nil;

    // Cap lifetime of persistent, client-side cookies.
    if (cappedLifetime)
        return capExpiryOfPersistentCookie(cookie.get(), *cappedLifetime);

    return cookie;
}

RetainPtr<NSHTTPCookie> CookieStorageSession::parseDOMCookie(const String& initialCookieString, NSURL *cookieURL, std::optional<Seconds> cappedLifetime, const String& partition)
{
    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (initialCookieString.isEmpty())
        return nil;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    auto cookieString = initialCookieString.contains('=') ? initialCookieString : makeString(initialCookieString, '=');

    // FIXME: <rdar://185837942> Remove this once CFNetwork's cookie-date parser accepts a date that
    // writes the month before the day of the month. RFC 6265 section 5.1.1 accepts either ordering.
    if (auto dayFirst = CookieUtil::cookieStringWithDayFirstExpires(cookieString))
        cookieString = WTF::move(*dayFirst);

    return adjustScriptWrittenCookie([NSHTTPCookie _cookieForSetCookieString:cookieString.createNSString().get() forURL:cookieURL partition:nsStringNilIfEmpty(partition).get()], cappedLifetime);
}

void CookieStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& cookieString, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, std::optional<Seconds> cappedLifetime, const String& partition) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr cookieURL = url.createNSURL();

    RetainPtr cookie = parseDOMCookie(cookieString, cookieURL.get(), cappedLifetime, partition);
    if (!cookie)
        return;

    setHTTPCookiesForURL(cookieStorage().get(), @[cookie.get()], cookieURL.get(), firstParty.createNSURL().get(), nsStringNilIfEmpty(partition).get(), sameSiteInfo, thirdPartyCookieBlockingDecision);

    END_BLOCK_OBJC_EXCEPTIONS
}

bool CookieStorageSession::setCookieFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const Cookie& cookie, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, std::optional<Seconds> cappedLifetime, const String& partition) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr nsCookie = adjustScriptWrittenCookie(cookie.createNSHTTPCookie().get(), cappedLifetime);
    if (!nsCookie)
        return false;

    setHTTPCookiesForURL(cookieStorage().get(), @[nsCookie.get()], url.createNSURL().get(), firstParty.createNSURL().get(), nsStringNilIfEmpty(partition).get(), sameSiteInfo, thirdPartyCookieBlockingDecision);
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

void CookieStorageSession::setCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [nsCookieStorage() setCookie:cookie.createNSHTTPCookie().get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool CookieStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition, Vector<Cookie>& rawCookies) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    rawCookies = nsCookiesToCookieVector(cookiesForURL(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, partition).get());

    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void CookieStorageSession::deleteCookie(const URL& firstParty, const URL& url, const String& cookieName, const String& partition, CompletionHandler<void()>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto aggregator = CallbackAggregator::create(WTF::move(completionHandler));

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    RetainPtr<NSArray> cookies = httpCookiesForURL(cookieStorage.get(), firstParty.createNSURL().get(), std::nullopt, url.createNSURL().get(), ThirdPartyCookieBlockingDecision::None, nsStringNilIfEmpty(partition).get());

    RetainPtr cookieNameString = cookieName.createNSString();

    NSUInteger count = [cookies count];
    for (NSUInteger i = 0; i < count; ++i) {
        RetainPtr<NSHTTPCookie> cookie = [cookies objectAtIndex:i];
        if ([[cookie name] isEqualToString:cookieNameString.get()])
            deleteHTTPCookie(cookieStorage.get(), cookie.get(), [aggregator] { });
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void CookieStorageSession::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto work = [completionHandler = WTF::move(completionHandler), cookieStorage = RetainPtr { cookieStorage() }] () mutable {
        if (!cookieStorage) {
            RetainPtr cookieStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
            for (NSHTTPCookie *cookie in [cookieStorage cookies])
                [cookieStorage deleteCookie:cookie];
        } else
            CFHTTPCookieStorageDeleteAllCookies(cookieStorage.get());
        ensureOnMainThread(WTF::move(completionHandler));
    };

    if (m_isInMemoryCookieStore)
        return work();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr(WTF::move(work)).get());
}

} // namespace WebCore

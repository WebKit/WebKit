/*
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/StringView.h>
#import <wtf/text/cf/StringConcatenateCF.h>
#import <wtf/unicode/CharacterNames.h>

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
    RetainPtr sessionProperties = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, sessionPropertyKeys, sessionPropertyValues, sizeof(sessionPropertyKeys) / sizeof(*sessionPropertyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, sessionProperties.get()));

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
        RetainPtr cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
        if (!cache)
            return nullptr;

        CFURLCacheSetMemoryCapacity(cache.get(), [[NSURLCache sharedURLCache] memoryCapacity]);
    }

    RetainPtr cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
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

static RetainPtr<NSDictionary> policyProperties(const SameSiteInfo& sameSiteInfo, NSURL *url, NSString *partition, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision)
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

static RetainPtr<NSArray> cookiesForURLFromStorage(NSHTTPCookieStorage *storage, NSURL *url, NSURL *mainDocumentURL, const std::optional<SameSiteInfo>& sameSiteInfo, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, NSString *partition = nullptr)
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
        RetainPtr properties = adoptNS([[cookie properties] mutableCopy]);
        RetainPtr date = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:cap.seconds()]);
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
        ASSERT([cookie._storagePartition isEqualToString:partitionKey]);
        return cookie;
    }

    RetainPtr properties = adoptNS([[cookie properties] mutableCopy]);
    [properties setObject:partitionKey forKey:@"StoragePartition"];
    return [NSHTTPCookie cookieWithProperties:properties.get()];
}
#endif

RetainPtr<NSArray> CookieStorageSession::cookiesForURL(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return httpCookiesForURL(cookieStorage().get(), firstParty.createNSURL().get(), sameSiteInfo, url.createNSURL().get(), thirdPartyCookieBlockingDecision, nsStringNilIfNull(partition).get());
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
        if (![[cookie name] length])
            continue;
        if (cookiesFor == CookiesFor::DOMAccess && [cookie isHTTPOnly])
            continue;
        if ([cookie isSecure]) {
            didAccessSecureCookies = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
                continue;
        }
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

    Vector<Cookie> cookiesVector;
    RetainPtr name = cookieName.createNSString();
    for (NSHTTPCookie *cookie in cookies.get()) {
        if (![[cookie name] length])
            continue;
        if (cookiesFor == CookiesFor::DOMAccess && [cookie isHTTPOnly])
            continue;
        if ([cookie isSecure] && includeSecureCookies == IncludeSecureCookies::No)
            continue;
        if (!cookieName.isNull() && ![[cookie name] isEqualToString:name.get()])
            continue;

        cookiesVector.append(Cookie(cookie));
    }
    return cookiesVector;

    END_BLOCK_OBJC_EXCEPTIONS
    return std::nullopt;
}

std::pair<String, bool> CookieStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, CookiesFor::DOMAccess, includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

std::pair<String, bool> CookieStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, IncludeSecureCookies includeSecureCookies, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, CookiesFor::HTTPHeader, includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

std::pair<String, bool> CookieStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partition) const
{
    return cookiesForSession(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, CookiesFor::HTTPHeader, headerFieldProxy.includeSecureCookies, thirdPartyCookieBlockingDecision, partition);
}

static RetainPtr<NSHTTPCookie> cookieWithPropertyOverrides(NSHTTPCookie *cookie, NSDictionary<NSHTTPCookiePropertyKey, id> *overrides)
{
    if (!cookie)
        return nil;
    RetainPtr properties = adoptNS([[cookie properties] mutableCopy]);
    if (!properties)
        return nil;
    [properties.get() addEntriesFromDictionary:overrides];
    return adoptNS([[NSHTTPCookie alloc] initWithProperties:properties.get()]);
}

#if HAVE(BROKEN_LEADING_BOM_COOKIE_PARSER)
// FIXME: <rdar://186225250> Remove this once NSHTTPCookie stops discarding a leading byte order
// mark from the name and the value. Every property dictionary discards one, so this must be the
// last step before storing; it only acts after observing the loss, so it goes inert once fixed.
static RetainPtr<NSHTTPCookie> cookieWithLeadingByteOrderMarkRestored(RetainPtr<NSHTTPCookie>&& cookie, StringView name, StringView value)
{
    if (!cookie)
        return WTF::move(cookie);

    bool nameLostMark = name.startsWith(byteOrderMark) && String { [cookie name] } == name.substring(1);
    bool valueLostMark = value.startsWith(byteOrderMark) && String { [cookie value] } == value.substring(1);
    if (!nameLostMark && !valueLostMark)
        return WTF::move(cookie);

    auto withExtraMark = [](bool lostMark, StringView text) {
        return lostMark ? makeString(byteOrderMark, text) : text.toString();
    };
    RetainPtr restored = cookieWithPropertyOverrides(cookie.get(), @{
        NSHTTPCookieName: withExtraMark(nameLostMark, name).createNSString().get(),
        NSHTTPCookieValue: withExtraMark(valueLostMark, value).createNSString().get()
    });

    return restored ? WTF::move(restored) : WTF::move(cookie);
}
#endif


static RetainPtr<NSHTTPCookie> adjustScriptWrittenCookie(NSHTTPCookie *initialCookie, std::optional<Seconds> cappedLifetime)
{
    if (!initialCookie)
        return nil;

#if ENABLE(JS_COOKIE_CHECKING)
    RetainPtr cookie = cookieWithPropertyOverrides(initialCookie, @{ @"SetInJavaScript": @1 });
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
        return CookieStorageSession::capExpiryOfPersistentCookie(cookie.get(), *cappedLifetime);

    return cookie;
}

#if HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
static RetainPtr<NSHTTPCookie> cookieWithNonASCIINameAndValue(const String& cookieString, std::pair<StringView, StringView> nameAndValue, NSURL *cookieURL, const String& partition)
{
    auto placeholder = CookieUtil::cookieStringWithNonASCIIReplaced(cookieString);
#if HAVE(BROKEN_COOKIE_DATE_PARSER)
    // Compose with the Expires workarounds: a cookie can carry several of these defects at once.
    if (auto repaired = CookieUtil::cookieStringWithRepairedExpires(placeholder))
        placeholder = WTF::move(*repaired);
#endif

    RetainPtr parsed = [NSHTTPCookie _cookieForSetCookieString:placeholder.createNSString().get() forURL:cookieURL partition:nsStringNilIfEmpty(partition).get()];
    if (!parsed)
        return nil;

    return cookieWithPropertyOverrides(parsed.get(), @{
        NSHTTPCookieName: nameAndValue.first.createNSString().get(),
        NSHTTPCookieValue: nameAndValue.second.createNSString().get()
    });
}
#endif

static RetainPtr<NSHTTPCookie> parseDOMCookie(String cookieString, NSURL* cookieURL, std::optional<Seconds> cappedLifetime, const String& partition)
{
    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (cookieString.isEmpty())
        return nil;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    cookieString = cookieString.contains('=') ? cookieString : makeString(cookieString, '=');

#if HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
    // A non-ASCII Path or Domain cannot survive the all-ASCII stand-in, so leave those cookies alone.
    if (!cookieString.containsOnlyLatin1() && !CookieUtil::cookieAttributesContainNonASCII(cookieString)) {
        auto nameAndValue = CookieUtil::cookieNameAndValue(cookieString);
        if (nameAndValue) {
            if (RetainPtr cookie = cookieWithNonASCIINameAndValue(cookieString, *nameAndValue, cookieURL, partition)) {
                RetainPtr adjusted = adjustScriptWrittenCookie(cookie.get(), cappedLifetime);
#if HAVE(BROKEN_LEADING_BOM_COOKIE_PARSER)
                adjusted = cookieWithLeadingByteOrderMarkRestored(WTF::move(adjusted), nameAndValue->first, nameAndValue->second);
#endif
                return adjusted;
            }
        }
    }
#endif

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
    // FIXME: <rdar://185837942>, <rdar://186224951> Remove this once CFNetwork's cookie-date parser
    // accepts a date that writes the month before the day of the month, and matches the day and
    // month names case-insensitively. RFC 6265 section 5.1.1 requires both.
    if (auto repaired = CookieUtil::cookieStringWithRepairedExpires(cookieString))
        cookieString = WTF::move(*repaired);
#endif

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

    RetainPtr nshttpCookie = adjustScriptWrittenCookie(cookie.createNSHTTPCookie().get(), cappedLifetime);
    if (!nshttpCookie)
        return false;

#if HAVE(BROKEN_LEADING_BOM_COOKIE_PARSER)
    nshttpCookie = cookieWithLeadingByteOrderMarkRestored(WTF::move(nshttpCookie), cookie.name, cookie.value);
#endif

    setHTTPCookiesForURL(cookieStorage().get(), @[ nshttpCookie.get() ], url.createNSURL().get(), firstParty.createNSURL().get(), nsStringNilIfEmpty(partition).get(), sameSiteInfo, thirdPartyCookieBlockingDecision);
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

#if HAVE(BROKEN_COOKIE_DATE_PARSER) || HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
// Only a cookie CFNetwork actually stored is repaired, and it is repaired in place, so every policy
// CFNetwork applied to it carries over and a comma inside a value can never synthesize a cookie.
void CookieStorageSession::repairCookiesFromHTTPResponse(const URL& firstParty, const URL& url, const SameSiteInfo& sameSiteInfo, const String& setCookieHeaderValue, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision, const String& partitionKey, NOESCAPE const Function<RetainPtr<NSArray>(NSArray *)>& transformCookies) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    if (setCookieHeaderValue.isEmpty())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr cookieURL = url.createNSURL();
    RetainPtr firstPartyURL = firstParty.createNSURL();

    RetainPtr<NSArray> storedCookies;
    auto fetchStoredCookies = [&] {
        storedCookies = httpCookiesForURL(cookieStorage().get(), firstPartyURL.get(), std::nullopt, cookieURL.get(), thirdPartyCookieBlockingDecision, nsStringNilIfNull(partitionKey).get());
    };
    // A cookie's identity is its name, domain, path and partition; the value guards against a
    // fragment of another cookie's value, or an older cookie that this header failed to replace.
    auto findStoredCookie = [&](StringView name, StringView value, NSHTTPCookie *identity) -> NSHTTPCookie * {
        if (!storedCookies)
            fetchStoredCookies();
        for (NSHTTPCookie *cookie in storedCookies.get()) {
            if (String { cookie.name } == name && String { cookie.value } == value
                && [cookie.domain isEqualToString:identity.domain] && [cookie.path isEqualToString:identity.path]
                && (cookie._storagePartition == identity._storagePartition || [cookie._storagePartition isEqualToString:identity._storagePartition]))
                return cookie;
        }
        return nil;
    };

    RetainPtr<NSMutableArray<NSHTTPCookie *>> repaired;
    Vector<std::pair<RetainPtr<NSHTTPCookie>, RetainPtr<NSHTTPCookie>>> renamed;

    for (auto cookieString : CookieUtil::splitCoalescedSetCookieHeader(setCookieHeaderValue)) {
        auto storedNameAndValue = CookieUtil::cookieNameAndValue(cookieString);
        if (!storedNameAndValue)
            continue;

        bool isAllASCII = cookieString.containsOnlyASCII();

        std::optional<std::pair<StringView, StringView>> recoveredNameAndValue;
#if HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
        // CFNetwork stores each byte of the header as one code unit, so a UTF-8 name or value is
        // recovered by decoding those code units as bytes.
        std::optional<String> recovered;
        if (!isAllASCII && (recovered = CookieUtil::cookieStringWithRecoveredUTF8(cookieString)))
            recoveredNameAndValue = CookieUtil::cookieNameAndValue(*recovered);
#endif

        // The all-ASCII stand-in keeps a mangled name from making CFNetwork reject the whole string,
        // and parsing it tells us the domain, path and partition CFNetwork stored the cookie under.
        String asciiCookieString = isAllASCII ? String { } : CookieUtil::cookieStringWithNonASCIIReplaced(cookieString);
        StringView parseableCookieString = isAllASCII ? cookieString : StringView { asciiCookieString };
        std::optional<String> repairedExpiresString;
#if HAVE(BROKEN_COOKIE_DATE_PARSER)
        repairedExpiresString = CookieUtil::cookieStringWithRepairedExpires(parseableCookieString);
#endif
        if (!recoveredNameAndValue && !repairedExpiresString)
            continue;

        RetainPtr parsed = [NSHTTPCookie _cookieForSetCookieString:(repairedExpiresString ? StringView { *repairedExpiresString } : parseableCookieString).createNSString().get() forURL:cookieURL.get() partition:nsStringNilIfEmpty(partitionKey).get()];
        if (!parsed)
            continue;
        RetainPtr identity = transformCookies ? [transformCookies(@[ parsed.get() ]).get() firstObject] : parsed.get();
        if (!identity)
            continue;

        RetainPtr stored = findStoredCookie(storedNameAndValue->first, storedNameAndValue->second, identity.get());
        if (!stored)
            continue;

        RetainPtr<NSDate> expiresDate = repairedExpiresString ? [parsed expiresDate] : nil;
        // A cookie CFNetwork gave an expiry to did not have its date rejected.
        if (![stored isSessionOnly])
            expiresDate = nil;
        if (!recoveredNameAndValue && !expiresDate)
            continue;

        RetainPtr properties = adoptNS([[stored properties] mutableCopy]);
        if (recoveredNameAndValue) {
            [properties setObject:recoveredNameAndValue->first.createNSString().get() forKey:NSHTTPCookieName];
            [properties setObject:recoveredNameAndValue->second.createNSString().get() forKey:NSHTTPCookieValue];
        }
        if (expiresDate) {
            // A session cookie is marked Discard, which wins over any expiry.
            [properties setObject:expiresDate.get() forKey:NSHTTPCookieExpires];
            [properties removeObjectForKey:NSHTTPCookieDiscard];
        }
        RetainPtr cookie = adoptNS([[NSHTTPCookie alloc] initWithProperties:properties.get()]);
        if (cookie && transformCookies)
            cookie = [transformCookies(@[ cookie.get() ]).get() firstObject];
#if HAVE(BROKEN_LEADING_BOM_COOKIE_PARSER)
        if (recoveredNameAndValue)
            cookie = cookieWithLeadingByteOrderMarkRestored(WTF::move(cookie), recoveredNameAndValue->first, recoveredNameAndValue->second);
#endif
        if (!cookie)
            continue;

        if (!repaired)
            repaired = adoptNS([[NSMutableArray alloc] init]);
        [repaired addObject:cookie.get()];

        // A new name is a new storage key, so the mangled cookie would otherwise stay behind.
        if (![[cookie name] isEqualToString:[stored name]])
            renamed.append({ WTF::move(stored), WTF::move(cookie) });
    }

    if (!repaired)
        return;

    setHTTPCookiesForURL(cookieStorage().get(), repaired.get(), cookieURL.get(), firstPartyURL.get(), nsStringNilIfEmpty(partitionKey).get(), sameSiteInfo, thirdPartyCookieBlockingDecision);

    if (renamed.isEmpty())
        return;

    // Delete a mangled cookie only once its replacement is known to be stored, and synchronously,
    // so the next request cannot send both.
    fetchStoredCookies();
    for (auto& [mangled, replacement] : renamed) {
        if (!findStoredCookie(String { [replacement name] }, String { [replacement value] }, replacement.get()))
            continue;
        if (RetainPtr storage = cookieStorage())
            CFHTTPCookieStorageDeleteCookie(storage.get(), [mangled _GetInternalCFHTTPCookie]);
        else
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:mangled.get()];
    }

    END_BLOCK_OBJC_EXCEPTIONS
}
#endif

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

    RetainPtr<NSArray> cookies = cookiesForURL(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, partition);
    NSUInteger count = [cookies count];
    rawCookies = Vector<Cookie>(count, [cookies](size_t i) {
        return Cookie { checked_objc_cast<NSHTTPCookie>([cookies objectAtIndex:i]) };
    });

    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void CookieStorageSession::deleteCookie(const URL& firstParty, const URL& url, const String& cookieName, const String& partition, CompletionHandler<void()>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto aggregator = CallbackAggregator::create(WTF::move(completionHandler));

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    RetainPtr<NSArray> cookies = httpCookiesForURL(cookieStorage.get(), firstParty.createNSURL().get(), std::nullopt, url.createNSURL().get(), ThirdPartyCookieBlockingDecision::None, nsStringNilIfNull(partition).get());

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

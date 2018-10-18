/*
 * Copyright (C) 2015-2018 Apple Inc.  All rights reserved.
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
#import "NetworkStorageSession.h"

#import "Cookie.h"
#import "CookieRequestHeaderFieldProxy.h"
#import "CookieStorageObserver.h"
#import "CookiesStrategy.h"
#import "SameSiteInfo.h"
#import "URL.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/text/StringBuilder.h>

@interface NSURL ()
- (CFURLRef)_cfurl;
@end

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
@interface NSHTTPCookieStorage (Staging)
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition policyProperties:(NSDictionary*)props completionHandler:(void (^)(NSArray *))completionHandler;
- (void)_setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL policyProperties:(NSDictionary*) props;
@end
#endif

namespace WebCore {

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [nsCookieStorage() setCookie:(NSHTTPCookie *)cookie];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    RetainPtr<NSMutableArray> nsCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:cookies.size()]);
    for (const auto& cookie : cookies)
        [nsCookies addObject:(NSHTTPCookie *)cookie];

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [nsCookieStorage() setCookies:nsCookies.get() forURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [nsCookieStorage() deleteCookie:(NSHTTPCookie *)cookie];
}

static Vector<Cookie> nsCookiesToCookieVector(NSArray<NSHTTPCookie *> *nsCookies)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    Vector<Cookie> cookies;
    cookies.reserveInitialCapacity(nsCookies.count);
    for (NSHTTPCookie *nsCookie in nsCookies)
        cookies.uncheckedAppend(nsCookie);

    return cookies;
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector(nsCookieStorage().cookies);
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector([nsCookieStorage() cookiesForURL:(NSURL *)url]);
}

void NetworkStorageSession::flushCookieStore()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [nsCookieStorage() _saveCookies];
}

NSHTTPCookieStorage *NetworkStorageSession::nsCookieStorage() const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    auto cfCookieStorage = cookieStorage();
    if (!cfCookieStorage || [NSHTTPCookieStorage sharedHTTPCookieStorage]._cookieStorage == cfCookieStorage)
        return [NSHTTPCookieStorage sharedHTTPCookieStorage];

    return [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cfCookieStorage.get()] autorelease];
}

CookieStorageObserver& NetworkStorageSession::cookieStorageObserver() const
{
    if (!m_cookieStorageObserver)
        m_cookieStorageObserver = CookieStorageObserver::create(nsCookieStorage());

    return *m_cookieStorageObserver;
}

CFURLStorageSessionRef createPrivateStorageSession(CFStringRef identifier)
{
    const void* sessionPropertyKeys[] = { _kCFURLStorageSessionIsPrivate };
    const void* sessionPropertyValues[] = { kCFBooleanTrue };
    auto sessionProperties = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, sessionPropertyKeys, sessionPropertyValues, sizeof(sessionPropertyKeys) / sizeof(*sessionPropertyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, sessionProperties.get()));

    if (!storageSession)
        return nullptr;

    // The private storage session should have the same properties as the default storage session,
    // with the exception that it should be in-memory only storage.

    // FIXME 9199649: If any of the storages do not exist, do no use the storage session.
    // This could occur if there is an issue figuring out where to place a storage on disk (e.g. the
    // sandbox does not allow CFNetwork access).

    auto cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
    if (!cache)
        return nullptr;

    CFURLCacheSetDiskCapacity(cache.get(), 0); // Setting disk cache size should not be necessary once <rdar://problem/12656814> is fixed.
    CFURLCacheSetMemoryCapacity(cache.get(), [[NSURLCache sharedURLCache] memoryCapacity]);

    if (!NetworkStorageSession::processMayUseCookieAPI())
        return storageSession.leakRef();

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    // FIXME: Use _CFHTTPCookieStorageGetDefault when USE(CFNETWORK) is defined in WebKit for consistency.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy]);

    return storageSession.leakRef();
}

static NSArray *httpCookies(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    if (!cookieStorage)
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies];
    
    auto cookies = adoptCF(CFHTTPCookieStorageCopyCookies(cookieStorage));
    return [NSHTTPCookie _cf2nsCookies:cookies.get()];
}

static void deleteHTTPCookie(CFHTTPCookieStorageRef cookieStorage, NSHTTPCookie *cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    if (!cookieStorage) {
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:cookie];
        return;
    }
    
    CFHTTPCookieStorageDeleteCookie(cookieStorage, [cookie _GetInternalCFHTTPCookie]);
}

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
static RetainPtr<NSDictionary> policyProperties(const SameSiteInfo& sameSiteInfo, NSURL *url)
{
    static NSURL *emptyURL = [[NSURL alloc] initWithString:@""];
    NSDictionary *policyProperties = @{
        @"_kCFHTTPCookiePolicyPropertySiteForCookies": sameSiteInfo.isSameSite ? url : emptyURL,
        @"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation": [NSNumber numberWithBool:sameSiteInfo.isTopSite],
    };
    return policyProperties;
}
#endif

static NSArray *cookiesForURL(NSHTTPCookieStorage *storage, NSURL *url, NSURL *mainDocumentURL, const std::optional<SameSiteInfo>& sameSiteInfo, NSString *partition = nullptr)
{
    // The _getCookiesForURL: method calls the completionHandler synchronously. We use std::optional<> to ensure this invariant.
    std::optional<RetainPtr<NSArray *>> cookiesPtr;
    auto completionHandler = [&cookiesPtr] (NSArray *cookies) {
        cookiesPtr = retainPtr(cookies);
    };
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
    if ([storage respondsToSelector:@selector(_getCookiesForURL:mainDocumentURL:partition:policyProperties:completionHandler:)])
        [storage _getCookiesForURL:url mainDocumentURL:mainDocumentURL partition:partition policyProperties:sameSiteInfo ? policyProperties(sameSiteInfo.value(), url).get() : nullptr completionHandler:completionHandler];
    else
        [storage _getCookiesForURL:url mainDocumentURL:mainDocumentURL partition:partition completionHandler:completionHandler];
#else
    [storage _getCookiesForURL:url mainDocumentURL:mainDocumentURL partition:partition completionHandler:completionHandler];
    UNUSED_PARAM(sameSiteInfo);
#endif
    ASSERT(!!cookiesPtr);
    return cookiesPtr->autorelease();
}

static void setHTTPCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSArray *cookies, NSURL *url, NSURL *mainDocumentURL, const SameSiteInfo& sameSiteInfo)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    if (!cookieStorage) {
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
        if ([NSHTTPCookieStorage instancesRespondToSelector:@selector(_setCookies:forURL:mainDocumentURL:policyProperties:)])
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url).get()];
        else
#endif
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL];
        return;
    }
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
    if ([NSHTTPCookieStorage instancesRespondToSelector:@selector(_setCookies:forURL:mainDocumentURL:policyProperties:)]) {
        // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
        // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
        RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
        [nsCookieStorage _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url).get()];
    } else {
#endif
        auto cfCookies = adoptCF([NSHTTPCookie _ns2cfCookies:cookies]);
        CFHTTPCookieStorageSetCookies(cookieStorage, cfCookies.get(), [url _cfurl], [mainDocumentURL _cfurl]);
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
    }
#else
    UNUSED_PARAM(sameSiteInfo);
#endif
}

static NSArray *httpCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSURL *firstParty, const std::optional<SameSiteInfo>& sameSiteInfo, NSURL *url)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    if (!cookieStorage)
        cookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
    return cookiesForURL(nsCookieStorage.get(), url, firstParty, sameSiteInfo);
}

static RetainPtr<NSArray> filterCookies(NSArray *unfilteredCookies)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    NSUInteger count = [unfilteredCookies count];
    RetainPtr<NSMutableArray> filteredCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:count]);

    const NSTimeInterval secondsPerWeek = 7 * 24 * 60 * 60;
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[unfilteredCookies objectAtIndex:i];

        // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
        // which would be sent as "Cookie: =". We have a workaround in setCookies() to prevent
        // that, but we also need to avoid sending cookies that were previously stored, and
        // there's no harm to doing this check because such a cookie is never valid.
        if (![[cookie name] length])
            continue;

        if ([cookie isHTTPOnly])
            continue;

        // Cap lifetime of persistent, client-side cookies to a week.
        if (![cookie isSessionOnly]) {
            if (!cookie.expiresDate || cookie.expiresDate.timeIntervalSinceNow > secondsPerWeek) {
                RetainPtr<NSMutableDictionary<NSHTTPCookiePropertyKey, id>> properties = adoptNS([[cookie properties] mutableCopy]);
                RetainPtr<NSDate> dateInAWeek = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:secondsPerWeek]);
                [properties setObject:dateInAWeek.get() forKey:NSHTTPCookieExpires];
                cookie = [NSHTTPCookie cookieWithProperties:properties.get()];
            }
        }

        [filteredCookies.get() addObject:cookie];
    }

    return filteredCookies;
}

static NSArray *cookiesForURL(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (session.shouldBlockCookies(firstParty, url, frameID, pageID))
        return nil;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
#endif
    return httpCookiesForURL(session.cookieStorage().get(), firstParty, sameSiteInfo, url);
}

enum IncludeHTTPOnlyOrNot { DoNotIncludeHTTPOnly, IncludeHTTPOnly };
static std::pair<String, bool> cookiesForSession(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeHTTPOnlyOrNot includeHTTPOnly, IncludeSecureCookies includeSecureCookies)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = cookiesForURL(session, firstParty, sameSiteInfo, url, frameID, pageID);
    if (![cookies count])
        return { String(), false }; // Return a null string, not an empty one that StringBuilder would create below.

    StringBuilder cookiesBuilder;
    bool didAccessSecureCookies = false;
    for (NSHTTPCookie *cookie in cookies) {
        if (![[cookie name] length])
            continue;

        if (!includeHTTPOnly && [cookie isHTTPOnly])
            continue;

        if ([cookie isSecure]) {
            didAccessSecureCookies = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
                continue;
        }

        if (!cookiesBuilder.isEmpty())
            cookiesBuilder.appendLiteral("; ");

        cookiesBuilder.append([cookie name]);
        cookiesBuilder.append('=');
        cookiesBuilder.append([cookie value]);
    }
    return { cookiesBuilder.toString(), didAccessSecureCookies };

    END_BLOCK_OBJC_EXCEPTIONS;
    return { String(), false };
}

static void deleteAllHTTPCookies(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!cookieStorage) {
        NSHTTPCookieStorage *cookieStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
        NSArray *cookies = [cookieStorage cookies];
        if (!cookies)
            return;

        for (NSHTTPCookie *cookie in cookies)
            [cookieStorage deleteCookie:cookie];
        return;
    }

    CFHTTPCookieStorageDeleteAllCookies(cookieStorage);
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    return cookiesForSession(*this, firstParty, sameSiteInfo, url, frameID, pageID, DoNotIncludeHTTPOnly, includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    return cookiesForSession(*this, firstParty, sameSiteInfo, url, frameID, pageID, IncludeHTTPOnly, includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookiesForSession(*this, headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, IncludeHTTPOnly, headerFieldProxy.includeSecureCookies);
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& cookieStr) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (cookieStr.isEmpty())
        return;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = cookieStr.contains('=') ? cookieStr : cookieStr + "=";

    NSURL *cookieURL = url;
    NSDictionary *headerFields = [NSDictionary dictionaryWithObject:cookieString forKey:@"Set-Cookie"];

#if PLATFORM(MAC)
    NSArray *unfilteredCookies = [NSHTTPCookie _parsedCookiesWithResponseHeaderFields:headerFields forURL:cookieURL];
#else
    NSArray *unfilteredCookies = [NSHTTPCookie cookiesWithResponseHeaderFields:headerFields forURL:cookieURL];
#endif

    RetainPtr<NSArray> filteredCookies = filterCookies(unfilteredCookies);
    ASSERT([filteredCookies.get() count] <= 1);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldBlockCookies(firstParty, url, frameID, pageID))
        return;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
#endif

    setHTTPCookiesForURL(cookieStorage().get(), filteredCookies.get(), cookieURL, firstParty, sameSiteInfo);

    END_BLOCK_OBJC_EXCEPTIONS;
}

static NSHTTPCookieAcceptPolicy httpCookieAcceptPolicy(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!cookieStorage)
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    return static_cast<NSHTTPCookieAcceptPolicy>(CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage));
}

bool NetworkStorageSession::cookiesEnabled() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = httpCookieAcceptPolicy(cookieStorage().get());
    return cookieAcceptPolicy == NSHTTPCookieAcceptPolicyAlways || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain;

    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies) const
{
    rawCookies.clear();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = cookiesForURL(*this, firstParty, sameSiteInfo, url, frameID, pageID);
    NSUInteger count = [cookies count];
    rawCookies.reserveCapacity(count);
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        rawCookies.uncheckedAppend({ cookie });
    }

    END_BLOCK_OBJC_EXCEPTIONS;
    return true;
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& cookieName) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    NSArray *cookies = httpCookiesForURL(cookieStorage.get(), nil, std::nullopt, url);

    NSString *cookieNameString = cookieName;

    NSUInteger count = [cookies count];
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        if ([[cookie name] isEqualToString:cookieNameString])
            deleteHTTPCookie(cookieStorage.get(), cookie);
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = httpCookies(cookieStorage().get());
    
    for (NSHTTPCookie* cookie in cookies)
        hostnames.add([cookie domain]);
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::deleteAllCookies()
{
    deleteAllHTTPCookies(cookieStorage().get());
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    NSArray *cookies = httpCookies(cookieStorage.get());
    if (!cookies)
        return;

    HashMap<String, Vector<RetainPtr<NSHTTPCookie>>> cookiesByDomain;
    for (NSHTTPCookie *cookie in cookies) {
        if (!cookie.domain)
            continue;
        cookiesByDomain.ensure(cookie.domain, [] {
            return Vector<RetainPtr<NSHTTPCookie>>();
        }).iterator->value.append(cookie);
    }

    for (const auto& hostname : hostnames) {
        auto it = cookiesByDomain.find(hostname);
        if (it == cookiesByDomain.end())
            continue;

        for (auto& cookie : it->value)
            deleteHTTPCookie(cookieStorage.get(), cookie.get());
    }

    [nsCookieStorage() _saveCookies];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime timePoint)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (![NSHTTPCookieStorage instancesRespondToSelector:@selector(removeCookiesSinceDate:)])
        return;

    NSTimeInterval timeInterval = timePoint.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];

    auto *storage = nsCookieStorage();

    [storage removeCookiesSinceDate:date];
    [storage _saveCookies];
}

} // namespace WebCore

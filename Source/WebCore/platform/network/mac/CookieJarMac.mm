/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#import "PlatformCookieJar.h"

#import "CookiesStrategy.h"
#import "NetworkStorageSession.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>

#import "Cookie.h"
#import "CookieStorage.h"
#import "URL.h"
#import <wtf/Optional.h>
#import <wtf/text/StringBuilder.h>

@interface NSURL ()
- (CFURLRef)_cfurl;
@end

namespace WebCore {

static NSArray *httpCookies(CFHTTPCookieStorageRef cookieStorage)
{
    if (!cookieStorage)
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies];
    
    auto cookies = adoptCF(CFHTTPCookieStorageCopyCookies(cookieStorage));
    return [NSHTTPCookie _cf2nsCookies:cookies.get()];
}

static void deleteHTTPCookie(CFHTTPCookieStorageRef cookieStorage, NSHTTPCookie *cookie)
{
    if (!cookieStorage) {
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:cookie];
        return;
    }
    
    CFHTTPCookieStorageDeleteCookie(cookieStorage, [cookie _GetInternalCFHTTPCookie]);
}

static NSArray *httpCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSURL *firstParty, NSURL *url)
{
    if (!cookieStorage)
        cookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);

    bool secure = ![[url scheme] caseInsensitiveCompare:@"https"];
    
    auto cookies = adoptCF(_CFHTTPCookieStorageCopyCookiesForURLWithMainDocumentURL(cookieStorage, static_cast<CFURLRef>(url), static_cast<CFURLRef>(firstParty), secure));
    NSArray *nsCookies = [NSHTTPCookie _cf2nsCookies:cookies.get()];

    return nsCookies;
}

    
static RetainPtr<NSArray> filterCookies(NSArray *unfilteredCookies)
{
    NSUInteger count = [unfilteredCookies count];
    RetainPtr<NSMutableArray> filteredCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:count]);

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

        [filteredCookies.get() addObject:cookie];
    }

    return filteredCookies;
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)

static NSArray *applyPartitionToCookies(NSString *partition, NSArray *cookies)
{
    // FIXME 24747739: CFNetwork should expose this key as SPI
    static NSString * const partitionKey = @"StoragePartition";

    NSMutableArray *partitionedCookies = [NSMutableArray arrayWithCapacity:cookies.count];
    for (NSHTTPCookie *cookie in cookies) {
        RetainPtr<NSMutableDictionary> properties = adoptNS([cookie.properties mutableCopy]);
        [properties setObject:partition forKey:partitionKey];
        [partitionedCookies addObject:[NSHTTPCookie cookieWithProperties:properties.get()]];
    }

    return partitionedCookies;
}

static bool cookiesAreBlockedForURL(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    return session.shouldBlockCookies(firstParty, url);
}

static NSArray *cookiesInPartitionForURL(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID)
{
    String partition = session.cookieStoragePartition(firstParty, url, frameID, pageID);
    if (partition.isEmpty())
        return nil;

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> cookieStorage;
    if (auto storage = session.cookieStorage())
        cookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage.get()]);
    else
        cookieStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];

    // The _getCookiesForURL: method calls the completionHandler synchronously.
    std::optional<RetainPtr<NSArray *>> cookiesPtr;
    [cookieStorage _getCookiesForURL:url mainDocumentURL:firstParty partition:partition completionHandler:[&cookiesPtr](NSArray *cookies) {
        cookiesPtr = retainPtr(cookies);
    }];
    ASSERT(!!cookiesPtr);

    return cookiesPtr->autorelease();
}

#endif // HAVE(CFNETWORK_STORAGE_PARTITIONING)
    
static NSArray *cookiesForURL(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID)
{
#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    if (cookiesAreBlockedForURL(session, firstParty, url))
        return nil;
    
    if (NSArray *cookies = cookiesInPartitionForURL(session, firstParty, url, frameID, pageID))
        return cookies;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
#endif
    return httpCookiesForURL(session.cookieStorage().get(), firstParty, url);
}

enum IncludeHTTPOnlyOrNot { DoNotIncludeHTTPOnly, IncludeHTTPOnly };
static std::pair<String, bool> cookiesForSession(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeHTTPOnlyOrNot includeHTTPOnly, IncludeSecureCookies includeSecureCookies)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = cookiesForURL(session, firstParty, url, frameID, pageID);
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

static void setHTTPCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSArray *cookies, NSURL *url, NSURL *mainDocumentURL)
{
    if (!cookieStorage) {
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL];
        return;
    }

    auto cfCookies = adoptCF([NSHTTPCookie _ns2cfCookies:cookies]);
    CFHTTPCookieStorageSetCookies(cookieStorage, cfCookies.get(), [url _cfurl], [mainDocumentURL _cfurl]);
}

static void deleteAllHTTPCookies(CFHTTPCookieStorageRef cookieStorage)
{
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

std::pair<String, bool> cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return cookiesForSession(session, firstParty, url, frameID, pageID, DoNotIncludeHTTPOnly, includeSecureCookies);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return cookiesForSession(session, firstParty, url, frameID, pageID, IncludeHTTPOnly, includeSecureCookies);
}

void setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& cookieStr)
{
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

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    String partition = session.cookieStoragePartition(firstParty, url, frameID, pageID);
    if (!partition.isEmpty())
        filteredCookies = applyPartitionToCookies(partition, filteredCookies.get());
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
#endif

    setHTTPCookiesForURL(session.cookieStorage().get(), filteredCookies.get(), cookieURL, firstParty);

    END_BLOCK_OBJC_EXCEPTIONS;
}

static NSHTTPCookieAcceptPolicy httpCookieAcceptPolicy(CFHTTPCookieStorageRef cookieStorage)
{
    if (!cookieStorage)
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    return static_cast<NSHTTPCookieAcceptPolicy>(CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage));
}

bool cookiesEnabled(const NetworkStorageSession& session)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = httpCookieAcceptPolicy(session.cookieStorage().get());
    return cookieAcceptPolicy == NSHTTPCookieAcceptPolicyAlways || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain;

    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

bool getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    rawCookies.clear();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = cookiesForURL(session, firstParty, url, frameID, pageID);
    NSUInteger count = [cookies count];
    rawCookies.reserveCapacity(count);
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        rawCookies.uncheckedAppend({ cookie });
    }

    END_BLOCK_OBJC_EXCEPTIONS;
    return true;
}

void deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookieName)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = session.cookieStorage();
    NSArray *cookies = httpCookiesForURL(cookieStorage.get(), nil, url);

    NSString *cookieNameString = cookieName;

    NSUInteger count = [cookies count];
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        if ([[cookie name] isEqualToString:cookieNameString])
            deleteHTTPCookie(cookieStorage.get(), cookie);
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void getHostnamesWithCookies(const NetworkStorageSession& session, HashSet<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray *cookies = httpCookies(session.cookieStorage().get());
    
    for (NSHTTPCookie* cookie in cookies)
        hostnames.add([cookie domain]);
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void deleteAllCookies(const NetworkStorageSession& session)
{
    deleteAllHTTPCookies(session.cookieStorage().get());
}

void deleteCookiesForHostnames(const NetworkStorageSession& session, const Vector<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = session.cookieStorage();
    NSArray *cookies = httpCookies(cookieStorage.get());
    if (!cookies)
        return;

    HashMap<String, Vector<RetainPtr<NSHTTPCookie>>> cookiesByDomain;
    for (NSHTTPCookie* cookie in cookies) {
        auto& cookies = cookiesByDomain.add(cookie.domain, Vector<RetainPtr<NSHTTPCookie>>()).iterator->value;
        cookies.append(cookie);
    }

    for (const auto& hostname : hostnames) {
        auto it = cookiesByDomain.find(hostname);
        if (it == cookiesByDomain.end())
            continue;

        for (auto& cookie : it->value)
            deleteHTTPCookie(cookieStorage.get(), cookie.get());
    }

    [session.nsCookieStorage() _saveCookies];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void deleteAllCookiesModifiedSince(const NetworkStorageSession& session, WallTime timePoint)
{
    if (![NSHTTPCookieStorage instancesRespondToSelector:@selector(removeCookiesSinceDate:)])
        return;

    NSTimeInterval timeInterval = timePoint.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];

    auto *storage = session.nsCookieStorage();

    [storage removeCookiesSinceDate:date];
    [storage _saveCookies];
}

}

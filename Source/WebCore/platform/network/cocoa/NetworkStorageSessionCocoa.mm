/*
 * Copyright (C) 2015-2020 Apple Inc.  All rights reserved.
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
#import "HTTPCookieAcceptPolicyCocoa.h"
#import "SameSiteInfo.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringBuilder.h>

@interface NSURL ()
- (CFURLRef)_cfurl;
@end

namespace WebCore {

NetworkStorageSession::~NetworkStorageSession()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    unregisterCookieChangeListenersIfNecessary();
#endif
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [nsCookieStorage() setCookie:(NSHTTPCookie *)cookie];
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    auto nsCookies = createNSArray(cookies, [] (auto& cookie) -> NSHTTPCookie * {
        return cookie;
    });

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [nsCookieStorage() setCookies:nsCookies.get() forURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL];
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    [nsCookieStorage() deleteCookie:(NSHTTPCookie *)cookie];
}

static Vector<Cookie> nsCookiesToCookieVector(NSArray<NSHTTPCookie *> *nsCookies, const Function<bool(NSHTTPCookie *)>& filter = { })
{
    Vector<Cookie> cookies;
    cookies.reserveInitialCapacity(nsCookies.count);
    for (NSHTTPCookie *nsCookie in nsCookies) {
        if (!filter || filter(nsCookie))
            cookies.uncheckedAppend(nsCookie);
    }
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

void NetworkStorageSession::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    
    for (NSHTTPCookie *nsCookie in nsCookieStorage().cookies) {
        if (RegistrableDomain::uncheckedCreateFromHost(nsCookie.domain) == domain) {
            completionHandler(true);
            return;
        }
    }

    completionHandler(false);
}

void NetworkStorageSession::setAllCookiesToSameSiteStrict(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

#if HAVE(CFNETWORK_SAMESITE_COOKIE_API)
    RetainPtr<NSMutableArray<NSHTTPCookie *>> oldCookiesToDelete = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<NSHTTPCookie *>> newCookiesToAdd = adoptNS([[NSMutableArray alloc] init]);

    for (NSHTTPCookie *nsCookie in nsCookieStorage().cookies) {
        if (RegistrableDomain::uncheckedCreateFromHost(nsCookie.domain) == domain && nsCookie.sameSitePolicy != NSHTTPCookieSameSiteStrict) {
            [oldCookiesToDelete addObject:nsCookie];
            RetainPtr<NSMutableDictionary<NSHTTPCookiePropertyKey, id>> mutableProperties = adoptNS([[nsCookie properties] mutableCopy]);
            mutableProperties.get()[NSHTTPCookieSameSitePolicy] = NSHTTPCookieSameSiteStrict;
            NSHTTPCookie *strictCookie = [NSHTTPCookie cookieWithProperties:mutableProperties.get()];
            [newCookiesToAdd addObject:strictCookie];
        }
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    for (NSHTTPCookie *oldCookie in oldCookiesToDelete.get())
        deleteHTTPCookie(cookieStorage().get(), oldCookie);

    for (NSHTTPCookie *newCookie in newCookiesToAdd.get())
        [nsCookieStorage() setCookie:newCookie];
    END_BLOCK_OBJC_EXCEPTIONS
#else
    UNUSED_PARAM(domain);
#endif
    completionHandler();
}

void NetworkStorageSession::flushCookieStore()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [nsCookieStorage() _saveCookies];
}

NSHTTPCookieStorage *NetworkStorageSession::nsCookieStorage() const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    auto cfCookieStorage = cookieStorage();
    if (!cfCookieStorage || [NSHTTPCookieStorage sharedHTTPCookieStorage]._cookieStorage == cfCookieStorage)
        return [NSHTTPCookieStorage sharedHTTPCookieStorage];

    return [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cfCookieStorage.get()] autorelease];
}

CookieStorageObserver& NetworkStorageSession::cookieStorageObserver() const
{
    if (!m_cookieStorageObserver)
        m_cookieStorageObserver = makeUnique<CookieStorageObserver>(nsCookieStorage());

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

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    // FIXME: Use _CFHTTPCookieStorageGetDefault when USE(CFNETWORK) is defined in WebKit for consistency.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy]);

    return storageSession.leakRef();
}

NSArray *NetworkStorageSession::httpCookies(CFHTTPCookieStorageRef cookieStorage) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies];
    }
    
    auto cookies = adoptCF(CFHTTPCookieStorageCopyCookies(cookieStorage));
    return [NSHTTPCookie _cf2nsCookies:cookies.get()];
}

void NetworkStorageSession::deleteHTTPCookie(CFHTTPCookieStorageRef cookieStorage, NSHTTPCookie *cookie) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:cookie];
        return;
    }
    
    CFHTTPCookieStorageDeleteCookie(cookieStorage, [cookie _GetInternalCFHTTPCookie]);
}

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
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

static NSArray *cookiesForURL(NSHTTPCookieStorage *storage, NSURL *url, NSURL *mainDocumentURL, const Optional<SameSiteInfo>& sameSiteInfo, NSString *partition = nullptr)
{
    // The _getCookiesForURL: method calls the completionHandler synchronously. We use Optional<> to ensure this invariant.
    Optional<RetainPtr<NSArray>> cookiesPtr;
    auto completionHandler = [&cookiesPtr] (NSArray *cookies) {
        cookiesPtr = retainPtr(cookies);
    };
// FIXME: Seems like this newer code path can be used for watchOS and tvOS too.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
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

void NetworkStorageSession::setHTTPCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSArray *cookies, NSURL *url, NSURL *mainDocumentURL, const SameSiteInfo& sameSiteInfo) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
// FIXME: Seems like this newer code path can be used for watchOS and tvOS too.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        if ([NSHTTPCookieStorage instancesRespondToSelector:@selector(_setCookies:forURL:mainDocumentURL:policyProperties:)])
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url).get()];
        else
#endif
            [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL];
        return;
    }
// FIXME: Seems like this newer code path can be used for watchOS and tvOS too.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if ([NSHTTPCookieStorage instancesRespondToSelector:@selector(_setCookies:forURL:mainDocumentURL:policyProperties:)]) {
        // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
        // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
        RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
        [nsCookieStorage _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url).get()];
    } else {
#endif
        auto cfCookies = adoptCF([NSHTTPCookie _ns2cfCookies:cookies]);
        CFHTTPCookieStorageSetCookies(cookieStorage, cfCookies.get(), [url _cfurl], [mainDocumentURL _cfurl]);
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    }
#else
    UNUSED_PARAM(sameSiteInfo);
#endif
}

NSArray *NetworkStorageSession::httpCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSURL *firstParty, const Optional<SameSiteInfo>& sameSiteInfo, NSURL *url) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        cookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    }

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
    return WebCore::cookiesForURL(nsCookieStorage.get(), url, firstParty, sameSiteInfo);
}

NSHTTPCookie *NetworkStorageSession::capExpiryOfPersistentCookie(NSHTTPCookie *cookie, Seconds cap)
{
    if ([cookie isSessionOnly])
        return cookie;

    if (!cookie.expiresDate || cookie.expiresDate.timeIntervalSinceNow > cap.seconds()) {
        auto properties = adoptNS([[cookie properties] mutableCopy]);
        auto date = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:cap.seconds()]);
        [properties setObject:date.get() forKey:NSHTTPCookieExpires];
        cookie = [NSHTTPCookie cookieWithProperties:properties.get()];
    }
    return cookie;
}

NSArray *NetworkStorageSession::cookiesForURL(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldAskITP == ShouldAskITP::Yes && shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking))
        return nil;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(shouldAskITP);
#endif
    return httpCookiesForURL(cookieStorage().get(), firstParty, sameSiteInfo, url);
}

std::pair<String, bool> NetworkStorageSession::cookiesForSession(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeHTTPOnlyOrNot includeHTTPOnly, IncludeSecureCookies includeSecureCookies, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSArray *cookies = cookiesForURL(firstParty, sameSiteInfo, url, frameID, pageID, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
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

    END_BLOCK_OBJC_EXCEPTIONS
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

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, DoNotIncludeHTTPOnly, includeSecureCookies, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, IncludeHTTPOnly, includeSecureCookies, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookiesForSession(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, IncludeHTTPOnly, headerFieldProxy.includeSecureCookies, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No);
}

static NSHTTPCookie *parseDOMCookie(String cookieString, NSURL* cookieURL, Optional<Seconds> cappedLifetime)
{
    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (cookieString.isEmpty())
        return nil;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    cookieString = cookieString.contains('=') ? cookieString : cookieString + "=";

    NSHTTPCookie *cookie = [NSHTTPCookie _cookieForSetCookieString:cookieString forURL:cookieURL partition:nil];
    if (!cookie)
        return nil;

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
        return NetworkStorageSession::capExpiryOfPersistentCookie(cookie, *cappedLifetime);

    return cookie;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldAskITP shouldAskITP, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldAskITP == ShouldAskITP::Yes && shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking))
        return;
#else
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(shouldAskITP);
#endif

    NSURL *cookieURL = url;

    Optional<Seconds> cookieCap;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    cookieCap = clientSideCookieCap(RegistrableDomain { firstParty }, pageID);
#endif

    NSHTTPCookie *cookie = parseDOMCookie(cookieString, cookieURL, cookieCap);
    if (!cookie)
        return;

    setHTTPCookiesForURL(cookieStorage().get(), @[cookie], cookieURL, firstParty, sameSiteInfo);

    END_BLOCK_OBJC_EXCEPTIONS
}

static NSHTTPCookieAcceptPolicy httpCookieAcceptPolicy(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!cookieStorage)
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];

    return static_cast<NSHTTPCookieAcceptPolicy>(CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage));
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto policy = httpCookieAcceptPolicy(cookieStorage().get());
    return toHTTPCookieAcceptPolicy(policy);
    END_BLOCK_OBJC_EXCEPTIONS

    return HTTPCookieAcceptPolicy::Never;
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldAskITP shouldAskITP, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    rawCookies.clear();
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSArray *cookies = cookiesForURL(firstParty, sameSiteInfo, url, frameID, pageID, shouldAskITP, shouldRelaxThirdPartyCookieBlocking);
    NSUInteger count = [cookies count];
    rawCookies.reserveCapacity(count);
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        rawCookies.uncheckedAppend({ cookie });
    }

    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& cookieName) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    NSArray *cookies = httpCookiesForURL(cookieStorage.get(), nil, WTF::nullopt, url);

    NSString *cookieNameString = cookieName;

    NSUInteger count = [cookies count];
    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[cookies objectAtIndex:i];
        if ([[cookie name] isEqualToString:cookieNameString])
            deleteHTTPCookie(cookieStorage.get(), cookie);
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSArray *cookies = httpCookies(cookieStorage().get());
    
    for (NSHTTPCookie* cookie in cookies) {
        if (NSString *domain = [cookie domain])
            hostnames.add(domain);
        else
            ASSERT_NOT_REACHED();
    }
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteAllCookies()
{
    deleteAllHTTPCookies(cookieStorage().get());
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames)
{
    deleteCookiesForHostnames(hostnames, IncludeHttpOnlyCookies::Yes);
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames, IncludeHttpOnlyCookies includeHttpOnlyCookies)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    NSArray *cookies = httpCookies(cookieStorage.get());
    if (!cookies)
        return;

    HashMap<String, Vector<RetainPtr<NSHTTPCookie>>> cookiesByDomain;
    for (NSHTTPCookie *cookie in cookies) {
        if (!cookie.domain || (includeHttpOnlyCookies == IncludeHttpOnlyCookies::No && cookie.isHTTPOnly))
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

    END_BLOCK_OBJC_EXCEPTIONS
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

Vector<Cookie> NetworkStorageSession::domCookiesForHost(const String& host)
{
    NSArray *nsCookies = [nsCookieStorage() _getCookiesForDomain:(NSString *)host];
    return nsCookiesToCookieVector(nsCookies, [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)

void NetworkStorageSession::registerCookieChangeListenersIfNecessary()
{
    if (m_didRegisterCookieListeners)
        return;

    m_didRegisterCookieListeners = true;

    [nsCookieStorage() _setCookiesChangedHandler:^(NSArray<NSHTTPCookie*>* addedCookies, NSString* domainForChangedCookie) {
        String host = domainForChangedCookie;
        auto it = m_cookieChangeObservers.find(host);
        if (it == m_cookieChangeObservers.end())
            return;
        auto cookies = nsCookiesToCookieVector(addedCookies, [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
        if (cookies.isEmpty())
            return;
        for (auto* observer : it->value)
            observer->cookiesAdded(host, cookies);
    } onQueue:dispatch_get_main_queue()];

    [nsCookieStorage() _setCookiesRemovedHandler:^(NSArray<NSHTTPCookie*>* removedCookies, NSString* domainForRemovedCookies, bool removeAllCookies) {
        if (removeAllCookies) {
            for (auto& observers : m_cookieChangeObservers.values()) {
                for (auto* observer : observers)
                    observer->allCookiesDeleted();
            }
            return;
        }

        String host = domainForRemovedCookies;
        auto it = m_cookieChangeObservers.find(host);
        if (it == m_cookieChangeObservers.end())
            return;

        auto cookies = nsCookiesToCookieVector(removedCookies, [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
        if (cookies.isEmpty())
            return;
        for (auto* observer : it->value)
            observer->cookiesDeleted(host, cookies);
    } onQueue:dispatch_get_main_queue()];
}

void NetworkStorageSession::unregisterCookieChangeListenersIfNecessary()
{
    if (!m_didRegisterCookieListeners)
        return;

    [nsCookieStorage() _setCookiesChangedHandler:nil onQueue:nil];
    [nsCookieStorage() _setCookiesRemovedHandler:nil onQueue:nil];

    [nsCookieStorage() _setSubscribedDomainsForCookieChanges:nil];
    m_didRegisterCookieListeners = false;
}

void NetworkStorageSession::startListeningForCookieChangeNotifications(CookieChangeObserver& observer, const String& host)
{
    registerCookieChangeListenersIfNecessary();

    auto& observers = m_cookieChangeObservers.ensure(host, [] {
        return HashSet<CookieChangeObserver*> { };
    }).iterator->value;
    ASSERT(!observers.contains(&observer));
    observers.add(&observer);

    if (!m_subscribedDomainsForCookieChanges)
        m_subscribedDomainsForCookieChanges = adoptNS([[NSMutableSet alloc] init]);
    else if ([m_subscribedDomainsForCookieChanges containsObject:(NSString *)host])
        return;

    [m_subscribedDomainsForCookieChanges addObject:(NSString *)host];
    [nsCookieStorage() _setSubscribedDomainsForCookieChanges:m_subscribedDomainsForCookieChanges.get()];
}

void NetworkStorageSession::stopListeningForCookieChangeNotifications(CookieChangeObserver& observer, const HashSet<String>& hosts)
{
    bool subscribedURLsChanged = false;
    for (auto& host : hosts) {
        auto it = m_cookieChangeObservers.find(host);
        ASSERT(it != m_cookieChangeObservers.end());
        if (it == m_cookieChangeObservers.end())
            continue;

        auto& observers = it->value;
        ASSERT(observers.contains(&observer));
        observers.remove(&observer);
        if (observers.isEmpty()) {
            m_cookieChangeObservers.remove(it);
            ASSERT([m_subscribedDomainsForCookieChanges containsObject:(NSString *)host]);
            [m_subscribedDomainsForCookieChanges removeObject:(NSString *)host];
            subscribedURLsChanged = true;
        }
    }
    if (subscribedURLsChanged)
        [nsCookieStorage() _setSubscribedDomainsForCookieChanges:m_subscribedDomainsForCookieChanges.get()];
}

// FIXME: This can eventually go away, this is merely to ensure a smooth transition to the new API.
bool NetworkStorageSession::supportsCookieChangeListenerAPI() const
{
    static const bool supportsAPI = [nsCookieStorage() respondsToSelector:@selector(_setCookiesChangedHandler:onQueue:)];
    return supportsAPI;
}

#endif // HAVE(COOKIE_CHANGE_LISTENER_API)

} // namespace WebCore

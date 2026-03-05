/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#import "ClientOrigin.h"
#import "Cookie.h"
#import "CookieRequestHeaderFieldProxy.h"
#import "CookieStorageObserver.h"
#import "CookieStoreGetOptions.h"
#import "HTTPCookieAcceptPolicyCocoa.h"
#import "ResourceRequest.h"
#import "SameSiteInfo.h"
#import <algorithm>
#import <optional>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/cf/StringConcatenateCF.h>

@interface NSURL ()
- (CFURLRef)_cfurl;
@end

namespace WebCore {

NetworkStorageSession::~NetworkStorageSession()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    unregisterCookieChangeListenersIfNecessary();
#endif
    clearCookiesVersionChangeCallbacks();
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [nsCookieStorage() setCookie:cookie.createNSHTTPCookie().get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::setCookie(const Cookie& cookie, const URL& url, const URL& mainDocumentURL)
{
    setCookies({ cookie }, url, mainDocumentURL);
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    auto nsCookies = createNSArray(cookies, [] (auto& cookie) -> NSHTTPCookie * {
        return cookie.createNSHTTPCookie().autorelease();
    });

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [nsCookieStorage() setCookies:nsCookies.get() forURL:url.createNSURL().get() mainDocumentURL:mainDocumentURL.createNSURL().get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    auto work = [completionHandler = WTF::move(completionHandler), cookieStorage = RetainPtr { nsCookieStorage() }, cookie = cookie.createNSHTTPCookie()] () mutable {
        [cookieStorage deleteCookie:cookie.get()];
        ensureOnMainThread(WTF::move(completionHandler));
    };

    if (m_isInMemoryCookieStore)
        return work();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr(WTF::move(work)).get());
}

static Vector<Cookie> nsCookiesToCookieVector(NSArray<NSHTTPCookie *> *nsCookies, NOESCAPE const Function<bool(NSHTTPCookie *)>& filter = { })
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

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector(retainPtr([nsCookieStorage() cookies]).get());
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector(retainPtr([nsCookieStorage() cookiesForURL:url.createNSURL().get()]).get());
}

void NetworkStorageSession::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    bool hasCookieForDomain = false;
    
    for (NSHTTPCookie *nsCookie in [nsCookieStorage() cookies]) {
        if (RegistrableDomain::uncheckedCreateFromHost(nsCookie.domain) == domain) {
            hasCookieForDomain = true;
            break;
        }
    }

    // FIXME: rdar://168454473 (Remove workaround in CookieStorageObserver once CFNetwork bug is resolved)
    if (m_cookieStorageObserver && cookieStorage().get())
        protect(cookieStorageObserver())->registerInternalsForNotifications(true);

    completionHandler(hasCookieForDomain);
}

void NetworkStorageSession::setAllCookiesToSameSiteStrict(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    RetainPtr<NSMutableArray<NSHTTPCookie *>> oldCookiesToDelete = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<NSHTTPCookie *>> newCookiesToAdd = adoptNS([[NSMutableArray alloc] init]);

    for (NSHTTPCookie *nsCookie in [nsCookieStorage() cookies]) {
        if (RegistrableDomain::uncheckedCreateFromHost(nsCookie.domain) == domain && nsCookie.sameSitePolicy != NSHTTPCookieSameSiteStrict) {
            [oldCookiesToDelete addObject:nsCookie];
            RetainPtr<NSMutableDictionary<NSHTTPCookiePropertyKey, id>> mutableProperties = adoptNS([[nsCookie properties] mutableCopy]);
            mutableProperties.get()[NSHTTPCookieSameSitePolicy] = NSHTTPCookieSameSiteStrict;
            RetainPtr strictCookie = adoptNS([[NSHTTPCookie alloc] initWithProperties:mutableProperties.get()]);
            [newCookiesToAdd addObject:strictCookie.get()];
        }
    }

    auto aggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler), newCookiesToAdd = WTF::move(newCookiesToAdd), cookieStorage = RetainPtr { nsCookieStorage() }] () mutable {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        for (NSHTTPCookie *newCookie in newCookiesToAdd.get())
            [cookieStorage setCookie:newCookie];
        END_BLOCK_OBJC_EXCEPTIONS
        completionHandler();
    });

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    for (NSHTTPCookie *oldCookie in oldCookiesToDelete.get())
        deleteHTTPCookie(cookieStorage().get(), oldCookie, [aggregator] { });
    END_BLOCK_OBJC_EXCEPTIONS
}

RetainPtr<NSHTTPCookieStorage> NetworkStorageSession::nsCookieStorage() const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    auto cfCookieStorage = cookieStorage();
    ASSERT(cfCookieStorage || !m_isInMemoryCookieStore);
    if (!m_isInMemoryCookieStore && (!cfCookieStorage || [NSHTTPCookieStorage sharedHTTPCookieStorage]._cookieStorage == cfCookieStorage))
        return [NSHTTPCookieStorage sharedHTTPCookieStorage];

    return adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cfCookieStorage.get()]);
}

CookieStorageObserver& NetworkStorageSession::cookieStorageObserver() const
{
    if (!m_cookieStorageObserver)
        m_cookieStorageObserver = makeUnique<CookieStorageObserver>(nsCookieStorage().get());

    return *m_cookieStorageObserver;
}

RetainPtr<CFURLStorageSessionRef> createPrivateStorageSession(CFStringRef identifier, std::optional<HTTPCookieAcceptPolicy> cookieAcceptPolicy, NetworkStorageSession::ShouldDisableCFURLCache shouldDisableCFURLCache)
{
    const void* sessionPropertyKeys[] = { _kCFURLStorageSessionIsPrivate };
    const void* sessionPropertyValues[] = { kCFBooleanTrue };
    auto sessionProperties = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, sessionPropertyKeys, sessionPropertyValues, sizeof(sessionPropertyKeys) / sizeof(*sessionPropertyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, sessionProperties.get()));

    if (!storageSession)
        return nullptr;

    if (shouldDisableCFURLCache == NetworkStorageSession::ShouldDisableCFURLCache::Yes)
        _CFURLStorageSessionDisableCache(storageSession.get());

    // The private storage session should have the same properties as the default storage session,
    // with the exception that it should be in-memory only storage.

    // FIXME 9199649: If any of the storages do not exist, do no use the storage session.
    // This could occur if there is an issue figuring out where to place a storage on disk (e.g. the
    // sandbox does not allow CFNetwork access).

    if (shouldDisableCFURLCache == NetworkStorageSession::ShouldDisableCFURLCache::No) {
        auto cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
        if (!cache)
            return nullptr;

        CFURLCacheSetMemoryCapacity(cache.get(), [[NSURLCache sharedURLCache] memoryCapacity]);
    }

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
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

RetainPtr<NSArray> NetworkStorageSession::httpCookies(CFHTTPCookieStorageRef cookieStorage) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies];
    }
    
    auto cookies = adoptCF(CFHTTPCookieStorageCopyCookies(cookieStorage));
    return [NSHTTPCookie _cf2nsCookies:cookies.get()];
}

void NetworkStorageSession::deleteHTTPCookie(CFHTTPCookieStorageRef cookieStorage, NSHTTPCookie *cookie, CompletionHandler<void()>&& completionHandler) const
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
    policyProperties.get()[@"_kCFHTTPCookiePolicyPropertySiteForCookies"] = sameSiteInfo.isSameSite ? url : URL::emptyNSURL();
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

void NetworkStorageSession::setHTTPCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSArray *cookies, NSURL *url, NSURL *mainDocumentURL, NSString *partition, const SameSiteInfo& sameSiteInfo, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    if (!cookieStorage) {
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url, partition, thirdPartyCookieBlockingDecision).get()];
        return;
    }

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
    [nsCookieStorage _setCookies:cookies forURL:url mainDocumentURL:mainDocumentURL policyProperties:policyProperties(sameSiteInfo, url, partition, thirdPartyCookieBlockingDecision).get()];
}

RetainPtr<NSArray> NetworkStorageSession::httpCookiesForURL(CFHTTPCookieStorageRef cookieStorage, NSURL *firstParty, const std::optional<SameSiteInfo>& sameSiteInfo, NSURL *url, ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);
    if (!cookieStorage) {
        RELEASE_ASSERT(!m_isInMemoryCookieStore);
        cookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    }

    // FIXME: Stop creating a new NSHTTPCookieStorage object each time we want to query the cookie jar.
    // NetworkStorageSession could instead keep a NSHTTPCookieStorage object for us.
    RetainPtr<NSHTTPCookieStorage> nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage]);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    RetainPtr partitionKey = isOptInCookiePartitioningEnabled() ? cookiePartitionIdentifier(firstParty).createNSString() : nil;
#else
    RetainPtr<NSString> partitionKey;
#endif
    return cookiesForURLFromStorage(nsCookieStorage.get(), url, firstParty, sameSiteInfo, thirdPartyCookieBlockingDecision, partitionKey.get());
}

RetainPtr<NSHTTPCookie> NetworkStorageSession::capExpiryOfPersistentCookie(NSHTTPCookie *cookie, Seconds cap)
{
    if ([cookie isSessionOnly])
        return cookie;

    if (!cookie.expiresDate || cookie.expiresDate.timeIntervalSinceNow > cap.seconds()) {
        auto properties = adoptNS([[cookie properties] mutableCopy]);
        auto date = adoptNS([[NSDate alloc] initWithTimeIntervalSinceNow:cap.seconds()]);
        [properties setObject:date.get() forKey:NSHTTPCookieExpires];
        return adoptNS([[NSHTTPCookie alloc] initWithProperties:properties.get()]);
    }
    return cookie;
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
NSHTTPCookie *NetworkStorageSession::setCookiePartition(NSHTTPCookie *cookie, NSString* partitionKey)
{
    if (!cookie)
        return cookie;

    if (!partitionKey)
        return cookie;

    if (cookie._storagePartition) {
        ASSERT(cookie._storagePartition == partitionKey);
        return cookie;
    }

    auto properties = adoptNS([[cookie properties] mutableCopy]);
    [properties setObject:partitionKey forKey:@"StoragePartition"];
    return [NSHTTPCookie cookieWithProperties:properties.get()];
}
#endif

RetainPtr<NSArray> NetworkStorageSession::cookiesForURL(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All)
        return nil;
    return httpCookiesForURL(cookieStorage().get(), firstParty.createNSURL().get(), sameSiteInfo, url.createNSURL().get(), thirdPartyCookieBlockingDecision);
}

std::pair<String, bool> NetworkStorageSession::cookiesForSession(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto cookies = cookiesForURL(firstParty, sameSiteInfo, url, frameID, pageID, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (![cookies count])
        return { String(), false }; // Return a null string; StringBuilder below would create an empty one.

    StringBuilder cookiesBuilder;
    bool didAccessSecureCookies = false;
    for (NSHTTPCookie *cookie in cookies.get()) {
        if (![[cookie name] length])
            continue;
        if (cookiesFor == CookiesFor::DOM && [cookie isHTTPOnly])
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

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForSessionAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker, CookieStoreGetOptions&& options) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto cookies = cookiesForURL(firstParty, sameSiteInfo, url, frameID, pageID, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (![cookies count])
        return Vector<Cookie> { };

    Vector<Cookie> cookiesVector;
    RetainPtr name = options.name.createNSString();
    for (NSHTTPCookie *cookie in cookies.get()) {
        if (![[cookie name] length])
            continue;
        if (cookiesFor == CookiesFor::DOM && [cookie isHTTPOnly])
            continue;
        if ([cookie isSecure] && includeSecureCookies == IncludeSecureCookies::No)
            continue;
        if (!options.name.isNull() && ![[cookie name] isEqualToString:name.get()])
            continue;

        cookiesVector.append(Cookie(cookie));
    }
    return cookiesVector;

    END_BLOCK_OBJC_EXCEPTIONS
    return std::nullopt;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::DOM, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker, CookieStoreGetOptions&& options) const
{
    return cookiesForSessionAsVector(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::DOM, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker, WTF::move(options));
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::HTTP, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookiesForSession(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, CookiesFor::HTTP, headerFieldProxy.includeSecureCookies, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No);
}

static RetainPtr<NSHTTPCookie> adjustScriptWrittenCookie(NSHTTPCookie *initialCookie, std::optional<Seconds> cappedLifetime)
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
        return NetworkStorageSession::capExpiryOfPersistentCookie(cookie.get(), *cappedLifetime);

    return cookie;
}

static RetainPtr<NSHTTPCookie> parseDOMCookie(String cookieString, NSURL* cookieURL, std::optional<Seconds> cappedLifetime, const String& partition)
{
    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (cookieString.isEmpty())
        return nil;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    cookieString = cookieString.contains('=') ? cookieString : makeString(cookieString, '=');

    return adjustScriptWrittenCookie([NSHTTPCookie _cookieForSetCookieString:cookieString.createNSString().get() forURL:cookieURL partition:nsStringNilIfEmpty(partition).get()], cappedLifetime);
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(thirdPartyCookieBlockingDecision))
        return;

    RetainPtr cookieURL = url.createNSURL();

    auto cookieCap = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    String partitionKey = isOptInCookiePartitioningEnabled() ? cookiePartitionIdentifier(firstParty) : String { };
#else
    String partitionKey;
#endif

    RetainPtr cookie = parseDOMCookie(cookieString, cookieURL.get(), cookieCap, partitionKey);
    if (!cookie)
        return;

    setHTTPCookiesForURL(cookieStorage().get(), @[cookie.get()], cookieURL.get(), firstParty.createNSURL().get(), nsStringNilIfEmpty(partitionKey).get(), sameSiteInfo, thirdPartyCookieBlockingDecision);

    END_BLOCK_OBJC_EXCEPTIONS
}

bool NetworkStorageSession::setCookieFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const Cookie& cookie, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(thirdPartyCookieBlockingDecision))
        return false;

    auto expiryCap = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);
    RetainPtr nshttpCookie = adjustScriptWrittenCookie(cookie.createNSHTTPCookie().get(), expiryCap);
    if (!nshttpCookie)
        return false;

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    RetainPtr partition = isOptInCookiePartitioningEnabled() ? nsStringNilIfEmpty(cookiePartitionIdentifier(firstParty)) : nil;
#else
    RetainPtr<NSString> partition;
#endif

    setHTTPCookiesForURL(cookieStorage().get(), @[ nshttpCookie.get() ], url.createNSURL().get(), firstParty.createNSURL().get(), partition.get(), sameSiteInfo, thirdPartyCookieBlockingDecision);
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
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

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<NSArray> cookies = cookiesForURL(firstParty, sameSiteInfo, url, frameID, pageID, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker::No);
    NSUInteger count = [cookies count];
    rawCookies = Vector<Cookie>(count, [cookies](size_t i) {
        return Cookie { checked_objc_cast<NSHTTPCookie>([cookies objectAtIndex:i]) };
    });

    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void NetworkStorageSession::deleteCookie(const URL& firstParty, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler) const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto aggregator = CallbackAggregator::create(WTF::move(completionHandler));
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    RetainPtr<NSArray> cookies = httpCookiesForURL(cookieStorage.get(), firstParty.createNSURL().get(), std::nullopt, url.createNSURL().get(), ThirdPartyCookieBlockingDecision::None);

    RetainPtr cookieNameString = cookieName.createNSString();

    NSUInteger count = [cookies count];
    for (NSUInteger i = 0; i < count; ++i) {
        RetainPtr<NSHTTPCookie> cookie = [cookies objectAtIndex:i];
        if ([[cookie name] isEqualToString:cookieNameString.get()])
            deleteHTTPCookie(cookieStorage.get(), cookie.get(), [aggregator] { });
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<NSArray> cookies = httpCookies(cookieStorage().get());
    
    for (NSHTTPCookie* cookie in cookies.get()) {
        if (RetainPtr<NSString> domain = [cookie domain])
            hostnames.add(domain.get());
        else
            ASSERT_NOT_REACHED();
    }
    
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
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

void NetworkStorageSession::deleteCookiesMatching(NOESCAPE const Function<bool(NSHTTPCookie *)>& matches, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || m_isInMemoryCookieStore);

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    auto nsCookieStorage = adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorage.get()]);
    auto aggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler), nsCookieStorage = WTF::move(nsCookieStorage)] () mutable {
        [nsCookieStorage _saveCookies:makeBlockPtr([completionHandler = WTF::move(completionHandler)] () mutable {
            ensureOnMainThread(WTF::move(completionHandler));
        }).get()];
    });

    RetainPtr<NSArray> cookies = httpCookies(cookieStorage.get());
    if (!cookies)
        return;

    for (NSHTTPCookie *cookie in cookies.get()) {
        @autoreleasepool {
            if (matches(cookie))
                deleteHTTPCookie(cookieStorage.get(), cookie, [aggregator] { });
        }
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteCookies(const ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    Vector<String> cachePartitions { cookiePartitionIdentifier(origin.topOrigin.toURL()) };
    if (origin.topOrigin == origin.clientOrigin)
        cachePartitions.append({ });
    auto domain = origin.clientOrigin.host();

    deleteCookiesMatching([&domain, &cachePartitions](auto *cookie) {
        bool partitionMatched = std::ranges::any_of(cachePartitions, [&cookie](auto& cachePartition) {
            return equalIgnoringNullity(cachePartition, String(cookie._storagePartition));
        });
        return partitionMatched && domain == String(cookie.domain);
    }, WTF::move(completionHandler));
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames, IncludeHttpOnlyCookies includeHttpOnlyCookies, ScriptWrittenCookiesOnly scriptWrittenCookiesOnly, CompletionHandler<void()>&& completionHandler)
{
    HashSet<String> hostnamesSet;
    for (auto& hostname : hostnames)
        hostnamesSet.add(hostname);

    deleteCookiesMatching([&](NSHTTPCookie* cookie) {
        if (!cookie.domain || (includeHttpOnlyCookies == IncludeHttpOnlyCookies::No && cookie.isHTTPOnly))
            return false;
#if ENABLE(JS_COOKIE_CHECKING)
        bool setInJS = [retainPtr([cookie properties]) valueForKey:@"SetInJavaScript"];
        if (scriptWrittenCookiesOnly == ScriptWrittenCookiesOnly::Yes && !setInJS)
            return false;
#else
        UNUSED_PARAM(scriptWrittenCookiesOnly);
#endif
        return hostnamesSet.contains(String(cookie.domain));
    }, WTF::move(completionHandler));
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime timePoint, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    // FIXME: Do we still need this check? Probably not.
    if (![NSHTTPCookieStorage instancesRespondToSelector:@selector(removeCookiesSinceDate:)])
        return completionHandler();

    NSTimeInterval timeInterval = timePoint.secondsSinceEpoch().seconds();
    auto work = [completionHandler = WTF::move(completionHandler), storage = RetainPtr { nsCookieStorage() }, date = RetainPtr { [NSDate dateWithTimeIntervalSince1970:timeInterval] }] () mutable {
        [storage removeCookiesSinceDate:date.get()];
        [storage _saveCookies:makeBlockPtr([completionHandler = WTF::move(completionHandler)] () mutable {
            ensureOnMainThread(WTF::move(completionHandler));
        }).get()];
    };

    if (m_isInMemoryCookieStore)
        return work();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr(WTF::move(work)).get());
}

Vector<Cookie> NetworkStorageSession::domCookiesForHost(const URL& firstParty)
{
    RetainPtr host = firstParty.host().createNSString();

    // _getCookiesForDomain only returned unpartitioned (i.e., nil partition) cookies
    RetainPtr<NSArray> unpartitionedCookies = [nsCookieStorage() _getCookiesForDomain:host.get()];
    RetainPtr nsCookies = adoptNS([[NSMutableArray alloc] initWithArray:unpartitionedCookies.get()]);

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    if (isOptInCookiePartitioningEnabled()) {
        // Next, get all cookies in the partition for this site. However, we
        // only want the cookies for this host, so we filter all cookies that
        // don't match.
        // The _getCookiesForPartition: method calls the
        // completionHandler synchronously. We crash if this invariant is not
        // met.
        bool wasCompletionHandlerCalled { false };
        RetainPtr partitionKey = cookiePartitionIdentifier(firstParty).createNSString();
        auto completionHandler = [&wasCompletionHandlerCalled, &nsCookies, &host, &partitionKey, &firstParty] (NSArray *cookies) {
            wasCompletionHandlerCalled = true;

            RetainPtr registrableDomain = RegistrableDomain { firstParty }.string().createNSString();
            for (NSHTTPCookie *nsCookie in cookies) {
                if (![nsCookie.domain hasSuffix:registrableDomain.get()])
                    continue;
                if (![host hasSuffix:nsCookie.domain])
                    continue;

                ASSERT([nsCookie._storagePartition isEqualToString:partitionKey.get()]);
                if (![nsCookie._storagePartition isEqualToString:partitionKey.get()])
                    continue;

                [nsCookies addObject:nsCookie];
            }
        };

        [nsCookieStorage() _getCookiesForPartition:partitionKey.get() completionHandler:completionHandler];
        RELEASE_ASSERT(wasCompletionHandlerCalled);
    }
#endif

    return nsCookiesToCookieVector(nsCookies.get(), [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
void NetworkStorageSession::setOptInCookiePartitioningEnabled(bool enabled)
{
#if defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    m_isOptInCookiePartitioningEnabled = enabled;
#else
    RELEASE_ASSERT(m_thirdPartyCookieBlockingMode != WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned);
    UNUSED_PARAM(enabled);
#endif
}
#endif

#if HAVE(COOKIE_CHANGE_LISTENER_API)

void NetworkStorageSession::registerCookieChangeListenersIfNecessary()
{
    if (m_didRegisterCookieListeners)
        return;

    m_didRegisterCookieListeners = true;

    [nsCookieStorage() _setCookiesChangedHandler:makeBlockPtr([weakThis = WeakPtr { *this }](NSArray<NSHTTPCookie *> *addedCookies, NSString *domainForChangedCookie) {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;
        String host = domainForChangedCookie;
        auto it = checkedThis->m_cookieChangeObservers.find(host);
        if (it == checkedThis->m_cookieChangeObservers.end())
            return;
        auto cookies = nsCookiesToCookieVector(addedCookies, [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
        if (cookies.isEmpty())
            return;
        for (Ref observer : it->value)
            observer->cookiesAdded(host, cookies);
    }).get() onQueue:mainDispatchQueueSingleton()];

    [nsCookieStorage() _setCookiesRemovedHandler:makeBlockPtr([weakThis = WeakPtr { *this }](NSArray<NSHTTPCookie *> *removedCookies, NSString *domainForRemovedCookies, bool removeAllCookies) {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;
        if (removeAllCookies) {
            for (auto& observers : checkedThis->m_cookieChangeObservers.values()) {
                for (Ref observer : observers)
                    observer->allCookiesDeleted();
            }
            return;
        }

        String host = domainForRemovedCookies;
        auto it = checkedThis->m_cookieChangeObservers.find(host);
        if (it == checkedThis->m_cookieChangeObservers.end())
            return;

        auto cookies = nsCookiesToCookieVector(removedCookies, [](NSHTTPCookie *cookie) { return !cookie.HTTPOnly; });
        if (cookies.isEmpty())
            return;
        for (Ref observer : it->value)
            observer->cookiesDeleted(host, cookies);
    }).get() onQueue:mainDispatchQueueSingleton()];
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

bool NetworkStorageSession::startListeningForCookieChangeNotifications(CookieChangeObserver& observer, const URL& url, const URL& firstParty, FrameIdentifier frameID, PageIdentifier pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker)
{
    if (shouldBlockCookies(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker))
        return false;

    registerCookieChangeListenersIfNecessary();

    auto host = url.host().toString();
    auto& observers = m_cookieChangeObservers.ensure(host, [] {
        return WeakHashSet<CookieChangeObserver> { };
    }).iterator->value;

    observers.add(observer);

    if (!m_subscribedDomainsForCookieChanges)
        m_subscribedDomainsForCookieChanges = adoptNS([[NSMutableSet alloc] init]);
    else if ([m_subscribedDomainsForCookieChanges containsObject:host.createNSString().get()])
        return true;

    [m_subscribedDomainsForCookieChanges addObject:host.createNSString().get()];
    [nsCookieStorage() _setSubscribedDomainsForCookieChanges:m_subscribedDomainsForCookieChanges.get()];
    return true;
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
        ASSERT(observers.contains(observer));
        observers.remove(observer);
        if (observers.isEmptyIgnoringNullReferences()) {
            m_cookieChangeObservers.remove(it);
            ASSERT([m_subscribedDomainsForCookieChanges containsObject:host.createNSString().get()]);
            [m_subscribedDomainsForCookieChanges removeObject:host.createNSString().get()];
            subscribedURLsChanged = true;
        }
    }
    if (subscribedURLsChanged)
        [nsCookieStorage() _setSubscribedDomainsForCookieChanges:m_subscribedDomainsForCookieChanges.get()];
}

#endif // HAVE(COOKIE_CHANGE_LISTENER_API)

} // namespace WebCore

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
#import "NetworkStorageSession.h"

#import "CookieStorageObserver.h"
#import <WebCore/ClientOrigin.h>
#import <WebCore/Cookie.h>
#import <WebCore/CookieRequestHeaderFieldProxy.h>
#import <WebCore/CookieStoreGetOptions.h>
#import <WebCore/HTTPCookieAcceptPolicyCocoa.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SameSiteInfo.h>
#import <algorithm>
#import <optional>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

namespace WebKit {
using namespace WebCore;

NetworkStorageSession::~NetworkStorageSession()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    unregisterCookieChangeListenersIfNecessary();
#endif
    clearCookiesVersionChangeCallbacks();
}

void NetworkStorageSession::setCookie(const Cookie& cookie, const URL& url, const URL& mainDocumentURL)
{
    setCookies({ cookie }, url, mainDocumentURL);
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || isInMemoryCookieStore());

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto nsCookies = createNSArray(cookies, [] (auto& cookie) -> NSHTTPCookie * {
        return cookie.createNSHTTPCookie().autorelease();
    });

    [nsCookieStorage() setCookies:nsCookies.get() forURL:url.createNSURL().get() mainDocumentURL:mainDocumentURL.createNSURL().get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || isInMemoryCookieStore());

    auto work = [completionHandler = WTF::move(completionHandler), cookieStorage = RetainPtr { nsCookieStorage() }, cookie = cookie.createNSHTTPCookie()] () mutable {
        [cookieStorage deleteCookie:cookie.get()];
        ensureOnMainThread(WTF::move(completionHandler));
    };

    if (isInMemoryCookieStore())
        return work();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr(WTF::move(work)).get());
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

CookieStorageObserver& NetworkStorageSession::cookieStorageObserver() const
{
    if (!m_cookieStorageObserver)
        m_cookieStorageObserver = makeUnique<CookieStorageObserver>(nsCookieStorage().get());

    return *m_cookieStorageObserver;
}

String NetworkStorageSession::cookiePartitionIdentifierIfEnabled(const URL& firstParty) const
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    return isOptInCookiePartitioningEnabled() ? cookiePartitionIdentifier(firstParty) : String { };
#else
    UNUSED_PARAM(firstParty);
    return { };
#endif
}

RetainPtr<NSArray> NetworkStorageSession::cookiesForURL(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All)
        return nil;
    return CookieStorageSession::cookiesForURL(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, cookiePartitionIdentifierIfEnabled(firstParty));
}

std::pair<String, bool> NetworkStorageSession::cookiesForSession(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All)
        return { String(), false };
    return CookieStorageSession::cookiesForSession(firstParty, sameSiteInfo, url, cookiesFor, includeSecureCookies, thirdPartyCookieBlockingDecision, cookiePartitionIdentifierIfEnabled(firstParty));
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForSessionAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, CookiesFor cookiesFor, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker, CookieStoreGetOptions&& options) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All)
        return Vector<Cookie> { };
    return CookieStorageSession::cookiesForSessionAsVector(firstParty, sameSiteInfo, url, cookiesFor, includeSecureCookies, thirdPartyCookieBlockingDecision, cookiePartitionIdentifierIfEnabled(firstParty), options.name);
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::Dom, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker, CookieStoreGetOptions&& options) const
{
    return cookiesForSessionAsVector(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::Dom, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker, WTF::move(options));
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return cookiesForSession(firstParty, sameSiteInfo, url, frameID, pageID, CookiesFor::Http, includeSecureCookies, applyTrackingPrevention, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookiesForSession(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, CookiesFor::Http, headerFieldProxy.includeSecureCookies, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No);
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(thirdPartyCookieBlockingDecision))
        return;

    auto cookieCap = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);
    CookieStorageSession::setCookiesFromDOM(firstParty, sameSiteInfo, url, cookieString, thirdPartyCookieBlockingDecision, cookieCap, cookiePartitionIdentifierIfEnabled(firstParty));
}

bool NetworkStorageSession::setCookieFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const Cookie& cookie, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && shouldBlockCookies(thirdPartyCookieBlockingDecision))
        return false;

    auto cookieCap = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);
    return CookieStorageSession::setCookieFromDOM(firstParty, sameSiteInfo, url, cookie, thirdPartyCookieBlockingDecision, cookieCap, cookiePartitionIdentifierIfEnabled(firstParty));
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return toHTTPCookieAcceptPolicy([nsCookieStorage() cookieAcceptPolicy]);
    END_BLOCK_OBJC_EXCEPTIONS

    return HTTPCookieAcceptPolicy::Never;
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention applyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker::No);
    if (applyTrackingPrevention == ApplyTrackingPrevention::Yes && thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All) {
        rawCookies = { };
        return true;
    }
    return CookieStorageSession::getRawCookies(firstParty, sameSiteInfo, url, thirdPartyCookieBlockingDecision, cookiePartitionIdentifierIfEnabled(firstParty), rawCookies);
}

void NetworkStorageSession::deleteCookie(const URL& firstParty, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler) const
{
    CookieStorageSession::deleteCookie(firstParty, url, cookieName, cookiePartitionIdentifierIfEnabled(firstParty), WTF::move(completionHandler));
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    for (NSHTTPCookie *cookie in [nsCookieStorage() cookies]) {
        RetainPtr<NSString> domain = [cookie domain];
        if (!domain) {
            ASSERT_NOT_REACHED();
            continue;
        }
        hostnames.add(domain.get());
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void NetworkStorageSession::deleteCookiesMatching(NOESCAPE const Function<bool(NSHTTPCookie *)>& matches, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies) || isInMemoryCookieStore());

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();
    RetainPtr nsCookieStorage = this->nsCookieStorage();
    auto aggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler), nsCookieStorage] () mutable {
        [nsCookieStorage _saveCookies:makeBlockPtr([completionHandler = WTF::move(completionHandler)] () mutable {
            ensureOnMainThread(WTF::move(completionHandler));
        }).get()];
    });

    for (NSHTTPCookie *cookie in [nsCookieStorage cookies]) {
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

void NetworkStorageSession::deleteCookiesForHostnames(std::span<const String> hostnames, IncludeHttpOnlyCookies includeHttpOnlyCookies, ScriptWrittenCookiesOnly scriptWrittenCookiesOnly, CompletionHandler<void()>&& completionHandler)
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

    if (isInMemoryCookieStore())
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

    return nsCookiesToCookieVector(nsCookies.get(), [](NSHTTPCookie *cookie) {
        return !cookie.HTTPOnly;
    });
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
        auto cookies = nsCookiesToCookieVector(addedCookies, [](NSHTTPCookie *cookie) {
            return !cookie.HTTPOnly;
        });
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

        auto cookies = nsCookiesToCookieVector(removedCookies, [](NSHTTPCookie *cookie) {
            return !cookie.HTTPOnly;
        });
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

} // namespace WebKit

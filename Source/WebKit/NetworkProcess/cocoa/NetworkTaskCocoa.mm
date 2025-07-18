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

#import "config.h"
#import "NetworkTaskCocoa.h"

#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkSession.h"
#import "WebPrivacyHelpers.h"
#import <WebCore/DNS.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/Quirks.h>
#import <WebCore/RegistrableDomain.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

static inline bool computeIsAlwaysOnLoggingAllowed(NetworkSession& session)
{
    if (session.networkProcess().sessionIsControlledByAutomation(session.sessionID()))
        return true;

    return session.sessionID().isAlwaysOnLoggingAllowed();
}

NetworkTaskCocoa::NetworkTaskCocoa(NetworkSession& session)
    : m_networkSession(session)
    , m_isAlwaysOnLoggingAllowed(computeIsAlwaysOnLoggingAllowed(session))
{
}

RetainPtr<NSURLSessionTask> NetworkTaskCocoa::protectedTask() const
{
    return task();
}

CheckedPtr<NetworkSession> NetworkTaskCocoa::checkedNetworkSession() const
{
    ASSERT(m_networkSession);
    return m_networkSession.get();
}

static bool shouldCapCookieExpiryForThirdPartyIPAddress(const WebCore::IPAddress& remote, const WebCore::IPAddress& firstParty)
{
    auto matchingLength = remote.matchingNetMaskLength(firstParty);
    if (remote.isIPv4())
        return matchingLength < 4 * sizeof(struct in_addr);
    return matchingLength < 4 * sizeof(struct in6_addr);
}

bool NetworkTaskCocoa::shouldApplyCookiePolicyForThirdPartyCloaking() const
{
    CheckedPtr networkStorageSession = checkedNetworkSession()->networkStorageSession();
    return networkStorageSession && networkStorageSession->trackingPreventionEnabled();
}

NSHTTPCookieStorage *NetworkTaskCocoa::statelessCookieStorage()
{
    static NeverDestroyed<RetainPtr<NSHTTPCookieStorage>> statelessCookieStorage;
    if (!statelessCookieStorage.get()) {
        statelessCookieStorage.get() = adoptNS([[NSHTTPCookieStorage alloc] _initWithIdentifier:nil private:YES]);
        statelessCookieStorage.get().get().cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    }
    ASSERT(!statelessCookieStorage.get().get().cookies.count);
    return statelessCookieStorage.get().get();
}

NSString *NetworkTaskCocoa::lastRemoteIPAddress(NSURLSessionTask *task)
{
    // FIXME (246428): In a future patch, this should adopt CFNetwork API that retrieves the original
    // IP address of the proxied response, rather than the proxy itself.
    return task._incompleteTaskMetrics.transactionMetrics.lastObject.remoteAddress;
}

WebCore::RegistrableDomain NetworkTaskCocoa::lastCNAMEDomain(String cname)
{
    if (cname.endsWith('.'))
        cname = cname.left(cname.length() - 1);
    return WebCore::RegistrableDomain::uncheckedCreateFromHost(cname);
}

static RetainPtr<NSArray<NSHTTPCookie *>> cookiesByCappingExpiry(NSArray<NSHTTPCookie *> *cookies, Seconds ageCap)
{
    RetainPtr cappedCookies = [NSMutableArray arrayWithCapacity:cookies.count];
    for (NSHTTPCookie *cookie in cookies)
        [cappedCookies addObject:WebCore::NetworkStorageSession::capExpiryOfPersistentCookie(cookie, ageCap).get()];
    return cappedCookies;
}

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
static NSArray<NSHTTPCookie *> *cookiesBySettingPartition(NSArray<NSHTTPCookie *> *cookies, NSString* partition)
{
    RetainPtr<NSMutableArray> partitionedCookies = [NSMutableArray arrayWithCapacity:cookies.count];
    for (NSHTTPCookie *cookie in cookies) {
        RetainPtr partitionedCookie = WebCore::NetworkStorageSession::setCookiePartition(cookie, partition);
        if (partitionedCookie)
            [partitionedCookies addObject:partitionedCookie.get()];
    }
    return partitionedCookies.get();
}
#endif

// FIXME: Temporary fix for <rdar://60089022> and <rdar://100500464> until content can be updated.
bool NetworkTaskCocoa::needsFirstPartyCookieBlockingLatchModeQuirk(const URL& firstPartyURL, const URL& requestURL, const URL& redirectingURL) const
{
    using RegistrableDomain = WebCore::RegistrableDomain;
    static NeverDestroyed<HashMap<RegistrableDomain, RegistrableDomain>> quirkPairs = [] {
        HashMap<RegistrableDomain, RegistrableDomain> map;
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("ymail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("yahoo.com"_s));
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aolmail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aol.com"_s));
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("googleusercontent.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("google.com"_s));
        return map;
    }();

    RegistrableDomain firstPartyDomain { firstPartyURL };
    RegistrableDomain requestDomain { requestURL };
    if (firstPartyDomain != requestDomain)
        return false;

    RegistrableDomain redirectingDomain { redirectingURL };
    auto quirk = quirkPairs.get().find(redirectingDomain);
    if (quirk == quirkPairs.get().end())
        return false;

    return quirk->value == requestDomain;
}

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
void NetworkTaskCocoa::setCookieTransformForThirdPartyRequest(const WebCore::ResourceRequest& request, IsRedirect isRedirect)
{
    ASSERT(request.isThirdParty());
    if (!request.isThirdParty())
        return;

    ASSERT_UNUSED(isRedirect, !task()._cookieTransformCallback || isRedirect == IsRedirect::Yes);
    task()._cookieTransformCallback = nil;

    if (!Quirks::needsPartitionedCookies(request))
        return;

    CheckedPtr networkStorageSession = m_networkSession->networkStorageSession();
    if (!networkStorageSession)
        return;

    if (!networkStorageSession->isOptInCookiePartitioningEnabled())
        return;

    String cookiePartition = networkStorageSession->cookiePartitionIdentifier(request);

    task()._cookieTransformCallback = makeBlockPtr([
        requestURL = crossThreadCopy(request.url())
        , weakTask = WeakObjCPtr<NSURLSessionTask>(task())
        , cookiePartition = crossThreadCopy(cookiePartition)]
        (NSArray<NSHTTPCookie*> *cookiesSetInResponse) -> NSArray<NSHTTPCookie*> * {
        auto task = weakTask.get();
        if (!task || ![cookiesSetInResponse count])
            return cookiesSetInResponse;

        // FIXME: Consider making these session cookies, as well.
        if (!cookiePartition.isEmpty())
            cookiesSetInResponse = cookiesBySettingPartition(cookiesSetInResponse, cookiePartition.createNSString().get());

        return cookiesSetInResponse;
    }).get();
}
#endif

void NetworkTaskCocoa::setCookieTransformForFirstPartyRequest(const WebCore::ResourceRequest& request)
{
    if (!shouldApplyCookiePolicyForThirdPartyCloaking())
        return;

    ASSERT(!request.isThirdParty());
    if (request.isThirdParty()) {
        protectedTask().get()._cookieTransformCallback = nil;
        return;
    }

    // Cap expiry of incoming cookies in response if it is a same-site subresource but
    // it resolves to a different CNAME or IP address range than the top site request,
    // i.e. third-party CNAME or IP address cloaking.
    auto firstPartyURL = request.firstPartyForCookies();
    auto firstPartyHostName = firstPartyURL.host().toString();
    CheckedPtr networkSession = m_networkSession.get();

    protectedTask().get()._cookieTransformCallback = makeBlockPtr([
        requestURL = crossThreadCopy(request.url())
        , firstPartyURL = crossThreadCopy(firstPartyURL)
        , firstPartyHostCNAME = crossThreadCopy(networkSession->firstPartyHostCNAMEDomain(firstPartyHostName))
        , firstPartyAddress = crossThreadCopy(networkSession->firstPartyHostIPAddress(firstPartyHostName))
        , thirdPartyCNAMEDomainForTesting = crossThreadCopy(m_networkSession->thirdPartyCNAMEDomainForTesting())
        , ageCapForCNAMECloakedCookies = crossThreadCopy(m_ageCapForCNAMECloakedCookies)
        , weakTask = WeakObjCPtr<NSURLSessionTask>(task())
        , firstPartyRegistrableDomainName = crossThreadCopy(RegistrableDomain { firstPartyURL }.string())
        , debugLoggingEnabled = networkSession->networkStorageSession()->trackingPreventionDebugLoggingEnabled()]
        (NSArray<NSHTTPCookie*> *cookiesSetInResponse) -> NSArray<NSHTTPCookie*> * {
        auto task = weakTask.get();
        if (!task || ![cookiesSetInResponse count])
            return cookiesSetInResponse;

        auto cnameDomain = [&task]() {
            if (RetainPtr lastResolvedCNAMEInChain = [[task _resolvedCNAMEChain] lastObject])
                return lastCNAMEDomain(lastResolvedCNAMEInChain.get());
            return RegistrableDomain { };
        }();
        if (cnameDomain.isEmpty() && thirdPartyCNAMEDomainForTesting)
            cnameDomain = *thirdPartyCNAMEDomainForTesting;

        if (cnameDomain.isEmpty()) {
            if (!firstPartyAddress)
                return cookiesSetInResponse;

            auto remoteAddress = WebCore::IPAddress::fromString(lastRemoteIPAddress(task.get()));
            if (!remoteAddress)
                return cookiesSetInResponse;

            auto needsThirdPartyIPAddressQuirk = [] (const URL& requestURL, const String& firstPartyRegistrableDomainName) {
                // We only apply this quirk if we're already on Google or youtube.com;
                // otherwise, we would've already bailed at the top of this method, due to
                // the request being third party.
                auto hostName = requestURL.host();
                if (hostName == "accounts.google.com"_s)
                    return true;

                return (firstPartyRegistrableDomainName.startsWith("google."_s) || firstPartyRegistrableDomainName == "youtube.com"_s)
                    && hostName == makeString("consent."_s, firstPartyRegistrableDomainName);
            };

            if (shouldCapCookieExpiryForThirdPartyIPAddress(*remoteAddress, *firstPartyAddress) && !needsThirdPartyIPAddressQuirk(requestURL, firstPartyRegistrableDomainName)) {
                RetainPtr cappedCookies = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies);
                if (debugLoggingEnabled) {
                    for (NSHTTPCookie *cookie in cappedCookies.get())
                        RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party IP address cookie named %{public}@.", cookie.name);
                }
                return cappedCookies.autorelease();
            }

            return cookiesSetInResponse;
        }

        // CNAME cloaking is a first-party sub resource that resolves
        // through a CNAME that differs from the first-party domain and
        // also differs from the top frame host's CNAME, if one exists.
        if (!cnameDomain.matches(firstPartyURL) && (!firstPartyHostCNAME || cnameDomain != *firstPartyHostCNAME)) {
            // Don't use RetainPtr here. This array has to be retained and
            // auto released to not be released before returned to the code
            // executing the block.
            RetainPtr cappedCookies = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies).autorelease();
            if (debugLoggingEnabled) {
                for (NSHTTPCookie *cookie in cappedCookies.get())
                    RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party CNAME cloaked cookie named %{public}@.", cookie.name);
            }
            return cappedCookies.autorelease();
        }

        return cookiesSetInResponse;
    }).get();
}

void NetworkTaskCocoa::setCookieTransform(const WebCore::ResourceRequest& request, IsRedirect isRedirect)
{
    if (request.isThirdParty()) {
#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
        setCookieTransformForThirdPartyRequest(request, isRedirect);
#endif
        return;
    }
    setCookieTransformForFirstPartyRequest(request);
}

void NetworkTaskCocoa::blockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (m_hasBeenSetToUseStatelessCookieStorage)
        return;

    [protectedTask() _setExplicitCookieStorage:RetainPtr { statelessCookieStorage() }.get()._cookieStorage];
    m_hasBeenSetToUseStatelessCookieStorage = true;
}

void NetworkTaskCocoa::unblockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!m_hasBeenSetToUseStatelessCookieStorage)
        return;

    if (CheckedPtr storageSession = checkedNetworkSession()->networkStorageSession()) {
        [protectedTask() _setExplicitCookieStorage:[storageSession->nsCookieStorage() _cookieStorage]];
        m_hasBeenSetToUseStatelessCookieStorage = false;
    }
}

bool NetworkTaskCocoa::shouldBlockCookies(WebCore::ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision)
{
    return thirdPartyCookieBlockingDecision == WebCore::ThirdPartyCookieBlockingDecision::All;
}

WebCore::ThirdPartyCookieBlockingDecision NetworkTaskCocoa::requestThirdPartyCookieBlockingDecision(const WebCore::ResourceRequest& request) const
{
    auto thirdPartyCookieBlockingDecision = storedCredentialsPolicy() == WebCore::StoredCredentialsPolicy::EphemeralStateless ? WebCore::ThirdPartyCookieBlockingDecision::All : WebCore::ThirdPartyCookieBlockingDecision::None;
    if (CheckedPtr networkStorageSession = checkedNetworkSession()->networkStorageSession()) {
        if (!shouldBlockCookies(thirdPartyCookieBlockingDecision))
            thirdPartyCookieBlockingDecision = networkStorageSession->thirdPartyCookieBlockingDecisionForRequest(request, frameID(), pageID(), shouldRelaxThirdPartyCookieBlocking());
    }

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS) && HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
    if (thirdPartyCookieBlockingDecision == WebCore::ThirdPartyCookieBlockingDecision::AllExceptPartitioned && request.isThirdParty() && isKnownTrackerAddressOrDomain(request.url().host()))
        return WebCore::ThirdPartyCookieBlockingDecision::All;
#endif
    return thirdPartyCookieBlockingDecision;
}

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
bool NetworkTaskCocoa::isOptInCookiePartitioningEnabled() const
{
    bool isOptInCookiePartitioningEnabled { false };
    if (CheckedPtr networkStorageSession = m_networkSession->networkStorageSession())
        isOptInCookiePartitioningEnabled = networkStorageSession->isOptInCookiePartitioningEnabled();
    return isOptInCookiePartitioningEnabled;
}
#endif

void NetworkTaskCocoa::updateTaskWithFirstPartyForSameSiteCookies(NSURLSessionTask* task, const WebCore::ResourceRequest& request)
{
    if (request.isSameSiteUnspecified())
        return;
#if HAVE(FOUNDATION_WITH_SAME_SITE_COOKIE_SUPPORT)
    task._siteForCookies = RetainPtr { request.isSameSite() ? task.currentRequest.URL : URL::emptyNSURL() }.get();
    task._isTopLevelNavigation = request.isTopSite();
#else
    UNUSED_PARAM(task);
#endif
}

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
void NetworkTaskCocoa::updateTaskWithStoragePartitionIdentifier(const WebCore::ResourceRequest& request)
{
    if (!isOptInCookiePartitioningEnabled())
        return;

    CheckedPtr networkStorageSession = m_networkSession->networkStorageSession();
    if (!networkStorageSession)
        return;

    // FIXME: Remove respondsToSelector when available with NWLoader. rdar://134913391
    if ([task() respondsToSelector:@selector(set_storagePartitionIdentifier:)])
        task()._storagePartitionIdentifier = networkStorageSession->cookiePartitionIdentifier(request).createNSString().get();
}
#endif

void NetworkTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
#if ENABLE(APP_PRIVACY_REPORT)
    request.setIsAppInitiated(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).attribution == NSURLRequestAttributionDeveloper);
#endif

    setCookieTransform(request, IsRedirect::Yes);
    if (!m_hasBeenSetToUseStatelessCookieStorage) {
        auto thirdPartyCookieBlockingDecision = requestThirdPartyCookieBlockingDecision(request);
        if (shouldBlockCookies(thirdPartyCookieBlockingDecision))
            blockCookies();
#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
        else {
            RetainPtr<NSMutableURLRequest> mutableRequest = adoptNS([request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody) mutableCopy]);
            if (isOptInCookiePartitioningEnabled() && [mutableRequest respondsToSelector:@selector(_setAllowOnlyPartitionedCookies:)]) {
                auto shouldAllowOnlyPartitioned = thirdPartyCookieBlockingDecision == WebCore::ThirdPartyCookieBlockingDecision::AllExceptPartitioned ? YES : NO;
                [mutableRequest _setAllowOnlyPartitionedCookies:shouldAllowOnlyPartitioned];
                request = mutableRequest.get();
            }
        }
#endif
    } else if (storedCredentialsPolicy() != WebCore::StoredCredentialsPolicy::EphemeralStateless && needsFirstPartyCookieBlockingLatchModeQuirk(request.firstPartyForCookies(), request.url(), redirectResponse.url()))
        unblockCookies();
#if !RELEASE_LOG_DISABLED
    if (checkedNetworkSession()->shouldLogCookieInformation())
        RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%lu: %s cookies for redirect URL %s", this, pageID() ? pageID()->toUInt64() : 0, frameID() ? frameID()->toUInt64() : 0, (unsigned long)[task() taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#else
    LOG(NetworkSession, "%lu %s cookies for redirect URL %s", (unsigned long)[task() taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#endif

    updateTaskWithFirstPartyForSameSiteCookies(protectedTask().get(), request);
#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
    updateTaskWithStoragePartitionIdentifier(request);
#endif
    completionHandler(WTFMove(request));
}

ShouldRelaxThirdPartyCookieBlocking NetworkTaskCocoa::shouldRelaxThirdPartyCookieBlocking() const
{
    return checkedNetworkSession()->networkProcess().shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID());
}

} // namespace WebKit

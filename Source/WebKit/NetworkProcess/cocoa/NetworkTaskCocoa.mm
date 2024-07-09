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
#import <WebCore/DNS.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
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

NetworkTaskCocoa::NetworkTaskCocoa(NetworkSession& session, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
    : m_networkSession(session)
    , m_isAlwaysOnLoggingAllowed(computeIsAlwaysOnLoggingAllowed(session))
    , m_shouldRelaxThirdPartyCookieBlocking(shouldRelaxThirdPartyCookieBlocking)
{
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
    return m_networkSession->networkStorageSession() && m_networkSession->networkStorageSession()->trackingPreventionEnabled();
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

static NSArray<NSHTTPCookie *> *cookiesByCappingExpiry(NSArray<NSHTTPCookie *> *cookies, Seconds ageCap)
{
    auto *cappedCookies = [NSMutableArray arrayWithCapacity:cookies.count];
    for (NSHTTPCookie *cookie in cookies)
        [cappedCookies addObject:WebCore::NetworkStorageSession::capExpiryOfPersistentCookie(cookie, ageCap)];
    return cappedCookies;
}

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

void NetworkTaskCocoa::applyCookiePolicyForThirdPartyCloaking(const WebCore::ResourceRequest& request)
{
    if (!shouldApplyCookiePolicyForThirdPartyCloaking())
        return;

    if (request.isThirdParty()) {
        task()._cookieTransformCallback = nil;
        return;
    }

    // Cap expiry of incoming cookies in response if it is a same-site subresource but
    // it resolves to a different CNAME or IP address range than the top site request,
    // i.e. third-party CNAME or IP address cloaking.
    auto firstPartyURL = request.firstPartyForCookies();
    auto firstPartyHostName = firstPartyURL.host().toString();

    task()._cookieTransformCallback = makeBlockPtr([
        requestURL = crossThreadCopy(request.url())
        , firstPartyURL = crossThreadCopy(firstPartyURL)
        , firstPartyHostCNAME = crossThreadCopy(m_networkSession->firstPartyHostCNAMEDomain(firstPartyHostName))
        , firstPartyAddress = crossThreadCopy(m_networkSession->firstPartyHostIPAddress(firstPartyHostName))
        , thirdPartyCNAMEDomainForTesting = crossThreadCopy(m_networkSession->thirdPartyCNAMEDomainForTesting())
        , ageCapForCNAMECloakedCookies = crossThreadCopy(m_ageCapForCNAMECloakedCookies)
        , weakTask = WeakObjCPtr<NSURLSessionTask>(task())
        , firstPartyRegistrableDomainName = crossThreadCopy(RegistrableDomain { firstPartyURL }.string())
        , debugLoggingEnabled = m_networkSession->networkStorageSession()->trackingPreventionDebugLoggingEnabled()]
        (NSArray<NSHTTPCookie*> *cookiesSetInResponse) -> NSArray<NSHTTPCookie*> * {
        auto task = weakTask.get();
        if (!task || ![cookiesSetInResponse count])
            return cookiesSetInResponse;

        auto cnameDomain = [&task]() {
            if (auto* lastResolvedCNAMEInChain = [[task _resolvedCNAMEChain] lastObject])
                return lastCNAMEDomain(lastResolvedCNAMEInChain);
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
                cookiesSetInResponse = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies);
                if (debugLoggingEnabled) {
                    for (NSHTTPCookie *cookie in cookiesSetInResponse)
                        RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party IP address cookie named %{public}@.", cookie.name);
                }
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
            cookiesSetInResponse = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies);
            if (debugLoggingEnabled) {
                for (NSHTTPCookie *cookie in cookiesSetInResponse)
                    RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party CNAME cloaked cookie named %{public}@.", cookie.name);
            }
        }

        return cookiesSetInResponse;
    }).get();
}

void NetworkTaskCocoa::blockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (m_hasBeenSetToUseStatelessCookieStorage)
        return;

    [task() _setExplicitCookieStorage:statelessCookieStorage()._cookieStorage];
    m_hasBeenSetToUseStatelessCookieStorage = true;
}

void NetworkTaskCocoa::unblockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!m_hasBeenSetToUseStatelessCookieStorage)
        return;

    if (auto* storageSession = m_networkSession->networkStorageSession()) {
        [task() _setExplicitCookieStorage:[storageSession->nsCookieStorage() _cookieStorage]];
        m_hasBeenSetToUseStatelessCookieStorage = false;
    }
}

void NetworkTaskCocoa::updateTaskWithFirstPartyForSameSiteCookies(NSURLSessionTask* task, const WebCore::ResourceRequest& request)
{
    if (request.isSameSiteUnspecified())
        return;
#if HAVE(FOUNDATION_WITH_SAME_SITE_COOKIE_SUPPORT)
    static NSURL *emptyURL = [[NSURL alloc] initWithString:@""];
    task._siteForCookies = request.isSameSite() ? task.currentRequest.URL : emptyURL;
    task._isTopLevelNavigation = request.isTopSite();
#else
    UNUSED_PARAM(task);
#endif
}

void NetworkTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
#if ENABLE(APP_PRIVACY_REPORT)
    request.setIsAppInitiated(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).attribution == NSURLRequestAttributionDeveloper);
#endif

    applyCookiePolicyForThirdPartyCloaking(request);
    if (!m_hasBeenSetToUseStatelessCookieStorage) {
        if (storedCredentialsPolicy() == WebCore::StoredCredentialsPolicy::EphemeralStateless
            || (m_networkSession->networkStorageSession() && m_networkSession->networkStorageSession()->shouldBlockCookies(request, frameID(), pageID(), m_shouldRelaxThirdPartyCookieBlocking)))
            blockCookies();
    } else if (storedCredentialsPolicy() != WebCore::StoredCredentialsPolicy::EphemeralStateless && needsFirstPartyCookieBlockingLatchModeQuirk(request.firstPartyForCookies(), request.url(), redirectResponse.url()))
        unblockCookies();
#if !RELEASE_LOG_DISABLED
    if (m_networkSession->shouldLogCookieInformation())
        RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%lu: %s cookies for redirect URL %s", this, pageID()->toUInt64(), frameID()->object().toUInt64(), (unsigned long)[task() taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#else
    LOG(NetworkSession, "%lu %s cookies for redirect URL %s", (unsigned long)[task() taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#endif

    updateTaskWithFirstPartyForSameSiteCookies(task(), request);
    completionHandler(WTFMove(request));
}

} // namespace WebKit

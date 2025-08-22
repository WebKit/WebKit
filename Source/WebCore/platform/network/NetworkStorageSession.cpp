/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkStorageSession.h"

#include "ClientOrigin.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "HTTPCookieAcceptPolicy.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PublicSuffixStore.h"
#include "ResourceRequest.h"
#include "ShouldPartitionCookie.h"
#include "Site.h"
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkStorageSession);

static HashSet<OrganizationStorageAccessPromptQuirk>& updatableStorageAccessPromptQuirks()
{
    ASSERT(RunLoop::isMain());
    // FIXME: Move this isn't an instance of a class, probably as a member of NetworkStorageSession.
    static MainThreadNeverDestroyed<HashSet<OrganizationStorageAccessPromptQuirk>> set;
    return set.get();
}

bool NetworkStorageSession::m_processMayUseCookieAPI = false;

bool NetworkStorageSession::processMayUseCookieAPI()
{
    return m_processMayUseCookieAPI;
}

void NetworkStorageSession::permitProcessToUseCookieAPI(bool value)
{
    m_processMayUseCookieAPI = value;
    if (m_processMayUseCookieAPI)
        addProcessPrivilege(ProcessPrivilege::CanAccessRawCookies);
    else
        removeProcessPrivilege(ProcessPrivilege::CanAccessRawCookies);
}

#if !PLATFORM(COCOA) && !USE(SOUP)
Vector<Cookie> NetworkStorageSession::domCookiesForHost(const URL&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}
#endif // !PLATFORM(COCOA) && !USE(SOUP)

#if !USE(SOUP)
void NetworkStorageSession::setTrackingPreventionEnabled(bool enabled)
{
    m_isTrackingPreventionEnabled = enabled;
}

bool NetworkStorageSession::trackingPreventionEnabled() const
{
    return m_isTrackingPreventionEnabled;
}
#endif

void NetworkStorageSession::setTrackingPreventionDebugLoggingEnabled(bool enabled)
{
    m_isTrackingPreventionDebugLoggingEnabled = enabled;
}

bool NetworkStorageSession::shouldBlockThirdPartyCookies(const RegistrableDomain& registrableDomain) const
{
    if (!m_isTrackingPreventionEnabled || registrableDomain.isEmpty())
        return false;

    ASSERT(!(m_registrableDomainsToBlockAndDeleteCookiesFor.contains(registrableDomain) && m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain)));

    return m_registrableDomainsToBlockAndDeleteCookiesFor.contains(registrableDomain)
        || m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain);
}

bool NetworkStorageSession::shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(const RegistrableDomain& registrableDomain) const
{
    if (!m_isTrackingPreventionEnabled || registrableDomain.isEmpty())
        return false;

    ASSERT(!(m_registrableDomainsToBlockAndDeleteCookiesFor.contains(registrableDomain) && m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain)));

    return m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain);
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
void NetworkStorageSession::setCookie(const URL& firstParty, const Cookie& cookie, ShouldPartitionCookie shouldPartitionCookie)
{
    if (!isOptInCookiePartitioningEnabled() || shouldPartitionCookie != ShouldPartitionCookie::Yes || !cookie.partitionKey.isEmpty()) {
        setCookie(cookie);
        return;
    }
    auto partitionedCookie = cookie;
    partitionedCookie.partitionKey = cookiePartitionIdentifier(firstParty);
    setCookie(partitionedCookie);
}
#endif

#if !PLATFORM(COCOA)
void NetworkStorageSession::setAllCookiesToSameSiteStrict(const RegistrableDomain&, CompletionHandler<void()>&& completionHandler)
{
    // Not implemented.
    completionHandler();
}
#endif

bool NetworkStorageSession::hasHadUserInteractionAsFirstParty(const RegistrableDomain& registrableDomain) const
{
    if (registrableDomain.isEmpty())
        return false;

    return m_registrableDomainsWithUserInteractionAsFirstParty.contains(registrableDomain);
}

ThirdPartyCookieBlockingDecision NetworkStorageSession::thirdPartyCookieBlockingDecisionForRequest(const ResourceRequest& request, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return thirdPartyCookieBlockingDecisionForRequest(request.firstPartyForCookies(), request.url(), frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}
    
ThirdPartyCookieBlockingDecision NetworkStorageSession::thirdPartyCookieBlockingDecisionForRequest(const URL& firstPartyForCookies, const URL& resource, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    if (shouldRelaxThirdPartyCookieBlocking == ShouldRelaxThirdPartyCookieBlocking::Yes)
        return ThirdPartyCookieBlockingDecision::None;

    if (!m_isTrackingPreventionEnabled)
        return ThirdPartyCookieBlockingDecision::None;

    if (!firstPartyForCookies.isValid())
        return ThirdPartyCookieBlockingDecision::All;

    RegistrableDomain firstPartyDomain { firstPartyForCookies };
    if (firstPartyDomain.isEmpty())
        return ThirdPartyCookieBlockingDecision::None;

    if (!resource.isValid())
        return ThirdPartyCookieBlockingDecision::All;

    RegistrableDomain resourceDomain { resource };
    if (resourceDomain.isEmpty())
        return ThirdPartyCookieBlockingDecision::None;

    if (firstPartyDomain == resourceDomain)
        return ThirdPartyCookieBlockingDecision::None;

    if (hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID))
        return ThirdPartyCookieBlockingDecision::None;

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    const auto decideThirdPartyCookieBlocking = [isOptInCookiePartitioningEnabled = isOptInCookiePartitioningEnabled()] (bool shouldAllowUnpartitionedCookies) {
        if (shouldAllowUnpartitionedCookies)
            return ThirdPartyCookieBlockingDecision::None;
        return isOptInCookiePartitioningEnabled ? ThirdPartyCookieBlockingDecision::AllExceptPartitioned : ThirdPartyCookieBlockingDecision::All;
    };
#else
    const auto decideThirdPartyCookieBlocking = [] (bool shouldAllowUnpartitionedCookies) {
        return shouldAllowUnpartitionedCookies ? ThirdPartyCookieBlockingDecision::None : ThirdPartyCookieBlockingDecision::All;
    };
    UNUSED_PARAM(isKnownCrossSiteTracker);
#endif

    switch (m_thirdPartyCookieBlockingMode) {
    case ThirdPartyCookieBlockingMode::All:
        return ThirdPartyCookieBlockingDecision::All;
    case ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains:
        return decideThirdPartyCookieBlocking(shouldExemptDomainPairFromThirdPartyCookieBlocking(firstPartyDomain, resourceDomain));
    case ThirdPartyCookieBlockingMode::AllExceptManagedDomains:
        return m_managedDomains.contains(firstPartyDomain) ? ThirdPartyCookieBlockingDecision::None : ThirdPartyCookieBlockingDecision::All;
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    case ThirdPartyCookieBlockingMode::AllExceptPartitioned:
        return (isOptInCookiePartitioningEnabled() && isKnownCrossSiteTracker == IsKnownCrossSiteTracker::No) ? ThirdPartyCookieBlockingDecision::AllExceptPartitioned : ThirdPartyCookieBlockingDecision::All;
#endif
    case ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction:
        if (!hasHadUserInteractionAsFirstParty(firstPartyDomain))
            return decideThirdPartyCookieBlocking(false);
        [[fallthrough]];
    case ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy:
        return decideThirdPartyCookieBlocking(!shouldBlockThirdPartyCookies(resourceDomain));
    }

    ASSERT_NOT_REACHED();
    return ThirdPartyCookieBlockingDecision::None;
}

bool NetworkStorageSession::shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return shouldBlockCookies(thirdPartyCookieBlockingDecisionForRequest(firstPartyForCookies, resource, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker));
}

bool NetworkStorageSession::shouldBlockCookies(const ResourceRequest& request, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return shouldBlockCookies(request.firstPartyForCookies(), request.url(), frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker);
}

bool NetworkStorageSession::shouldBlockCookies(ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision)
{
    return thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All;
}

bool NetworkStorageSession::shouldExemptDomainPairFromThirdPartyCookieBlocking(const RegistrableDomain& topFrameDomain, const RegistrableDomain& resourceDomain) const
{
    ASSERT(topFrameDomain != resourceDomain);
    if (topFrameDomain.isEmpty() || resourceDomain.isEmpty())
        return false;

    return topFrameDomain == resourceDomain || (m_appBoundDomains.contains(topFrameDomain) && m_appBoundDomains.contains(resourceDomain));
}

String NetworkStorageSession::cookiePartitionIdentifier(const URL& firstPartyForCookies)
{
    return Site { firstPartyForCookies }.toString();
}

String NetworkStorageSession::cookiePartitionIdentifier(const ResourceRequest& request)
{
    return cookiePartitionIdentifier(request.firstPartyForCookies());
}

std::optional<Seconds> NetworkStorageSession::maxAgeCacheCap(const ResourceRequest& request, IsKnownCrossSiteTracker isKnownCrossSiteTracker)
{
    auto thirdPartyCookieBlockingDecision = thirdPartyCookieBlockingDecisionForRequest(request, std::nullopt, std::nullopt, ShouldRelaxThirdPartyCookieBlocking::No, isKnownCrossSiteTracker);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    bool shouldEnforceMaxAgeCacheCap = thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All || thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::AllExceptPartitioned;
#else
    bool shouldEnforceMaxAgeCacheCap = thirdPartyCookieBlockingDecision == ThirdPartyCookieBlockingDecision::All;
#endif
    if (m_cacheMaxAgeCapForPrevalentResources && shouldEnforceMaxAgeCacheCap)
        return m_cacheMaxAgeCapForPrevalentResources;
    return std::nullopt;
}

void NetworkStorageSession::setAgeCapForClientSideCookies(std::optional<Seconds> seconds)
{
    m_ageCapForClientSideCookies = seconds;
    m_ageCapForClientSideCookiesShort = seconds ? Seconds { seconds->seconds() / 7. } : seconds;
    m_ageCapForClientSideCookiesForScriptTrackingPrivacy = seconds;
#if ENABLE(JS_COOKIE_CHECKING)
    m_ageCapForClientSideCookiesForLinkDecorationTargetPage = seconds;
#endif
}

void NetworkStorageSession::setPrevalentDomainsToBlockAndDeleteCookiesFor(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsToBlockAndDeleteCookiesFor.clear();
    m_registrableDomainsToBlockAndDeleteCookiesFor.addAll(domains);
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy)
        cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::setPrevalentDomainsToBlockButKeepCookiesFor(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsToBlockButKeepCookiesFor.clear();
    m_registrableDomainsToBlockButKeepCookiesFor.addAll(domains);
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy)
        cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::setDomainsWithUserInteractionAsFirstParty(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsWithUserInteractionAsFirstParty.clear();
    m_registrableDomainsWithUserInteractionAsFirstParty.addAll(domains);
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction)
        cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::setDomainsWithCrossPageStorageAccess(const HashMap<TopFrameDomain, Vector<SubResourceDomain>>& domains)
{
    m_pairsGrantedCrossPageStorageAccess.clear();
    for (auto& [topDomain, subResourceDomains] : domains) {
        for (auto&& subResourceDomain : subResourceDomains)
            grantCrossPageStorageAccess(topDomain, subResourceDomain);
    }
}

void NetworkStorageSession::grantCrossPageStorageAccess(const TopFrameDomain& topFrameDomain, const SubResourceDomain& resourceDomain)
{
    m_pairsGrantedCrossPageStorageAccess.ensure(topFrameDomain, [] { return HashSet<RegistrableDomain> { };
        }).iterator->value.add(resourceDomain);

    // Some sites have quirks where multiple login domains require storage access.
    if (auto additionalLoginDomain = findAdditionalLoginDomain(topFrameDomain, resourceDomain)) {
        m_pairsGrantedCrossPageStorageAccess.ensure(topFrameDomain, [] { return HashSet<RegistrableDomain> { };
            }).iterator->value.add(*additionalLoginDomain);
    }
}

bool NetworkStorageSession::hasStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID) const
{
    if (!pageID)
        return false;

    if (frameID) {
        auto framesGrantedIterator = m_framesGrantedStorageAccess.find(*pageID);
        if (framesGrantedIterator != m_framesGrantedStorageAccess.end()) {
            auto it = framesGrantedIterator->value.find(frameID.value());
            if (it != framesGrantedIterator->value.end() && it->value == resourceDomain)
                return true;
        }
    }

    if (!firstPartyDomain.isEmpty()) {
        auto pagesGrantedIterator = m_pagesGrantedStorageAccess.find(*pageID);
        if (pagesGrantedIterator != m_pagesGrantedStorageAccess.end()) {
            auto it = pagesGrantedIterator->value.find(firstPartyDomain);
            if (it != pagesGrantedIterator->value.end() && it->value == resourceDomain)
                return true;
        }

        auto it = m_pairsGrantedCrossPageStorageAccess.find(firstPartyDomain);
        if (it != m_pairsGrantedCrossPageStorageAccess.end() && it->value.contains(resourceDomain))
            return true;
    }

    return false;
}

Vector<String> NetworkStorageSession::getAllStorageAccessEntries() const
{
    Vector<String> entries;
    for (auto& innerMap : m_framesGrantedStorageAccess.values()) {
        for (auto& value : innerMap.values())
            entries.append(value.string());
    }
    return entries;
}
    
void NetworkStorageSession::grantStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID)
{
    if (NetworkStorageSession::loginDomainMatchesRequestingDomain(firstPartyDomain, resourceDomain)) {
        grantCrossPageStorageAccess(firstPartyDomain, resourceDomain);
        return;
    }

    if (!frameID) {
        if (firstPartyDomain.isEmpty())
            return;
        auto pagesGrantedIterator = m_pagesGrantedStorageAccess.find(pageID);
        if (pagesGrantedIterator == m_pagesGrantedStorageAccess.end()) {
            HashMap<RegistrableDomain, RegistrableDomain> entry;
            entry.add(firstPartyDomain, resourceDomain);
            m_pagesGrantedStorageAccess.add(pageID, entry);
        } else {
            auto firstPartyDomainIterator = pagesGrantedIterator->value.find(firstPartyDomain);
            if (firstPartyDomainIterator == pagesGrantedIterator->value.end())
                pagesGrantedIterator->value.add(firstPartyDomain, resourceDomain);
            else
                firstPartyDomainIterator->value = resourceDomain;
        }
        return;
    }

    auto pagesGrantedIterator = m_framesGrantedStorageAccess.find(pageID);
    if (pagesGrantedIterator == m_framesGrantedStorageAccess.end()) {
        HashMap<FrameIdentifier, RegistrableDomain> entry;
        entry.add(frameID.value(), resourceDomain);
        m_framesGrantedStorageAccess.add(pageID, entry);
    } else {
        auto framesGrantedIterator = pagesGrantedIterator->value.find(frameID.value());
        if (framesGrantedIterator == pagesGrantedIterator->value.end())
            pagesGrantedIterator->value.add(frameID.value(), resourceDomain);
        else
            framesGrantedIterator->value = resourceDomain;
    }
}

void NetworkStorageSession::removeStorageAccessForFrame(FrameIdentifier frameID, PageIdentifier pageID)
{
    auto iteration = m_framesGrantedStorageAccess.find(pageID);
    if (iteration == m_framesGrantedStorageAccess.end())
        return;

    iteration->value.remove(frameID);
}

void NetworkStorageSession::clearPageSpecificDataForResourceLoadStatistics(PageIdentifier pageID)
{
    m_pagesGrantedStorageAccess.remove(pageID);
    m_framesGrantedStorageAccess.remove(pageID);
    if (!m_navigationWithLinkDecorationTestMode)
        m_navigatedToWithLinkDecorationByPrevalentResource.remove(pageID);
}

void NetworkStorageSession::removeAllStorageAccess()
{
    m_pagesGrantedStorageAccess.clear();
    m_framesGrantedStorageAccess.clear();
    m_pairsGrantedCrossPageStorageAccess.clear();
}

void NetworkStorageSession::setCacheMaxAgeCapForPrevalentResources(Seconds seconds)
{
    m_cacheMaxAgeCapForPrevalentResources = seconds;
}
    
void NetworkStorageSession::resetCacheMaxAgeCapForPrevalentResources()
{
    m_cacheMaxAgeCapForPrevalentResources = std::nullopt;
}

void NetworkStorageSession::didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(const RegistrableDomain& toDomain, PageIdentifier pageID)
{
    m_navigatedToWithLinkDecorationByPrevalentResource.add(pageID, toDomain);
}

void NetworkStorageSession::resetCrossSiteLoadsWithLinkDecorationForTesting()
{
    m_navigatedToWithLinkDecorationByPrevalentResource.clear();
    m_navigationWithLinkDecorationTestMode = true;
}

void NetworkStorageSession::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode blockingMode)
{
    m_thirdPartyCookieBlockingMode = blockingMode;
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && !PLATFORM(COCOA)
void NetworkStorageSession::setOptInCookiePartitioningEnabled(bool)
{
}
#endif

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkStorageSession::setAppBoundDomains(HashSet<RegistrableDomain>&& domains)
{
    m_appBoundDomains = WTFMove(domains);
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains)
        cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::resetAppBoundDomains()
{
    m_appBoundDomains.clear();
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains)
        cookieEnabledStateMayHaveChanged();
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void NetworkStorageSession::setManagedDomains(HashSet<RegistrableDomain>&& domains)
{
    m_managedDomains = WTFMove(domains);
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::AllExceptManagedDomains)
        cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::resetManagedDomains()
{
    m_managedDomains.clear();
    if (m_thirdPartyCookieBlockingMode == ThirdPartyCookieBlockingMode::AllExceptManagedDomains)
        cookieEnabledStateMayHaveChanged();
}
#endif

std::optional<Seconds> NetworkStorageSession::clientSideCookieCap(const RegistrableDomain& firstParty, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, std::optional<PageIdentifier> pageID) const
{
    if (requiresScriptTrackingPrivacy == RequiresScriptTrackingPrivacy::Yes)
        return m_ageCapForClientSideCookiesForScriptTrackingPrivacy;

#if ENABLE(JS_COOKIE_CHECKING)
    if (!pageID)
        return std::nullopt;

    auto domainIterator = m_navigatedToWithLinkDecorationByPrevalentResource.find(*pageID);
    if (domainIterator != m_navigatedToWithLinkDecorationByPrevalentResource.end() && domainIterator->value == firstParty)
        return m_ageCapForClientSideCookiesForLinkDecorationTargetPage;

    return std::nullopt;
#else
    if (!m_ageCapForClientSideCookies || !pageID || m_navigatedToWithLinkDecorationByPrevalentResource.isEmpty())
        return m_ageCapForClientSideCookies;

    auto domainIterator = m_navigatedToWithLinkDecorationByPrevalentResource.find(*pageID);
    if (domainIterator == m_navigatedToWithLinkDecorationByPrevalentResource.end())
        return m_ageCapForClientSideCookies;

    if (domainIterator->value == firstParty)
        return m_ageCapForClientSideCookiesShort;

    return m_ageCapForClientSideCookies;
#endif
}

const HashMap<RegistrableDomain, HashSet<RegistrableDomain>>& NetworkStorageSession::storageAccessQuirks()
{
    static NeverDestroyed<HashMap<RegistrableDomain, HashSet<RegistrableDomain>>> map = [] {
        HashMap<RegistrableDomain, HashSet<RegistrableDomain>> map;
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("microsoft.com"_s),
            HashSet { RegistrableDomain::uncheckedCreateFromRegistrableDomainString("microsoftonline.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("playstation.com"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sonyentertainmentnetwork.com"_s),
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sony.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("bbc.co.uk"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("radioplayer.co.uk"_s) });
        return map;
    }();
    return map.get();
}

void NetworkStorageSession::updateStorageAccessPromptQuirks(Vector<OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirks)
{
    auto& quirks = updatableStorageAccessPromptQuirks();
    quirks.clear();
    for (auto&& quirk : organizationStorageAccessPromptQuirks)
        quirks.add(quirk);
}

bool NetworkStorageSession::loginDomainMatchesRequestingDomain(const TopFrameDomain& topFrameDomain, const SubResourceDomain& resourceDomain)
{
    auto loginDomains = NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(topFrameDomain);
    return (loginDomains && loginDomains.value().contains(resourceDomain)) || !!storageAccessQuirkForDomainPair(topFrameDomain, resourceDomain);
}

bool NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(const SubResourceDomain& resourceDomain, const TopFrameDomain& topFrameDomain)
{
    ASSERT(RunLoop::isMain());
    return loginDomainMatchesRequestingDomain(topFrameDomain, resourceDomain);
}

std::optional<HashSet<RegistrableDomain>> NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(const RegistrableDomain& topFrameDomain)
{
    auto it = storageAccessQuirks().find(topFrameDomain);
    if (it != storageAccessQuirks().end())
        return it->value;
    return std::nullopt;
}

std::optional<RegistrableDomain> NetworkStorageSession::findAdditionalLoginDomain(const TopFrameDomain& topDomain, const SubResourceDomain& subDomain)
{
    if (subDomain.string() == "sony.com"_s && topDomain.string() == "playstation.com"_s)
        return RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sonyentertainmentnetwork.com"_s);

    if (subDomain.string() == "sonyentertainmentnetwork.com"_s && topDomain.string() == "playstation.com"_s)
        return RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sony.com"_s);

    return std::nullopt;
}

Vector<RegistrableDomain> NetworkStorageSession::storageAccessQuirkForTopFrameDomain(const URL& topFrameURL)
{
    for (auto&& quirk : updatableStorageAccessPromptQuirks()) {
        if (!quirk.triggerPages.isEmpty() && !quirk.triggerPages.contains(topFrameURL))
            continue;

        auto quirkDomains = quirk.quirkDomains;
        auto entry = quirkDomains.find(RegistrableDomain { topFrameURL });
        if (entry == quirkDomains.end())
            continue;
        return entry->value;
    }
    return { };
}

std::optional<OrganizationStorageAccessPromptQuirk> NetworkStorageSession::storageAccessQuirkForDomainPair(const TopFrameDomain& topDomain, const SubResourceDomain& subDomain)
{
    for (auto&& quirk : updatableStorageAccessPromptQuirks()) {
        auto& quirkDomains = quirk.quirkDomains;
        auto entry = quirkDomains.find(topDomain);
        if (entry == quirkDomains.end())
            continue;
        if (!std::ranges::any_of(entry->value, [&subDomain](auto&& entry) { return entry == subDomain; }))
            break;
        return quirk;
    }
    return std::nullopt;
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames, CompletionHandler<void()>&& completionHandler)
{
    deleteCookiesForHostnames(cookieHostNames, IncludeHttpOnlyCookies::Yes, ScriptWrittenCookiesOnly::No, WTFMove(completionHandler));
}

#if !PLATFORM(COCOA)
void NetworkStorageSession::deleteCookies(const ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: Stop ignoring origin.topOrigin.
    notImplemented();

    deleteCookiesForHostnames(Vector { origin.clientOrigin.host() }, WTFMove(completionHandler));
}
#endif

bool NetworkStorageSession::cookiesEnabled(const URL& firstParty, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker isKnownCrossSiteTracker) const
{
    return thirdPartyCookieBlockingDecisionForRequest(firstParty, url, frameID, pageID, shouldRelaxThirdPartyCookieBlocking, isKnownCrossSiteTracker) != ThirdPartyCookieBlockingDecision::All;
}

void NetworkStorageSession::addCookiesEnabledStateObserver(CookiesEnabledStateObserver& observer)
{
    m_cookiesEnabledStateObservers.add(observer);
}

void NetworkStorageSession::removeCookiesEnabledStateObserver(CookiesEnabledStateObserver& observer)
{
    m_cookiesEnabledStateObservers.remove(observer);
}

void NetworkStorageSession::cookieEnabledStateMayHaveChanged()
{
    for (Ref observer : m_cookiesEnabledStateObservers)
        observer->cookieEnabledStateMayHaveChanged();
}

void NetworkStorageSession::setCookiesVersion(uint64_t version)
{
    // Ensure version always increases.
    if (version <= m_cookiesVersion)
        return;

    RELEASE_LOG(Loading, "%p - NetworkStorageSession::setCookiesVersion session=%" PRIu64 ", version=%" PRIu64, this, m_sessionID.toUInt64(), version);
    m_cookiesVersion = version;
    auto cookiesVersionChangeCallbacks = std::exchange(m_cookiesVersionChangeCallbacks, { });
    while (!cookiesVersionChangeCallbacks.isEmpty()) {
        auto callback = cookiesVersionChangeCallbacks.takeFirst();
        if (callback.version <= m_cookiesVersion) {
            callback.callback(CookieVersionChangeCallback::Reason::VersionChange);
            continue;
        }
        m_cookiesVersionChangeCallbacks.append(WTFMove(callback));
    }
}

void NetworkStorageSession::addCookiesVersionChangeCallback(CookieVersionChangeCallback&& callback)
{
    ASSERT(callback.version < m_cookiesVersion);
    m_cookiesVersionChangeCallbacks.append(WTFMove(callback));
}

void NetworkStorageSession::clearCookiesVersionChangeCallbacks()
{
    while (!m_cookiesVersionChangeCallbacks.isEmpty()) {
        auto callback = m_cookiesVersionChangeCallbacks.takeFirst();
        callback.callback(CookieVersionChangeCallback::Reason::SessionClose);
    }
}

}

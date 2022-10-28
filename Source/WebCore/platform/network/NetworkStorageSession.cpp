/*
 * Copyright (C) 2016-2018 Apple Inc.  All rights reserved.
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

#include "Cookie.h"
#include "CookieJar.h"
#include "HTTPCookieAcceptPolicy.h"
#include "RuntimeApplicationChecks.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>

#if ENABLE(TRACKING_PREVENTION)
#include "ResourceRequest.h"
#if ENABLE(PUBLIC_SUFFIX_LIST)
#include "PublicSuffix.h"
#endif
#endif

namespace WebCore {

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

#if !PLATFORM(COCOA)
Vector<Cookie> NetworkStorageSession::domCookiesForHost(const String&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}
#endif // !PLATFORM(COCOA)

#if ENABLE(TRACKING_PREVENTION)

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

bool NetworkStorageSession::trackingPreventionDebugLoggingEnabled() const
{
    return m_isTrackingPreventionDebugLoggingEnabled;
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

bool NetworkStorageSession::shouldBlockCookies(const ResourceRequest& request, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    return shouldBlockCookies(request.firstPartyForCookies(), request.url(), frameID, pageID, shouldRelaxThirdPartyCookieBlocking);
}
    
bool NetworkStorageSession::shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    if (shouldRelaxThirdPartyCookieBlocking == ShouldRelaxThirdPartyCookieBlocking::Yes)
        return false;

    if (!m_isTrackingPreventionEnabled)
        return false;

    RegistrableDomain firstPartyDomain { firstPartyForCookies };
    if (firstPartyDomain.isEmpty())
        return false;

    RegistrableDomain resourceDomain { resource };
    if (resourceDomain.isEmpty())
        return false;

    if (firstPartyDomain == resourceDomain)
        return false;

    if (pageID && hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID.value()))
        return false;

    switch (m_thirdPartyCookieBlockingMode) {
    case ThirdPartyCookieBlockingMode::All:
        return true;
    case ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains:
        return !shouldExemptDomainPairFromThirdPartyCookieBlocking(firstPartyDomain, resourceDomain);
    case ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction:
        if (!hasHadUserInteractionAsFirstParty(firstPartyDomain))
            return true;
        FALLTHROUGH;
    case ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy:
        return shouldBlockThirdPartyCookies(resourceDomain);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool NetworkStorageSession::shouldExemptDomainPairFromThirdPartyCookieBlocking(const RegistrableDomain& topFrameDomain, const RegistrableDomain& resourceDomain) const
{
    ASSERT(topFrameDomain != resourceDomain);
    if (topFrameDomain.isEmpty() || resourceDomain.isEmpty())
        return false;

    return topFrameDomain == resourceDomain || (m_appBoundDomains.contains(topFrameDomain) && m_appBoundDomains.contains(resourceDomain));
}

std::optional<Seconds> NetworkStorageSession::maxAgeCacheCap(const ResourceRequest& request)
{
    if (m_cacheMaxAgeCapForPrevalentResources && shouldBlockCookies(request, std::nullopt, std::nullopt, ShouldRelaxThirdPartyCookieBlocking::No))
        return m_cacheMaxAgeCapForPrevalentResources;
    return std::nullopt;
}

void NetworkStorageSession::setAgeCapForClientSideCookies(std::optional<Seconds> seconds)
{
    m_ageCapForClientSideCookies = seconds;
    m_ageCapForClientSideCookiesShort = seconds ? Seconds { seconds->seconds() / 7. } : seconds;
#if ENABLE(JS_COOKIE_CHECKING)
    m_ageCapForClientSideCookiesForLinkDecorationTargetPage = seconds;
#endif
}

void NetworkStorageSession::setPrevalentDomainsToBlockAndDeleteCookiesFor(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsToBlockAndDeleteCookiesFor.clear();
    m_registrableDomainsToBlockAndDeleteCookiesFor.add(domains.begin(), domains.end());
}

void NetworkStorageSession::setPrevalentDomainsToBlockButKeepCookiesFor(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsToBlockButKeepCookiesFor.clear();
    m_registrableDomainsToBlockButKeepCookiesFor.add(domains.begin(), domains.end());
}

void NetworkStorageSession::setDomainsWithUserInteractionAsFirstParty(const Vector<RegistrableDomain>& domains)
{
    m_registrableDomainsWithUserInteractionAsFirstParty.clear();
    m_registrableDomainsWithUserInteractionAsFirstParty.add(domains.begin(), domains.end());
}

void NetworkStorageSession::setDomainsWithCrossPageStorageAccess(const HashMap<TopFrameDomain, SubResourceDomain>& domains)
{
    m_pairsGrantedCrossPageStorageAccess.clear();
    for (auto& topFrameDomain : domains.keys())
        grantCrossPageStorageAccess(topFrameDomain, domains.get(topFrameDomain));
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

bool NetworkStorageSession::hasStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID) const
{
    if (frameID) {
        auto framesGrantedIterator = m_framesGrantedStorageAccess.find(pageID);
        if (framesGrantedIterator != m_framesGrantedStorageAccess.end()) {
            auto it = framesGrantedIterator->value.find(frameID.value());
            if (it != framesGrantedIterator->value.end() && it->value == resourceDomain)
                return true;
        }
    }

    if (!firstPartyDomain.isEmpty()) {
        auto pagesGrantedIterator = m_pagesGrantedStorageAccess.find(pageID);
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

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkStorageSession::setAppBoundDomains(HashSet<RegistrableDomain>&& domains)
{
    m_appBoundDomains = WTFMove(domains);
}

void NetworkStorageSession::resetAppBoundDomains()
{
    m_appBoundDomains.clear();
}
#endif

std::optional<Seconds> NetworkStorageSession::clientSideCookieCap(const RegistrableDomain& firstParty, std::optional<PageIdentifier> pageID) const
{
    auto domainIterator = m_navigatedToWithLinkDecorationByPrevalentResource.find(*pageID);
#if ENABLE(JS_COOKIE_CHECKING)
    if (domainIterator != m_navigatedToWithLinkDecorationByPrevalentResource.end() && domainIterator->value == firstParty)
        return m_ageCapForClientSideCookiesForLinkDecorationTargetPage;

    return std::nullopt;
#else
    if (!m_ageCapForClientSideCookies || !pageID || m_navigatedToWithLinkDecorationByPrevalentResource.isEmpty())
        return m_ageCapForClientSideCookies;

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
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("live.com"_s),
            HashSet { RegistrableDomain::uncheckedCreateFromRegistrableDomainString("skype.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("playstation.com"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sonyentertainmentnetwork.com"_s),
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sony.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("bbc.co.uk"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("radioplayer.co.uk"_s) });
        return map;
    }();
    return map.get();
}

bool NetworkStorageSession::loginDomainMatchesRequestingDomain(const TopFrameDomain& topFrameDomain, const SubResourceDomain& resourceDomain)
{
    auto loginDomains = WebCore::NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(topFrameDomain);
    return loginDomains && loginDomains.value().contains(resourceDomain);
}

bool NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(const SubResourceDomain& resourceDomain, const TopFrameDomain& topFrameDomain)
{
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

#endif // ENABLE(TRACKING_PREVENTION)

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames, CompletionHandler<void()>&& completionHandler)
{
    deleteCookiesForHostnames(cookieHostNames, IncludeHttpOnlyCookies::Yes, ScriptWrittenCookiesOnly::No, WTFMove(completionHandler));
}

}

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
#include "HTTPCookieAcceptPolicy.h"
#include "RuntimeApplicationChecks.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>

#if ENABLE(RESOURCE_LOAD_STATISTICS)
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)

bool NetworkStorageSession::shouldBlockThirdPartyCookies(const RegistrableDomain& registrableDomain) const
{
    if (!m_isResourceLoadStatisticsEnabled || registrableDomain.isEmpty())
        return false;

    ASSERT(!(m_registrableDomainsToBlockAndDeleteCookiesFor.contains(registrableDomain) && m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain)));

    return m_registrableDomainsToBlockAndDeleteCookiesFor.contains(registrableDomain)
        || m_registrableDomainsToBlockButKeepCookiesFor.contains(registrableDomain);
}

bool NetworkStorageSession::shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(const RegistrableDomain& registrableDomain) const
{
    if (!m_isResourceLoadStatisticsEnabled || registrableDomain.isEmpty())
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

bool NetworkStorageSession::shouldBlockCookies(const ResourceRequest& request, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    return shouldBlockCookies(request.firstPartyForCookies(), request.url(), frameID, pageID, shouldRelaxThirdPartyCookieBlocking);
}
    
bool NetworkStorageSession::shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking) const
{
    if (shouldRelaxThirdPartyCookieBlocking == ShouldRelaxThirdPartyCookieBlocking::Yes)
        return false;

    if (!m_isResourceLoadStatisticsEnabled)
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

Optional<Seconds> NetworkStorageSession::maxAgeCacheCap(const ResourceRequest& request)
{
    if (m_cacheMaxAgeCapForPrevalentResources && shouldBlockCookies(request, WTF::nullopt, WTF::nullopt, ShouldRelaxThirdPartyCookieBlocking::No))
        return m_cacheMaxAgeCapForPrevalentResources;
    return WTF::nullopt;
}

void NetworkStorageSession::setAgeCapForClientSideCookies(Optional<Seconds> seconds)
{
    m_ageCapForClientSideCookies = seconds;
    m_ageCapForClientSideCookiesShort = seconds ? Seconds { seconds->seconds() / 7. } : seconds;
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

bool NetworkStorageSession::hasStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID) const
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
    
void NetworkStorageSession::grantStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID)
{
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
}

void NetworkStorageSession::setCacheMaxAgeCapForPrevalentResources(Seconds seconds)
{
    m_cacheMaxAgeCapForPrevalentResources = seconds;
}
    
void NetworkStorageSession::resetCacheMaxAgeCapForPrevalentResources()
{
    m_cacheMaxAgeCapForPrevalentResources = WTF::nullopt;
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

void NetworkStorageSession::setAppBoundDomains(HashSet<RegistrableDomain>&& domains)
{
    m_appBoundDomains = WTFMove(domains);
}

void NetworkStorageSession::resetAppBoundDomains()
{
    m_appBoundDomains.clear();
}

Optional<Seconds> NetworkStorageSession::clientSideCookieCap(const RegistrableDomain& firstParty, Optional<PageIdentifier> pageID) const
{
    if (!m_ageCapForClientSideCookies || !pageID || m_navigatedToWithLinkDecorationByPrevalentResource.isEmpty())
        return m_ageCapForClientSideCookies;

    auto domainIterator = m_navigatedToWithLinkDecorationByPrevalentResource.find(*pageID);
    if (domainIterator == m_navigatedToWithLinkDecorationByPrevalentResource.end())
        return m_ageCapForClientSideCookies;

    if (domainIterator->value == firstParty)
        return m_ageCapForClientSideCookiesShort;

    return m_ageCapForClientSideCookies;
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

}

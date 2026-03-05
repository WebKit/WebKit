/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "config.h"
#include "EnhancedSecurityTracking.h"

#include <WebCore/IPAddressSpace.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

namespace WebKit {

using namespace WebCore;

using EnhancedSecuritySitesMap = HashMap<WebCore::RegistrableDomain, EnhancedSecurityReason>;

static EnhancedSecuritySitesMap& NODELETE enabledSitesMap()
{
    static MainRunLoopNeverDestroyed<EnhancedSecuritySitesMap> staticEnabledSites;
    return staticEnabledSites;
}

static bool didSitePreviouslyUseEnhancedSecurity(const API::Navigation& navigation)
{
    return enabledSitesMap().contains(RegistrableDomain { navigation.currentRequest().url() });
}

static void trackSiteSeenOutsideEnhancedSecurity(const API::Navigation& navigation)
{
    enabledSitesMap().remove(RegistrableDomain { navigation.currentRequest().url() });
}

static void updateEnhancedSecurityDomains(HashSet<RegistrableDomain>&& domains)
{
    for (const auto& domain : domains)
        enabledSitesMap().add(domain, EnhancedSecurityReason::InsecureLoad);
}

void EnhancedSecurityTracking::initializeWithWebsiteDataStore(WebsiteDataStore& websiteDataStore)
{
    websiteDataStore.fetchEnhancedSecurityOnlyDomains([](HashSet<RegistrableDomain>&& domains) {
        updateEnhancedSecurityDomains(WTF::move(domains));
    });
}

void EnhancedSecurityTracking::initializeFrom(const EnhancedSecurityTracking& other)
{
    m_activeState = other.m_activeState;
    m_activeReason = other.m_activeReason;
    m_initialProtectedDomain = other.m_initialProtectedDomain;
}

EnhancedSecurity EnhancedSecurityTracking::enhancedSecurityState() const
{
    if (m_activeState != ActivationState::Active)
        return EnhancedSecurity::Disabled;

    switch (enhancedSecurityReason()) {
    case EnhancedSecurityReason::None:
        ASSERT_NOT_REACHED();
        return EnhancedSecurity::Disabled;

    case EnhancedSecurityReason::InsecureProvisional:
    case EnhancedSecurityReason::InsecureLoad:
        return EnhancedSecurity::EnabledInsecure;

    case EnhancedSecurityReason::LinkSecurity:
        return EnhancedSecurity::EnabledLinkSecurity;

    case EnhancedSecurityReason::Policy:
        return EnhancedSecurity::EnabledPolicy;
    }

    ASSERT_NOT_REACHED();
    return EnhancedSecurity::Disabled;
}

void EnhancedSecurityTracking::reset()
{
    m_activeState = ActivationState::None;
    m_activeReason = EnhancedSecurityReason::None;
}

void EnhancedSecurityTracking::makeDormant()
{
    m_activeState = ActivationState::Dormant;
}

void EnhancedSecurityTracking::makeActive()
{
    m_activeState = ActivationState::Active;
}

static EnhancedSecurityReason NODELETE reasonForEnhancedSecurity(EnhancedSecurity state)
{
    switch (state) {
    case EnhancedSecurity::Disabled:
        return EnhancedSecurityReason::None;

    case EnhancedSecurity::EnabledInsecure:
        return EnhancedSecurityReason::InsecureLoad;

    case EnhancedSecurity::EnabledPolicy:
        return EnhancedSecurityReason::Policy;

    case EnhancedSecurity::EnabledLinkSecurity:
        return EnhancedSecurityReason::LinkSecurity;
    }

    ASSERT_NOT_REACHED();
    return EnhancedSecurityReason::None;
}

void EnhancedSecurityTracking::enableFor(EnhancedSecurityReason reason, const API::Navigation& navigation)
{
    m_activeState = ActivationState::Active;
    m_activeReason = reason;
    m_initialProtectedDomain = RegistrableDomain(navigation.currentRequest().url());

    enabledSitesMap().set(m_initialProtectedDomain, m_activeReason);
}

void EnhancedSecurityTracking::trackChangingSiteNavigation()
{
    if (enhancedSecurityReason() == EnhancedSecurityReason::InsecureProvisional) {
        m_activeReason = EnhancedSecurityReason::InsecureLoad;

        if (auto it = enabledSitesMap().find(m_initialProtectedDomain); it != enabledSitesMap().end()) {
            if (it->value == EnhancedSecurityReason::InsecureProvisional)
                it->value = EnhancedSecurityReason::InsecureLoad;
        }
    }
}

void EnhancedSecurityTracking::trackSameSiteNavigation(const API::Navigation& navigation)
{
    bool isHTTPS = navigation.currentRequest().url().protocolIs("https"_s);

    if (enhancedSecurityReason() == EnhancedSecurityReason::InsecureProvisional && isHTTPS) {
        reset();

        RegistrableDomain domain { navigation.currentRequest().url() };
        if (auto it = enabledSitesMap().find(domain); it != enabledSitesMap().end()) {
            if (it->value == EnhancedSecurityReason::InsecureProvisional)
                enabledSitesMap().remove(it);
        }
    }
}

bool EnhancedSecurityTracking::enableIfRequired(const API::Navigation& navigation)
{
    auto currentRequestURL = navigation.currentRequest().url();

    if (currentRequestURL.protocolIs("http"_s)
        && !SecurityOrigin::isLocalHostOrLoopbackIPAddress(currentRequestURL.host())) {
        enableFor(EnhancedSecurityReason::InsecureProvisional, navigation);
        return true;
    }

    if (navigation.isEnhancedSecurityLinkForCurrentSite()) {
        enableFor(EnhancedSecurityReason::LinkSecurity, navigation);
        return true;
    }

    return false;
}

void EnhancedSecurityTracking::handleBackForwardNavigation(const API::Navigation& navigation)
{
    EnhancedSecurity priorState = navigation.targetItem() ? navigation.targetItem()->enhancedSecurity() : EnhancedSecurity::Disabled;

    if (priorState == EnhancedSecurity::Disabled) {
        if (m_activeState != ActivationState::None)
            makeDormant();
    } else
        enableFor(reasonForEnhancedSecurity(priorState), navigation);
}

void EnhancedSecurityTracking::trackNavigation(const API::Navigation& navigation, bool hasOpenedPage)
{
    auto lastNavigationAction = navigation.lastNavigationAction();
    if (lastNavigationAction && lastNavigationAction->hasOpener)
        return;

    bool isRequestFromClientOrUserInput = navigation.isRequestFromClientOrUserInput() && !navigation.substituteData();

    if (navigation.hasOpenedFrames() && hasOpenedPage && !isRequestFromClientOrUserInput)
        return;

    bool isBackForward = lastNavigationAction && lastNavigationAction->navigationType == NavigationType::BackForward;
    bool isReload = lastNavigationAction && lastNavigationAction->navigationType == NavigationType::Reload;
    bool isInitialUIDriven = navigation.isRequestFromClientOrUserInput() && !navigation.currentRequestIsRedirect();

    if (isBackForward) {
        handleBackForwardNavigation(navigation);
        return;
    }

    if (m_activeState != ActivationState::None && isInitialUIDriven && !isReload)
        reset();

    if (m_activeState != ActivationState::Active && enableIfRequired(navigation))
        return;

    if (m_activeState == ActivationState::Active
        && m_activeReason == EnhancedSecurityReason::InsecureProvisional) {
        if (!m_initialProtectedDomain.matches(navigation.currentRequest().url()))
            trackChangingSiteNavigation();
        else
            trackSameSiteNavigation(navigation);
    }

    if (m_activeState == ActivationState::None) {
        trackSiteSeenOutsideEnhancedSecurity(navigation);
        return;
    }

    if (didSitePreviouslyUseEnhancedSecurity(navigation)) {
        if (m_activeState == ActivationState::Dormant)
            makeActive();

        ASSERT(m_activeState == ActivationState::Active);
        return;
    }

    bool doesSiteHaveStorage = navigation.hasStorageForCurrentSite();

    if (m_activeState == ActivationState::Dormant && !doesSiteHaveStorage)
        makeActive();

    if (m_activeState == ActivationState::Active) {
        if (doesSiteHaveStorage)
            makeDormant();
        else
            enabledSitesMap().set(RegistrableDomain { navigation.currentRequest().url() }, m_activeReason);
    }
}

} // namespace WebKit

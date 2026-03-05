/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFramePolicyListenerProxy.h"

#include "APINavigation.h"
#include "APIWebsitePolicies.h"
#include "BrowsingWarning.h"
#include "WebFrameProxy.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"

namespace WebKit {

WebFramePolicyListenerProxy::WebFramePolicyListenerProxy(Reply&& reply, OptionSet<PolicyListenerCheck> checks)
    : m_requiredChecks(checks | PolicyListenerCheck::PolicyDecision)
    , m_reply(WTF::move(reply))
{
}

WebFramePolicyListenerProxy::~WebFramePolicyListenerProxy() = default;

void WebFramePolicyListenerProxy::didReceiveAppBoundDomainResult(std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_completedChecks.contains(PolicyListenerCheck::AppBoundDomainResult));
    m_isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
    m_completedChecks.add(PolicyListenerCheck::AppBoundDomainResult);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::didReceiveSafeBrowsingResults(RefPtr<BrowsingWarning>&& safeBrowsingWarning)
{
    ASSERT(isMainRunLoop());
    if (!m_safeBrowsingWarning)
        m_safeBrowsingWarning = WTF::move(safeBrowsingWarning);
    m_completedChecks.add(PolicyListenerCheck::SafeBrowsingResult);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::didReceiveInitialLinkDecorationFilteringData()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_completedChecks.contains(PolicyListenerCheck::LinkDecorationFiltering));
    m_completedChecks.add(PolicyListenerCheck::LinkDecorationFiltering);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::didReceiveSiteHasStorageResults()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_completedChecks.contains(PolicyListenerCheck::SiteHasStorage));
    m_completedChecks.add(PolicyListenerCheck::SiteHasStorage);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::didReceiveEnhancedSecurityLinkResults()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_completedChecks.contains(PolicyListenerCheck::EnhancedSecurityLink));
    m_completedChecks.add(PolicyListenerCheck::EnhancedSecurityLink);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::didReceiveDNSResolutionResults()
{
    ASSERT(RunLoop::isMain());
    if (m_completedChecks.contains(PolicyListenerCheck::DNSResolution))
        return;
    m_completedChecks.add(PolicyListenerCheck::DNSResolution);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::fireReply()
{
    ASSERT(m_policyResult);
    if (m_reply)
        m_reply(WebCore::PolicyAction::Use, m_policyResult->first.get(), m_policyResult->second, m_isNavigatingToAppBoundDomain, WasNavigationIntercepted::No);
}

void WebFramePolicyListenerProxy::use(API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient)
{
    if (m_completedChecks.contains(PolicyListenerCheck::PolicyDecision))
        return;
    m_policyResult = { { policies, processSwapRequestedByClient } };
    m_completedChecks.add(PolicyListenerCheck::PolicyDecision);
    if (m_completedChecks == m_requiredChecks)
        fireReply();
}

void WebFramePolicyListenerProxy::download()
{
    if (m_reply)
        m_reply(WebCore::PolicyAction::Download, nullptr, ProcessSwapRequestedByClient::No, { }, WasNavigationIntercepted::No);
}

void WebFramePolicyListenerProxy::ignore(WasNavigationIntercepted wasNavigationIntercepted)
{
    if (m_reply)
        m_reply(WebCore::PolicyAction::Ignore, nullptr, ProcessSwapRequestedByClient::No, { }, wasNavigationIntercepted);
}

} // namespace WebKit

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
#include "SafeBrowsingWarning.h"
#include "WebFrameProxy.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"

namespace WebKit {

WebFramePolicyListenerProxy::WebFramePolicyListenerProxy(Reply&& reply, ShouldExpectSafeBrowsingResult expectSafeBrowsingResult, ShouldExpectAppBoundDomainResult expectAppBoundDomainResult)
    : m_reply(WTFMove(reply))
{
    if (expectSafeBrowsingResult == ShouldExpectSafeBrowsingResult::No)
        didReceiveSafeBrowsingResults({ });
    if (expectAppBoundDomainResult == ShouldExpectAppBoundDomainResult::No)
        didReceiveAppBoundDomainResult({ });
}

WebFramePolicyListenerProxy::~WebFramePolicyListenerProxy() = default;

void WebFramePolicyListenerProxy::didReceiveAppBoundDomainResult(std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    ASSERT(RunLoop::isMain());

    if (m_policyResult && m_safeBrowsingWarning) {
        if (m_reply)
            m_reply(WebCore::PolicyAction::Use, m_policyResult->first.get(), m_policyResult->second, WTFMove(*m_safeBrowsingWarning), isNavigatingToAppBoundDomain);
    } else
        m_isNavigatingToAppBoundDomain = isNavigatingToAppBoundDomain;
}

void WebFramePolicyListenerProxy::didReceiveSafeBrowsingResults(RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning)
{
    ASSERT(isMainRunLoop());
    ASSERT(!m_safeBrowsingWarning);
    if (m_policyResult && m_isNavigatingToAppBoundDomain) {
        if (m_reply)
            m_reply(WebCore::PolicyAction::Use, m_policyResult->first.get(), m_policyResult->second, WTFMove(safeBrowsingWarning), *m_isNavigatingToAppBoundDomain);
    } else
        m_safeBrowsingWarning = WTFMove(safeBrowsingWarning);
}

void WebFramePolicyListenerProxy::use(API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient)
{
    if (m_safeBrowsingWarning && m_isNavigatingToAppBoundDomain) {
        if (m_reply)
            m_reply(WebCore::PolicyAction::Use, policies, processSwapRequestedByClient, WTFMove(*m_safeBrowsingWarning), *m_isNavigatingToAppBoundDomain);
    } else if (!m_policyResult)
        m_policyResult = {{ policies, processSwapRequestedByClient }};
}

void WebFramePolicyListenerProxy::download()
{
    if (m_reply)
        m_reply(WebCore::PolicyAction::Download, nullptr, ProcessSwapRequestedByClient::No, { }, { });
}

void WebFramePolicyListenerProxy::ignore()
{
    if (m_reply)
        m_reply(WebCore::PolicyAction::Ignore, nullptr, ProcessSwapRequestedByClient::No, { }, { });
}

} // namespace WebKit

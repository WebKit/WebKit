/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "APIWebsiteDataStore.h"
#include "APIWebsitePolicies.h"
#include "WebFrameProxy.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"

namespace WebKit {

WebFramePolicyListenerProxy::WebFramePolicyListenerProxy(WebFrameProxy* frame, uint64_t listenerID, PolicyListenerType policyType)
    : m_policyType(policyType)
    , m_frame(frame)
    , m_listenerID(listenerID)
{
}

void WebFramePolicyListenerProxy::receivedPolicyDecision(WebCore::PolicyAction action, std::optional<WebsitePoliciesData>&& data, ShouldProcessSwapIfPossible swap)
{
    if (!m_frame)
        return;
    
    m_frame->receivedPolicyDecision(action, m_listenerID, m_navigation.get(), WTFMove(data), swap);
    m_frame = nullptr;
}

void WebFramePolicyListenerProxy::setNavigation(Ref<API::Navigation>&& navigation)
{
    m_navigation = WTFMove(navigation);
}
    
void WebFramePolicyListenerProxy::use(API::WebsitePolicies* policies, ShouldProcessSwapIfPossible swap)
{
    std::optional<WebsitePoliciesData> data;
    if (policies) {
        data = policies->data();
        if (m_frame && policies->websiteDataStore())
            m_frame->changeWebsiteDataStore(policies->websiteDataStore()->websiteDataStore());
    }

    receivedPolicyDecision(WebCore::PolicyAction::Use, WTFMove(data), swap);
}

void WebFramePolicyListenerProxy::download()
{
    receivedPolicyDecision(WebCore::PolicyAction::Download, std::nullopt, ShouldProcessSwapIfPossible::No);
}

void WebFramePolicyListenerProxy::ignore()
{
    receivedPolicyDecision(WebCore::PolicyAction::Ignore, std::nullopt, ShouldProcessSwapIfPossible::No);
}

} // namespace WebKit

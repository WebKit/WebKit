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

#pragma once

#include "APIObject.h"
#include "PolicyDecision.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/CompletionHandler.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace API {
class WebsitePolicies;
}

namespace WebKit {

class BrowsingWarning;

enum class ProcessSwapRequestedByClient : bool { No, Yes };
enum class WasNavigationIntercepted : bool { No, Yes };

enum class PolicyListenerCheck : uint8_t {
    PolicyDecision          = 1 << 0,
    SafeBrowsingResult      = 1 << 1,
    AppBoundDomainResult    = 1 << 2,
    LinkDecorationFiltering = 1 << 3,
    SiteHasStorage          = 1 << 4,
    EnhancedSecurityLink    = 1 << 5,
    DNSResolution           = 1 << 6,
};

class WebFramePolicyListenerProxy : public API::ObjectImpl<API::Object::Type::FramePolicyListener>, public CanMakeWeakPtr<WebFramePolicyListenerProxy> {
public:

    using Reply = CompletionHandler<void(WebCore::PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>;
    static Ref<WebFramePolicyListenerProxy> create(Reply&& reply, OptionSet<PolicyListenerCheck> checks)
    {
        return adoptRef(*new WebFramePolicyListenerProxy(WTF::move(reply), checks));
    }
    ~WebFramePolicyListenerProxy();

    void use(API::WebsitePolicies* = nullptr, ProcessSwapRequestedByClient = ProcessSwapRequestedByClient::No);
    void download();
    void ignore(WasNavigationIntercepted = WasNavigationIntercepted::No);

    void didReceiveSafeBrowsingResults(RefPtr<BrowsingWarning>&&);
    void didReceiveAppBoundDomainResult(std::optional<NavigatingToAppBoundDomain>);
    void didReceiveManagedDomainResult(std::optional<NavigatingToAppBoundDomain>);
    void didReceiveInitialLinkDecorationFilteringData();
    void didReceiveSiteHasStorageResults();
    void didReceiveEnhancedSecurityLinkResults();
    void didReceiveDNSResolutionResults();

private:
    WebFramePolicyListenerProxy(Reply&&, OptionSet<PolicyListenerCheck>);

    void fireReply();

    std::optional<std::pair<RefPtr<API::WebsitePolicies>, ProcessSwapRequestedByClient>> m_policyResult;
    std::optional<RefPtr<BrowsingWarning>> m_safeBrowsingWarning;
    std::optional<NavigatingToAppBoundDomain> m_isNavigatingToAppBoundDomain;
    OptionSet<PolicyListenerCheck> m_requiredChecks;
    OptionSet<PolicyListenerCheck> m_completedChecks;
    Reply m_reply;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebFramePolicyListenerProxy)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::FramePolicyListener; }
SPECIALIZE_TYPE_TRAITS_END()

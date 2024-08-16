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

#pragma once

#include <WebCore/NavigationIdentifier.h>
#include <optional>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/Scope.h>

namespace WebCore {
enum class PolicyAction : uint8_t;
enum class PolicyDecisionMode;
class FormState;
class HitTestResult;
class NavigationAction;
class ResourceRequest;
class ResourceResponse;
using FramePolicyFunction = CompletionHandler<void(PolicyAction)>;
using SandboxFlags = int;
}

namespace WebKit {

class WebFrame;
struct NavigationActionData;
struct WebsitePoliciesData;

class WebFrameLoaderClient {
public:
    WebFrame& webFrame() const { return m_frame.get(); }

    std::optional<NavigationActionData> navigationActionData(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, const String& clientRedirectSourceForHistory, std::optional<WebCore::NavigationIdentifier>, std::optional<WebCore::HitTestResult>&&, bool hasOpener, WebCore::SandboxFlags) const;

    virtual void applyWebsitePolicies(WebsitePoliciesData&&) = 0;

    virtual ~WebFrameLoaderClient();

    ScopeExit<Function<void()>> takeFrameInvalidator() { return WTFMove(m_frameInvalidator); }

protected:
    WebFrameLoaderClient(Ref<WebFrame>&&, ScopeExit<Function<void()>>&& frameInvalidator);

    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, const String&, std::optional<WebCore::NavigationIdentifier>, std::optional<WebCore::HitTestResult>&&, bool, WebCore::SandboxFlags, WebCore::PolicyDecisionMode, WebCore::FramePolicyFunction&&);

    Ref<WebFrame> m_frame;
    ScopeExit<Function<void()>> m_frameInvalidator;
};

}

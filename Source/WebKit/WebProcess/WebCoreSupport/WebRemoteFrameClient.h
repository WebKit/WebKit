/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RemoteFrameClient.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Scope.h>

namespace WebKit {

class WebRemoteFrameClient final : public WebCore::RemoteFrameClient, public WebFrameLoaderClient {
public:
    explicit WebRemoteFrameClient(Ref<WebFrame>&&, ScopeExit<Function<void()>>&& frameInvalidator);
    ~WebRemoteFrameClient();

    ScopeExit<Function<void()>> takeFrameInvalidator() { return WTFMove(m_frameInvalidator); }

private:
    void frameDetached() final;
    void sizeDidChange(WebCore::IntSize) final;
    void postMessageToRemote(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData> targetOrigin, const WebCore::MessageWithMessagePorts&) final;
    void changeLocation(WebCore::FrameLoadRequest&&) final;
    String renderTreeAsText(size_t baseIndent, OptionSet<WebCore::RenderAsTextFlag>) final;
    void broadcastFrameRemovalToOtherProcesses() final;
    void closePage() final;
    void focus() final;
    void unfocus() final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, const String& clientRedirectSourceForHistory, uint64_t navigationID, std::optional<WebCore::HitTestResult>&&, bool hasOpener, WebCore::SandboxFlags, WebCore::PolicyDecisionMode, WebCore::FramePolicyFunction&&) final;

    ScopeExit<Function<void()>> m_frameInvalidator;
};

}

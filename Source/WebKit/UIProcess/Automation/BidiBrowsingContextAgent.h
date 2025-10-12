/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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


#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <WebCore/FrameIdentifier.h>
#include <cstdint>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

struct FrameTreeNodeData;
class WebAutomationSession;
class WebPageProxy;

class BidiBrowsingContextAgent final : public Inspector::BidiBrowsingContextBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiBrowsingContextAgent);
public:
    BidiBrowsingContextAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiBrowsingContextAgent() override;

    // Inspector::BidiBrowsingContextDispatcherHandler methods.
    void activate(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, Inspector::CommandCallback<void>&&) override;
    void close(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, std::optional<bool>&& optionalPromptUnload, Inspector::CommandCallback<void>&&) override;
    void create(Inspector::Protocol::BidiBrowsingContext::CreateType, const Inspector::Protocol::BidiBrowsingContext::BrowsingContext& optionalReferenceContext, std::optional<bool>&& optionalBackground, const String& optionalUserContext, Inspector::CommandCallback<String>&&) override;
    void getTree(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext& optionalRoot, std::optional<double>&& optionalMaxDepth, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>>>&&) override;
    void handleUserPrompt(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, std::optional<bool>&& optionalShouldAccept, const String& userText, Inspector::CommandCallback<void>&&) override;
    void navigate(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, const String& url, std::optional<Inspector::Protocol::BidiBrowsingContext::ReadinessState>&&, Inspector::CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::NavigationID>&&) override;
    void reload(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, std::optional<bool>&& optionalIgnoreCache, std::optional<Inspector::Protocol::BidiBrowsingContext::ReadinessState>&& optionalWait, Inspector::CommandCallbackOf<String, Inspector::Protocol::BidiBrowsingContext::NavigationID>&&) override;

private:
    enum class IncludeParentID: bool { No, Yes };

    void getNextTree(Vector<Ref<WebPageProxy>>&&, Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>>, std::optional<uint64_t> maxDepth, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>>>&&);
    Ref<Inspector::Protocol::BidiBrowsingContext::Info> getNavigableInfo(const WebKit::FrameTreeNodeData&, std::optional<uint64_t> maxDepth, IncludeParentID);
    Inspector::Protocol::BidiBrowsingContext::BrowsingContext getBrowsingContextID(const WebCore::FrameIdentifier&) const;

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiBrowsingContextBackendDispatcher> m_browsingContextDomainDispatcher;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "InspectorTargetProxy.h"
#include <JavaScriptCore/InspectorAgentRegistry.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
}

namespace WebKit {

class InspectorBrowserAgent;
struct WebPageAgentContext;

class WebPageInspectorController {
    WTF_MAKE_NONCOPYABLE(WebPageInspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebPageInspectorController(WebPageProxy&);

    void init();
    void pageClosed();

    bool hasLocalFrontend() const;

    void connectFrontend(Inspector::FrontendChannel&, bool isAutomaticInspection = false, bool immediatelyPause = false);
    void disconnectFrontend(Inspector::FrontendChannel&);
    void disconnectAllFrontends();

    void dispatchMessageFromFrontend(const String& message);

#if ENABLE(REMOTE_INSPECTOR)
    void setIndicating(bool);
#endif

    void createInspectorTarget(const String& targetId, Inspector::InspectorTargetType);
    void destroyInspectorTarget(const String& targetId);
    void sendMessageToInspectorFrontend(const String& targetId, const String& message);

    bool shouldPauseLoading(const ProvisionalPageProxy&) const;
    void setContinueLoadingCallback(const ProvisionalPageProxy&, WTF::Function<void()>&&);

    void didCreateProvisionalPage(ProvisionalPageProxy&);
    void willDestroyProvisionalPage(const ProvisionalPageProxy&);
    void didCommitProvisionalPage(WebCore::PageIdentifier oldWebPageID, WebCore::PageIdentifier newWebPageID);

    InspectorBrowserAgent* enabledBrowserAgent() const { return m_enabledBrowserAgent; }
    void setEnabledBrowserAgent(InspectorBrowserAgent*);

    void browserExtensionsEnabled(HashMap<String, String>&&);
    void browserExtensionsDisabled(HashSet<String>&&);

private:
    WebPageAgentContext webPageAgentContext();
    void createLazyAgents();

    void addTarget(std::unique_ptr<InspectorTargetProxy>&&);

    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Inspector::AgentRegistry m_agents;

    WebPageProxy& m_inspectedPage;

    Inspector::InspectorTargetAgent* m_targetAgent { nullptr };
    HashMap<String, std::unique_ptr<InspectorTargetProxy>> m_targets;

    InspectorBrowserAgent* m_enabledBrowserAgent { nullptr };

    bool m_didCreateLazyAgents { false };
};

} // namespace WebKit

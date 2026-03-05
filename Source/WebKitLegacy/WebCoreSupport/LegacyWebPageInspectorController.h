/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/InspectorAgentRegistry.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorTarget.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class LocalFrame;
class Page;
}

class LegacyWebPageInspectorController : public RefCountedAndCanMakeWeakPtr<LegacyWebPageInspectorController> {
    WTF_MAKE_TZONE_ALLOCATED(LegacyWebPageInspectorController);
    WTF_MAKE_NONCOPYABLE(LegacyWebPageInspectorController);
public:
    static Ref<LegacyWebPageInspectorController> create(WebCore::Page&);

    void frameCreated(WebCore::LocalFrame&);
    void willDestroyFrame(const WebCore::LocalFrame&);
    void willDestroyPage(const WebCore::Page&);

    void connectFrontend(Inspector::FrontendChannel&);
    void disconnectFrontend(Inspector::FrontendChannel&);
    void disconnectAllFrontends();
    bool hasLocalFrontend() const;

    void dispatchMessageFromFrontend(const String& message);
    void sendMessageToInspectorFrontend(const String& targetID, const String& message);

private:
    explicit LegacyWebPageInspectorController(WebCore::Page&);

    Inspector::InspectorTargetAgent* targetAgent() const { return m_targetAgent; }

    void addTarget(std::unique_ptr<Inspector::InspectorTarget>&&);
    void removeTarget(const String& targetID);

    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Inspector::AgentRegistry m_agents;
    CheckedPtr<Inspector::InspectorTargetAgent> m_targetAgent;
    HashMap<String, std::unique_ptr<Inspector::InspectorTarget>> m_targets;
};

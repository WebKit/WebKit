/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/InspectorAgentRegistry.h>
#include <JavaScriptCore/InspectorEnvironment.h>
#include <WebCore/InspectorOverlay.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
}

namespace WebCore {

class InspectorBackendClient;
class InspectorFrontendClient;
class InspectorInstrumentation;
class InstrumentingAgents;
class LocalFrame;
class WebInjectedScriptManager;
struct FrameAgentContext;

class FrameInspectorController final : public Inspector::InspectorEnvironment, public CanMakeWeakPtr<FrameInspectorController> {
    WTF_MAKE_NONCOPYABLE(FrameInspectorController);
    WTF_MAKE_TZONE_ALLOCATED(FrameInspectorController);
public:
    FrameInspectorController(LocalFrame&);
    ~FrameInspectorController() override;

    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;

    WEBCORE_EXPORT void connectFrontend(Inspector::FrontendChannel&, bool isAutomaticInspection = false, bool immediatelyPause = false);
    WEBCORE_EXPORT void disconnectFrontend(Inspector::FrontendChannel&);
    WEBCORE_EXPORT void dispatchMessageFromFrontend(const String& message);

    void inspectedFrameDestroyed();

    // InspectorEnvironment
    bool developerExtrasEnabled() const override;
    bool canAccessInspectedScriptState(JSC::JSGlobalObject*) const override;
    Inspector::InspectorFunctionCallHandler functionCallHandler() const override;
    Inspector::InspectorEvaluateHandler evaluateHandler() const override;
    void frontendInitialized() override;
    WTF::Stopwatch& executionStopwatch() const final;
    JSC::Debugger* debugger() override;
    JSC::VM& vm() override;

private:
    friend class InspectorInstrumentation;

    FrameAgentContext frameAgentContext();
    void createLazyAgents();

    WeakRef<LocalFrame> m_frame;
    const Ref<InstrumentingAgents> m_instrumentingAgents;
    const UniqueRef<WebInjectedScriptManager> m_injectedScriptManager;
    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    const Ref<WTF::Stopwatch> m_executionStopwatch;
    Inspector::AgentRegistry m_agents;

    bool m_didCreateLazyAgents { false };
    WeakPtr<InspectorFrontendClient> m_inspectorFrontendClient;
};

} // namespace WebCore

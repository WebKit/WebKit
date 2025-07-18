/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "WorkerDebuggerAgent.h"
#include "WorkerOrWorkletGlobalScope.h"
#include <JavaScriptCore/InspectorAgentRegistry.h>
#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMalloc.h>

namespace Inspector {
class FrontendChannel;
class FrontendRouter;
};

namespace WebCore {

class InstrumentingAgents;
class WebInjectedScriptManager;
class WorkerDebugger;
struct WorkerAgentContext;

class WorkerInspectorController final : public Inspector::InspectorEnvironment {
    WTF_MAKE_NONCOPYABLE(WorkerInspectorController);
    WTF_MAKE_TZONE_ALLOCATED(WorkerInspectorController);
public:
    explicit WorkerInspectorController(WorkerOrWorkletGlobalScope&);
    ~WorkerInspectorController() override;

    void workerTerminating();

    void connectFrontend(bool isAutomaticInspection = false, bool immediatelyPause = false);
    void disconnectFrontend(Inspector::DisconnectReason);

    void dispatchMessageFromFrontend(const String&);

    // InspectorEnvironment
    bool developerExtrasEnabled() const override { return true; }
    bool canAccessInspectedScriptState(JSC::JSGlobalObject*) const override { return true; }
    Inspector::InspectorFunctionCallHandler functionCallHandler() const override;
    Inspector::InspectorEvaluateHandler evaluateHandler() const override;
    void frontendInitialized() final;
    WTF::Stopwatch& executionStopwatch() const override;
    JSC::Debugger* debugger() override;
    JSC::VM& vm() override;

private:
    friend class InspectorInstrumentation;

    WorkerAgentContext workerAgentContext();
    void createLazyAgents();
    WorkerDebuggerAgent& ensureDebuggerAgent();

    void updateServiceWorkerPageFrontendCount();

    const Ref<InstrumentingAgents> m_instrumentingAgents;
    const UniqueRef<WebInjectedScriptManager> m_injectedScriptManager;
    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    const Ref<WTF::Stopwatch> m_executionStopwatch;
    std::unique_ptr<WorkerDebugger> m_debugger;
    Inspector::AgentRegistry m_agents;
    CheckedPtr<WorkerDebuggerAgent> m_debuggerAgent;
    WeakRef<WorkerOrWorkletGlobalScope> m_globalScope;
    std::unique_ptr<Inspector::FrontendChannel> m_forwardingChannel;
    bool m_didCreateLazyAgents { false };
    bool m_isAutomaticInspection { false };
    bool m_pauseAfterInitialization { false };
    Function<void()> m_frontendInitializedCallback;
};

} // namespace WebCore

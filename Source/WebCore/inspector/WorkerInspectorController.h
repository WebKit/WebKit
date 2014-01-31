/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WorkerInspectorController_h
#define WorkerInspectorController_h

#if ENABLE(INSPECTOR)

#include "InspectorInstrumentationCookie.h"
#include "InspectorWebAgentBase.h"
#include <inspector/InspectorAgentRegistry.h>
#include <inspector/InspectorEnvironment.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace Inspector {
class InspectorBackendDispatcher;
}

namespace WebCore {

class InspectorInstrumentation;
class InstrumentingAgents;
class PageInjectedScriptManager;
class WorkerGlobalScope;
class WorkerRuntimeAgent;

class WorkerInspectorController final : public Inspector::InspectorEnvironment {
    WTF_MAKE_NONCOPYABLE(WorkerInspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerInspectorController(WorkerGlobalScope&);
    ~WorkerInspectorController();

    void connectFrontend();
    void disconnectFrontend(Inspector::InspectorDisconnectReason);
    void dispatchMessageFromFrontend(const String&);
    void resume();

    virtual bool developerExtrasEnabled() const override { return true; }
    virtual bool canAccessInspectedScriptState(JSC::ExecState*) const override { return true; }
    virtual Inspector::InspectorFunctionCallHandler functionCallHandler() const override;
    virtual Inspector::InspectorEvaluateHandler evaluateHandler() const override;
    virtual void willCallInjectedScriptFunction(JSC::ExecState*, const String& scriptName, int scriptLine) override;
    virtual void didCallInjectedScriptFunction(JSC::ExecState*) override;

private:
    friend InstrumentingAgents* instrumentationForWorkerGlobalScope(WorkerGlobalScope*);

    WorkerGlobalScope& m_workerGlobalScope;
    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    std::unique_ptr<PageInjectedScriptManager> m_injectedScriptManager;
    WorkerRuntimeAgent* m_runtimeAgent;
    Inspector::InspectorAgentRegistry m_agents;
    std::unique_ptr<InspectorFrontendChannel> m_frontendChannel;
    RefPtr<Inspector::InspectorBackendDispatcher> m_backendDispatcher;
    Vector<InspectorInstrumentationCookie, 2> m_injectedScriptInstrumentationCookies;
};

}

#endif // ENABLE(INSPECTOR)

#endif // !defined(WorkerInspectorController_h)

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WorkerInspectorController.h"

#include "CommandLineAPIHost.h"
#include "InspectorInstrumentation.h"
#include "InspectorTimelineAgent.h"
#include "InstrumentingAgents.h"
#include "JSMainThreadExecState.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include "WorkerConsoleAgent.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerGlobalScope.h"
#include "WorkerReportingProxy.h"
#include "WorkerRuntimeAgent.h"
#include "WorkerThread.h"
#include <inspector/InspectorBackendDispatcher.h>
#include <inspector/InspectorFrontendChannel.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorFrontendRouter.h>
#include <wtf/Stopwatch.h>

using namespace Inspector;

namespace WebCore {

namespace {

class PageInspectorProxy : public FrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PageInspectorProxy(WorkerGlobalScope& workerGlobalScope)
        : m_workerGlobalScope(workerGlobalScope) { }
    virtual ~PageInspectorProxy() { }

    virtual ConnectionType connectionType() const override { return ConnectionType::Local; }
private:
    virtual bool sendMessageToFrontend(const String& message) override
    {
        m_workerGlobalScope.thread().workerReportingProxy().postMessageToPageInspector(message);
        return true;
    }
    WorkerGlobalScope& m_workerGlobalScope;
};

}

WorkerInspectorController::WorkerInspectorController(WorkerGlobalScope& workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
    , m_instrumentingAgents(InstrumentingAgents::create(*this))
    , m_injectedScriptManager(std::make_unique<WebInjectedScriptManager>(*this, WebInjectedScriptHost::create()))
    , m_executionStopwatch(Stopwatch::create())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
{
    AgentContext baseContext = {
        *this,
        *m_injectedScriptManager,
        m_frontendRouter.get(),
        m_backendDispatcher.get()
    };

    WebAgentContext webContext = {
        baseContext,
        m_instrumentingAgents.get()
    };

    WorkerAgentContext workerContext = {
        webContext,
        workerGlobalScope,
    };

    auto runtimeAgent = std::make_unique<WorkerRuntimeAgent>(workerContext);
    m_runtimeAgent = runtimeAgent.get();
    m_instrumentingAgents->setWorkerRuntimeAgent(m_runtimeAgent);
    m_agents.append(WTF::move(runtimeAgent));

    auto consoleAgent = std::make_unique<WorkerConsoleAgent>(workerContext);
    m_instrumentingAgents->setWebConsoleAgent(consoleAgent.get());

    auto debuggerAgent = std::make_unique<WorkerDebuggerAgent>(workerContext);
    m_runtimeAgent->setScriptDebugServer(&debuggerAgent->scriptDebugServer());
    m_agents.append(WTF::move(debuggerAgent));

    m_agents.append(std::make_unique<InspectorTimelineAgent>(workerContext, nullptr, InspectorTimelineAgent::WorkerInspector));
    m_agents.append(WTF::move(consoleAgent));

    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost()) {
        commandLineAPIHost->init(nullptr
            , nullptr
            , nullptr
            , nullptr
            , nullptr
        );
    }
}
 
WorkerInspectorController::~WorkerInspectorController()
{
    ASSERT(!m_frontendRouter->hasFrontends());
    ASSERT(!m_forwardingChannel);

    m_instrumentingAgents->reset();
}

void WorkerInspectorController::connectFrontend()
{
    ASSERT(!m_frontendRouter->hasFrontends());
    ASSERT(!m_forwardingChannel);

    m_forwardingChannel = std::make_unique<PageInspectorProxy>(m_workerGlobalScope);
    m_frontendRouter->connectFrontend(m_forwardingChannel.get());
    m_agents.didCreateFrontendAndBackend(&m_frontendRouter.get(), &m_backendDispatcher.get());
}

void WorkerInspectorController::disconnectFrontend(Inspector::DisconnectReason reason)
{
    ASSERT(m_frontendRouter->hasFrontends());
    ASSERT(m_forwardingChannel);

    m_agents.willDestroyFrontendAndBackend(reason);
    m_frontendRouter->disconnectFrontend(m_forwardingChannel.get());
    m_forwardingChannel = nullptr;
}

void WorkerInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

void WorkerInspectorController::resume()
{
    ErrorString unused;
    m_runtimeAgent->run(unused);
}

InspectorFunctionCallHandler WorkerInspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler WorkerInspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

void WorkerInspectorController::willCallInjectedScriptFunction(JSC::ExecState* scriptState, const String& scriptName, int scriptLine)
{
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(scriptState);
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willCallFunction(scriptExecutionContext, scriptName, scriptLine);
    m_injectedScriptInstrumentationCookies.append(cookie);
}

void WorkerInspectorController::didCallInjectedScriptFunction(JSC::ExecState* scriptState)
{
    ASSERT(!m_injectedScriptInstrumentationCookies.isEmpty());
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(scriptState);
    InspectorInstrumentationCookie cookie = m_injectedScriptInstrumentationCookies.takeLast();
    InspectorInstrumentation::didCallFunction(cookie, scriptExecutionContext);
}

Ref<Stopwatch> WorkerInspectorController::executionStopwatch()
{
    return m_executionStopwatch.copyRef();
}

} // namespace WebCore

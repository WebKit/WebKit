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

#include "config.h"

#if ENABLE(INSPECTOR) && ENABLE(WORKERS)

#include "WorkerInspectorController.h"

#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorClient.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendChannel.h"
#include "InspectorRuntimeAgent.h"
#include "InspectorState.h"
#include "InspectorStateClient.h"
#include "InstrumentingAgents.h"
#include "WorkerContext.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerReportingProxy.h"
#include "WorkerThread.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

namespace {

class WorkerRuntimeAgent : public InspectorRuntimeAgent {
public:
    WorkerRuntimeAgent(InstrumentingAgents* instrumentingAgents, InjectedScriptManager* injectedScriptManager, WorkerContext* workerContext)
        : InspectorRuntimeAgent(instrumentingAgents, injectedScriptManager)
        , m_workerContext(workerContext) { }
    virtual ~WorkerRuntimeAgent() { }

private:
    virtual ScriptState* scriptStateForFrameId(const String&)
    {
        return 0;
    }

    virtual ScriptState* getDefaultInspectedState()
    {
        return scriptStateFromWorkerContext(m_workerContext);
    }

    WorkerContext* m_workerContext;
};

class PageInspectorProxy : public InspectorFrontendChannel {
public:
    explicit PageInspectorProxy(WorkerContext* workerContext) : m_workerContext(workerContext) { }
    virtual ~PageInspectorProxy() { }
private:
    virtual bool sendMessageToFrontend(const String& message)
    {
        m_workerContext->thread()->workerReportingProxy().postMessageToPageInspector(message);
        return true;
    }
    WorkerContext* m_workerContext;
};

class WorkerStateClient : public InspectorStateClient {
public:
    WorkerStateClient(WorkerContext* context) : m_workerContext(context) { }
    virtual ~WorkerStateClient() { }

private:
    virtual void updateInspectorStateCookie(const String& cookie)
    {
        m_workerContext->thread()->workerReportingProxy().updateInspectorStateCookie(cookie);
    }

    WorkerContext* m_workerContext;
};

}

WorkerInspectorController::WorkerInspectorController(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_stateClient(adoptPtr(new WorkerStateClient(workerContext)))
    , m_state(adoptPtr(new InspectorState(m_stateClient.get())))
    , m_instrumentingAgents(adoptPtr(new InstrumentingAgents()))
    , m_injectedScriptManager(InjectedScriptManager::createForWorker())
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerAgent(WorkerDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), workerContext, m_injectedScriptManager.get()))
#endif
    , m_runtimeAgent(adoptPtr(new WorkerRuntimeAgent(m_instrumentingAgents.get(), m_injectedScriptManager.get(), workerContext)))
{
    m_injectedScriptManager->injectedScriptHost()->init(0
        , 0
#if ENABLE(SQL_DATABASE)
        , 0
#endif
        , 0
    );

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_runtimeAgent->setScriptDebugServer(&m_debuggerAgent->scriptDebugServer());
#endif
}
 
WorkerInspectorController::~WorkerInspectorController()
{
    disconnectFrontend();
}

void WorkerInspectorController::connectFrontend()
{
    ASSERT(!m_frontend);
    m_state->unmute();
    m_frontendChannel = adoptPtr(new PageInspectorProxy(m_workerContext));
    m_frontend = adoptPtr(new InspectorFrontend(m_frontendChannel.get()));
    m_backendDispatcher = adoptRef(new InspectorBackendDispatcher(
        m_frontendChannel.get(),
        0, // InspectorApplicationCacheAgent
#if ENABLE(JAVASCRIPT_DEBUGGER)
        0, // InspectorDOMDebuggerAgent
#endif
        0, // InspectorCSSAgent
        0, // InspectorConsoleAgent
        0, // InspectorDOMAgent
        0, // InspectorDOMStorageAgent
#if ENABLE(SQL_DATABASE)
        0, // InspectorDatabaseAgent
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
        m_debuggerAgent.get(),
#endif
        0, // InspectorResourceAgent
        0, // InspectorPageAgent
#if ENABLE(JAVASCRIPT_DEBUGGER)
        0, // InspectorProfilerAgent
#endif
        m_runtimeAgent.get(),
        0, // InspectorTimelineAgent
        0 // InspectorWorkerAgent
    ));

    m_injectedScriptManager->injectedScriptHost()->setFrontend(m_frontend.get());
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->setFrontend(m_frontend.get());
#endif
}

void WorkerInspectorController::disconnectFrontend()
{
    if (!m_frontend)
        return;
    m_backendDispatcher->clearFrontend();
    m_backendDispatcher.clear();
    // Destroying agents would change the state, but we don't want that.
    // Pre-disconnect state will be used to restore inspector agents.
    m_state->mute();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->clearFrontend();
#endif
    m_injectedScriptManager->injectedScriptHost()->clearFrontend();

    m_frontend.clear();
    m_frontendChannel.clear();
}

void WorkerInspectorController::restoreInspectorStateFromCookie(const String& inspectorCookie)
{
    ASSERT(!m_frontend);
    connectFrontend();
    m_state->loadFromCookie(inspectorCookie);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->restore();
#endif
}

void WorkerInspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_backendDispatcher)
        m_backendDispatcher->dispatch(message);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void WorkerInspectorController::resume()
{
    ErrorString unused;
    m_runtimeAgent->run(&unused);
}
#endif

}

#endif

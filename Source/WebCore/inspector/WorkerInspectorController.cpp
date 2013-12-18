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

#if ENABLE(INSPECTOR)

#include "WorkerInspectorController.h"

#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorForwarding.h"
#include "InspectorHeapProfilerAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "WorkerConsoleAgent.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerGlobalScope.h"
#include "WorkerReportingProxy.h"
#include "WorkerRuntimeAgent.h"
#include "WorkerThread.h"
#include <inspector/InspectorBackendDispatcher.h>
#include <wtf/PassOwnPtr.h>

using namespace Inspector;

namespace WebCore {

namespace {

class PageInspectorProxy : public InspectorFrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PageInspectorProxy(WorkerGlobalScope* workerGlobalScope) : m_workerGlobalScope(workerGlobalScope) { }
    virtual ~PageInspectorProxy() { }
private:
    virtual bool sendMessageToFrontend(const String& message)
    {
        m_workerGlobalScope->thread()->workerReportingProxy().postMessageToPageInspector(message);
        return true;
    }
    WorkerGlobalScope* m_workerGlobalScope;
};

}

WorkerInspectorController::WorkerInspectorController(WorkerGlobalScope* workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
    , m_instrumentingAgents(InstrumentingAgents::create())
    , m_injectedScriptManager(InjectedScriptManager::createForWorker())
    , m_runtimeAgent(0)
{
    OwnPtr<InspectorRuntimeAgent> runtimeAgent = WorkerRuntimeAgent::create(m_instrumentingAgents.get(), m_injectedScriptManager.get(), workerGlobalScope);
    m_runtimeAgent = runtimeAgent.get();
    m_agents.append(runtimeAgent.release());

    OwnPtr<InspectorConsoleAgent> consoleAgent = WorkerConsoleAgent::create(m_instrumentingAgents.get(), m_injectedScriptManager.get());
#if ENABLE(JAVASCRIPT_DEBUGGER)
    OwnPtr<InspectorDebuggerAgent> debuggerAgent = WorkerDebuggerAgent::create(m_instrumentingAgents.get(), workerGlobalScope, m_injectedScriptManager.get());
    m_runtimeAgent->setScriptDebugServer(&debuggerAgent->scriptDebugServer());
    m_agents.append(debuggerAgent.release());

    m_agents.append(InspectorProfilerAgent::create(m_instrumentingAgents.get(), consoleAgent.get(), workerGlobalScope, m_injectedScriptManager.get()));
    m_agents.append(InspectorHeapProfilerAgent::create(m_instrumentingAgents.get(), m_injectedScriptManager.get()));
#endif
    m_agents.append(InspectorTimelineAgent::create(m_instrumentingAgents.get(), 0, 0, InspectorTimelineAgent::WorkerInspector, 0));
    m_agents.append(consoleAgent.release());

    m_injectedScriptManager->injectedScriptHost()->init(0
        , 0
#if ENABLE(SQL_DATABASE)
        , 0
#endif
        , 0
        , 0
    );
}
 
WorkerInspectorController::~WorkerInspectorController()
{
    m_instrumentingAgents->reset();
    disconnectFrontend();
}

void WorkerInspectorController::connectFrontend()
{
    ASSERT(!m_frontendChannel);
    m_frontendChannel = adoptPtr(new PageInspectorProxy(m_workerGlobalScope));
    m_backendDispatcher = InspectorBackendDispatcher::create(m_frontendChannel.get());
    m_agents.didCreateFrontendAndBackend(m_frontendChannel.get(), m_backendDispatcher.get());
}

void WorkerInspectorController::disconnectFrontend()
{
    if (!m_frontendChannel)
        return;
    m_agents.willDestroyFrontendAndBackend();
    m_backendDispatcher->clearFrontend();
    m_backendDispatcher.clear();
    m_frontendChannel.clear();
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

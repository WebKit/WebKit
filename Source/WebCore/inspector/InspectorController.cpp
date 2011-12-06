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
#include "InspectorController.h"

#if ENABLE(INSPECTOR)

#include "Frame.h"
#include "GraphicsContext.h"
#include "IdentifiersFactory.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorAgent.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorBaseAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFileSystemAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorMemoryAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorState.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "PageConsoleAgent.h"
#include "PageDebuggerAgent.h"
#include "PageRuntimeAgent.h"
#include "Page.h"
#include "ScriptObject.h"
#include "Settings.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

InspectorController::InspectorController(Page* page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(adoptPtr(new InstrumentingAgents()))
    , m_injectedScriptManager(InjectedScriptManager::createForPage())
    , m_state(adoptPtr(new InspectorState(inspectorClient)))
    , m_page(page)
    , m_inspectorClient(inspectorClient)
{
    OwnPtr<InspectorAgent> inspectorAgentPtr(InspectorAgent::create(page, m_injectedScriptManager.get(), m_instrumentingAgents.get(), m_state.get()));
    m_inspectorAgent = inspectorAgentPtr.get();
    m_agents.append(inspectorAgentPtr.release());

    OwnPtr<InspectorPageAgent> pageAgentPtr(InspectorPageAgent::create(m_instrumentingAgents.get(), page, m_state.get(), m_injectedScriptManager.get()));
    InspectorPageAgent* pageAgent = pageAgentPtr.get();
    m_agents.append(pageAgentPtr.release());

    OwnPtr<InspectorDOMAgent> domAgentPtr(InspectorDOMAgent::create(m_instrumentingAgents.get(), pageAgent, inspectorClient, m_state.get(), m_injectedScriptManager.get()));
    m_domAgent = domAgentPtr.get();
    m_agents.append(domAgentPtr.release());

    m_agents.append(InspectorCSSAgent::create(m_instrumentingAgents.get(), m_state.get(), m_domAgent));

#if ENABLE(SQL_DATABASE)
    OwnPtr<InspectorDatabaseAgent> databaseAgentPtr(InspectorDatabaseAgent::create(m_instrumentingAgents.get(), m_state.get()));
    InspectorDatabaseAgent* databaseAgent = databaseAgentPtr.get();
    m_agents.append(databaseAgentPtr.release());
#endif

#if ENABLE(FILE_SYSTEM)
    m_agents.append(InspectorFileSystemAgent::create(m_instrumentingAgents.get(), m_state.get()));
#endif
    OwnPtr<InspectorDOMStorageAgent> domStorageAgentPtr(InspectorDOMStorageAgent::create(m_instrumentingAgents.get(), m_state.get()));
    InspectorDOMStorageAgent* domStorageAgent = domStorageAgentPtr.get();
    m_agents.append(domStorageAgentPtr.release());

    m_agents.append(InspectorTimelineAgent::create(m_instrumentingAgents.get(), m_state.get()));
    m_agents.append(InspectorApplicationCacheAgent::create(m_instrumentingAgents.get(), m_state.get(), pageAgent));

    OwnPtr<InspectorResourceAgent> resourceAgentPtr(InspectorResourceAgent::create(m_instrumentingAgents.get(), pageAgent, inspectorClient, m_state.get()));
    m_resourceAgent = resourceAgentPtr.get();
    m_agents.append(resourceAgentPtr.release());

    OwnPtr<InspectorRuntimeAgent> runtimeAgentPtr(PageRuntimeAgent::create(m_instrumentingAgents.get(), m_state.get(), m_injectedScriptManager.get(), page, pageAgent));
    InspectorRuntimeAgent* runtimeAgent = runtimeAgentPtr.get();
    m_agents.append(runtimeAgentPtr.release());

    OwnPtr<InspectorConsoleAgent> consoleAgentPtr(PageConsoleAgent::create(m_instrumentingAgents.get(), m_inspectorAgent, m_state.get(), m_injectedScriptManager.get(), m_domAgent));
    InspectorConsoleAgent* consoleAgent = consoleAgentPtr.get();
    m_agents.append(consoleAgentPtr.release());

#if ENABLE(JAVASCRIPT_DEBUGGER)
    OwnPtr<InspectorDebuggerAgent> debuggerAgentPtr(PageDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), page, m_injectedScriptManager.get()));
    m_debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(debuggerAgentPtr.release());

    m_agents.append(InspectorDOMDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), m_domAgent, m_debuggerAgent, m_inspectorAgent));

    OwnPtr<InspectorProfilerAgent> profilerAgentPtr(InspectorProfilerAgent::create(m_instrumentingAgents.get(), consoleAgent, page, m_state.get(), m_injectedScriptManager.get()));
    m_profilerAgent = profilerAgentPtr.get();
    m_agents.append(profilerAgentPtr.release());
#endif

#if ENABLE(WORKERS)
    m_agents.append(InspectorWorkerAgent::create(m_instrumentingAgents.get(), m_state.get()));
#endif

    m_agents.append(InspectorMemoryAgent::create(m_instrumentingAgents.get(), m_state.get(), m_page));

    ASSERT_ARG(inspectorClient, inspectorClient);
    m_injectedScriptManager->injectedScriptHost()->init(m_inspectorAgent
        , consoleAgent
#if ENABLE(SQL_DATABASE)
        , databaseAgent
#endif
        , domStorageAgent
    );

#if ENABLE(JAVASCRIPT_DEBUGGER)
    runtimeAgent->setScriptDebugServer(&m_debuggerAgent->scriptDebugServer());
#endif
}

InspectorController::~InspectorController()
{
    for (Agents::iterator it = m_agents.begin(); it != m_agents.end(); ++it)
        (*it)->discardAgent();

    ASSERT(!m_inspectorClient);
}

void InspectorController::inspectedPageDestroyed()
{
    disconnectFrontend();
    m_injectedScriptManager->disconnect();
    m_inspectorClient->inspectorDestroyed();
    m_inspectorClient = 0;
    m_page = 0;
}

void InspectorController::setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient> inspectorFrontendClient)
{
    m_inspectorFrontendClient = inspectorFrontendClient;
}

bool InspectorController::hasInspectorFrontendClient() const
{
    return m_inspectorFrontendClient;
}

void InspectorController::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    // If the page is supposed to serve as InspectorFrontend notify inspector frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame == m_page->mainFrame())
        m_inspectorFrontendClient->windowObjectCleared();
}

void InspectorController::connectFrontend()
{
    m_inspectorFrontend = adoptPtr(new InspectorFrontend(m_inspectorClient));
    // We can reconnect to existing front-end -> unmute state.
    m_state->unmute();

    InspectorFrontend* frontend = m_inspectorFrontend.get();
    for (Agents::iterator it = m_agents.begin(); it != m_agents.end(); ++it)
        (*it)->setFrontend(frontend);

    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(true);
    InspectorInstrumentation::frontendCreated();

    ASSERT(m_inspectorClient);
    m_inspectorBackendDispatcher = adoptRef(new InspectorBackendDispatcher(m_inspectorClient));

    InspectorBackendDispatcher* dispatcher = m_inspectorBackendDispatcher.get();
    for (Agents::iterator it = m_agents.begin(); it != m_agents.end(); ++it)
        (*it)->registerInDispatcher(dispatcher);
}

void InspectorController::disconnectFrontend()
{
    if (!m_inspectorFrontend)
        return;
    m_inspectorBackendDispatcher->clearFrontend();
    m_inspectorBackendDispatcher.clear();

    // Destroying agents would change the state, but we don't want that.
    // Pre-disconnect state will be used to restore inspector agents.
    m_state->mute();

    for (Agents::iterator it = m_agents.begin(); it != m_agents.end(); ++it)
        (*it)->clearFrontend();

    m_inspectorFrontend.clear();

    InspectorInstrumentation::frontendDeleted();
    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(false);
}

void InspectorController::show()
{
    if (!enabled())
        return;

    if (m_inspectorFrontend)
        m_inspectorClient->bringFrontendToFront();
    else {
        m_inspectorClient->openInspectorFrontend(this);
        connectFrontend();
    }
}

void InspectorController::close()
{
    if (!m_inspectorFrontend)
        return;
    disconnectFrontend();
    m_inspectorClient->closeInspectorFrontend();
}

void InspectorController::restoreInspectorStateFromCookie(const String& inspectorStateCookie)
{
    ASSERT(!m_inspectorFrontend);
    connectFrontend();
    m_state->loadFromCookie(inspectorStateCookie);

    for (Agents::iterator it = m_agents.begin(); it != m_agents.end(); ++it)
        (*it)->restore();
    m_inspectorAgent->emitCommitLoadIfNeeded();
}

void InspectorController::setProcessId(long processId)
{
    IdentifiersFactory::setProcessId(processId);
}

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    m_inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void InspectorController::drawHighlight(GraphicsContext& context) const
{
    m_domAgent->drawHighlight(context);
}

void InspectorController::inspect(Node* node)
{
    if (!enabled())
        return;

    show();

    m_domAgent->inspect(node);
}

bool InspectorController::enabled() const
{
    return m_inspectorAgent->developerExtrasEnabled();
}

Page* InspectorController::inspectedPage() const
{
    return m_page;
}

void InspectorController::setInspectorExtensionAPI(const String& source)
{
    m_inspectorAgent->setInspectorExtensionAPI(source);
}

void InspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_inspectorBackendDispatcher)
        m_inspectorBackendDispatcher->dispatch(message);
}

void InspectorController::hideHighlight()
{
    ErrorString error;
    m_domAgent->hideHighlight(&error);
}

Node* InspectorController::highlightedNode() const
{
    return m_domAgent->highlightedNode();
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::enableProfiler()
{
    ErrorString error;
    m_profilerAgent->enable(&error);
}

void InspectorController::disableProfiler()
{
    ErrorString error;
    m_profilerAgent->disable(&error);
}

bool InspectorController::profilerEnabled()
{
    return m_profilerAgent->enabled();
}

void InspectorController::resume()
{
    if (m_debuggerAgent) {
        ErrorString error;
        m_debuggerAgent->resume(&error);
    }
}

#endif

void InspectorController::setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize)
{
    m_resourceAgent->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

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
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorAgent.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorConsoleAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFrontend.h"
#include "InspectorFrontendClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorPageAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorRuntimeAgent.h"
#include "InspectorState.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "PageDebuggerAgent.h"
#include "Page.h"
#include "ScriptObject.h"
#include "Settings.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

namespace {

class PageRuntimeAgent : public InspectorRuntimeAgent {
public:
    PageRuntimeAgent(InjectedScriptManager* injectedScriptManager, Page* page)
        : InspectorRuntimeAgent(injectedScriptManager)
        , m_inspectedPage(page) { }
    virtual ~PageRuntimeAgent() { }

private:
    virtual ScriptState* getDefaultInspectedState() { return mainWorldScriptState(m_inspectedPage->mainFrame()); }
    Page* m_inspectedPage;
};

}

InspectorController::InspectorController(Page* page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(adoptPtr(new InstrumentingAgents()))
    , m_injectedScriptManager(InjectedScriptManager::createForPage())
    , m_state(adoptPtr(new InspectorState(inspectorClient)))
    , m_inspectorAgent(adoptPtr(new InspectorAgent(page, m_injectedScriptManager.get(), m_instrumentingAgents.get())))
    , m_pageAgent(InspectorPageAgent::create(m_instrumentingAgents.get(), page, m_injectedScriptManager.get()))
    , m_domAgent(InspectorDOMAgent::create(m_instrumentingAgents.get(), m_pageAgent.get(), inspectorClient, m_state.get(), m_injectedScriptManager.get()))
    , m_cssAgent(adoptPtr(new InspectorCSSAgent(m_instrumentingAgents.get(), m_domAgent.get())))
#if ENABLE(DATABASE)
    , m_databaseAgent(InspectorDatabaseAgent::create(m_instrumentingAgents.get(), m_state.get()))
#endif
#if ENABLE(DOM_STORAGE)
    , m_domStorageAgent(InspectorDOMStorageAgent::create(m_instrumentingAgents.get(), m_state.get()))
#endif
    , m_timelineAgent(InspectorTimelineAgent::create(m_instrumentingAgents.get(), m_state.get()))
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    , m_applicationCacheAgent(adoptPtr(new InspectorApplicationCacheAgent(m_instrumentingAgents.get(), page)))
#endif
    , m_resourceAgent(InspectorResourceAgent::create(m_instrumentingAgents.get(), m_pageAgent.get(), m_state.get()))
    , m_runtimeAgent(adoptPtr(new PageRuntimeAgent(m_injectedScriptManager.get(), page)))
    , m_consoleAgent(adoptPtr(new InspectorConsoleAgent(m_instrumentingAgents.get(), m_inspectorAgent.get(), m_state.get(), m_injectedScriptManager.get(), m_domAgent.get())))
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerAgent(PageDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), page, m_injectedScriptManager.get()))
    , m_domDebuggerAgent(InspectorDOMDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), m_domAgent.get(), m_debuggerAgent.get(), m_inspectorAgent.get()))
    , m_profilerAgent(InspectorProfilerAgent::create(m_instrumentingAgents.get(), m_consoleAgent.get(), page, m_state.get()))
#endif
#if ENABLE(WORKERS)
    , m_workerAgent(InspectorWorkerAgent::create(m_instrumentingAgents.get()))
#endif
    , m_page(page)
    , m_inspectorClient(inspectorClient)
    , m_openingFrontend(false)
    , m_startUserInitiatedDebuggingWhenFrontedIsConnected(false)
{
    ASSERT_ARG(inspectorClient, inspectorClient);
    m_injectedScriptManager->injectedScriptHost()->init(m_inspectorAgent.get()
        , m_consoleAgent.get()
#if ENABLE(DATABASE)
        , m_databaseAgent.get()
#endif
#if ENABLE(DOM_STORAGE)
        , m_domStorageAgent.get()
#endif
    );
    InspectorInstrumentation::bindInstrumentingAgents(m_page, m_instrumentingAgents.get());
}

InspectorController::~InspectorController()
{
    ASSERT(!m_inspectorClient);
}

void InspectorController::inspectedPageDestroyed()
{
    InspectorInstrumentation::unbindInstrumentingAgents(m_page);
    disconnectFrontend();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_domDebuggerAgent.clear();
    m_debuggerAgent.clear();
#endif
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

void InspectorController::startTimelineProfiler()
{
    ErrorString error;
    m_timelineAgent->start(&error);
}

void InspectorController::stopTimelineProfiler()
{
    ErrorString error;
    m_timelineAgent->stop(&error);
}

void InspectorController::connectFrontend()
{
    m_openingFrontend = false;
    m_inspectorFrontend = adoptPtr(new InspectorFrontend(m_inspectorClient));
    m_injectedScriptManager->injectedScriptHost()->setFrontend(m_inspectorFrontend.get());
    // We can reconnect to existing front-end -> unmute state.
    m_state->unmute();

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent->setFrontend(m_inspectorFrontend.get());
#endif
    m_pageAgent->setFrontend(m_inspectorFrontend.get());
    m_domAgent->setFrontend(m_inspectorFrontend.get());
    m_consoleAgent->setFrontend(m_inspectorFrontend.get());
    m_timelineAgent->setFrontend(m_inspectorFrontend.get());
    m_resourceAgent->setFrontend(m_inspectorFrontend.get());
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->setFrontend(m_inspectorFrontend.get());
    m_profilerAgent->setFrontend(m_inspectorFrontend.get());
#endif
#if ENABLE(DATABASE)
    m_databaseAgent->setFrontend(m_inspectorFrontend.get());
#endif
#if ENABLE(DOM_STORAGE)
    m_domStorageAgent->setFrontend(m_inspectorFrontend.get());
#endif
#if ENABLE(WORKERS)
    m_workerAgent->setFrontend(m_inspectorFrontend.get());
#endif
    m_inspectorAgent->setFrontend(m_inspectorFrontend.get());

    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(true);
    InspectorInstrumentation::frontendCreated();

    ASSERT(m_inspectorClient);
    m_inspectorBackendDispatcher = adoptRef(new InspectorBackendDispatcher(
        m_inspectorClient,
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        m_applicationCacheAgent.get(),
#endif
        m_cssAgent.get(),
        m_consoleAgent.get(),
        m_domAgent.get(),
#if ENABLE(JAVASCRIPT_DEBUGGER)
        m_domDebuggerAgent.get(),
#endif
#if ENABLE(DOM_STORAGE)
        m_domStorageAgent.get(),
#endif
#if ENABLE(DATABASE)
        m_databaseAgent.get(),
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
        m_debuggerAgent.get(),
#endif
        m_resourceAgent.get(),
        m_pageAgent.get(),
#if ENABLE(JAVASCRIPT_DEBUGGER)
        m_profilerAgent.get(),
#endif
        m_runtimeAgent.get(),
        m_timelineAgent.get()
#if ENABLE(WORKERS)
        , m_workerAgent.get()
#endif
    ));

    if (m_startUserInitiatedDebuggingWhenFrontedIsConnected) {
        m_inspectorFrontend->inspector()->startUserInitiatedDebugging();
        m_startUserInitiatedDebuggingWhenFrontedIsConnected = false;
    }
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

    m_inspectorAgent->clearFrontend();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->clearFrontend();
    m_domDebuggerAgent->clearFrontend();
    m_profilerAgent->clearFrontend();
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    m_applicationCacheAgent->clearFrontend();
#endif
    m_consoleAgent->clearFrontend();
    m_domAgent->clearFrontend();
    m_timelineAgent->clearFrontend();
    m_resourceAgent->clearFrontend();
#if ENABLE(DATABASE)
    m_databaseAgent->clearFrontend();
#endif
#if ENABLE(DOM_STORAGE)
    m_domStorageAgent->clearFrontend();
#endif
    m_pageAgent->clearFrontend();
#if ENABLE(WORKERS)
    m_workerAgent->clearFrontend();
#endif
    m_injectedScriptManager->injectedScriptHost()->clearFrontend();

    m_inspectorFrontend.clear();

    InspectorInstrumentation::frontendDeleted();
    if (!InspectorInstrumentation::hasFrontends())
        ScriptController::setCaptureCallStackForUncaughtExceptions(false);
}

void InspectorController::show()
{
    if (!enabled())
        return;

    if (m_openingFrontend)
        return;

    if (m_inspectorFrontend)
        m_inspectorFrontend->inspector()->bringToFront();
    else {
        m_openingFrontend = true;
        m_inspectorClient->openInspectorFrontend(this);
    }
}

void InspectorController::close()
{
    if (!m_inspectorFrontend)
        return;
    m_inspectorFrontend->inspector()->disconnectFromBackend();
    disconnectFrontend();
}

void InspectorController::restoreInspectorStateFromCookie(const String& inspectorStateCookie)
{
    ASSERT(!m_inspectorFrontend);
    connectFrontend();
    m_state->loadFromCookie(inspectorStateCookie);

    m_domAgent->restore();
    m_resourceAgent->restore();
    m_timelineAgent->restore();
#if ENABLE(DATABASE)
    m_databaseAgent->restore();
#endif
#if ENABLE(DOM_STORAGE)
    m_domStorageAgent->restore();
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_debuggerAgent->restore();
    m_profilerAgent->restore();
#endif
    m_inspectorAgent->restore();
}

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    m_inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    m_domAgent->drawNodeHighlight(context);
}

void InspectorController::showConsole()
{
    if (!enabled())
        return;
    show();
    m_inspectorAgent->showConsole();
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
    return m_inspectorAgent->enabled();
}

Page* InspectorController::inspectedPage() const
{
    return m_page;
}

bool InspectorController::timelineProfilerEnabled()
{
    return m_timelineAgent->started();
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

bool InspectorController::debuggerEnabled()
{
    return m_debuggerAgent->enabled();
}

void InspectorController::showAndEnableDebugger()
{
    if (!enabled())
        return;
    show();

    if (m_inspectorFrontend)
        m_inspectorFrontend->inspector()->startUserInitiatedDebugging();
    else
        m_startUserInitiatedDebuggingWhenFrontedIsConnected = true;
}

void InspectorController::disableDebugger()
{
    ErrorString error;
    m_debuggerAgent->disable(&error);
}

void InspectorController::startUserInitiatedProfiling()
{
    m_profilerAgent->startUserInitiatedProfiling();
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;
    show();
    m_profilerAgent->stopUserInitiatedProfiling();
    m_inspectorAgent->showProfilesPanel();
}

bool InspectorController::isRecordingUserInitiatedProfile() const
{
    return m_profilerAgent->isRecordingUserInitiatedProfile();
}

void InspectorController::resume()
{
    if (m_debuggerAgent) {
        ErrorString error;
        m_debuggerAgent->resume(&error);
    }
}

#endif

void InspectorController::requestAttachWindow()
{
    if (!m_inspectorFrontendClient)
        return;

    m_inspectorFrontendClient->requestAttachWindow();
}
    
bool InspectorController::canAttachWindow()
{
    if (!m_inspectorFrontendClient)
        return false;
    
    return m_inspectorFrontendClient->canAttachWindow();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

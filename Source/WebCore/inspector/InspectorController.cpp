/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "CommandLineAPIHost.h"
#include "DOMWrapperWorld.h"
#include "GraphicsContext.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorFrontendClient.h"
#include "InspectorIndexedDBAgent.h"
#include "InspectorInstrumentation.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorNetworkAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorReplayAgent.h"
#include "InspectorTimelineAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowCustom.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageConsoleAgent.h"
#include "PageDebuggerAgent.h"
#include "PageRuntimeAgent.h"
#include "PageScriptDebugServer.h"
#include "Settings.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/InspectorBackendDispatcher.h>
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorFrontendRouter.h>
#include <inspector/agents/InspectorAgent.h>
#include <inspector/agents/InspectorHeapAgent.h>
#include <profiler/LegacyProfiler.h>
#include <runtime/JSLock.h>
#include <wtf/Stopwatch.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "PageDebuggable.h"
#endif

using namespace JSC;
using namespace Inspector;

namespace WebCore {

InspectorController::InspectorController(Page& page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(InstrumentingAgents::create(*this))
    , m_injectedScriptManager(std::make_unique<WebInjectedScriptManager>(*this, WebInjectedScriptHost::create()))
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_overlay(std::make_unique<InspectorOverlay>(page, inspectorClient))
    , m_executionStopwatch(Stopwatch::create())
    , m_scriptDebugServer(page)
    , m_page(page)
    , m_inspectorClient(inspectorClient)
{
    ASSERT_ARG(inspectorClient, inspectorClient);

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

    PageAgentContext pageContext = {
        webContext,
        m_page
    };

    auto inspectorAgentPtr = std::make_unique<InspectorAgent>(pageContext);
    m_inspectorAgent = inspectorAgentPtr.get();
    m_instrumentingAgents->setInspectorAgent(m_inspectorAgent);
    m_agents.append(WTF::move(inspectorAgentPtr));

    auto pageAgentPtr = std::make_unique<InspectorPageAgent>(pageContext, inspectorClient, m_overlay.get());
    InspectorPageAgent* pageAgent = pageAgentPtr.get();
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(WTF::move(pageAgentPtr));

    auto runtimeAgentPtr = std::make_unique<PageRuntimeAgent>(pageContext, pageAgent);
    PageRuntimeAgent* runtimeAgent = runtimeAgentPtr.get();
    m_instrumentingAgents->setPageRuntimeAgent(runtimeAgent);
    m_agents.append(WTF::move(runtimeAgentPtr));

    auto domAgentPtr = std::make_unique<InspectorDOMAgent>(pageContext, pageAgent, m_overlay.get());
    m_domAgent = domAgentPtr.get();
    m_agents.append(WTF::move(domAgentPtr));

    m_agents.append(std::make_unique<InspectorCSSAgent>(pageContext, m_domAgent));

    auto databaseAgentPtr = std::make_unique<InspectorDatabaseAgent>(pageContext);
    InspectorDatabaseAgent* databaseAgent = databaseAgentPtr.get();
    m_agents.append(WTF::move(databaseAgentPtr));

    m_agents.append(std::make_unique<InspectorNetworkAgent>(pageContext, pageAgent));

#if ENABLE(INDEXED_DATABASE)
    m_agents.append(std::make_unique<InspectorIndexedDBAgent>(pageContext, pageAgent));
#endif

#if ENABLE(WEB_REPLAY)
    m_agents.append(std::make_unique<InspectorReplayAgent>(pageContext));
#endif

    auto domStorageAgentPtr = std::make_unique<InspectorDOMStorageAgent>(pageContext, m_pageAgent);
    InspectorDOMStorageAgent* domStorageAgent = domStorageAgentPtr.get();
    m_agents.append(WTF::move(domStorageAgentPtr));

    auto timelineAgentPtr = std::make_unique<InspectorTimelineAgent>(pageContext, pageAgent);
    m_timelineAgent = timelineAgentPtr.get();
    m_agents.append(WTF::move(timelineAgentPtr));

    auto consoleAgentPtr = std::make_unique<PageConsoleAgent>(pageContext, m_domAgent);
    WebConsoleAgent* consoleAgent = consoleAgentPtr.get();
    m_instrumentingAgents->setWebConsoleAgent(consoleAgentPtr.get());
    m_agents.append(WTF::move(consoleAgentPtr));

    auto debuggerAgentPtr = std::make_unique<PageDebuggerAgent>(pageContext, pageAgent, m_overlay.get());
    PageDebuggerAgent* debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(WTF::move(debuggerAgentPtr));

    m_agents.append(std::make_unique<InspectorDOMDebuggerAgent>(pageContext, m_domAgent, debuggerAgent));
    m_agents.append(std::make_unique<InspectorHeapAgent>(pageContext));
    m_agents.append(std::make_unique<InspectorApplicationCacheAgent>(pageContext, pageAgent));
    m_agents.append(std::make_unique<InspectorLayerTreeAgent>(pageContext));

    ASSERT(m_injectedScriptManager->commandLineAPIHost());
    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost()) {
        commandLineAPIHost->init(m_inspectorAgent
            , consoleAgent
            , m_domAgent
            , domStorageAgent
            , databaseAgent
        );
    }
}

InspectorController::~InspectorController()
{
    m_instrumentingAgents->reset();
    ASSERT(!m_inspectorClient);
}

void InspectorController::inspectedPageDestroyed()
{
    m_injectedScriptManager->disconnect();

    // If the local frontend page was destroyed, close the window.
    if (m_inspectorFrontendClient)
        m_inspectorFrontendClient->closeWindow();

    // The frontend should call setInspectorFrontendClient(nullptr) under closeWindow().
    ASSERT(!m_inspectorFrontendClient);

    // Clean up resources and disconnect local and remote frontends.
    disconnectAllFrontends();
}

void InspectorController::setInspectorFrontendClient(InspectorFrontendClient* inspectorFrontendClient)
{
    m_inspectorFrontendClient = inspectorFrontendClient;
}

bool InspectorController::hasLocalFrontend() const
{
    return m_frontendRouter->hasLocalFrontend();
}

bool InspectorController::hasRemoteFrontend() const
{
    return m_frontendRouter->hasRemoteFrontend();
}

unsigned InspectorController::inspectionLevel() const
{
    return m_inspectorFrontendClient ? m_inspectorFrontendClient->inspectionLevel() : 0;
}

void InspectorController::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorld())
        return;

    if (frame.isMainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    // If the page is supposed to serve as InspectorFrontend notify inspector frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame.isMainFrame())
        m_inspectorFrontendClient->windowObjectCleared();
}

void InspectorController::connectFrontend(Inspector::FrontendChannel* frontendChannel, bool isAutomaticInspection)
{
    ASSERT_ARG(frontendChannel, frontendChannel);
    ASSERT(m_inspectorClient);

    bool connectedFirstFrontend = !m_frontendRouter->hasFrontends();
    m_isAutomaticInspection = isAutomaticInspection;

    m_frontendRouter->connectFrontend(frontendChannel);

    InspectorInstrumentation::frontendCreated();

    if (connectedFirstFrontend) {
        InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
        m_agents.didCreateFrontendAndBackend(&m_frontendRouter.get(), &m_backendDispatcher.get());
    }

#if ENABLE(REMOTE_INSPECTOR)
    if (!m_frontendRouter->hasRemoteFrontend())
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::disconnectFrontend(FrontendChannel* frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);
    m_isAutomaticInspection = false;

    InspectorInstrumentation::frontendDeleted();

    bool disconnectedLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectedLastFrontend) {
        // Notify agents first.
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);

        // Destroy the inspector overlay's page.
        m_overlay->freePage();

        // Unplug all instrumentations since they aren't needed now.
        InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    }

#if ENABLE(REMOTE_INSPECTOR)
    if (!m_frontendRouter->hasFrontends())
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::disconnectAllFrontends()
{
    // The local frontend client should be disconnected already.
    ASSERT(!m_inspectorFrontendClient);

    for (unsigned i = 0; i < m_frontendRouter->frontendCount(); ++i)
        InspectorInstrumentation::frontendDeleted();

    // Unplug all instrumentations to prevent further agent callbacks.
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());

    // Notify agents first, since they may need to use InspectorClient.
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Destroy the inspector overlay's page.
    m_overlay->freePage();

    // Disconnect local WK2 frontend and destroy the client.
    m_inspectorClient->inspectedPageDestroyed();
    m_inspectorClient = nullptr;

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();
    m_isAutomaticInspection = false;

#if ENABLE(REMOTE_INSPECTOR)
    m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::show()
{
    ASSERT(!m_frontendRouter->hasRemoteFrontend());

    if (!enabled())
        return;

    if (m_frontendRouter->hasLocalFrontend())
        m_inspectorClient->bringFrontendToFront();
    else if (Inspector::FrontendChannel* frontendChannel = m_inspectorClient->openLocalFrontend(this))
        connectFrontend(frontendChannel);
}

void InspectorController::setProcessId(long processId)
{
    IdentifiersFactory::setProcessId(processId);
}

void InspectorController::evaluateForTestInFrontend(const String& script)
{
    m_inspectorAgent->evaluateForTestInFrontend(script);
}

void InspectorController::drawHighlight(GraphicsContext& context) const
{
    m_overlay->paint(context);
}

void InspectorController::getHighlight(Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem) const
{
    m_overlay->getHighlight(highlight, coordinateSystem);
}

Ref<Inspector::Protocol::Array<Inspector::Protocol::OverlayTypes::NodeHighlightData>> InspectorController::buildObjectForHighlightedNodes() const
{
    return m_overlay->buildObjectForHighlightedNodes();
}

void InspectorController::inspect(Node* node)
{
    if (!enabled())
        return;

    if (!hasRemoteFrontend())
        show();

    m_domAgent->inspect(node);
}

bool InspectorController::enabled() const
{
    return developerExtrasEnabled();
}

Page& InspectorController::inspectedPage() const
{
    return m_page;
}

void InspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

void InspectorController::hideHighlight()
{
    ErrorString unused;
    m_domAgent->hideHighlight(unused);
}

Node* InspectorController::highlightedNode() const
{
    return m_overlay->highlightedNode();
}

void InspectorController::setIndicating(bool indicating)
{
#if !PLATFORM(IOS)
    m_overlay->setIndicating(indicating);
#else
    if (indicating)
        m_inspectorClient->showInspectorIndication();
    else
        m_inspectorClient->hideInspectorIndication();
#endif
}

bool InspectorController::profilerEnabled() const
{
    return m_instrumentingAgents->persistentInspectorTimelineAgent();
}

void InspectorController::setProfilerEnabled(bool enable)
{
    ErrorString unused;

    if (enable) {
        m_instrumentingAgents->setPersistentInspectorTimelineAgent(m_timelineAgent);
        m_timelineAgent->start(unused);
    } else {
        m_instrumentingAgents->setPersistentInspectorTimelineAgent(nullptr);
        m_timelineAgent->stop(unused);
    }
}

bool InspectorController::developerExtrasEnabled() const
{
    return m_page.settings().developerExtrasEnabled();
}

bool InspectorController::canAccessInspectedScriptState(JSC::ExecState* scriptState) const
{
    JSLockHolder lock(scriptState);
    JSDOMWindow* inspectedWindow = toJSDOMWindow(scriptState->lexicalGlobalObject());
    if (!inspectedWindow)
        return false;

    return BindingSecurity::shouldAllowAccessToDOMWindow(scriptState, inspectedWindow->wrapped(), DoNotReportSecurityError);
}

InspectorFunctionCallHandler InspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler InspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

void InspectorController::willCallInjectedScriptFunction(JSC::ExecState* scriptState, const String&, int)
{
    LegacyProfiler::profiler()->suspendProfiling(scriptState);
}

void InspectorController::didCallInjectedScriptFunction(JSC::ExecState* scriptState)
{
    LegacyProfiler::profiler()->unsuspendProfiling(scriptState);
}

void InspectorController::frontendInitialized()
{
#if ENABLE(REMOTE_INSPECTOR)
    if (m_isAutomaticInspection)
        m_page.inspectorDebuggable().unpauseForInitializedInspector();
#endif
}

Ref<Stopwatch> InspectorController::executionStopwatch()
{
    return m_executionStopwatch.copyRef();
}

PageScriptDebugServer& InspectorController::scriptDebugServer()
{
    return m_scriptDebugServer;
}

JSC::VM& InspectorController::vm()
{
    return JSDOMWindowBase::commonVM();
}

void InspectorController::didComposite(Frame& frame)
{
    InspectorInstrumentation::didComposite(frame);
}

} // namespace WebCore

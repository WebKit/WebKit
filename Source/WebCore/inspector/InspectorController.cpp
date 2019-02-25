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
#include "CommonVM.h"
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCPUProfilerAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorCanvasAgent.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorFrontendClient.h"
#include "InspectorIndexedDBAgent.h"
#include "InspectorInstrumentation.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorMemoryAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowCustom.h"
#include "JSExecState.h"
#include "Page.h"
#include "PageAuditAgent.h"
#include "PageConsoleAgent.h"
#include "PageDebuggerAgent.h"
#include "PageHeapAgent.h"
#include "PageNetworkAgent.h"
#include "PageRuntimeAgent.h"
#include "PageScriptDebugServer.h"
#include "Settings.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InspectorAgent.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorScriptProfilerAgent.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/Stopwatch.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "PageDebuggable.h"
#endif

namespace WebCore {

using namespace JSC;
using namespace Inspector;

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

    auto pageContext = pageAgentContext();

    auto inspectorAgentPtr = std::make_unique<InspectorAgent>(pageContext);
    m_inspectorAgent = inspectorAgentPtr.get();
    m_instrumentingAgents->setInspectorAgent(m_inspectorAgent);
    m_agents.append(WTFMove(inspectorAgentPtr));

    auto pageAgentPtr = std::make_unique<InspectorPageAgent>(pageContext, inspectorClient, m_overlay.get());
    InspectorPageAgent* pageAgent = pageAgentPtr.get();
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(WTFMove(pageAgentPtr));

    auto runtimeAgent = std::make_unique<PageRuntimeAgent>(pageContext, pageAgent);
    m_instrumentingAgents->setPageRuntimeAgent(runtimeAgent.get());
    m_agents.append(WTFMove(runtimeAgent));

    auto domAgentPtr = std::make_unique<InspectorDOMAgent>(pageContext, pageAgent, m_overlay.get());
    m_domAgent = domAgentPtr.get();
    m_agents.append(WTFMove(domAgentPtr));

    auto databaseAgentPtr = std::make_unique<InspectorDatabaseAgent>(pageContext);
    InspectorDatabaseAgent* databaseAgent = databaseAgentPtr.get();
    m_agents.append(WTFMove(databaseAgentPtr));

    auto domStorageAgentPtr = std::make_unique<InspectorDOMStorageAgent>(pageContext, m_pageAgent);
    InspectorDOMStorageAgent* domStorageAgent = domStorageAgentPtr.get();
    m_agents.append(WTFMove(domStorageAgentPtr));

    auto heapAgentPtr = std::make_unique<PageHeapAgent>(pageContext);
    InspectorHeapAgent* heapAgent = heapAgentPtr.get();
    m_agents.append(WTFMove(heapAgentPtr));

    auto scriptProfilerAgentPtr = std::make_unique<InspectorScriptProfilerAgent>(pageContext);
    InspectorScriptProfilerAgent* scriptProfilerAgent = scriptProfilerAgentPtr.get();
    m_agents.append(WTFMove(scriptProfilerAgentPtr));

    auto consoleAgentPtr = std::make_unique<PageConsoleAgent>(pageContext, heapAgent, m_domAgent);
    WebConsoleAgent* consoleAgent = consoleAgentPtr.get();
    m_instrumentingAgents->setWebConsoleAgent(consoleAgentPtr.get());
    m_agents.append(WTFMove(consoleAgentPtr));

    m_agents.append(std::make_unique<InspectorTimelineAgent>(pageContext, scriptProfilerAgent, heapAgent, pageAgent));

    auto canvasAgentPtr = std::make_unique<InspectorCanvasAgent>(pageContext);
    m_instrumentingAgents->setInspectorCanvasAgent(canvasAgentPtr.get());
    m_agents.append(WTFMove(canvasAgentPtr));

    ASSERT(m_injectedScriptManager->commandLineAPIHost());
    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost())
        commandLineAPIHost->init(m_inspectorAgent, consoleAgent, m_domAgent, domStorageAgent, databaseAgent);
}

InspectorController::~InspectorController()
{
    m_instrumentingAgents->reset();
    ASSERT(!m_inspectorClient);
}

PageAgentContext InspectorController::pageAgentContext()
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

    PageAgentContext pageContext = {
        webContext,
        m_page
    };

    return pageContext;
}

void InspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;

    auto pageContext = pageAgentContext();

    auto debuggerAgent = std::make_unique<PageDebuggerAgent>(pageContext, m_pageAgent);
    auto debuggerAgentPtr = debuggerAgent.get();

    m_agents.append(WTFMove(debuggerAgent));
    m_agents.append(std::make_unique<PageNetworkAgent>(pageContext, m_pageAgent));
    m_agents.append(std::make_unique<InspectorCSSAgent>(pageContext, m_domAgent));
    m_agents.append(std::make_unique<InspectorDOMDebuggerAgent>(pageContext, m_domAgent, debuggerAgentPtr));
    m_agents.append(std::make_unique<InspectorApplicationCacheAgent>(pageContext, m_pageAgent));
    m_agents.append(std::make_unique<InspectorLayerTreeAgent>(pageContext));
    m_agents.append(std::make_unique<InspectorWorkerAgent>(pageContext));
#if ENABLE(INDEXED_DATABASE)
    m_agents.append(std::make_unique<InspectorIndexedDBAgent>(pageContext, m_pageAgent));
#endif
#if ENABLE(RESOURCE_USAGE)
    m_agents.append(std::make_unique<InspectorCPUProfilerAgent>(pageContext));
    m_agents.append(std::make_unique<InspectorMemoryAgent>(pageContext));
#endif
    m_agents.append(std::make_unique<PageAuditAgent>(pageContext));
}

void InspectorController::inspectedPageDestroyed()
{
    // Clean up resources and disconnect local and remote frontends.
    disconnectAllFrontends();

    // Disconnect the client.
    m_inspectorClient->inspectedPageDestroyed();
    m_inspectorClient = nullptr;

    m_agents.discardValues();
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

void InspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel, bool isAutomaticInspection, bool immediatelyPause)
{
    ASSERT(m_inspectorClient);

    // If a frontend has connected enable the developer extras and keep them enabled.
    m_page.settings().setDeveloperExtrasEnabled(true);

    createLazyAgents();

    bool connectedFirstFrontend = !m_frontendRouter->hasFrontends();
    m_isAutomaticInspection = isAutomaticInspection;
    m_pauseAfterInitialization = immediatelyPause;

    m_frontendRouter->connectFrontend(frontendChannel);

    InspectorInstrumentation::frontendCreated();

    if (connectedFirstFrontend) {
        InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
        m_agents.didCreateFrontendAndBackend(&m_frontendRouter.get(), &m_backendDispatcher.get());
    }

    m_inspectorClient->frontendCountChanged(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (hasLocalFrontend())
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::disconnectFrontend(FrontendChannel& frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);

    m_isAutomaticInspection = false;
    m_pauseAfterInitialization = false;

    InspectorInstrumentation::frontendDeleted();

    bool disconnectedLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectedLastFrontend) {
        // Notify agents first.
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);

        // Clean up inspector resources.
        m_injectedScriptManager->discardInjectedScripts();

        // Unplug all instrumentations since they aren't needed now.
        InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    }

    m_inspectorClient->frontendCountChanged(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (disconnectedLastFrontend)
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::disconnectAllFrontends()
{
    // If the local frontend page was destroyed, close the window.
    if (m_inspectorFrontendClient)
        m_inspectorFrontendClient->closeWindow();

    // The frontend should call setInspectorFrontendClient(nullptr) under closeWindow().
    ASSERT(!m_inspectorFrontendClient);

    if (!m_frontendRouter->hasFrontends())
        return;

    for (unsigned i = 0; i < m_frontendRouter->frontendCount(); ++i)
        InspectorInstrumentation::frontendDeleted();

    // Unplug all instrumentations to prevent further agent callbacks.
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());

    // Notify agents first, since they may need to use InspectorClient.
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Clean up inspector resources.
    m_injectedScriptManager->disconnect();

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();
    m_isAutomaticInspection = false;
    m_pauseAfterInitialization = false;

    m_inspectorClient->frontendCountChanged(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::show()
{
    ASSERT(!hasRemoteFrontend());

    if (!enabled())
        return;

    if (m_frontendRouter->hasLocalFrontend())
        m_inspectorClient->bringFrontendToFront();
    else if (Inspector::FrontendChannel* frontendChannel = m_inspectorClient->openLocalFrontend(this))
        connectFrontend(*frontendChannel);
}

void InspectorController::setIsUnderTest(bool value)
{
    if (value == m_isUnderTest)
        return;

    m_isUnderTest = value;

    // <rdar://problem/26768628> Try to catch suspicious scenarios where we may have a dangling frontend while running tests.
    RELEASE_ASSERT(!m_isUnderTest || !m_frontendRouter->hasFrontends());
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
#if !PLATFORM(IOS_FAMILY)
    m_overlay->setIndicating(indicating);
#else
    if (indicating)
        m_inspectorClient->showInspectorIndication();
    else
        m_inspectorClient->hideInspectorIndication();
#endif
}

bool InspectorController::developerExtrasEnabled() const
{
    return m_page.settings().developerExtrasEnabled();
}

bool InspectorController::canAccessInspectedScriptState(JSC::ExecState* scriptState) const
{
    JSLockHolder lock(scriptState);

    JSDOMWindow* inspectedWindow = toJSDOMWindow(scriptState->vm(), scriptState->lexicalGlobalObject());
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

void InspectorController::frontendInitialized()
{
    if (m_pauseAfterInitialization) {
        m_pauseAfterInitialization = false;
        if (PageDebuggerAgent* debuggerAgent = m_instrumentingAgents->pageDebuggerAgent()) {
            ErrorString ignored;
            debuggerAgent->pause(ignored);
        }
    }

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
    return commonVM();
}

void InspectorController::didComposite(Frame& frame)
{
    InspectorInstrumentation::didComposite(frame);
}

} // namespace WebCore

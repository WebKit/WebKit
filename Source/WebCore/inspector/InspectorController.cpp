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

#include "CommandLineAPIHost.h"
#include "DOMWrapperWorld.h"
#include "GraphicsContext.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorCanvasAgent.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorFrontendClient.h"
#include "InspectorHeapProfilerAgent.h"
#include "InspectorIndexedDBAgent.h"
#include "InspectorInputAgent.h"
#include "InspectorInstrumentation.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorMemoryAgent.h"
#include "InspectorOverlay.h"
#include "InspectorPageAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowCustom.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageConsoleAgent.h"
#include "PageDebuggerAgent.h"
#include "PageRuntimeAgent.h"
#include "Settings.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/InspectorBackendDispatcher.h>
#include <inspector/agents/InspectorAgent.h>
#include <runtime/JSLock.h>

using namespace JSC;
using namespace Inspector;

namespace WebCore {

InspectorController::InspectorController(Page& page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(InstrumentingAgents::create(*this))
    , m_injectedScriptManager(std::make_unique<WebInjectedScriptManager>(*this, WebInjectedScriptHost::create()))
    , m_overlay(std::make_unique<InspectorOverlay>(page, inspectorClient))
    , m_inspectorFrontendChannel(nullptr)
    , m_page(page)
    , m_inspectorClient(inspectorClient)
    , m_isUnderTest(false)
#if ENABLE(REMOTE_INSPECTOR)
    , m_hasRemoteFrontend(false)
#endif
{
    ASSERT_ARG(inspectorClient, inspectorClient);

    auto inspectorAgentPtr = std::make_unique<InspectorAgent>();
    m_inspectorAgent = inspectorAgentPtr.get();
    m_instrumentingAgents->setInspectorAgent(m_inspectorAgent);
    m_agents.append(std::move(inspectorAgentPtr));

    auto pageAgentPtr = std::make_unique<InspectorPageAgent>(m_instrumentingAgents.get(), &page, inspectorClient, m_overlay.get());
    InspectorPageAgent* pageAgent = pageAgentPtr.get();
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(std::move(pageAgentPtr));

    auto runtimeAgentPtr = std::make_unique<PageRuntimeAgent>(m_injectedScriptManager.get(), &page, pageAgent);
    PageRuntimeAgent* runtimeAgent = runtimeAgentPtr.get();
    m_instrumentingAgents->setPageRuntimeAgent(runtimeAgent);
    m_agents.append(std::move(runtimeAgentPtr));

    auto domAgentPtr = std::make_unique<InspectorDOMAgent>(m_instrumentingAgents.get(), pageAgent, m_injectedScriptManager.get(), m_overlay.get());
    m_domAgent = domAgentPtr.get();
    m_agents.append(std::move(domAgentPtr));

    m_agents.append(std::make_unique<InspectorCSSAgent>(m_instrumentingAgents.get(), m_domAgent));

#if ENABLE(SQL_DATABASE)
    auto databaseAgentPtr = std::make_unique<InspectorDatabaseAgent>(m_instrumentingAgents.get());
    InspectorDatabaseAgent* databaseAgent = databaseAgentPtr.get();
    m_agents.append(std::move(databaseAgentPtr));
#endif

#if ENABLE(INDEXED_DATABASE)
    m_agents.append(std::make_unique<InspectorIndexedDBAgent>(m_instrumentingAgents.get(), m_injectedScriptManager.get(), pageAgent));
#endif

    auto domStorageAgentPtr = std::make_unique<InspectorDOMStorageAgent>(m_instrumentingAgents.get(), m_pageAgent);
    InspectorDOMStorageAgent* domStorageAgent = domStorageAgentPtr.get();
    m_agents.append(std::move(domStorageAgentPtr));

    auto memoryAgentPtr = std::make_unique<InspectorMemoryAgent>(m_instrumentingAgents.get());
    m_memoryAgent = memoryAgentPtr.get();
    m_agents.append(std::move(memoryAgentPtr));

    m_agents.append(std::make_unique<InspectorTimelineAgent>(m_instrumentingAgents.get(), pageAgent, m_memoryAgent, InspectorTimelineAgent::PageInspector, inspectorClient));
    m_agents.append(std::make_unique<InspectorApplicationCacheAgent>(m_instrumentingAgents.get(), pageAgent));

    auto resourceAgentPtr = std::make_unique<InspectorResourceAgent>(m_instrumentingAgents.get(), pageAgent, inspectorClient);
    m_resourceAgent = resourceAgentPtr.get();
    m_agents.append(std::move(resourceAgentPtr));

    auto consoleAgentPtr = std::make_unique<PageConsoleAgent>(m_injectedScriptManager.get(), m_domAgent);
    WebConsoleAgent* consoleAgent = consoleAgentPtr.get();
    m_instrumentingAgents->setWebConsoleAgent(consoleAgentPtr.get());
    m_agents.append(std::move(consoleAgentPtr));

    auto debuggerAgentPtr = std::make_unique<PageDebuggerAgent>(m_injectedScriptManager.get(), m_instrumentingAgents.get(), pageAgent, m_overlay.get());
    m_debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(std::move(debuggerAgentPtr));

    auto domDebuggerAgentPtr = std::make_unique<InspectorDOMDebuggerAgent>(m_instrumentingAgents.get(), m_domAgent, m_debuggerAgent);
    m_domDebuggerAgent = domDebuggerAgentPtr.get();
    m_agents.append(std::move(domDebuggerAgentPtr));

    auto profilerAgentPtr = InspectorProfilerAgent::create(m_instrumentingAgents.get(), consoleAgent, &page, m_injectedScriptManager.get());
    m_profilerAgent = profilerAgentPtr.get();
    m_agents.append(std::move(profilerAgentPtr));

    m_agents.append(std::make_unique<InspectorHeapProfilerAgent>(m_instrumentingAgents.get(), m_injectedScriptManager.get()));

    m_agents.append(std::make_unique<InspectorWorkerAgent>(m_instrumentingAgents.get()));

    m_agents.append(std::make_unique<InspectorCanvasAgent>(m_instrumentingAgents.get(), pageAgent, m_injectedScriptManager.get()));

    m_agents.append(std::make_unique<InspectorInputAgent>(m_instrumentingAgents.get(), &page));

    m_agents.append(std::make_unique<InspectorLayerTreeAgent>(m_instrumentingAgents.get()));

    ASSERT(m_injectedScriptManager->commandLineAPIHost());
    if (CommandLineAPIHost* commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost()) {
        commandLineAPIHost->init(m_inspectorAgent
            , consoleAgent
            , m_domAgent
            , domStorageAgent
#if ENABLE(SQL_DATABASE)
            , databaseAgent
#endif
        );
    }

    runtimeAgent->setScriptDebugServer(&m_debuggerAgent->scriptDebugServer());
}

InspectorController::~InspectorController()
{
    m_instrumentingAgents->reset();
    m_agents.discardAgents();
    ASSERT(!m_inspectorClient);
}

void InspectorController::inspectedPageDestroyed()
{
    disconnectFrontend(InspectorDisconnectReason::InspectedTargetDestroyed);
    m_injectedScriptManager->disconnect();
    m_inspectorClient->inspectorDestroyed();
    m_inspectorClient = nullptr;
}

void InspectorController::setInspectorFrontendClient(std::unique_ptr<InspectorFrontendClient> inspectorFrontendClient)
{
    m_inspectorFrontendClient = std::move(inspectorFrontendClient);
}

bool InspectorController::hasLocalFrontend() const
{
#if ENABLE(REMOTE_INSPECTOR)
    return hasFrontend() && !m_hasRemoteFrontend;
#else
    return hasFrontend();
#endif
}

bool InspectorController::hasRemoteFrontend() const
{
#if ENABLE(REMOTE_INSPECTOR)
    return m_hasRemoteFrontend;
#else
    return false;
#endif
}

bool InspectorController::hasInspectorFrontendClient() const
{
    return m_inspectorFrontendClient.get();
}

void InspectorController::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorld())
        return;

    if (frame->isMainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    // If the page is supposed to serve as InspectorFrontend notify inspector frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame->isMainFrame())
        m_inspectorFrontendClient->windowObjectCleared();
}

void InspectorController::connectFrontend(InspectorFrontendChannel* frontendChannel)
{
    ASSERT(frontendChannel);
    ASSERT(m_inspectorClient);
    ASSERT(!m_inspectorFrontendChannel);
    ASSERT(!m_inspectorBackendDispatcher);

    m_inspectorFrontendChannel = frontendChannel;
    m_inspectorBackendDispatcher = InspectorBackendDispatcher::create(frontendChannel);

    m_agents.didCreateFrontendAndBackend(frontendChannel, m_inspectorBackendDispatcher.get());

    InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
    InspectorInstrumentation::frontendCreated();

#if ENABLE(REMOTE_INSPECTOR)
    if (!m_hasRemoteFrontend)
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::disconnectFrontend(InspectorDisconnectReason reason)
{
    if (!m_inspectorFrontendChannel)
        return;

    m_agents.willDestroyFrontendAndBackend(reason);

    m_inspectorBackendDispatcher->clearFrontend();
    m_inspectorBackendDispatcher.clear();
    m_inspectorFrontendChannel = nullptr;

    // Release overlay page resources.
    m_overlay->freePage();
    InspectorInstrumentation::frontendDeleted();
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());

#if ENABLE(REMOTE_INSPECTOR)
    if (!m_hasRemoteFrontend)
        m_page.remoteInspectorInformationDidChange();
#endif
}

void InspectorController::show()
{
    ASSERT(!hasRemoteFrontend());

    if (!enabled())
        return;

    if (m_inspectorFrontendChannel)
        m_inspectorClient->bringFrontendToFront();
    else {
        InspectorFrontendChannel* frontendChannel = m_inspectorClient->openInspectorFrontend(this);
        if (frontendChannel)
            connectFrontend(frontendChannel);
    }
}

void InspectorController::close()
{
    if (!m_inspectorFrontendChannel)
        return;
    disconnectFrontend(InspectorDisconnectReason::InspectorDestroyed);
    m_inspectorClient->closeInspectorFrontend();
}

void InspectorController::setProcessId(long processId)
{
    IdentifiersFactory::setProcessId(processId);
}

bool InspectorController::isUnderTest()
{
    return m_isUnderTest;
}

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    m_isUnderTest = true;
    m_inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void InspectorController::drawHighlight(GraphicsContext& context) const
{
    m_overlay->paint(context);
}

void InspectorController::getHighlight(Highlight* highlight) const
{
    m_overlay->getHighlight(highlight);
}

PassRefPtr<InspectorObject> InspectorController::buildObjectForHighlightedNode() const
{
    return m_overlay->buildObjectForHighlightedNode();
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
    return m_overlay->highlightedNode();
}

void InspectorController::setIndicating(bool indicating)
{
    // FIXME: For non-iOS clients, we should have InspectorOverlay do something here.
    if (indicating)
        m_inspectorClient->indicate();
    else
        m_inspectorClient->hideIndication();
}

bool InspectorController::profilerEnabled() const
{
    return m_profilerAgent->enabled();
}

void InspectorController::setProfilerEnabled(bool enable)
{
    ErrorString error;
    if (enable)
        m_profilerAgent->enable(&error);
    else
        m_profilerAgent->disable(&error);
}

void InspectorController::resume()
{
    if (m_debuggerAgent) {
        ErrorString error;
        m_debuggerAgent->resume(&error);
    }
}

void InspectorController::setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize)
{
    m_resourceAgent->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

void InspectorController::didBeginFrame()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didBeginFrame();
    if (InspectorCanvasAgent* canvasAgent = m_instrumentingAgents->inspectorCanvasAgent())
        canvasAgent->didBeginFrame();
}

void InspectorController::didCancelFrame()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didCancelFrame();
}

void InspectorController::willComposite()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->willComposite();
}

void InspectorController::didComposite()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didComposite();
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

    return BindingSecurity::shouldAllowAccessToDOMWindow(scriptState, inspectedWindow->impl(), DoNotReportSecurityError);
}

InspectorFunctionCallHandler InspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler InspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

void InspectorController::willCallInjectedScriptFunction(JSC::ExecState* scriptState, const String& scriptName, int scriptLine)
{
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(scriptState);
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willCallFunction(scriptExecutionContext, scriptName, scriptLine);
    m_injectedScriptInstrumentationCookies.append(cookie);
}

void InspectorController::didCallInjectedScriptFunction(JSC::ExecState* scriptState)
{
    ASSERT(!m_injectedScriptInstrumentationCookies.isEmpty());
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromExecState(scriptState);
    InspectorInstrumentationCookie cookie = m_injectedScriptInstrumentationCookies.takeLast();
    InspectorInstrumentation::didCallFunction(cookie, scriptExecutionContext);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

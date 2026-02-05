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
#include "PageInspectorController.h"

#include "CommandLineAPIHost.h"
#include "CommonVM.h"
#include "DOMWrapperWorld.h"
#include "EventTargetInlines.h"
#include "GraphicsContext.h"
#include "InspectorAnimationAgent.h"
#include "InspectorBackendClient.h"
#include "InspectorCPUProfilerAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorFrontendClient.h"
#include "InspectorIndexedDBAgent.h"
#include "InspectorInstrumentation.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorMemoryAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMWindow.h"
#include "JSExecState.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageAuditAgent.h"
#include "PageCanvasAgent.h"
#include "PageConsoleAgent.h"
#include "PageDOMDebuggerAgent.h"
#include "PageDebugger.h"
#include "PageDebuggerAgent.h"
#include "PageHeapAgent.h"
#include "PageNetworkAgent.h"
#include "PageRuntimeAgent.h"
#include "PageTimelineAgent.h"
#include "PageWorkerAgent.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InspectorAgent.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorScriptProfilerAgent.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageInspectorController);

PageInspectorController::PageInspectorController(Page& page, std::unique_ptr<InspectorBackendClient>&& inspectorBackendClient)
    : m_page(page)
    , m_instrumentingAgents(InstrumentingAgents::create(*this))
    , m_injectedScriptManager(WebInjectedScriptManager::create(*this, WebInjectedScriptHost::create()))
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_overlay(makeUniqueRefWithoutRefCountedCheck<InspectorOverlay>(*this, inspectorBackendClient.get()))
    , m_executionStopwatch(Stopwatch::create())
    , m_inspectorBackendClient(WTF::move(inspectorBackendClient))
{
    ASSERT_ARG(inspectorBackendClient, m_inspectorBackendClient);

    auto pageContext = pageAgentContext();

    auto consoleAgent = makeUniqueRef<PageConsoleAgent>(pageContext);
    m_instrumentingAgents->setWebConsoleAgent(consoleAgent.ptr());
    m_agents.append(WTF::move(consoleAgent));
}

PageInspectorController::~PageInspectorController()
{
    m_instrumentingAgents->reset();
    ASSERT(!m_inspectorBackendClient);
}

void PageInspectorController::ref() const
{
    m_page->ref();
}

void PageInspectorController::deref() const
{
    m_page->deref();
}

PageAgentContext PageInspectorController::pageAgentContext()
{
    AgentContext baseContext = {
        *this,
        m_injectedScriptManager.get(),
        m_frontendRouter.get(),
        m_backendDispatcher
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

void PageInspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;

    m_debugger = makeUnique<PageDebugger>(m_page);

    auto pageContext = pageAgentContext();

    ensureInspectorAgent();
    ensurePageAgent();

    m_agents.append(makeUniqueRef<PageRuntimeAgent>(pageContext));

    auto debuggerAgent = makeUniqueRef<PageDebuggerAgent>(pageContext);
    auto debuggerAgentPtr = debuggerAgent.ptr();
    m_agents.append(WTF::move(debuggerAgent));

    m_agents.append(makeUniqueRef<PageNetworkAgent>(pageContext, m_inspectorBackendClient.get()));
    m_agents.append(makeUniqueRef<InspectorCSSAgent>(pageContext));
    ensureDOMAgent();
    m_agents.append(makeUniqueRef<PageDOMDebuggerAgent>(pageContext, debuggerAgentPtr));
    m_agents.append(makeUniqueRef<InspectorLayerTreeAgent>(pageContext));
    m_agents.append(makeUniqueRef<PageWorkerAgent>(pageContext));
    m_agents.append(makeUniqueRef<InspectorDOMStorageAgent>(pageContext));
    m_agents.append(makeUniqueRef<InspectorIndexedDBAgent>(pageContext));

    auto scriptProfilerAgent = makeUniqueRef<InspectorScriptProfilerAgent>(pageContext);
    m_instrumentingAgents->setPersistentScriptProfilerAgent(scriptProfilerAgent.ptr());
    m_agents.append(WTF::move(scriptProfilerAgent));

#if ENABLE(RESOURCE_USAGE)
    m_agents.append(makeUniqueRef<InspectorCPUProfilerAgent>(pageContext));
    m_agents.append(makeUniqueRef<InspectorMemoryAgent>(pageContext));
#endif
    m_agents.append(makeUniqueRef<PageHeapAgent>(pageContext));
    m_agents.append(makeUniqueRef<PageAuditAgent>(pageContext));
    m_agents.append(makeUniqueRef<PageCanvasAgent>(pageContext));
    m_agents.append(makeUniqueRef<PageTimelineAgent>(pageContext));
    m_agents.append(makeUniqueRef<InspectorAnimationAgent>(pageContext));
}

void PageInspectorController::inspectedPageDestroyed()
{
    // Clean up resources and disconnect local and remote frontends.
    disconnectAllFrontends();

    // Disconnect the client.
    m_inspectorBackendClient->inspectedPageDestroyed();
    m_inspectorBackendClient = nullptr;

    m_agents.discardValues();

    m_debugger = nullptr;
}

void PageInspectorController::setInspectorFrontendClient(InspectorFrontendClient* inspectorFrontendClient)
{
    m_inspectorFrontendClient = inspectorFrontendClient;
}

bool PageInspectorController::hasLocalFrontend() const
{
    return m_frontendRouter->hasLocalFrontend();
}

bool PageInspectorController::hasRemoteFrontend() const
{
    return m_frontendRouter->hasRemoteFrontend();
}

unsigned PageInspectorController::inspectionLevel() const
{
    return m_inspectorFrontendClient ? m_inspectorFrontendClient->inspectionLevel() : 0;
}

void PageInspectorController::didClearWindowObjectInWorld(LocalFrame& frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorldSingleton())
        return;

    if (frame.isMainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    // If the page is supposed to serve as InspectorFrontend notify inspector frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame.isMainFrame())
        m_inspectorFrontendClient->windowObjectCleared();
}

void PageInspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel, bool isAutomaticInspection, bool immediatelyPause)
{
    ASSERT(m_inspectorBackendClient);
    Ref page = m_page.get();

    // If a frontend has connected enable the developer extras and keep them enabled.
    page->settings().setDeveloperExtrasEnabled(true);

    createLazyAgents();

    bool connectedFirstFrontend = !m_frontendRouter->hasFrontends();
    m_isAutomaticInspection = isAutomaticInspection;
    m_pauseAfterInitialization = immediatelyPause;

    m_frontendRouter->connectFrontend(frontendChannel);

    InspectorInstrumentation::frontendCreated();

    if (connectedFirstFrontend) {
        InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
        m_injectedScriptManager->addClient();
        m_agents.didCreateFrontendAndBackend();
    }

    m_inspectorBackendClient->frontendCountChanged(m_frontendRouter->frontendCount());
}

void PageInspectorController::disconnectFrontend(FrontendChannel& frontendChannel)
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
        m_injectedScriptManager->removeClient();

        // Unplug all instrumentations since they aren't needed now.
        InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    }

    m_inspectorBackendClient->frontendCountChanged(m_frontendRouter->frontendCount());
}

void PageInspectorController::disconnectAllFrontends()
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

    // Notify agents first, since they may need to use InspectorBackendClient.
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Clean up inspector resources.
    m_injectedScriptManager->removeClient();

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();
    m_isAutomaticInspection = false;
    m_pauseAfterInitialization = false;

    m_inspectorBackendClient->frontendCountChanged(m_frontendRouter->frontendCount());
}

void PageInspectorController::show()
{
    ASSERT(!hasRemoteFrontend());

    if (!enabled())
        return;

    if (m_frontendRouter->hasLocalFrontend())
        m_inspectorBackendClient->bringFrontendToFront();
    else if (Inspector::FrontendChannel* frontendChannel = m_inspectorBackendClient->openLocalFrontend(this))
        connectFrontend(*frontendChannel);
}

void PageInspectorController::evaluateForTestInFrontend(const String& script)
{
    CheckedRef { ensureInspectorAgent() }->evaluateForTestInFrontend(script);
}

void PageInspectorController::drawHighlight(GraphicsContext& context) const
{
    m_overlay->paint(context);
}

void PageInspectorController::getHighlight(InspectorOverlay::Highlight& highlight, InspectorOverlay::CoordinateSystem coordinateSystem) const
{
    m_overlay->getHighlight(highlight, coordinateSystem);
}

unsigned PageInspectorController::gridOverlayCount() const
{
    return m_overlay->gridOverlayCount();
}

unsigned PageInspectorController::flexOverlayCount() const
{
    return m_overlay->flexOverlayCount();
}

unsigned PageInspectorController::paintRectCount() const
{
    if (m_inspectorBackendClient->overridesShowPaintRects())
        return m_inspectorBackendClient->paintRectCount();

    return m_overlay->paintRectCount();
}

bool PageInspectorController::shouldShowOverlay() const
{
    return m_overlay->shouldShowOverlay();
}

void PageInspectorController::inspect(Node* node)
{
    if (!enabled())
        return;

    if (!hasRemoteFrontend())
        show();

    CheckedRef { ensureDOMAgent() }->inspect(node);
}

bool PageInspectorController::enabled() const
{
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    return developerExtrasEnabled();
}

Page& PageInspectorController::inspectedPage() const
{
    return m_page;
}

Ref<Page> PageInspectorController::protectedInspectedPage() const
{
    return inspectedPage();
}

void PageInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

void PageInspectorController::hideHighlight()
{
    m_overlay->hideHighlight();
}

Node* PageInspectorController::highlightedNode() const
{
    return m_overlay->highlightedNode();
}

void PageInspectorController::setIndicating(bool indicating)
{
#if !PLATFORM(IOS_FAMILY)
    m_overlay->setIndicating(indicating);
#else
    if (indicating)
        m_inspectorBackendClient->showInspectorIndication();
    else
        m_inspectorBackendClient->hideInspectorIndication();
#endif
}

InspectorAgent& PageInspectorController::ensureInspectorAgent()
{
    if (!m_inspectorAgent) {
        auto pageContext = pageAgentContext();
        auto inspectorAgent = makeUniqueRef<InspectorAgent>(pageContext);
        m_inspectorAgent = inspectorAgent.ptr();
        m_instrumentingAgents->setPersistentInspectorAgent(m_inspectorAgent.get());
        m_agents.append(WTF::move(inspectorAgent));
    }
    return *m_inspectorAgent;
}

InspectorDOMAgent& PageInspectorController::ensureDOMAgent()
{
    if (!m_domAgent) {
        auto pageContext = pageAgentContext();
        auto domAgent = makeUniqueRef<InspectorDOMAgent>(pageContext, m_overlay.get());
        m_domAgent = domAgent.ptr();
        m_agents.append(WTF::move(domAgent));
    }
    return *m_domAgent;
}

InspectorPageAgent& PageInspectorController::ensurePageAgent()
{
    if (!m_pageAgent) {
        auto pageContext = pageAgentContext();
        auto pageAgent = makeUniqueRef<InspectorPageAgent>(pageContext, m_inspectorBackendClient.get(), m_overlay.get());
        m_pageAgent = pageAgent.ptr();
        m_agents.append(WTF::move(pageAgent));
    }
    return *m_pageAgent;
}

bool PageInspectorController::developerExtrasEnabled() const
{
    return m_page->settings().developerExtrasEnabled();
}

bool PageInspectorController::canAccessInspectedScriptState(JSC::JSGlobalObject* lexicalGlobalObject) const
{
    JSLockHolder lock(lexicalGlobalObject);

    auto* inspectedWindow = jsDynamicCast<JSDOMWindow*>(lexicalGlobalObject);
    if (!inspectedWindow)
        return false;

    return BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, inspectedWindow->protectedWrapped(), DoNotReportSecurityError);
}

InspectorFunctionCallHandler PageInspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler PageInspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

void PageInspectorController::frontendInitialized()
{
    if (m_pauseAfterInitialization) {
        m_pauseAfterInitialization = false;
        if (auto* debuggerAgent = m_instrumentingAgents->enabledPageDebuggerAgent())
            std::ignore = debuggerAgent->pause();
    }
}

Stopwatch& PageInspectorController::executionStopwatch() const
{
    return m_executionStopwatch;
}

JSC::Debugger* PageInspectorController::debugger()
{
    ASSERT_IMPLIES(m_didCreateLazyAgents, m_debugger);
    return m_debugger.get();
}

JSC::VM& PageInspectorController::vm()
{
    return commonVM();
}

void PageInspectorController::willComposite(LocalFrame& frame)
{
    InspectorInstrumentation::willComposite(frame);
}

void PageInspectorController::didComposite(LocalFrame& frame)
{
    InspectorInstrumentation::didComposite(frame);
}

} // namespace WebCore

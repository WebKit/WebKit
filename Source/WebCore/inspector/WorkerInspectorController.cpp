/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerInspectorController.h"

#include "CommandLineAPIHost.h"
#include "InspectorBackendClient.h"
#include "InspectorController.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "Page.h"
#include "SWContextManager.h"
#include "ServiceWorkerAgent.h"
#include "ServiceWorkerGlobalScope.h"
#include "WebHeapAgent.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include "WorkerAuditAgent.h"
#include "WorkerCanvasAgent.h"
#include "WorkerConsoleAgent.h"
#include "WorkerDOMDebuggerAgent.h"
#include "WorkerDebugger.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerNetworkAgent.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerRuntimeAgent.h"
#include "WorkerThread.h"
#include "WorkerTimelineAgent.h"
#include "WorkerToPageFrontendChannel.h"
#include "WorkerWorkerAgent.h"
#include <JavaScriptCore/InspectorAgent.h>
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorScriptProfilerAgent.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerInspectorController);

WorkerInspectorController::WorkerInspectorController(WorkerOrWorkletGlobalScope& globalScope)
    : m_instrumentingAgents(InstrumentingAgents::create(*this))
    , m_injectedScriptManager(makeUniqueRef<WebInjectedScriptManager>(*this, WebInjectedScriptHost::create()))
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_executionStopwatch(Stopwatch::create())
    , m_globalScope(globalScope)
{
    ASSERT(globalScope.isContextThread());

    auto workerContext = workerAgentContext();

    auto consoleAgent = makeUnique<WorkerConsoleAgent>(workerContext);
    m_instrumentingAgents->setWebConsoleAgent(consoleAgent.get());
    m_agents.append(WTFMove(consoleAgent));
}

WorkerInspectorController::~WorkerInspectorController()
{
    ASSERT(!m_frontendRouter->hasFrontends());
    ASSERT(!m_forwardingChannel);

    m_instrumentingAgents->reset();
}

void WorkerInspectorController::workerTerminating()
{
    m_injectedScriptManager->disconnect();

    disconnectFrontend(Inspector::DisconnectReason::InspectedTargetDestroyed);

    m_agents.discardValues();

    m_debugger = nullptr;
}

void WorkerInspectorController::frontendInitialized()
{
#if ENABLE(REMOTE_INSPECTOR) && ENABLE(REMOTE_INSPECTOR_SERVICE_WORKER_AUTO_INSPECTION)
    if (m_pauseAfterInitialization) {
        m_pauseAfterInitialization = false;

        ensureDebuggerAgent().enable();
        ensureDebuggerAgent().pause();
    }

    if (m_isAutomaticInspection && is<ServiceWorkerGlobalScope>(m_globalScope)) {
        auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(m_globalScope.get()).thread().identifier();
        SWContextManager::singleton().stopRunningDebuggerTasksOnServiceWorker(serviceWorkerIdentifier);
    }
#endif
}

void WorkerInspectorController::connectFrontend(bool isAutomaticInspection, bool immediatelyPause)
{
    ASSERT(!m_frontendRouter->hasFrontends());
    ASSERT(!m_forwardingChannel);

    m_isAutomaticInspection = isAutomaticInspection;
    m_pauseAfterInitialization = immediatelyPause;

    createLazyAgents();

    callOnMainThread([] {
        InspectorInstrumentation::frontendCreated();
    });

    m_executionStopwatch->reset();
    m_executionStopwatch->start();

    m_forwardingChannel = makeUnique<WorkerToPageFrontendChannel>(m_globalScope);
    m_frontendRouter->connectFrontend(*m_forwardingChannel.get());
    m_agents.didCreateFrontendAndBackend();

    updateServiceWorkerPageFrontendCount();
}

void WorkerInspectorController::disconnectFrontend(Inspector::DisconnectReason reason)
{
    m_isAutomaticInspection = false;
    m_pauseAfterInitialization = false;

    if (!m_frontendRouter->hasFrontends())
        return;

    ASSERT(m_forwardingChannel);

    callOnMainThread([] {
        InspectorInstrumentation::frontendDeleted();
    });

    m_agents.willDestroyFrontendAndBackend(reason);
    m_frontendRouter->disconnectFrontend(*m_forwardingChannel.get());
    m_forwardingChannel = nullptr;

    updateServiceWorkerPageFrontendCount();
}

void WorkerInspectorController::updateServiceWorkerPageFrontendCount()
{
    RefPtr globalScope = dynamicDowncast<ServiceWorkerGlobalScope>(m_globalScope.get());
    if (!globalScope)
        return;

    RefPtr serviceWorkerPage = globalScope->serviceWorkerPage();
    if (!serviceWorkerPage)
        return;

    ASSERT(isMainThread());

    // When a service worker is loaded in a Page, we need to report its inspector frontend count
    // up to the page's inspectorController so the client knows about it.
    auto inspectorBackendClient = serviceWorkerPage->inspectorController().inspectorBackendClient();
    if (!inspectorBackendClient)
        return;

    inspectorBackendClient->frontendCountChanged(m_frontendRouter->frontendCount());
}

void WorkerInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

WorkerAgentContext WorkerInspectorController::workerAgentContext()
{
    AgentContext baseContext = {
        *this,
        m_injectedScriptManager,
        m_frontendRouter,
        m_backendDispatcher,
    };

    WebAgentContext webContext = {
        baseContext,
        m_instrumentingAgents.get(),
    };

    WorkerAgentContext workerContext = {
        webContext,
        m_globalScope,
    };

    return workerContext;
}

void WorkerInspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;

    m_debugger = makeUnique<WorkerDebugger>(m_globalScope);

    m_injectedScriptManager->connect();

    auto workerContext = workerAgentContext();

    m_agents.append(makeUnique<WorkerRuntimeAgent>(workerContext));

    if (is<ServiceWorkerGlobalScope>(m_globalScope)) {
        m_agents.append(makeUnique<InspectorAgent>(workerContext));
        m_agents.append(makeUnique<ServiceWorkerAgent>(workerContext));
        m_agents.append(makeUnique<WorkerNetworkAgent>(workerContext));
    }

    m_agents.append(makeUnique<WebHeapAgent>(workerContext));

    ensureDebuggerAgent();
    m_agents.append(makeUnique<WorkerDOMDebuggerAgent>(workerContext, m_debuggerAgent.get()));

    m_agents.append(makeUnique<WorkerAuditAgent>(workerContext));
    m_agents.append(makeUnique<WorkerCanvasAgent>(workerContext));
    m_agents.append(makeUnique<WorkerTimelineAgent>(workerContext));
    m_agents.append(makeUnique<WorkerWorkerAgent>(workerContext));

    auto scriptProfilerAgentPtr = makeUnique<InspectorScriptProfilerAgent>(workerContext);
    m_instrumentingAgents->setPersistentScriptProfilerAgent(scriptProfilerAgentPtr.get());
    m_agents.append(WTFMove(scriptProfilerAgentPtr));

    if (auto& commandLineAPIHost = m_injectedScriptManager->commandLineAPIHost())
        commandLineAPIHost->init(m_instrumentingAgents.copyRef());
}

WorkerDebuggerAgent& WorkerInspectorController::ensureDebuggerAgent()
{
    if (!m_debuggerAgent) {
        auto workerContext = workerAgentContext();
        auto debuggerAgent = makeUnique<WorkerDebuggerAgent>(workerContext);
        m_debuggerAgent = debuggerAgent.get();
        m_agents.append(WTFMove(debuggerAgent));
    }
    return *m_debuggerAgent;
}

InspectorFunctionCallHandler WorkerInspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler WorkerInspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

Stopwatch& WorkerInspectorController::executionStopwatch() const
{
    return m_executionStopwatch;
}

JSC::Debugger* WorkerInspectorController::debugger()
{
    ASSERT_IMPLIES(m_didCreateLazyAgents, m_debugger);
    return m_debugger.get();
}

VM& WorkerInspectorController::vm()
{
    return m_globalScope->vm();
}

} // namespace WebCore

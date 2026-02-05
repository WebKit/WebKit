/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "FrameInspectorController.h"

#include "CommonVM.h"
#include "DocumentPage.h"
#include "FrameInlines.h"
#include "InspectorInstrumentation.h"
#include "InspectorWebAgentBase.h"
#include "InstrumentingAgents.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMWindow.h"
#include "JSExecState.h"
#include "PageInspectorController.h"
#include "Settings.h"
#include "WebInjectedScriptHost.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/Strong.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameInspectorController);

FrameInspectorController::FrameInspectorController(LocalFrame& frame)
    : m_frame(frame)
    , m_instrumentingAgents(InstrumentingAgents::create(*this, protect(frame.page())->protectedInspectorController()->instrumentingAgents()))
    , m_injectedScriptManager(protect(frame.page())->protectedInspectorController()->injectedScriptManager())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef(), &protect(frame.page())->protectedInspectorController()->backendDispatcher()))
    , m_executionStopwatch(Stopwatch::create())
{
}

FrameInspectorController::~FrameInspectorController()
{
    m_instrumentingAgents->reset();
}

void FrameInspectorController::ref() const
{
    m_frame->ref();
}

void FrameInspectorController::deref() const
{
    m_frame->deref();
}

FrameAgentContext FrameInspectorController::frameAgentContext()
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
    return {
        webContext,
        m_frame
    };
}

void FrameInspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;
}

void FrameInspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel, bool isAutomaticInspection, bool immediatelyPause)
{
    UNUSED_PARAM(isAutomaticInspection);
    UNUSED_PARAM(immediatelyPause);

    if (RefPtr page = m_frame->page())
        page->settings().setDeveloperExtrasEnabled(true);

    bool connectedFirstFrontend = !m_frontendRouter->hasFrontends();

    createLazyAgents();
    m_frontendRouter->connectFrontend(frontendChannel);
    InspectorInstrumentation::frontendCreated();

    if (connectedFirstFrontend) {
        InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
        m_injectedScriptManager->addClient();
        m_agents.didCreateFrontendAndBackend();
    }
}

void FrameInspectorController::disconnectFrontend(Inspector::FrontendChannel& frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);
    InspectorInstrumentation::frontendDeleted();

    bool disconnectedLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectedLastFrontend) {
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
        m_injectedScriptManager->removeClient();
        InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    }
}

void FrameInspectorController::inspectedFrameDestroyed()
{
    if (!m_frontendRouter->hasFrontends())
        return;

    for (unsigned i = 0; i < m_frontendRouter->frontendCount(); ++i)
        InspectorInstrumentation::frontendDeleted();

    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    m_injectedScriptManager->removeClient();
    m_frontendRouter->disconnectAllFrontends();

    m_agents.discardValues();
}

void FrameInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

bool FrameInspectorController::developerExtrasEnabled() const
{
    RefPtr page = m_frame->page();
    return page && page->settings().developerExtrasEnabled();
}

bool FrameInspectorController::canAccessInspectedScriptState(JSC::JSGlobalObject* lexicalGlobalObject) const
{
    JSLockHolder lock(lexicalGlobalObject);

    auto* inspectedWindow = jsDynamicCast<JSDOMWindow*>(lexicalGlobalObject);
    if (!inspectedWindow)
        return false;

    Ref protectedWindow { inspectedWindow->wrapped() };
    return BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, protectedWindow.get(), DoNotReportSecurityError);
}

InspectorFunctionCallHandler FrameInspectorController::functionCallHandler() const
{
    return WebCore::functionCallHandlerFromAnyThread;
}

InspectorEvaluateHandler FrameInspectorController::evaluateHandler() const
{
    return WebCore::evaluateHandlerFromAnyThread;
}

void FrameInspectorController::frontendInitialized()
{
}

Stopwatch& FrameInspectorController::executionStopwatch() const
{
    return m_executionStopwatch;
}

JSC::Debugger* FrameInspectorController::debugger()
{
    // FIXME <https://webkit.org/b/298909> Add Debugger support for frame targets.
    return nullptr;
}

JSC::VM& FrameInspectorController::vm()
{
    return commonVM();
}

} // namespace WebCore

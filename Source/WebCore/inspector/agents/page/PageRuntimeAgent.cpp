/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "PageRuntimeAgent.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindowBase.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "ScriptController.h"
#include "ScriptState.h"
#include "SecurityOrigin.h"
#include "UserGestureIndicator.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>

using Inspector::Protocol::Runtime::ExecutionContextDescription;


namespace WebCore {

using namespace Inspector;

PageRuntimeAgent::PageRuntimeAgent(PageAgentContext& context)
    : InspectorRuntimeAgent(context)
    , m_frontendDispatcher(makeUnique<Inspector::RuntimeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::RuntimeBackendDispatcher::create(context.backendDispatcher, this))
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedPage(context.inspectedPage)
{
}

PageRuntimeAgent::~PageRuntimeAgent() = default;

void PageRuntimeAgent::enable(ErrorString& errorString)
{
    bool enabled = m_instrumentingAgents.pageRuntimeAgent() == this;

    InspectorRuntimeAgent::enable(errorString);

    m_instrumentingAgents.setPageRuntimeAgent(this);

    if (!enabled)
        reportExecutionContextCreation();
}

void PageRuntimeAgent::disable(ErrorString& errorString)
{
    m_instrumentingAgents.setPageRuntimeAgent(nullptr);

    InspectorRuntimeAgent::disable(errorString);
}

void PageRuntimeAgent::didCreateMainWorldContext(Frame& frame)
{
    auto* pageAgent = m_instrumentingAgents.inspectorPageAgent();
    if (!pageAgent)
        return;

    auto frameId = pageAgent->frameId(&frame);
    auto* scriptState = mainWorldExecState(&frame);
    notifyContextCreated(frameId, scriptState, nullptr, true);
}

InjectedScript PageRuntimeAgent::injectedScriptForEval(ErrorString& errorString, const int* executionContextId)
{
    if (!executionContextId) {
        JSC::ExecState* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
        InjectedScript result = injectedScriptManager().injectedScriptFor(scriptState);
        if (result.hasNoValue())
            errorString = "Internal error: main world execution context not found"_s;
        return result;
    }

    InjectedScript injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId"_s;
    return injectedScript;
}

void PageRuntimeAgent::muteConsole()
{
    PageConsoleClient::mute();
}

void PageRuntimeAgent::unmuteConsole()
{
    PageConsoleClient::unmute();
}

void PageRuntimeAgent::reportExecutionContextCreation()
{
    auto* pageAgent = m_instrumentingAgents.inspectorPageAgent();
    if (!pageAgent)
        return;

    Vector<std::pair<JSC::ExecState*, SecurityOrigin*>> isolatedContexts;
    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
            continue;

        String frameId = pageAgent->frameId(frame);

        JSC::ExecState* scriptState = mainWorldExecState(frame);
        notifyContextCreated(frameId, scriptState, nullptr, true);
        frame->script().collectIsolatedContexts(isolatedContexts);
        if (isolatedContexts.isEmpty())
            continue;
        for (auto& context : isolatedContexts)
            notifyContextCreated(frameId, context.first, context.second, false);
        isolatedContexts.clear();
    }
}

void PageRuntimeAgent::notifyContextCreated(const String& frameId, JSC::ExecState* scriptState, SecurityOrigin* securityOrigin, bool isPageContext)
{
    ASSERT(securityOrigin || isPageContext);

    InjectedScript result = injectedScriptManager().injectedScriptFor(scriptState);
    if (result.hasNoValue())
        return;

    int executionContextId = injectedScriptManager().injectedScriptIdFor(scriptState);
    String name = securityOrigin ? securityOrigin->toRawString() : String();
    m_frontendDispatcher->executionContextCreated(ExecutionContextDescription::create()
        .setId(executionContextId)
        .setIsPageContext(isPageContext)
        .setName(name)
        .setFrameId(frameId)
        .release());
}

void PageRuntimeAgent::evaluate(ErrorString& errorString, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* emulateUserGesture, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    auto& pageChromeClient = m_inspectedPage.chrome().client();

    auto shouldEmulateUserGesture = emulateUserGesture && *emulateUserGesture;

    Optional<ProcessingUserGestureState> userGestureState = shouldEmulateUserGesture ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt;
    UserGestureIndicator gestureIndicator(userGestureState);

    bool userWasInteracting = false;
    if (shouldEmulateUserGesture) {
        userWasInteracting = pageChromeClient.userIsInteracting();
        if (!userWasInteracting)
            pageChromeClient.setUserIsInteracting(true);
    }

    InspectorRuntimeAgent::evaluate(errorString, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, executionContextId, returnByValue, generatePreview, saveResult, emulateUserGesture, result, wasThrown, savedResultIndex);

    if (shouldEmulateUserGesture && !userWasInteracting && pageChromeClient.userIsInteracting())
        pageChromeClient.setUserIsInteracting(false);
}

} // namespace WebCore

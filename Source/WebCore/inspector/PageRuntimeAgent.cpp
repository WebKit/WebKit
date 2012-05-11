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

#include "PageRuntimeAgent.h"

#include "Console.h"
#include "Document.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "SecurityOrigin.h"

using WebCore::TypeBuilder::Runtime::ExecutionContextDescription;

namespace WebCore {

namespace PageRuntimeAgentState {
static const char reportExecutionContextCreation[] = "reportExecutionContextCreation";
};

PageRuntimeAgent::PageRuntimeAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InjectedScriptManager* injectedScriptManager, Page* page, InspectorPageAgent* pageAgent)
    : InspectorRuntimeAgent(instrumentingAgents, state, injectedScriptManager)
    , m_inspectedPage(page)
    , m_pageAgent(pageAgent)
    , m_frontend(0)
{
}

PageRuntimeAgent::~PageRuntimeAgent()
{
}

void PageRuntimeAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->runtime();
    m_instrumentingAgents->setPageRuntimeAgent(this);
}

void PageRuntimeAgent::clearFrontend()
{
    m_instrumentingAgents->setPageRuntimeAgent(0);
    m_frontend = 0;
    m_state->setBoolean(PageRuntimeAgentState::reportExecutionContextCreation, false);
}

void PageRuntimeAgent::restore()
{
    if (!m_state->getBoolean(PageRuntimeAgentState::reportExecutionContextCreation))
        return;
    String error;
    setReportExecutionContextCreation(&error, true);
}

void PageRuntimeAgent::setReportExecutionContextCreation(ErrorString*, bool enable)
{
    m_state->setBoolean(PageRuntimeAgentState::reportExecutionContextCreation, enable);
    if (!enable)
        return;
    Vector<std::pair<ScriptState*, SecurityOrigin*> > isolatedContexts;
    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (!frame->script()->canExecuteScripts(NotAboutToExecuteScript))
            continue;
        String frameId = m_pageAgent->frameId(frame);

        ScriptState* scriptState = mainWorldScriptState(frame);
        notifyContextCreated(frameId, scriptState, 0, true);
        frame->script()->collectIsolatedContexts(isolatedContexts);
        if (isolatedContexts.isEmpty())
            continue;
        for (size_t i = 0; i< isolatedContexts.size(); i++)
            notifyContextCreated(frameId, isolatedContexts[i].first, isolatedContexts[i].second, false);
        isolatedContexts.clear();
    }
}

void PageRuntimeAgent::didClearWindowObject(Frame* frame)
{
    if (!m_state->getBoolean(PageRuntimeAgentState::reportExecutionContextCreation))
        return;
    ASSERT(m_frontend);
    String frameId = m_pageAgent->frameId(frame);
    ScriptState* scriptState = mainWorldScriptState(frame);
    notifyContextCreated(frameId, scriptState, 0, true);
}

void PageRuntimeAgent::didCreateIsolatedContext(Frame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
    if (!m_state->getBoolean(PageRuntimeAgentState::reportExecutionContextCreation))
        return;
    ASSERT(m_frontend);
    String frameId = m_pageAgent->frameId(frame);
    notifyContextCreated(frameId, scriptState, origin, false);
}

InjectedScript PageRuntimeAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    if (!executionContextId) {
        ScriptState* scriptState = mainWorldScriptState(m_inspectedPage->mainFrame());
        return injectedScriptManager()->injectedScriptFor(scriptState);
    }
    InjectedScript injectedScript = injectedScriptManager()->injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        *errorString = "Execution context with given id not found.";
    return injectedScript;
}

void PageRuntimeAgent::muteConsole()
{
    Console::mute();
}

void PageRuntimeAgent::unmuteConsole()
{
    Console::unmute();
}

void PageRuntimeAgent::notifyContextCreated(const String& frameId, ScriptState* scriptState, SecurityOrigin* securityOrigin, bool isPageContext)
{
    ASSERT(securityOrigin || isPageContext);
    long executionContextId = injectedScriptManager()->injectedScriptIdFor(scriptState);
    String name = securityOrigin ? securityOrigin->toString() : "";
    m_frontend->isolatedContextCreated(ExecutionContextDescription::create()
        .setId(executionContextId)
        .setIsPageContext(isPageContext)
        .setName(name)
        .setFrameId(frameId)
        .release());
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

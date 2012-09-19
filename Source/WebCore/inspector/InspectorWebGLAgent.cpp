/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR) && ENABLE(WEBGL)

#include "InspectorWebGLAgent.h"

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InjectedScriptWebGLModule.h" 
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "ScriptObject.h"
#include "ScriptState.h"

namespace WebCore {

namespace WebGLAgentState {
static const char webGLAgentEnabled[] = "webGLAgentEnabled";
};

InspectorWebGLAgent::InspectorWebGLAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, Page* page, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorWebGLAgent>("WebGL", instrumentingAgents, state)
    , m_inspectedPage(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_enabled(false)
{
    m_instrumentingAgents->setInspectorWebGLAgent(this);
}

InspectorWebGLAgent::~InspectorWebGLAgent()
{
    m_instrumentingAgents->setInspectorWebGLAgent(0);
}

void InspectorWebGLAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    m_frontend = frontend->webgl();
}

void InspectorWebGLAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorWebGLAgent::restore()
{
    m_enabled = m_state->getBoolean(WebGLAgentState::webGLAgentEnabled);
}

void InspectorWebGLAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(WebGLAgentState::webGLAgentEnabled, m_enabled);
}

void InspectorWebGLAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(WebGLAgentState::webGLAgentEnabled, m_enabled);
}

void InspectorWebGLAgent::dropTraceLog(ErrorString* errorString, const String& traceLogId)
{
    InjectedScriptWebGLModule module = injectedScriptWebGLModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.dropTraceLog(errorString, traceLogId);
}

void InspectorWebGLAgent::captureFrame(ErrorString* errorString, String* traceLogId)
{
    ScriptState* scriptState = mainWorldScriptState(m_inspectedPage->mainFrame());
    InjectedScriptWebGLModule module = InjectedScriptWebGLModule::moduleForState(m_injectedScriptManager, scriptState);
    if (module.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    module.captureFrame(errorString, traceLogId);
}

void InspectorWebGLAgent::getTraceLog(ErrorString* errorString, const String& traceLogId, RefPtr<TypeBuilder::WebGL::TraceLog>& traceLog)
{
    InjectedScriptWebGLModule module = injectedScriptWebGLModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.traceLog(errorString, traceLogId, &traceLog);
}

void InspectorWebGLAgent::replayTraceLog(ErrorString* errorString, const String& traceLogId, int stepNo, String* result)
{
    InjectedScriptWebGLModule module = injectedScriptWebGLModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.replayTraceLog(errorString, traceLogId, stepNo, result);
}

ScriptObject InspectorWebGLAgent::wrapWebGLRenderingContextForInstrumentation(const ScriptObject& glContext)
{
    if (glContext.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    InjectedScriptWebGLModule module = InjectedScriptWebGLModule::moduleForState(m_injectedScriptManager, glContext.scriptState());
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    return module.wrapWebGLContext(glContext);
}

InjectedScriptWebGLModule InspectorWebGLAgent::injectedScriptWebGLModuleForTraceLogId(ErrorString* errorString, const String& traceLogId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(traceLogId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return InjectedScriptWebGLModule();
    }
    InjectedScriptWebGLModule module = InjectedScriptWebGLModule::moduleForState(m_injectedScriptManager, injectedScript.scriptState());
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: no WebGL module";
        return InjectedScriptWebGLModule();
    }
    return module;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(WEBGL)

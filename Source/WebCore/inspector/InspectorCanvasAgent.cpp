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

#if ENABLE(INSPECTOR)

#include "InspectorCanvasAgent.h"

#include "InjectedScript.h"
#include "InjectedScriptCanvasModule.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "ScriptObject.h"
#include "ScriptState.h"

namespace WebCore {

namespace CanvasAgentState {
static const char enabled[] = "enabled";
};

InspectorCanvasAgent::InspectorCanvasAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, Page* page, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorCanvasAgent>("Canvas", instrumentingAgents, state)
    , m_inspectedPage(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
{
}

InspectorCanvasAgent::~InspectorCanvasAgent()
{
}

void InspectorCanvasAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    m_frontend = frontend->canvas();
}

void InspectorCanvasAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorCanvasAgent::restore()
{
    if (m_state->getBoolean(CanvasAgentState::enabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorCanvasAgent::enable(ErrorString*)
{
    m_state->setBoolean(CanvasAgentState::enabled, true);
    m_instrumentingAgents->setInspectorCanvasAgent(this);
}

void InspectorCanvasAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorCanvasAgent(0);
    m_state->setBoolean(CanvasAgentState::enabled, false);
}

void InspectorCanvasAgent::dropTraceLog(ErrorString* errorString, const String& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.dropTraceLog(errorString, traceLogId);
}

void InspectorCanvasAgent::captureFrame(ErrorString* errorString, String* traceLogId)
{
    ScriptState* scriptState = mainWorldScriptState(m_inspectedPage->mainFrame());
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, scriptState);
    if (module.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    module.captureFrame(errorString, traceLogId);
}

void InspectorCanvasAgent::startCapturing(ErrorString* errorString, String* traceLogId)
{
    ScriptState* scriptState = mainWorldScriptState(m_inspectedPage->mainFrame());
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, scriptState);
    if (module.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    module.startCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::stopCapturing(ErrorString* errorString, const String& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.stopCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::getTraceLog(ErrorString* errorString, const String& traceLogId, const int* startOffset, RefPtr<TypeBuilder::Canvas::TraceLog>& traceLog)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.traceLog(errorString, traceLogId, startOffset, &traceLog);
}

void InspectorCanvasAgent::replayTraceLog(ErrorString* errorString, const String& traceLogId, int stepNo, String* result)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.replayTraceLog(errorString, traceLogId, stepNo, result);
}

ScriptObject InspectorCanvasAgent::wrapCanvas2DRenderingContextForInstrumentation(const ScriptObject& context)
{
    if (context.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, context.scriptState());
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    return module.wrapCanvas2DContext(context);
}

#if ENABLE(WEBGL)
ScriptObject InspectorCanvasAgent::wrapWebGLRenderingContextForInstrumentation(const ScriptObject& glContext)
{
    if (glContext.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, glContext.scriptState());
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    return module.wrapWebGLContext(glContext);
}
#endif

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModuleForTraceLogId(ErrorString* errorString, const String& traceLogId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(traceLogId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return InjectedScriptCanvasModule();
    }
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, injectedScript.scriptState());
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: no Canvas module";
        return InjectedScriptCanvasModule();
    }
    return module;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

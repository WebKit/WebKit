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

#include "BindingVisitors.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "InjectedScript.h"
#include "InjectedScriptCanvasModule.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "ScriptObject.h"
#include "ScriptProfiler.h"
#include "ScriptState.h"

namespace WebCore {

namespace CanvasAgentState {
static const char canvasAgentEnabled[] = "canvasAgentEnabled";
};

InspectorCanvasAgent::InspectorCanvasAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, Page* page, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorCanvasAgent>("Canvas", instrumentingAgents, state)
    , m_inspectedPage(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_enabled(false)
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
    if (m_state->getBoolean(CanvasAgentState::canvasAgentEnabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorCanvasAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(CanvasAgentState::canvasAgentEnabled, m_enabled);
    m_instrumentingAgents->setInspectorCanvasAgent(this);
    findFramesWithUninstrumentedCanvases();
}

void InspectorCanvasAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_state->setBoolean(CanvasAgentState::canvasAgentEnabled, m_enabled);
    m_instrumentingAgents->setInspectorCanvasAgent(0);
    m_framesWithUninstrumentedCanvases.clear();
}

void InspectorCanvasAgent::dropTraceLog(ErrorString* errorString, const String& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.dropTraceLog(errorString, traceLogId);
}

void InspectorCanvasAgent::hasUninstrumentedCanvases(ErrorString* errorString, bool* result)
{
    if (!checkIsEnabled(errorString))
        return;
    *result = m_framesWithUninstrumentedCanvases.contains(m_inspectedPage->mainFrame());
}

void InspectorCanvasAgent::captureFrame(ErrorString* errorString, String* traceLogId)
{
    if (!checkIsEnabled(errorString))
        return;
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
    if (!checkIsEnabled(errorString))
        return;
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
    if (!checkIsEnabled(errorString))
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.stopCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::getTraceLog(ErrorString* errorString, const String& traceLogId, const int* startOffset, RefPtr<TypeBuilder::Canvas::TraceLog>& traceLog)
{
    if (!checkIsEnabled(errorString))
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.traceLog(errorString, traceLogId, startOffset, &traceLog);
}

void InspectorCanvasAgent::replayTraceLog(ErrorString* errorString, const String& traceLogId, int stepNo, RefPtr<TypeBuilder::Canvas::ResourceState>& result)
{
    if (!checkIsEnabled(errorString))
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.replayTraceLog(errorString, traceLogId, stepNo, &result);
}

void InspectorCanvasAgent::getResourceInfo(ErrorString* errorString, const String& resourceId, RefPtr<TypeBuilder::Canvas::ResourceInfo>& result)
{
    if (!checkIsEnabled(errorString))
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, resourceId);
    if (!module.hasNoValue())
        module.resourceInfo(errorString, resourceId, &result);
}

void InspectorCanvasAgent::getResourceState(ErrorString* errorString, const String& traceLogId, const String& resourceId, RefPtr<TypeBuilder::Canvas::ResourceState>& result)
{
    if (!checkIsEnabled(errorString))
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModuleForTraceLogId(errorString, traceLogId);
    if (!module.hasNoValue())
        module.resourceState(errorString, traceLogId, resourceId, &result);
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

void InspectorCanvasAgent::findFramesWithUninstrumentedCanvases()
{
    class NodeVisitor : public WrappedNodeVisitor {
    public:
        NodeVisitor(Page* page, FramesWithUninstrumentedCanvases& hasUninstrumentedCanvasesResults)
            : m_page(page)
            , m_framesWithUninstrumentedCanvases(hasUninstrumentedCanvasesResults)
        {
        }

        virtual void visitNode(Node* node) OVERRIDE
        {
            if (!node->hasTagName(HTMLNames::canvasTag) || !node->document() || !node->document()->frame())
                return;
            
            Frame* frame = node->document()->frame();
            if (frame->page() != m_page)
                return;

            HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(node);
            if (canvas->renderingContext())
                m_framesWithUninstrumentedCanvases.add(frame);
        }

    private:
        Page* m_page;
        FramesWithUninstrumentedCanvases& m_framesWithUninstrumentedCanvases;
    } nodeVisitor(m_inspectedPage, m_framesWithUninstrumentedCanvases);

    ScriptProfiler::visitNodeWrappers(&nodeVisitor);
}

bool InspectorCanvasAgent::checkIsEnabled(ErrorString* errorString) const
{
    if (m_enabled)
        return true;
    *errorString = "Canvas agent is not enabled";
    return false;
}

void InspectorCanvasAgent::reset()
{
    m_framesWithUninstrumentedCanvases.clear();
    if (m_enabled)
        findFramesWithUninstrumentedCanvases();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

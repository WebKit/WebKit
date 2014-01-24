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

#include "DOMWindow.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "InjectedScriptCanvasModule.h"
#include "InspectorPageAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "MainFrame.h"
#include "NodeList.h"
#include "Page.h"
#include "ScriptProfiler.h"
#include "ScriptState.h"
#include <bindings/ScriptObject.h>
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>

using Inspector::TypeBuilder::Array;
using Inspector::TypeBuilder::Canvas::ResourceId;
using Inspector::TypeBuilder::Canvas::ResourceInfo;
using Inspector::TypeBuilder::Canvas::ResourceState;
using Inspector::TypeBuilder::Canvas::TraceLog;
using Inspector::TypeBuilder::Canvas::TraceLogId;
using Inspector::TypeBuilder::Network::FrameId;

using namespace Inspector;

namespace WebCore {

InspectorCanvasAgent::InspectorCanvasAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Canvas"), instrumentingAgents)
    , m_pageAgent(pageAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_enabled(false)
{
}

InspectorCanvasAgent::~InspectorCanvasAgent()
{
}

void InspectorCanvasAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorCanvasFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorCanvasBackendDispatcher::create(backendDispatcher, this);
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    disable(nullptr);
}

void InspectorCanvasAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_instrumentingAgents->setInspectorCanvasAgent(this);
    findFramesWithUninstrumentedCanvases();
}

void InspectorCanvasAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_instrumentingAgents->setInspectorCanvasAgent(nullptr);
    m_framesWithUninstrumentedCanvases.clear();
}

void InspectorCanvasAgent::dropTraceLog(ErrorString* errorString, const TraceLogId& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.hasNoValue())
        module.dropTraceLog(errorString, traceLogId);
}

void InspectorCanvasAgent::hasUninstrumentedCanvases(ErrorString* errorString, bool* result)
{
    if (!checkIsEnabled(errorString))
        return;
    for (FramesWithUninstrumentedCanvases::iterator it = m_framesWithUninstrumentedCanvases.begin(); it != m_framesWithUninstrumentedCanvases.end(); ++it) {
        if (it->value) {
            *result = true;
            return;
        }
    }
    *result = false;
}

void InspectorCanvasAgent::captureFrame(ErrorString* errorString, const FrameId* frameId, TraceLogId* traceLogId)
{
    Frame* frame = frameId ? m_pageAgent->assertFrame(errorString, *frameId) : m_pageAgent->mainFrame();
    if (!frame)
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, mainWorldExecState(frame));
    if (!module.hasNoValue())
        module.captureFrame(errorString, traceLogId);
}

void InspectorCanvasAgent::startCapturing(ErrorString* errorString, const FrameId* frameId, TraceLogId* traceLogId)
{
    Frame* frame = frameId ? m_pageAgent->assertFrame(errorString, *frameId) : m_pageAgent->mainFrame();
    if (!frame)
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, mainWorldExecState(frame));
    if (!module.hasNoValue())
        module.startCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::stopCapturing(ErrorString* errorString, const TraceLogId& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.hasNoValue())
        module.stopCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::getTraceLog(ErrorString* errorString, const TraceLogId& traceLogId, const int* startOffset, const int* maxLength, RefPtr<TraceLog>& traceLog)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.hasNoValue())
        module.traceLog(errorString, traceLogId, startOffset, maxLength, &traceLog);
}

void InspectorCanvasAgent::replayTraceLog(ErrorString* errorString, const TraceLogId& traceLogId, int stepNo, RefPtr<ResourceState>& result)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.hasNoValue())
        module.replayTraceLog(errorString, traceLogId, stepNo, &result);
}

void InspectorCanvasAgent::getResourceInfo(ErrorString* errorString, const ResourceId& resourceId, RefPtr<ResourceInfo>& result)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, resourceId);
    if (!module.hasNoValue())
        module.resourceInfo(errorString, resourceId, &result);
}

void InspectorCanvasAgent::getResourceState(ErrorString* errorString, const TraceLogId& traceLogId, const ResourceId& resourceId, RefPtr<ResourceState>& result)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.hasNoValue())
        module.resourceState(errorString, traceLogId, resourceId, &result);
}

Deprecated::ScriptObject InspectorCanvasAgent::wrapCanvas2DRenderingContextForInstrumentation(const Deprecated::ScriptObject& context)
{
    ErrorString error;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, context);
    if (module.hasNoValue())
        return Deprecated::ScriptObject();
    return notifyRenderingContextWasWrapped(module.wrapCanvas2DContext(context));
}

#if ENABLE(WEBGL)
Deprecated::ScriptObject InspectorCanvasAgent::wrapWebGLRenderingContextForInstrumentation(const Deprecated::ScriptObject& glContext)
{
    ErrorString error;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, glContext);
    if (module.hasNoValue())
        return Deprecated::ScriptObject();
    return notifyRenderingContextWasWrapped(module.wrapWebGLContext(glContext));
}
#endif

Deprecated::ScriptObject InspectorCanvasAgent::notifyRenderingContextWasWrapped(const Deprecated::ScriptObject& wrappedContext)
{
    ASSERT(m_frontendDispatcher);
    JSC::ExecState* scriptState = wrappedContext.scriptState();
    DOMWindow* domWindow = scriptState ? domWindowFromExecState(scriptState) : nullptr;
    Frame* frame = domWindow ? domWindow->frame() : nullptr;
    if (frame && !m_framesWithUninstrumentedCanvases.contains(frame))
        m_framesWithUninstrumentedCanvases.set(frame, false);
    String frameId = m_pageAgent->frameId(frame);
    if (!frameId.isEmpty())
        m_frontendDispatcher->contextCreated(frameId);
    return wrappedContext;
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, JSC::ExecState* scriptState)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, scriptState);
    if (module.hasNoValue()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: no Canvas module";
    }
    return module;
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, const Deprecated::ScriptObject& scriptObject)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    if (scriptObject.hasNoValue()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: original Deprecated::ScriptObject has no value";
        return InjectedScriptCanvasModule();
    }
    return injectedScriptCanvasModule(errorString, scriptObject.scriptState());
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, const String& objectId)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return InjectedScriptCanvasModule();
    }
    return injectedScriptCanvasModule(errorString, injectedScript.scriptState());
}

void InspectorCanvasAgent::findFramesWithUninstrumentedCanvases()
{
    m_framesWithUninstrumentedCanvases.clear();

    for (Frame* frame = &m_pageAgent->page()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->document())
            continue;

        RefPtr<NodeList> canvases = frame->document()->getElementsByTagName(HTMLNames::canvasTag.localName());
        if (canvases) {
            for (unsigned i = 0, length = canvases->length(); i < length; i++) {
                const HTMLCanvasElement* canvas = toHTMLCanvasElement(canvases->item(i));
                if (canvas->renderingContext()) {
                    m_framesWithUninstrumentedCanvases.set(frame, true);
                    break;
                }
            }
        }
    }

    for (FramesWithUninstrumentedCanvases::iterator it = m_framesWithUninstrumentedCanvases.begin(); it != m_framesWithUninstrumentedCanvases.end(); ++it) {
        String frameId = m_pageAgent->frameId(it->key);
        if (!frameId.isEmpty())
            m_frontendDispatcher->contextCreated(frameId);
    }
}

bool InspectorCanvasAgent::checkIsEnabled(ErrorString* errorString) const
{
    if (m_enabled)
        return true;
    *errorString = "Canvas agent is not enabled";
    return false;
}

void InspectorCanvasAgent::frameNavigated(Frame* frame)
{
    if (!m_enabled)
        return;
    if (frame == m_pageAgent->mainFrame()) {
        for (FramesWithUninstrumentedCanvases::iterator it = m_framesWithUninstrumentedCanvases.begin(); it != m_framesWithUninstrumentedCanvases.end(); ++it)
            m_framesWithUninstrumentedCanvases.set(it->key, false);
        m_frontendDispatcher->traceLogsRemoved(nullptr, nullptr);
    } else {
        while (frame) {
            if (m_framesWithUninstrumentedCanvases.contains(frame))
                m_framesWithUninstrumentedCanvases.set(frame, false);
            if (m_pageAgent->hasIdForFrame(frame)) {
                String frameId = m_pageAgent->frameId(frame);
                m_frontendDispatcher->traceLogsRemoved(&frameId, nullptr);
            }
            frame = frame->tree().traverseNext();
        }
    }
}

void InspectorCanvasAgent::frameDetached(Frame* frame)
{
    if (m_enabled)
        m_framesWithUninstrumentedCanvases.remove(frame);
}

void InspectorCanvasAgent::didBeginFrame()
{
    if (!m_enabled)
        return;
    ErrorString error;
    for (FramesWithUninstrumentedCanvases::iterator it = m_framesWithUninstrumentedCanvases.begin(); it != m_framesWithUninstrumentedCanvases.end(); ++it) {
        InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, mainWorldExecState(it->key));
        if (!module.hasNoValue())
            module.markFrameEnd();
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

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

#ifndef InspectorCanvasAgent_h
#define InspectorCanvasAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InspectorWebTypeBuilders.h"
#include "ScriptState.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace Deprecated {
class ScriptObject;
}

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class Frame;
class InjectedScriptCanvasModule;
class InspectorPageAgent;
class InstrumentingAgents;

typedef String ErrorString;

class InspectorCanvasAgent : public InspectorAgentBase, public Inspector::InspectorCanvasBackendDispatcherHandler {
public:
    InspectorCanvasAgent(InstrumentingAgents*, InspectorPageAgent*, Inspector::InjectedScriptManager*);
    ~InspectorCanvasAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend() override;

    void frameNavigated(Frame*);
    void frameDetached(Frame*);
    void didBeginFrame();

    // Called from InspectorCanvasInstrumentation.
    Deprecated::ScriptObject wrapCanvas2DRenderingContextForInstrumentation(const Deprecated::ScriptObject&);
#if ENABLE(WEBGL)
    Deprecated::ScriptObject wrapWebGLRenderingContextForInstrumentation(const Deprecated::ScriptObject&);
#endif

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void dropTraceLog(ErrorString*, const Inspector::TypeBuilder::Canvas::TraceLogId&);
    virtual void hasUninstrumentedCanvases(ErrorString*, bool*);
    virtual void captureFrame(ErrorString*, const Inspector::TypeBuilder::Network::FrameId*, Inspector::TypeBuilder::Canvas::TraceLogId*);
    virtual void startCapturing(ErrorString*, const Inspector::TypeBuilder::Network::FrameId*, Inspector::TypeBuilder::Canvas::TraceLogId*);
    virtual void stopCapturing(ErrorString*, const Inspector::TypeBuilder::Canvas::TraceLogId&);
    virtual void getTraceLog(ErrorString*, const Inspector::TypeBuilder::Canvas::TraceLogId&, const int*, const int*, RefPtr<Inspector::TypeBuilder::Canvas::TraceLog>&);
    virtual void replayTraceLog(ErrorString*, const Inspector::TypeBuilder::Canvas::TraceLogId&, int, RefPtr<Inspector::TypeBuilder::Canvas::ResourceState>&);
    virtual void getResourceInfo(ErrorString*, const Inspector::TypeBuilder::Canvas::ResourceId&, RefPtr<Inspector::TypeBuilder::Canvas::ResourceInfo>&);
    virtual void getResourceState(ErrorString*, const Inspector::TypeBuilder::Canvas::TraceLogId&, const Inspector::TypeBuilder::Canvas::ResourceId&, RefPtr<Inspector::TypeBuilder::Canvas::ResourceState>&);

private:
    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, JSC::ExecState*);
    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, const Deprecated::ScriptObject&);
    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, const String&);

    void findFramesWithUninstrumentedCanvases();
    bool checkIsEnabled(ErrorString*) const;
    Deprecated::ScriptObject notifyRenderingContextWasWrapped(const Deprecated::ScriptObject&);

    InspectorPageAgent* m_pageAgent;
    Inspector::InjectedScriptManager* m_injectedScriptManager;
    std::unique_ptr<Inspector::InspectorCanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorCanvasBackendDispatcher> m_backendDispatcher;
    bool m_enabled;
    // Contains all frames with canvases, value is true only for frames that have an uninstrumented canvas.
    typedef HashMap<Frame*, bool> FramesWithUninstrumentedCanvases;
    FramesWithUninstrumentedCanvases m_framesWithUninstrumentedCanvases;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorCanvasAgent_h)

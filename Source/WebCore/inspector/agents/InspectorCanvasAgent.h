/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "CallTracerTypes.h"
#include "CanvasBase.h"
#include "InspectorCanvas.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <initializer_list>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "InspectorShaderProgram.h"
#endif

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class CanvasRenderingContext;
#if ENABLE(WEBGL)
class WebGLProgram;
class WebGLRenderingContextBase;
#endif

typedef String ErrorString;

class InspectorCanvasAgent final : public InspectorAgentBase, public Inspector::CanvasBackendDispatcherHandler, public CanvasObserver {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorCanvasAgent(PageAgentContext&);
    virtual ~InspectorCanvasAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);
    void discardAgent();

    // CanvasBackendDispatcherHandler
    void enable(ErrorString&);
    void disable(ErrorString&);
    void requestNode(ErrorString&, const String& canvasId, int* nodeId);
    void requestContent(ErrorString&, const String& canvasId, String* content);
    void requestCSSCanvasClientNodes(ErrorString&, const String& canvasId, RefPtr<JSON::ArrayOf<int>>&);
    void resolveCanvasContext(ErrorString&, const String& canvasId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>&);
    void setRecordingAutoCaptureFrameCount(ErrorString&, int count);
    void startRecording(ErrorString&, const String& canvasId, const int* frameCount, const int* memoryLimit);
    void stopRecording(ErrorString&, const String& canvasId);
    void requestShaderSource(ErrorString&, const String& programId, const String& shaderType, String*);
    void updateShader(ErrorString&, const String& programId, const String& shaderType, const String& source);
    void setShaderProgramDisabled(ErrorString&, const String& programId, bool disabled);
    void setShaderProgramHighlighted(ErrorString&, const String& programId, bool highlighted);

    // CanvasObserver
    void canvasChanged(CanvasBase&, const FloatRect&);
    void canvasResized(CanvasBase&) { }
    void canvasDestroyed(CanvasBase&);

    // InspectorInstrumentation
    void frameNavigated(Frame&);
    void didChangeCSSCanvasClientNodes(CanvasBase&);
    void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    void willDestroyCanvasRenderingContext(CanvasRenderingContext&);
    void didChangeCanvasMemory(CanvasRenderingContext&);
    void recordCanvasAction(CanvasRenderingContext&, const String&, std::initializer_list<RecordCanvasActionVariant>&& = { });
    void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
    void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::ExecState&, JSC::JSObject* options);
#if ENABLE(WEBGL)
    void didEnableExtension(WebGLRenderingContextBase&, const String&);
    void didCreateProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDeleteProgram(WebGLProgram&);
    bool isShaderProgramDisabled(WebGLProgram&);
    bool isShaderProgramHighlighted(WebGLProgram&);
#endif

private:
    struct RecordingOptions {
        Optional<long> frameCount;
        Optional<long> memoryLimit;
        Optional<String> name;
    };
    void startRecording(InspectorCanvas&, Inspector::Protocol::Recording::Initiator, RecordingOptions&& = { });

    void canvasDestroyedTimerFired();
    void clearCanvasData();
    InspectorCanvas& bindCanvas(CanvasRenderingContext&, bool captureBacktrace);
    String unbindCanvas(InspectorCanvas&);
    RefPtr<InspectorCanvas> assertInspectorCanvas(ErrorString&, const String& canvasId);
    RefPtr<InspectorCanvas> findInspectorCanvas(CanvasRenderingContext&);
#if ENABLE(WEBGL)
    String unbindProgram(InspectorShaderProgram&);
    RefPtr<InspectorShaderProgram> assertInspectorProgram(ErrorString&, const String& programId);
    RefPtr<InspectorShaderProgram> findInspectorProgram(WebGLProgram&);
#endif

    std::unique_ptr<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CanvasBackendDispatcher> m_backendDispatcher;

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    Page& m_inspectedPage;

    HashMap<String, RefPtr<InspectorCanvas>> m_identifierToInspectorCanvas;
#if ENABLE(WEBGL)
    HashMap<String, RefPtr<InspectorShaderProgram>> m_identifierToInspectorProgram;
#endif
    Vector<String> m_removedCanvasIdentifiers;

    Optional<size_t> m_recordingAutoCaptureFrameCount;

    Timer m_canvasDestroyedTimer;
};

} // namespace WebCore

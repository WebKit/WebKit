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

class InspectorCanvasAgent final : public InspectorAgentBase, public CanvasObserver, public Inspector::CanvasBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorCanvasAgent(WebAgentContext&);
    virtual ~InspectorCanvasAgent() = default;

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;
    void discardAgent() override;

    // CanvasBackendDispatcherHandler
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void requestNode(ErrorString&, const String& canvasId, int* nodeId) override;
    void requestContent(ErrorString&, const String& canvasId, String* content) override;
    void requestCSSCanvasClientNodes(ErrorString&, const String& canvasId, RefPtr<JSON::ArrayOf<int>>&) override;
    void resolveCanvasContext(ErrorString&, const String& canvasId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>&) override;
    void setRecordingAutoCaptureFrameCount(ErrorString&, int count) override;
    void startRecording(ErrorString&, const String& canvasId, const int* frameCount, const int* memoryLimit) override;
    void stopRecording(ErrorString&, const String& canvasId) override;
    void requestShaderSource(ErrorString&, const String& programId, const String& shaderType, String*) override;
    void updateShader(ErrorString&, const String& programId, const String& shaderType, const String& source) override;
    void setShaderProgramDisabled(ErrorString&, const String& programId, bool disabled) override;
    void setShaderProgramHighlighted(ErrorString&, const String& programId, bool highlighted) override;

    // InspectorInstrumentation
    void frameNavigated(Frame&);
    void didChangeCSSCanvasClientNodes(CanvasBase&);
    void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    void willDestroyCanvasRenderingContext(CanvasRenderingContext&);
    void didChangeCanvasMemory(CanvasRenderingContext&);
    void recordCanvasAction(CanvasRenderingContext&, const String&, Vector<RecordCanvasActionVariant>&& = { });
    void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
    void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::ExecState&, JSC::JSObject* options);
#if ENABLE(WEBGL)
    void didEnableExtension(WebGLRenderingContextBase&, const String&);
    void didCreateProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDeleteProgram(WebGLProgram&);
    bool isShaderProgramDisabled(WebGLProgram&);
    bool isShaderProgramHighlighted(WebGLProgram&);
#endif

    // CanvasObserver
    void canvasChanged(CanvasBase&, const FloatRect&) override { }
    void canvasResized(CanvasBase&) override { }
    void canvasDestroyed(CanvasBase&) override;

private:
    struct RecordingOptions {
        Optional<long> frameCount;
        Optional<long> memoryLimit;
        Optional<String> name;
    };
    void startRecording(InspectorCanvas&, Inspector::Protocol::Recording::Initiator, RecordingOptions&& = { });

    void canvasDestroyedTimerFired();
    void canvasRecordingTimerFired();
    void clearCanvasData();
    InspectorCanvas& bindCanvas(CanvasRenderingContext&, bool captureBacktrace);
    String unbindCanvas(InspectorCanvas&);
    InspectorCanvas* assertInspectorCanvas(ErrorString&, const String& identifier);
    InspectorCanvas* findInspectorCanvas(CanvasRenderingContext&);
#if ENABLE(WEBGL)
    String unbindProgram(InspectorShaderProgram&);
    InspectorShaderProgram* assertInspectorProgram(ErrorString&, const String& identifier);
    InspectorShaderProgram* findInspectorProgram(WebGLProgram&);

    HashMap<String, RefPtr<InspectorShaderProgram>> m_identifierToInspectorProgram;
#endif

    std::unique_ptr<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CanvasBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;
    HashMap<String, RefPtr<InspectorCanvas>> m_identifierToInspectorCanvas;
    Vector<String> m_removedCanvasIdentifiers;
    Timer m_canvasDestroyedTimer;
    Timer m_canvasRecordingTimer;
    Optional<size_t> m_recordingAutoCaptureFrameCount;
};

} // namespace WebCore

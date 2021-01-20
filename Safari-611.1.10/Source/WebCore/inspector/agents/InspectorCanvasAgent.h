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
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class CanvasRenderingContext;
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
class InspectorShaderProgram;
#if ENABLE(WEBGL)
class WebGLProgram;
class WebGLRenderingContextBase;
#endif // ENABLE(WEBGL)
#if ENABLE(WEBGPU)
class GPUCanvasContext;
class WebGPUDevice;
class WebGPUPipeline;
class WebGPUSwapChain;
#endif // ENABLE(WEBGPU)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

class InspectorCanvasAgent final : public InspectorAgentBase, public Inspector::CanvasBackendDispatcherHandler, public CanvasObserver, public CanMakeWeakPtr<InspectorCanvasAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorCanvasAgent(PageAgentContext&);
    ~InspectorCanvasAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);
    void discardAgent();

    // CanvasBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> requestNode(const Inspector::Protocol::Canvas::CanvasId&);
    Inspector::Protocol::ErrorStringOr<String> requestContent(const Inspector::Protocol::Canvas::CanvasId&);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> requestClientNodes(const Inspector::Protocol::Canvas::CanvasId&);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveContext(const Inspector::Protocol::Canvas::CanvasId&, const String& objectGroup);
    Inspector::Protocol::ErrorStringOr<void> setRecordingAutoCaptureFrameCount(int);
    Inspector::Protocol::ErrorStringOr<void> startRecording(const Inspector::Protocol::Canvas::CanvasId&, Optional<int>&& frameCount, Optional<int>&& memoryLimit);
    Inspector::Protocol::ErrorStringOr<void> stopRecording(const Inspector::Protocol::Canvas::CanvasId&);
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    Inspector::Protocol::ErrorStringOr<String> requestShaderSource(const Inspector::Protocol::Canvas::ProgramId&, Inspector::Protocol::Canvas::ShaderType);
    Inspector::Protocol::ErrorStringOr<void> updateShader(const Inspector::Protocol::Canvas::ProgramId&, Inspector::Protocol::Canvas::ShaderType, const String& source);
#if ENABLE(WEBGL)
    Inspector::Protocol::ErrorStringOr<void> setShaderProgramDisabled(const Inspector::Protocol::Canvas::ProgramId&, bool disabled);
    Inspector::Protocol::ErrorStringOr<void> setShaderProgramHighlighted(const Inspector::Protocol::Canvas::ProgramId&, bool highlighted);
#endif // ENABLE(WEBGL)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

    // CanvasObserver
    void canvasChanged(CanvasBase&, const FloatRect&);
    void canvasResized(CanvasBase&) { }
    void canvasDestroyed(CanvasBase&);

    // InspectorInstrumentation
    void frameNavigated(Frame&);
    void didChangeCSSCanvasClientNodes(CanvasBase&);
    void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    void didChangeCanvasMemory(CanvasRenderingContext&);
    void recordCanvasAction(CanvasRenderingContext&, const String&, std::initializer_list<RecordCanvasActionVariant>&& = { });
    void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
    void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::JSGlobalObject&, JSC::JSObject* options);
    void consoleStopRecordingCanvas(CanvasRenderingContext&);
#if ENABLE(WEBGL)
    void didEnableExtension(WebGLRenderingContextBase&, const String&);
    void didCreateWebGLProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDestroyWebGLProgram(WebGLProgram&);
    bool isWebGLProgramDisabled(WebGLProgram&);
    bool isWebGLProgramHighlighted(WebGLProgram&);
#endif // ENABLE(WEBGL)
#if ENABLE(WEBGPU)
    void didCreateWebGPUDevice(WebGPUDevice&);
    void willDestroyWebGPUDevice(WebGPUDevice&);
    void willConfigureSwapChain(GPUCanvasContext&, WebGPUSwapChain&);
    void didCreateWebGPUPipeline(WebGPUDevice&, WebGPUPipeline&);
    void willDestroyWebGPUPipeline(WebGPUPipeline&);
#endif // ENABLE(WEBGPU)

private:
    struct RecordingOptions {
        Optional<long> frameCount;
        Optional<long> memoryLimit;
        Optional<String> name;
    };
    void startRecording(InspectorCanvas&, Inspector::Protocol::Recording::Initiator, RecordingOptions&& = { });

    void canvasDestroyedTimerFired();
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    void programDestroyedTimerFired();
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)
    void reset();

    InspectorCanvas& bindCanvas(CanvasRenderingContext&, bool captureBacktrace);
#if ENABLE(WEBGPU)
    InspectorCanvas& bindCanvas(WebGPUDevice&, bool captureBacktrace);
#endif // ENABLE(WEBGPU)
    void unbindCanvas(InspectorCanvas&);
    RefPtr<InspectorCanvas> assertInspectorCanvas(Inspector::Protocol::ErrorString&, const String& canvasId);
    RefPtr<InspectorCanvas> findInspectorCanvas(CanvasRenderingContext&);
#if ENABLE(WEBGPU)
    RefPtr<InspectorCanvas> findInspectorCanvas(WebGPUDevice&);
#endif // ENABLE(WEBGPU)

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    void unbindProgram(InspectorShaderProgram&);
    RefPtr<InspectorShaderProgram> assertInspectorProgram(Inspector::Protocol::ErrorString&, const String& programId);
#if ENABLE(WEBGL)
    RefPtr<InspectorShaderProgram> findInspectorProgram(WebGLProgram&);
#endif // ENABLE(WEBGL)
#if ENABLE(WEBGPU)
    RefPtr<InspectorShaderProgram> findInspectorProgram(WebGPUPipeline&);
#endif // ENABLE(WEBGPU)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

    std::unique_ptr<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CanvasBackendDispatcher> m_backendDispatcher;

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    Page& m_inspectedPage;

    HashMap<String, RefPtr<InspectorCanvas>> m_identifierToInspectorCanvas;
    Vector<String> m_removedCanvasIdentifiers;
    Timer m_canvasDestroyedTimer;

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    HashMap<String, RefPtr<InspectorShaderProgram>> m_identifierToInspectorProgram;
    Vector<String> m_removedProgramIdentifiers;
    Timer m_programDestroyedTimer;
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

    HashSet<String> m_recordingCanvasIdentifiers;

    Optional<size_t> m_recordingAutoCaptureFrameCount;
};

} // namespace WebCore

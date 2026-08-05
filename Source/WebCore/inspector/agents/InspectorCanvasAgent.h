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

#include "CanvasBase.h"
#include "CanvasObserver.h"
#include "InspectorCanvas.h"
#include "InspectorCanvasProcessedArguments.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <initializer_list>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class CanvasRenderingContext;
class GPUComputePipeline;
class GPUDevice;
class GPURenderPipeline;
class InspectorShaderProgram;
class ScriptExecutionContext;

#if ENABLE(WEBGL)
class WebGLProgram;
class WebGLRenderingContextBase;
#endif // ENABLE(WEBGL)

namespace WebGPU {
class RenderPipeline;
}

class InspectorCanvasAgent : public InspectorAgentBase, public Inspector::CanvasBackendDispatcherHandler, public CanvasObserver, public CanMakeCheckedPtr<InspectorCanvasAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorCanvasAgent);
    WTF_MAKE_TZONE_ALLOCATED(InspectorCanvasAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorCanvasAgent);
public:
    ~InspectorCanvasAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend();
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);
    void discardAgent();
    virtual bool enabled() const;

    // CanvasObserver.
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

    // CanvasBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<String> requestContent(const Inspector::Protocol::Canvas::CanvasId&);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveContext(const Inspector::Protocol::Canvas::CanvasId&, const String& objectGroup);
    Inspector::Protocol::ErrorStringOr<void> setRecordingAutoCaptureFrameCount(int);
    Inspector::Protocol::ErrorStringOr<void> startRecording(const Inspector::Protocol::Canvas::CanvasId&, std::optional<int>&& frameCount, std::optional<int>&& memoryLimit);
    Inspector::Protocol::ErrorStringOr<void> stopRecording(const Inspector::Protocol::Canvas::CanvasId&);
    Inspector::Protocol::ErrorStringOr<String> requestShaderSource(const Inspector::Protocol::Canvas::ProgramId&, Inspector::Protocol::Canvas::ShaderType);
    void updateShader(const Inspector::Protocol::Canvas::ProgramId&, Inspector::Protocol::Canvas::ShaderType, const String& source, Ref<UpdateShaderCallback>&&);
    Inspector::Protocol::ErrorStringOr<void> setShaderProgramDisabled(const Inspector::Protocol::Canvas::ProgramId&, bool disabled);
    void setShaderProgramHighlighted(const Inspector::Protocol::Canvas::ProgramId&, bool highlighted, Ref<SetShaderProgramHighlightedCallback>&&);

    // CanvasObserver
    void canvasChanged(CanvasBase&, const FloatRect&) final;
    void canvasResized(CanvasBase&) final { }
    void canvasDestroyed(CanvasBase&) final;

    // InspectorInstrumentation
    void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    void didChangeCanvasSize(CanvasRenderingContext&);
    void didChangeCanvasMemory(const CanvasRenderingContext&);
    void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
    void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::JSGlobalObject&, JSC::JSObject* options);
    void consoleStartRecordingCanvas(GPUDevice&, JSC::JSGlobalObject&, JSC::JSObject* options);
    void consoleStopRecordingCanvas(CanvasRenderingContext&);
    void consoleStopRecordingCanvas(GPUDevice&);
#if ENABLE(WEBGL)
    void didEnableExtension(WebGLRenderingContextBase&, const String&);
    void didCreateWebGLProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDestroyWebGLProgram(WebGLProgram&);
    bool isWebGLProgramDisabled(WebGLProgram&);
    bool isWebGLProgramHighlighted(WebGLProgram&);
#endif // ENABLE(WEBGL)
    void didCreateWebGPUDevice(GPUDevice&);
    void willDestroyWebGPUDevice(GPUDevice&);
    virtual void didChangeGPUDeviceClientNodes(GPUDevice&);
    void didCreateWebGPUComputePipeline(GPUDevice&, GPUComputePipeline&);
    void willDestroyWebGPUComputePipeline(GPUComputePipeline&);
    void didCreateWebGPURenderPipeline(GPUDevice&, GPURenderPipeline&);
    void willDestroyWebGPURenderPipeline(GPURenderPipeline&);
    bool isWebGPURenderPipelineDisabled(GPURenderPipeline&);
    void didFinishRecordingCanvasFrame(GPUDevice&, bool forceDispatch = false);
    RefPtr<WebGPU::RenderPipeline> renderPipelineForWebGPUHighlighting(GPURenderPipeline&, unsigned canvasColorAttachmentMask);

    void recordAction(CanvasRenderingContext&, String&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(CanvasRenderingContext&, InspectorCanvasProcessedArgument&& receiver, String&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(GPUDevice&, String&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(GPUDevice&, InspectorCanvasProcessedArgument&& receiver, String&&, InspectorCanvasProcessedArguments&& = { });

    RefPtr<InspectorCanvas> assertInspectorCanvas(Inspector::Protocol::ErrorString&, const String& canvasId);
    RefPtr<InspectorCanvas> findInspectorCanvas(const CanvasRenderingContext&);
    RefPtr<InspectorCanvas> findInspectorCanvas(const GPUDevice&);

protected:
    InspectorCanvasAgent(WebAgentContext&);

    virtual void internalEnable();
    virtual void internalDisable();

    void reset();
    void unbindCanvas(InspectorCanvas&);

    virtual Ref<Inspector::Protocol::Canvas::Canvas> buildObjectForCanvas(InspectorCanvas&, bool captureBacktrace);
    virtual bool matchesCurrentContext(ScriptExecutionContext*) const = 0;

    const UniqueRef<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;

    MemoryCompactRobinHoodHashMap<String, Ref<InspectorCanvas>> m_identifierToInspectorCanvas;

private:
    struct RecordingOptions {
        std::optional<long> frameCount;
        std::optional<long> memoryLimit;
        std::optional<String> name;
    };
    void startRecording(InspectorCanvas&, Inspector::Protocol::Recording::Initiator, RecordingOptions&& = { });
    void consoleStartRecordingCanvas(InspectorCanvas&, JSC::JSGlobalObject&, JSC::JSObject* options);
    void didFinishRecordingCanvasFrame(InspectorCanvas&, bool forceDispatch);
    void scheduleRecordingCanvasFrame(InspectorCanvas&);

    void canvasDestroyedTimerFired();
    void programDestroyedTimerFired();

    InspectorCanvas& bindCanvas(CanvasRenderingContext&, bool captureBacktrace);
    InspectorCanvas& bindCanvas(GPUDevice&, bool captureBacktrace);
    void dispatchCanvasSizeChanged(InspectorCanvas&);

    void unbindProgram(InspectorShaderProgram&);
    RefPtr<InspectorShaderProgram> assertInspectorProgram(Inspector::Protocol::ErrorString&, const String& programId);
#if ENABLE(WEBGL)
    RefPtr<InspectorShaderProgram> findInspectorProgram(WebGLProgram&);
#endif // ENABLE(WEBGL)
    RefPtr<InspectorShaderProgram> findInspectorProgram(GPUComputePipeline&);
    RefPtr<InspectorShaderProgram> findInspectorProgram(GPURenderPipeline&);

    const Ref<Inspector::CanvasBackendDispatcher> m_backendDispatcher;

    const CheckedRef<Inspector::InjectedScriptManager> m_injectedScriptManager;

    Vector<String> m_removedCanvasIdentifiers;
    Timer m_canvasDestroyedTimer;

    MemoryCompactRobinHoodHashMap<String, Ref<InspectorShaderProgram>> m_identifierToInspectorProgram;
    Vector<String> m_removedProgramIdentifiers;
    Timer m_programDestroyedTimer;

    MemoryCompactRobinHoodHashSet<String> m_recordingCanvasIdentifiers;

    std::optional<size_t> m_recordingAutoCaptureFrameCount;
};

} // namespace WebCore

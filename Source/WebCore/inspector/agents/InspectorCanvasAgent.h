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
#include <WebCore/WebGPUCppAPI.h>
#include <initializer_list>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>
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
    Inspector::Protocol::ErrorStringOr<RefPtr<StringImpl>> requestContent(StringImpl*);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveContext(StringImpl*, StringImpl* objectGroup);
    Inspector::Protocol::ErrorStringOr<void> setRecordingAutoCaptureFrameCount(int);
    Inspector::Protocol::ErrorStringOr<void> startRecording(StringImpl*, std::optional<int>&& frameCount, std::optional<int>&& memoryLimit);
    Inspector::Protocol::ErrorStringOr<void> stopRecording(StringImpl*);
    Inspector::Protocol::ErrorStringOr<RefPtr<StringImpl>> requestShaderSource(StringImpl*, Inspector::Protocol::Canvas::ShaderType);
    void updateShader(StringImpl*, Inspector::Protocol::Canvas::ShaderType, StringImpl* source, Ref<UpdateShaderCallback>&&);
    Inspector::Protocol::ErrorStringOr<void> setShaderProgramDisabled(StringImpl*, bool disabled);
    void setShaderProgramHighlighted(StringImpl*, bool highlighted, Ref<SetShaderProgramHighlightedCallback>&&);

    // CanvasObserver
    void canvasContentsWillChange(CanvasBase&, const FloatRect&) final;
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
    void didEnableExtension(WebGLRenderingContextBase&, StringImpl*);
    void didCreateWebGLProgram(WebGLRenderingContextBase&, WebGLProgram&);
    void willDestroyWebGLProgram(WebGLProgram&);
    bool isWebGLProgramDisabled(WebGLProgram&);
    bool isWebGLProgramHighlighted(WebGLProgram&);
#endif // ENABLE(WEBGL)
    void didChangeCSSCanvasClientNodes(CanvasBase&);
    void didCreateWebGPUDevice(GPUDevice&);
    void willDestroyWebGPUDevice(GPUDevice&);
    void didChangeWebGPUDeviceLabel(GPUDevice&);
    virtual void didChangeGPUDeviceClientNodes(GPUDevice&);
    void didChangeWebGPUMemory(GPUDevice&);
    void didCreateWebGPUComputePipeline(GPUDevice&, GPUComputePipeline&);
    void willDestroyWebGPUComputePipeline(GPUComputePipeline&);
    void didChangeWebGPUComputePipelineLabel(GPUComputePipeline&);
    void didCreateWebGPURenderPipeline(GPUDevice&, GPURenderPipeline&);
    void willDestroyWebGPURenderPipeline(GPURenderPipeline&);
    void didChangeWebGPURenderPipelineLabel(GPURenderPipeline&);
    bool isWebGPURenderPipelineDisabled(GPURenderPipeline&);
    void didFinishRecordingCanvasFrame(GPUDevice&, bool forceDispatch = false);
    RefPtr<::WebGPU::RenderPipeline> renderPipelineForWebGPUHighlighting(GPURenderPipeline&, unsigned canvasColorAttachmentMask);

    void recordAction(CanvasRenderingContext&, RefPtr<StringImpl>&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(CanvasRenderingContext&, InspectorCanvasProcessedArgument&& receiver, RefPtr<StringImpl>&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(GPUDevice&, RefPtr<StringImpl>&&, InspectorCanvasProcessedArguments&& = { });
    void recordAction(GPUDevice&, InspectorCanvasProcessedArgument&& receiver, RefPtr<StringImpl>&&, InspectorCanvasProcessedArguments&& = { });
    void recordActionResult(CanvasRenderingContext&, InspectorCanvasProcessedArgument&&);
    void recordActionResult(GPUDevice&, InspectorCanvasProcessedArgument&&);

    RefPtr<InspectorCanvas> assertInspectorCanvas(Inspector::Protocol::ErrorString&, StringImpl* canvasId);
    NODELETE RefPtr<InspectorCanvas> findInspectorCanvas(const CanvasRenderingContext&);
    NODELETE RefPtr<InspectorCanvas> findInspectorCanvas(const GPUDevice&);

protected:
    InspectorCanvasAgent(WebAgentContext&);

    virtual void internalEnable();
    virtual void internalDisable();

    void reset();
    void unbindCanvas(InspectorCanvas&);

    virtual Ref<Inspector::Protocol::Canvas::Canvas> buildObjectForCanvas(InspectorCanvas&, bool captureBacktrace);
    virtual bool matchesCurrentContext(ScriptExecutionContext*) const = 0;

    void dispatchNodesChanged(InspectorCanvas&);
    void dispatchCSSCanvasClientNodesChanged(InspectorCanvas&);
    void dispatchCSSCanvasNamesChanged(InspectorCanvas&);

    const UniqueRef<Inspector::CanvasFrontendDispatcher> m_frontendDispatcher;

    MemoryCompactRobinHoodHashMap<RefPtr<StringImpl>, Ref<InspectorCanvas>> m_identifierToInspectorCanvas;
    WeakHashSet<InspectorCanvas> m_pendingNodesChange;
    WeakHashSet<InspectorCanvas> m_pendingCSSCanvasClientNodesChange;

private:
    struct RecordingOptions {
        std::optional<long> frameCount;
        std::optional<long> memoryLimit;
        std::optional<RefPtr<StringImpl>> name;
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
    RefPtr<InspectorShaderProgram> assertInspectorProgram(Inspector::Protocol::ErrorString&, StringImpl* programId);
#if ENABLE(WEBGL)
    NODELETE RefPtr<InspectorShaderProgram> findInspectorProgram(WebGLProgram&);
#endif // ENABLE(WEBGL)
    NODELETE RefPtr<InspectorShaderProgram> findInspectorProgram(GPUComputePipeline&);
    NODELETE RefPtr<InspectorShaderProgram> findInspectorProgram(GPURenderPipeline&);

    const Ref<Inspector::CanvasBackendDispatcher> m_backendDispatcher;

    const CheckedRef<Inspector::InjectedScriptManager> m_injectedScriptManager;

    Vector<RefPtr<StringImpl>> m_removedCanvasIdentifiers;
    Timer m_canvasDestroyedTimer;

    MemoryCompactRobinHoodHashMap<RefPtr<StringImpl>, Ref<InspectorShaderProgram>> m_identifierToInspectorProgram;
    Vector<RefPtr<StringImpl>> m_removedProgramIdentifiers;
    Timer m_programDestroyedTimer;

    MemoryCompactRobinHoodHashSet<RefPtr<StringImpl>> m_recordingCanvasIdentifiers;

    std::optional<size_t> m_recordingAutoCaptureFrameCount;
};

} // namespace WebCore

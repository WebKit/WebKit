/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InspectorCanvasAgent.h"

#include "CSSStyleImageValue.h"
#include "CanvasBase.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "DOMMatrix2DInit.h"
#include "DOMPointInit.h"
#include "EventLoop.h"
#include "GPUCanvasContext.h"
#include "GPUComputePipeline.h"
#include "GPUDevice.h"
#include "GPURenderPipeline.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageData.h"
#include "InspectorCanvasCallTracer.h"
#include "InspectorShaderProgram.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "Path2D.h"
#include "PlaceholderRenderingContext.h"
#include "StringAdaptors.h"
#include "WebGL2RenderingContext.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArrayObject.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>


#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif
namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorCanvasAgent);

InspectorCanvasAgent::InspectorCanvasAgent(WebAgentContext& context)
    : InspectorAgentBase("Canvas"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_canvasDestroyedTimer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
    , m_programDestroyedTimer(*this, &InspectorCanvasAgent::programDestroyedTimerFired)
{
}

InspectorCanvasAgent::~InspectorCanvasAgent() = default;

void InspectorCanvasAgent::didCreateFrontendAndBackend()
{
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    std::ignore = disable();
}

void InspectorCanvasAgent::discardAgent()
{
    reset();
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::enable()
{
    if (enabled())
        return makeUnexpected("Canvas domain already enabled"_s);

    internalEnable();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::disable()
{
    internalDisable();

    return { };
}

bool InspectorCanvasAgent::enabled() const
{
    return Ref { m_instrumentingAgents.get() }->enabledCanvasAgent() == this;
}

void InspectorCanvasAgent::internalEnable()
{
    ASSERT(!enabled());

    Ref { m_instrumentingAgents.get() }->setEnabledCanvasAgent(this);

    {
        Locker locker { CanvasRenderingContext::instancesLock() };
        for (SUPPRESS_UNCOUNTED_ARG auto* context : CanvasRenderingContext::instances()) {
            if (!context->isContextThread())
                continue;
            if (!is<CanvasRenderingContext2D>(context)
                && !is<ImageBitmapRenderingContext>(context)
#if ENABLE(OFFSCREEN_CANVAS)
                && !is<OffscreenCanvasRenderingContext2D>(context)
#endif
#if ENABLE(WEBGL)
                && !is<WebGLRenderingContext>(context)
                && !is<WebGL2RenderingContext>(context)
#endif
            )
                continue;

            if (matchesCurrentContext(context->canvasBase().scriptExecutionContext()))
                bindCanvas(*context, false);
        }
    }

    Vector<WeakPtr<GPUDevice, WeakPtrImplWithEventTargetData>> devices;
    {
        Locker locker { GPUDevice::instancesLock() };
        for (SUPPRESS_UNCOUNTED_ARG auto* device : GPUDevice::instances()) {
            if (!device->isContextThread())
                continue;
            RefPtr scriptExecutionContext = device->scriptExecutionContext();
            if (!scriptExecutionContext)
                continue;
            if (matchesCurrentContext(scriptExecutionContext))
                devices.append(*device);
        }
    }
    for (auto& device : devices) {
        if (device)
            bindCanvas(*device, false);
    }

#if ENABLE(WEBGL)
    {
        Locker locker { WebGLProgram::instancesLock() };
        for (SUPPRESS_UNCOUNTED_ARG auto& [program, contextWebGLBase] : WebGLProgram::instances()) {
            if (!contextWebGLBase || !contextWebGLBase->isContextThread())
                continue;

            if (matchesCurrentContext(contextWebGLBase->canvasBase().scriptExecutionContext()))
                didCreateWebGLProgram(protect(*contextWebGLBase), protect(*program));
        }
    }
#endif

    Vector<std::pair<WeakPtr<GPUComputePipeline>, WeakPtr<GPUDevice, WeakPtrImplWithEventTargetData>>> computePipelines;
    {
        Locker locker { GPUComputePipeline::instancesLock() };
        for (SUPPRESS_UNCOUNTED_ARG auto& [pipeline, device] : GPUComputePipeline::instances()) {
            if (!device || !device->isContextThread())
                continue;
            RefPtr scriptExecutionContext = device->scriptExecutionContext();
            if (!scriptExecutionContext)
                continue;
            if (matchesCurrentContext(scriptExecutionContext))
                computePipelines.append({ *pipeline, *device });
        }
    }
    for (auto& [pipeline, device] : computePipelines) {
        if (pipeline && device)
            didCreateWebGPUComputePipeline(*device, *pipeline);
    }

    Vector<std::pair<WeakPtr<GPURenderPipeline>, WeakPtr<GPUDevice, WeakPtrImplWithEventTargetData>>> renderPipelines;
    {
        Locker locker { GPURenderPipeline::instancesLock() };
        for (SUPPRESS_UNCOUNTED_ARG auto& [pipeline, device] : GPURenderPipeline::instances()) {
            if (!device || !device->isContextThread())
                continue;
            RefPtr scriptExecutionContext = device->scriptExecutionContext();
            if (!scriptExecutionContext)
                continue;
            if (matchesCurrentContext(scriptExecutionContext))
                renderPipelines.append({ *pipeline, *device });
        }
    }
    for (auto& [pipeline, device] : renderPipelines) {
        if (pipeline && device)
            didCreateWebGPURenderPipeline(*device, *pipeline);
    }
}

void InspectorCanvasAgent::internalDisable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledCanvasAgent(nullptr);

    reset();

    m_recordingAutoCaptureFrameCount = std::nullopt;
}

Inspector::Protocol::ErrorStringOr<String> InspectorCanvasAgent::requestContent(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);
    return inspectorCanvas->getContentAsDataURL();
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> InspectorCanvasAgent::resolveContext(const Inspector::Protocol::Canvas::CanvasId& canvasId, const String& objectGroup)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    RefPtr scriptExecutionContext = inspectorCanvas->scriptExecutionContext();
    if (!scriptExecutionContext)
        return makeUnexpected("Canvas is detached from context"_s);

    auto* state = scriptExecutionContext->globalObject();
    auto injectedScript = m_injectedScriptManager->injectedScriptFor(state);
    ASSERT(!injectedScript.hasNoValue());

    JSC::JSValue value = inspectorCanvas->resolveContext(state);

    if (!value) {
        ASSERT_NOT_REACHED();
        return makeUnexpected("Internal error: unknown context of canvas for given canvasId"_s);
    }

    auto result = injectedScript.wrapObject(value, objectGroup);
    if (!result)
        return makeUnexpected("Internal error: unable to cast Context"_s);

    return result.releaseNonNull();
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::setRecordingAutoCaptureFrameCount(int count)
{
    if (count > 0)
        m_recordingAutoCaptureFrameCount = count;
    else
        m_recordingAutoCaptureFrameCount = std::nullopt;
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::startRecording(const Inspector::Protocol::Canvas::CanvasId& canvasId, std::optional<int>&& frameCount, std::optional<int>&& memoryLimit)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    if (inspectorCanvas->hasActiveInspectorCanvasCallTracer())
        return makeUnexpected("Already recording canvas"_s);

    RecordingOptions recordingOptions;
    if (frameCount)
        recordingOptions.frameCount = *frameCount;
    if (memoryLimit)
        recordingOptions.memoryLimit = *memoryLimit;
    startRecording(*inspectorCanvas, Inspector::Protocol::Recording::Initiator::Frontend, WTF::move(recordingOptions));

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::stopRecording(const Inspector::Protocol::Canvas::CanvasId& canvasId)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    if (!inspectorCanvas->hasActiveInspectorCanvasCallTracer())
        return makeUnexpected("Not recording canvas"_s);

    didFinishRecordingCanvasFrame(*inspectorCanvas, true);

    return { };
}

Inspector::Protocol::ErrorStringOr<String> InspectorCanvasAgent::requestShaderSource(const Inspector::Protocol::Canvas::ProgramId& programId, Inspector::Protocol::Canvas::ShaderType shaderType)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    auto source = inspectorProgram->requestShaderSource(shaderType);
    if (!source)
        return makeUnexpected("Missing shader of given shaderType for given programId"_s);

    return source;
}

void InspectorCanvasAgent::updateShader(const Inspector::Protocol::Canvas::ProgramId& programId, Inspector::Protocol::Canvas::ShaderType shaderType, const String& source, Ref<UpdateShaderCallback>&& callback)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram) {
        callback->sendFailure(errorString);
        return;
    }

    inspectorProgram->updateShader(shaderType, source, [callback = WTF::move(callback)](bool success) mutable {
        if (!success) {
            callback->sendFailure("Failed to update shader of given shaderType for given programId"_s);
            return;
        }
        callback->sendSuccess();
    });
}

Inspector::Protocol::ErrorStringOr<void> InspectorCanvasAgent::setShaderProgramDisabled(const Inspector::Protocol::Canvas::ProgramId& programId, bool disabled)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    if (!inspectorProgram->setDisabled(disabled))
        return makeUnexpected("Failed to disable shader for given programId"_s);

    return { };
}

void InspectorCanvasAgent::setShaderProgramHighlighted(const Inspector::Protocol::Canvas::ProgramId& programId, bool highlighted, Ref<SetShaderProgramHighlightedCallback>&& callback)
{
    Inspector::Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram) {
        callback->sendFailure(errorString);
        return;
    }

    if (!inspectorProgram->setHighlighted(highlighted)) {
        callback->sendFailure("Shader program does not support highlighting"_s);
        return;
    }

    if (!highlighted) {
        callback->sendSuccess();
        return;
    }

    inspectorProgram->prepareRenderPipelinesForHighlighting([callback = WTF::move(callback)]() mutable {
        callback->sendSuccess();
    });
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(CanvasRenderingContext& context)
{
    if (findInspectorCanvas(context)) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref inspectorCanvas = bindCanvas(context, true);

    if (m_recordingAutoCaptureFrameCount) {
        RecordingOptions recordingOptions;
        recordingOptions.frameCount = m_recordingAutoCaptureFrameCount.value();
        startRecording(inspectorCanvas, Inspector::Protocol::Recording::Initiator::AutoCapture, WTF::move(recordingOptions));
    }
}

void InspectorCanvasAgent::didChangeCanvasSize(CanvasRenderingContext& context)
{
    RefPtr<InspectorCanvas> inspectorCanvas;
    if (WeakPtr gpuCanvasContext = dynamicDowncast<GPUCanvasContext>(context)) {
        WeakPtr device = gpuCanvasContext->device();
        if (!device)
            return;
        inspectorCanvas = findInspectorCanvas(*device);
    } else
        inspectorCanvas = findInspectorCanvas(context);

    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    dispatchCanvasSizeChanged(*inspectorCanvas);
}

void InspectorCanvasAgent::didChangeCanvasMemory(const CanvasRenderingContext& context)
{
    RefPtr<InspectorCanvas> inspectorCanvas;
    if (WeakPtr gpuCanvasContext = dynamicDowncast<GPUCanvasContext>(context)) {
        WeakPtr device = gpuCanvasContext->device();
        if (!device)
            return;
        inspectorCanvas = findInspectorCanvas(*device);
    } else
        inspectorCanvas = findInspectorCanvas(context);

    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), inspectorCanvas->memoryCost());
}

void InspectorCanvasAgent::canvasChanged(CanvasBase& canvasBase, const FloatRect&)
{
    RefPtr context = canvasBase.renderingContext();
    if (!context)
        return;

    auto inspectorCanvas = findInspectorCanvas(*context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    inspectorCanvas->canvasChanged();
}

void InspectorCanvasAgent::canvasDestroyed(CanvasBase& canvasBase)
{
    RefPtr context = canvasBase.renderingContext();
    if (!context)
        return;

    auto inspectorCanvas = findInspectorCanvas(*context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    unbindCanvas(*inspectorCanvas);
}

void InspectorCanvasAgent::didFinishRecordingCanvasFrame(CanvasRenderingContext& context, bool forceDispatch)
{
    if (!context.hasActiveInspectorCanvasCallTracer())
        return;

    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    didFinishRecordingCanvasFrame(*inspectorCanvas, forceDispatch);
}

void InspectorCanvasAgent::didFinishRecordingCanvasFrame(GPUDevice& device, bool forceDispatch)
{
    if (!device.hasActiveInspectorCanvasCallTracer())
        return;

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    didFinishRecordingCanvasFrame(*inspectorCanvas, forceDispatch);
}

void InspectorCanvasAgent::didFinishRecordingCanvasFrame(InspectorCanvas& inspectorCanvas, bool forceDispatch)
{
    if (!inspectorCanvas.hasRecordingData()) {
        if (forceDispatch) {
            m_frontendDispatcher->recordingFinished(inspectorCanvas.identifier(), nullptr);
            inspectorCanvas.resetRecordingData();
            ASSERT(!m_recordingCanvasIdentifiers.contains(inspectorCanvas.identifier()));
        }
        return;
    }

    if (forceDispatch)
        inspectorCanvas.markCurrentFrameIncomplete();

    inspectorCanvas.finalizeFrame();
    if (inspectorCanvas.currentFrameHasData())
        m_frontendDispatcher->recordingProgress(inspectorCanvas.identifier(), inspectorCanvas.releaseFrames(), inspectorCanvas.bufferUsed());

    if (!forceDispatch && inspectorCanvas.hasBufferSpace() && !inspectorCanvas.overFrameCount())
        return;

    m_frontendDispatcher->recordingFinished(inspectorCanvas.identifier(), inspectorCanvas.releaseObjectForRecording());

    m_recordingCanvasIdentifiers.remove(inspectorCanvas.identifier());
}

void InspectorCanvasAgent::consoleStartRecordingCanvas(CanvasRenderingContext& context, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    consoleStartRecordingCanvas(*inspectorCanvas, exec, options);
}

void InspectorCanvasAgent::consoleStartRecordingCanvas(GPUDevice& device, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    RefPtr inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    consoleStartRecordingCanvas(*inspectorCanvas, exec, options);
}

void InspectorCanvasAgent::consoleStartRecordingCanvas(InspectorCanvas& inspectorCanvas, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    RecordingOptions recordingOptions;
    if (options) {
        JSC::VM& vm = exec.vm();
        if (JSC::JSValue optionSingleFrame = options->get(&exec, JSC::Identifier::fromString(vm, "singleFrame"_s)); !optionSingleFrame.isUndefined())
            recordingOptions.frameCount = optionSingleFrame.toBoolean(&exec) ? 1 : 0;
        if (JSC::JSValue optionFrameCount = options->get(&exec, JSC::Identifier::fromString(vm, "frameCount"_s)); !optionFrameCount.isUndefined())
            recordingOptions.frameCount = optionFrameCount.toNumber(&exec);
        if (JSC::JSValue optionMemoryLimit = options->get(&exec, JSC::Identifier::fromString(vm, "memoryLimit"_s)); !optionMemoryLimit.isUndefined())
            recordingOptions.memoryLimit = optionMemoryLimit.toNumber(&exec);
        if (JSC::JSValue optionName = options->get(&exec, JSC::Identifier::fromString(vm, "name"_s)); !optionName.isUndefined())
            recordingOptions.name = optionName.toWTFString(&exec);
    }
    startRecording(inspectorCanvas, Inspector::Protocol::Recording::Initiator::Console, WTF::move(recordingOptions));
}

void InspectorCanvasAgent::consoleStopRecordingCanvas(CanvasRenderingContext& context)
{
    didFinishRecordingCanvasFrame(context, true);
}

void InspectorCanvasAgent::consoleStopRecordingCanvas(GPUDevice& device)
{
    didFinishRecordingCanvasFrame(device, true);
}

#if ENABLE(WEBGL)

void InspectorCanvasAgent::didEnableExtension(WebGLRenderingContextBase& context, const String& extension)
{
    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->extensionEnabled(inspectorCanvas->identifier(), extension);
}

void InspectorCanvasAgent::didCreateWebGLProgram(WebGLRenderingContextBase& context, WebGLProgram& program)
{
    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    auto inspectorProgramRef = InspectorShaderProgram::create(program, *inspectorCanvas);
    Ref inspectorProgram = inspectorProgramRef.get();
    m_identifierToInspectorProgram.set(inspectorProgram->identifier(), WTF::move(inspectorProgramRef));
    m_frontendDispatcher->programCreated(inspectorProgram->buildObjectForShaderProgram());
}

void InspectorCanvasAgent::willDestroyWebGLProgram(WebGLProgram& program)
{
    auto inspectorProgram = findInspectorProgram(program);
    if (!inspectorProgram)
        return;

    unbindProgram(*inspectorProgram);
}

bool InspectorCanvasAgent::isWebGLProgramDisabled(WebGLProgram& program)
{
    auto inspectorProgram = findInspectorProgram(program);
    ASSERT(inspectorProgram);
    if (!inspectorProgram)
        return false;

    return inspectorProgram->disabled();
}

bool InspectorCanvasAgent::isWebGLProgramHighlighted(WebGLProgram& program)
{
    auto inspectorProgram = findInspectorProgram(program);
    ASSERT(inspectorProgram);
    if (!inspectorProgram)
        return false;

    return inspectorProgram->highlighted();
}

#endif // ENABLE(WEBGL)

void InspectorCanvasAgent::didCreateWebGPUDevice(GPUDevice& device)
{
    if (findInspectorCanvas(device)) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref inspectorCanvas = bindCanvas(device, true);

    if (m_recordingAutoCaptureFrameCount) {
        RecordingOptions recordingOptions;
        recordingOptions.frameCount = m_recordingAutoCaptureFrameCount.value();
        startRecording(inspectorCanvas, Inspector::Protocol::Recording::Initiator::AutoCapture, WTF::move(recordingOptions));
    }
}

void InspectorCanvasAgent::willDestroyWebGPUDevice(GPUDevice& device)
{
    RefPtr inspectorCanvas = findInspectorCanvas(device);
    if (!inspectorCanvas)
        return;

    unbindCanvas(*inspectorCanvas);
}

void InspectorCanvasAgent::didChangeGPUDeviceClientNodes(GPUDevice& device)
{
    RefPtr inspectorCanvas = findInspectorCanvas(device);
    if (!inspectorCanvas)
        return;

    dispatchCanvasSizeChanged(*inspectorCanvas);
}

void InspectorCanvasAgent::didChangeWebGPUMemory(GPUDevice& device)
{
    RefPtr inspectorCanvas = findInspectorCanvas(device);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), inspectorCanvas->memoryCost());
}

void InspectorCanvasAgent::didCreateWebGPUComputePipeline(GPUDevice& device, GPUComputePipeline& pipeline)
{
    auto inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    auto inspectorProgramRef = InspectorShaderProgram::create(pipeline, *inspectorCanvas);
    Ref inspectorProgram = inspectorProgramRef.get();
    m_identifierToInspectorProgram.set(inspectorProgram->identifier(), WTF::move(inspectorProgramRef));
    m_frontendDispatcher->programCreated(inspectorProgram->buildObjectForShaderProgram());
}

void InspectorCanvasAgent::willDestroyWebGPUComputePipeline(GPUComputePipeline& pipeline)
{
    auto inspectorProgram = findInspectorProgram(pipeline);
    if (!inspectorProgram)
        return;

    unbindProgram(*inspectorProgram);
}

void InspectorCanvasAgent::didCreateWebGPURenderPipeline(GPUDevice& device, GPURenderPipeline& pipeline)
{
    auto inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    auto inspectorProgramRef = InspectorShaderProgram::create(pipeline, *inspectorCanvas);
    Ref inspectorProgram = inspectorProgramRef.get();
    m_identifierToInspectorProgram.set(inspectorProgram->identifier(), WTF::move(inspectorProgramRef));
    m_frontendDispatcher->programCreated(inspectorProgram->buildObjectForShaderProgram());
}

void InspectorCanvasAgent::willDestroyWebGPURenderPipeline(GPURenderPipeline& pipeline)
{
    auto inspectorProgram = findInspectorProgram(pipeline);
    if (!inspectorProgram)
        return;

    unbindProgram(*inspectorProgram);
}

bool InspectorCanvasAgent::isWebGPURenderPipelineDisabled(GPURenderPipeline& pipeline)
{
    RefPtr inspectorProgram = findInspectorProgram(pipeline);
    ASSERT(inspectorProgram);
    if (!inspectorProgram)
        return false;

    return inspectorProgram->disabled();
}

RefPtr<WebGPU::RenderPipeline> InspectorCanvasAgent::renderPipelineForWebGPUHighlighting(GPURenderPipeline& pipeline, unsigned canvasColorAttachmentMask)
{
    RefPtr inspectorProgram = findInspectorProgram(pipeline);
    ASSERT(inspectorProgram);
    if (!inspectorProgram)
        return nullptr;
    return inspectorProgram->renderPipelineForHighlighting(canvasColorAttachmentMask);
}

void InspectorCanvasAgent::recordAction(CanvasRenderingContext& canvasRenderingContext, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(canvasRenderingContext.hasActiveInspectorCanvasCallTracer());

    auto inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);

    scheduleRecordingCanvasFrame(*inspectorCanvas);
    inspectorCanvas->recordAction(WTF::move(name), WTF::move(arguments));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(canvasRenderingContext, true);
}

void InspectorCanvasAgent::recordAction(CanvasRenderingContext& canvasRenderingContext, InspectorCanvasProcessedArgument&& receiver, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(canvasRenderingContext.hasActiveInspectorCanvasCallTracer());

    RefPtr inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);

    scheduleRecordingCanvasFrame(*inspectorCanvas);
    inspectorCanvas->recordAction(WTF::move(name), WTF::move(receiver), WTF::move(arguments));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(canvasRenderingContext, true);
}

void InspectorCanvasAgent::recordAction(GPUDevice& device, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(device.hasActiveInspectorCanvasCallTracer());

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    scheduleRecordingCanvasFrame(*inspectorCanvas);
    inspectorCanvas->recordAction(WTF::move(name), WTF::move(arguments));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(device, true);
}

void InspectorCanvasAgent::recordAction(GPUDevice& device, InspectorCanvasProcessedArgument&& receiver, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(device.hasActiveInspectorCanvasCallTracer());

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    scheduleRecordingCanvasFrame(*inspectorCanvas);
    inspectorCanvas->recordAction(WTF::move(name), WTF::move(receiver), WTF::move(arguments));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(device, true);
}

void InspectorCanvasAgent::recordActionResult(CanvasRenderingContext& canvasRenderingContext, InspectorCanvasProcessedArgument&& result)
{
    ASSERT(canvasRenderingContext.hasActiveInspectorCanvasCallTracer());

    RefPtr inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    inspectorCanvas->recordActionResult(WTF::move(result));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(canvasRenderingContext, true);
}

void InspectorCanvasAgent::recordActionResult(GPUDevice& device, InspectorCanvasProcessedArgument&& result)
{
    ASSERT(device.hasActiveInspectorCanvasCallTracer());

    RefPtr inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    inspectorCanvas->recordActionResult(WTF::move(result));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(device, true);
}

void InspectorCanvasAgent::scheduleRecordingCanvasFrame(InspectorCanvas& inspectorCanvas)
{
    // Only enqueue one microtask for all actively recording canvases.
    if (m_recordingCanvasIdentifiers.isEmpty()) {
        if (RefPtr scriptExecutionContext = inspectorCanvas.scriptExecutionContext()) {
            scriptExecutionContext->eventLoop().queueMicrotask(scriptExecutionContext->vm(), [weakThis = WeakPtr { *this }] {
                if (!weakThis)
                    return;

                CheckedRef canvasAgent = *weakThis;

                auto identifiers = copyToVector(canvasAgent->m_recordingCanvasIdentifiers);
                for (auto& identifier : identifiers) {
                    RefPtr inspectorCanvas = canvasAgent->m_identifierToInspectorCanvas.get(identifier);
                    if (!inspectorCanvas)
                        continue;

                    if (RefPtr canvasRenderingContext = inspectorCanvas->canvasContext(); canvasRenderingContext && canvasRenderingContext->hasActiveInspectorCanvasCallTracer())
                        canvasAgent->didFinishRecordingCanvasFrame(*canvasRenderingContext);
                    else if (RefPtr device = inspectorCanvas->deviceContext(); device && device->hasActiveInspectorCanvasCallTracer())
                        canvasAgent->didFinishRecordingCanvasFrame(*device);
                }

                canvasAgent->m_recordingCanvasIdentifiers.clear();
            });
        }
    }

    m_recordingCanvasIdentifiers.add(inspectorCanvas.identifier());
}

void InspectorCanvasAgent::startRecording(InspectorCanvas& inspectorCanvas, Inspector::Protocol::Recording::Initiator initiator, RecordingOptions&& recordingOptions)
{
    RefPtr context = inspectorCanvas.canvasContext();
    if (context
        && !is<CanvasRenderingContext2D>(context)
        && !is<ImageBitmapRenderingContext>(context)
#if ENABLE(OFFSCREEN_CANVAS)
        && !is<OffscreenCanvasRenderingContext2D>(context)
#endif
#if ENABLE(WEBGL)
        && !is<WebGLRenderingContext>(context)
        && !is<WebGL2RenderingContext>(context)
#endif
    )
        return;

    if (inspectorCanvas.hasActiveInspectorCanvasCallTracer())
        return;

    inspectorCanvas.resetRecordingData();
    if (recordingOptions.frameCount)
        inspectorCanvas.setFrameCount(recordingOptions.frameCount.value());
    if (recordingOptions.memoryLimit)
        inspectorCanvas.setBufferLimit(recordingOptions.memoryLimit.value());
    if (recordingOptions.name)
        inspectorCanvas.setRecordingName(recordingOptions.name.value());
    inspectorCanvas.setHasActiveInspectorCanvasCallTracer(true);

    m_frontendDispatcher->recordingStarted(inspectorCanvas.identifier(), initiator);
}

void InspectorCanvasAgent::canvasDestroyedTimerFired()
{
    if (!m_removedCanvasIdentifiers.size())
        return;

    for (auto& identifier : m_removedCanvasIdentifiers)
        m_frontendDispatcher->canvasRemoved(identifier);

    m_removedCanvasIdentifiers.clear();
}

void InspectorCanvasAgent::programDestroyedTimerFired()
{
    if (!m_removedProgramIdentifiers.size())
        return;

    for (auto& identifier : m_removedProgramIdentifiers)
        m_frontendDispatcher->programDeleted(identifier);

    m_removedProgramIdentifiers.clear();
}

void InspectorCanvasAgent::reset()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        inspectorCanvas->setHasActiveInspectorCanvasCallTracer(false);

        if (RefPtr context = inspectorCanvas->canvasContext())
            context->canvasBase().removeObserver(*this);
    }

    m_identifierToInspectorCanvas.clear();
    m_removedCanvasIdentifiers.clear();
    if (m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.stop();

    m_identifierToInspectorProgram.clear();
    m_removedProgramIdentifiers.clear();
    if (m_programDestroyedTimer.isActive())
        m_programDestroyedTimer.stop();

    m_recordingCanvasIdentifiers.clear();
}

InspectorCanvas& InspectorCanvasAgent::bindCanvas(CanvasRenderingContext& context, bool captureBacktrace)
{
    auto inspectorCanvas = InspectorCanvas::create(context);
    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), inspectorCanvas.copyRef());

    context.canvasBase().addObserver(*this);

    m_frontendDispatcher->canvasAdded(buildObjectForCanvas(inspectorCanvas, captureBacktrace));

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        auto& contextWebGL = downcast<WebGLRenderingContextBase>(context);
        if (std::optional<Vector<String>> extensions = contextWebGL.getSupportedExtensions()) {
            for (const String& extension : *extensions) {
                if (contextWebGL.extensionIsEnabled(extension))
                    m_frontendDispatcher->extensionEnabled(inspectorCanvas->identifier(), extension);
            }
        }
    }
#endif

    return inspectorCanvas.unsafeGet();
}

InspectorCanvas& InspectorCanvasAgent::bindCanvas(GPUDevice& device, bool captureBacktrace)
{
    auto inspectorCanvas = InspectorCanvas::create(device);
    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), inspectorCanvas.copyRef());

    m_frontendDispatcher->canvasAdded(buildObjectForCanvas(inspectorCanvas, captureBacktrace));

    return inspectorCanvas.unsafeGet();
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvasAgent::buildObjectForCanvas(InspectorCanvas& inspectorCanvas, bool captureBacktrace)
{
    return inspectorCanvas.buildObjectForCanvas(captureBacktrace);
}

void InspectorCanvasAgent::dispatchCanvasSizeChanged(InspectorCanvas& inspectorCanvas)
{
    RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::Size>> sizesPayload;
    auto sizes = inspectorCanvas.sizes();
    if (!sizes.isEmpty()) {
        sizesPayload = JSON::ArrayOf<Inspector::Protocol::GenericTypes::Size>::create();
        for (auto& size : sizes) {
            sizesPayload->addItem(Inspector::Protocol::GenericTypes::Size::create()
                .setWidth(size.width())
                .setHeight(size.height())
                .release());
        }
    }
    m_frontendDispatcher->canvasSizeChanged(inspectorCanvas.identifier(), WTF::move(sizesPayload));
}

void InspectorCanvasAgent::unbindCanvas(InspectorCanvas& inspectorCanvas)
{
    if (inspectorCanvas.hasActiveInspectorCanvasCallTracer())
        didFinishRecordingCanvasFrame(inspectorCanvas, true);

    if (RefPtr context = inspectorCanvas.canvasContext())
        context->canvasBase().removeObserver(*this);

    Vector<InspectorShaderProgram*> programsToRemove;
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (&inspectorProgram->canvas() == &inspectorCanvas)
            programsToRemove.append(inspectorProgram.ptr());
    }
    for (RefPtr inspectorProgram : programsToRemove)
        unbindProgram(*inspectorProgram);

    String identifier = inspectorCanvas.identifier();
    m_identifierToInspectorCanvas.remove(identifier);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedCanvasIdentifiers.append(identifier);

    if (!m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.startOneShot(0_s);
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::assertInspectorCanvas(Inspector::Protocol::ErrorString& errorString, const String& canvasId)
{
    RefPtr inspectorCanvas = m_identifierToInspectorCanvas.get(canvasId);
    if (!inspectorCanvas) {
        errorString = "Missing canvas for given canvasId"_s;
        return nullptr;
    }
    return inspectorCanvas;
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::findInspectorCanvas(const CanvasRenderingContext& context)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (inspectorCanvas->canvasContext() == &context)
            return inspectorCanvas.ptr();
    }
    return nullptr;
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::findInspectorCanvas(const GPUDevice& device)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (inspectorCanvas->deviceContext() == &device)
            return protect(inspectorCanvas);
    }
    return nullptr;
}

void InspectorCanvasAgent::unbindProgram(InspectorShaderProgram& inspectorProgram)
{
    String identifier = inspectorProgram.identifier();
    m_identifierToInspectorProgram.remove(identifier);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedProgramIdentifiers.append(identifier);

    if (!m_programDestroyedTimer.isActive())
        m_programDestroyedTimer.startOneShot(0_s);
}

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::assertInspectorProgram(Inspector::Protocol::ErrorString& errorString, const String& programId)
{
    RefPtr inspectorProgram = m_identifierToInspectorProgram.get(programId);
    if (!inspectorProgram) {
        errorString = "Missing program for given programId"_s;
        return nullptr;
    }
    return inspectorProgram;
}

#if ENABLE(WEBGL)

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::findInspectorProgram(WebGLProgram& program)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (inspectorProgram->program() == &program)
            return inspectorProgram.ptr();
    }
    return nullptr;
}

#endif // ENABLE(WEBGL)

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::findInspectorProgram(GPUComputePipeline& pipeline)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (inspectorProgram->computePipeline() == &pipeline)
            return inspectorProgram.ptr();
    }
    return nullptr;
}

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::findInspectorProgram(GPURenderPipeline& pipeline)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (inspectorProgram->renderPipeline() == &pipeline)
            return inspectorProgram.ptr();
    }
    return nullptr;
}

} // namespace WebCore

/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Element.h"
#include "EventLoop.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "InspectorDOMAgent.h"
#include "InspectorShaderProgram.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "ScriptState.h"
#include "StringAdaptors.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLProgram.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#endif

#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "GPUCanvasContext.h"
#include "WebGPUComputePipeline.h"
#include "WebGPUDevice.h"
#include "WebGPUPipeline.h"
#include "WebGPURenderPipeline.h"
#include "WebGPUSwapChain.h"
#endif

namespace WebCore {

using namespace Inspector;

InspectorCanvasAgent::InspectorCanvasAgent(PageAgentContext& context)
    : InspectorAgentBase("Canvas"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_inspectedPage(context.inspectedPage)
    , m_canvasDestroyedTimer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    , m_programDestroyedTimer(*this, &InspectorCanvasAgent::programDestroyedTimerFired)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)
{
}

InspectorCanvasAgent::~InspectorCanvasAgent() = default;

void InspectorCanvasAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString ignored;
    disable(ignored);
}

void InspectorCanvasAgent::discardAgent()
{
    reset();
}

void InspectorCanvasAgent::enable(ErrorString&)
{
    if (m_instrumentingAgents.enabledCanvasAgent() == this)
        return;

    m_instrumentingAgents.setEnabledCanvasAgent(this);

    const auto existsInCurrentPage = [&] (ScriptExecutionContext* scriptExecutionContext) {
        if (!is<Document>(scriptExecutionContext))
            return false;

        // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
        auto* document = downcast<Document>(scriptExecutionContext);
        return document->page() == &m_inspectedPage;
    };

    {
        LockHolder lock(CanvasRenderingContext::instancesMutex());
        for (auto* context : CanvasRenderingContext::instances(lock)) {
#if ENABLE(WEBGPU)
            // The actual "context" for WebGPU is the `WebGPUDevice`, not the <canvas>.
            if (is<GPUCanvasContext>(context))
                continue;
#endif // ENABLE(WEBGPU)

            if (existsInCurrentPage(context->canvasBase().scriptExecutionContext()))
                bindCanvas(*context, false);
        }
    }

#if ENABLE(WEBGPU)
    {
        LockHolder lock(WebGPUDevice::instancesMutex());
        for (auto* device : WebGPUDevice::instances(lock)) {
            if (existsInCurrentPage(device->scriptExecutionContext()))
                bindCanvas(*device, false);
        }
    }
#endif // ENABLE(WEBGPU)

#if ENABLE(WEBGL)
    {
        LockHolder lock(WebGLProgram::instancesMutex());
        for (auto& [program, contextWebGLBase] : WebGLProgram::instances(lock)) {
            if (contextWebGLBase && existsInCurrentPage(contextWebGLBase->canvasBase().scriptExecutionContext()))
                didCreateWebGLProgram(*contextWebGLBase, *program);
        }
    }
#endif // ENABLE(WEBGL)

#if ENABLE(WEBGPU)
    {
        LockHolder lock(WebGPUPipeline::instancesMutex());
        for (auto& [pipeline, device] : WebGPUPipeline::instances(lock)) {
            if (device && existsInCurrentPage(device->scriptExecutionContext()) && pipeline->isValid())
                didCreateWebGPUPipeline(*device, *pipeline);
        }
    }
#endif // ENABLE(WEBGPU)
}

void InspectorCanvasAgent::disable(ErrorString&)
{
    m_instrumentingAgents.setEnabledCanvasAgent(nullptr);

    reset();

    m_recordingAutoCaptureFrameCount = WTF::nullopt;
}

void InspectorCanvasAgent::requestNode(ErrorString& errorString, const String& canvasId, int* nodeId)
{
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    auto* node = inspectorCanvas->canvasElement();
    if (!node) {
        errorString = "Missing element of canvas for given canvasId"_s;
        return;
    }

    int documentNodeId = m_instrumentingAgents.persistentDOMAgent()->boundNodeId(&node->document());
    if (!documentNodeId) {
        errorString = "Document must have been requested"_s;
        return;
    }

    *nodeId = m_instrumentingAgents.persistentDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, node);
}

void InspectorCanvasAgent::requestContent(ErrorString& errorString, const String& canvasId, String* content)
{
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    *content = inspectorCanvas->getCanvasContentAsDataURL(errorString);
}

void InspectorCanvasAgent::requestClientNodes(ErrorString& errorString, const String& canvasId, RefPtr<JSON::ArrayOf<int>>& clientNodeIds)
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return;
    }

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    clientNodeIds = JSON::ArrayOf<int>::create();
    for (auto& clientNode : inspectorCanvas->clientNodes()) {
        if (auto documentNodeId = domAgent->boundNodeId(&clientNode->document()))
            clientNodeIds->addItem(domAgent->pushNodeToFrontend(errorString, documentNodeId, clientNode));
    }
}

void InspectorCanvasAgent::resolveContext(ErrorString& errorString, const String& canvasId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result)
{
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    auto* state = inspectorCanvas->scriptExecutionContext()->execState();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    ASSERT(!injectedScript.hasNoValue());

    JSC::JSValue value = inspectorCanvas->resolveContext(state);

    if (!value) {
        ASSERT_NOT_REACHED();
        errorString = "Internal error: unknown context of canvas for given canvasId"_s;
        return;
    }

    String objectGroupName = objectGroup ? *objectGroup : String();
    result = injectedScript.wrapObject(value, objectGroupName);
}

void InspectorCanvasAgent::setRecordingAutoCaptureFrameCount(ErrorString&, int count)
{
    if (count > 0)
        m_recordingAutoCaptureFrameCount = count;
    else
        m_recordingAutoCaptureFrameCount = WTF::nullopt;
}

void InspectorCanvasAgent::startRecording(ErrorString& errorString, const String& canvasId, const int* frameCount, const int* memoryLimit)
{
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    auto* context = inspectorCanvas->canvasContext();
    if (!context)
        return;

    if (context->callTracingActive()) {
        errorString = "Already recording canvas"_s;
        return;
    }

    RecordingOptions recordingOptions;
    if (frameCount)
        recordingOptions.frameCount = *frameCount;
    if (memoryLimit)
        recordingOptions.memoryLimit = *memoryLimit;
    startRecording(*inspectorCanvas, Inspector::Protocol::Recording::Initiator::Frontend, WTFMove(recordingOptions));
}

void InspectorCanvasAgent::stopRecording(ErrorString& errorString, const String& canvasId)
{
    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    auto* context = inspectorCanvas->canvasContext();
    if (!context)
        return;

    if (!context->callTracingActive()) {
        errorString = "Not recording canvas"_s;
        return;
    }

    didFinishRecordingCanvasFrame(*context, true);
}

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
void InspectorCanvasAgent::requestShaderSource(ErrorString& errorString, const String& programId, const String& shaderTypeString, String* outSource)
{
    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    auto shaderType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::Canvas::ShaderType>(shaderTypeString);
    if (!shaderType) {
        errorString = makeString("Unknown shaderType: "_s, shaderTypeString);
        return;
    }

    auto source = inspectorProgram->requestShaderSource(shaderType.value());
    if (!source) {
        errorString = "Missing shader of given shaderType for given programId"_s;
        return;
    }

    *outSource = source;
}

void InspectorCanvasAgent::updateShader(ErrorString& errorString, const String& programId, const String& shaderTypeString, const String& source)
{
    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    auto shaderType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::Canvas::ShaderType>(shaderTypeString);
    if (!shaderType) {
        errorString = makeString("Unknown shaderType: "_s, shaderTypeString);
        return;
    }

    if (!inspectorProgram->updateShader(shaderType.value(), source))
        errorString = "Failed to update shader of given shaderType for given programId"_s;
}

#if ENABLE(WEBGL)
void InspectorCanvasAgent::setShaderProgramDisabled(ErrorString& errorString, const String& programId, bool disabled)
{
    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    inspectorProgram->setDisabled(disabled);
}

void InspectorCanvasAgent::setShaderProgramHighlighted(ErrorString& errorString, const String& programId, bool highlighted)
{
    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    inspectorProgram->setHighlighted(highlighted);
}
#endif // ENABLE(WEBGL)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

void InspectorCanvasAgent::frameNavigated(Frame& frame)
{
    if (frame.isMainFrame()) {
        reset();
        return;
    }

    Vector<InspectorCanvas*> inspectorCanvases;
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (auto* canvasElement = inspectorCanvas->canvasElement()) {
            if (canvasElement->document().frame() == &frame)
                inspectorCanvases.append(inspectorCanvas.get());
        }
    }

    for (auto* inspectorCanvas : inspectorCanvases)
        unbindCanvas(*inspectorCanvas);
}

void InspectorCanvasAgent::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    auto* context = canvasBase.renderingContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto inspectorCanvas = findInspectorCanvas(*context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->clientNodesChanged(inspectorCanvas->identifier());
}

void InspectorCanvasAgent::didCreateCanvasRenderingContext(CanvasRenderingContext& context)
{
    if (findInspectorCanvas(context)) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& inspectorCanvas = bindCanvas(context, true);

    if (m_recordingAutoCaptureFrameCount) {
        RecordingOptions recordingOptions;
        recordingOptions.frameCount = m_recordingAutoCaptureFrameCount.value();
        startRecording(inspectorCanvas, Inspector::Protocol::Recording::Initiator::AutoCapture, WTFMove(recordingOptions));
    }
}

void InspectorCanvasAgent::didChangeCanvasMemory(CanvasRenderingContext& context)
{
    RefPtr<InspectorCanvas> inspectorCanvas;

#if ENABLE(WEBGPU)
    if (is<GPUCanvasContext>(context)) {
        for (auto& item : m_identifierToInspectorCanvas.values()) {
            if (item->isDeviceForCanvasContext(context)) {
                inspectorCanvas = item;
                break;
            }
        }
    }
#endif

    if (!inspectorCanvas)
        inspectorCanvas = findInspectorCanvas(context);

    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = inspectorCanvas->canvasElement())
        m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), node->memoryCost());
}

void InspectorCanvasAgent::recordCanvasAction(CanvasRenderingContext& canvasRenderingContext, const String& name, std::initializer_list<RecordCanvasActionVariant>&& parameters)
{
    auto inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    ASSERT(canvasRenderingContext.callTracingActive());
    if (!canvasRenderingContext.callTracingActive())
        return;

    // Only enqueue one microtask for all actively recording canvases.
    if (m_recordingCanvasIdentifiers.isEmpty()) {
        if (auto* scriptExecutionContext = inspectorCanvas->scriptExecutionContext()) {
            scriptExecutionContext->eventLoop().queueMicrotask([weakThis = makeWeakPtr(*this)] {
                if (!weakThis)
                    return;

                auto& canvasAgent = *weakThis;

                auto identifiers = copyToVector(canvasAgent.m_recordingCanvasIdentifiers);
                for (auto& identifier : identifiers) {
                    auto inspectorCanvas = canvasAgent.m_identifierToInspectorCanvas.get(identifier);
                    if (!inspectorCanvas)
                        continue;

                    auto* canvasRenderingContext = inspectorCanvas->canvasContext();
                    ASSERT(canvasRenderingContext);
                    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

                    if (canvasRenderingContext->callTracingActive())
                        canvasAgent.didFinishRecordingCanvasFrame(*canvasRenderingContext);
                }

                canvasAgent.m_recordingCanvasIdentifiers.clear();
            });
        }
    }

    m_recordingCanvasIdentifiers.add(inspectorCanvas->identifier());

    inspectorCanvas->recordAction(name, WTFMove(parameters));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(canvasRenderingContext, true);
}

void InspectorCanvasAgent::canvasChanged(CanvasBase& canvasBase, const FloatRect&)
{
    auto* context = canvasBase.renderingContext();
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
    auto* context = canvasBase.renderingContext();
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
    if (!context.callTracingActive())
        return;

    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    if (!inspectorCanvas->hasRecordingData()) {
        if (forceDispatch) {
            m_frontendDispatcher->recordingFinished(inspectorCanvas->identifier(), nullptr);
            inspectorCanvas->resetRecordingData();
            ASSERT(!m_recordingCanvasIdentifiers.contains(inspectorCanvas->identifier()));
        }
        return;
    }

    if (forceDispatch)
        inspectorCanvas->markCurrentFrameIncomplete();

    inspectorCanvas->finalizeFrame();
    if (inspectorCanvas->currentFrameHasData())
        m_frontendDispatcher->recordingProgress(inspectorCanvas->identifier(), inspectorCanvas->releaseFrames(), inspectorCanvas->bufferUsed());

    if (!forceDispatch && !inspectorCanvas->overFrameCount())
        return;

    m_frontendDispatcher->recordingFinished(inspectorCanvas->identifier(), inspectorCanvas->releaseObjectForRecording());

    m_recordingCanvasIdentifiers.remove(inspectorCanvas->identifier());
}

void InspectorCanvasAgent::consoleStartRecordingCanvas(CanvasRenderingContext& context, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    auto inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    RecordingOptions recordingOptions;
    if (options) {
        JSC::VM& vm = exec.vm();
        if (JSC::JSValue optionSingleFrame = options->get(&exec, JSC::Identifier::fromString(vm, "singleFrame")))
            recordingOptions.frameCount = optionSingleFrame.toBoolean(&exec) ? 1 : 0;
        if (JSC::JSValue optionFrameCount = options->get(&exec, JSC::Identifier::fromString(vm, "frameCount")))
            recordingOptions.frameCount = optionFrameCount.toNumber(&exec);
        if (JSC::JSValue optionMemoryLimit = options->get(&exec, JSC::Identifier::fromString(vm, "memoryLimit")))
            recordingOptions.memoryLimit = optionMemoryLimit.toNumber(&exec);
        if (JSC::JSValue optionName = options->get(&exec, JSC::Identifier::fromString(vm, "name")))
            recordingOptions.name = optionName.toWTFString(&exec);
    }
    startRecording(*inspectorCanvas, Inspector::Protocol::Recording::Initiator::Console, WTFMove(recordingOptions));
}

void InspectorCanvasAgent::consoleStopRecordingCanvas(CanvasRenderingContext& context)
{
    didFinishRecordingCanvasFrame(context, true);
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
    auto& inspectorProgram = inspectorProgramRef.get();
    m_identifierToInspectorProgram.set(inspectorProgram.identifier(), WTFMove(inspectorProgramRef));
    m_frontendDispatcher->programCreated(inspectorProgram.buildObjectForShaderProgram());
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

#if ENABLE(WEBGPU)
void InspectorCanvasAgent::didCreateWebGPUDevice(WebGPUDevice& device)
{
    if (findInspectorCanvas(device)) {
        ASSERT_NOT_REACHED();
        return;
    }

    bindCanvas(device, true);
}

void InspectorCanvasAgent::willDestroyWebGPUDevice(WebGPUDevice& device)
{
    auto inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    unbindCanvas(*inspectorCanvas);
}

void InspectorCanvasAgent::willConfigureSwapChain(GPUCanvasContext& contextGPU, WebGPUSwapChain& newSwapChain)
{
    auto notifyDeviceForSwapChain = [&] (WebGPUSwapChain& webGPUSwapChain) {
        for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
            if (auto* device = inspectorCanvas->deviceContext()) {
                if (device->device().swapChain() == webGPUSwapChain.swapChain())
                    m_frontendDispatcher->clientNodesChanged(inspectorCanvas->identifier());
            }
        }
    };

    if (auto* existingSwapChain = contextGPU.swapChain())
        notifyDeviceForSwapChain(*existingSwapChain);

    notifyDeviceForSwapChain(newSwapChain);
}

void InspectorCanvasAgent::didCreateWebGPUPipeline(WebGPUDevice& device, WebGPUPipeline& pipeline)
{
    auto inspectorCanvas = findInspectorCanvas(device);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    ASSERT(pipeline.isValid());

    auto inspectorProgramRef = InspectorShaderProgram::create(pipeline, *inspectorCanvas);
    auto& inspectorProgram = inspectorProgramRef.get();
    m_identifierToInspectorProgram.set(inspectorProgram.identifier(), WTFMove(inspectorProgramRef));
    m_frontendDispatcher->programCreated(inspectorProgram.buildObjectForShaderProgram());
}

void InspectorCanvasAgent::willDestroyWebGPUPipeline(WebGPUPipeline& pipeline)
{
    auto inspectorProgram = findInspectorProgram(pipeline);
    if (!inspectorProgram)
        return;

    unbindProgram(*inspectorProgram);
}
#endif // ENABLE(WEBGPU)

void InspectorCanvasAgent::startRecording(InspectorCanvas& inspectorCanvas, Inspector::Protocol::Recording::Initiator initiator, RecordingOptions&& recordingOptions)
{
    auto* context = inspectorCanvas.canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    if (!is<CanvasRenderingContext2D>(context)
        && !is<ImageBitmapRenderingContext>(context)
#if ENABLE(WEBGL)
        && !is<WebGLRenderingContext>(context)
#endif // ENABLE(WEBGL)
#if ENABLE(WEBGL2)
        && !is<WebGL2RenderingContext>(context)
#endif // ENABLE(WEBGL2)
    )
        return;

    if (context->callTracingActive())
        return;

    inspectorCanvas.resetRecordingData();
    if (recordingOptions.frameCount)
        inspectorCanvas.setFrameCount(recordingOptions.frameCount.value());
    if (recordingOptions.memoryLimit)
        inspectorCanvas.setBufferLimit(recordingOptions.memoryLimit.value());
    if (recordingOptions.name)
        inspectorCanvas.setRecordingName(recordingOptions.name.value());
    context->setCallTracingActive(true);

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

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
void InspectorCanvasAgent::programDestroyedTimerFired()
{
    if (!m_removedProgramIdentifiers.size())
        return;

    for (auto& identifier : m_removedProgramIdentifiers)
        m_frontendDispatcher->programDeleted(identifier);

    m_removedProgramIdentifiers.clear();
}
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

void InspectorCanvasAgent::reset()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (auto* context = inspectorCanvas->canvasContext())
            context->canvasBase().removeObserver(*this);
    }

    m_identifierToInspectorCanvas.clear();
    m_removedCanvasIdentifiers.clear();
    if (m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.stop();

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
    m_identifierToInspectorProgram.clear();
    m_removedProgramIdentifiers.clear();
    if (m_programDestroyedTimer.isActive())
        m_programDestroyedTimer.stop();
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

    m_recordingCanvasIdentifiers.clear();
}

InspectorCanvas& InspectorCanvasAgent::bindCanvas(CanvasRenderingContext& context, bool captureBacktrace)
{
    auto inspectorCanvas = InspectorCanvas::create(context);
    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), inspectorCanvas.copyRef());

    context.canvasBase().addObserver(*this);

    m_frontendDispatcher->canvasAdded(inspectorCanvas->buildObjectForCanvas(captureBacktrace));

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        auto& contextWebGL = downcast<WebGLRenderingContextBase>(context);
        if (Optional<Vector<String>> extensions = contextWebGL.getSupportedExtensions()) {
            for (const String& extension : *extensions) {
                if (contextWebGL.extensionIsEnabled(extension))
                    m_frontendDispatcher->extensionEnabled(inspectorCanvas->identifier(), extension);
            }
        }
    }
#endif

    return inspectorCanvas;
}

#if ENABLE(WEBGPU)
InspectorCanvas& InspectorCanvasAgent::bindCanvas(WebGPUDevice& device, bool captureBacktrace)
{
    auto inspectorCanvas = InspectorCanvas::create(device);
    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), inspectorCanvas.copyRef());

    m_frontendDispatcher->canvasAdded(inspectorCanvas->buildObjectForCanvas(captureBacktrace));

    return inspectorCanvas;
}
#endif // ENABLE(WEBGPU)

void InspectorCanvasAgent::unbindCanvas(InspectorCanvas& inspectorCanvas)
{
#if ENABLE(WEBGL)
    Vector<InspectorShaderProgram*> programsToRemove;
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (&inspectorProgram->canvas() == &inspectorCanvas)
            programsToRemove.append(inspectorProgram.get());
    }

    for (auto* inspectorProgram : programsToRemove)
        unbindProgram(*inspectorProgram);
#endif // ENABLE(WEBGL)

    if (auto* context = inspectorCanvas.canvasContext())
        context->canvasBase().removeObserver(*this);

    String identifier = inspectorCanvas.identifier();
    m_identifierToInspectorCanvas.remove(identifier);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedCanvasIdentifiers.append(identifier);

    if (!m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.startOneShot(0_s);
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::assertInspectorCanvas(ErrorString& errorString, const String& canvasId)
{
    auto inspectorCanvas = m_identifierToInspectorCanvas.get(canvasId);
    if (!inspectorCanvas) {
        errorString = "Missing canvas for given canvasId"_s;
        return nullptr;
    }
    return inspectorCanvas;
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::findInspectorCanvas(CanvasRenderingContext& context)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (inspectorCanvas->canvasContext() == &context)
            return inspectorCanvas;
    }
    return nullptr;
}

#if ENABLE(WEBGPU)
RefPtr<InspectorCanvas> InspectorCanvasAgent::findInspectorCanvas(WebGPUDevice& device)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (inspectorCanvas->deviceContext() == &device)
            return inspectorCanvas;
    }
    return nullptr;
}
#endif // ENABLE(WEBGPU)

#if ENABLE(WEBGL) || ENABLE(WEBGPU)
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

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::assertInspectorProgram(ErrorString& errorString, const String& programId)
{
    auto inspectorProgram = m_identifierToInspectorProgram.get(programId);
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
            return inspectorProgram;
    }
    return nullptr;
}
#endif // ENABLE(WEBGL)

#if ENABLE(WEBGPU)
RefPtr<InspectorShaderProgram> InspectorCanvasAgent::findInspectorProgram(WebGPUPipeline& pipeline)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (inspectorProgram->pipeline() == &pipeline)
            return inspectorProgram;
    }
    return nullptr;
}
#endif // ENABLE(WEBGPU)
#endif // ENABLE(WEBGL) || ENABLE(WEBGPU)

} // namespace WebCore

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

#include "config.h"
#include "InspectorCanvasAgent.h"

#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "ImageBitmapRenderingContext.h"
#include "InstrumentingAgents.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSExecState.h"
#include "JSImageBitmapRenderingContext.h"
#include "OffscreenCanvas.h"
#include "ScriptState.h"
#include "StringAdaptors.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#include "WebGLProgram.h"
#include "WebGLShader.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "JSGPUCanvasContext.h"
#endif

#if ENABLE(WEBMETAL)
#include "JSWebMetalRenderingContext.h"
#endif


namespace WebCore {

using namespace Inspector;

InspectorCanvasAgent::InspectorCanvasAgent(WebAgentContext& context)
    : InspectorAgentBase("Canvas"_s, context)
    , m_frontendDispatcher(std::make_unique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_canvasDestroyedTimer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
    , m_canvasRecordingTimer(*this, &InspectorCanvasAgent::canvasRecordingTimerFired)
{
}

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
    clearCanvasData();
}

void InspectorCanvasAgent::enable(ErrorString&)
{
    if (m_instrumentingAgents.inspectorCanvasAgent() == this)
        return;

    m_instrumentingAgents.setInspectorCanvasAgent(this);

    const auto canvasExistsInCurrentPage = [&] (CanvasRenderingContext* canvasRenderingContext) {
        if (!canvasRenderingContext)
            return false;

        auto* scriptExecutionContext = canvasRenderingContext->canvasBase().scriptExecutionContext();
        if (!is<Document>(scriptExecutionContext))
            return false;

        if (auto* inspectorPageAgent = m_instrumentingAgents.inspectorPageAgent()) {
            // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
            auto* document = downcast<Document>(scriptExecutionContext);
            return document->page() == &inspectorPageAgent->page();
        }

        return false;
    };

    {
        LockHolder lock(CanvasRenderingContext::instancesMutex());
        for (auto* canvasRenderingContext : CanvasRenderingContext::instances(lock)) {
            if (canvasExistsInCurrentPage(canvasRenderingContext))
                bindCanvas(*canvasRenderingContext, false);
        }
    }

#if ENABLE(WEBGL)
    {
        LockHolder lock(WebGLProgram::instancesMutex());
        for (auto& entry : WebGLProgram::instances(lock)) {
            if (canvasExistsInCurrentPage(entry.value))
                didCreateProgram(*entry.value, *entry.key);
        }
    }
#endif
}

void InspectorCanvasAgent::disable(ErrorString&)
{
    m_instrumentingAgents.setInspectorCanvasAgent(nullptr);

#if ENABLE(WEBGL)
    m_identifierToInspectorProgram.clear();
#endif

    m_identifierToInspectorCanvas.clear();
    m_removedCanvasIdentifiers.clear();

    if (m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.stop();

    if (m_canvasRecordingTimer.isActive())
        m_canvasRecordingTimer.stop();

    m_recordingAutoCaptureFrameCount = WTF::nullopt;
}

void InspectorCanvasAgent::requestNode(ErrorString& errorString, const String& canvasId, int* nodeId)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    auto* node = inspectorCanvas->canvasElement();
    if (!node) {
        errorString = "No node for canvas"_s;
        return;
    }

    int documentNodeId = m_instrumentingAgents.inspectorDOMAgent()->boundNodeId(&node->document());
    if (!documentNodeId) {
        errorString = "Document has not been requested"_s;
        return;
    }

    *nodeId = m_instrumentingAgents.inspectorDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, node);
}

void InspectorCanvasAgent::requestContent(ErrorString& errorString, const String& canvasId, String* content)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = inspectorCanvas->canvasElement()) {
        if (is<CanvasRenderingContext2D>(inspectorCanvas->context()) || is<ImageBitmapRenderingContext>(inspectorCanvas->context())) {
            auto result = node->toDataURL("image/png"_s);
            if (result.hasException()) {
                errorString = result.releaseException().releaseMessage();
                return;
            }
            *content = result.releaseReturnValue().string;
            return;
        }

#if ENABLE(WEBGL)
        if (is<WebGLRenderingContextBase>(inspectorCanvas->context())) {
            WebGLRenderingContextBase& contextWebGLBase = downcast<WebGLRenderingContextBase>(inspectorCanvas->context());

            contextWebGLBase.setPreventBufferClearForInspector(true);
            auto result = node->toDataURL("image/png"_s);
            contextWebGLBase.setPreventBufferClearForInspector(false);

            if (result.hasException()) {
                errorString = result.releaseException().releaseMessage();
                return;
            }
            *content = result.releaseReturnValue().string;
            return;
        }
#endif
    }

    // FIXME: <https://webkit.org/b/173621> Web Inspector: Support getting the content of WebMetal context;
    errorString = "Unsupported canvas context type"_s;
}

void InspectorCanvasAgent::requestCSSCanvasClientNodes(ErrorString& errorString, const String& canvasId, RefPtr<JSON::ArrayOf<int>>& result)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    result = JSON::ArrayOf<int>::create();
    for (auto* client : inspectorCanvas->context().canvasBase().cssCanvasClients()) {
        if (int documentNodeId = m_instrumentingAgents.inspectorDOMAgent()->boundNodeId(&client->document()))
            result->addItem(m_instrumentingAgents.inspectorDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, client));
    }
}

static JSC::JSValue contextAsScriptValue(JSC::ExecState& state, CanvasRenderingContext& context)
{
    JSC::JSLockHolder lock(&state);

    if (is<CanvasRenderingContext2D>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<CanvasRenderingContext2D>(context));
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebGLRenderingContext>(context));
#endif
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebGL2RenderingContext>(context));
#endif
#if ENABLE(WEBGPU)
    if (is<GPUCanvasContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<GPUCanvasContext>(context));
#endif
#if ENABLE(WEBMETAL)
    if (is<WebMetalRenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<WebMetalRenderingContext>(context));
#endif
    if (is<ImageBitmapRenderingContext>(context))
        return toJS(&state, deprecatedGlobalObjectForPrototype(&state), downcast<ImageBitmapRenderingContext>(context));

    return { };
}

void InspectorCanvasAgent::resolveCanvasContext(ErrorString& errorString, const String& canvasId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result)
{
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    auto& state = *inspectorCanvas->context().canvasBase().scriptExecutionContext()->execState();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    ASSERT(!injectedScript.hasNoValue());

    JSC::JSValue value = contextAsScriptValue(state, inspectorCanvas->context());
    if (!value) {
        ASSERT_NOT_REACHED();
        errorString = "Unknown context type"_s;
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
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    if (inspectorCanvas->context().callTracingActive()) {
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
    auto* inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return;

    if (!inspectorCanvas->context().callTracingActive()) {
        errorString = "No active recording for canvas"_s;
        return;
    }

    didFinishRecordingCanvasFrame(inspectorCanvas->context(), true);
}

void InspectorCanvasAgent::requestShaderSource(ErrorString& errorString, const String& programId, const String& shaderType, String* content)
{
#if ENABLE(WEBGL)
    auto* inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    auto* shader = inspectorProgram->shaderForType(shaderType);
    if (!shader) {
        errorString = "No shader for given type."_s;
        return;
    }

    *content = shader->getSource();
#else
    UNUSED_PARAM(programId);
    UNUSED_PARAM(shaderType);
    UNUSED_PARAM(content);
    errorString = "WebGL is not supported."_s;
#endif
}

void InspectorCanvasAgent::updateShader(ErrorString& errorString, const String& programId, const String& shaderType, const String& source)
{
#if ENABLE(WEBGL)
    auto* inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    auto* shader = inspectorProgram->shaderForType(shaderType);
    if (!shader) {
        errorString = "No shader for given type."_s;
        return;
    }

    WebGLRenderingContextBase& contextWebGL = inspectorProgram->context();
    contextWebGL.shaderSource(shader, source);
    contextWebGL.compileShader(shader);

    if (!shader->isValid()) {
        errorString = "Shader compilation failed."_s;
        return;
    }

    contextWebGL.linkProgramWithoutInvalidatingAttribLocations(&inspectorProgram->program());
#else
    UNUSED_PARAM(programId);
    UNUSED_PARAM(shaderType);
    UNUSED_PARAM(source);
    errorString = "WebGL is not supported."_s;
#endif
}

void InspectorCanvasAgent::setShaderProgramDisabled(ErrorString& errorString, const String& programId, bool disabled)
{
#if ENABLE(WEBGL)
    auto* inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    inspectorProgram->setDisabled(disabled);
#else
    UNUSED_PARAM(programId);
    UNUSED_PARAM(disabled);
    errorString = "WebGL is not supported."_s;
#endif
}

void InspectorCanvasAgent::setShaderProgramHighlighted(ErrorString& errorString, const String& programId, bool highlighted)
{
#if ENABLE(WEBGL)
    auto* inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return;

    inspectorProgram->setHighlighted(highlighted);
#else
    UNUSED_PARAM(programId);
    UNUSED_PARAM(highlighted);
    errorString = "WebGL is not supported."_s;
#endif
}

void InspectorCanvasAgent::frameNavigated(Frame& frame)
{
    if (frame.isMainFrame()) {
        clearCanvasData();
        return;
    }

    Vector<InspectorCanvas*> inspectorCanvases;
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (auto* canvasElement = inspectorCanvas->canvasElement()) {
            if (canvasElement->document().frame() == &frame)
                inspectorCanvases.append(inspectorCanvas.get());
        }
    }

    for (auto* inspectorCanvas : inspectorCanvases) {
        inspectorCanvas->context().canvasBase().removeObserver(*this);

        String identifier = unbindCanvas(*inspectorCanvas);
        m_frontendDispatcher->canvasRemoved(identifier);
    }
}

void InspectorCanvasAgent::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    auto* context = canvasBase.renderingContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* inspectorCanvas = findInspectorCanvas(*context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->cssCanvasClientNodesChanged(inspectorCanvas->identifier());
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
    auto* inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = inspectorCanvas->canvasElement())
        m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), node->memoryCost());
}

void InspectorCanvasAgent::recordCanvasAction(CanvasRenderingContext& canvasRenderingContext, const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    auto* inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    ASSERT(canvasRenderingContext.callTracingActive());
    if (!canvasRenderingContext.callTracingActive())
        return;

    inspectorCanvas->recordAction(name, WTFMove(parameters));

    if (!m_canvasRecordingTimer.isActive())
        m_canvasRecordingTimer.startOneShot(0_s);

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(inspectorCanvas->context(), true);
}

void InspectorCanvasAgent::canvasDestroyed(CanvasBase& canvasBase)
{
    auto* context = canvasBase.renderingContext();
    if (!context)
        return;

    auto* inspectorCanvas = findInspectorCanvas(*context);
    if (!inspectorCanvas)
        return;

    String identifier = unbindCanvas(*inspectorCanvas);

    // WebCore::CanvasObserver::canvasDestroyed is called in response to the GC destroying the CanvasBase.
    // Due to the single-process model used in WebKit1, the event must be dispatched from a timer to prevent
    // the frontend from making JS allocations while the GC is still active.
    m_removedCanvasIdentifiers.append(identifier);

    if (!m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.startOneShot(0_s);
}

void InspectorCanvasAgent::didFinishRecordingCanvasFrame(CanvasRenderingContext& context, bool forceDispatch)
{
    auto* inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    if (!inspectorCanvas->context().callTracingActive())
        return;

    if (!inspectorCanvas->hasRecordingData()) {
        if (forceDispatch) {
            m_frontendDispatcher->recordingFinished(inspectorCanvas->identifier(), nullptr);

            inspectorCanvas->resetRecordingData();
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

    // FIXME: <https://webkit.org/b/176008> Web Inspector: Record actions performed on WebGL2RenderingContext

    Inspector::Protocol::Recording::Type type;
    if (is<CanvasRenderingContext2D>(inspectorCanvas->context()))
        type = Inspector::Protocol::Recording::Type::Canvas2D;
    else if (is<ImageBitmapRenderingContext>(inspectorCanvas->context()))
        type = Inspector::Protocol::Recording::Type::CanvasBitmapRenderer;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(inspectorCanvas->context()))
        type = Inspector::Protocol::Recording::Type::CanvasWebGL;
#endif
    else {
        ASSERT_NOT_REACHED();
        type = Inspector::Protocol::Recording::Type::Canvas2D;
    }

    auto recording = Inspector::Protocol::Recording::Recording::create()
        .setVersion(Inspector::Protocol::Recording::VERSION)
        .setType(type)
        .setInitialState(inspectorCanvas->releaseInitialState())
        .setData(inspectorCanvas->releaseData())
        .release();

    const String& name = inspectorCanvas->recordingName();
    if (!name.isEmpty())
        recording->setName(name);

    m_frontendDispatcher->recordingFinished(inspectorCanvas->identifier(), WTFMove(recording));

    inspectorCanvas->resetRecordingData();
}

void InspectorCanvasAgent::consoleStartRecordingCanvas(CanvasRenderingContext& context, JSC::ExecState& exec, JSC::JSObject* options)
{
    auto* inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    RecordingOptions recordingOptions;
    if (options) {
        if (JSC::JSValue optionSingleFrame = options->get(&exec, JSC::Identifier::fromString(&exec, "singleFrame")))
            recordingOptions.frameCount = optionSingleFrame.toBoolean(&exec) ? 1 : 0;
        if (JSC::JSValue optionFrameCount = options->get(&exec, JSC::Identifier::fromString(&exec, "frameCount")))
            recordingOptions.frameCount = optionFrameCount.toNumber(&exec);
        if (JSC::JSValue optionMemoryLimit = options->get(&exec, JSC::Identifier::fromString(&exec, "memoryLimit")))
            recordingOptions.memoryLimit = optionMemoryLimit.toNumber(&exec);
        if (JSC::JSValue optionName = options->get(&exec, JSC::Identifier::fromString(&exec, "name")))
            recordingOptions.name = optionName.toWTFString(&exec);
    }
    startRecording(*inspectorCanvas, Inspector::Protocol::Recording::Initiator::Console, WTFMove(recordingOptions));
}

#if ENABLE(WEBGL)
void InspectorCanvasAgent::didEnableExtension(WebGLRenderingContextBase& context, const String& extension)
{
    auto* inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    m_frontendDispatcher->extensionEnabled(inspectorCanvas->identifier(), extension);
}

void InspectorCanvasAgent::didCreateProgram(WebGLRenderingContextBase& context, WebGLProgram& program)
{
    auto* inspectorCanvas = findInspectorCanvas(context);
    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    auto inspectorProgram = InspectorShaderProgram::create(program, *inspectorCanvas);
    String programIdentifier = inspectorProgram->identifier();
    m_identifierToInspectorProgram.set(programIdentifier, WTFMove(inspectorProgram));
    m_frontendDispatcher->programCreated(inspectorCanvas->identifier(), programIdentifier);
}

void InspectorCanvasAgent::willDeleteProgram(WebGLProgram& program)
{
    auto* inspectorProgram = findInspectorProgram(program);
    if (!inspectorProgram)
        return;

    String identifier = unbindProgram(*inspectorProgram);
    m_frontendDispatcher->programDeleted(identifier);
}

bool InspectorCanvasAgent::isShaderProgramDisabled(WebGLProgram& program)
{
    auto* inspectorProgram = findInspectorProgram(program);
    if (!inspectorProgram)
        return false;

    return inspectorProgram->disabled();
}

bool InspectorCanvasAgent::isShaderProgramHighlighted(WebGLProgram& program)
{
    auto* inspectorProgram = findInspectorProgram(program);
    if (!inspectorProgram)
        return false;

    return inspectorProgram->highlighted();
}
#endif

void InspectorCanvasAgent::startRecording(InspectorCanvas& inspectorCanvas, Inspector::Protocol::Recording::Initiator initiator, RecordingOptions&& recordingOptions)
{
    auto& canvasRenderingContext = inspectorCanvas.context();

    if (!is<CanvasRenderingContext2D>(canvasRenderingContext)
#if ENABLE(WEBGL)
        && !is<WebGLRenderingContext>(canvasRenderingContext)
#endif
        && !is<ImageBitmapRenderingContext>(canvasRenderingContext))
        return;

    if (canvasRenderingContext.callTracingActive())
        return;

    inspectorCanvas.resetRecordingData();
    if (recordingOptions.frameCount)
        inspectorCanvas.setFrameCount(recordingOptions.frameCount.value());
    if (recordingOptions.memoryLimit)
        inspectorCanvas.setBufferLimit(recordingOptions.memoryLimit.value());
    if (recordingOptions.name)
        inspectorCanvas.setRecordingName(recordingOptions.name.value());
    canvasRenderingContext.setCallTracingActive(true);

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

void InspectorCanvasAgent::canvasRecordingTimerFired()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (!inspectorCanvas->context().callTracingActive())
            continue;

        didFinishRecordingCanvasFrame(inspectorCanvas->context());
    }
}

void InspectorCanvasAgent::clearCanvasData()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values())
        inspectorCanvas->context().canvasBase().removeObserver(*this);

    m_identifierToInspectorCanvas.clear();
    m_removedCanvasIdentifiers.clear();
#if ENABLE(WEBGL)
    m_identifierToInspectorProgram.clear();
#endif

    if (m_canvasRecordingTimer.isActive())
        m_canvasRecordingTimer.stop();

    if (m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.stop();
}

InspectorCanvas& InspectorCanvasAgent::bindCanvas(CanvasRenderingContext& context, bool captureBacktrace)
{
    context.canvasBase().addObserver(*this);

    auto inspectorCanvas = InspectorCanvas::create(context);
    m_identifierToInspectorCanvas.set(inspectorCanvas->identifier(), inspectorCanvas.copyRef());

    m_frontendDispatcher->canvasAdded(inspectorCanvas->buildObjectForCanvas(captureBacktrace));

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(inspectorCanvas->context())) {
        WebGLRenderingContextBase& contextWebGL = downcast<WebGLRenderingContextBase>(inspectorCanvas->context());
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

String InspectorCanvasAgent::unbindCanvas(InspectorCanvas& inspectorCanvas)
{
#if ENABLE(WEBGL)
    Vector<InspectorShaderProgram*> programsToRemove;
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (&inspectorProgram->canvas() == &inspectorCanvas)
            programsToRemove.append(inspectorProgram.get());
    }

    for (auto* inspectorProgram : programsToRemove)
        unbindProgram(*inspectorProgram);
#endif

    String identifier = inspectorCanvas.identifier();
    m_identifierToInspectorCanvas.remove(identifier);

    return identifier;
}

InspectorCanvas* InspectorCanvasAgent::assertInspectorCanvas(ErrorString& errorString, const String& identifier)
{
    RefPtr<InspectorCanvas> inspectorCanvas = m_identifierToInspectorCanvas.get(identifier);
    if (!inspectorCanvas) {
        errorString = "No canvas for given identifier."_s;
        return nullptr;
    }

    return inspectorCanvas.get();
}

InspectorCanvas* InspectorCanvasAgent::findInspectorCanvas(CanvasRenderingContext& context)
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values()) {
        if (&inspectorCanvas->context() == &context)
            return inspectorCanvas.get();
    }

    return nullptr;
}

#if ENABLE(WEBGL)
String InspectorCanvasAgent::unbindProgram(InspectorShaderProgram& inspectorProgram)
{
    String identifier = inspectorProgram.identifier();
    m_identifierToInspectorProgram.remove(identifier);

    return identifier;
}

InspectorShaderProgram* InspectorCanvasAgent::assertInspectorProgram(ErrorString& errorString, const String& identifier)
{
    RefPtr<InspectorShaderProgram> inspectorProgram = m_identifierToInspectorProgram.get(identifier);
    if (!inspectorProgram) {
        errorString = "No shader program for given identifier."_s;
        return nullptr;
    }

    return inspectorProgram.get();
}

InspectorShaderProgram* InspectorCanvasAgent::findInspectorProgram(WebGLProgram& program)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (&inspectorProgram->program() == &program)
            return inspectorProgram.get();
    }

    return nullptr;
}
#endif

} // namespace WebCore

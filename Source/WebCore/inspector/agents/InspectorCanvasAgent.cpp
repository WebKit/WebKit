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
#include "Document.h"
#include "Element.h"
#include "EventLoop.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageData.h"
#include "InspectorDOMAgent.h"
#include "InspectorInstrumentation.h"
#include "InspectorShaderProgram.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "OffscreenCanvas.h"
#include "Path2D.h"
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
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

InspectorCanvasAgent::InspectorCanvasAgent(PageAgentContext& context)
    : InspectorAgentBase("Canvas"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::CanvasFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CanvasBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_inspectedPage(context.inspectedPage)
    , m_canvasDestroyedTimer(*this, &InspectorCanvasAgent::canvasDestroyedTimerFired)
#if ENABLE(WEBGL)
    , m_programDestroyedTimer(*this, &InspectorCanvasAgent::programDestroyedTimerFired)
#endif
{
}

InspectorCanvasAgent::~InspectorCanvasAgent() = default;

void InspectorCanvasAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorCanvasAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

void InspectorCanvasAgent::discardAgent()
{
    reset();
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::enable()
{
    if (m_instrumentingAgents.enabledCanvasAgent() == this)
        return { };

    m_instrumentingAgents.setEnabledCanvasAgent(this);

    const auto existsInCurrentPage = [&] (ScriptExecutionContext* scriptExecutionContext) {
        if (!is<Document>(scriptExecutionContext))
            return false;

        // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
        auto* document = downcast<Document>(scriptExecutionContext);
        return document->page() == &m_inspectedPage;
    };

    {
        Locker locker { CanvasRenderingContext::instancesLock() };
        for (auto* context : CanvasRenderingContext::instances()) {

            if (existsInCurrentPage(context->canvasBase().scriptExecutionContext()))
                bindCanvas(*context, false);
        }
    }

#if ENABLE(WEBGL)
    {
        Locker locker { WebGLProgram::instancesLock() };
        for (auto& [program, contextWebGLBase] : WebGLProgram::instances()) {
            if (contextWebGLBase && existsInCurrentPage(contextWebGLBase->canvasBase().scriptExecutionContext()))
                didCreateWebGLProgram(*contextWebGLBase, *program);
        }
    }
#endif

    return { };
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::disable()
{
    m_instrumentingAgents.setEnabledCanvasAgent(nullptr);

    reset();

    m_recordingAutoCaptureFrameCount = std::nullopt;

    return { };
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorCanvasAgent::requestNode(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto* node = inspectorCanvas->canvasElement();
    if (!node)
        makeUnexpected("Missing element of canvas for given canvasId"_s);

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    int documentNodeId = m_instrumentingAgents.persistentDOMAgent()->boundNodeId(&node->document());
    if (!documentNodeId)
        makeUnexpected("Document must have been requested"_s);

    return m_instrumentingAgents.persistentDOMAgent()->pushNodeToFrontend(errorString, documentNodeId, node);
}

Protocol::ErrorStringOr<String> InspectorCanvasAgent::requestContent(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto result = inspectorCanvas->getCanvasContentAsDataURL(errorString);
    if (!result)
        return makeUnexpected(errorString);

    return result;
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::NodeId>>> InspectorCanvasAgent::requestClientNodes(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto clientNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
    for (auto& clientNode : inspectorCanvas->clientNodes()) {
        // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
        if (auto documentNodeId = domAgent->boundNodeId(&clientNode->document()))
            clientNodeIds->addItem(domAgent->pushNodeToFrontend(errorString, documentNodeId, clientNode));
    }
    return clientNodeIds;
}

Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> InspectorCanvasAgent::resolveContext(const Protocol::Canvas::CanvasId& canvasId, const String& objectGroup)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto* state = inspectorCanvas->scriptExecutionContext()->globalObject();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
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

Protocol::ErrorStringOr<void> InspectorCanvasAgent::setRecordingAutoCaptureFrameCount(int count)
{
    if (count > 0)
        m_recordingAutoCaptureFrameCount = count;
    else
        m_recordingAutoCaptureFrameCount = std::nullopt;
    return { };
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::startRecording(const Protocol::Canvas::CanvasId& canvasId, std::optional<int>&& frameCount, std::optional<int>&& memoryLimit)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto& context = inspectorCanvas->canvasContext();

    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    if (context.hasActiveInspectorCanvasCallTracer())
        return makeUnexpected("Already recording canvas"_s);

    RecordingOptions recordingOptions;
    if (frameCount)
        recordingOptions.frameCount = *frameCount;
    if (memoryLimit)
        recordingOptions.memoryLimit = *memoryLimit;
    startRecording(*inspectorCanvas, Protocol::Recording::Initiator::Frontend, WTFMove(recordingOptions));

    return { };
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::stopRecording(const Protocol::Canvas::CanvasId& canvasId)
{
    Protocol::ErrorString errorString;

    auto inspectorCanvas = assertInspectorCanvas(errorString, canvasId);
    if (!inspectorCanvas)
        return makeUnexpected(errorString);

    auto& context = inspectorCanvas->canvasContext();

    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    if (!context.hasActiveInspectorCanvasCallTracer())
        return makeUnexpected("Not recording canvas"_s);

    didFinishRecordingCanvasFrame(context, true);

    return { };
}

#if ENABLE(WEBGL)

Protocol::ErrorStringOr<String> InspectorCanvasAgent::requestShaderSource(const Protocol::Canvas::ProgramId& programId, Protocol::Canvas::ShaderType shaderType)
{
    Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    auto source = inspectorProgram->requestShaderSource(shaderType);
    if (!source)
        return makeUnexpected("Missing shader of given shaderType for given programId"_s);

    return source;
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::updateShader(const Protocol::Canvas::ProgramId& programId, Protocol::Canvas::ShaderType shaderType, const String& source)
{
    Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    if (!inspectorProgram->updateShader(shaderType, source))
        return makeUnexpected("Failed to update shader of given shaderType for given programId"_s);

    return { };
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::setShaderProgramDisabled(const Protocol::Canvas::ProgramId& programId, bool disabled)
{
    Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    inspectorProgram->setDisabled(disabled);

    return { };
}

Protocol::ErrorStringOr<void> InspectorCanvasAgent::setShaderProgramHighlighted(const Protocol::Canvas::ProgramId& programId, bool highlighted)
{
    Protocol::ErrorString errorString;

    auto inspectorProgram = assertInspectorProgram(errorString, programId);
    if (!inspectorProgram)
        return makeUnexpected(errorString);

    inspectorProgram->setHighlighted(highlighted);

    return { };
}

#endif // ENABLE(WEBGL)

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
        startRecording(inspectorCanvas, Protocol::Recording::Initiator::AutoCapture, WTFMove(recordingOptions));
    }
}

void InspectorCanvasAgent::didChangeCanvasMemory(CanvasRenderingContext& context)
{
    RefPtr<InspectorCanvas> inspectorCanvas;

    if (!inspectorCanvas)
        inspectorCanvas = findInspectorCanvas(context);

    ASSERT(inspectorCanvas);
    if (!inspectorCanvas)
        return;

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = inspectorCanvas->canvasElement())
        m_frontendDispatcher->canvasMemoryChanged(inspectorCanvas->identifier(), node->memoryCost());
}

void InspectorCanvasAgent::canvasChanged(CanvasBase& canvasBase, const std::optional<FloatRect>&)
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
    if (!context.hasActiveInspectorCanvasCallTracer())
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
        if (JSC::JSValue optionSingleFrame = options->get(&exec, JSC::Identifier::fromString(vm, "singleFrame"_s)))
            recordingOptions.frameCount = optionSingleFrame.toBoolean(&exec) ? 1 : 0;
        if (JSC::JSValue optionFrameCount = options->get(&exec, JSC::Identifier::fromString(vm, "frameCount"_s)))
            recordingOptions.frameCount = optionFrameCount.toNumber(&exec);
        if (JSC::JSValue optionMemoryLimit = options->get(&exec, JSC::Identifier::fromString(vm, "memoryLimit"_s)))
            recordingOptions.memoryLimit = optionMemoryLimit.toNumber(&exec);
        if (JSC::JSValue optionName = options->get(&exec, JSC::Identifier::fromString(vm, "name"_s)))
            recordingOptions.name = optionName.toWTFString(&exec);
    }
    startRecording(*inspectorCanvas, Protocol::Recording::Initiator::Console, WTFMove(recordingOptions));
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

#define PROCESS_ARGUMENT_DEFINITION(ArgumentType) \
std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvasAgent::processArgument(CanvasRenderingContext& canvasRenderingContext, ArgumentType argument) \
{ \
    auto inspectorCanvas = findInspectorCanvas(canvasRenderingContext); \
    ASSERT(inspectorCanvas); \
    return inspectorCanvas->processArgument(argument); \
} \
// end of PROCESS_ARGUMENT_DEFINITION
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_ARGUMENT(PROCESS_ARGUMENT_DEFINITION)
#undef PROCESS_ARGUMENT_DEFINITION

void InspectorCanvasAgent::recordAction(CanvasRenderingContext& canvasRenderingContext, String&& name, InspectorCanvasCallTracer::ProcessedArguments&& arguments)
{
    ASSERT(canvasRenderingContext.hasActiveInspectorCanvasCallTracer());

    auto inspectorCanvas = findInspectorCanvas(canvasRenderingContext);
    ASSERT(inspectorCanvas);

    // Only enqueue one microtask for all actively recording canvases.
    if (m_recordingCanvasIdentifiers.isEmpty()) {
        if (auto* scriptExecutionContext = inspectorCanvas->scriptExecutionContext()) {
            scriptExecutionContext->eventLoop().queueMicrotask([weakThis = WeakPtr { *this }] {
                if (!weakThis)
                    return;

                auto& canvasAgent = *weakThis;

                auto identifiers = copyToVector(canvasAgent.m_recordingCanvasIdentifiers);
                for (auto& identifier : identifiers) {
                    auto inspectorCanvas = canvasAgent.m_identifierToInspectorCanvas.get(identifier);
                    if (!inspectorCanvas)
                        continue;

                    auto& canvasRenderingContext = inspectorCanvas->canvasContext();
                    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

                    if (canvasRenderingContext.hasActiveInspectorCanvasCallTracer())
                        canvasAgent.didFinishRecordingCanvasFrame(canvasRenderingContext);
                }

                canvasAgent.m_recordingCanvasIdentifiers.clear();
            });
        }
    }

    m_recordingCanvasIdentifiers.add(inspectorCanvas->identifier());

    inspectorCanvas->recordAction(WTFMove(name), WTFMove(arguments));

    if (!inspectorCanvas->hasBufferSpace())
        didFinishRecordingCanvasFrame(canvasRenderingContext, true);
}

void InspectorCanvasAgent::startRecording(InspectorCanvas& inspectorCanvas, Protocol::Recording::Initiator initiator, RecordingOptions&& recordingOptions)
{
    auto& context = inspectorCanvas.canvasContext();

    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    if (!is<CanvasRenderingContext2D>(context)
        && !is<ImageBitmapRenderingContext>(context)
#if ENABLE(WEBGL)
        && !is<WebGLRenderingContext>(context)
        && !is<WebGL2RenderingContext>(context)
#endif
    )
        return;

    if (context.hasActiveInspectorCanvasCallTracer())
        return;

    inspectorCanvas.resetRecordingData();
    if (recordingOptions.frameCount)
        inspectorCanvas.setFrameCount(recordingOptions.frameCount.value());
    if (recordingOptions.memoryLimit)
        inspectorCanvas.setBufferLimit(recordingOptions.memoryLimit.value());
    if (recordingOptions.name)
        inspectorCanvas.setRecordingName(recordingOptions.name.value());
    context.setHasActiveInspectorCanvasCallTracer(true);

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

#if ENABLE(WEBGL)

void InspectorCanvasAgent::programDestroyedTimerFired()
{
    if (!m_removedProgramIdentifiers.size())
        return;

    for (auto& identifier : m_removedProgramIdentifiers)
        m_frontendDispatcher->programDeleted(identifier);

    m_removedProgramIdentifiers.clear();
}

#endif

void InspectorCanvasAgent::reset()
{
    for (auto& inspectorCanvas : m_identifierToInspectorCanvas.values())
        inspectorCanvas->canvasContext().canvasBase().removeObserver(*this);

    m_identifierToInspectorCanvas.clear();
    m_removedCanvasIdentifiers.clear();
    if (m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.stop();

#if ENABLE(WEBGL)
    m_identifierToInspectorProgram.clear();
    m_removedProgramIdentifiers.clear();
    if (m_programDestroyedTimer.isActive())
        m_programDestroyedTimer.stop();
#endif

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
        if (std::optional<Vector<String>> extensions = contextWebGL.getSupportedExtensions()) {
            for (const String& extension : *extensions) {
                if (contextWebGL.extensionIsEnabled(extension))
                    m_frontendDispatcher->extensionEnabled(inspectorCanvas->identifier(), extension);
            }
        }
    }
#endif

    return inspectorCanvas;
}

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
#endif

    inspectorCanvas.canvasContext().canvasBase().removeObserver(*this);

    String identifier = inspectorCanvas.identifier();
    m_identifierToInspectorCanvas.remove(identifier);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedCanvasIdentifiers.append(identifier);

    if (!m_canvasDestroyedTimer.isActive())
        m_canvasDestroyedTimer.startOneShot(0_s);
}

RefPtr<InspectorCanvas> InspectorCanvasAgent::assertInspectorCanvas(Protocol::ErrorString& errorString, const String& canvasId)
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
        if (&inspectorCanvas->canvasContext() == &context)
            return inspectorCanvas;
    }
    return nullptr;
}

#if ENABLE(WEBGL)

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

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::assertInspectorProgram(Protocol::ErrorString& errorString, const String& programId)
{
    auto inspectorProgram = m_identifierToInspectorProgram.get(programId);
    if (!inspectorProgram) {
        errorString = "Missing program for given programId"_s;
        return nullptr;
    }
    return inspectorProgram;
}

RefPtr<InspectorShaderProgram> InspectorCanvasAgent::findInspectorProgram(WebGLProgram& program)
{
    for (auto& inspectorProgram : m_identifierToInspectorProgram.values()) {
        if (&inspectorProgram->program() == &program)
            return inspectorProgram;
    }
    return nullptr;
}

#endif // ENABLE(WEBGL)

} // namespace WebCore

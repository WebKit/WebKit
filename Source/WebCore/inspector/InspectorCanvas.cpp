/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorCanvas.h"

#include "AffineTransform.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext.h"
#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "Element.h"
#include "FloatPoint.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorDOMAgent.h"
#include "JSCanvasDirection.h"
#include "JSCanvasFillRule.h"
#include "JSCanvasLineCap.h"
#include "JSCanvasLineJoin.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSCanvasTextAlign.h"
#include "JSCanvasTextBaseline.h"
#include "JSExecState.h"
#include "JSImageBitmapRenderingContext.h"
#include "JSImageSmoothingQuality.h"
#include "Path2D.h"
#include "Pattern.h"
#include "RecordingSwizzleTypes.h"
#include "SVGPathUtilities.h"
#include "StringAdaptors.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/Function.h>

#if ENABLE(CSS_TYPED_OM)
#include "TypedOMCSSImageValue.h"
#endif

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#include "WebGLRenderingContext.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(WEBGPU)
#include "GPUCanvasContext.h"
#include "JSWebGPUDevice.h"
#include "WebGPUDevice.h"
#endif

namespace WebCore {

using namespace Inspector;

#if ENABLE(WEBGPU)
static HTMLCanvasElement* canvasIfContextMatchesDevice(CanvasRenderingContext& context, WebGPUDevice& device)
{
    if (is<GPUCanvasContext>(context)) {
        auto& contextGPU = downcast<GPUCanvasContext>(context);
        if (auto* webGPUSwapChain = contextGPU.swapChain()) {
            if (auto* gpuSwapChain = webGPUSwapChain->swapChain()) {
                if (gpuSwapChain == device.device().swapChain()) {
                    if (is<HTMLCanvasElement>(contextGPU.canvasBase()))
                        return &downcast<HTMLCanvasElement>(contextGPU.canvasBase());
                }
            }
        }
    }
    return nullptr;
}
#endif

Ref<InspectorCanvas> InspectorCanvas::create(CanvasRenderingContext& context)
{
    return adoptRef(*new InspectorCanvas(context));
}

#if ENABLE(WEBGPU)
Ref<InspectorCanvas> InspectorCanvas::create(WebGPUDevice& device)
{
    return adoptRef(*new InspectorCanvas(device));
}
#endif

InspectorCanvas::InspectorCanvas(CanvasRenderingContext& context)
    : m_identifier("canvas:" + IdentifiersFactory::createIdentifier())
    , m_context(context)
{
#if ENABLE(WEBGPU)
    // The actual "context" for WebGPU is the `WebGPUDevice`, not the <canvas>.
    ASSERT(!is<GPUCanvasContext>(context));
#endif
}

#if ENABLE(WEBGPU)
InspectorCanvas::InspectorCanvas(WebGPUDevice& device)
    : m_identifier("canvas:" + IdentifiersFactory::createIdentifier())
    , m_context(device)
{
}
#endif

CanvasRenderingContext* InspectorCanvas::canvasContext() const
{
    if (auto* contextWrapper = WTF::get_if<std::reference_wrapper<CanvasRenderingContext>>(m_context))
        return &contextWrapper->get();
    return nullptr;
}

HTMLCanvasElement* InspectorCanvas::canvasElement() const
{
    return WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) -> HTMLCanvasElement* {
            auto& context = contextWrapper.get();
            if (is<HTMLCanvasElement>(context.canvasBase()))
                return &downcast<HTMLCanvasElement>(context.canvasBase());
            return nullptr;
        },
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUDevice> deviceWrapper) -> HTMLCanvasElement* {
            auto& device = deviceWrapper.get();
            {
                LockHolder lock(CanvasRenderingContext::instancesMutex());
                for (auto* canvasRenderingContext : CanvasRenderingContext::instances(lock)) {
                    if (auto* canvasElement = canvasIfContextMatchesDevice(*canvasRenderingContext, device))
                        return canvasElement;
                }
            }
            return nullptr;
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
    return nullptr;
}

#if ENABLE(WEBGPU)
WebGPUDevice* InspectorCanvas::deviceContext() const
{
    if (auto* deviceWrapper = WTF::get_if<std::reference_wrapper<WebGPUDevice>>(m_context))
        return &deviceWrapper->get();
    return nullptr;
}

bool InspectorCanvas::isDeviceForCanvasContext(CanvasRenderingContext& context) const
{
    if (auto* device = deviceContext())
        return canvasIfContextMatchesDevice(context, *device);
    return false;
}
#endif

ScriptExecutionContext* InspectorCanvas::scriptExecutionContext() const
{
    return WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) {
            auto& context = contextWrapper.get();
            return context.canvasBase().scriptExecutionContext();
        },
#if ENABLE(WEBGPU)
        [] (std::reference_wrapper<WebGPUDevice> deviceWrapper) {
            auto& device = deviceWrapper.get();
            return device.scriptExecutionContext();
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
}

JSC::JSValue InspectorCanvas::resolveContext(JSC::JSGlobalObject* exec) const
{
    JSC::JSLockHolder lock(exec);

    auto* globalObject = deprecatedGlobalObjectForPrototype(exec);

    return WTF::switchOn(m_context,
        [&] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) {
            auto& context = contextWrapper.get();
            if (is<CanvasRenderingContext2D>(context))
                return toJS(exec, globalObject, downcast<CanvasRenderingContext2D>(context));
            if (is<ImageBitmapRenderingContext>(context))
                return toJS(exec, globalObject, downcast<ImageBitmapRenderingContext>(context));
#if ENABLE(WEBGL)
            if (is<WebGLRenderingContext>(context))
                return toJS(exec, globalObject, downcast<WebGLRenderingContext>(context));
#endif
#if ENABLE(WEBGL2)
            if (is<WebGL2RenderingContext>(context))
                return toJS(exec, globalObject, downcast<WebGL2RenderingContext>(context));
#endif
            return JSC::JSValue();
        },
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUDevice> deviceWrapper) {
            return toJS(exec, globalObject, deviceWrapper.get());
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return JSC::JSValue();
        }
    );
}

HashSet<Element*> InspectorCanvas::clientNodes() const
{
    return WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) {
            auto& context = contextWrapper.get();
            return context.canvasBase().cssCanvasClients();
        },
#if ENABLE(WEBGPU)
        [&] (std::reference_wrapper<WebGPUDevice> deviceWrapper) {
            auto& device = deviceWrapper.get();

            HashSet<Element*> canvasElementClients;
            {
                LockHolder lock(CanvasRenderingContext::instancesMutex());
                for (auto* canvasRenderingContext : CanvasRenderingContext::instances(lock)) {
                    if (auto* canvasElement = canvasIfContextMatchesDevice(*canvasRenderingContext, device))
                        canvasElementClients.add(canvasElement);
                }
            }
            return canvasElementClients;
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return HashSet<Element*>();
        }
    );
}

void InspectorCanvas::canvasChanged()
{
    auto* context = canvasContext();
    ASSERT(context);

    if (!context->callTracingActive())
        return;

    // Since 2D contexts are able to be fully reproduced in the frontend, we don't need snapshots.
    if (is<CanvasRenderingContext2D>(context))
        return;

    m_contentChanged = true;
}

void InspectorCanvas::resetRecordingData()
{
    m_initialState = nullptr;
    m_frames = nullptr;
    m_currentActions = nullptr;
    m_serializedDuplicateData = nullptr;
    m_indexedDuplicateData.clear();
    m_recordingName = { };
    m_bufferLimit = 100 * 1024 * 1024;
    m_bufferUsed = 0;
    m_frameCount = WTF::nullopt;
    m_framesCaptured = 0;
    m_contentChanged = false;

    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    context->setCallTracingActive(false);
}

bool InspectorCanvas::hasRecordingData() const
{
    return m_bufferUsed > 0;
}

bool InspectorCanvas::currentFrameHasData() const
{
    return !!m_frames;
}

static bool shouldSnapshotBitmapRendererAction(const String& name)
{
    return name == "transferFromImageBitmap";
}

#if ENABLE(WEBGL)
static bool shouldSnapshotWebGLAction(const String& name)
{
    return name == "clear"
        || name == "drawArrays"
        || name == "drawElements";
}
#endif

#if ENABLE(WEBGL2)
static bool shouldSnapshotWebGL2Action(const String& name)
{
    return name == "clear"
        || name == "drawArrays"
        || name == "drawArraysInstanced"
        || name == "drawElements"
        || name == "drawElementsInstanced";
}
#endif

void InspectorCanvas::recordAction(const String& name, std::initializer_list<RecordCanvasActionVariant>&& parameters)
{
    if (!m_initialState) {
        // We should only construct the initial state for the first action of the recording.
        ASSERT(!m_frames && !m_currentActions);

        m_initialState = buildInitialState();
        m_bufferUsed += m_initialState->memoryCost();
    }

    if (!m_frames)
        m_frames = JSON::ArrayOf<Inspector::Protocol::Recording::Frame>::create();

    if (!m_currentActions) {
        m_currentActions = JSON::ArrayOf<JSON::Value>::create();

        auto frame = Inspector::Protocol::Recording::Frame::create()
            .setActions(m_currentActions)
            .release();

        m_frames->addItem(WTFMove(frame));
        ++m_framesCaptured;

        m_currentFrameStartTime = MonotonicTime::now();
    }

    appendActionSnapshotIfNeeded();

    m_lastRecordedAction = buildAction(name, WTFMove(parameters));
    m_bufferUsed += m_lastRecordedAction->memoryCost();
    m_currentActions->addItem(m_lastRecordedAction.get());

    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    if (is<ImageBitmapRenderingContext>(context) && shouldSnapshotBitmapRendererAction(name))
        m_contentChanged = true;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context) && shouldSnapshotWebGLAction(name))
        m_contentChanged = true;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context) && shouldSnapshotWebGL2Action(name))
        m_contentChanged = true;
#endif
}

void InspectorCanvas::finalizeFrame()
{
    appendActionSnapshotIfNeeded();

    if (m_frames && m_frames->length() && !std::isnan(m_currentFrameStartTime)) {
        auto currentFrame = static_cast<Inspector::Protocol::Recording::Frame*>(m_frames->get(m_frames->length() - 1).get());
        currentFrame->setDuration((MonotonicTime::now() - m_currentFrameStartTime).milliseconds());

        m_currentFrameStartTime = MonotonicTime::nan();
    }

    m_currentActions = nullptr;
}

void InspectorCanvas::markCurrentFrameIncomplete()
{
    if (!m_currentActions || !m_frames || !m_frames->length())
        return;

    static_cast<Inspector::Protocol::Recording::Frame*>(m_frames->get(m_frames->length() - 1).get())->setIncomplete(true);
}

void InspectorCanvas::setBufferLimit(long memoryLimit)
{
    m_bufferLimit = std::min<long>(memoryLimit, std::numeric_limits<int>::max());
}

bool InspectorCanvas::hasBufferSpace() const
{
    return m_bufferUsed < m_bufferLimit;
}

void InspectorCanvas::setFrameCount(long frameCount)
{
    if (frameCount > 0)
        m_frameCount = std::min<long>(frameCount, std::numeric_limits<int>::max());
    else
        m_frameCount = WTF::nullopt;
}

bool InspectorCanvas::overFrameCount() const
{
    return m_frameCount && m_framesCaptured >= m_frameCount.value();
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(bool captureBacktrace)
{
    using ContextTypeType = Optional<Inspector::Protocol::Canvas::ContextType>;
    auto contextType = WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) -> ContextTypeType {
            auto& context = contextWrapper.get();
            if (is<CanvasRenderingContext2D>(context))
                return Inspector::Protocol::Canvas::ContextType::Canvas2D;
            if (is<ImageBitmapRenderingContext>(context))
                return Inspector::Protocol::Canvas::ContextType::BitmapRenderer;
#if ENABLE(WEBGL)
            if (is<WebGLRenderingContext>(context))
                return Inspector::Protocol::Canvas::ContextType::WebGL;
#endif
#if ENABLE(WEBGL2)
            if (is<WebGL2RenderingContext>(context))
                return Inspector::Protocol::Canvas::ContextType::WebGL2;
#endif
            return WTF::nullopt;
        },
#if ENABLE(WEBGPU)
        [] (std::reference_wrapper<WebGPUDevice>) {
            return Inspector::Protocol::Canvas::ContextType::WebGPU;
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return WTF::nullopt;
        }
    );
    if (!contextType) {
        ASSERT_NOT_REACHED();
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(m_identifier)
        .setContextType(contextType.value())
        .release();

    if (auto* node = canvasElement()) {
        String cssCanvasName = node->document().nameForCSSCanvasElement(*node);
        if (!cssCanvasName.isEmpty())
            canvas->setCssCanvasName(cssCanvasName);

        // FIXME: <https://webkit.org/b/178282> Web Inspector: send a DOM node with each Canvas payload and eliminate Canvas.requestNode
    }

    using ContextAttributesType = RefPtr<Inspector::Protocol::Canvas::ContextAttributes>;
    auto contextAttributes = WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) -> ContextAttributesType {
            auto& context = contextWrapper.get();
            if (is<ImageBitmapRenderingContext>(context)) {
                auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
                    .release();
                contextAttributesPayload->setAlpha(downcast<ImageBitmapRenderingContext>(context).hasAlpha());
                return contextAttributesPayload;
            }

#if ENABLE(WEBGL)
            if (is<WebGLRenderingContextBase>(context)) {
                if (const auto& attributes = downcast<WebGLRenderingContextBase>(context).getContextAttributes()) {
                    auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
                        .release();
                    contextAttributesPayload->setAlpha(attributes->alpha);
                    contextAttributesPayload->setDepth(attributes->depth);
                    contextAttributesPayload->setStencil(attributes->stencil);
                    contextAttributesPayload->setAntialias(attributes->antialias);
                    contextAttributesPayload->setPremultipliedAlpha(attributes->premultipliedAlpha);
                    contextAttributesPayload->setPreserveDrawingBuffer(attributes->preserveDrawingBuffer);
                    switch (attributes->powerPreference) {
                    case WebGLPowerPreference::Default:
                        contextAttributesPayload->setPowerPreference("default");
                        break;
                    case WebGLPowerPreference::LowPower:
                        contextAttributesPayload->setPowerPreference("low-power");
                        break;
                    case WebGLPowerPreference::HighPerformance:
                        contextAttributesPayload->setPowerPreference("high-performance");
                        break;
                    }
                    contextAttributesPayload->setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat);
                    return contextAttributesPayload;
                }
            }
#endif
            return nullptr;
        },
#if ENABLE(WEBGPU)
        [] (std::reference_wrapper<WebGPUDevice> deviceWrapper) -> ContextAttributesType {
            auto& device = deviceWrapper.get();
            if (const auto& options = device.adapter().options()) {
                auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
                    .release();
                if (const auto& powerPreference = options->powerPreference) {
                    switch (powerPreference.value()) {
                    case GPUPowerPreference::LowPower:
                        contextAttributesPayload->setPowerPreference("low-power");
                        break;

                    case GPUPowerPreference::HighPerformance:
                        contextAttributesPayload->setPowerPreference("high-performance");
                        break;
                    }
                }
                return WTFMove(contextAttributesPayload);
            }
            return nullptr;
        },
#endif
        [] (Monostate) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
    if (contextAttributes)
        canvas->setContextAttributes(WTFMove(contextAttributes));

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = canvasElement()) {
        if (size_t memoryCost = node->memoryCost())
            canvas->setMemoryCost(memoryCost);
    }

    if (captureBacktrace) {
        auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState(), Inspector::ScriptCallStack::maxCallStackSizeToCapture);
        canvas->setBacktrace(stackTrace->buildInspectorArray());
    }

    return canvas;
}

Ref<Inspector::Protocol::Recording::Recording> InspectorCanvas::releaseObjectForRecording()
{
    ASSERT(!m_currentActions);
    ASSERT(!m_lastRecordedAction);
    ASSERT(!m_frames);

    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    Inspector::Protocol::Recording::Type type;
    if (is<CanvasRenderingContext2D>(context))
        type = Inspector::Protocol::Recording::Type::Canvas2D;
    else if (is<ImageBitmapRenderingContext>(context))
        type = Inspector::Protocol::Recording::Type::CanvasBitmapRenderer;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context))
        type = Inspector::Protocol::Recording::Type::CanvasWebGL;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context))
        type = Inspector::Protocol::Recording::Type::CanvasWebGL2;
#endif
    else {
        ASSERT_NOT_REACHED();
        type = Inspector::Protocol::Recording::Type::Canvas2D;
    }

    auto recording = Inspector::Protocol::Recording::Recording::create()
        .setVersion(Inspector::Protocol::Recording::VERSION)
        .setType(type)
        .setInitialState(m_initialState.releaseNonNull())
        .setData(m_serializedDuplicateData.releaseNonNull())
        .release();

    if (!m_recordingName.isEmpty())
        recording->setName(m_recordingName);

    resetRecordingData();

    return recording;
}

String InspectorCanvas::getCanvasContentAsDataURL(ErrorString& errorString)
{
    auto* node = canvasElement();
    if (!node) {
        errorString = "Missing HTMLCanvasElement of canvas for given canvasId"_s;
        return emptyString();
    }

#if ENABLE(WEBGL)
    auto* context = node->renderingContext();
    if (is<WebGLRenderingContextBase>(context))
        downcast<WebGLRenderingContextBase>(*context).setPreventBufferClearForInspector(true);
#endif

    ExceptionOr<UncachedString> result = node->toDataURL("image/png"_s);

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context))
        downcast<WebGLRenderingContextBase>(*context).setPreventBufferClearForInspector(false);
#endif

    if (result.hasException()) {
        errorString = result.releaseException().releaseMessage();
        return emptyString();
    }

    return result.releaseReturnValue().string;
}

void InspectorCanvas::appendActionSnapshotIfNeeded()
{
    if (!m_lastRecordedAction)
        return;

    if (m_contentChanged) {
        m_bufferUsed -= m_lastRecordedAction->memoryCost();

        ErrorString ignored;
        m_lastRecordedAction->addItem(indexForData(getCanvasContentAsDataURL(ignored)));

        m_bufferUsed += m_lastRecordedAction->memoryCost();
    }

    m_lastRecordedAction = nullptr;
    m_contentChanged = false;
}

int InspectorCanvas::indexForData(DuplicateDataVariant data)
{
    size_t index = m_indexedDuplicateData.findMatching([&] (auto item) {
        if (data == item)
            return true;

        auto traceA = WTF::get_if<RefPtr<ScriptCallStack>>(data);
        auto traceB = WTF::get_if<RefPtr<ScriptCallStack>>(item);
        if (traceA && *traceA && traceB && *traceB)
            return (*traceA)->isEqual((*traceB).get());

        return false;
    });
    if (index != notFound) {
        ASSERT(index < static_cast<size_t>(std::numeric_limits<int>::max()));
        return static_cast<int>(index);
    }

    if (!m_serializedDuplicateData)
        m_serializedDuplicateData = JSON::ArrayOf<JSON::Value>::create();

    RefPtr<JSON::Value> item;
    WTF::switchOn(data,
        [&] (const RefPtr<HTMLImageElement>& imageElement) {
            String dataURL = "data:,"_s;

            if (CachedImage* cachedImage = imageElement->cachedImage()) {
                Image* image = cachedImage->image();
                if (image && image != &Image::nullImage()) {
                    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(image->size(), RenderingMode::Unaccelerated);
                    imageBuffer->context().drawImage(*image, FloatPoint(0, 0));
                    dataURL = imageBuffer->toDataURL("image/png");
                }
            }

            index = indexForData(dataURL);
        },
#if ENABLE(VIDEO)
        [&] (RefPtr<HTMLVideoElement>& videoElement) {
            String dataURL = "data:,"_s;

            unsigned videoWidth = videoElement->videoWidth();
            unsigned videoHeight = videoElement->videoHeight();
            std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingMode::Unaccelerated);
            if (imageBuffer) {
                videoElement->paintCurrentFrameInContext(imageBuffer->context(), FloatRect(0, 0, videoWidth, videoHeight));
                dataURL = imageBuffer->toDataURL("image/png");
            }

            index = indexForData(dataURL);
        },
#endif
        [&] (RefPtr<HTMLCanvasElement>& canvasElement) {
            String dataURL = "data:,"_s;

            ExceptionOr<UncachedString> result = canvasElement->toDataURL("image/png"_s);
            if (!result.hasException())
                dataURL = result.releaseReturnValue().string;

            index = indexForData(dataURL);
        },
        [&] (const RefPtr<CanvasGradient>& canvasGradient) { item = buildArrayForCanvasGradient(*canvasGradient); },
        [&] (const RefPtr<CanvasPattern>& canvasPattern) { item = buildArrayForCanvasPattern(*canvasPattern); },
        [&] (const RefPtr<ImageData>& imageData) { item = buildArrayForImageData(*imageData); },
        [&] (RefPtr<ImageBitmap>& imageBitmap) {
            index = indexForData(imageBitmap->buffer()->toDataURL("image/png"));
        },
        [&] (const RefPtr<ScriptCallStack>& scriptCallStack) {
            auto array = JSON::ArrayOf<double>::create();
            for (size_t i = 0; i < scriptCallStack->size(); ++i)
                array->addItem(indexForData(scriptCallStack->at(i)));
            item = WTFMove(array);
        },
#if ENABLE(CSS_TYPED_OM)
        [&] (const RefPtr<TypedOMCSSImageValue>& cssImageValue) {
            String dataURL = "data:,"_s;

            if (auto* cachedImage = cssImageValue->image()) {
                auto* image = cachedImage->image();
                if (image && image != &Image::nullImage()) {
                    auto imageBuffer = ImageBuffer::create(image->size(), RenderingMode::Unaccelerated);
                    imageBuffer->context().drawImage(*image, FloatPoint(0, 0));
                    dataURL = imageBuffer->toDataURL("image/png");
                }
            }

            index = indexForData(dataURL);
        },
#endif
        [&] (const ScriptCallFrame& scriptCallFrame) {
            auto array = JSON::ArrayOf<double>::create();
            array->addItem(indexForData(scriptCallFrame.functionName()));
            array->addItem(indexForData(scriptCallFrame.sourceURL()));
            array->addItem(static_cast<int>(scriptCallFrame.lineNumber()));
            array->addItem(static_cast<int>(scriptCallFrame.columnNumber()));
            item = WTFMove(array);
        },
#if ENABLE(OFFSCREEN_CANVAS)
        [&] (const RefPtr<OffscreenCanvas> offscreenCanvas) {
            String dataURL = "data:,"_s;

            if (offscreenCanvas->originClean() && offscreenCanvas->hasCreatedImageBuffer()) {
                if (auto *buffer = offscreenCanvas->buffer())
                    dataURL = buffer->toDataURL("image/png");
            }

            index = indexForData(dataURL);
        },
#endif
        [&] (const String& value) { item = JSON::Value::create(value); }
    );

    if (item) {
        m_bufferUsed += item->memoryCost();
        m_serializedDuplicateData->addItem(WTFMove(item));

        m_indexedDuplicateData.append(data);
        index = m_indexedDuplicateData.size() - 1;
    }

    ASSERT(index < static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(index);
}

String InspectorCanvas::stringIndexForKey(const String& key)
{
    return String::number(indexForData(key));
}

static Ref<JSON::ArrayOf<double>> buildArrayForAffineTransform(const AffineTransform& affineTransform)
{
    auto array = JSON::ArrayOf<double>::create();
    array->addItem(affineTransform.a());
    array->addItem(affineTransform.b());
    array->addItem(affineTransform.c());
    array->addItem(affineTransform.d());
    array->addItem(affineTransform.e());
    array->addItem(affineTransform.f());
    return array;
}

template<typename T> static Ref<JSON::ArrayOf<JSON::Value>> buildArrayForVector(const Vector<T>& vector)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : vector)
        array->addItem(item);
    return array;
}

Ref<Inspector::Protocol::Recording::InitialState> InspectorCanvas::buildInitialState()
{
    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    auto initialStatePayload = Inspector::Protocol::Recording::InitialState::create().release();

    auto attributesPayload = JSON::Object::create();
    attributesPayload->setInteger("width"_s, context->canvasBase().width());
    attributesPayload->setInteger("height"_s, context->canvasBase().height());

    auto statesPayload = JSON::ArrayOf<JSON::Object>::create();

    auto parametersPayload = JSON::ArrayOf<JSON::Value>::create();

    if (is<CanvasRenderingContext2D>(context)) {
        auto& context2d = downcast<CanvasRenderingContext2D>(*context);
        for (auto& state : context2d.stateStack()) {
            auto statePayload = JSON::Object::create();

            statePayload->setArray(stringIndexForKey("setTransform"_s), buildArrayForAffineTransform(state.transform));
            statePayload->setDouble(stringIndexForKey("globalAlpha"_s), context2d.globalAlpha());
            statePayload->setInteger(stringIndexForKey("globalCompositeOperation"_s), indexForData(context2d.globalCompositeOperation()));
            statePayload->setDouble(stringIndexForKey("lineWidth"_s), context2d.lineWidth());
            statePayload->setInteger(stringIndexForKey("lineCap"_s), indexForData(convertEnumerationToString(context2d.lineCap())));
            statePayload->setInteger(stringIndexForKey("lineJoin"_s), indexForData(convertEnumerationToString(context2d.lineJoin())));
            statePayload->setDouble(stringIndexForKey("miterLimit"_s), context2d.miterLimit());
            statePayload->setDouble(stringIndexForKey("shadowOffsetX"_s), context2d.shadowOffsetX());
            statePayload->setDouble(stringIndexForKey("shadowOffsetY"_s), context2d.shadowOffsetY());
            statePayload->setDouble(stringIndexForKey("shadowBlur"_s), context2d.shadowBlur());
            statePayload->setInteger(stringIndexForKey("shadowColor"_s), indexForData(context2d.shadowColor()));

            // The parameter to `setLineDash` is itself an array, so we need to wrap the parameters
            // list in an array to allow spreading.
            auto setLineDash = JSON::ArrayOf<JSON::Value>::create();
            setLineDash->addItem(buildArrayForVector(state.lineDash));
            statePayload->setArray(stringIndexForKey("setLineDash"_s), WTFMove(setLineDash));

            statePayload->setDouble(stringIndexForKey("lineDashOffset"_s), context2d.lineDashOffset());
            statePayload->setInteger(stringIndexForKey("font"_s), indexForData(context2d.font()));
            statePayload->setInteger(stringIndexForKey("textAlign"_s), indexForData(convertEnumerationToString(context2d.textAlign())));
            statePayload->setInteger(stringIndexForKey("textBaseline"_s), indexForData(convertEnumerationToString(context2d.textBaseline())));
            statePayload->setInteger(stringIndexForKey("direction"_s), indexForData(convertEnumerationToString(context2d.direction())));

            int strokeStyleIndex;
            if (auto canvasGradient = state.strokeStyle.canvasGradient())
                strokeStyleIndex = indexForData(canvasGradient);
            else if (auto canvasPattern = state.strokeStyle.canvasPattern())
                strokeStyleIndex = indexForData(canvasPattern);
            else
                strokeStyleIndex = indexForData(state.strokeStyle.color());
            statePayload->setInteger(stringIndexForKey("strokeStyle"_s), strokeStyleIndex);

            int fillStyleIndex;
            if (auto canvasGradient = state.fillStyle.canvasGradient())
                fillStyleIndex = indexForData(canvasGradient);
            else if (auto canvasPattern = state.fillStyle.canvasPattern())
                fillStyleIndex = indexForData(canvasPattern);
            else
                fillStyleIndex = indexForData(state.fillStyle.color());
            statePayload->setInteger(stringIndexForKey("fillStyle"_s), fillStyleIndex);

            statePayload->setBoolean(stringIndexForKey("imageSmoothingEnabled"_s), context2d.imageSmoothingEnabled());
            statePayload->setInteger(stringIndexForKey("imageSmoothingQuality"_s), indexForData(convertEnumerationToString(context2d.imageSmoothingQuality())));

            auto setPath = JSON::ArrayOf<JSON::Value>::create();
            setPath->addItem(indexForData(buildStringFromPath(context2d.getPath()->path())));
            statePayload->setArray(stringIndexForKey("setPath"_s), WTFMove(setPath));

            statesPayload->addItem(WTFMove(statePayload));
        }
    }
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContextBase>(context)) {
        auto& contextWebGLBase = downcast<WebGLRenderingContextBase>(*context);
        if (Optional<WebGLContextAttributes> webGLContextAttributes = contextWebGLBase.getContextAttributes()) {
            auto webGLContextAttributesPayload = JSON::Object::create();
            webGLContextAttributesPayload->setBoolean("alpha"_s, webGLContextAttributes->alpha);
            webGLContextAttributesPayload->setBoolean("depth"_s, webGLContextAttributes->depth);
            webGLContextAttributesPayload->setBoolean("stencil"_s, webGLContextAttributes->stencil);
            webGLContextAttributesPayload->setBoolean("antialias"_s, webGLContextAttributes->antialias);
            webGLContextAttributesPayload->setBoolean("premultipliedAlpha"_s, webGLContextAttributes->premultipliedAlpha);
            webGLContextAttributesPayload->setBoolean("preserveDrawingBuffer"_s, webGLContextAttributes->preserveDrawingBuffer);
            webGLContextAttributesPayload->setBoolean("failIfMajorPerformanceCaveat"_s, webGLContextAttributes->failIfMajorPerformanceCaveat);
            parametersPayload->addItem(WTFMove(webGLContextAttributesPayload));
        }
    }
#endif

    initialStatePayload->setAttributes(WTFMove(attributesPayload));

    if (statesPayload->length())
        initialStatePayload->setStates(WTFMove(statesPayload));

    if (parametersPayload->length())
        initialStatePayload->setParameters(WTFMove(parametersPayload));

    ErrorString ignored;
    initialStatePayload->setContent(getCanvasContentAsDataURL(ignored));

    return initialStatePayload;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildAction(const String& name, std::initializer_list<RecordCanvasActionVariant>&& parameters)
{
    auto action = JSON::ArrayOf<JSON::Value>::create();
    action->addItem(indexForData(name));

    auto parametersData = JSON::ArrayOf<JSON::Value>::create();
    auto swizzleTypes = JSON::ArrayOf<int>::create();

    auto addParameter = [&parametersData, &swizzleTypes] (auto value, RecordingSwizzleTypes swizzleType) {
        parametersData->addItem(value);
        swizzleTypes->addItem(static_cast<int>(swizzleType));
    };

    // Declared before it's initialized so it can be used recursively.
    Function<void(const RecordCanvasActionVariant&)> parseParameter;
    parseParameter = [&] (const auto& parameter) {
        WTF::switchOn(parameter,
            [&] (CanvasDirection value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasFillRule value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasLineCap value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasLineJoin value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasTextAlign value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasTextBaseline value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (ImageSmoothingQuality value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (const DOMMatrix2DInit& value) {
                auto array = JSON::ArrayOf<double>::create();
                array->addItem(value.a.valueOr(1));
                array->addItem(value.b.valueOr(0));
                array->addItem(value.c.valueOr(0));
                array->addItem(value.d.valueOr(1));
                array->addItem(value.e.valueOr(0));
                array->addItem(value.f.valueOr(0));
                addParameter(array.ptr(), RecordingSwizzleTypes::DOMMatrix);
            },
            [&] (const Element* value) {
                if (value) {
                    // Elements are not serializable, so add a string as a placeholder since the actual
                    // element cannot be reconstructed in the frontend.
                    addParameter(indexForData("Element"), RecordingSwizzleTypes::None);
                }
            },
            [&] (HTMLImageElement* value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image); },
            [&] (ImageBitmap* value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::ImageBitmap); },
            [&] (ImageData* value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::ImageData); },
            [&] (const Path2D* value) {
                if (value)
                    addParameter(indexForData(buildStringFromPath(value->path())), RecordingSwizzleTypes::Path2D); },
#if ENABLE(WEBGL)
            // FIXME: <https://webkit.org/b/176009> Web Inspector: send data for WebGL objects during a recording instead of a placeholder string
            [&] (const WebGLBuffer* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLBuffer);
            },
            [&] (const WebGLFramebuffer* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLFramebuffer);
            },
            [&] (const WebGLProgram* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLProgram);
            },
            [&] (const WebGLQuery* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLQuery);
            },
            [&] (const WebGLRenderbuffer* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLRenderbuffer);
            },
            [&] (const WebGLSampler* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLSampler);
            },
            [&] (const WebGLShader* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLShader);
            },
            [&] (const WebGLSync* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLSync);
            },
            [&] (const WebGLTexture* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLTexture);
            },
            [&] (const WebGLTransformFeedback* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLTransformFeedback);
            },
            [&] (const WebGLUniformLocation* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLUniformLocation);
            },
            [&] (const WebGLVertexArrayObject* value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::WebGLVertexArrayObject);
            },
#endif
            [&] (const RefPtr<ArrayBuffer>& value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::TypedArray);
            },
            [&] (const RefPtr<ArrayBufferView>& value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::TypedArray);
            },
            [&] (const RefPtr<CanvasGradient>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::CanvasGradient);
            },
            [&] (const RefPtr<CanvasPattern>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::CanvasPattern);
            },
            [&] (const RefPtr<Float32Array>& value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::TypedArray);
            },
            [&] (const RefPtr<HTMLCanvasElement>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image);
            },
            [&] (const RefPtr<HTMLImageElement>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image);
            },
#if ENABLE(OFFSCREEN_CANVAS)
            [&] (const RefPtr<OffscreenCanvas>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image);
            },
#endif
#if ENABLE(VIDEO)
            [&] (const RefPtr<HTMLVideoElement>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image);
            },
#endif
#if ENABLE(CSS_TYPED_OM)
            [&] (const RefPtr<TypedOMCSSImageValue>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::Image);
            },
#endif
            [&] (const RefPtr<ImageBitmap>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::ImageBitmap);
            },
            [&] (const RefPtr<ImageData>& value) {
                if (value)
                    addParameter(indexForData(value), RecordingSwizzleTypes::ImageData);
            },
            [&] (const RefPtr<Int32Array>& value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::TypedArray);
            },
            [&] (const RefPtr<Uint32Array>& value) {
                if (value)
                    addParameter(0, RecordingSwizzleTypes::TypedArray);
            },
            [&] (const CanvasImageSource& value) {
                WTF::visit(parseParameter, value);
            },
            [&] (const CanvasRenderingContext2DBase::StyleVariant& value) {
                WTF::visit(parseParameter, value);
            },
#if ENABLE(WEBGL)
            [&] (const WebGLRenderingContextBase::BufferDataSource& value) {
                WTF::visit(parseParameter, value);
            },
            [&] (const Optional<WebGLRenderingContextBase::BufferDataSource>& value) {
                if (value)
                    parseParameter(value.value());
            },
            [&] (const WebGLRenderingContextBase::TexImageSource& value) {
                WTF::visit(parseParameter, value);
            },
            [&] (const Optional<WebGLRenderingContextBase::TexImageSource>& value) {
                if (value)
                    parseParameter(value.value());
            },
#endif
            [&] (const Vector<String>& value) {
                auto deduplicated = value.map([&] (const String& item) {
                    return indexForData(item);
                });
                addParameter(buildArrayForVector(deduplicated).ptr(), RecordingSwizzleTypes::String);
            },
            [&] (const Vector<float>& value) { addParameter(buildArrayForVector(value).ptr(), RecordingSwizzleTypes::Array); },
            [&] (const Vector<uint32_t>& value) {
                auto mapped = value.map([&] (uint32_t item) {
                    return static_cast<double>(item);
                });
                addParameter(buildArrayForVector(mapped).ptr(), RecordingSwizzleTypes::Array);
            },
            [&] (const Vector<int32_t>& value) { addParameter(buildArrayForVector(value).ptr(), RecordingSwizzleTypes::Array); },
#if ENABLE(WEBGL)
            [&] (const WebGLRenderingContextBase::Float32List::VariantType& value) {
                WTF::visit(parseParameter, value);
            },
            [&] (const WebGLRenderingContextBase::Int32List::VariantType& value) {
                WTF::visit(parseParameter, value);
            },
#endif
#if ENABLE(WEBGL2)
            [&] (const WebGL2RenderingContext::Uint32List::VariantType& value) {
                WTF::visit(parseParameter, value);
            },
#endif
            [&] (const String& value) { addParameter(indexForData(value), RecordingSwizzleTypes::String); },
            [&] (double value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (float value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (const Optional<float>& value) {
                if (value)
                    parseParameter(value.value());
            },
            [&] (uint64_t value) { addParameter(static_cast<double>(value), RecordingSwizzleTypes::Number); },
            [&] (int64_t value) { addParameter(static_cast<double>(value), RecordingSwizzleTypes::Number); },
            [&] (uint32_t value) { addParameter(static_cast<double>(value), RecordingSwizzleTypes::Number); },
            [&] (int32_t value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (uint8_t value) { addParameter(static_cast<int>(value), RecordingSwizzleTypes::Number); },
            [&] (bool value) { addParameter(value, RecordingSwizzleTypes::Boolean); }
        );
    };
    for (auto& parameter : parameters)
        parseParameter(parameter);

    action->addItem(WTFMove(parametersData));
    action->addItem(WTFMove(swizzleTypes));

    auto trace = Inspector::createScriptCallStack(JSExecState::currentState(), Inspector::ScriptCallStack::maxCallStackSizeToCapture);
    action->addItem(indexForData(trace.ptr()));

    return action;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForCanvasGradient(const CanvasGradient& canvasGradient)
{
    const auto& gradient = canvasGradient.gradient();
    
    String type = gradient.type() == Gradient::Type::Radial ? "radial-gradient"_s : gradient.type() == Gradient::Type::Linear ? "linear-gradient"_s : "conic-gradient"_s;

    auto parameters = JSON::ArrayOf<float>::create();
    WTF::switchOn(gradient.data(),
        [&parameters] (const Gradient::LinearData& data) {
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
        },
        [&parameters] (const Gradient::RadialData& data) {
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.startRadius);
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
            parameters->addItem(data.endRadius);
        },
        [&parameters] (const Gradient::ConicData& data) {
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.angleRadians);
        }
    );

    auto stops = JSON::ArrayOf<JSON::Value>::create();
    for (auto& colorStop : gradient.stops()) {
        auto stop = JSON::ArrayOf<JSON::Value>::create();
        stop->addItem(colorStop.offset);
        stop->addItem(indexForData(colorStop.color.cssText()));
        stops->addItem(WTFMove(stop));
    }

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(indexForData(type));
    array->addItem(WTFMove(parameters));
    array->addItem(WTFMove(stops));
    return array;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForCanvasPattern(const CanvasPattern& canvasPattern)
{
    Image& tileImage = canvasPattern.pattern().tileImage();
    auto imageBuffer = ImageBuffer::create(tileImage.size(), RenderingMode::Unaccelerated);
    imageBuffer->context().drawImage(tileImage, FloatPoint(0, 0));

    String repeat;
    bool repeatX = canvasPattern.pattern().repeatX();
    bool repeatY = canvasPattern.pattern().repeatY();
    if (repeatX && repeatY)
        repeat = "repeat"_s;
    else if (repeatX && !repeatY)
        repeat = "repeat-x"_s;
    else if (!repeatX && repeatY)
        repeat = "repeat-y"_s;
    else
        repeat = "no-repeat"_s;

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(indexForData(imageBuffer->toDataURL("image/png")));
    array->addItem(indexForData(repeat));
    return array;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForImageData(const ImageData& imageData)
{
    auto data = JSON::ArrayOf<int>::create();
    for (size_t i = 0; i < imageData.data()->length(); ++i)
        data->addItem(imageData.data()->item(i));

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(WTFMove(data));
    array->addItem(imageData.width());
    array->addItem(imageData.height());
    return array;
}

} // namespace WebCore


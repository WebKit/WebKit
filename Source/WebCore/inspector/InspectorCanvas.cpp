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
#include "CanvasRenderingContext2D.h"
#include "Document.h"
#include "FloatPoint.h"
#include "Frame.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSCanvasDirection.h"
#include "JSCanvasFillRule.h"
#include "JSCanvasLineCap.h"
#include "JSCanvasLineJoin.h"
#include "JSCanvasTextAlign.h"
#include "JSCanvasTextBaseline.h"
#include "JSImageSmoothingQuality.h"
#include "JSMainThreadExecState.h"
#include "Path2D.h"
#include "Pattern.h"
#include "RecordingSwizzleTypes.h"
#include "SVGPathUtilities.h"
#include "StringAdaptors.h"
#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"
#endif
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#if ENABLE(WEBGPU)
#include "WebGPURenderingContext.h"
#endif
#include <inspector/IdentifiersFactory.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <wtf/CurrentTime.h>


namespace WebCore {
using namespace Inspector;

Ref<InspectorCanvas> InspectorCanvas::create(HTMLCanvasElement& canvas, const String& cssCanvasName)
{
    return adoptRef(*new InspectorCanvas(canvas, cssCanvasName));
}

InspectorCanvas::InspectorCanvas(HTMLCanvasElement& canvas, const String& cssCanvasName)
    : m_identifier("canvas:" + IdentifiersFactory::createIdentifier())
    , m_canvas(canvas)
    , m_cssCanvasName(cssCanvasName)
{
}

InspectorCanvas::~InspectorCanvas()
{
    resetRecordingData();
}

void InspectorCanvas::resetRecordingData()
{
    m_initialState = nullptr;
    m_frames = nullptr;
    m_currentActions = nullptr;
    m_actionNeedingSnapshot = nullptr;
    m_serializedDuplicateData = nullptr;
    m_indexedDuplicateData.clear();
    m_bufferLimit = 100 * 1024 * 1024;
    m_bufferUsed = 0;
    m_singleFrame = true;

    m_canvas.renderingContext()->setCallTracingActive(false);
}

bool InspectorCanvas::hasRecordingData() const
{
    return m_initialState && m_frames;
}

static bool shouldSnapshotWebGLAction(const String& name)
{
    return name == "clear"
        || name == "drawArrays"
        || name == "drawElements";
}

void InspectorCanvas::recordAction(const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    if (!hasRecordingData()) {
        m_initialState = buildInitialState();
        m_bufferUsed += m_initialState->memoryCost();

        m_frames = Inspector::Protocol::Array<Inspector::Protocol::Recording::Frame>::create();
    }

    if (!m_currentActions) {
        m_currentActions = Inspector::Protocol::Array<InspectorValue>::create();

        auto frame = Inspector::Protocol::Recording::Frame::create()
            .setActions(m_currentActions)
            .release();

        m_frames->addItem(WTFMove(frame));

        m_currentFrameStartTime = monotonicallyIncreasingTimeMS();
    }

    appendActionSnapshotIfNeeded();

    auto action = buildAction(name, WTFMove(parameters));
    m_bufferUsed += action->memoryCost();
    m_currentActions->addItem(action);

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(m_canvas.renderingContext()) && shouldSnapshotWebGLAction(name))
        m_actionNeedingSnapshot = action;
#endif
}

RefPtr<Inspector::Protocol::Recording::InitialState>&& InspectorCanvas::releaseInitialState()
{
    return WTFMove(m_initialState);
}

RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Recording::Frame>>&& InspectorCanvas::releaseFrames()
{
    appendActionSnapshotIfNeeded();

    return WTFMove(m_frames);
}

RefPtr<Inspector::Protocol::Array<InspectorValue>>&& InspectorCanvas::releaseData()
{
    m_indexedDuplicateData.clear();
    return WTFMove(m_serializedDuplicateData);
}

void InspectorCanvas::finalizeFrame()
{
    if (m_frames->length() && !std::isnan(m_currentFrameStartTime)) {
        auto currentFrame = static_cast<Inspector::Protocol::Recording::Frame*>(m_frames->get(m_frames->length() - 1).get());
        currentFrame->setDuration(monotonicallyIncreasingTimeMS() - m_currentFrameStartTime);

        m_currentFrameStartTime = NAN;
    }

    m_currentActions = nullptr;
}

void InspectorCanvas::markCurrentFrameIncomplete()
{
    if (!m_currentActions)
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

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(InstrumentingAgents& instrumentingAgents, bool captureBacktrace)
{
    Document& document = m_canvas.document();
    Frame* frame = document.frame();
    CanvasRenderingContext* context = m_canvas.renderingContext();

    Inspector::Protocol::Canvas::ContextType contextType;
    if (is<CanvasRenderingContext2D>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGL2;
#endif
#if ENABLE(WEBGPU)
    else if (is<WebGPURenderingContext>(context))
        contextType = Inspector::Protocol::Canvas::ContextType::WebGPU;
#endif
    else {
        ASSERT_NOT_REACHED();
        contextType = Inspector::Protocol::Canvas::ContextType::Canvas2D;
    }

    auto canvas = Inspector::Protocol::Canvas::Canvas::create()
        .setCanvasId(m_identifier)
        .setFrameId(instrumentingAgents.inspectorPageAgent()->frameId(frame))
        .setContextType(contextType)
        .release();

    if (!m_cssCanvasName.isEmpty())
        canvas->setCssCanvasName(m_cssCanvasName);
    else {
        InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent();
        int nodeId = domAgent->boundNodeId(&m_canvas);
        if (!nodeId) {
            if (int documentNodeId = domAgent->boundNodeId(&m_canvas.document())) {
                ErrorString ignored;
                nodeId = domAgent->pushNodeToFrontend(ignored, documentNodeId, &m_canvas);
            }
        }

        if (nodeId)
            canvas->setNodeId(nodeId);
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        if (std::optional<WebGLContextAttributes> attributes = downcast<WebGLRenderingContextBase>(context)->getContextAttributes()) {
            canvas->setContextAttributes(Inspector::Protocol::Canvas::ContextAttributes::create()
                .setAlpha(attributes->alpha)
                .setDepth(attributes->depth)
                .setStencil(attributes->stencil)
                .setAntialias(attributes->antialias)
                .setPremultipliedAlpha(attributes->premultipliedAlpha)
                .setPreserveDrawingBuffer(attributes->preserveDrawingBuffer)
                .setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat)
                .release());
        }
    }
#endif

    if (size_t memoryCost = m_canvas.memoryCost())
        canvas->setMemoryCost(memoryCost);

    if (captureBacktrace) {
        auto stackTrace = Inspector::createScriptCallStack(JSMainThreadExecState::currentState(), Inspector::ScriptCallStack::maxCallStackSizeToCapture);
        canvas->setBacktrace(stackTrace->buildInspectorArray());
    }

    return canvas;
}

void InspectorCanvas::appendActionSnapshotIfNeeded()
{
    if (!m_actionNeedingSnapshot)
        return;

    m_actionNeedingSnapshot->addItem(indexForData(getCanvasContentAsDataURL()));
    m_actionNeedingSnapshot = nullptr;
}

String InspectorCanvas::getCanvasContentAsDataURL()
{
#if ENABLE(WEBGL)
    CanvasRenderingContext* canvasRenderingContext = m_canvas.renderingContext();
    if (is<WebGLRenderingContextBase>(canvasRenderingContext))
        downcast<WebGLRenderingContextBase>(canvasRenderingContext)->setPreventBufferClearForInspector(true);
#endif

    ExceptionOr<UncachedString> result = m_canvas.toDataURL(ASCIILiteral("image/png"));

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(canvasRenderingContext))
        downcast<WebGLRenderingContextBase>(canvasRenderingContext)->setPreventBufferClearForInspector(false);
#endif

    if (result.hasException())
        return String();

    return result.releaseReturnValue().string;
}

int InspectorCanvas::indexForData(DuplicateDataVariant data)
{
    size_t index = m_indexedDuplicateData.find(data);
    if (index != notFound) {
        ASSERT(index < std::numeric_limits<int>::max());
        return static_cast<int>(index);
    }

    if (!m_serializedDuplicateData)
        m_serializedDuplicateData = Inspector::Protocol::Array<InspectorValue>::create();

    RefPtr<InspectorValue> item;
    WTF::switchOn(data,
        [&] (const HTMLImageElement* imageElement) {
            String dataURL = ASCIILiteral("data:,");

            if (CachedImage* cachedImage = imageElement->cachedImage()) {
                Image* image = cachedImage->image();
                if (image && image != &Image::nullImage()) {
                    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(image->size(), RenderingMode::Unaccelerated);
                    imageBuffer->context().drawImage(*image, FloatPoint(0, 0));
                    dataURL = imageBuffer->toDataURL("image/png");
                }
            }

            item = InspectorValue::create(dataURL);
        },
#if ENABLE(VIDEO)
        [&] (HTMLVideoElement* videoElement) {
            String dataURL = ASCIILiteral("data:,");

            unsigned videoWidth = videoElement->videoWidth();
            unsigned videoHeight = videoElement->videoHeight();
            std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingMode::Unaccelerated);
            if (imageBuffer) {
                videoElement->paintCurrentFrameInContext(imageBuffer->context(), FloatRect(0, 0, videoWidth, videoHeight));
                dataURL = imageBuffer->toDataURL("image/png");
            }

            item = InspectorValue::create(dataURL);
        },
#endif
        [&] (HTMLCanvasElement* canvasElement) {
            String dataURL = ASCIILiteral("data:,");

            ExceptionOr<UncachedString> result = canvasElement->toDataURL(ASCIILiteral("image/png"));
            if (!result.hasException())
                dataURL = result.releaseReturnValue().string;

            item = InspectorValue::create(dataURL);
        },
        [&] (const CanvasGradient* canvasGradient) { item = buildArrayForCanvasGradient(*canvasGradient); },
        [&] (const CanvasPattern* canvasPattern) { item = buildArrayForCanvasPattern(*canvasPattern); },
        [&] (const ImageData* imageData) { item = buildArrayForImageData(*imageData); },
        [&] (const ImageBitmap* imageBitmap) { item = buildArrayForImageBitmap(*imageBitmap); },
        [&] (const ScriptCallFrame& scriptCallFrame) {
            auto array = Inspector::Protocol::Array<double>::create();
            array->addItem(indexForData(scriptCallFrame.functionName()));
            array->addItem(indexForData(scriptCallFrame.sourceURL()));
            array->addItem(static_cast<int>(scriptCallFrame.lineNumber()));
            array->addItem(static_cast<int>(scriptCallFrame.columnNumber()));
            item = WTFMove(array);
        },
        [&] (const String& value) { item = InspectorValue::create(value); }
    );

    m_bufferUsed += item->memoryCost();
    m_serializedDuplicateData->addItem(WTFMove(item));

    m_indexedDuplicateData.append(data);
    index = m_indexedDuplicateData.size() - 1;

    ASSERT(index < std::numeric_limits<int>::max());
    return static_cast<int>(index);
}

static RefPtr<Inspector::Protocol::Array<double>> buildArrayForAffineTransform(const AffineTransform& affineTransform)
{
    RefPtr<Inspector::Protocol::Array<double>> array = Inspector::Protocol::Array<double>::create();
    array->addItem(affineTransform.a());
    array->addItem(affineTransform.b());
    array->addItem(affineTransform.c());
    array->addItem(affineTransform.d());
    array->addItem(affineTransform.e());
    array->addItem(affineTransform.f());
    return array;
}

template <typename T>
static RefPtr<Inspector::Protocol::Array<InspectorValue>> buildArrayForVector(const Vector<T>& vector)
{
    RefPtr<Inspector::Protocol::Array<InspectorValue>> array = Inspector::Protocol::Array<InspectorValue>::create();
    for (auto& item : vector)
        array->addItem(item);
    return array;
}

RefPtr<Inspector::Protocol::Recording::InitialState> InspectorCanvas::buildInitialState()
{
    RefPtr<Inspector::Protocol::Recording::InitialState> initialState = Inspector::Protocol::Recording::InitialState::create()
        .release();

    auto attributes = InspectorObject::create();
    attributes->setInteger(ASCIILiteral("width"), canvas().width());
    attributes->setInteger(ASCIILiteral("height"), canvas().height());

    auto parameters = Inspector::Protocol::Array<InspectorValue>::create();

    CanvasRenderingContext* canvasRenderingContext = canvas().renderingContext();
    if (is<CanvasRenderingContext2D>(canvasRenderingContext)) {
        const CanvasRenderingContext2D* context2d = downcast<CanvasRenderingContext2D>(canvasRenderingContext);
        const CanvasRenderingContext2D::State& state = context2d->state();

        attributes->setArray(ASCIILiteral("setTransform"), buildArrayForAffineTransform(state.transform));
        attributes->setDouble(ASCIILiteral("globalAlpha"), context2d->globalAlpha());
        attributes->setInteger(ASCIILiteral("globalCompositeOperation"), indexForData(context2d->globalCompositeOperation()));
        attributes->setDouble(ASCIILiteral("lineWidth"), context2d->lineWidth());
        attributes->setInteger(ASCIILiteral("lineCap"), indexForData(convertEnumerationToString(context2d->lineCap())));
        attributes->setInteger(ASCIILiteral("lineJoin"), indexForData(convertEnumerationToString(context2d->lineJoin())));
        attributes->setDouble(ASCIILiteral("miterLimit"), context2d->miterLimit());
        attributes->setDouble(ASCIILiteral("shadowOffsetX"), context2d->shadowOffsetX());
        attributes->setDouble(ASCIILiteral("shadowOffsetY"), context2d->shadowOffsetY());
        attributes->setDouble(ASCIILiteral("shadowBlur"), context2d->shadowBlur());
        attributes->setInteger(ASCIILiteral("shadowColor"), indexForData(context2d->shadowColor()));

        // The parameter to `setLineDash` is itself an array, so we need to wrap the parameters
        // list in an array to allow spreading.
        auto setLineDash = Inspector::Protocol::Array<InspectorValue>::create();
        setLineDash->addItem(buildArrayForVector(state.lineDash));
        attributes->setArray(ASCIILiteral("setLineDash"), WTFMove(setLineDash));

        attributes->setDouble(ASCIILiteral("lineDashOffset"), context2d->lineDashOffset());
        attributes->setInteger(ASCIILiteral("font"), indexForData(context2d->font()));
        attributes->setInteger(ASCIILiteral("textAlign"), indexForData(convertEnumerationToString(context2d->textAlign())));
        attributes->setInteger(ASCIILiteral("textBaseline"), indexForData(convertEnumerationToString(context2d->textBaseline())));
        attributes->setInteger(ASCIILiteral("direction"), indexForData(convertEnumerationToString(context2d->direction())));

        int strokeStyleIndex;
        if (auto canvasGradient = state.strokeStyle.canvasGradient())
            strokeStyleIndex = indexForData(canvasGradient.get());
        else if (auto canvasPattern = state.strokeStyle.canvasPattern())
            strokeStyleIndex = indexForData(canvasPattern.get());
        else
            strokeStyleIndex = indexForData(state.strokeStyle.color());
        attributes->setInteger(ASCIILiteral("strokeStyle"), strokeStyleIndex);

        int fillStyleIndex;
        if (auto canvasGradient = state.fillStyle.canvasGradient())
            fillStyleIndex = indexForData(canvasGradient.get());
        else if (auto canvasPattern = state.fillStyle.canvasPattern())
            fillStyleIndex = indexForData(canvasPattern.get());
        else
            fillStyleIndex = indexForData(state.fillStyle.color());
        attributes->setInteger(ASCIILiteral("fillStyle"), fillStyleIndex);

        attributes->setBoolean(ASCIILiteral("imageSmoothingEnabled"), context2d->imageSmoothingEnabled());
        attributes->setInteger(ASCIILiteral("imageSmoothingQuality"), indexForData(convertEnumerationToString(context2d->imageSmoothingQuality())));

        auto setPath = Inspector::Protocol::Array<InspectorValue>::create();
        setPath->addItem(indexForData(buildStringFromPath(context2d->getPath()->path())));
        attributes->setArray(ASCIILiteral("setPath"), WTFMove(setPath));
    }
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContextBase>(canvasRenderingContext)) {
        WebGLRenderingContextBase* contextWebGLBase = downcast<WebGLRenderingContextBase>(canvasRenderingContext);
        if (std::optional<WebGLContextAttributes> attributes = contextWebGLBase->getContextAttributes()) {
            RefPtr<InspectorObject> contextAttributes = InspectorObject::create();
            contextAttributes->setBoolean(ASCIILiteral("alpha"), attributes->alpha);
            contextAttributes->setBoolean(ASCIILiteral("depth"), attributes->depth);
            contextAttributes->setBoolean(ASCIILiteral("stencil"), attributes->stencil);
            contextAttributes->setBoolean(ASCIILiteral("antialias"), attributes->antialias);
            contextAttributes->setBoolean(ASCIILiteral("premultipliedAlpha"), attributes->premultipliedAlpha);
            contextAttributes->setBoolean(ASCIILiteral("preserveDrawingBuffer"), attributes->preserveDrawingBuffer);
            contextAttributes->setBoolean(ASCIILiteral("failIfMajorPerformanceCaveat"), attributes->failIfMajorPerformanceCaveat);
            parameters->addItem(WTFMove(contextAttributes));
        }
    }
#endif

    initialState->setAttributes(WTFMove(attributes));

    if (parameters->length())
        initialState->setParameters(WTFMove(parameters));

    initialState->setContent(getCanvasContentAsDataURL());

    return initialState;
}

RefPtr<Inspector::Protocol::Array<Inspector::InspectorValue>> InspectorCanvas::buildAction(const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    RefPtr<Inspector::Protocol::Array<InspectorValue>> action = Inspector::Protocol::Array<InspectorValue>::create();
    action->addItem(indexForData(name));

    RefPtr<Inspector::Protocol::Array<InspectorValue>> parametersData = Inspector::Protocol::Array<Inspector::InspectorValue>::create();
    RefPtr<Inspector::Protocol::Array<int>> swizzleTypes = Inspector::Protocol::Array<int>::create();

    auto addParameter = [&parametersData, &swizzleTypes] (auto value, RecordingSwizzleTypes swizzleType) {
        parametersData->addItem(value);
        swizzleTypes->addItem(static_cast<int>(swizzleType));
    };

    for (RecordCanvasActionVariant& item : parameters) {
        WTF::switchOn(item,
            [&] (CanvasDirection value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasFillRule value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasLineCap value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasLineJoin value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasTextAlign value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (CanvasTextBaseline value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (const DOMMatrix2DInit& value) {
                RefPtr<Inspector::Protocol::Array<double>> array = Inspector::Protocol::Array<double>::create();
                array->addItem(value.a.value_or(1));
                array->addItem(value.b.value_or(0));
                array->addItem(value.c.value_or(0));
                array->addItem(value.d.value_or(1));
                array->addItem(value.e.value_or(0));
                array->addItem(value.f.value_or(0));
                addParameter(WTFMove(array), RecordingSwizzleTypes::DOMMatrix);
            },
            [&] (const Element*) {
                // Elements are not serializable, so add a string as a placeholder since the actual
                // element cannot be reconstructed in the frontend.
                addParameter(indexForData("Element"), RecordingSwizzleTypes::None);
            },
            [&] (HTMLImageElement* value) { addParameter(indexForData(value), RecordingSwizzleTypes::Image); },
            [&] (ImageData* value) { addParameter(indexForData(value), RecordingSwizzleTypes::ImageData); },
            [&] (ImageSmoothingQuality value) { addParameter(indexForData(convertEnumerationToString(value)), RecordingSwizzleTypes::String); },
            [&] (const Path2D* value) { addParameter(indexForData(buildStringFromPath(value->path())), RecordingSwizzleTypes::Path2D); },
#if ENABLE(WEBGL)
            // FIXME: <https://webkit.org/b/176009> Web Inspector: send data for WebGL objects during a recording instead of a placeholder string
            [&] (const WebGLBuffer*) { addParameter(0, RecordingSwizzleTypes::WebGLBuffer); },
            [&] (const WebGLFramebuffer*) { addParameter(0, RecordingSwizzleTypes::WebGLFramebuffer); },
            [&] (const WebGLProgram*) { addParameter(0, RecordingSwizzleTypes::WebGLProgram); },
            [&] (const WebGLRenderbuffer*) { addParameter(0, RecordingSwizzleTypes::WebGLRenderbuffer); },
            [&] (const WebGLShader*) { addParameter(0, RecordingSwizzleTypes::WebGLShader); },
            [&] (const WebGLTexture*) { addParameter(0, RecordingSwizzleTypes::WebGLTexture); },
            [&] (const WebGLUniformLocation*) { addParameter(0, RecordingSwizzleTypes::WebGLUniformLocation); },
#endif
            [&] (const RefPtr<ArrayBuffer>&) { addParameter(0, RecordingSwizzleTypes::TypedArray); },
            [&] (const RefPtr<ArrayBufferView>&) { addParameter(0, RecordingSwizzleTypes::TypedArray); },
            [&] (const RefPtr<CanvasGradient>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::CanvasGradient); },
            [&] (const RefPtr<CanvasPattern>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::CanvasPattern); },
            [&] (const RefPtr<Float32Array>&) { addParameter(0, RecordingSwizzleTypes::TypedArray); },
            [&] (RefPtr<HTMLCanvasElement>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::Image); },
            [&] (const RefPtr<HTMLImageElement>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::Image); },
#if ENABLE(VIDEO)
            [&] (RefPtr<HTMLVideoElement>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::Image); },
#endif
            [&] (const RefPtr<ImageBitmap>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::ImageBitmap); },
            [&] (const RefPtr<ImageData>& value) { addParameter(indexForData(value.get()), RecordingSwizzleTypes::ImageData); },
            [&] (const RefPtr<Int32Array>&) { addParameter(0, RecordingSwizzleTypes::TypedArray); },
            [&] (const Vector<float>& value) { addParameter(buildArrayForVector(value), RecordingSwizzleTypes::Array); },
            [&] (const Vector<int>& value) { addParameter(buildArrayForVector(value), RecordingSwizzleTypes::Array); },
            [&] (const String& value) { addParameter(indexForData(value), RecordingSwizzleTypes::String); },
            [&] (double value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (float value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (int64_t value) { addParameter(static_cast<double>(value), RecordingSwizzleTypes::Number); },
            [&] (uint32_t value) { addParameter(static_cast<double>(value), RecordingSwizzleTypes::Number); },
            [&] (int32_t value) { addParameter(value, RecordingSwizzleTypes::Number); },
            [&] (uint8_t value) { addParameter(static_cast<int>(value), RecordingSwizzleTypes::Number); },
            [&] (bool value) { addParameter(value, RecordingSwizzleTypes::Boolean); }
        );
    }

    action->addItem(WTFMove(parametersData));
    action->addItem(WTFMove(swizzleTypes));

    RefPtr<Inspector::Protocol::Array<double>> trace = Inspector::Protocol::Array<double>::create();
    auto stackTrace = Inspector::createScriptCallStack(JSMainThreadExecState::currentState(), Inspector::ScriptCallStack::maxCallStackSizeToCapture);
    for (size_t i = 0; i < stackTrace->size(); ++i)
        trace->addItem(indexForData(stackTrace->at(i)));
    action->addItem(WTFMove(trace));

    return action;
}

RefPtr<Inspector::Protocol::Array<InspectorValue>> InspectorCanvas::buildArrayForCanvasGradient(const CanvasGradient& canvasGradient)
{
    const Gradient& gradient = canvasGradient.gradient();
    bool isRadial = gradient.isRadial();

    String type = isRadial ? ASCIILiteral("radial-gradient") : ASCIILiteral("linear-gradient");

    RefPtr<Inspector::Protocol::Array<float>> parameters = Inspector::Protocol::Array<float>::create();
    parameters->addItem(gradient.p0().x());
    parameters->addItem(gradient.p0().y());
    if (isRadial)
        parameters->addItem(gradient.startRadius());
    parameters->addItem(gradient.p1().x());
    parameters->addItem(gradient.p1().y());
    if (isRadial)
        parameters->addItem(gradient.endRadius());

    RefPtr<Inspector::Protocol::Array<InspectorValue>> stops = Inspector::Protocol::Array<InspectorValue>::create();
    for (const Gradient::ColorStop& colorStop : gradient.stops()) {
        RefPtr<Inspector::Protocol::Array<InspectorValue>> stop = Inspector::Protocol::Array<InspectorValue>::create();
        stop->addItem(colorStop.offset);
        stop->addItem(indexForData(colorStop.color.cssText()));
        stops->addItem(WTFMove(stop));
    }

    RefPtr<Inspector::Protocol::Array<Inspector::InspectorValue>> array = Inspector::Protocol::Array<Inspector::InspectorValue>::create();
    array->addItem(indexForData(type));
    array->addItem(WTFMove(parameters));
    array->addItem(WTFMove(stops));
    return array;
}

RefPtr<Inspector::Protocol::Array<InspectorValue>> InspectorCanvas::buildArrayForCanvasPattern(const CanvasPattern& canvasPattern)
{
    Image& tileImage = canvasPattern.pattern().tileImage();
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(tileImage.size(), RenderingMode::Unaccelerated);
    imageBuffer->context().drawImage(tileImage, FloatPoint(0, 0));

    String repeat;
    bool repeatX = canvasPattern.pattern().repeatX();
    bool repeatY = canvasPattern.pattern().repeatY();
    if (repeatX && repeatY)
        repeat = ASCIILiteral("repeat");
    else if (repeatX && !repeatY)
        repeat = ASCIILiteral("repeat-x");
    else if (!repeatX && repeatY)
        repeat = ASCIILiteral("repeat-y");
    else
        repeat = ASCIILiteral("no-repeat");

    RefPtr<Inspector::Protocol::Array<Inspector::InspectorValue>> array = Inspector::Protocol::Array<Inspector::InspectorValue>::create();
    array->addItem(indexForData(imageBuffer->toDataURL("image/png")));
    array->addItem(indexForData(repeat));
    return array;
}

RefPtr<Inspector::Protocol::Array<InspectorValue>> InspectorCanvas::buildArrayForImageData(const ImageData& imageData)
{
    RefPtr<Inspector::Protocol::Array<int>> data = Inspector::Protocol::Array<int>::create();
    for (size_t i = 0; i < imageData.data()->length(); ++i)
        data->addItem(imageData.data()->item(i));

    RefPtr<Inspector::Protocol::Array<Inspector::InspectorValue>> array = Inspector::Protocol::Array<Inspector::InspectorValue>::create();
    array->addItem(WTFMove(data));
    array->addItem(imageData.width());
    array->addItem(imageData.height());
    return array;
}

RefPtr<Inspector::Protocol::Array<InspectorValue>> InspectorCanvas::buildArrayForImageBitmap(const ImageBitmap& imageBitmap)
{
    // FIXME: Needs to include the data somehow.
    RefPtr<Inspector::Protocol::Array<Inspector::InspectorValue>> array = Inspector::Protocol::Array<Inspector::InspectorValue>::create();
    array->addItem(static_cast<int>(imageBitmap.width()));
    array->addItem(static_cast<int>(imageBitmap.height()));
    return array;
}

} // namespace WebCore


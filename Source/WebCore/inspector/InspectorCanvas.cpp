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
#include "DOMPath.h"
#include "Document.h"
#include "FloatPoint.h"
#include "Frame.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSMainThreadExecState.h"
#include "Pattern.h"
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
#include <interpreter/CallFrame.h>
#include <interpreter/StackVisitor.h>

using namespace Inspector;

namespace WebCore {

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

void InspectorCanvas::recordAction(const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    if (!hasRecordingData()) {
        m_initialState = buildInitialState();
        m_bufferUsed += m_initialState->memoryCost();

        m_frames = Inspector::Protocol::Array<Inspector::Protocol::Recording::Frame>::create();
    }

    if (!m_currentActions) {
        m_currentActions = Inspector::Protocol::Array<JSON::Value>::create();

        auto frame = Inspector::Protocol::Recording::Frame::create()
            .setActions(m_currentActions)
            .release();

        m_frames->addItem(WTFMove(frame));
    }

    auto action = buildAction(name, WTFMove(parameters));
    m_bufferUsed += action->memoryCost();
    m_currentActions->addItem(WTFMove(action));
}

RefPtr<Inspector::Protocol::Array<JSON::Value>>&& InspectorCanvas::releaseData()
{
    m_indexedDuplicateData.clear();
    return WTFMove(m_serializedDuplicateData);
}

void InspectorCanvas::markNewFrame()
{
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

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(InstrumentingAgents& instrumentingAgents)
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

    return canvas;
}

int InspectorCanvas::indexForData(DuplicateDataVariant data)
{
    size_t index = m_indexedDuplicateData.find(data);
    if (index != notFound) {
        ASSERT(index < std::numeric_limits<int>::max());
        return static_cast<int>(index);
    }

    if (!m_serializedDuplicateData)
        m_serializedDuplicateData = Inspector::Protocol::Array<JSON::Value>::create();

    RefPtr<JSON::Value> item;
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

            item = JSON::Value::create(dataURL);
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

            item = JSON::Value::create(dataURL);
        },
#endif
        [&] (HTMLCanvasElement* canvasElement) {
            String dataURL = ASCIILiteral("data:,");

            ExceptionOr<UncachedString> result = canvasElement->toDataURL(ASCIILiteral("image/png"));
            if (!result.hasException())
                dataURL = result.releaseReturnValue().string;

            item = JSON::Value::create(dataURL);
        },
        [&] (const CanvasGradient* canvasGradient) { item = buildArrayForCanvasGradient(*canvasGradient); },
        [&] (const CanvasPattern* canvasPattern) { item = buildArrayForCanvasPattern(*canvasPattern); },
        [&] (const ImageData* imageData) { item = buildArrayForImageData(*imageData); },
        [&] (const ScriptCallFrame& scriptCallFrame) {
            auto array = Inspector::Protocol::Array<double>::create();
            array->addItem(indexForData(scriptCallFrame.functionName()));
            array->addItem(indexForData(scriptCallFrame.sourceURL()));
            array->addItem(static_cast<int>(scriptCallFrame.lineNumber()));
            array->addItem(static_cast<int>(scriptCallFrame.columnNumber()));
            item = WTFMove(array);
        },
        [&] (const String& value) { item = JSON::Value::create(value); }
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

static RefPtr<Inspector::Protocol::Array<double>> buildArrayForVector(const Vector<float>& vector)
{
    RefPtr<Inspector::Protocol::Array<double>> array = Inspector::Protocol::Array<double>::create();
    for (double item : vector)
        array->addItem(item);
    return array;
}

RefPtr<Inspector::Protocol::Recording::InitialState> InspectorCanvas::buildInitialState()
{
    RefPtr<Inspector::Protocol::Recording::InitialState> initialState = Inspector::Protocol::Recording::InitialState::create()
        .release();

    auto attributes = JSON::Object::create();
    attributes->setInteger(ASCIILiteral("width"), canvas().width());
    attributes->setInteger(ASCIILiteral("height"), canvas().height());

    auto parameters = Inspector::Protocol::Array<JSON::Value>::create();

    const CanvasRenderingContext* canvasRenderingContext = canvas().renderingContext();
    if (is<CanvasRenderingContext2D>(canvasRenderingContext)) {
        const CanvasRenderingContext2D* context2d = downcast<CanvasRenderingContext2D>(canvasRenderingContext);
        const CanvasRenderingContext2D::State& state = context2d->state();

        attributes->setArray(ASCIILiteral("setTransform"), buildArrayForAffineTransform(state.transform));
        attributes->setDouble(ASCIILiteral("globalAlpha"), context2d->globalAlpha());
        attributes->setInteger(ASCIILiteral("globalCompositeOperation"), indexForData(context2d->globalCompositeOperation()));
        attributes->setDouble(ASCIILiteral("lineWidth"), context2d->lineWidth());
        attributes->setInteger(ASCIILiteral("lineCap"), indexForData(context2d->lineCap()));
        attributes->setInteger(ASCIILiteral("lineJoin"), indexForData(context2d->lineJoin()));
        attributes->setDouble(ASCIILiteral("miterLimit"), context2d->miterLimit());
        attributes->setDouble(ASCIILiteral("shadowOffsetX"), context2d->shadowOffsetX());
        attributes->setDouble(ASCIILiteral("shadowOffsetY"), context2d->shadowOffsetY());
        attributes->setDouble(ASCIILiteral("shadowBlur"), context2d->shadowBlur());
        attributes->setInteger(ASCIILiteral("shadowColor"), indexForData(context2d->shadowColor()));

        // The parameter to `setLineDash` is itself an array, so we need to wrap the parameters
        // list in an array to allow spreading.
        auto setLineDash = Inspector::Protocol::Array<JSON::Value>::create();
        setLineDash->addItem(buildArrayForVector(state.lineDash));
        attributes->setArray(ASCIILiteral("setLineDash"), WTFMove(setLineDash));

        attributes->setDouble(ASCIILiteral("lineDashOffset"), context2d->lineDashOffset());
        attributes->setInteger(ASCIILiteral("font"), indexForData(context2d->font()));
        attributes->setInteger(ASCIILiteral("textAlign"), indexForData(context2d->textAlign()));
        attributes->setInteger(ASCIILiteral("textBaseline"), indexForData(context2d->textBaseline()));
        attributes->setInteger(ASCIILiteral("direction"), indexForData(context2d->direction()));

        int strokeStyleIndex;
        if (CanvasGradient* canvasGradient = state.strokeStyle.canvasGradient())
            strokeStyleIndex = indexForData(canvasGradient);
        else if (CanvasPattern* canvasPattern = state.strokeStyle.canvasPattern())
            strokeStyleIndex = indexForData(canvasPattern);
        else
            strokeStyleIndex = indexForData(state.strokeStyle.color());
        attributes->setInteger(ASCIILiteral("strokeStyle"), strokeStyleIndex);

        int fillStyleIndex;
        if (CanvasGradient* canvasGradient = state.fillStyle.canvasGradient())
            fillStyleIndex = indexForData(canvasGradient);
        else if (CanvasPattern* canvasPattern = state.fillStyle.canvasPattern())
            fillStyleIndex = indexForData(canvasPattern);
        else
            fillStyleIndex = indexForData(state.fillStyle.color());
        attributes->setInteger(ASCIILiteral("fillStyle"), fillStyleIndex);

        attributes->setBoolean(ASCIILiteral("imageSmoothingEnabled"), context2d->imageSmoothingEnabled());
        attributes->setInteger(ASCIILiteral("imageSmoothingQuality"), indexForData(CanvasRenderingContext2D::stringForImageSmoothingQuality(context2d->imageSmoothingQuality())));

        auto setPath = Inspector::Protocol::Array<JSON::Value>::create();
        setPath->addItem(indexForData(buildStringFromPath(context2d->getPath()->path())));
        attributes->setArray(ASCIILiteral("setPath"), WTFMove(setPath));
    }

    // <https://webkit.org/b/174483> Web Inspector: Record actions performed on WebGLRenderingContext

    initialState->setAttributes(WTFMove(attributes));

    if (parameters->length())
        initialState->setParameters(WTFMove(parameters));

    ExceptionOr<UncachedString> result = canvas().toDataURL(ASCIILiteral("image/png"));
    if (!result.hasException())
        initialState->setContent(result.releaseReturnValue().string);

    return initialState;
}

RefPtr<Inspector::Protocol::Array<JSON::Value>> InspectorCanvas::buildAction(const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    RefPtr<Inspector::Protocol::Array<JSON::Value>> action = Inspector::Protocol::Array<JSON::Value>::create();
    action->addItem(indexForData(name));

    RefPtr<Inspector::Protocol::Array<JSON::Value>> parametersData = Inspector::Protocol::Array<JSON::Value>::create();
    for (RecordCanvasActionVariant& item : parameters) {
        WTF::switchOn(item,
            [&] (const CanvasRenderingContext2D::WindingRule& value) {
                String windingRule = CanvasRenderingContext2D::stringForWindingRule(value);
                parametersData->addItem(indexForData(windingRule));
            },
            [&] (const CanvasRenderingContext2D::ImageSmoothingQuality& value) {
                String imageSmoothingQuality = CanvasRenderingContext2D::stringForImageSmoothingQuality(value);
                parametersData->addItem(indexForData(imageSmoothingQuality));
            },
            [&] (const DOMMatrixInit& value) {
                RefPtr<Inspector::Protocol::Array<double>> array = Inspector::Protocol::Array<double>::create();
                array->addItem(value.a.value_or(1));
                array->addItem(value.b.value_or(0));
                array->addItem(value.c.value_or(0));
                array->addItem(value.d.value_or(1));
                array->addItem(value.e.value_or(0));
                array->addItem(value.f.value_or(0));
                parametersData->addItem(WTFMove(array));
            },
            [&] (const DOMPath* value) { parametersData->addItem(indexForData(buildStringFromPath(value->path()))); },
            [&] (const Element*) {
                // Elements are not serializable, so add a string as a placeholder since the actual
                // element cannot be reconstructed in the frontend.
                parametersData->addItem(indexForData(String("element")));
            },
            [&] (HTMLImageElement* value) { parametersData->addItem(indexForData(value)); },
            [&] (ImageData* value) {
                if (value)
                    parametersData->addItem(indexForData(value));
            },
            [&] (const RefPtr<CanvasGradient>& value) { parametersData->addItem(indexForData(value.get())); },
            [&] (const RefPtr<CanvasPattern>& value) { parametersData->addItem(indexForData(value.get())); },
            [&] (RefPtr<HTMLCanvasElement>& value) { parametersData->addItem(indexForData(value.get())); },
            [&] (const RefPtr<HTMLImageElement>& value) { parametersData->addItem(indexForData(value.get())); },
#if ENABLE(VIDEO)
            [&] (RefPtr<HTMLVideoElement>& value) { parametersData->addItem(indexForData(value.get())); },
#endif
            [&] (const Vector<float>& value) { parametersData->addItem(buildArrayForVector(value)); },
            [&] (const String& value) { parametersData->addItem(indexForData(value)); },
            [&] (double value) { parametersData->addItem(value); },
            [&] (float value) { parametersData->addItem(value); },
            [&] (int value) { parametersData->addItem(value); },
            [&] (bool value) { parametersData->addItem(value); },
            [&] (const std::optional<float>& value) {
                if (value)
                    parametersData->addItem(value.value());
            }
        );
    }
    action->addItem(WTFMove(parametersData));

    RefPtr<Inspector::Protocol::Array<double>> trace = Inspector::Protocol::Array<double>::create();
    if (JSC::CallFrame* callFrame = JSMainThreadExecState::currentState()->vm().topCallFrame) {
        callFrame->iterate([&] (JSC::StackVisitor& visitor) {
            // Only skip Native frames if they are the first frame (e.g. CanvasRenderingContext2D.prototype.save).
            if (!trace->length() && visitor->isNativeFrame())
                return JSC::StackVisitor::Continue;

            unsigned line = 0;
            unsigned column = 0;
            visitor->computeLineAndColumn(line, column);

            ScriptCallFrame scriptCallFrame(visitor->functionName(), visitor->sourceURL(), static_cast<JSC::SourceID>(visitor->sourceID()), line, column);
            trace->addItem(indexForData(scriptCallFrame));

            return JSC::StackVisitor::Continue;
        });
    }
    action->addItem(WTFMove(trace));

    return action;
}

RefPtr<Inspector::Protocol::Array<JSON::Value>> InspectorCanvas::buildArrayForCanvasGradient(const CanvasGradient& canvasGradient)
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

    RefPtr<Inspector::Protocol::Array<JSON::Value>> stops = Inspector::Protocol::Array<JSON::Value>::create();
    for (auto& colorStop : gradient.stops()) {
        RefPtr<Inspector::Protocol::Array<JSON::Value>> stop = Inspector::Protocol::Array<JSON::Value>::create();
        stop->addItem(colorStop.offset);
        stop->addItem(indexForData(colorStop.color.cssText()));
        stops->addItem(WTFMove(stop));
    }

    RefPtr<Inspector::Protocol::Array<JSON::Value>> array = Inspector::Protocol::Array<JSON::Value>::create();
    array->addItem(indexForData(type));
    array->addItem(WTFMove(parameters));
    array->addItem(WTFMove(stops));
    return array;
}

RefPtr<Inspector::Protocol::Array<JSON::Value>> InspectorCanvas::buildArrayForCanvasPattern(const CanvasPattern& canvasPattern)
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

    RefPtr<Inspector::Protocol::Array<JSON::Value>> array = Inspector::Protocol::Array<JSON::Value>::create();
    array->addItem(indexForData("pattern"));
    array->addItem(indexForData(imageBuffer->toDataURL("image/png")));
    array->addItem(indexForData(repeat));
    return array;
}

RefPtr<Inspector::Protocol::Array<JSON::Value>> InspectorCanvas::buildArrayForImageData(const ImageData& imageData)
{
    RefPtr<Inspector::Protocol::Array<int>> data = Inspector::Protocol::Array<int>::create();
    for (size_t i = 0; i < imageData.data()->length(); ++i)
        data->addItem(imageData.data()->item(i));

    RefPtr<Inspector::Protocol::Array<JSON::Value>> array = Inspector::Protocol::Array<JSON::Value>::create();
    array->addItem(WTFMove(data));
    array->addItem(imageData.width());
    array->addItem(imageData.height());
    return array;
}

} // namespace WebCore


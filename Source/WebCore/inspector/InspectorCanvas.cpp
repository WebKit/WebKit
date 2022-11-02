/*
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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
#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "CanvasBase.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "ColorSerialization.h"
#include "DOMMatrix2DInit.h"
#include "DOMPointInit.h"
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
#include "InspectorCanvasAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorInstrumentation.h"
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
#include "JSPredefinedColorSpace.h"
#include "JSWebGL2RenderingContext.h"
#include "JSWebGLRenderingContext.h"
#include "OffscreenCanvas.h"
#include "Path2D.h"
#include "Pattern.h"
#include "RecordingSwizzleType.h"
#include "SVGPathUtilities.h"
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
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/TypedArrays.h>
#include <variant>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

Ref<InspectorCanvas> InspectorCanvas::create(CanvasRenderingContext& context)
{
    return adoptRef(*new InspectorCanvas(context));
}

InspectorCanvas::InspectorCanvas(CanvasRenderingContext& context)
    : m_identifier("canvas:" + IdentifiersFactory::createIdentifier())
    , m_context(context)
{
}

CanvasRenderingContext* InspectorCanvas::canvasContext() const
{
    if (auto* contextWrapper = std::get_if<std::reference_wrapper<CanvasRenderingContext>>(&m_context))
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
        }, [] (std::monostate) -> HTMLCanvasElement* {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
    return nullptr;
}

ScriptExecutionContext* InspectorCanvas::scriptExecutionContext() const
{
    return WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) {
            auto& context = contextWrapper.get();
            return context.canvasBase().scriptExecutionContext();
        }, [] (std::monostate) -> ScriptExecutionContext* {
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
        [] (std::monostate) {
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
        [] (std::monostate) {
            ASSERT_NOT_REACHED();
            return HashSet<Element*>();
        }
    );
}

void InspectorCanvas::canvasChanged()
{
    auto* context = canvasContext();
    ASSERT(context);

    if (!context->hasActiveInspectorCanvasCallTracer())
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
    m_frameCount = std::nullopt;
    m_framesCaptured = 0;
    m_contentChanged = false;

    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    context->setHasActiveInspectorCanvasCallTracer(false);
}

bool InspectorCanvas::hasRecordingData() const
{
    return m_bufferUsed > 0;
}

bool InspectorCanvas::currentFrameHasData() const
{
    return !!m_frames;
}

template<typename T> static Ref<JSON::ArrayOf<JSON::Value>> buildArrayForVector(const Vector<T>& vector)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : vector)
        array->addItem(item);
    return array;
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasDirection argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasFillRule argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasImageSource& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasLineCap argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasLineJoin argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasRenderingContext2DBase::StyleVariant& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasTextAlign argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasTextBaseline argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(DOMMatrix2DInit& argument)
{
    auto array = JSON::ArrayOf<double>::create();
    array->addItem(argument.a.value_or(1));
    array->addItem(argument.b.value_or(0));
    array->addItem(argument.c.value_or(0));
    array->addItem(argument.d.value_or(1));
    array->addItem(argument.e.value_or(0));
    array->addItem(argument.f.value_or(0));
    return {{ WTFMove(array), RecordingSwizzleType::DOMMatrix }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Element* argument)
{
    if (!argument)
        return std::nullopt;

    // Elements are not serializable, so add a string as a placeholder since the actual
    // element cannot be reconstructed in the frontend.
    return {{ valueIndexForData("Element"_s), RecordingSwizzleType::None }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(HTMLImageElement* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::Image }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(ImageBitmap* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::ImageBitmap }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(ImageData* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::ImageData }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(ImageDataSettings&)
{
    // FIXME: Implement.
    return std::nullopt;
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(ImageSmoothingQuality argument)
{
    return {{ valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(std::optional<double>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(*argument), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(std::optional<float>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<double>(*argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Path2D* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(buildStringFromPath(argument->path())), RecordingSwizzleType::Path2D }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(PredefinedColorSpace argument)
{
    return { { valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String } };
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<CanvasGradient>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::CanvasGradient }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<CanvasPattern>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::CanvasPattern }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<HTMLCanvasElement>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::Image }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<HTMLImageElement>& argument)
{
    return processArgument(argument.get());
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<ImageBitmap>& argument)
{
    return processArgument(argument.get());
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<ImageData>& argument)
{
    return processArgument(argument.get());
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<JSC::ArrayBuffer>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<JSC::ArrayBufferView>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<JSC::Float32Array>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<JSC::Int32Array>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<JSC::Uint32Array>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(String& argument)
{
    return {{ valueIndexForData(argument), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Vector<String>& argument)
{
    auto deduplicated = argument.map([&] (const String& item) {
        return indexForData(item);
    });
    return {{ buildArrayForVector(WTFMove(deduplicated)), RecordingSwizzleType::String }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Vector<double>& argument)
{
    return {{ buildArrayForVector(argument), RecordingSwizzleType::Array }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Vector<float>& argument)
{
    return {{ buildArrayForVector(argument), RecordingSwizzleType::Array }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Vector<uint32_t>& argument)
{
    auto mapped = argument.map([&] (uint32_t item) {
        return static_cast<double>(item);
    });
    return {{ buildArrayForVector(mapped), RecordingSwizzleType::Array }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(Vector<int32_t>& argument)
{
    auto mapped = argument.map([&] (int32_t item) {
        return static_cast<double>(item);
    });
    return {{ buildArrayForVector(mapped), RecordingSwizzleType::Array }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(CanvasPath::RadiusVariant& argument)
{
    return WTF::switchOn(argument,
        [](DOMPointInit) -> std::optional<InspectorCanvasCallTracer::ProcessedArgument> {
            // FIXME We'd likely want to either create a new RecordingSwizzleType::DOMPointInit or RecordingSwizzleType::Object to avoid
            // encoding the same data multiple times. See https://webkit.org/b/233255
            return std::nullopt;
        },
        [](double radius) -> std::optional<InspectorCanvasCallTracer::ProcessedArgument> {
            return { { JSON::Value::create(radius), RecordingSwizzleType::Number } };
        });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WTF::Vector<CanvasPath::RadiusVariant>& argument)
{
    auto processed = argument.map([&](const CanvasPath::RadiusVariant& item) -> Ref<JSON::Value> {
        return WTF::switchOn(item,
            [](DOMPointInit point) -> Ref<JSON::Value> {
                auto object = JSON::Object::create();
                object->setDouble("x"_s, point.x);
                object->setDouble("y"_s, point.y);
                object->setDouble("z"_s, point.z);
                object->setDouble("w"_s, point.w);
                // FIXME We'd likely want to either create a new RecordingSwizzleType::DOMPointInit or RecordingSwizzleType::Object to avoid
                // encoding the same data multiple times
                return object;
            },
            [](double radius) -> Ref<JSON::Value> {
                return JSON::Value::create(radius);
            });
    });
    // Did not use buildArrayForVector due to WTFMov'ing the Ref<Value> to the vector as Value copy constructor was deleted.
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : processed)
        array->addItem(WTFMove(item));
    return { { array, RecordingSwizzleType::Array } };
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(double argument)
{
    return {{ JSON::Value::create(argument), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(float argument)
{
    return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(uint64_t argument)
{
    return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(int64_t argument)
{
    return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(uint32_t argument)
{
    return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(int32_t argument)
{
    return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(uint8_t argument)
{
    return {{ JSON::Value::create(static_cast<int>(argument)), RecordingSwizzleType::Number }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(bool argument)
{
    return {{ JSON::Value::create(argument), RecordingSwizzleType::Boolean }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<CSSStyleImageValue>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#if ENABLE(OFFSCREEN_CANVAS)

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<OffscreenCanvas>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(OFFSCREEN_CANVAS)

#if ENABLE(VIDEO)

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<HTMLVideoElement>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(VIDEO)

#if ENABLE(WEB_CODECS)

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(RefPtr<WebCodecsVideoFrame>& argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(0), RecordingSwizzleType::Image }};
}

#endif // ENABLE(WEB_CODECS)

#if ENABLE(WEBGL)

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(std::optional<WebGLRenderingContextBase::BufferDataSource>& argument)
{
    if (!argument)
        return std::nullopt;

    return WTF::switchOn(*argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(std::optional<WebGLRenderingContextBase::TexImageSource>& argument)
{
    if (!argument)
        return std::nullopt;

    return WTF::switchOn(*argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLBuffer* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLBuffer }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLFramebuffer* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLFramebuffer }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLProgram* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLProgram }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLQuery* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLQuery }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLRenderbuffer* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLRenderbuffer }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLRenderingContextBase::BufferDataSource& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLRenderingContextBase::Float32List::VariantType& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLRenderingContextBase::Int32List::VariantType& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLRenderingContextBase::TexImageSource& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLSampler* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLSampler }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLShader* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLShader }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLSync* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLSync }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLTexture* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLTexture }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLUniformLocation* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(argument->location()), RecordingSwizzleType::WebGLUniformLocation }};
}

#endif // ENABLE(WEBGL)

#if ENABLE(WEBGL2)

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLVertexArrayObject* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLVertexArrayObject }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGLTransformFeedback* argument)
{
    if (!argument)
        return std::nullopt;
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLTransformFeedback }};
}

std::optional<InspectorCanvasCallTracer::ProcessedArgument> InspectorCanvas::processArgument(WebGL2RenderingContext::Uint32List::VariantType& argument)
{
    return WTF::switchOn(argument, [&] (auto& value) {
        return processArgument(value);
    });
}

#endif // ENABLE(WEBGL2)

static bool shouldSnapshotBitmapRendererAction(const String& name)
{
    return name == "transferFromImageBitmap"_s;
}

#if ENABLE(WEBGL)
static bool shouldSnapshotWebGLAction(const String& name)
{
    return name == "clear"_s
        || name == "drawArrays"_s
        || name == "drawElements"_s;
}
#endif

#if ENABLE(WEBGL2)
static bool shouldSnapshotWebGL2Action(const String& name)
{
    return name == "clear"_s
        || name == "drawArrays"_s
        || name == "drawArraysInstanced"_s
        || name == "drawElements"_s
        || name == "drawElementsInstanced"_s;
}
#endif

void InspectorCanvas::recordAction(String&& name, InspectorCanvasCallTracer::ProcessedArguments&& arguments)
{
    if (!m_initialState) {
        // We should only construct the initial state for the first action of the recording.
        ASSERT(!m_frames && !m_currentActions);

        m_initialState = buildInitialState();
        m_bufferUsed += m_initialState->memoryCost();
    }

    if (!m_frames)
        m_frames = JSON::ArrayOf<Protocol::Recording::Frame>::create();

    if (!m_currentActions) {
        m_currentActions = JSON::ArrayOf<JSON::Value>::create();

        auto frame = Protocol::Recording::Frame::create()
            .setActions(*m_currentActions)
            .release();

        m_frames->addItem(WTFMove(frame));
        ++m_framesCaptured;

        m_currentFrameStartTime = MonotonicTime::now();
    }

    appendActionSnapshotIfNeeded();

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

    m_lastRecordedAction = buildAction(WTFMove(name), WTFMove(arguments));
    m_bufferUsed += m_lastRecordedAction->memoryCost();
    m_currentActions->addItem(*m_lastRecordedAction);
}

void InspectorCanvas::finalizeFrame()
{
    appendActionSnapshotIfNeeded();

    if (m_frames && m_frames->length() && !std::isnan(m_currentFrameStartTime)) {
        auto currentFrame = static_reference_cast<Protocol::Recording::Frame>(m_frames->get(m_frames->length() - 1));
        currentFrame->setDuration((MonotonicTime::now() - m_currentFrameStartTime).milliseconds());

        m_currentFrameStartTime = MonotonicTime::nan();
    }

    m_currentActions = nullptr;
}

void InspectorCanvas::markCurrentFrameIncomplete()
{
    if (!m_currentActions || !m_frames || !m_frames->length())
        return;

    auto currentFrame = static_reference_cast<Protocol::Recording::Frame>(m_frames->get(m_frames->length() - 1));
    currentFrame->setIncomplete(true);
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
        m_frameCount = std::nullopt;
}

bool InspectorCanvas::overFrameCount() const
{
    return m_frameCount && m_framesCaptured >= m_frameCount.value();
}

static RefPtr<Inspector::Protocol::Canvas::ContextAttributes> buildObjectForCanvasContextAttributes(CanvasRenderingContext& context)
{
    if (is<CanvasRenderingContext2D>(context)) {
        auto attributes = downcast<CanvasRenderingContext2D>(context).getContextAttributes();
        auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
            .release();
        switch (attributes.colorSpace) {
        case PredefinedColorSpace::SRGB:
            contextAttributesPayload->setColorSpace(Protocol::Canvas::ColorSpace::SRGB);
            break;

#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpace::DisplayP3:
            contextAttributesPayload->setColorSpace(Protocol::Canvas::ColorSpace::DisplayP3);
            break;
#endif
        }
        contextAttributesPayload->setDesynchronized(attributes.desynchronized);
        return contextAttributesPayload;
    }

    if (is<ImageBitmapRenderingContext>(context)) {
        auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
            .release();
        contextAttributesPayload->setAlpha(downcast<ImageBitmapRenderingContext>(context).hasAlpha());
        return contextAttributesPayload;
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        const auto& attributes = downcast<WebGLRenderingContextBase>(context).getContextAttributes();
        if (!attributes)
            return nullptr;

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
            contextAttributesPayload->setPowerPreference("default"_s);
            break;
        case WebGLPowerPreference::LowPower:
            contextAttributesPayload->setPowerPreference("low-power"_s);
            break;
        case WebGLPowerPreference::HighPerformance:
            contextAttributesPayload->setPowerPreference("high-performance"_s);
            break;
        }
        contextAttributesPayload->setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat);
        return contextAttributesPayload;
    }
#endif // ENABLE(WEBGL)

    return nullptr;
}

Ref<Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(bool captureBacktrace)
{
    using ContextTypeType = std::optional<Protocol::Canvas::ContextType>;
    auto contextType = WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) -> ContextTypeType {
            auto& context = contextWrapper.get();
            if (is<CanvasRenderingContext2D>(context))
                return Protocol::Canvas::ContextType::Canvas2D;
            if (is<ImageBitmapRenderingContext>(context))
                return Protocol::Canvas::ContextType::BitmapRenderer;
#if ENABLE(WEBGL)
            if (is<WebGLRenderingContext>(context))
                return Protocol::Canvas::ContextType::WebGL;
#endif
#if ENABLE(WEBGL2)
            if (is<WebGL2RenderingContext>(context))
                return Protocol::Canvas::ContextType::WebGL2;
#endif
            return std::nullopt;
        }, [] (std::monostate) -> ContextTypeType {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    );
    if (!contextType) {
        ASSERT_NOT_REACHED();
        contextType = Protocol::Canvas::ContextType::Canvas2D;
    }

    auto canvas = Protocol::Canvas::Canvas::create()
        .setCanvasId(m_identifier)
        .setContextType(contextType.value())
        .release();

    if (auto* node = canvasElement()) {
        String cssCanvasName = node->document().nameForCSSCanvasElement(*node);
        if (!cssCanvasName.isEmpty())
            canvas->setCssCanvasName(cssCanvasName);

        // FIXME: <https://webkit.org/b/178282> Web Inspector: send a DOM node with each Canvas payload and eliminate Canvas.requestNode
    }

    auto contextAttributes = WTF::switchOn(m_context,
        [] (std::reference_wrapper<CanvasRenderingContext> contextWrapper) {
            return buildObjectForCanvasContextAttributes(contextWrapper);
        }, [] (std::monostate) -> RefPtr<Inspector::Protocol::Canvas::ContextAttributes> {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
    if (contextAttributes)
        canvas->setContextAttributes(contextAttributes.releaseNonNull());

    // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations

    if (auto* node = canvasElement()) {
        if (size_t memoryCost = node->memoryCost())
            canvas->setMemoryCost(memoryCost);
    }

    if (captureBacktrace) {
        auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
        canvas->setStackTrace(stackTrace->buildInspectorObject());
    }

    return canvas;
}

Ref<Protocol::Recording::Recording> InspectorCanvas::releaseObjectForRecording()
{
    ASSERT(!m_currentActions);
    ASSERT(!m_lastRecordedAction);
    ASSERT(!m_frames);

    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    Protocol::Recording::Type type;
    if (is<CanvasRenderingContext2D>(context))
        type = Protocol::Recording::Type::Canvas2D;
    else if (is<ImageBitmapRenderingContext>(context))
        type = Protocol::Recording::Type::CanvasBitmapRenderer;
#if ENABLE(WEBGL)
    else if (is<WebGLRenderingContext>(context))
        type = Protocol::Recording::Type::CanvasWebGL;
#endif
#if ENABLE(WEBGL2)
    else if (is<WebGL2RenderingContext>(context))
        type = Protocol::Recording::Type::CanvasWebGL2;
#endif
    else {
        ASSERT_NOT_REACHED();
        type = Protocol::Recording::Type::Canvas2D;
    }

    auto recording = Protocol::Recording::Recording::create()
        .setVersion(Protocol::Recording::VERSION)
        .setType(type)
        .setInitialState(m_initialState.releaseNonNull())
        .setData(m_serializedDuplicateData.releaseNonNull())
        .release();

    if (!m_recordingName.isEmpty())
        recording->setName(m_recordingName);

    resetRecordingData();

    return recording;
}

String InspectorCanvas::getCanvasContentAsDataURL(Protocol::ErrorString& errorString)
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

        Protocol::ErrorString ignored;
        m_lastRecordedAction->addItem(indexForData(getCanvasContentAsDataURL(ignored)));

        m_bufferUsed += m_lastRecordedAction->memoryCost();
    }

    m_lastRecordedAction = nullptr;
    m_contentChanged = false;
}

int InspectorCanvas::indexForData(DuplicateDataVariant data)
{
    size_t index = m_indexedDuplicateData.findIf([&] (auto item) {
        if (data == item)
            return true;

        auto stackTraceA = std::get_if<RefPtr<ScriptCallStack>>(&data);
        auto stackTraceB = std::get_if<RefPtr<ScriptCallStack>>(&item);
        if (stackTraceA && *stackTraceA && stackTraceB && *stackTraceB)
            return (*stackTraceA)->isEqual((*stackTraceB).get());

        auto parentStackTraceA = std::get_if<RefPtr<AsyncStackTrace>>(&data);
        auto parentStackTraceB = std::get_if<RefPtr<AsyncStackTrace>>(&item);
        if (parentStackTraceA && *parentStackTraceA && parentStackTraceB && *parentStackTraceB)
            return *parentStackTraceA == *parentStackTraceB;

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
                    auto imageBuffer = ImageBuffer::create(image->size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                    imageBuffer->context().drawImage(*image, FloatPoint(0, 0));
                    dataURL = imageBuffer->toDataURL("image/png"_s);
                }
            }

            index = indexForData(dataURL);
        },
#if ENABLE(VIDEO)
        [&] (RefPtr<HTMLVideoElement>& videoElement) {
            String dataURL = "data:,"_s;

            unsigned videoWidth = videoElement->videoWidth();
            unsigned videoHeight = videoElement->videoHeight();
            auto imageBuffer = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
            if (imageBuffer) {
                videoElement->paintCurrentFrameInContext(imageBuffer->context(), FloatRect(0, 0, videoWidth, videoHeight));
                dataURL = imageBuffer->toDataURL("image/png"_s);
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
            index = indexForData(imageBitmap->buffer()->toDataURL("image/png"_s));
        },
        [&] (const RefPtr<ScriptCallStack>& scriptCallStack) {
            auto stackTrace = JSON::ArrayOf<JSON::Value>::create();

            auto callFrames = JSON::ArrayOf<double>::create();
            for (size_t i = 0; i < scriptCallStack->size(); ++i)
                callFrames->addItem(indexForData(scriptCallStack->at(i)));
            stackTrace->addItem(WTFMove(callFrames));

            stackTrace->addItem(/* topCallFrameIsBoundary */ false);

            stackTrace->addItem(scriptCallStack->truncated());

            if (const auto& parentStackTrace = scriptCallStack->parentStackTrace())
                stackTrace->addItem(indexForData(parentStackTrace));

            item = WTFMove(stackTrace);
        },
        [&] (const RefPtr<AsyncStackTrace>& parentStackTrace) {
            auto stackTrace = JSON::ArrayOf<JSON::Value>::create();

            auto callFrames = JSON::ArrayOf<double>::create();
            for (size_t i = 0; i < parentStackTrace->size(); ++i)
                callFrames->addItem(indexForData(parentStackTrace->at(i)));
            stackTrace->addItem(WTFMove(callFrames));

            stackTrace->addItem(parentStackTrace->topCallFrameIsBoundary());

            stackTrace->addItem(parentStackTrace->truncated());

            if (const auto& grandparentStackTrace = parentStackTrace->parentStackTrace())
                stackTrace->addItem(indexForData(grandparentStackTrace));

            item = WTFMove(stackTrace);
        },
        [&] (const RefPtr<CSSStyleImageValue>& cssImageValue) {
            String dataURL = "data:,"_s;

            if (auto* cachedImage = cssImageValue->image()) {
                auto* image = cachedImage->image();
                if (image && image != &Image::nullImage()) {
                    auto imageBuffer = ImageBuffer::create(image->size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                    imageBuffer->context().drawImage(*image, FloatPoint(0, 0));
                    dataURL = imageBuffer->toDataURL("image/png"_s);
                }
            }

            index = indexForData(dataURL);
        },
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
                    dataURL = buffer->toDataURL("image/png"_s);
            }

            index = indexForData(dataURL);
        },
#endif
        [&] (const String& value) { item = JSON::Value::create(value); }
    );

    if (item) {
        m_bufferUsed += item->memoryCost();
        m_serializedDuplicateData->addItem(item.releaseNonNull());

        m_indexedDuplicateData.append(data);
        index = m_indexedDuplicateData.size() - 1;
    }

    ASSERT(index < static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(index);
}

Ref<JSON::Value> InspectorCanvas::valueIndexForData(DuplicateDataVariant data)
{
    return JSON::Value::create(indexForData(data));
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

Ref<Protocol::Recording::InitialState> InspectorCanvas::buildInitialState()
{
    auto* context = canvasContext();
    ASSERT(context);
    // FIXME: <https://webkit.org/b/201651> Web Inspector: Canvas: support canvas recordings for WebGPUDevice

    auto initialStatePayload = Protocol::Recording::InitialState::create().release();

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
            statePayload->setDouble(stringIndexForKey("globalAlpha"_s), state.globalAlpha);
            statePayload->setInteger(stringIndexForKey("globalCompositeOperation"_s), indexForData(state.globalCompositeOperationString()));
            statePayload->setDouble(stringIndexForKey("lineWidth"_s), state.lineWidth);
            statePayload->setInteger(stringIndexForKey("lineCap"_s), indexForData(convertEnumerationToString(state.canvasLineCap())));
            statePayload->setInteger(stringIndexForKey("lineJoin"_s), indexForData(convertEnumerationToString(state.canvasLineJoin())));
            statePayload->setDouble(stringIndexForKey("miterLimit"_s), state.miterLimit);
            statePayload->setDouble(stringIndexForKey("shadowOffsetX"_s), state.shadowOffset.width());
            statePayload->setDouble(stringIndexForKey("shadowOffsetY"_s), state.shadowOffset.height());
            statePayload->setDouble(stringIndexForKey("shadowBlur"_s), state.shadowBlur);
            statePayload->setInteger(stringIndexForKey("shadowColor"_s), indexForData(serializationForHTML(state.shadowColor)));

            // The parameter to `setLineDash` is itself an array, so we need to wrap the parameters
            // list in an array to allow spreading.
            auto setLineDash = JSON::ArrayOf<JSON::Value>::create();
            setLineDash->addItem(buildArrayForVector(state.lineDash));
            statePayload->setArray(stringIndexForKey("setLineDash"_s), WTFMove(setLineDash));

            statePayload->setDouble(stringIndexForKey("lineDashOffset"_s), state.lineDashOffset);
            statePayload->setInteger(stringIndexForKey("font"_s), indexForData(state.fontString()));
            statePayload->setInteger(stringIndexForKey("textAlign"_s), indexForData(convertEnumerationToString(state.canvasTextAlign())));
            statePayload->setInteger(stringIndexForKey("textBaseline"_s), indexForData(convertEnumerationToString(state.canvasTextBaseline())));
            statePayload->setInteger(stringIndexForKey("direction"_s), indexForData(convertEnumerationToString(state.direction)));

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

            statePayload->setBoolean(stringIndexForKey("imageSmoothingEnabled"_s), state.imageSmoothingEnabled);
            statePayload->setInteger(stringIndexForKey("imageSmoothingQuality"_s), indexForData(convertEnumerationToString(state.imageSmoothingQuality)));

            // FIXME: This is wrong: it will repeat the context's current path for every level in the stack, ignoring saved paths.
            auto setPath = JSON::ArrayOf<JSON::Value>::create();
            setPath->addItem(indexForData(buildStringFromPath(context2d.getPath()->path())));
            statePayload->setArray(stringIndexForKey("setPath"_s), WTFMove(setPath));

            statesPayload->addItem(WTFMove(statePayload));
        }
    }

    if (auto contextAttributes = buildObjectForCanvasContextAttributes(*context))
        parametersPayload->addItem(contextAttributes.releaseNonNull());

    initialStatePayload->setAttributes(WTFMove(attributesPayload));

    if (statesPayload->length())
        initialStatePayload->setStates(WTFMove(statesPayload));

    if (parametersPayload->length())
        initialStatePayload->setParameters(WTFMove(parametersPayload));

    Protocol::ErrorString ignored;
    initialStatePayload->setContent(getCanvasContentAsDataURL(ignored));

    return initialStatePayload;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildAction(String&& name, InspectorCanvasCallTracer::ProcessedArguments&& arguments)
{
    auto action = JSON::ArrayOf<JSON::Value>::create();
    action->addItem(indexForData(WTFMove(name)));

    auto parametersData = JSON::ArrayOf<JSON::Value>::create();
    auto swizzleTypes = JSON::ArrayOf<int>::create();
    for (auto&& argument : WTFMove(arguments)) {
        if (!argument)
            continue;

        parametersData->addItem(argument->value.copyRef());
        swizzleTypes->addItem(static_cast<int>(argument->swizzleType));
    }
    action->addItem(WTFMove(parametersData));
    action->addItem(WTFMove(swizzleTypes));

    auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
    action->addItem(indexForData(stackTrace.ptr()));

    return action;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForCanvasGradient(const CanvasGradient& canvasGradient)
{
    ASCIILiteral type = "linear-gradient"_s;
    auto parameters = JSON::ArrayOf<double>::create();
    WTF::switchOn(canvasGradient.gradient().data(),
        [&] (const Gradient::LinearData& data) {
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
        },
        [&] (const Gradient::RadialData& data) {
            type = "radial-gradient"_s;
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.startRadius);
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
            parameters->addItem(data.endRadius);
        },
        [&] (const Gradient::ConicData& data) {
            type = "conic-gradient"_s;
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.angleRadians);
        }
    );

    auto stops = JSON::ArrayOf<JSON::Value>::create();
    for (auto& colorStop : canvasGradient.gradient().stops()) {
        auto stop = JSON::ArrayOf<JSON::Value>::create();
        stop->addItem(colorStop.offset);
        stop->addItem(indexForData(serializationForCSS(colorStop.color)));
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
    auto imageBuffer = canvasPattern.pattern().tileImageBuffer();

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
    array->addItem(indexForData(imageBuffer->toDataURL("image/png"_s)));
    array->addItem(indexForData(repeat));
    return array;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForImageData(const ImageData& imageData)
{
    auto data = JSON::ArrayOf<int>::create();
    for (size_t i = 0; i < imageData.data().length(); ++i)
        data->addItem(imageData.data().item(i));

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(WTFMove(data));
    array->addItem(imageData.width());
    array->addItem(imageData.height());
    return array;
}

} // namespace WebCore


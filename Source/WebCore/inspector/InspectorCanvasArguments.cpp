/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "InspectorCanvasArguments.h"

#include "Path2D.h"
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

namespace WebCore {

// MARK: - Dictionaries

auto InspectorCanvasArgumentProcessor<IDLDictionary<DOMMatrix2DInit>>::operator()(InspectorCanvas&, const DOMMatrix2DInit& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    auto array = JSON::ArrayOf<double>::create();
    array->addItem(argument.a.value_or(1));
    array->addItem(argument.b.value_or(0));
    array->addItem(argument.c.value_or(0));
    array->addItem(argument.d.value_or(1));
    array->addItem(argument.e.value_or(0));
    array->addItem(argument.f.value_or(0));
    return {{ WTF::move(array), RecordingSwizzleType::DOMMatrix }};
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<ImageDataSettings>>::operator()(InspectorCanvas&, const ImageDataSettings&) -> std::optional<InspectorCanvasProcessedArgument>
{
    // FIXME: Implement.
    return std::nullopt;
}

// MARK: - Strings

auto InspectorCanvasArgumentProcessor<IDLDOMString>::operator()(InspectorCanvas& context, const String& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::String }};
}

// MARK: - Numerics

auto InspectorCanvasArgumentProcessor<IDLBoolean>::operator()(InspectorCanvas&, bool argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(argument), RecordingSwizzleType::Boolean }};
}

// MARK: - Typed Arrays

auto InspectorCanvasArgumentProcessor<IDLArrayBuffer>::operator()(InspectorCanvas&, const Ref<JSC::ArrayBuffer>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLArrayBufferView>::operator()(InspectorCanvas&, const Ref<JSC::ArrayBufferView>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLFloat32Array>::operator()(InspectorCanvas&, const Ref<JSC::Float32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLInt32Array>::operator()(InspectorCanvas&, const Ref<JSC::Int32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLUint32Array>::operator()(InspectorCanvas&, const Ref<JSC::Uint32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

// MARK: - Interfaces

auto InspectorCanvasArgumentProcessor<IDLInterface<Element>>::operator()(InspectorCanvas& context, const Ref<Element>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    // Elements are not serializable, so add a string as a placeholder since the actual
    // element cannot be reconstructed in the frontend.
    return {{ context.valueIndexForData("Element"_s), RecordingSwizzleType::None }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLImageElement>>::operator()(InspectorCanvas& context, const Ref<HTMLImageElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<SVGImageElement>>::operator()(InspectorCanvas& context, const Ref<SVGImageElement>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    // FIXME: To maintain existing behavior for initial specialization adoption, we pretend SVGImageElement goes down the Element path.
    return {{ context.valueIndexForData("Element"_s), RecordingSwizzleType::None }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLCanvasElement>>::operator()(InspectorCanvas& context, const Ref<HTMLCanvasElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CSSStyleImageValue>>::operator()(InspectorCanvas& context, const Ref<CSSStyleImageValue>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CanvasGradient>>::operator()(InspectorCanvas& context, const Ref<CanvasGradient>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(const_cast<Ref<CanvasGradient>&>(argument)), RecordingSwizzleType::CanvasGradient }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CanvasPattern>>::operator()(InspectorCanvas& context, const Ref<CanvasPattern>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::CanvasPattern }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<Path2D>>::operator()(InspectorCanvas& context, const Ref<Path2D>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(buildStringFromPath(argument->path())), RecordingSwizzleType::Path2D }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<ImageBitmap>>::operator()(InspectorCanvas& context, const Ref<ImageBitmap>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::ImageBitmap }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<ImageData>>::operator()(InspectorCanvas& context, const Ref<ImageData>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::ImageData }};
}

#if ENABLE(OFFSCREEN_CANVAS)

auto InspectorCanvasArgumentProcessor<IDLInterface<OffscreenCanvas>>::operator()(InspectorCanvas& context, const Ref<OffscreenCanvas>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(OFFSCREEN_CANVAS)

#if ENABLE(VIDEO)

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLVideoElement>>::operator()(InspectorCanvas& context, const Ref<HTMLVideoElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(VIDEO)

#if ENABLE(WEB_CODECS)

auto InspectorCanvasArgumentProcessor<IDLInterface<WebCodecsVideoFrame>>::operator()(InspectorCanvas&, const Ref<WebCodecsVideoFrame>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::Image }};
}

#endif // ENABLE(WEB_CODECS)

#if ENABLE(WEBGL)

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLBuffer>>::operator()(InspectorCanvas&, const Ref<WebGLBuffer>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLBuffer }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLFramebuffer>>::operator()(InspectorCanvas&, const Ref<WebGLFramebuffer>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLFramebuffer }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLProgram>>::operator()(InspectorCanvas&, const Ref<WebGLProgram>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLProgram }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLQuery>>::operator()(InspectorCanvas&, const Ref<WebGLQuery>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLQuery }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLRenderbuffer>>::operator()(InspectorCanvas&, const Ref<WebGLRenderbuffer>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLRenderbuffer }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLSampler>>::operator()(InspectorCanvas&, const Ref<WebGLSampler>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLSampler }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLShader>>::operator()(InspectorCanvas&, const Ref<WebGLShader>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLShader }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLSync>>::operator()(InspectorCanvas&, const Ref<WebGLSync>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLSync }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLTexture>>::operator()(InspectorCanvas&, const Ref<WebGLTexture>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLTexture }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLUniformLocation>>::operator()(InspectorCanvas&, const Ref<WebGLUniformLocation>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(argument->location()), RecordingSwizzleType::WebGLUniformLocation }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLVertexArrayObject>>::operator()(InspectorCanvas&, const Ref<WebGLVertexArrayObject>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLVertexArrayObject }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<WebGLTransformFeedback>>::operator()(InspectorCanvas&, const Ref<WebGLTransformFeedback>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(static_cast<int>(argument->object())), RecordingSwizzleType::WebGLTransformFeedback }};
}

#endif // ENABLE(WEBGL)

// MARK: - Unions

auto InspectorCanvasArgumentProcessor<IDLCanvasImageSourceUnion>::operator()(InspectorCanvas& context, const CanvasImageSource& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&]<typename T>(const Ref<T>& value) {
            return InspectorCanvasArgumentProcessor<IDLInterface<T>>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLCanvasStyleVariantUnion>::operator()(InspectorCanvas& context, const CanvasRenderingContext2DBase::StyleVariant& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&](const String& value) {
            return InspectorCanvasArgumentProcessor<IDLDOMString>{}(context, value);
        },
        [&]<typename T>(const Ref<T>& value) {
            return InspectorCanvasArgumentProcessor<IDLInterface<T>>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLCanvasPathRadiusUnion>::operator()(InspectorCanvas&, const CanvasPath::RadiusVariant& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [](const DOMPointInit&) -> std::optional<InspectorCanvasProcessedArgument> {
            // FIXME: We'd likely want to either create a new RecordingSwizzleType::DOMPointInit or RecordingSwizzleType::Object to avoid encoding the same data multiple times. See https://webkit.org/b/233255.
            return std::nullopt;
        },
        [](double radius) -> std::optional<InspectorCanvasProcessedArgument> {
            return {{ JSON::Value::create(radius), RecordingSwizzleType::Number }};
        }
    );
}

#if ENABLE(WEBGL)

auto InspectorCanvasArgumentProcessor<IDLTexImageSourceUnion>::operator()(InspectorCanvas& context, const WebGLRenderingContextBase::TexImageSource& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&]<typename T>(const Ref<T>& value) {
            return InspectorCanvasArgumentProcessor<IDLInterface<T>>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLBufferDataSourceUnion>::operator()(InspectorCanvas& context, const WebGLRenderingContextBase::BufferDataSource& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&](const Ref<ArrayBuffer>& value) -> std::optional<InspectorCanvasProcessedArgument> {
            return InspectorCanvasArgumentProcessor<IDLArrayBuffer>{}(context, value);
        },
        [&](const Ref<ArrayBufferView>& value) -> std::optional<InspectorCanvasProcessedArgument> {
            return InspectorCanvasArgumentProcessor<IDLArrayBufferView>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLFloat32ListUnion>::operator()(InspectorCanvas& context, const WebGLRenderingContextBase::Float32List::VariantType& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&](const Ref<Float32Array>& value) {
            return InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLFloat32Array>>{}(context, value);
        },
        [&](const Vector<float>& value) {
            return InspectorCanvasArgumentProcessor<IDLSequence<IDLUnrestrictedFloat>>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLInt32ListUnion>::operator()(InspectorCanvas& context, const WebGLRenderingContextBase::Int32List::VariantType& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&](const Ref<Int32Array>& value) {
            return InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLInt32Array>>{}(context, value);
        },
        [&](const Vector<int>& value) {
            return InspectorCanvasArgumentProcessor<IDLSequence<IDLLong>>{}(context, value);
        }
    );
}

auto InspectorCanvasArgumentProcessor<IDLUint32ListUnion>::operator()(InspectorCanvas& context, const WebGLRenderingContextBase::Uint32List::VariantType& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return WTF::switchOn(argument,
        [&](const Ref<Uint32Array>& value) {
            return InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLUint32Array>>{}(context, value);
        },
        [&](const Vector<uint32_t>& value) {
            return InspectorCanvasArgumentProcessor<IDLSequence<IDLUnsignedLong>>{}(context, value);
        }
    );
}

#endif // ENABLE(WEBGL)

// MARK: - Sequences

static Ref<JSON::ArrayOf<JSON::Value>> mapToArray(const auto& range)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : range)
        array->addItem(item);
    return array;
}

static Ref<JSON::ArrayOf<JSON::Value>> mapToArray(const auto& range, NOESCAPE auto&& functor)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : range)
        array->addItem(functor(item));
    return array;
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLDOMString>>::operator()(InspectorCanvas& context, const Vector<String>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument, [&](const auto& item) { return context.indexForData(item); }), RecordingSwizzleType::String }};
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLUnrestrictedDouble>>::operator()(InspectorCanvas&, const Vector<double>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument), RecordingSwizzleType::Array }};
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLUnrestrictedFloat>>::operator()(InspectorCanvas&, const Vector<float>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument), RecordingSwizzleType::Array }};
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLUnsignedLong>>::operator()(InspectorCanvas&, const Vector<uint32_t>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument, [&](const auto& item) { return static_cast<double>(item); }), RecordingSwizzleType::Array }};
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLLong>>::operator()(InspectorCanvas&, const Vector<int32_t>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument, [&](const auto& item) { return static_cast<double>(item); }), RecordingSwizzleType::Array }};
}

auto InspectorCanvasArgumentProcessor<IDLSequence<IDLCanvasPathRadiusUnion>>::operator()(InspectorCanvas&, const Vector<CanvasPath::RadiusVariant>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ mapToArray(argument, [&](const CanvasPath::RadiusVariant& item) -> Ref<JSON::Value> {
        return WTF::switchOn(item,
            [](DOMPointInit point) -> Ref<JSON::Value> {
                auto object = JSON::Object::create();
                object->setDouble("x"_s, point.x);
                object->setDouble("y"_s, point.y);
                object->setDouble("z"_s, point.z);
                object->setDouble("w"_s, point.w);
                // FIXME: We'd likely want to either create a new RecordingSwizzleType::DOMPointInit or RecordingSwizzleType::Object to avoid encoding the same data multiple times.
                return object;
            },
            [](double radius) -> Ref<JSON::Value> {
                return JSON::Value::create(radius);
            });
    }), RecordingSwizzleType::Array }};
}

} // namespace WebCore

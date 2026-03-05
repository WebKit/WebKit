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

#pragma once

#include "CSSStyleImageValue.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2DBase.h"
#include "DOMMatrix2DInit.h"
#include "DOMPointInit.h"
#include "Element.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "IDLTypes.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorCanvas.h"
#include "InspectorCanvasProcessedArguments.h"
#include "JSDOMConvertBufferSource.h"
#include "SVGImageElement.h"
#include "WebGL2RenderingContext.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif

namespace WebCore {

template<typename> struct InspectorCanvasArgumentProcessor;

// MARK: - Adaptors

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLNullable<IDLInterface<IDLType>>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        if (!value)
            return std::nullopt;
        return InspectorCanvasArgumentProcessor<IDLInterface<IDLType>>{}(context, *value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLNullable<IDLAllowSharedAdaptor<IDLType>>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, auto& value)
    {
        if (!value)
            return std::nullopt;
        return InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLType>>{}(context, value.releaseNonNull());
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLNullable<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        if (!value)
            return std::nullopt;
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, *value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLOptional<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        if (!value)
            return std::nullopt;
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, *value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLLegacyNullToEmptyStringAdaptor<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLEnforceRangeAdaptor<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

// MARK: - Enumerations

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLEnumeration<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, auto argument)
    {
        return {{ context.valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
    }
};

// MARK: - Dictionaries

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<DOMMatrix2DInit>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const DOMMatrix2DInit&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<ImageDataSettings>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const ImageDataSettings&);
};

// MARK: - Strings

template<> struct InspectorCanvasArgumentProcessor<IDLDOMString> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const String&);
};

// MARK: - Numerics

template<> struct InspectorCanvasArgumentProcessor<IDLBoolean> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, bool);
};

template<typename IDLType>
    requires (IsIDLNumber<IDLType>::value)
struct InspectorCanvasArgumentProcessor<IDLType> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, auto argument)
    {
        return {{ JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number }};
    }
};

// MARK: - Typed Arrays

template<> struct InspectorCanvasArgumentProcessor<IDLArrayBuffer> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<JSC::ArrayBuffer>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLArrayBufferView> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<JSC::ArrayBufferView>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLFloat32Array> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<JSC::Float32Array>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInt32Array> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<JSC::Int32Array>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLUint32Array> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<JSC::Uint32Array>&);
};

// MARK: - Interfaces

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<Element>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<Element>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<HTMLImageElement>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<HTMLImageElement>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<SVGImageElement>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<SVGImageElement>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<HTMLCanvasElement>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<HTMLCanvasElement>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<CSSStyleImageValue>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<CSSStyleImageValue>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<CanvasGradient>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<CanvasGradient>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<CanvasPattern>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<CanvasPattern>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<Path2D>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<Path2D>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<ImageBitmap>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<ImageBitmap>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<ImageData>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<ImageData>&);
};

#if ENABLE(OFFSCREEN_CANVAS)

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<OffscreenCanvas>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<OffscreenCanvas>&);
};

#endif // ENABLE(OFFSCREEN_CANVAS)

#if ENABLE(VIDEO)

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<HTMLVideoElement>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<HTMLVideoElement>&);
};

#endif // ENABLE(VIDEO)

#if ENABLE(WEB_CODECS)

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebCodecsVideoFrame>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebCodecsVideoFrame>&);
};

#endif // ENABLE(WEB_CODECS)

#if ENABLE(WEBGL)

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLBuffer>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLBuffer>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLFramebuffer>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLFramebuffer>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLProgram>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLProgram>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLQuery>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLQuery>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLRenderbuffer>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLRenderbuffer>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLSampler>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLSampler>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLShader>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLShader>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLSync>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLSync>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLTexture>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLTexture>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLUniformLocation>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLUniformLocation>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLVertexArrayObject>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLVertexArrayObject>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLTransformFeedback>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLTransformFeedback>&);
};

#endif // ENABLE(WEBGL)

// MARK: - Unions

using IDLCanvasImageSourceUnion = IDLUnion<
    IDLInterface<HTMLImageElement>,
    IDLInterface<SVGImageElement>,
    IDLInterface<HTMLCanvasElement>,
    IDLInterface<ImageBitmap>,
    IDLInterface<CSSStyleImageValue>
#if ENABLE(OFFSCREEN_CANVAS)
    , IDLInterface<OffscreenCanvas>
#endif
#if ENABLE(VIDEO)
    , IDLInterface<HTMLVideoElement>
#endif
#if ENABLE(WEB_CODECS)
    , IDLInterface<WebCodecsVideoFrame>
#endif
>;

using IDLCanvasStyleVariantUnion = IDLUnion<
    IDLDOMString,
    IDLInterface<CanvasGradient>,
    IDLInterface<CanvasPattern>
>;

using IDLCanvasPathRadiusUnion = IDLUnion<
    IDLUnrestrictedDouble,
    IDLDictionary<DOMPointInit>
>;

template<> struct InspectorCanvasArgumentProcessor<IDLCanvasImageSourceUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const CanvasImageSource&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLCanvasStyleVariantUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const CanvasRenderingContext2DBase::StyleVariant&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLCanvasPathRadiusUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const CanvasPath::RadiusVariant&);
};

#if ENABLE(WEBGL)

using IDLTexImageSourceUnion = IDLUnion<
    IDLInterface<ImageBitmap>,
    IDLInterface<ImageData>,
    IDLInterface<HTMLImageElement>,
    IDLInterface<HTMLCanvasElement>
#if ENABLE(VIDEO)
    , IDLInterface<HTMLVideoElement>
#endif
#if ENABLE(OFFSCREEN_CANVAS)
    , IDLInterface<OffscreenCanvas>
#endif
#if ENABLE(WEB_CODECS)
    , IDLInterface<WebCodecsVideoFrame>
#endif
>;

using IDLBufferDataSourceUnion = IDLUnion<
    IDLAllowSharedAdaptor<IDLArrayBuffer>,
    IDLAllowSharedAdaptor<IDLArrayBufferView>
>;

using IDLFloat32ListUnion = IDLUnion<
    IDLAllowSharedAdaptor<IDLFloat32Array>,
    IDLSequence<IDLUnrestrictedFloat>
>;

using IDLInt32ListUnion = IDLUnion<
    IDLAllowSharedAdaptor<IDLInt32Array>,
    IDLSequence<IDLLong>
>;

using IDLUint32ListUnion = IDLUnion<
    IDLAllowSharedAdaptor<IDLUint32Array>,
    IDLSequence<IDLUnsignedLong>
>;

template<> struct InspectorCanvasArgumentProcessor<IDLTexImageSourceUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const WebGLRenderingContextBase::TexImageSource&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLBufferDataSourceUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const WebGLRenderingContextBase::BufferDataSource&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLFloat32ListUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const WebGLRenderingContextBase::Float32List::VariantType&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLInt32ListUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const WebGLRenderingContextBase::Int32List::VariantType&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLUint32ListUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const WebGL2RenderingContext::Uint32List::VariantType&);
};

#endif // ENABLE(WEBGL)

// MARK: - Sequences

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLDOMString>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<String>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLUnrestrictedDouble>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<double>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLUnrestrictedFloat>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<float>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLUnsignedLong>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<uint32_t>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLLong>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<int32_t>&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLCanvasPathRadiusUnion>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Vector<CanvasPath::RadiusVariant>&);
};

} // namespace WebCore

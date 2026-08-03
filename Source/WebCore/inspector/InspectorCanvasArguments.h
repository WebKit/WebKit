/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CanvasElementImage.h"
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
#include "WebGLCopyElementImageConfig.h"
#include "WebGLExtension.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/TypedArrays.h>
#include <limits>
#include <type_traits>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif

namespace WebCore {

class GPUBindGroup;
class GPUBindGroupLayout;
class GPUBuffer;
class GPUCommandBuffer;
class GPUCommandEncoder;
class GPUComputePassEncoder;
class GPUComputePipeline;
class GPUExternalTexture;
class GPUPipelineLayout;
class GPUQuerySet;
class GPUQueue;
class GPURenderBundle;
class GPURenderBundleEncoder;
class GPURenderPassEncoder;
class GPURenderPipeline;
class GPUSampler;
class GPUShaderModule;
class GPUTexture;
class GPUTextureView;

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
    std::optional<InspectorCanvasProcessedArgument> NODELETE operator()(InspectorCanvas&, const ImageDataSettings&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<WebGLCopyElementImageConfig>> {
    std::optional<InspectorCanvasProcessedArgument> NODELETE operator()(InspectorCanvas&, const WebGLCopyElementImageConfig&);
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

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<CanvasElementImage>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<CanvasElementImage>&);
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

template<typename Receiver>
constexpr RecordingSwizzleType recordingSwizzleTypeForWebGPUReceiver()
{
    using ReceiverType = std::remove_cvref_t<Receiver>;
    if constexpr (std::is_same_v<ReceiverType, GPUBindGroup>)
        return RecordingSwizzleType::GPUBindGroup;
    if constexpr (std::is_same_v<ReceiverType, GPUBindGroupLayout>)
        return RecordingSwizzleType::GPUBindGroupLayout;
    if constexpr (std::is_same_v<ReceiverType, GPUBuffer>)
        return RecordingSwizzleType::GPUBuffer;
    if constexpr (std::is_same_v<ReceiverType, GPUCommandBuffer>)
        return RecordingSwizzleType::GPUCommandBuffer;
    if constexpr (std::is_same_v<ReceiverType, GPUCommandEncoder>)
        return RecordingSwizzleType::GPUCommandEncoder;
    if constexpr (std::is_same_v<ReceiverType, GPUComputePassEncoder>)
        return RecordingSwizzleType::GPUComputePassEncoder;
    if constexpr (std::is_same_v<ReceiverType, GPUComputePipeline>)
        return RecordingSwizzleType::GPUComputePipeline;
    if constexpr (std::is_same_v<ReceiverType, GPUExternalTexture>)
        return RecordingSwizzleType::GPUExternalTexture;
    if constexpr (std::is_same_v<ReceiverType, GPUPipelineLayout>)
        return RecordingSwizzleType::GPUPipelineLayout;
    if constexpr (std::is_same_v<ReceiverType, GPUQuerySet>)
        return RecordingSwizzleType::GPUQuerySet;
    if constexpr (std::is_same_v<ReceiverType, GPUQueue>)
        return RecordingSwizzleType::GPUQueue;
    if constexpr (std::is_same_v<ReceiverType, GPURenderBundle>)
        return RecordingSwizzleType::GPURenderBundle;
    if constexpr (std::is_same_v<ReceiverType, GPURenderBundleEncoder>)
        return RecordingSwizzleType::GPURenderBundleEncoder;
    if constexpr (std::is_same_v<ReceiverType, GPURenderPassEncoder>)
        return RecordingSwizzleType::GPURenderPassEncoder;
    if constexpr (std::is_same_v<ReceiverType, GPURenderPipeline>)
        return RecordingSwizzleType::GPURenderPipeline;
    if constexpr (std::is_same_v<ReceiverType, GPUSampler>)
        return RecordingSwizzleType::GPUSampler;
    if constexpr (std::is_same_v<ReceiverType, GPUShaderModule>)
        return RecordingSwizzleType::GPUShaderModule;
    if constexpr (std::is_same_v<ReceiverType, GPUTexture>)
        return RecordingSwizzleType::GPUTexture;
    if constexpr (std::is_same_v<ReceiverType, GPUTextureView>)
        return RecordingSwizzleType::GPUTextureView;
    return RecordingSwizzleType::None;
}

template<typename IDLType>
    requires (requires (IDLType& object) { object.device(); })
struct InspectorCanvasArgumentProcessor<IDLInterface<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const IDLType& argument)
    {
        constexpr auto swizzleType = recordingSwizzleTypeForWebGPUReceiver<IDLType>();
        static_assert(swizzleType != RecordingSwizzleType::None);

        size_t identifier = context.identifierForRecordingObject(reinterpret_cast<uintptr_t>(&argument));
        RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));
        return { { JSON::Value::create(static_cast<int>(identifier)), swizzleType } };
    }
};

#if ENABLE(WEBGL)

class WebGLTimerQueryEXT;
class WebGLVertexArrayObjectOES;

RecordingSwizzleType recordingSwizzleTypeForWebGLExtension(WebGLExtensionName);

template<typename IDLType>
    requires (std::is_base_of_v<WebGLExtensionBase, IDLType>)
struct InspectorCanvasArgumentProcessor<IDLInterface<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const Ref<IDLType>& argument)
    {
        auto swizzleType = recordingSwizzleTypeForWebGLExtension(argument->name());
        size_t identifier = context.identifierForRecordingObject(reinterpret_cast<uintptr_t>(argument.ptr()));
        RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));
        return { { JSON::Value::create(static_cast<int>(identifier)), swizzleType } };
    }
};

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

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLTimerQueryEXT>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLTimerQueryEXT>&);
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

template<> struct InspectorCanvasArgumentProcessor<IDLInterface<WebGLVertexArrayObjectOES>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const Ref<WebGLVertexArrayObjectOES>&);
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

using IDLCanvasElementImageSourceUnion = IDLUnion<
    IDLInterface<Element>,
    IDLInterface<CanvasElementImage>
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

template<> struct InspectorCanvasArgumentProcessor<IDLCanvasElementImageSourceUnion> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const CanvasElementImageSource&);
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

template<> struct InspectorCanvasArgumentProcessor<IDLUnion<IDLInt32Array, IDLSequence<IDLLong>>> : InspectorCanvasArgumentProcessor<IDLInt32ListUnion> { };
template<> struct InspectorCanvasArgumentProcessor<IDLUnion<IDLUint32Array, IDLSequence<IDLUnsignedLong>>> : InspectorCanvasArgumentProcessor<IDLUint32ListUnion> { };

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

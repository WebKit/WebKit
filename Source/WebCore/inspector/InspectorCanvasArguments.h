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
#include "GPUColorDict.h"
#include "GPUExtent3DDict.h"
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
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif

namespace WebCore {

struct GPUBindGroupDescriptor;
struct GPUComputePassDescriptor;
struct GPUComputePipelineDescriptor;
struct GPUCopyElementImageDestination;
struct GPUCopyElementImageSource;
struct GPUExternalTextureDescriptor;
struct GPUImageCopyBuffer;
struct GPUImageCopyExternalImage;
struct GPUImageCopyTexture;
struct GPUImageCopyTextureTagged;
struct GPUPipelineLayoutDescriptor;
struct GPURenderPassDescriptor;
struct GPURenderPipelineDescriptor;
struct GPUShaderModuleDescriptor;
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

template<typename IDLType> struct InspectorCanvasArgumentProcessor {
    template<typename T>
        requires (IsIDLDictionary<IDLType>::value && requires (const T& value) { value.toJSON(); })
    static Ref<JSON::Value> toJSON(const T& argument)
    {
        return argument.toJSON();
    }

    template<typename T>
        requires (IsIDLDictionary<IDLType>::value && requires (const T& value) { value.toJSON(); })
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const T& argument)
    {
        return { { context.valueIndexForData(toJSON(argument)->toJSONString()), RecordingSwizzleType::JSON } };
    }
};

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

template<typename IDLType> struct InspectorCanvasNullableArgumentProcessor {
    template<typename T>
    static decltype(auto) extractValue(const T& value)
    {
        if constexpr (requires { *value; })
            return *value;
        else
            return IDLType::extractValueFromNullable(value);
    }

    template<typename T>
        requires (requires (const T& value) {
            IDLType::isNullValue(value);
            InspectorCanvasArgumentProcessor<IDLType>::toJSON(extractValue(value));
        })
    static Ref<JSON::Value> toJSON(const T& value)
    {
        if (IDLType::isNullValue(value))
            return JSON::Value::null();
        return InspectorCanvasArgumentProcessor<IDLType>::toJSON(extractValue(value));
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        if (IDLType::isNullValue(value))
            return std::nullopt;
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, extractValue(value));
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLNullable<IDLType>> : InspectorCanvasNullableArgumentProcessor<IDLType> { };

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLOptional<IDLType>> : InspectorCanvasNullableArgumentProcessor<IDLType> { };

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLLegacyNullToEmptyStringAdaptor<IDLType>> {
    template<typename T>
        requires requires (const T& value) { InspectorCanvasArgumentProcessor<IDLType>::toJSON(value); }
    static Ref<JSON::Value> toJSON(const T& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>::toJSON(value);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLEnforceRangeAdaptor<IDLType>> {
    template<typename T>
        requires requires (const T& value) { InspectorCanvasArgumentProcessor<IDLType>::toJSON(value); }
    static Ref<JSON::Value> toJSON(const T& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>::toJSON(value);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLAllowSharedAdaptor<IDLType>> {
    template<typename T>
        requires requires (const T& value) { InspectorCanvasArgumentProcessor<IDLType>::toJSON(value); }
    static Ref<JSON::Value> toJSON(const T& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>::toJSON(value);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const auto& value)
    {
        return InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
    }
};

// MARK: - Enumerations

template<typename IDLType> struct InspectorCanvasArgumentProcessor<IDLEnumeration<IDLType>> {
    static Ref<JSON::Value> toJSON(IDLType argument)
    {
        return JSON::Value::create(convertEnumerationToString(argument));
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, auto argument)
    {
        return {{ context.valueIndexForData(convertEnumerationToString(argument)), RecordingSwizzleType::String }};
    }
};

// MARK: - Dictionaries

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<DOMMatrix2DInit>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const DOMMatrix2DInit&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUComputePassDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUComputePassDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUComputePipelineDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUComputePipelineDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUCopyElementImageDestination>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUCopyElementImageDestination&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUCopyElementImageSource>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUCopyElementImageSource&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUExternalTextureDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUExternalTextureDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyBuffer>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUImageCopyBuffer&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyExternalImage>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUImageCopyExternalImage&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyTexture>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUImageCopyTexture&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyTextureTagged>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUImageCopyTextureTagged&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUPipelineLayoutDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUPipelineLayoutDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPURenderPassDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPURenderPassDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUBindGroupDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUBindGroupDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPURenderPipelineDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPURenderPipelineDescriptor&);
};

template<> struct InspectorCanvasArgumentProcessor<IDLDictionary<GPUShaderModuleDescriptor>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const GPUShaderModuleDescriptor&);
};

// MARK: - Strings

template<> struct InspectorCanvasArgumentProcessor<IDLDOMString> {
    static Ref<JSON::Value> toJSON(const String& value)
    {
        return JSON::Value::create(value);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, const String&);
};

template<typename IDLType>
    requires (IsIDLString<IDLType>::value)
struct InspectorCanvasArgumentProcessor<IDLType> : InspectorCanvasArgumentProcessor<IDLDOMString> { };

// MARK: - Numerics

template<> struct InspectorCanvasArgumentProcessor<IDLBoolean> {
    static Ref<JSON::Value> toJSON(bool value)
    {
        return JSON::Value::create(value);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas&, bool);
};

template<typename IDLType>
    requires (IsIDLNumber<IDLType>::value)
struct InspectorCanvasArgumentProcessor<IDLType> {
    static Ref<JSON::Value> toJSON(auto argument)
    {
        return JSON::Value::create(static_cast<double>(argument));
    }

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

        size_t identifier = context.identifierForRecordingObject(swizzleType, reinterpret_cast<uintptr_t>(&argument));
        RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));
        return { { JSON::Value::create(static_cast<int>(identifier)), swizzleType } };
    }
};

#if ENABLE(WEBGL)

class WebGLTimerQueryEXT;
class WebGLVertexArrayObjectOES;

template<typename Receiver>
constexpr RecordingSwizzleType recordingSwizzleTypeForWebGLReceiver()
{
    using ReceiverType = std::remove_cvref_t<Receiver>;
    if constexpr (std::is_same_v<ReceiverType, WebGLBuffer>)
        return RecordingSwizzleType::WebGLBuffer;
    if constexpr (std::is_same_v<ReceiverType, WebGLFramebuffer>)
        return RecordingSwizzleType::WebGLFramebuffer;
    if constexpr (std::is_same_v<ReceiverType, WebGLProgram>)
        return RecordingSwizzleType::WebGLProgram;
    if constexpr (std::is_same_v<ReceiverType, WebGLQuery>)
        return RecordingSwizzleType::WebGLQuery;
    if constexpr (std::is_same_v<ReceiverType, WebGLRenderbuffer>)
        return RecordingSwizzleType::WebGLRenderbuffer;
    if constexpr (std::is_same_v<ReceiverType, WebGLSampler>)
        return RecordingSwizzleType::WebGLSampler;
    if constexpr (std::is_same_v<ReceiverType, WebGLShader>)
        return RecordingSwizzleType::WebGLShader;
    if constexpr (std::is_same_v<ReceiverType, WebGLSync>)
        return RecordingSwizzleType::WebGLSync;
    if constexpr (std::is_same_v<ReceiverType, WebGLTimerQueryEXT>)
        return RecordingSwizzleType::WebGLTimerQueryEXT;
    if constexpr (std::is_same_v<ReceiverType, WebGLTexture>)
        return RecordingSwizzleType::WebGLTexture;
    if constexpr (std::is_same_v<ReceiverType, WebGLUniformLocation>)
        return RecordingSwizzleType::WebGLUniformLocation;
    if constexpr (std::is_same_v<ReceiverType, WebGLVertexArrayObject>)
        return RecordingSwizzleType::WebGLVertexArrayObject;
    if constexpr (std::is_same_v<ReceiverType, WebGLVertexArrayObjectOES>)
        return RecordingSwizzleType::WebGLVertexArrayObjectOES;
    if constexpr (std::is_same_v<ReceiverType, WebGLTransformFeedback>)
        return RecordingSwizzleType::WebGLTransformFeedback;
    return RecordingSwizzleType::None;
}

template<typename IDLType>
    requires (recordingSwizzleTypeForWebGLReceiver<IDLType>() != RecordingSwizzleType::None)
struct InspectorCanvasArgumentProcessor<IDLInterface<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const Ref<IDLType>& argument)
    {
        constexpr auto swizzleType = recordingSwizzleTypeForWebGLReceiver<IDLType>();
        static_assert(swizzleType != RecordingSwizzleType::None);

        size_t identifier = context.identifierForRecordingObject(swizzleType, reinterpret_cast<uintptr_t>(argument.ptr()));
        RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));
        return { { JSON::Value::create(static_cast<int>(identifier)), swizzleType } };
    }
};

RecordingSwizzleType recordingSwizzleTypeForWebGLExtension(WebGLExtensionName);

template<typename IDLType>
    requires (std::is_base_of_v<WebGLExtensionBase, IDLType>)
struct InspectorCanvasArgumentProcessor<IDLInterface<IDLType>> {
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const Ref<IDLType>& argument)
    {
        auto swizzleType = recordingSwizzleTypeForWebGLExtension(argument->name());
        size_t identifier = context.identifierForRecordingObject(swizzleType, reinterpret_cast<uintptr_t>(argument.ptr()));
        RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));
        return { { JSON::Value::create(static_cast<int>(identifier)), swizzleType } };
    }
};

#endif // ENABLE(WEBGL)

// MARK: - Unions

template<typename... IDLTypes> struct InspectorCanvasArgumentProcessor<IDLUnion<IDLTypes...>> {
    using ImplementationType = typename IDLUnion<IDLTypes...>::ImplementationType;

    static Ref<JSON::Value> toJSON(const ImplementationType& argument)
        requires ((requires (const typename IDLTypes::UnionStorageType& value) {
            InspectorCanvasArgumentProcessor<IDLTypes>::toJSON(value);
        }) && ...)
    {
        return WTF::switchOn(argument, [](const typename IDLTypes::UnionStorageType& value) {
            return InspectorCanvasArgumentProcessor<IDLTypes>::toJSON(value);
        }...);
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const ImplementationType& argument)
    {
        return WTF::switchOn(argument, [&](const typename IDLTypes::UnionStorageType& value) {
            return InspectorCanvasArgumentProcessor<IDLTypes>{}(context, value);
        }...);
    }

    template<typename T>
        requires requires (const T& value) { static_cast<const ImplementationType&>(value.variant()); }
    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const T& argument)
    {
        return operator()(context, static_cast<const ImplementationType&>(argument.variant()));
    }
};

// MARK: - Sequences

static Ref<JSON::ArrayOf<JSON::Value>> mapToArray(const auto& range, NOESCAPE auto&& functor)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    for (auto& item : range)
        array->addItem(functor(item));
    return array;
}

template<typename IDLType, size_t inlineCapacity> struct InspectorCanvasArgumentProcessor<IDLSequence<IDLType, inlineCapacity>> {
    using SequenceType = IDLSequence<IDLType, inlineCapacity>;
    using ImplementationType = typename SequenceType::ImplementationType;

    static Ref<JSON::Value> toJSON(const ImplementationType& argument)
        requires (requires (const typename IDLType::InnerParameterType& value) {
            InspectorCanvasArgumentProcessor<IDLType>::toJSON(value);
        })
    {
        return mapToArray(argument, [](const auto& value) {
            return InspectorCanvasArgumentProcessor<IDLType>::toJSON(value);
        });
    }

    std::optional<InspectorCanvasProcessedArgument> operator()(InspectorCanvas& context, const ImplementationType& argument)
    {
        if constexpr (requires { toJSON(argument); })
            return { { context.valueIndexForData(toJSON(argument)->toJSONString()), RecordingSwizzleType::JSON } };

        auto array = JSON::ArrayOf<JSON::Value>::create();
        for (const auto& value : argument) {
            auto processed = InspectorCanvasArgumentProcessor<IDLType>{}(context, value);
            if (!processed)
                return std::nullopt;

            auto item = JSON::ArrayOf<JSON::Value>::create();
            item->addItem(processed->value.copyRef());
            item->addItem(static_cast<int>(processed->swizzleType));
            array->addItem(WTF::move(item));
        }
        return { { context.valueIndexForData(array->toJSONString()), RecordingSwizzleType::ArrayOf } };
    }
};

} // namespace WebCore

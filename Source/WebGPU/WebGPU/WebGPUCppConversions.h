/*
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

#pragma once

#import "WebGPUCppBridge.h"
#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <optional>
#import <type_traits>
#import <wtf/IndexedRange.h>
#import <wtf/OptionSet.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

// Conversions between the WebGPU C API (webgpu.h) and the WebGPU C++ API: enums, flags, strings and
// descriptors. fromAPI() returns std::nullopt for C API values that have no C++ API equivalent, such
// as the _Undefined values and unknown bits, and for descriptors that contain such a value.
// The descriptor conversions are the conversions of the C API entry points, so they do not depend
// on the implementation.
//
// C++ API descriptors borrow their arrays as spans. A descriptor conversion that has to convert the
// elements of an array takes a caller-owned XDescriptorStorage, stores the converted elements in it
// and returns a descriptor whose spans point into it. The storage has to outlive every use of the
// descriptor, and it must not be modified while the descriptor is in use.
//
// Handles in descriptors resolve to the C++ API objects through the transition bridge
// (WebGPUCppBridge.h), so the conversions do not depend on the implementation that created them.

namespace WebGPU::Metal {

inline String fromAPI(WGPUStringView string)
{
    if (!string.data)
        return { };
    if (string.length == WGPU_STRLEN)
        return String::fromUTF8(string.data);
    return String::fromUTF8(unsafeMakeSpan(string.data, string.length));
}

// Associates a chainable extension struct with its sType tag. Specialize for each struct
// that findChainedStruct() is used with.
template<typename T> struct ChainedStructSType;

template<> struct ChainedStructSType<WGPUShaderSourceWGSL> {
    static constexpr WGPUSType value = WGPUSType_ShaderSourceWGSL;
};

template<> struct ChainedStructSType<WGPUExternalTextureBindingEntry> {
    static constexpr WGPUSType value = WGPUSType_ExternalTextureBindingEntry;
};

template<> struct ChainedStructSType<WGPUExternalTextureBindingLayout> {
    static constexpr WGPUSType value = WGPUSType_ExternalTextureBindingLayout;
};

template<> struct ChainedStructSType<WGPUTextureComponentSwizzleDescriptor> {
    static constexpr WGPUSType value = WGPUSType_TextureComponentSwizzleDescriptor;
};

template<> struct ChainedStructSType<WGPURenderPassMaxDrawCount> {
    static constexpr WGPUSType value = WGPUSType_RenderPassMaxDrawCount;
};

// Walks a descriptor's nextInChain looking for one particular extension struct. Every
// chainable struct starts with its WGPUChainedStruct, so the match can be cast to it.
template<typename T>
inline const T* findChainedStruct(const WGPUChainedStruct* chain)
{
    static_assert(std::is_same_v<decltype(T::chain), WGPUChainedStruct>);
    for (; chain; chain = chain->next) {
        if (chain->sType == ChainedStructSType<T>::value)
            return reinterpret_cast<const T*>(chain);
    }
    return nullptr;
}

inline String fromAPI(const char* string)
{
    return String::fromUTF8(string);
}

constexpr std::optional<WebGPU::AddressMode> fromAPI(WGPUAddressMode value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUAddressMode_Undefined:
        return WebGPU::AddressMode::ClampToEdge;
    case WGPUAddressMode_ClampToEdge:
        return WebGPU::AddressMode::ClampToEdge;
    case WGPUAddressMode_Repeat:
        return WebGPU::AddressMode::Repeat;
    case WGPUAddressMode_MirrorRepeat:
        return WebGPU::AddressMode::MirrorRepeat;
    default:
        return std::nullopt;
    }
}

constexpr WGPUAddressMode toAPI(WebGPU::AddressMode value)
{
    switch (value) {
    case WebGPU::AddressMode::ClampToEdge:
        return WGPUAddressMode_ClampToEdge;
    case WebGPU::AddressMode::Repeat:
        return WGPUAddressMode_Repeat;
    case WebGPU::AddressMode::MirrorRepeat:
        return WGPUAddressMode_MirrorRepeat;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::BlendFactor> fromAPI(WGPUBlendFactor value)
{
    switch (value) {
    case WGPUBlendFactor_Zero:
        return WebGPU::BlendFactor::Zero;
    case WGPUBlendFactor_One:
        return WebGPU::BlendFactor::One;
    case WGPUBlendFactor_Src:
        return WebGPU::BlendFactor::Src;
    case WGPUBlendFactor_OneMinusSrc:
        return WebGPU::BlendFactor::OneMinusSrc;
    case WGPUBlendFactor_SrcAlpha:
        return WebGPU::BlendFactor::SrcAlpha;
    case WGPUBlendFactor_OneMinusSrcAlpha:
        return WebGPU::BlendFactor::OneMinusSrcAlpha;
    case WGPUBlendFactor_Dst:
        return WebGPU::BlendFactor::Dst;
    case WGPUBlendFactor_OneMinusDst:
        return WebGPU::BlendFactor::OneMinusDst;
    case WGPUBlendFactor_DstAlpha:
        return WebGPU::BlendFactor::DstAlpha;
    case WGPUBlendFactor_OneMinusDstAlpha:
        return WebGPU::BlendFactor::OneMinusDstAlpha;
    case WGPUBlendFactor_SrcAlphaSaturated:
        return WebGPU::BlendFactor::SrcAlphaSaturated;
    case WGPUBlendFactor_Constant:
        return WebGPU::BlendFactor::Constant;
    case WGPUBlendFactor_OneMinusConstant:
        return WebGPU::BlendFactor::OneMinusConstant;
    default:
        return std::nullopt;
    }
}

constexpr WGPUBlendFactor toAPI(WebGPU::BlendFactor value)
{
    switch (value) {
    case WebGPU::BlendFactor::Zero:
        return WGPUBlendFactor_Zero;
    case WebGPU::BlendFactor::One:
        return WGPUBlendFactor_One;
    case WebGPU::BlendFactor::Src:
        return WGPUBlendFactor_Src;
    case WebGPU::BlendFactor::OneMinusSrc:
        return WGPUBlendFactor_OneMinusSrc;
    case WebGPU::BlendFactor::SrcAlpha:
        return WGPUBlendFactor_SrcAlpha;
    case WebGPU::BlendFactor::OneMinusSrcAlpha:
        return WGPUBlendFactor_OneMinusSrcAlpha;
    case WebGPU::BlendFactor::Dst:
        return WGPUBlendFactor_Dst;
    case WebGPU::BlendFactor::OneMinusDst:
        return WGPUBlendFactor_OneMinusDst;
    case WebGPU::BlendFactor::DstAlpha:
        return WGPUBlendFactor_DstAlpha;
    case WebGPU::BlendFactor::OneMinusDstAlpha:
        return WGPUBlendFactor_OneMinusDstAlpha;
    case WebGPU::BlendFactor::SrcAlphaSaturated:
        return WGPUBlendFactor_SrcAlphaSaturated;
    case WebGPU::BlendFactor::Constant:
        return WGPUBlendFactor_Constant;
    case WebGPU::BlendFactor::OneMinusConstant:
        return WGPUBlendFactor_OneMinusConstant;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::BlendOperation> fromAPI(WGPUBlendOperation value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUBlendOperation_Undefined:
        return WebGPU::BlendOperation::Add;
    case WGPUBlendOperation_Add:
        return WebGPU::BlendOperation::Add;
    case WGPUBlendOperation_Subtract:
        return WebGPU::BlendOperation::Subtract;
    case WGPUBlendOperation_ReverseSubtract:
        return WebGPU::BlendOperation::ReverseSubtract;
    case WGPUBlendOperation_Min:
        return WebGPU::BlendOperation::Min;
    case WGPUBlendOperation_Max:
        return WebGPU::BlendOperation::Max;
    default:
        return std::nullopt;
    }
}

constexpr WGPUBlendOperation toAPI(WebGPU::BlendOperation value)
{
    switch (value) {
    case WebGPU::BlendOperation::Add:
        return WGPUBlendOperation_Add;
    case WebGPU::BlendOperation::Subtract:
        return WGPUBlendOperation_Subtract;
    case WebGPU::BlendOperation::ReverseSubtract:
        return WGPUBlendOperation_ReverseSubtract;
    case WebGPU::BlendOperation::Min:
        return WGPUBlendOperation_Min;
    case WebGPU::BlendOperation::Max:
        return WGPUBlendOperation_Max;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::BufferBindingType> fromAPI(WGPUBufferBindingType value)
{
    switch (value) {
    case WGPUBufferBindingType_Uniform:
        return WebGPU::BufferBindingType::Uniform;
    case WGPUBufferBindingType_Storage:
        return WebGPU::BufferBindingType::Storage;
    case WGPUBufferBindingType_ReadOnlyStorage:
        return WebGPU::BufferBindingType::ReadOnlyStorage;
    default:
        return std::nullopt;
    }
}

constexpr WGPUBufferBindingType toAPI(WebGPU::BufferBindingType value)
{
    switch (value) {
    case WebGPU::BufferBindingType::Uniform:
        return WGPUBufferBindingType_Uniform;
    case WebGPU::BufferBindingType::Storage:
        return WGPUBufferBindingType_Storage;
    case WebGPU::BufferBindingType::ReadOnlyStorage:
        return WGPUBufferBindingType_ReadOnlyStorage;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::CompareFunction> fromAPI(WGPUCompareFunction value)
{
    switch (value) {
    case WGPUCompareFunction_Never:
        return WebGPU::CompareFunction::Never;
    case WGPUCompareFunction_Less:
        return WebGPU::CompareFunction::Less;
    case WGPUCompareFunction_Equal:
        return WebGPU::CompareFunction::Equal;
    case WGPUCompareFunction_LessEqual:
        return WebGPU::CompareFunction::LessEqual;
    case WGPUCompareFunction_Greater:
        return WebGPU::CompareFunction::Greater;
    case WGPUCompareFunction_NotEqual:
        return WebGPU::CompareFunction::NotEqual;
    case WGPUCompareFunction_GreaterEqual:
        return WebGPU::CompareFunction::GreaterEqual;
    case WGPUCompareFunction_Always:
        return WebGPU::CompareFunction::Always;
    default:
        return std::nullopt;
    }
}

constexpr WGPUCompareFunction toAPI(WebGPU::CompareFunction value)
{
    switch (value) {
    case WebGPU::CompareFunction::Never:
        return WGPUCompareFunction_Never;
    case WebGPU::CompareFunction::Less:
        return WGPUCompareFunction_Less;
    case WebGPU::CompareFunction::Equal:
        return WGPUCompareFunction_Equal;
    case WebGPU::CompareFunction::LessEqual:
        return WGPUCompareFunction_LessEqual;
    case WebGPU::CompareFunction::Greater:
        return WGPUCompareFunction_Greater;
    case WebGPU::CompareFunction::NotEqual:
        return WGPUCompareFunction_NotEqual;
    case WebGPU::CompareFunction::GreaterEqual:
        return WGPUCompareFunction_GreaterEqual;
    case WebGPU::CompareFunction::Always:
        return WGPUCompareFunction_Always;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::CompilationMessageType> fromAPI(WGPUCompilationMessageType value)
{
    switch (value) {
    case WGPUCompilationMessageType_Error:
        return WebGPU::CompilationMessageType::Error;
    case WGPUCompilationMessageType_Warning:
        return WebGPU::CompilationMessageType::Warning;
    case WGPUCompilationMessageType_Info:
        return WebGPU::CompilationMessageType::Info;
    default:
        return std::nullopt;
    }
}

constexpr WGPUCompilationMessageType toAPI(WebGPU::CompilationMessageType value)
{
    switch (value) {
    case WebGPU::CompilationMessageType::Error:
        return WGPUCompilationMessageType_Error;
    case WebGPU::CompilationMessageType::Warning:
        return WGPUCompilationMessageType_Warning;
    case WebGPU::CompilationMessageType::Info:
        return WGPUCompilationMessageType_Info;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::CullMode> fromAPI(WGPUCullMode value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUCullMode_Undefined:
        return WebGPU::CullMode::None;
    case WGPUCullMode_None:
        return WebGPU::CullMode::None;
    case WGPUCullMode_Front:
        return WebGPU::CullMode::Front;
    case WGPUCullMode_Back:
        return WebGPU::CullMode::Back;
    default:
        return std::nullopt;
    }
}

constexpr WGPUCullMode toAPI(WebGPU::CullMode value)
{
    switch (value) {
    case WebGPU::CullMode::None:
        return WGPUCullMode_None;
    case WebGPU::CullMode::Front:
        return WGPUCullMode_Front;
    case WebGPU::CullMode::Back:
        return WGPUCullMode_Back;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::DeviceLostReason> fromAPI(WGPUDeviceLostReason value)
{
    switch (value) {
    case WGPUDeviceLostReason_Destroyed:
        return WebGPU::DeviceLostReason::Destroyed;
    case WGPUDeviceLostReason_Unknown:
        return WebGPU::DeviceLostReason::Unknown;
    default:
        return std::nullopt;
    }
}

constexpr WGPUDeviceLostReason toAPI(WebGPU::DeviceLostReason value)
{
    switch (value) {
    case WebGPU::DeviceLostReason::Destroyed:
        return WGPUDeviceLostReason_Destroyed;
    case WebGPU::DeviceLostReason::Unknown:
        return WGPUDeviceLostReason_Unknown;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::ErrorFilter> fromAPI(WGPUErrorFilter value)
{
    switch (value) {
    case WGPUErrorFilter_OutOfMemory:
        return WebGPU::ErrorFilter::OutOfMemory;
    case WGPUErrorFilter_Validation:
        return WebGPU::ErrorFilter::Validation;
    case WGPUErrorFilter_Internal:
        return WebGPU::ErrorFilter::Internal;
    default:
        return std::nullopt;
    }
}

constexpr WGPUErrorFilter toAPI(WebGPU::ErrorFilter value)
{
    switch (value) {
    case WebGPU::ErrorFilter::OutOfMemory:
        return WGPUErrorFilter_OutOfMemory;
    case WebGPU::ErrorFilter::Validation:
        return WGPUErrorFilter_Validation;
    case WebGPU::ErrorFilter::Internal:
        return WGPUErrorFilter_Internal;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::FeatureName> fromAPI(WGPUFeatureName value)
{
    switch (value) {
    case WGPUFeatureName_DepthClipControl:
        return WebGPU::FeatureName::DepthClipControl;
    case WGPUFeatureName_Depth32FloatStencil8:
        return WebGPU::FeatureName::Depth32floatStencil8;
    case WGPUFeatureName_TextureCompressionBC:
        return WebGPU::FeatureName::TextureCompressionBc;
    case WGPUFeatureName_TextureCompressionBCSliced3D:
        return WebGPU::FeatureName::TextureCompressionBcSliced3d;
    case WGPUFeatureName_TextureCompressionETC2:
        return WebGPU::FeatureName::TextureCompressionEtc2;
    case WGPUFeatureName_TextureCompressionASTC:
        return WebGPU::FeatureName::TextureCompressionAstc;
    case WGPUFeatureName_TextureCompressionASTCSliced3D:
        return WebGPU::FeatureName::TextureCompressionAstcSliced3d;
    case WGPUFeatureName_TimestampQuery:
        return WebGPU::FeatureName::TimestampQuery;
    case WGPUFeatureName_IndirectFirstInstance:
        return WebGPU::FeatureName::IndirectFirstInstance;
    case WGPUFeatureName_BGRA8UnormStorage:
        return WebGPU::FeatureName::Bgra8unormStorage;
    case WGPUFeatureName_ShaderF16:
        return WebGPU::FeatureName::ShaderF16;
    case WGPUFeatureName_RG11B10UfloatRenderable:
        return WebGPU::FeatureName::Rg11b10ufloatRenderable;
    case WGPUFeatureName_Float32Filterable:
        return WebGPU::FeatureName::Float32Filterable;
    case WGPUFeatureName_Float32Blendable:
        return WebGPU::FeatureName::Float32Blendable;
    case WGPUFeatureName_ClipDistances:
        return WebGPU::FeatureName::ClipDistances;
    case WGPUFeatureName_DualSourceBlending:
        return WebGPU::FeatureName::DualSourceBlending;
    case WGPUFeatureName_CoreFeaturesAndLimits:
        return WebGPU::FeatureName::CoreFeaturesAndLimits;
    case WGPUFeatureName_TextureFormatsTier1:
        return WebGPU::FeatureName::TextureFormatsTier1;
    case WGPUFeatureName_TextureFormatsTier2:
        return WebGPU::FeatureName::TextureFormatsTier2;
    case WGPUFeatureName_PrimitiveIndex:
        return WebGPU::FeatureName::PrimitiveIndex;
    case WGPUFeatureName_Subgroups:
        return WebGPU::FeatureName::Subgroups;
    default:
        return std::nullopt;
    }
}

constexpr std::optional<WebGPU::FilterMode> fromAPI(WGPUFilterMode value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUFilterMode_Undefined:
        return WebGPU::FilterMode::Nearest;
    case WGPUFilterMode_Nearest:
        return WebGPU::FilterMode::Nearest;
    case WGPUFilterMode_Linear:
        return WebGPU::FilterMode::Linear;
    default:
        return std::nullopt;
    }
}

constexpr WGPUFilterMode toAPI(WebGPU::FilterMode value)
{
    switch (value) {
    case WebGPU::FilterMode::Nearest:
        return WGPUFilterMode_Nearest;
    case WebGPU::FilterMode::Linear:
        return WGPUFilterMode_Linear;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::FrontFace> fromAPI(WGPUFrontFace value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUFrontFace_Undefined:
        return WebGPU::FrontFace::CCW;
    case WGPUFrontFace_CCW:
        return WebGPU::FrontFace::CCW;
    case WGPUFrontFace_CW:
        return WebGPU::FrontFace::CW;
    default:
        return std::nullopt;
    }
}

constexpr WGPUFrontFace toAPI(WebGPU::FrontFace value)
{
    switch (value) {
    case WebGPU::FrontFace::CCW:
        return WGPUFrontFace_CCW;
    case WebGPU::FrontFace::CW:
        return WGPUFrontFace_CW;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::IndexFormat> fromAPI(WGPUIndexFormat value)
{
    switch (value) {
    case WGPUIndexFormat_Uint16:
        return WebGPU::IndexFormat::Uint16;
    case WGPUIndexFormat_Uint32:
        return WebGPU::IndexFormat::Uint32;
    default:
        return std::nullopt;
    }
}

constexpr WGPUIndexFormat toAPI(WebGPU::IndexFormat value)
{
    switch (value) {
    case WebGPU::IndexFormat::Uint16:
        return WGPUIndexFormat_Uint16;
    case WebGPU::IndexFormat::Uint32:
        return WGPUIndexFormat_Uint32;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::LoadOp> fromAPI(WGPULoadOp value)
{
    switch (value) {
    case WGPULoadOp_Load:
        return WebGPU::LoadOp::Load;
    case WGPULoadOp_Clear:
        return WebGPU::LoadOp::Clear;
    default:
        return std::nullopt;
    }
}

constexpr WGPULoadOp toAPI(WebGPU::LoadOp value)
{
    switch (value) {
    case WebGPU::LoadOp::Load:
        return WGPULoadOp_Load;
    case WebGPU::LoadOp::Clear:
        return WGPULoadOp_Clear;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::MipmapFilterMode> fromAPI(WGPUMipmapFilterMode value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUMipmapFilterMode_Undefined:
        return WebGPU::MipmapFilterMode::Nearest;
    case WGPUMipmapFilterMode_Nearest:
        return WebGPU::MipmapFilterMode::Nearest;
    case WGPUMipmapFilterMode_Linear:
        return WebGPU::MipmapFilterMode::Linear;
    default:
        return std::nullopt;
    }
}

constexpr WGPUMipmapFilterMode toAPI(WebGPU::MipmapFilterMode value)
{
    switch (value) {
    case WebGPU::MipmapFilterMode::Nearest:
        return WGPUMipmapFilterMode_Nearest;
    case WebGPU::MipmapFilterMode::Linear:
        return WGPUMipmapFilterMode_Linear;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::PowerPreference> fromAPI(WGPUPowerPreference value)
{
    switch (value) {
    case WGPUPowerPreference_LowPower:
        return WebGPU::PowerPreference::LowPower;
    case WGPUPowerPreference_HighPerformance:
        return WebGPU::PowerPreference::HighPerformance;
    default:
        return std::nullopt;
    }
}

constexpr WGPUPowerPreference toAPI(WebGPU::PowerPreference value)
{
    switch (value) {
    case WebGPU::PowerPreference::LowPower:
        return WGPUPowerPreference_LowPower;
    case WebGPU::PowerPreference::HighPerformance:
        return WGPUPowerPreference_HighPerformance;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::PrimitiveTopology> fromAPI(WGPUPrimitiveTopology value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUPrimitiveTopology_Undefined:
        return WebGPU::PrimitiveTopology::TriangleList;
    case WGPUPrimitiveTopology_PointList:
        return WebGPU::PrimitiveTopology::PointList;
    case WGPUPrimitiveTopology_LineList:
        return WebGPU::PrimitiveTopology::LineList;
    case WGPUPrimitiveTopology_LineStrip:
        return WebGPU::PrimitiveTopology::LineStrip;
    case WGPUPrimitiveTopology_TriangleList:
        return WebGPU::PrimitiveTopology::TriangleList;
    case WGPUPrimitiveTopology_TriangleStrip:
        return WebGPU::PrimitiveTopology::TriangleStrip;
    default:
        return std::nullopt;
    }
}

constexpr WGPUPrimitiveTopology toAPI(WebGPU::PrimitiveTopology value)
{
    switch (value) {
    case WebGPU::PrimitiveTopology::PointList:
        return WGPUPrimitiveTopology_PointList;
    case WebGPU::PrimitiveTopology::LineList:
        return WGPUPrimitiveTopology_LineList;
    case WebGPU::PrimitiveTopology::LineStrip:
        return WGPUPrimitiveTopology_LineStrip;
    case WebGPU::PrimitiveTopology::TriangleList:
        return WGPUPrimitiveTopology_TriangleList;
    case WebGPU::PrimitiveTopology::TriangleStrip:
        return WGPUPrimitiveTopology_TriangleStrip;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::QueryType> fromAPI(WGPUQueryType value)
{
    switch (value) {
    case WGPUQueryType_Occlusion:
        return WebGPU::QueryType::Occlusion;
    case WGPUQueryType_Timestamp:
        return WebGPU::QueryType::Timestamp;
    default:
        return std::nullopt;
    }
}

constexpr WGPUQueryType toAPI(WebGPU::QueryType value)
{
    switch (value) {
    case WebGPU::QueryType::Occlusion:
        return WGPUQueryType_Occlusion;
    case WebGPU::QueryType::Timestamp:
        return WGPUQueryType_Timestamp;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::SamplerBindingType> fromAPI(WGPUSamplerBindingType value)
{
    switch (value) {
    case WGPUSamplerBindingType_Filtering:
        return WebGPU::SamplerBindingType::Filtering;
    case WGPUSamplerBindingType_NonFiltering:
        return WebGPU::SamplerBindingType::NonFiltering;
    case WGPUSamplerBindingType_Comparison:
        return WebGPU::SamplerBindingType::Comparison;
    default:
        return std::nullopt;
    }
}

constexpr WGPUSamplerBindingType toAPI(WebGPU::SamplerBindingType value)
{
    switch (value) {
    case WebGPU::SamplerBindingType::Filtering:
        return WGPUSamplerBindingType_Filtering;
    case WebGPU::SamplerBindingType::NonFiltering:
        return WGPUSamplerBindingType_NonFiltering;
    case WebGPU::SamplerBindingType::Comparison:
        return WGPUSamplerBindingType_Comparison;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::StencilOperation> fromAPI(WGPUStencilOperation value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUStencilOperation_Undefined:
        return WebGPU::StencilOperation::Keep;
    case WGPUStencilOperation_Keep:
        return WebGPU::StencilOperation::Keep;
    case WGPUStencilOperation_Zero:
        return WebGPU::StencilOperation::Zero;
    case WGPUStencilOperation_Replace:
        return WebGPU::StencilOperation::Replace;
    case WGPUStencilOperation_Invert:
        return WebGPU::StencilOperation::Invert;
    case WGPUStencilOperation_IncrementClamp:
        return WebGPU::StencilOperation::IncrementClamp;
    case WGPUStencilOperation_DecrementClamp:
        return WebGPU::StencilOperation::DecrementClamp;
    case WGPUStencilOperation_IncrementWrap:
        return WebGPU::StencilOperation::IncrementWrap;
    case WGPUStencilOperation_DecrementWrap:
        return WebGPU::StencilOperation::DecrementWrap;
    default:
        return std::nullopt;
    }
}

constexpr WGPUStencilOperation toAPI(WebGPU::StencilOperation value)
{
    switch (value) {
    case WebGPU::StencilOperation::Keep:
        return WGPUStencilOperation_Keep;
    case WebGPU::StencilOperation::Zero:
        return WGPUStencilOperation_Zero;
    case WebGPU::StencilOperation::Replace:
        return WGPUStencilOperation_Replace;
    case WebGPU::StencilOperation::Invert:
        return WGPUStencilOperation_Invert;
    case WebGPU::StencilOperation::IncrementClamp:
        return WGPUStencilOperation_IncrementClamp;
    case WebGPU::StencilOperation::DecrementClamp:
        return WGPUStencilOperation_DecrementClamp;
    case WebGPU::StencilOperation::IncrementWrap:
        return WGPUStencilOperation_IncrementWrap;
    case WebGPU::StencilOperation::DecrementWrap:
        return WGPUStencilOperation_DecrementWrap;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::StorageTextureAccess> fromAPI(WGPUStorageTextureAccess value)
{
    switch (value) {
    case WGPUStorageTextureAccess_WriteOnly:
        return WebGPU::StorageTextureAccess::WriteOnly;
    case WGPUStorageTextureAccess_ReadOnly:
        return WebGPU::StorageTextureAccess::ReadOnly;
    case WGPUStorageTextureAccess_ReadWrite:
        return WebGPU::StorageTextureAccess::ReadWrite;
    default:
        return std::nullopt;
    }
}

constexpr WGPUStorageTextureAccess toAPI(WebGPU::StorageTextureAccess value)
{
    switch (value) {
    case WebGPU::StorageTextureAccess::WriteOnly:
        return WGPUStorageTextureAccess_WriteOnly;
    case WebGPU::StorageTextureAccess::ReadOnly:
        return WGPUStorageTextureAccess_ReadOnly;
    case WebGPU::StorageTextureAccess::ReadWrite:
        return WGPUStorageTextureAccess_ReadWrite;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// std::nullopt is WGPULoadOp_Undefined.
constexpr WGPULoadOp toAPI(std::optional<WebGPU::LoadOp> value)
{
    return value ? toAPI(*value) : WGPULoadOp_Undefined;
}

constexpr std::optional<WebGPU::StoreOp> fromAPI(WGPUStoreOp value)
{
    switch (value) {
    case WGPUStoreOp_Store:
        return WebGPU::StoreOp::Store;
    case WGPUStoreOp_Discard:
        return WebGPU::StoreOp::Discard;
    default:
        return std::nullopt;
    }
}

constexpr WGPUStoreOp toAPI(WebGPU::StoreOp value)
{
    switch (value) {
    case WebGPU::StoreOp::Store:
        return WGPUStoreOp_Store;
    case WebGPU::StoreOp::Discard:
        return WGPUStoreOp_Discard;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::TextureAspect> fromAPI(WGPUTextureAspect value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUTextureAspect_Undefined:
        return WebGPU::TextureAspect::All;
    case WGPUTextureAspect_All:
        return WebGPU::TextureAspect::All;
    case WGPUTextureAspect_StencilOnly:
        return WebGPU::TextureAspect::StencilOnly;
    case WGPUTextureAspect_DepthOnly:
        return WebGPU::TextureAspect::DepthOnly;
    default:
        return std::nullopt;
    }
}

constexpr WGPUTextureAspect toAPI(WebGPU::TextureAspect value)
{
    switch (value) {
    case WebGPU::TextureAspect::All:
        return WGPUTextureAspect_All;
    case WebGPU::TextureAspect::StencilOnly:
        return WGPUTextureAspect_StencilOnly;
    case WebGPU::TextureAspect::DepthOnly:
        return WGPUTextureAspect_DepthOnly;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::TextureDimension> fromAPI(WGPUTextureDimension value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUTextureDimension_Undefined:
        return WebGPU::TextureDimension::_2d;
    case WGPUTextureDimension_1D:
        return WebGPU::TextureDimension::_1d;
    case WGPUTextureDimension_2D:
        return WebGPU::TextureDimension::_2d;
    case WGPUTextureDimension_3D:
        return WebGPU::TextureDimension::_3d;
    default:
        return std::nullopt;
    }
}

constexpr WGPUTextureDimension toAPI(WebGPU::TextureDimension value)
{
    switch (value) {
    case WebGPU::TextureDimension::_1d:
        return WGPUTextureDimension_1D;
    case WebGPU::TextureDimension::_2d:
        return WGPUTextureDimension_2D;
    case WebGPU::TextureDimension::_3d:
        return WGPUTextureDimension_3D;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::TextureFormat> fromAPI(WGPUTextureFormat value)
{
    switch (value) {
    case WGPUTextureFormat_R8Unorm:
        return WebGPU::TextureFormat::R8unorm;
    case WGPUTextureFormat_R8Snorm:
        return WebGPU::TextureFormat::R8snorm;
    case WGPUTextureFormat_R8Uint:
        return WebGPU::TextureFormat::R8uint;
    case WGPUTextureFormat_R8Sint:
        return WebGPU::TextureFormat::R8sint;
    case WGPUTextureFormat_R16Uint:
        return WebGPU::TextureFormat::R16uint;
    case WGPUTextureFormat_R16Unorm:
        return WebGPU::TextureFormat::R16unorm;
    case WGPUTextureFormat_R16Snorm:
        return WebGPU::TextureFormat::R16snorm;
    case WGPUTextureFormat_RG16Unorm:
        return WebGPU::TextureFormat::Rg16unorm;
    case WGPUTextureFormat_RG16Snorm:
        return WebGPU::TextureFormat::Rg16snorm;
    case WGPUTextureFormat_RGBA16Unorm:
        return WebGPU::TextureFormat::Rgba16unorm;
    case WGPUTextureFormat_RGBA16Snorm:
        return WebGPU::TextureFormat::Rgba16snorm;
    case WGPUTextureFormat_R16Sint:
        return WebGPU::TextureFormat::R16sint;
    case WGPUTextureFormat_R16Float:
        return WebGPU::TextureFormat::R16float;
    case WGPUTextureFormat_RG8Unorm:
        return WebGPU::TextureFormat::Rg8unorm;
    case WGPUTextureFormat_RG8Snorm:
        return WebGPU::TextureFormat::Rg8snorm;
    case WGPUTextureFormat_RG8Uint:
        return WebGPU::TextureFormat::Rg8uint;
    case WGPUTextureFormat_RG8Sint:
        return WebGPU::TextureFormat::Rg8sint;
    case WGPUTextureFormat_R32Uint:
        return WebGPU::TextureFormat::R32uint;
    case WGPUTextureFormat_R32Sint:
        return WebGPU::TextureFormat::R32sint;
    case WGPUTextureFormat_R32Float:
        return WebGPU::TextureFormat::R32float;
    case WGPUTextureFormat_RG16Uint:
        return WebGPU::TextureFormat::Rg16uint;
    case WGPUTextureFormat_RG16Sint:
        return WebGPU::TextureFormat::Rg16sint;
    case WGPUTextureFormat_RG16Float:
        return WebGPU::TextureFormat::Rg16float;
    case WGPUTextureFormat_RGBA8Unorm:
        return WebGPU::TextureFormat::Rgba8unorm;
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return WebGPU::TextureFormat::Rgba8unormSRGB;
    case WGPUTextureFormat_RGBA8Snorm:
        return WebGPU::TextureFormat::Rgba8snorm;
    case WGPUTextureFormat_RGBA8Uint:
        return WebGPU::TextureFormat::Rgba8uint;
    case WGPUTextureFormat_RGBA8Sint:
        return WebGPU::TextureFormat::Rgba8sint;
    case WGPUTextureFormat_BGRA8Unorm:
        return WebGPU::TextureFormat::Bgra8unorm;
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return WebGPU::TextureFormat::Bgra8unormSRGB;
    case WGPUTextureFormat_RGB9E5Ufloat:
        return WebGPU::TextureFormat::Rgb9e5ufloat;
    case WGPUTextureFormat_RGB10A2Uint:
        return WebGPU::TextureFormat::Rgb10a2uint;
    case WGPUTextureFormat_RGB10A2Unorm:
        return WebGPU::TextureFormat::Rgb10a2unorm;
    case WGPUTextureFormat_RG11B10Ufloat:
        return WebGPU::TextureFormat::Rg11b10ufloat;
    case WGPUTextureFormat_RG32Uint:
        return WebGPU::TextureFormat::Rg32uint;
    case WGPUTextureFormat_RG32Sint:
        return WebGPU::TextureFormat::Rg32sint;
    case WGPUTextureFormat_RG32Float:
        return WebGPU::TextureFormat::Rg32float;
    case WGPUTextureFormat_RGBA16Uint:
        return WebGPU::TextureFormat::Rgba16uint;
    case WGPUTextureFormat_RGBA16Sint:
        return WebGPU::TextureFormat::Rgba16sint;
    case WGPUTextureFormat_RGBA16Float:
        return WebGPU::TextureFormat::Rgba16float;
    case WGPUTextureFormat_RGBA32Uint:
        return WebGPU::TextureFormat::Rgba32uint;
    case WGPUTextureFormat_RGBA32Sint:
        return WebGPU::TextureFormat::Rgba32sint;
    case WGPUTextureFormat_RGBA32Float:
        return WebGPU::TextureFormat::Rgba32float;
    case WGPUTextureFormat_Stencil8:
        return WebGPU::TextureFormat::Stencil8;
    case WGPUTextureFormat_Depth16Unorm:
        return WebGPU::TextureFormat::Depth16unorm;
    case WGPUTextureFormat_Depth24Plus:
        return WebGPU::TextureFormat::Depth24plus;
    case WGPUTextureFormat_Depth24PlusStencil8:
        return WebGPU::TextureFormat::Depth24plusStencil8;
    case WGPUTextureFormat_Depth32Float:
        return WebGPU::TextureFormat::Depth32float;
    case WGPUTextureFormat_BC1RGBAUnorm:
        return WebGPU::TextureFormat::Bc1RgbaUnorm;
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return WebGPU::TextureFormat::Bc1RgbaUnormSRGB;
    case WGPUTextureFormat_BC2RGBAUnorm:
        return WebGPU::TextureFormat::Bc2RgbaUnorm;
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return WebGPU::TextureFormat::Bc2RgbaUnormSRGB;
    case WGPUTextureFormat_BC3RGBAUnorm:
        return WebGPU::TextureFormat::Bc3RgbaUnorm;
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return WebGPU::TextureFormat::Bc3RgbaUnormSRGB;
    case WGPUTextureFormat_BC4RUnorm:
        return WebGPU::TextureFormat::Bc4RUnorm;
    case WGPUTextureFormat_BC4RSnorm:
        return WebGPU::TextureFormat::Bc4RSnorm;
    case WGPUTextureFormat_BC5RGUnorm:
        return WebGPU::TextureFormat::Bc5RgUnorm;
    case WGPUTextureFormat_BC5RGSnorm:
        return WebGPU::TextureFormat::Bc5RgSnorm;
    case WGPUTextureFormat_BC6HRGBUfloat:
        return WebGPU::TextureFormat::Bc6hRgbUfloat;
    case WGPUTextureFormat_BC6HRGBFloat:
        return WebGPU::TextureFormat::Bc6hRgbFloat;
    case WGPUTextureFormat_BC7RGBAUnorm:
        return WebGPU::TextureFormat::Bc7RgbaUnorm;
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return WebGPU::TextureFormat::Bc7RgbaUnormSRGB;
    case WGPUTextureFormat_ETC2RGB8Unorm:
        return WebGPU::TextureFormat::Etc2Rgb8unorm;
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        return WebGPU::TextureFormat::Etc2Rgb8unormSRGB;
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
        return WebGPU::TextureFormat::Etc2Rgb8a1unorm;
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
        return WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB;
    case WGPUTextureFormat_ETC2RGBA8Unorm:
        return WebGPU::TextureFormat::Etc2Rgba8unorm;
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return WebGPU::TextureFormat::Etc2Rgba8unormSRGB;
    case WGPUTextureFormat_EACR11Unorm:
        return WebGPU::TextureFormat::EacR11unorm;
    case WGPUTextureFormat_EACR11Snorm:
        return WebGPU::TextureFormat::EacR11snorm;
    case WGPUTextureFormat_EACRG11Unorm:
        return WebGPU::TextureFormat::EacRg11unorm;
    case WGPUTextureFormat_EACRG11Snorm:
        return WebGPU::TextureFormat::EacRg11snorm;
    case WGPUTextureFormat_ASTC4x4Unorm:
        return WebGPU::TextureFormat::Astc4x4Unorm;
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
        return WebGPU::TextureFormat::Astc4x4UnormSRGB;
    case WGPUTextureFormat_ASTC5x4Unorm:
        return WebGPU::TextureFormat::Astc5x4Unorm;
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
        return WebGPU::TextureFormat::Astc5x4UnormSRGB;
    case WGPUTextureFormat_ASTC5x5Unorm:
        return WebGPU::TextureFormat::Astc5x5Unorm;
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
        return WebGPU::TextureFormat::Astc5x5UnormSRGB;
    case WGPUTextureFormat_ASTC6x5Unorm:
        return WebGPU::TextureFormat::Astc6x5Unorm;
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
        return WebGPU::TextureFormat::Astc6x5UnormSRGB;
    case WGPUTextureFormat_ASTC6x6Unorm:
        return WebGPU::TextureFormat::Astc6x6Unorm;
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
        return WebGPU::TextureFormat::Astc6x6UnormSRGB;
    case WGPUTextureFormat_ASTC8x5Unorm:
        return WebGPU::TextureFormat::Astc8x5Unorm;
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
        return WebGPU::TextureFormat::Astc8x5UnormSRGB;
    case WGPUTextureFormat_ASTC8x6Unorm:
        return WebGPU::TextureFormat::Astc8x6Unorm;
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
        return WebGPU::TextureFormat::Astc8x6UnormSRGB;
    case WGPUTextureFormat_ASTC8x8Unorm:
        return WebGPU::TextureFormat::Astc8x8Unorm;
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
        return WebGPU::TextureFormat::Astc8x8UnormSRGB;
    case WGPUTextureFormat_ASTC10x5Unorm:
        return WebGPU::TextureFormat::Astc10x5Unorm;
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
        return WebGPU::TextureFormat::Astc10x5UnormSRGB;
    case WGPUTextureFormat_ASTC10x6Unorm:
        return WebGPU::TextureFormat::Astc10x6Unorm;
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
        return WebGPU::TextureFormat::Astc10x6UnormSRGB;
    case WGPUTextureFormat_ASTC10x8Unorm:
        return WebGPU::TextureFormat::Astc10x8Unorm;
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
        return WebGPU::TextureFormat::Astc10x8UnormSRGB;
    case WGPUTextureFormat_ASTC10x10Unorm:
        return WebGPU::TextureFormat::Astc10x10Unorm;
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
        return WebGPU::TextureFormat::Astc10x10UnormSRGB;
    case WGPUTextureFormat_ASTC12x10Unorm:
        return WebGPU::TextureFormat::Astc12x10Unorm;
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
        return WebGPU::TextureFormat::Astc12x10UnormSRGB;
    case WGPUTextureFormat_ASTC12x12Unorm:
        return WebGPU::TextureFormat::Astc12x12Unorm;
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return WebGPU::TextureFormat::Astc12x12UnormSRGB;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return WebGPU::TextureFormat::Depth32floatStencil8;
    default:
        return std::nullopt;
    }
}

constexpr WGPUTextureFormat toAPI(WebGPU::TextureFormat value)
{
    switch (value) {
    case WebGPU::TextureFormat::R8unorm:
        return WGPUTextureFormat_R8Unorm;
    case WebGPU::TextureFormat::R8snorm:
        return WGPUTextureFormat_R8Snorm;
    case WebGPU::TextureFormat::R8uint:
        return WGPUTextureFormat_R8Uint;
    case WebGPU::TextureFormat::R8sint:
        return WGPUTextureFormat_R8Sint;
    case WebGPU::TextureFormat::R16uint:
        return WGPUTextureFormat_R16Uint;
    case WebGPU::TextureFormat::R16unorm:
        return WGPUTextureFormat_R16Unorm;
    case WebGPU::TextureFormat::R16snorm:
        return WGPUTextureFormat_R16Snorm;
    case WebGPU::TextureFormat::Rg16unorm:
        return WGPUTextureFormat_RG16Unorm;
    case WebGPU::TextureFormat::Rg16snorm:
        return WGPUTextureFormat_RG16Snorm;
    case WebGPU::TextureFormat::Rgba16unorm:
        return WGPUTextureFormat_RGBA16Unorm;
    case WebGPU::TextureFormat::Rgba16snorm:
        return WGPUTextureFormat_RGBA16Snorm;
    case WebGPU::TextureFormat::R16sint:
        return WGPUTextureFormat_R16Sint;
    case WebGPU::TextureFormat::R16float:
        return WGPUTextureFormat_R16Float;
    case WebGPU::TextureFormat::Rg8unorm:
        return WGPUTextureFormat_RG8Unorm;
    case WebGPU::TextureFormat::Rg8snorm:
        return WGPUTextureFormat_RG8Snorm;
    case WebGPU::TextureFormat::Rg8uint:
        return WGPUTextureFormat_RG8Uint;
    case WebGPU::TextureFormat::Rg8sint:
        return WGPUTextureFormat_RG8Sint;
    case WebGPU::TextureFormat::R32uint:
        return WGPUTextureFormat_R32Uint;
    case WebGPU::TextureFormat::R32sint:
        return WGPUTextureFormat_R32Sint;
    case WebGPU::TextureFormat::R32float:
        return WGPUTextureFormat_R32Float;
    case WebGPU::TextureFormat::Rg16uint:
        return WGPUTextureFormat_RG16Uint;
    case WebGPU::TextureFormat::Rg16sint:
        return WGPUTextureFormat_RG16Sint;
    case WebGPU::TextureFormat::Rg16float:
        return WGPUTextureFormat_RG16Float;
    case WebGPU::TextureFormat::Rgba8unorm:
        return WGPUTextureFormat_RGBA8Unorm;
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        return WGPUTextureFormat_RGBA8UnormSrgb;
    case WebGPU::TextureFormat::Rgba8snorm:
        return WGPUTextureFormat_RGBA8Snorm;
    case WebGPU::TextureFormat::Rgba8uint:
        return WGPUTextureFormat_RGBA8Uint;
    case WebGPU::TextureFormat::Rgba8sint:
        return WGPUTextureFormat_RGBA8Sint;
    case WebGPU::TextureFormat::Bgra8unorm:
        return WGPUTextureFormat_BGRA8Unorm;
    case WebGPU::TextureFormat::Bgra8unormSRGB:
        return WGPUTextureFormat_BGRA8UnormSrgb;
    case WebGPU::TextureFormat::Rgb9e5ufloat:
        return WGPUTextureFormat_RGB9E5Ufloat;
    case WebGPU::TextureFormat::Rgb10a2uint:
        return WGPUTextureFormat_RGB10A2Uint;
    case WebGPU::TextureFormat::Rgb10a2unorm:
        return WGPUTextureFormat_RGB10A2Unorm;
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return WGPUTextureFormat_RG11B10Ufloat;
    case WebGPU::TextureFormat::Rg32uint:
        return WGPUTextureFormat_RG32Uint;
    case WebGPU::TextureFormat::Rg32sint:
        return WGPUTextureFormat_RG32Sint;
    case WebGPU::TextureFormat::Rg32float:
        return WGPUTextureFormat_RG32Float;
    case WebGPU::TextureFormat::Rgba16uint:
        return WGPUTextureFormat_RGBA16Uint;
    case WebGPU::TextureFormat::Rgba16sint:
        return WGPUTextureFormat_RGBA16Sint;
    case WebGPU::TextureFormat::Rgba16float:
        return WGPUTextureFormat_RGBA16Float;
    case WebGPU::TextureFormat::Rgba32uint:
        return WGPUTextureFormat_RGBA32Uint;
    case WebGPU::TextureFormat::Rgba32sint:
        return WGPUTextureFormat_RGBA32Sint;
    case WebGPU::TextureFormat::Rgba32float:
        return WGPUTextureFormat_RGBA32Float;
    case WebGPU::TextureFormat::Stencil8:
        return WGPUTextureFormat_Stencil8;
    case WebGPU::TextureFormat::Depth16unorm:
        return WGPUTextureFormat_Depth16Unorm;
    case WebGPU::TextureFormat::Depth24plus:
        return WGPUTextureFormat_Depth24Plus;
    case WebGPU::TextureFormat::Depth24plusStencil8:
        return WGPUTextureFormat_Depth24PlusStencil8;
    case WebGPU::TextureFormat::Depth32float:
        return WGPUTextureFormat_Depth32Float;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
        return WGPUTextureFormat_BC1RGBAUnorm;
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
        return WGPUTextureFormat_BC1RGBAUnormSrgb;
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
        return WGPUTextureFormat_BC2RGBAUnorm;
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
        return WGPUTextureFormat_BC2RGBAUnormSrgb;
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
        return WGPUTextureFormat_BC3RGBAUnorm;
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return WGPUTextureFormat_BC3RGBAUnormSrgb;
    case WebGPU::TextureFormat::Bc4RUnorm:
        return WGPUTextureFormat_BC4RUnorm;
    case WebGPU::TextureFormat::Bc4RSnorm:
        return WGPUTextureFormat_BC4RSnorm;
    case WebGPU::TextureFormat::Bc5RgUnorm:
        return WGPUTextureFormat_BC5RGUnorm;
    case WebGPU::TextureFormat::Bc5RgSnorm:
        return WGPUTextureFormat_BC5RGSnorm;
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
        return WGPUTextureFormat_BC6HRGBUfloat;
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return WGPUTextureFormat_BC6HRGBFloat;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
        return WGPUTextureFormat_BC7RGBAUnorm;
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return WGPUTextureFormat_BC7RGBAUnormSrgb;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
        return WGPUTextureFormat_ETC2RGB8Unorm;
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return WGPUTextureFormat_ETC2RGB8UnormSrgb;
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
        return WGPUTextureFormat_ETC2RGB8A1Unorm;
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
        return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
        return WGPUTextureFormat_ETC2RGBA8Unorm;
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
    case WebGPU::TextureFormat::EacR11unorm:
        return WGPUTextureFormat_EACR11Unorm;
    case WebGPU::TextureFormat::EacR11snorm:
        return WGPUTextureFormat_EACR11Snorm;
    case WebGPU::TextureFormat::EacRg11unorm:
        return WGPUTextureFormat_EACRG11Unorm;
    case WebGPU::TextureFormat::EacRg11snorm:
        return WGPUTextureFormat_EACRG11Snorm;
    case WebGPU::TextureFormat::Astc4x4Unorm:
        return WGPUTextureFormat_ASTC4x4Unorm;
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
        return WGPUTextureFormat_ASTC4x4UnormSrgb;
    case WebGPU::TextureFormat::Astc5x4Unorm:
        return WGPUTextureFormat_ASTC5x4Unorm;
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
        return WGPUTextureFormat_ASTC5x4UnormSrgb;
    case WebGPU::TextureFormat::Astc5x5Unorm:
        return WGPUTextureFormat_ASTC5x5Unorm;
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
        return WGPUTextureFormat_ASTC5x5UnormSrgb;
    case WebGPU::TextureFormat::Astc6x5Unorm:
        return WGPUTextureFormat_ASTC6x5Unorm;
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
        return WGPUTextureFormat_ASTC6x5UnormSrgb;
    case WebGPU::TextureFormat::Astc6x6Unorm:
        return WGPUTextureFormat_ASTC6x6Unorm;
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
        return WGPUTextureFormat_ASTC6x6UnormSrgb;
    case WebGPU::TextureFormat::Astc8x5Unorm:
        return WGPUTextureFormat_ASTC8x5Unorm;
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
        return WGPUTextureFormat_ASTC8x5UnormSrgb;
    case WebGPU::TextureFormat::Astc8x6Unorm:
        return WGPUTextureFormat_ASTC8x6Unorm;
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
        return WGPUTextureFormat_ASTC8x6UnormSrgb;
    case WebGPU::TextureFormat::Astc8x8Unorm:
        return WGPUTextureFormat_ASTC8x8Unorm;
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
        return WGPUTextureFormat_ASTC8x8UnormSrgb;
    case WebGPU::TextureFormat::Astc10x5Unorm:
        return WGPUTextureFormat_ASTC10x5Unorm;
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
        return WGPUTextureFormat_ASTC10x5UnormSrgb;
    case WebGPU::TextureFormat::Astc10x6Unorm:
        return WGPUTextureFormat_ASTC10x6Unorm;
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
        return WGPUTextureFormat_ASTC10x6UnormSrgb;
    case WebGPU::TextureFormat::Astc10x8Unorm:
        return WGPUTextureFormat_ASTC10x8Unorm;
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
        return WGPUTextureFormat_ASTC10x8UnormSrgb;
    case WebGPU::TextureFormat::Astc10x10Unorm:
        return WGPUTextureFormat_ASTC10x10Unorm;
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
        return WGPUTextureFormat_ASTC10x10UnormSrgb;
    case WebGPU::TextureFormat::Astc12x10Unorm:
        return WGPUTextureFormat_ASTC12x10Unorm;
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
        return WGPUTextureFormat_ASTC12x10UnormSrgb;
    case WebGPU::TextureFormat::Astc12x12Unorm:
        return WGPUTextureFormat_ASTC12x12Unorm;
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return WGPUTextureFormat_ASTC12x12UnormSrgb;
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return WGPUTextureFormat_Depth32FloatStencil8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::TextureSampleType> fromAPI(WGPUTextureSampleType value)
{
    switch (value) {
    case WGPUTextureSampleType_Float:
        return WebGPU::TextureSampleType::Float;
    case WGPUTextureSampleType_UnfilterableFloat:
        return WebGPU::TextureSampleType::UnfilterableFloat;
    case WGPUTextureSampleType_Depth:
        return WebGPU::TextureSampleType::Depth;
    case WGPUTextureSampleType_Sint:
        return WebGPU::TextureSampleType::Sint;
    case WGPUTextureSampleType_Uint:
        return WebGPU::TextureSampleType::Uint;
    default:
        return std::nullopt;
    }
}

constexpr WGPUTextureSampleType toAPI(WebGPU::TextureSampleType value)
{
    switch (value) {
    case WebGPU::TextureSampleType::Float:
        return WGPUTextureSampleType_Float;
    case WebGPU::TextureSampleType::UnfilterableFloat:
        return WGPUTextureSampleType_UnfilterableFloat;
    case WebGPU::TextureSampleType::Depth:
        return WGPUTextureSampleType_Depth;
    case WebGPU::TextureSampleType::Sint:
        return WGPUTextureSampleType_Sint;
    case WebGPU::TextureSampleType::Uint:
        return WGPUTextureSampleType_Uint;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::TextureViewDimension> fromAPI(WGPUTextureViewDimension value)
{
    switch (value) {
    case WGPUTextureViewDimension_1D:
        return WebGPU::TextureViewDimension::_1d;
    case WGPUTextureViewDimension_2D:
        return WebGPU::TextureViewDimension::_2d;
    case WGPUTextureViewDimension_2DArray:
        return WebGPU::TextureViewDimension::_2dArray;
    case WGPUTextureViewDimension_Cube:
        return WebGPU::TextureViewDimension::Cube;
    case WGPUTextureViewDimension_CubeArray:
        return WebGPU::TextureViewDimension::CubeArray;
    case WGPUTextureViewDimension_3D:
        return WebGPU::TextureViewDimension::_3d;
    default:
        return std::nullopt;
    }
}

constexpr WGPUTextureViewDimension toAPI(WebGPU::TextureViewDimension value)
{
    switch (value) {
    case WebGPU::TextureViewDimension::_1d:
        return WGPUTextureViewDimension_1D;
    case WebGPU::TextureViewDimension::_2d:
        return WGPUTextureViewDimension_2D;
    case WebGPU::TextureViewDimension::_2dArray:
        return WGPUTextureViewDimension_2DArray;
    case WebGPU::TextureViewDimension::Cube:
        return WGPUTextureViewDimension_Cube;
    case WebGPU::TextureViewDimension::CubeArray:
        return WGPUTextureViewDimension_CubeArray;
    case WebGPU::TextureViewDimension::_3d:
        return WGPUTextureViewDimension_3D;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::VertexFormat> fromAPI(WGPUVertexFormat value)
{
    switch (value) {
    case WGPUVertexFormat_Uint8:
        return WebGPU::VertexFormat::Uint8;
    case WGPUVertexFormat_Uint8x2:
        return WebGPU::VertexFormat::Uint8x2;
    case WGPUVertexFormat_Uint8x4:
        return WebGPU::VertexFormat::Uint8x4;
    case WGPUVertexFormat_Sint8:
        return WebGPU::VertexFormat::Sint8;
    case WGPUVertexFormat_Sint8x2:
        return WebGPU::VertexFormat::Sint8x2;
    case WGPUVertexFormat_Sint8x4:
        return WebGPU::VertexFormat::Sint8x4;
    case WGPUVertexFormat_Unorm8:
        return WebGPU::VertexFormat::Unorm8;
    case WGPUVertexFormat_Unorm8x2:
        return WebGPU::VertexFormat::Unorm8x2;
    case WGPUVertexFormat_Unorm8x4:
        return WebGPU::VertexFormat::Unorm8x4;
    case WGPUVertexFormat_Snorm8:
        return WebGPU::VertexFormat::Snorm8;
    case WGPUVertexFormat_Snorm8x2:
        return WebGPU::VertexFormat::Snorm8x2;
    case WGPUVertexFormat_Snorm8x4:
        return WebGPU::VertexFormat::Snorm8x4;
    case WGPUVertexFormat_Uint16:
        return WebGPU::VertexFormat::Uint16;
    case WGPUVertexFormat_Uint16x2:
        return WebGPU::VertexFormat::Uint16x2;
    case WGPUVertexFormat_Uint16x4:
        return WebGPU::VertexFormat::Uint16x4;
    case WGPUVertexFormat_Sint16:
        return WebGPU::VertexFormat::Sint16;
    case WGPUVertexFormat_Sint16x2:
        return WebGPU::VertexFormat::Sint16x2;
    case WGPUVertexFormat_Sint16x4:
        return WebGPU::VertexFormat::Sint16x4;
    case WGPUVertexFormat_Unorm16:
        return WebGPU::VertexFormat::Unorm16;
    case WGPUVertexFormat_Unorm16x2:
        return WebGPU::VertexFormat::Unorm16x2;
    case WGPUVertexFormat_Unorm16x4:
        return WebGPU::VertexFormat::Unorm16x4;
    case WGPUVertexFormat_Snorm16:
        return WebGPU::VertexFormat::Snorm16;
    case WGPUVertexFormat_Snorm16x2:
        return WebGPU::VertexFormat::Snorm16x2;
    case WGPUVertexFormat_Snorm16x4:
        return WebGPU::VertexFormat::Snorm16x4;
    case WGPUVertexFormat_Float16:
        return WebGPU::VertexFormat::Float16;
    case WGPUVertexFormat_Float16x2:
        return WebGPU::VertexFormat::Float16x2;
    case WGPUVertexFormat_Float16x4:
        return WebGPU::VertexFormat::Float16x4;
    case WGPUVertexFormat_Float32:
        return WebGPU::VertexFormat::Float32;
    case WGPUVertexFormat_Float32x2:
        return WebGPU::VertexFormat::Float32x2;
    case WGPUVertexFormat_Float32x3:
        return WebGPU::VertexFormat::Float32x3;
    case WGPUVertexFormat_Float32x4:
        return WebGPU::VertexFormat::Float32x4;
    case WGPUVertexFormat_Uint32:
        return WebGPU::VertexFormat::Uint32;
    case WGPUVertexFormat_Uint32x2:
        return WebGPU::VertexFormat::Uint32x2;
    case WGPUVertexFormat_Uint32x3:
        return WebGPU::VertexFormat::Uint32x3;
    case WGPUVertexFormat_Uint32x4:
        return WebGPU::VertexFormat::Uint32x4;
    case WGPUVertexFormat_Sint32:
        return WebGPU::VertexFormat::Sint32;
    case WGPUVertexFormat_Sint32x2:
        return WebGPU::VertexFormat::Sint32x2;
    case WGPUVertexFormat_Sint32x3:
        return WebGPU::VertexFormat::Sint32x3;
    case WGPUVertexFormat_Sint32x4:
        return WebGPU::VertexFormat::Sint32x4;
    case WGPUVertexFormat_Unorm10_10_10_2:
        return WebGPU::VertexFormat::Unorm1010102;
    case WGPUVertexFormat_Unorm8x4BGRA:
        return WebGPU::VertexFormat::Unorm8x4Bgra;
    default:
        return std::nullopt;
    }
}

constexpr std::optional<WebGPU::VertexStepMode> fromAPI(WGPUVertexStepMode value)
{
    switch (value) {
    // _Undefined is the default.
    case WGPUVertexStepMode_Undefined:
        return WebGPU::VertexStepMode::Vertex;
    case WGPUVertexStepMode_Vertex:
        return WebGPU::VertexStepMode::Vertex;
    case WGPUVertexStepMode_Instance:
        return WebGPU::VertexStepMode::Instance;
    default:
        return std::nullopt;
    }
}

constexpr WGPUVertexStepMode toAPI(WebGPU::VertexStepMode value)
{
    switch (value) {
    case WebGPU::VertexStepMode::Vertex:
        return WGPUVertexStepMode_Vertex;
    case WebGPU::VertexStepMode::Instance:
        return WGPUVertexStepMode_Instance;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<OptionSet<WebGPU::BufferUsage>> bufferUsageFromAPI(WGPUBufferUsage value)
{
    OptionSet<WebGPU::BufferUsage> result;
    if (value & WGPUBufferUsage_MapRead) {
        result.add(WebGPU::BufferUsage::MapRead);
        value &= ~WGPUBufferUsage_MapRead;
    }
    if (value & WGPUBufferUsage_MapWrite) {
        result.add(WebGPU::BufferUsage::MapWrite);
        value &= ~WGPUBufferUsage_MapWrite;
    }
    if (value & WGPUBufferUsage_CopySrc) {
        result.add(WebGPU::BufferUsage::CopySource);
        value &= ~WGPUBufferUsage_CopySrc;
    }
    if (value & WGPUBufferUsage_CopyDst) {
        result.add(WebGPU::BufferUsage::CopyDestination);
        value &= ~WGPUBufferUsage_CopyDst;
    }
    if (value & WGPUBufferUsage_Index) {
        result.add(WebGPU::BufferUsage::Index);
        value &= ~WGPUBufferUsage_Index;
    }
    if (value & WGPUBufferUsage_Vertex) {
        result.add(WebGPU::BufferUsage::Vertex);
        value &= ~WGPUBufferUsage_Vertex;
    }
    if (value & WGPUBufferUsage_Uniform) {
        result.add(WebGPU::BufferUsage::Uniform);
        value &= ~WGPUBufferUsage_Uniform;
    }
    if (value & WGPUBufferUsage_Storage) {
        result.add(WebGPU::BufferUsage::Storage);
        value &= ~WGPUBufferUsage_Storage;
    }
    if (value & WGPUBufferUsage_Indirect) {
        result.add(WebGPU::BufferUsage::Indirect);
        value &= ~WGPUBufferUsage_Indirect;
    }
    if (value & WGPUBufferUsage_QueryResolve) {
        result.add(WebGPU::BufferUsage::QueryResolve);
        value &= ~WGPUBufferUsage_QueryResolve;
    }
    if (value)
        return std::nullopt;
    return result;
}

constexpr WGPUBufferUsage toAPI(OptionSet<WebGPU::BufferUsage> value)
{
    WGPUBufferUsage result = 0;
    if (value.contains(WebGPU::BufferUsage::MapRead))
        result |= WGPUBufferUsage_MapRead;
    if (value.contains(WebGPU::BufferUsage::MapWrite))
        result |= WGPUBufferUsage_MapWrite;
    if (value.contains(WebGPU::BufferUsage::CopySource))
        result |= WGPUBufferUsage_CopySrc;
    if (value.contains(WebGPU::BufferUsage::CopyDestination))
        result |= WGPUBufferUsage_CopyDst;
    if (value.contains(WebGPU::BufferUsage::Index))
        result |= WGPUBufferUsage_Index;
    if (value.contains(WebGPU::BufferUsage::Vertex))
        result |= WGPUBufferUsage_Vertex;
    if (value.contains(WebGPU::BufferUsage::Uniform))
        result |= WGPUBufferUsage_Uniform;
    if (value.contains(WebGPU::BufferUsage::Storage))
        result |= WGPUBufferUsage_Storage;
    if (value.contains(WebGPU::BufferUsage::Indirect))
        result |= WGPUBufferUsage_Indirect;
    if (value.contains(WebGPU::BufferUsage::QueryResolve))
        result |= WGPUBufferUsage_QueryResolve;
    return result;
}

constexpr std::optional<OptionSet<WebGPU::ColorWrite>> colorWriteFromAPI(WGPUColorWriteMask value)
{
    OptionSet<WebGPU::ColorWrite> result;
    if (value & WGPUColorWriteMask_Red) {
        result.add(WebGPU::ColorWrite::Red);
        value &= ~WGPUColorWriteMask_Red;
    }
    if (value & WGPUColorWriteMask_Green) {
        result.add(WebGPU::ColorWrite::Green);
        value &= ~WGPUColorWriteMask_Green;
    }
    if (value & WGPUColorWriteMask_Blue) {
        result.add(WebGPU::ColorWrite::Blue);
        value &= ~WGPUColorWriteMask_Blue;
    }
    if (value & WGPUColorWriteMask_Alpha) {
        result.add(WebGPU::ColorWrite::Alpha);
        value &= ~WGPUColorWriteMask_Alpha;
    }
    if (value)
        return std::nullopt;
    return result;
}

constexpr WGPUColorWriteMask toAPI(OptionSet<WebGPU::ColorWrite> value)
{
    WGPUColorWriteMask result = 0;
    if (value.contains(WebGPU::ColorWrite::Red))
        result |= WGPUColorWriteMask_Red;
    if (value.contains(WebGPU::ColorWrite::Green))
        result |= WGPUColorWriteMask_Green;
    if (value.contains(WebGPU::ColorWrite::Blue))
        result |= WGPUColorWriteMask_Blue;
    if (value.contains(WebGPU::ColorWrite::Alpha))
        result |= WGPUColorWriteMask_Alpha;
    return result;
}

constexpr std::optional<OptionSet<WebGPU::MapMode>> mapModeFromAPI(WGPUMapMode value)
{
    OptionSet<WebGPU::MapMode> result;
    if (value & WGPUMapMode_Read) {
        result.add(WebGPU::MapMode::Read);
        value &= ~WGPUMapMode_Read;
    }
    if (value & WGPUMapMode_Write) {
        result.add(WebGPU::MapMode::Write);
        value &= ~WGPUMapMode_Write;
    }
    if (value)
        return std::nullopt;
    return result;
}

constexpr WGPUMapMode toAPI(OptionSet<WebGPU::MapMode> value)
{
    WGPUMapMode result = 0;
    if (value.contains(WebGPU::MapMode::Read))
        result |= WGPUMapMode_Read;
    if (value.contains(WebGPU::MapMode::Write))
        result |= WGPUMapMode_Write;
    return result;
}

constexpr std::optional<OptionSet<WebGPU::ShaderStage>> shaderStageFromAPI(WGPUShaderStage value)
{
    OptionSet<WebGPU::ShaderStage> result;
    if (value & WGPUShaderStage_Vertex) {
        result.add(WebGPU::ShaderStage::Vertex);
        value &= ~WGPUShaderStage_Vertex;
    }
    if (value & WGPUShaderStage_Fragment) {
        result.add(WebGPU::ShaderStage::Fragment);
        value &= ~WGPUShaderStage_Fragment;
    }
    if (value & WGPUShaderStage_Compute) {
        result.add(WebGPU::ShaderStage::Compute);
        value &= ~WGPUShaderStage_Compute;
    }
    if (value)
        return std::nullopt;
    return result;
}

constexpr WGPUShaderStage toAPI(OptionSet<WebGPU::ShaderStage> value)
{
    WGPUShaderStage result = 0;
    if (value.contains(WebGPU::ShaderStage::Vertex))
        result |= WGPUShaderStage_Vertex;
    if (value.contains(WebGPU::ShaderStage::Fragment))
        result |= WGPUShaderStage_Fragment;
    if (value.contains(WebGPU::ShaderStage::Compute))
        result |= WGPUShaderStage_Compute;
    return result;
}

constexpr std::optional<OptionSet<WebGPU::TextureUsage>> textureUsageFromAPI(WGPUTextureUsage value)
{
    OptionSet<WebGPU::TextureUsage> result;
    if (value & WGPUTextureUsage_CopySrc) {
        result.add(WebGPU::TextureUsage::CopySource);
        value &= ~WGPUTextureUsage_CopySrc;
    }
    if (value & WGPUTextureUsage_CopyDst) {
        result.add(WebGPU::TextureUsage::CopyDestination);
        value &= ~WGPUTextureUsage_CopyDst;
    }
    if (value & WGPUTextureUsage_TextureBinding) {
        result.add(WebGPU::TextureUsage::TextureBinding);
        value &= ~WGPUTextureUsage_TextureBinding;
    }
    if (value & WGPUTextureUsage_StorageBinding) {
        result.add(WebGPU::TextureUsage::StorageBinding);
        value &= ~WGPUTextureUsage_StorageBinding;
    }
    if (value & WGPUTextureUsage_RenderAttachment) {
        result.add(WebGPU::TextureUsage::RenderAttachment);
        value &= ~WGPUTextureUsage_RenderAttachment;
    }
    if (value & WGPUTextureUsage_TransientAttachment) {
        result.add(WebGPU::TextureUsage::Transient);
        value &= ~WGPUTextureUsage_TransientAttachment;
    }
    if (value)
        return std::nullopt;
    return result;
}

constexpr WGPUTextureUsage toAPI(OptionSet<WebGPU::TextureUsage> value)
{
    WGPUTextureUsage result = 0;
    if (value.contains(WebGPU::TextureUsage::CopySource))
        result |= WGPUTextureUsage_CopySrc;
    if (value.contains(WebGPU::TextureUsage::CopyDestination))
        result |= WGPUTextureUsage_CopyDst;
    if (value.contains(WebGPU::TextureUsage::TextureBinding))
        result |= WGPUTextureUsage_TextureBinding;
    if (value.contains(WebGPU::TextureUsage::StorageBinding))
        result |= WGPUTextureUsage_StorageBinding;
    if (value.contains(WebGPU::TextureUsage::RenderAttachment))
        result |= WGPUTextureUsage_RenderAttachment;
    if (value.contains(WebGPU::TextureUsage::Transient))
        result |= WGPUTextureUsage_TransientAttachment;
    return result;
}

constexpr WebGPU::Extent3D fromAPI(const WGPUExtent3D& extent)
{
    return { .width = extent.width, .height = extent.height, .depthOrArrayLayers = extent.depthOrArrayLayers };
}

constexpr WGPUExtent3D toAPI(const WebGPU::Extent3D& extent)
{
    return { .width = extent.width, .height = extent.height, .depthOrArrayLayers = extent.depthOrArrayLayers };
}

constexpr WebGPU::Origin3D fromAPI(const WGPUOrigin3D& origin)
{
    return { .x = origin.x, .y = origin.y, .z = origin.z };
}

// WGPU_COPY_STRIDE_UNDEFINED is std::nullopt.
constexpr WebGPU::TexelCopyBufferLayout fromAPI(const WGPUTexelCopyBufferLayout& layout)
{
    auto stride = [](uint32_t value) {
        return value == WGPU_COPY_STRIDE_UNDEFINED ? std::nullopt : std::optional { value };
    };
    return { .offset = layout.offset, .bytesPerRow = stride(layout.bytesPerRow), .rowsPerImage = stride(layout.rowsPerImage) };
}

inline std::optional<WebGPU::TexelCopyBufferInfo> fromAPI(const WGPUTexelCopyBufferInfo& info)
{
    if (!info.buffer)
        return std::nullopt;
    return WebGPU::TexelCopyBufferInfo { .layout = fromAPI(info.layout), .buffer = WebGPU::fromAPI(info.buffer) };
}

inline std::optional<WebGPU::TexelCopyTextureInfo> fromAPI(const WGPUTexelCopyTextureInfo& info)
{
    auto aspect = fromAPI(info.aspect);
    if (!info.texture || !aspect)
        return std::nullopt;
    return WebGPU::TexelCopyTextureInfo {
        .texture = WebGPU::fromAPI(info.texture),
        .mipLevel = info.mipLevel,
        .origin = fromAPI(info.origin),
        .aspect = *aspect,
    };
}

inline std::optional<WebGPU::BufferDescriptor> fromAPI(const WGPUBufferDescriptor& descriptor)
{
    auto usage = bufferUsageFromAPI(descriptor.usage);
    if (!usage)
        return std::nullopt;

    return WebGPU::BufferDescriptor {
        .label = fromAPI(descriptor.label),
        .usage = *usage,
        .size = descriptor.size,
        .mappedAtCreation = !!descriptor.mappedAtCreation,
    };
}

// The size argument of wgpuBufferMapAsync() and wgpuBufferGetMappedRange(). WGPU_WHOLE_MAP_SIZE
// means the rest of the buffer after the offset.
constexpr std::optional<uint64_t> mapSizeFromAPI(size_t size)
{
    if (size == WGPU_WHOLE_MAP_SIZE)
        return std::nullopt;
    return size;
}

inline std::optional<WebGPU::QuerySetDescriptor> fromAPI(const WGPUQuerySetDescriptor& descriptor)
{
    auto type = fromAPI(descriptor.type);
    if (!type)
        return std::nullopt;

    return WebGPU::QuerySetDescriptor {
        .label = fromAPI(descriptor.label),
        .type = *type,
        .count = descriptor.count,
    };
}

inline std::optional<WebGPU::SamplerDescriptor> fromAPI(const WGPUSamplerDescriptor& descriptor)
{
    auto addressModeU = fromAPI(descriptor.addressModeU);
    auto addressModeV = fromAPI(descriptor.addressModeV);
    auto addressModeW = fromAPI(descriptor.addressModeW);
    auto magFilter = fromAPI(descriptor.magFilter);
    auto minFilter = fromAPI(descriptor.minFilter);
    auto mipmapFilter = fromAPI(descriptor.mipmapFilter);
    if (!addressModeU || !addressModeV || !addressModeW || !magFilter || !minFilter || !mipmapFilter)
        return std::nullopt;

    // WGPUCompareFunction_Undefined means that the sampler is not a comparison sampler.
    std::optional<WebGPU::CompareFunction> compare;
    if (descriptor.compare != WGPUCompareFunction_Undefined) {
        compare = fromAPI(descriptor.compare);
        if (!compare)
            return std::nullopt;
    }

    return WebGPU::SamplerDescriptor {
        .label = fromAPI(descriptor.label),
        .addressModeU = *addressModeU,
        .addressModeV = *addressModeV,
        .addressModeW = *addressModeW,
        .magFilter = *magFilter,
        .minFilter = *minFilter,
        .mipmapFilter = *mipmapFilter,
        .lodMinClamp = descriptor.lodMinClamp,
        .lodMaxClamp = descriptor.lodMaxClamp,
        .compare = compare,
        .maxAnisotropy = descriptor.maxAnisotropy,
    };
}

struct TextureDescriptorStorage {
    Vector<WebGPU::TextureFormat> viewFormats;
};

inline std::optional<WebGPU::TextureDescriptor> fromAPI(const WGPUTextureDescriptor& descriptor, TextureDescriptorStorage& storage LIFETIME_BOUND)
{
    auto usage = textureUsageFromAPI(descriptor.usage);
    auto dimension = fromAPI(descriptor.dimension);
    auto format = fromAPI(descriptor.format);
    if (!usage || !dimension || !format)
        return std::nullopt;

    storage.viewFormats.clear();
    for (auto viewFormat : unsafeMakeSpan(descriptor.viewFormats, descriptor.viewFormatCount)) {
        auto apiViewFormat = fromAPI(viewFormat);
        if (!apiViewFormat)
            return std::nullopt;
        storage.viewFormats.append(*apiViewFormat);
    }

    return WebGPU::TextureDescriptor {
        .label = fromAPI(descriptor.label),
        .usage = *usage,
        .dimension = *dimension,
        .size = fromAPI(descriptor.size),
        .format = *format,
        .mipLevelCount = descriptor.mipLevelCount,
        .sampleCount = descriptor.sampleCount,
        .viewFormats = storage.viewFormats.span(),
    };
}

// WGPUComponentSwizzle_Undefined selects the component of the channel itself.
inline std::optional<WebGPU::ComponentSwizzle> fromAPI(WGPUComponentSwizzle component, WebGPU::ComponentSwizzle channel)
{
    switch (component) {
    case WGPUComponentSwizzle_Undefined:
        return channel;
    case WGPUComponentSwizzle_Zero:
        return WebGPU::ComponentSwizzle::Zero;
    case WGPUComponentSwizzle_One:
        return WebGPU::ComponentSwizzle::One;
    case WGPUComponentSwizzle_R:
        return WebGPU::ComponentSwizzle::R;
    case WGPUComponentSwizzle_G:
        return WebGPU::ComponentSwizzle::G;
    case WGPUComponentSwizzle_B:
        return WebGPU::ComponentSwizzle::B;
    case WGPUComponentSwizzle_A:
        return WebGPU::ComponentSwizzle::A;
    case WGPUComponentSwizzle_Force32:
        break;
    }
    return std::nullopt;
}

inline std::optional<WebGPU::TextureComponentSwizzle> fromAPI(const WGPUTextureComponentSwizzle& swizzle)
{
    auto r = fromAPI(swizzle.r, WebGPU::ComponentSwizzle::R);
    auto g = fromAPI(swizzle.g, WebGPU::ComponentSwizzle::G);
    auto b = fromAPI(swizzle.b, WebGPU::ComponentSwizzle::B);
    auto a = fromAPI(swizzle.a, WebGPU::ComponentSwizzle::A);
    if (!r || !g || !b || !a)
        return std::nullopt;
    return WebGPU::TextureComponentSwizzle { *r, *g, *b, *a };
}

inline std::optional<WebGPU::TextureViewDescriptor> fromAPI(const WGPUTextureViewDescriptor& descriptor)
{
    // WGPUTextureFormat_Undefined, WGPUTextureViewDimension_Undefined, WGPU_MIP_LEVEL_COUNT_UNDEFINED
    // and WGPU_ARRAY_LAYER_COUNT_UNDEFINED mean that the value comes from the texture.
    std::optional<WebGPU::TextureFormat> format;
    if (descriptor.format != WGPUTextureFormat_Undefined) {
        format = fromAPI(descriptor.format);
        if (!format)
            return std::nullopt;
    }

    std::optional<WebGPU::TextureViewDimension> dimension;
    if (descriptor.dimension != WGPUTextureViewDimension_Undefined) {
        dimension = fromAPI(descriptor.dimension);
        if (!dimension)
            return std::nullopt;
    }

    auto aspect = fromAPI(descriptor.aspect);
    auto usage = textureUsageFromAPI(descriptor.usage);
    // Without a chained swizzle descriptor, the view does not swizzle.
    std::optional<WebGPU::TextureComponentSwizzle> swizzle = WebGPU::TextureComponentSwizzle { };
    if (auto* swizzleDescriptor = findChainedStruct<WGPUTextureComponentSwizzleDescriptor>(descriptor.nextInChain))
        swizzle = fromAPI(swizzleDescriptor->swizzle);
    if (!aspect || !usage || !swizzle)
        return std::nullopt;

    return WebGPU::TextureViewDescriptor {
        .label = fromAPI(descriptor.label),
        .format = format,
        .dimension = dimension,
        .baseMipLevel = descriptor.baseMipLevel,
        .mipLevelCount = descriptor.mipLevelCount == WGPU_MIP_LEVEL_COUNT_UNDEFINED ? std::nullopt : std::optional<uint32_t> { descriptor.mipLevelCount },
        .baseArrayLayer = descriptor.baseArrayLayer,
        .arrayLayerCount = descriptor.arrayLayerCount == WGPU_ARRAY_LAYER_COUNT_UNDEFINED ? std::nullopt : std::optional<uint32_t> { descriptor.arrayLayerCount },
        .aspect = *aspect,
        .usage = *usage,
        .swizzle = *swizzle,
    };
}

inline std::optional<WebGPU::BindGroupLayoutEntry> fromAPI(const WGPUBindGroupLayoutEntry& entry)
{
    auto visibility = shaderStageFromAPI(entry.visibility);
    if (!visibility)
        return std::nullopt;

    WebGPU::BindGroupLayoutEntry result {
        .binding = entry.binding,
        .visibility = *visibility,
        .buffer = std::nullopt,
        .sampler = std::nullopt,
        .texture = std::nullopt,
        .storageTexture = std::nullopt,
        .externalTexture = std::nullopt,
    };

    // A binding layout is present when its type is not _BindingNotUsed, and _Undefined is its
    // default. An external texture binding layout is chained.
    if (entry.bindingArraySize > 1)
        return std::nullopt;

    if (entry.buffer.type != WGPUBufferBindingType_BindingNotUsed) {
        auto type = entry.buffer.type == WGPUBufferBindingType_Undefined ? std::optional { WebGPU::BufferBindingType::Uniform } : fromAPI(entry.buffer.type);
        if (!type)
            return std::nullopt;
        result.buffer = WebGPU::BufferBindingLayout {
            .type = *type,
            .hasDynamicOffset = !!entry.buffer.hasDynamicOffset,
            .minBindingSize = entry.buffer.minBindingSize,
        };
    }

    if (entry.sampler.type != WGPUSamplerBindingType_BindingNotUsed) {
        auto type = entry.sampler.type == WGPUSamplerBindingType_Undefined ? std::optional { WebGPU::SamplerBindingType::Filtering } : fromAPI(entry.sampler.type);
        if (!type)
            return std::nullopt;
        result.sampler = WebGPU::SamplerBindingLayout { .type = *type };
    }

    if (entry.texture.sampleType != WGPUTextureSampleType_BindingNotUsed) {
        auto sampleType = entry.texture.sampleType == WGPUTextureSampleType_Undefined ? std::optional { WebGPU::TextureSampleType::Float } : fromAPI(entry.texture.sampleType);
        auto viewDimension = entry.texture.viewDimension == WGPUTextureViewDimension_Undefined ? std::optional { WebGPU::TextureViewDimension::_2d } : fromAPI(entry.texture.viewDimension);
        if (!sampleType || !viewDimension)
            return std::nullopt;
        result.texture = WebGPU::TextureBindingLayout {
            .sampleType = *sampleType,
            .viewDimension = *viewDimension,
            .multisampled = !!entry.texture.multisampled,
        };
    }

    if (entry.storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed) {
        auto access = entry.storageTexture.access == WGPUStorageTextureAccess_Undefined ? std::optional { WebGPU::StorageTextureAccess::WriteOnly } : fromAPI(entry.storageTexture.access);
        auto format = fromAPI(entry.storageTexture.format);
        auto viewDimension = entry.storageTexture.viewDimension == WGPUTextureViewDimension_Undefined ? std::optional { WebGPU::TextureViewDimension::_2d } : fromAPI(entry.storageTexture.viewDimension);
        if (!access || !format || !viewDimension)
            return std::nullopt;
        result.storageTexture = WebGPU::StorageTextureBindingLayout {
            .access = *access,
            .format = *format,
            .viewDimension = *viewDimension,
        };
    }

    if (findChainedStruct<WGPUExternalTextureBindingLayout>(entry.nextInChain))
        result.externalTexture = WebGPU::ExternalTextureBindingLayout { };

    return result;
}

struct BindGroupLayoutDescriptorStorage {
    Vector<WebGPU::BindGroupLayoutEntry> entries;
};

inline std::optional<WebGPU::BindGroupLayoutDescriptor> fromAPI(const WGPUBindGroupLayoutDescriptor& descriptor, BindGroupLayoutDescriptorStorage& storage LIFETIME_BOUND)
{
    storage.entries.clear();
    for (auto& entry : unsafeMakeSpan(descriptor.entries, descriptor.entryCount)) {
        auto apiEntry = fromAPI(entry);
        if (!apiEntry)
            return std::nullopt;
        storage.entries.append(WTF::move(*apiEntry));
    }

    return WebGPU::BindGroupLayoutDescriptor {
        .label = fromAPI(descriptor.label),
        .entries = storage.entries.span(),
    };
}

struct PipelineLayoutDescriptorStorage {
    Vector<Ref<WebGPU::BindGroupLayout>> bindGroupLayouts;
};

inline std::optional<WebGPU::PipelineLayoutDescriptor> fromAPI(const WGPUPipelineLayoutDescriptor& descriptor, PipelineLayoutDescriptorStorage& storage LIFETIME_BOUND)
{
    // A null bindGroupLayouts is a layout that the pipeline generates from its shaders.
    std::optional<std::span<const Ref<WebGPU::BindGroupLayout>>> bindGroupLayouts;
    storage.bindGroupLayouts.clear();
    if (descriptor.bindGroupLayouts) {
        for (auto bindGroupLayout : unsafeMakeSpan(descriptor.bindGroupLayouts, descriptor.bindGroupLayoutCount)) {
            if (!bindGroupLayout)
                return std::nullopt;
            storage.bindGroupLayouts.append(Ref { WebGPU::fromAPI(bindGroupLayout) });
        }
        bindGroupLayouts = storage.bindGroupLayouts.span();
    }

    return WebGPU::PipelineLayoutDescriptor {
        .label = fromAPI(descriptor.label),
        .bindGroupLayouts = bindGroupLayouts,
    };
}

inline std::optional<WebGPU::BindGroupEntry> fromAPI(const WGPUBindGroupEntry& entry)
{
    // Exactly one of the resource handles has to be set. An external texture is chained.
    auto* externalTextureEntry = findChainedStruct<WGPUExternalTextureBindingEntry>(entry.nextInChain);
    WGPUExternalTexture externalTexture = externalTextureEntry ? externalTextureEntry->externalTexture : nullptr;
    if (!!entry.buffer + !!entry.sampler + !!entry.textureView + !!externalTexture != 1)
        return std::nullopt;

    auto resource = [&]() -> WebGPU::BindingResource {
        if (entry.buffer) {
            return WebGPU::BufferBinding {
                .buffer = WebGPU::fromAPI(entry.buffer),
                .offset = entry.offset,
                .size = entry.size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional<uint64_t> { entry.size },
            };
        }
        if (entry.sampler)
            return Ref { WebGPU::fromAPI(entry.sampler) };
        if (entry.textureView)
            return Ref { WebGPU::fromAPI(entry.textureView) };
        return Ref { WebGPU::fromAPI(externalTexture) };
    }();

    return WebGPU::BindGroupEntry {
        .binding = entry.binding,
        .resource = WTF::move(resource),
    };
}

struct ShaderModuleDescriptorStorage {
    Vector<WebGPU::ShaderModuleCompilationHint> hints;
};

struct BindGroupDescriptorStorage {
    Vector<WebGPU::BindGroupEntry> entries;
};

inline std::optional<WebGPU::BindGroupDescriptor> fromAPI(const WGPUBindGroupDescriptor& descriptor, BindGroupDescriptorStorage& storage LIFETIME_BOUND)
{
    if (!descriptor.layout)
        return std::nullopt;

    storage.entries.clear();
    for (auto& entry : unsafeMakeSpan(descriptor.entries, descriptor.entryCount)) {
        auto apiEntry = fromAPI(entry);
        if (!apiEntry)
            return std::nullopt;
        storage.entries.append(WTF::move(*apiEntry));
    }

    return WebGPU::BindGroupDescriptor {
        .label = fromAPI(descriptor.label),
        .layout = WebGPU::fromAPI(descriptor.layout),
        .entries = storage.entries.span(),
    };
}

inline std::optional<WebGPU::ShaderModuleDescriptor> fromAPI(const WGPUShaderModuleDescriptor& descriptor, ShaderModuleDescriptorStorage& storage LIFETIME_BOUND)
{
    // WGSL is the only shader source.
    auto* wgsl = findChainedStruct<WGPUShaderSourceWGSL>(descriptor.nextInChain);
    if (!wgsl)
        return std::nullopt;

    auto code = fromAPI(wgsl->code);
    if (code.isNull())
        return std::nullopt;

    UNUSED_PARAM(storage);
    return WebGPU::ShaderModuleDescriptor {
        .label = fromAPI(descriptor.label),
        .code = WTF::move(code),
        .hints = { },
    };
}

// The C API line positions stay zero-based, as they were. The messages point into the UTF-8
// strings of messageStorage.
inline Vector<WGPUCompilationMessage> toAPI(const WebGPU::CompilationInfo& compilationInfo, Vector<UTF8CString>& messageStorage LIFETIME_BOUND)
{
    messageStorage.clear();
    for (auto& message : compilationInfo.messages)
        messageStorage.append(message.message.utf8());
    Vector<WGPUCompilationMessage> result;
    result.reserveInitialCapacity(compilationInfo.messages.size());
    for (auto [i, message] : indexedRange(compilationInfo.messages)) {
        auto utf8 = byteCast<char>(messageStorage[i].span());
        result.append(WGPUCompilationMessage {
            .nextInChain = nullptr,
            .message = { .data = utf8.data(), .length = utf8.size() },
            .type = toAPI(message.type),
            .lineNum = message.lineNum,
            .linePos = message.linePos - 1,
            .offset = message.offset,
            .length = message.length,
        });
    }
    return result;
}

// The null string view is the only entry point of the module for the stage. Any other one names
// an entry point even if it does not convert to a string.
inline String entryPointFromAPI(WGPUStringView entryPoint)
{
    if (!entryPoint.data && entryPoint.length == WGPU_STRLEN)
        return { };
    auto result = fromAPI(entryPoint);
    return result.isNull() ? emptyString() : result;
}

inline std::optional<WebGPU::ProgrammableStage> programmableStageFromAPI(WGPUShaderModule module, WGPUStringView entryPoint, std::span<const WGPUConstantEntry> constants, Vector<WebGPU::ConstantEntry>& constantsStorage LIFETIME_BOUND)
{
    if (!module)
        return std::nullopt;

    constantsStorage = WTF::map(constants, [](auto& constant) {
        return WebGPU::ConstantEntry { .key = fromAPI(constant.key), .value = constant.value };
    });
    return WebGPU::ProgrammableStage {
        .module = WebGPU::fromAPI(module),
        .entryPoint = entryPointFromAPI(entryPoint),
        .constants = constantsStorage.span(),
    };
}

struct ComputePipelineDescriptorStorage {
    Vector<WebGPU::ConstantEntry> constants;
};

inline std::optional<WebGPU::ComputePipelineDescriptor> fromAPI(const WGPUComputePipelineDescriptor& descriptor, ComputePipelineDescriptorStorage& storage LIFETIME_BOUND)
{
    auto compute = programmableStageFromAPI(descriptor.compute.module, descriptor.compute.entryPoint, unsafeMakeSpan(descriptor.compute.constants, descriptor.compute.constantCount), storage.constants);
    if (!compute)
        return std::nullopt;

    return WebGPU::ComputePipelineDescriptor {
        .label = fromAPI(descriptor.label),
        .layout = descriptor.layout ? RefPtr { &WebGPU::fromAPI(descriptor.layout) } : nullptr,
        .compute = WTF::move(*compute),
    };
}

inline std::optional<WebGPU::BlendComponent> fromAPI(const WGPUBlendComponent& component)
{
    auto operation = fromAPI(component.operation);
    // _Undefined is the default, which is One for the source and Zero for the destination.
    auto srcFactor = component.srcFactor == WGPUBlendFactor_Undefined ? std::optional { WebGPU::BlendFactor::One } : fromAPI(component.srcFactor);
    auto dstFactor = component.dstFactor == WGPUBlendFactor_Undefined ? std::optional { WebGPU::BlendFactor::Zero } : fromAPI(component.dstFactor);
    if (!operation || !srcFactor || !dstFactor)
        return std::nullopt;
    return WebGPU::BlendComponent { .operation = *operation, .srcFactor = *srcFactor, .dstFactor = *dstFactor };
}

// WGPUCompareFunction_Undefined in a stencil face state is WebGPU::CompareFunction::Always, the default.
inline std::optional<WebGPU::StencilFaceState> fromAPI(const WGPUStencilFaceState& state)
{
    auto compare = state.compare == WGPUCompareFunction_Undefined ? std::optional { WebGPU::CompareFunction::Always } : fromAPI(state.compare);
    auto failOp = fromAPI(state.failOp);
    auto depthFailOp = fromAPI(state.depthFailOp);
    auto passOp = fromAPI(state.passOp);
    if (!compare || !failOp || !depthFailOp || !passOp)
        return std::nullopt;
    return WebGPU::StencilFaceState { .compare = *compare, .failOp = *failOp, .depthFailOp = *depthFailOp, .passOp = *passOp };
}

inline std::optional<WebGPU::DepthStencilState> fromAPI(const WGPUDepthStencilState& state)
{
    auto format = fromAPI(state.format);
    auto stencilFront = fromAPI(state.stencilFront);
    auto stencilBack = fromAPI(state.stencilBack);
    if (!format || !stencilFront || !stencilBack)
        return std::nullopt;

    std::optional<bool> depthWriteEnabled;
    switch (state.depthWriteEnabled) {
    case WGPUOptionalBool_False:
        depthWriteEnabled = false;
        break;
    case WGPUOptionalBool_True:
        depthWriteEnabled = true;
        break;
    case WGPUOptionalBool_Undefined:
        break;
    default:
        return std::nullopt;
    }

    std::optional<WebGPU::CompareFunction> depthCompare;
    if (state.depthCompare != WGPUCompareFunction_Undefined) {
        depthCompare = fromAPI(state.depthCompare);
        if (!depthCompare)
            return std::nullopt;
    }

    return WebGPU::DepthStencilState {
        .format = *format,
        .depthWriteEnabled = depthWriteEnabled,
        .depthCompare = depthCompare,
        .stencilFront = *stencilFront,
        .stencilBack = *stencilBack,
        .stencilReadMask = state.stencilReadMask,
        .stencilWriteMask = state.stencilWriteMask,
        .depthBias = state.depthBias,
        .depthBiasSlopeScale = state.depthBiasSlopeScale,
        .depthBiasClamp = state.depthBiasClamp,
    };
}

struct RenderPipelineDescriptorStorage {
    Vector<WebGPU::ConstantEntry> vertexConstants;
    Vector<Vector<WebGPU::VertexAttribute>> vertexAttributes;
    Vector<std::optional<WebGPU::VertexBufferLayout>> vertexBuffers;
    Vector<WebGPU::ConstantEntry> fragmentConstants;
    Vector<std::optional<WebGPU::ColorTargetState>> fragmentTargets;
};

inline std::optional<WebGPU::RenderPipelineDescriptor> fromAPI(const WGPURenderPipelineDescriptor& descriptor, RenderPipelineDescriptorStorage& storage LIFETIME_BOUND)
{
    auto vertexStage = programmableStageFromAPI(descriptor.vertex.module, descriptor.vertex.entryPoint, unsafeMakeSpan(descriptor.vertex.constants, descriptor.vertex.constantCount), storage.vertexConstants);
    if (!vertexStage)
        return std::nullopt;

    // A vertex buffer layout with WGPU_COPY_STRIDE_UNDEFINED as its stride is an empty slot.
    auto buffers = unsafeMakeSpan(descriptor.vertex.buffers, descriptor.vertex.bufferCount);
    storage.vertexAttributes.clear();
    storage.vertexBuffers.clear();
    for (auto& buffer : buffers) {
        Vector<WebGPU::VertexAttribute> attributes;
        for (auto& attribute : unsafeMakeSpan(buffer.attributes, buffer.attributeCount)) {
            auto format = fromAPI(attribute.format);
            if (!format)
                return std::nullopt;
            attributes.append({ .format = *format, .offset = attribute.offset, .shaderLocation = attribute.shaderLocation });
        }
        storage.vertexAttributes.append(WTF::move(attributes));
    }
    for (auto [i, buffer] : indexedRange(buffers)) {
        // A slot is unused when its stride is WGPU_COPY_STRIDE_UNDEFINED, or its step mode is
        // _Undefined and it has no attributes.
        if (buffer.arrayStride == WGPU_COPY_STRIDE_UNDEFINED || (buffer.stepMode == WGPUVertexStepMode_Undefined && !buffer.attributeCount)) {
            storage.vertexBuffers.append(std::nullopt);
            continue;
        }
        auto stepMode = fromAPI(buffer.stepMode);
        if (!stepMode)
            return std::nullopt;
        storage.vertexBuffers.append(WebGPU::VertexBufferLayout {
            .arrayStride = buffer.arrayStride,
            .stepMode = *stepMode,
            .attributes = storage.vertexAttributes[i].span(),
        });
    }

    auto topology = fromAPI(descriptor.primitive.topology);
    auto frontFace = fromAPI(descriptor.primitive.frontFace);
    auto cullMode = fromAPI(descriptor.primitive.cullMode);
    if (!topology || !frontFace || !cullMode)
        return std::nullopt;
    std::optional<WebGPU::IndexFormat> stripIndexFormat;
    if (descriptor.primitive.stripIndexFormat != WGPUIndexFormat_Undefined) {
        stripIndexFormat = fromAPI(descriptor.primitive.stripIndexFormat);
        if (!stripIndexFormat)
            return std::nullopt;
    }

    std::optional<WebGPU::DepthStencilState> depthStencil;
    if (descriptor.depthStencil) {
        depthStencil = fromAPI(*descriptor.depthStencil);
        if (!depthStencil)
            return std::nullopt;
    }

    std::optional<WebGPU::FragmentState> fragment;
    if (auto* fragmentState = descriptor.fragment) {
        auto fragmentStage = programmableStageFromAPI(fragmentState->module, fragmentState->entryPoint, unsafeMakeSpan(fragmentState->constants, fragmentState->constantCount), storage.fragmentConstants);
        if (!fragmentStage)
            return std::nullopt;

        // A color target with WGPUTextureFormat_Undefined as its format is an empty slot.
        storage.fragmentTargets.clear();
        for (auto& target : unsafeMakeSpan(fragmentState->targets, fragmentState->targetCount)) {
            if (target.format == WGPUTextureFormat_Undefined) {
                storage.fragmentTargets.append(std::nullopt);
                continue;
            }
            auto format = fromAPI(target.format);
            auto writeMask = colorWriteFromAPI(target.writeMask);
            if (!format || !writeMask)
                return std::nullopt;
            std::optional<WebGPU::BlendState> blend;
            if (target.blend) {
                auto color = fromAPI(target.blend->color);
                auto alpha = fromAPI(target.blend->alpha);
                if (!color || !alpha)
                    return std::nullopt;
                blend = WebGPU::BlendState { .color = *color, .alpha = *alpha };
            }
            storage.fragmentTargets.append(WebGPU::ColorTargetState { .format = *format, .blend = blend, .writeMask = *writeMask });
        }
        fragment = WebGPU::FragmentState {
            .stage = WTF::move(*fragmentStage),
            .targets = storage.fragmentTargets.span(),
        };
    }

    return WebGPU::RenderPipelineDescriptor {
        .label = fromAPI(descriptor.label),
        .layout = descriptor.layout ? RefPtr { &WebGPU::fromAPI(descriptor.layout) } : nullptr,
        .vertex = {
            .stage = WTF::move(*vertexStage),
            .buffers = storage.vertexBuffers.span(),
        },
        .primitive = {
            .topology = *topology,
            .stripIndexFormat = stripIndexFormat,
            .frontFace = *frontFace,
            .cullMode = *cullMode,
            .unclippedDepth = !!descriptor.primitive.unclippedDepth,
        },
        .depthStencil = WTF::move(depthStencil),
        .multisample = {
            .count = descriptor.multisample.count,
            .mask = descriptor.multisample.mask,
            .alphaToCoverageEnabled = !!descriptor.multisample.alphaToCoverageEnabled,
        },
        .fragment = WTF::move(fragment),
    };
}

// std::nullopt is WGPUStoreOp_Undefined.
constexpr WGPUStoreOp toAPI(std::optional<WebGPU::StoreOp> value)
{
    return value ? toAPI(*value) : WGPUStoreOp_Undefined;
}

constexpr WebGPU::Color fromAPI(const WGPUColor& color)
{
    return { .r = color.r, .g = color.g, .b = color.b, .a = color.a };
}

// WGPU_QUERY_SET_INDEX_UNDEFINED is std::nullopt.
inline std::optional<WebGPU::PassTimestampWrites> fromAPI(const WGPUPassTimestampWrites& timestampWrites)
{
    if (!timestampWrites.querySet)
        return std::nullopt;
    auto writeIndex = [](uint32_t value) {
        return value == WGPU_QUERY_SET_INDEX_UNDEFINED ? std::nullopt : std::optional { value };
    };
    return WebGPU::PassTimestampWrites {
        .querySet = WebGPU::fromAPI(timestampWrites.querySet),
        .beginningOfPassWriteIndex = writeIndex(timestampWrites.beginningOfPassWriteIndex),
        .endOfPassWriteIndex = writeIndex(timestampWrites.endOfPassWriteIndex),
    };
}

inline std::optional<WebGPU::ComputePassDescriptor> fromAPI(const WGPUComputePassDescriptor& descriptor)
{
    std::optional<WebGPU::PassTimestampWrites> timestampWrites;
    if (auto* wgpuTimestampWrites = descriptor.timestampWrites) {
        timestampWrites = fromAPI(*wgpuTimestampWrites);
        if (!timestampWrites)
            return std::nullopt;
    }
    return WebGPU::ComputePassDescriptor {
        .label = fromAPI(descriptor.label),
        .timestampWrites = WTF::move(timestampWrites),
    };
}

// No view is std::nullopt.
inline std::optional<WebGPU::RenderPassAttachmentView> renderPassAttachmentViewFromAPI(WGPUTextureView view)
{
    if (view)
        return WebGPU::RenderPassAttachmentView { Ref { WebGPU::fromAPI(view) } };
    return std::nullopt;
}

// _Undefined is std::nullopt.
template<typename T>
inline std::optional<std::optional<T>> optionalOpFromAPI(auto value, auto undefinedValue)
{
    if (value == undefinedValue)
        return std::optional<T> { };
    auto result = fromAPI(value);
    if (!result)
        return std::nullopt;
    return std::optional<T> { *result };
}

struct RenderPassDescriptorStorage {
    Vector<std::optional<WebGPU::RenderPassColorAttachment>> colorAttachments;
};

inline std::optional<WebGPU::RenderPassDescriptor> fromAPI(const WGPURenderPassDescriptor& descriptor, RenderPassDescriptorStorage& storage LIFETIME_BOUND)
{
    // A color attachment without a view is an empty slot.
    storage.colorAttachments.clear();
    for (auto& attachment : unsafeMakeSpan(descriptor.colorAttachments, descriptor.colorAttachmentCount)) {
        auto view = renderPassAttachmentViewFromAPI(attachment.view);
        if (!view) {
            storage.colorAttachments.append(std::nullopt);
            continue;
        }
        auto loadOp = fromAPI(attachment.loadOp);
        auto storeOp = fromAPI(attachment.storeOp);
        if (!loadOp || !storeOp)
            return std::nullopt;
        storage.colorAttachments.append(WebGPU::RenderPassColorAttachment {
            .view = WTF::move(*view),
            .depthSlice = attachment.depthSlice == WGPU_DEPTH_SLICE_UNDEFINED ? std::nullopt : std::optional { attachment.depthSlice },
            .resolveTarget = renderPassAttachmentViewFromAPI(attachment.resolveTarget),
            .clearValue = fromAPI(attachment.clearValue),
            .loadOp = *loadOp,
            .storeOp = *storeOp,
        });
    }

    std::optional<WebGPU::RenderPassDepthStencilAttachment> depthStencilAttachment;
    if (auto* attachment = descriptor.depthStencilAttachment) {
        auto view = renderPassAttachmentViewFromAPI(attachment->view);
        auto depthLoadOp = optionalOpFromAPI<WebGPU::LoadOp>(attachment->depthLoadOp, WGPULoadOp_Undefined);
        auto depthStoreOp = optionalOpFromAPI<WebGPU::StoreOp>(attachment->depthStoreOp, WGPUStoreOp_Undefined);
        auto stencilLoadOp = optionalOpFromAPI<WebGPU::LoadOp>(attachment->stencilLoadOp, WGPULoadOp_Undefined);
        auto stencilStoreOp = optionalOpFromAPI<WebGPU::StoreOp>(attachment->stencilStoreOp, WGPUStoreOp_Undefined);
        if (!view || !depthLoadOp || !depthStoreOp || !stencilLoadOp || !stencilStoreOp)
            return std::nullopt;
        depthStencilAttachment = WebGPU::RenderPassDepthStencilAttachment {
            .view = WTF::move(*view),
            .depthClearValue = attachment->depthClearValue,
            .depthLoadOp = *depthLoadOp,
            .depthStoreOp = *depthStoreOp,
            .depthReadOnly = !!attachment->depthReadOnly,
            .stencilClearValue = attachment->stencilClearValue,
            .stencilLoadOp = *stencilLoadOp,
            .stencilStoreOp = *stencilStoreOp,
            .stencilReadOnly = !!attachment->stencilReadOnly,
        };
    }

    std::optional<WebGPU::PassTimestampWrites> timestampWrites;
    if (auto* wgpuTimestampWrites = descriptor.timestampWrites) {
        timestampWrites = fromAPI(*wgpuTimestampWrites);
        if (!timestampWrites)
            return std::nullopt;
    }

    auto* maxDrawCount = findChainedStruct<WGPURenderPassMaxDrawCount>(descriptor.nextInChain);
    return WebGPU::RenderPassDescriptor {
        .label = fromAPI(descriptor.label),
        .colorAttachments = storage.colorAttachments.span(),
        .depthStencilAttachment = WTF::move(depthStencilAttachment),
        .occlusionQuerySet = descriptor.occlusionQuerySet ? RefPtr { &WebGPU::fromAPI(descriptor.occlusionQuerySet) } : nullptr,
        .timestampWrites = WTF::move(timestampWrites),
        .maxDrawCount = maxDrawCount ? std::optional { maxDrawCount->maxDrawCount } : std::nullopt,
    };
}

struct RenderBundleEncoderDescriptorStorage {
    Vector<std::optional<WebGPU::TextureFormat>> colorFormats;
};

// WGPUTextureFormat_Undefined is std::nullopt: an empty color format slot, or no depth stencil format.
inline std::optional<WebGPU::RenderBundleEncoderDescriptor> fromAPI(const WGPURenderBundleEncoderDescriptor& descriptor, RenderBundleEncoderDescriptorStorage& storage LIFETIME_BOUND)
{
    auto optionalFormat = [](WGPUTextureFormat format) -> std::optional<std::optional<WebGPU::TextureFormat>> {
        if (format == WGPUTextureFormat_Undefined)
            return std::optional<WebGPU::TextureFormat> { };
        auto result = fromAPI(format);
        if (!result)
            return std::nullopt;
        return std::optional<WebGPU::TextureFormat> { *result };
    };

    storage.colorFormats.clear();
    for (auto format : unsafeMakeSpan(descriptor.colorFormats, descriptor.colorFormatCount)) {
        auto colorFormat = optionalFormat(format);
        if (!colorFormat)
            return std::nullopt;
        storage.colorFormats.append(*colorFormat);
    }
    auto depthStencilFormat = optionalFormat(descriptor.depthStencilFormat);
    if (!depthStencilFormat)
        return std::nullopt;

    return WebGPU::RenderBundleEncoderDescriptor {
        .label = fromAPI(descriptor.label),
        .colorFormats = storage.colorFormats.span(),
        .depthStencilFormat = *depthStencilFormat,
        .sampleCount = descriptor.sampleCount,
        .depthReadOnly = !!descriptor.depthReadOnly,
        .stencilReadOnly = !!descriptor.stencilReadOnly,
    };
}

constexpr WebGPU::Limits fromAPI(const WGPULimits& limits)
{
    return {
        .maxTextureDimension1D = limits.maxTextureDimension1D,
        .maxTextureDimension2D = limits.maxTextureDimension2D,
        .maxTextureDimension3D = limits.maxTextureDimension3D,
        .maxTextureArrayLayers = limits.maxTextureArrayLayers,
        .maxBindGroups = limits.maxBindGroups,
        .maxBindGroupsPlusVertexBuffers = limits.maxBindGroupsPlusVertexBuffers,
        .maxBindingsPerBindGroup = limits.maxBindingsPerBindGroup,
        .maxDynamicUniformBuffersPerPipelineLayout = limits.maxDynamicUniformBuffersPerPipelineLayout,
        .maxDynamicStorageBuffersPerPipelineLayout = limits.maxDynamicStorageBuffersPerPipelineLayout,
        .maxSampledTexturesPerShaderStage = limits.maxSampledTexturesPerShaderStage,
        .maxSamplersPerShaderStage = limits.maxSamplersPerShaderStage,
        .maxStorageBuffersPerShaderStage = limits.maxStorageBuffersPerShaderStage,
        .maxStorageTexturesPerShaderStage = limits.maxStorageTexturesPerShaderStage,
        .maxUniformBuffersPerShaderStage = limits.maxUniformBuffersPerShaderStage,
        .maxUniformBufferBindingSize = limits.maxUniformBufferBindingSize,
        .maxStorageBufferBindingSize = limits.maxStorageBufferBindingSize,
        .minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment,
        .minStorageBufferOffsetAlignment = limits.minStorageBufferOffsetAlignment,
        .maxVertexBuffers = limits.maxVertexBuffers,
        .maxBufferSize = limits.maxBufferSize,
        .maxVertexAttributes = limits.maxVertexAttributes,
        .maxVertexBufferArrayStride = limits.maxVertexBufferArrayStride,
        .maxInterStageShaderVariables = limits.maxInterStageShaderVariables,
        .maxColorAttachments = limits.maxColorAttachments,
        .maxColorAttachmentBytesPerSample = limits.maxColorAttachmentBytesPerSample,
        .maxComputeWorkgroupStorageSize = limits.maxComputeWorkgroupStorageSize,
        .maxComputeInvocationsPerWorkgroup = limits.maxComputeInvocationsPerWorkgroup,
        .maxComputeWorkgroupSizeX = limits.maxComputeWorkgroupSizeX,
        .maxComputeWorkgroupSizeY = limits.maxComputeWorkgroupSizeY,
        .maxComputeWorkgroupSizeZ = limits.maxComputeWorkgroupSizeZ,
        .maxComputeWorkgroupsPerDimension = limits.maxComputeWorkgroupsPerDimension,
        // The C API has no per-stage storage limits. They default to the per-shader-stage ones.
        .maxStorageBuffersInFragmentStage = limits.maxStorageBuffersPerShaderStage,
        .maxStorageTexturesInFragmentStage = limits.maxStorageTexturesPerShaderStage,
        .maxStorageBuffersInVertexStage = limits.maxStorageBuffersPerShaderStage,
        .maxStorageTexturesInVertexStage = limits.maxStorageTexturesPerShaderStage,
    };
}

constexpr WGPULimits toAPI(const WebGPU::Limits& limits)
{
    return {
        .nextInChain = nullptr,
        .maxTextureDimension1D = limits.maxTextureDimension1D,
        .maxTextureDimension2D = limits.maxTextureDimension2D,
        .maxTextureDimension3D = limits.maxTextureDimension3D,
        .maxTextureArrayLayers = limits.maxTextureArrayLayers,
        .maxBindGroups = limits.maxBindGroups,
        .maxBindGroupsPlusVertexBuffers = limits.maxBindGroupsPlusVertexBuffers,
        .maxBindingsPerBindGroup = limits.maxBindingsPerBindGroup,
        .maxDynamicUniformBuffersPerPipelineLayout = limits.maxDynamicUniformBuffersPerPipelineLayout,
        .maxDynamicStorageBuffersPerPipelineLayout = limits.maxDynamicStorageBuffersPerPipelineLayout,
        .maxSampledTexturesPerShaderStage = limits.maxSampledTexturesPerShaderStage,
        .maxSamplersPerShaderStage = limits.maxSamplersPerShaderStage,
        .maxStorageBuffersPerShaderStage = limits.maxStorageBuffersPerShaderStage,
        .maxStorageTexturesPerShaderStage = limits.maxStorageTexturesPerShaderStage,
        .maxUniformBuffersPerShaderStage = limits.maxUniformBuffersPerShaderStage,
        .maxUniformBufferBindingSize = limits.maxUniformBufferBindingSize,
        .maxStorageBufferBindingSize = limits.maxStorageBufferBindingSize,
        .minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment,
        .minStorageBufferOffsetAlignment = limits.minStorageBufferOffsetAlignment,
        .maxVertexBuffers = limits.maxVertexBuffers,
        .maxBufferSize = limits.maxBufferSize,
        .maxVertexAttributes = limits.maxVertexAttributes,
        .maxVertexBufferArrayStride = limits.maxVertexBufferArrayStride,
        .maxInterStageShaderVariables = limits.maxInterStageShaderVariables,
        .maxColorAttachments = limits.maxColorAttachments,
        .maxColorAttachmentBytesPerSample = limits.maxColorAttachmentBytesPerSample,
        .maxComputeWorkgroupStorageSize = limits.maxComputeWorkgroupStorageSize,
        .maxComputeInvocationsPerWorkgroup = limits.maxComputeInvocationsPerWorkgroup,
        .maxComputeWorkgroupSizeX = limits.maxComputeWorkgroupSizeX,
        .maxComputeWorkgroupSizeY = limits.maxComputeWorkgroupSizeY,
        .maxComputeWorkgroupSizeZ = limits.maxComputeWorkgroupSizeZ,
        .maxComputeWorkgroupsPerDimension = limits.maxComputeWorkgroupsPerDimension,
        // Immediates are not implemented.
        .maxImmediateSize = 0,
    };
}

// WGPUPowerPreference_Undefined is std::nullopt.
inline std::optional<WebGPU::RequestAdapterOptions> fromAPI(const WGPURequestAdapterOptions& options)
{
    std::optional<WebGPU::PowerPreference> powerPreference;
    if (options.powerPreference != WGPUPowerPreference_Undefined) {
        powerPreference = fromAPI(options.powerPreference);
        if (!powerPreference)
            return std::nullopt;
    }
    return WebGPU::RequestAdapterOptions {
        .powerPreference = powerPreference,
        .forceFallbackAdapter = !!options.forceFallbackAdapter,
        .xrCompatible = false,
    };
}

struct DeviceDescriptorStorage {
    Vector<WebGPU::FeatureName> requiredFeatures;
};

inline std::optional<WebGPU::DeviceDescriptor> fromAPI(const WGPUDeviceDescriptor& descriptor, DeviceDescriptorStorage& storage LIFETIME_BOUND)
{
    storage.requiredFeatures.clear();
    for (auto feature : unsafeMakeSpan(descriptor.requiredFeatures, descriptor.requiredFeatureCount)) {
        auto apiFeature = fromAPI(feature);
        if (!apiFeature)
            return std::nullopt;
        storage.requiredFeatures.append(*apiFeature);
    }
    return WebGPU::DeviceDescriptor {
        .label = fromAPI(descriptor.label),
        .requiredFeatures = storage.requiredFeatures.span(),
        .requiredLimits = descriptor.requiredLimits ? std::optional { fromAPI(*descriptor.requiredLimits) } : std::nullopt,
    };
}

// The features of the C API that have a C++ API equivalent.
inline Vector<WebGPU::FeatureName> featuresFromAPI(std::span<const WGPUFeatureName> features)
{
    Vector<WebGPU::FeatureName> result;
    for (auto feature : features) {
        if (auto apiFeature = fromAPI(feature))
            result.append(*apiFeature);
    }
    return result;
}

inline WGPUErrorType toAPI(const std::optional<WebGPU::Error>& error)
{
    if (!error)
        return WGPUErrorType_NoError;
    switch (error->type) {
    case WebGPU::ErrorType::Validation:
        return WGPUErrorType_Validation;
    case WebGPU::ErrorType::OutOfMemory:
        return WGPUErrorType_OutOfMemory;
    case WebGPU::ErrorType::Internal:
        return WGPUErrorType_Internal;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebGPU::Metal

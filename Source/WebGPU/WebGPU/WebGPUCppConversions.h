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

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <WebGPU/WebGPUExt.h>
#import <optional>
#import <wtf/OptionSet.h>

// Conversions between the enums and flags of the WebGPU C API (webgpu.h) and the WebGPU C++ API.
// fromAPI() returns std::nullopt for C API values that have no C++ API equivalent, such as the
// _Undefined values and unknown bits.

namespace WebGPU::Metal {

constexpr std::optional<WebGPU::AddressMode> fromAPI(WGPUAddressMode value)
{
    switch (value) {
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

constexpr std::optional<WebGPU::CanvasAlphaMode> fromAPI(WGPUCompositeAlphaMode value)
{
    switch (value) {
    case WGPUCompositeAlphaMode_Opaque:
        return WebGPU::CanvasAlphaMode::Opaque;
    case WGPUCompositeAlphaMode_Premultiplied:
        return WebGPU::CanvasAlphaMode::Premultiplied;
    default:
        return std::nullopt;
    }
}

constexpr WGPUCompositeAlphaMode toAPI(WebGPU::CanvasAlphaMode value)
{
    switch (value) {
    case WebGPU::CanvasAlphaMode::Opaque:
        return WGPUCompositeAlphaMode_Opaque;
    case WebGPU::CanvasAlphaMode::Premultiplied:
        return WGPUCompositeAlphaMode_Premultiplied;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::CanvasToneMappingMode> fromAPI(WGPUToneMappingMode value)
{
    switch (value) {
    case WGPUToneMappingMode_Standard:
        return WebGPU::CanvasToneMappingMode::Standard;
    case WGPUToneMappingMode_Extended:
        return WebGPU::CanvasToneMappingMode::Extended;
    default:
        return std::nullopt;
    }
}

constexpr WGPUToneMappingMode toAPI(WebGPU::CanvasToneMappingMode value)
{
    switch (value) {
    case WebGPU::CanvasToneMappingMode::Standard:
        return WGPUToneMappingMode_Standard;
    case WebGPU::CanvasToneMappingMode::Extended:
        return WGPUToneMappingMode_Extended;
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
    case WGPUDeviceLostReason_Undefined:
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
        return WGPUDeviceLostReason_Undefined;
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
    case WGPUFeatureName_Float16Renderable:
        return WebGPU::FeatureName::Float16Renderable;
    case WGPUFeatureName_Float32Renderable:
        return WebGPU::FeatureName::Float32Renderable;
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

constexpr WGPUFeatureName toAPI(WebGPU::FeatureName value)
{
    switch (value) {
    case WebGPU::FeatureName::DepthClipControl:
        return WGPUFeatureName_DepthClipControl;
    case WebGPU::FeatureName::Depth32floatStencil8:
        return WGPUFeatureName_Depth32FloatStencil8;
    case WebGPU::FeatureName::TextureCompressionBc:
        return WGPUFeatureName_TextureCompressionBC;
    case WebGPU::FeatureName::TextureCompressionBcSliced3d:
        return WGPUFeatureName_TextureCompressionBCSliced3D;
    case WebGPU::FeatureName::TextureCompressionEtc2:
        return WGPUFeatureName_TextureCompressionETC2;
    case WebGPU::FeatureName::TextureCompressionAstc:
        return WGPUFeatureName_TextureCompressionASTC;
    case WebGPU::FeatureName::TextureCompressionAstcSliced3d:
        return WGPUFeatureName_TextureCompressionASTCSliced3D;
    case WebGPU::FeatureName::TimestampQuery:
        return WGPUFeatureName_TimestampQuery;
    case WebGPU::FeatureName::IndirectFirstInstance:
        return WGPUFeatureName_IndirectFirstInstance;
    case WebGPU::FeatureName::Bgra8unormStorage:
        return WGPUFeatureName_BGRA8UnormStorage;
    case WebGPU::FeatureName::ShaderF16:
        return WGPUFeatureName_ShaderF16;
    case WebGPU::FeatureName::Rg11b10ufloatRenderable:
        return WGPUFeatureName_RG11B10UfloatRenderable;
    case WebGPU::FeatureName::Float32Filterable:
        return WGPUFeatureName_Float32Filterable;
    case WebGPU::FeatureName::Float16Renderable:
        return WGPUFeatureName_Float16Renderable;
    case WebGPU::FeatureName::Float32Renderable:
        return WGPUFeatureName_Float32Renderable;
    case WebGPU::FeatureName::Float32Blendable:
        return WGPUFeatureName_Float32Blendable;
    case WebGPU::FeatureName::ClipDistances:
        return WGPUFeatureName_ClipDistances;
    case WebGPU::FeatureName::DualSourceBlending:
        return WGPUFeatureName_DualSourceBlending;
    case WebGPU::FeatureName::CoreFeaturesAndLimits:
        return WGPUFeatureName_CoreFeaturesAndLimits;
    case WebGPU::FeatureName::TextureFormatsTier1:
        return WGPUFeatureName_TextureFormatsTier1;
    case WebGPU::FeatureName::TextureFormatsTier2:
        return WGPUFeatureName_TextureFormatsTier2;
    case WebGPU::FeatureName::PrimitiveIndex:
        return WGPUFeatureName_PrimitiveIndex;
    case WebGPU::FeatureName::Subgroups:
        return WGPUFeatureName_Subgroups;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::FilterMode> fromAPI(WGPUFilterMode value)
{
    switch (value) {
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
    case WGPUVertexFormat_Snorm1010102:
        return WebGPU::VertexFormat::Snorm1010102;
    case WGPUVertexFormat_Unorm1010102:
        return WebGPU::VertexFormat::Unorm1010102;
    case WGPUVertexFormat_Unorm8x4Bgra:
        return WebGPU::VertexFormat::Unorm8x4Bgra;
    default:
        return std::nullopt;
    }
}

constexpr WGPUVertexFormat toAPI(WebGPU::VertexFormat value)
{
    switch (value) {
    case WebGPU::VertexFormat::Uint8:
        return WGPUVertexFormat_Uint8;
    case WebGPU::VertexFormat::Uint8x2:
        return WGPUVertexFormat_Uint8x2;
    case WebGPU::VertexFormat::Uint8x4:
        return WGPUVertexFormat_Uint8x4;
    case WebGPU::VertexFormat::Sint8:
        return WGPUVertexFormat_Sint8;
    case WebGPU::VertexFormat::Sint8x2:
        return WGPUVertexFormat_Sint8x2;
    case WebGPU::VertexFormat::Sint8x4:
        return WGPUVertexFormat_Sint8x4;
    case WebGPU::VertexFormat::Unorm8:
        return WGPUVertexFormat_Unorm8;
    case WebGPU::VertexFormat::Unorm8x2:
        return WGPUVertexFormat_Unorm8x2;
    case WebGPU::VertexFormat::Unorm8x4:
        return WGPUVertexFormat_Unorm8x4;
    case WebGPU::VertexFormat::Snorm8:
        return WGPUVertexFormat_Snorm8;
    case WebGPU::VertexFormat::Snorm8x2:
        return WGPUVertexFormat_Snorm8x2;
    case WebGPU::VertexFormat::Snorm8x4:
        return WGPUVertexFormat_Snorm8x4;
    case WebGPU::VertexFormat::Uint16:
        return WGPUVertexFormat_Uint16;
    case WebGPU::VertexFormat::Uint16x2:
        return WGPUVertexFormat_Uint16x2;
    case WebGPU::VertexFormat::Uint16x4:
        return WGPUVertexFormat_Uint16x4;
    case WebGPU::VertexFormat::Sint16:
        return WGPUVertexFormat_Sint16;
    case WebGPU::VertexFormat::Sint16x2:
        return WGPUVertexFormat_Sint16x2;
    case WebGPU::VertexFormat::Sint16x4:
        return WGPUVertexFormat_Sint16x4;
    case WebGPU::VertexFormat::Unorm16:
        return WGPUVertexFormat_Unorm16;
    case WebGPU::VertexFormat::Unorm16x2:
        return WGPUVertexFormat_Unorm16x2;
    case WebGPU::VertexFormat::Unorm16x4:
        return WGPUVertexFormat_Unorm16x4;
    case WebGPU::VertexFormat::Snorm16:
        return WGPUVertexFormat_Snorm16;
    case WebGPU::VertexFormat::Snorm16x2:
        return WGPUVertexFormat_Snorm16x2;
    case WebGPU::VertexFormat::Snorm16x4:
        return WGPUVertexFormat_Snorm16x4;
    case WebGPU::VertexFormat::Float16:
        return WGPUVertexFormat_Float16;
    case WebGPU::VertexFormat::Float16x2:
        return WGPUVertexFormat_Float16x2;
    case WebGPU::VertexFormat::Float16x4:
        return WGPUVertexFormat_Float16x4;
    case WebGPU::VertexFormat::Float32:
        return WGPUVertexFormat_Float32;
    case WebGPU::VertexFormat::Float32x2:
        return WGPUVertexFormat_Float32x2;
    case WebGPU::VertexFormat::Float32x3:
        return WGPUVertexFormat_Float32x3;
    case WebGPU::VertexFormat::Float32x4:
        return WGPUVertexFormat_Float32x4;
    case WebGPU::VertexFormat::Uint32:
        return WGPUVertexFormat_Uint32;
    case WebGPU::VertexFormat::Uint32x2:
        return WGPUVertexFormat_Uint32x2;
    case WebGPU::VertexFormat::Uint32x3:
        return WGPUVertexFormat_Uint32x3;
    case WebGPU::VertexFormat::Uint32x4:
        return WGPUVertexFormat_Uint32x4;
    case WebGPU::VertexFormat::Sint32:
        return WGPUVertexFormat_Sint32;
    case WebGPU::VertexFormat::Sint32x2:
        return WGPUVertexFormat_Sint32x2;
    case WebGPU::VertexFormat::Sint32x3:
        return WGPUVertexFormat_Sint32x3;
    case WebGPU::VertexFormat::Sint32x4:
        return WGPUVertexFormat_Sint32x4;
    case WebGPU::VertexFormat::Snorm1010102:
        return WGPUVertexFormat_Snorm1010102;
    case WebGPU::VertexFormat::Unorm1010102:
        return WGPUVertexFormat_Unorm1010102;
    case WebGPU::VertexFormat::Unorm8x4Bgra:
        return WGPUVertexFormat_Unorm8x4Bgra;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr std::optional<WebGPU::VertexStepMode> fromAPI(WGPUVertexStepMode value)
{
    switch (value) {
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
    if (value & WGPUTextureUsage_Transient) {
        result.add(WebGPU::TextureUsage::Transient);
        value &= ~WGPUTextureUsage_Transient;
    }
    if (value & WGPUTextureUsage_Invalid) {
        result.add(WebGPU::TextureUsage::Invalid);
        value &= ~WGPUTextureUsage_Invalid;
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
        result |= WGPUTextureUsage_Transient;
    if (value.contains(WebGPU::TextureUsage::Invalid))
        result |= WGPUTextureUsage_Invalid;
    return result;
}

} // namespace WebGPU::Metal

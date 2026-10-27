/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RenderPipeline.h"

#import "APIConversions.h"
#import "BindGroupLayout.h"
#import "Device.h"
#import "Instance.h"
#import "IsValidToUseWith.h"
#import "Pipeline.h"
#import "RenderBundleEncoder.h"
#import "TextureOrTextureView.h"
#import "WGSLShaderModule.h"
#import <wtf/IndexedRange.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMallocInlines.h>

// FIXME: remove after radar://104903411 or after we place the mask into the last buffer
@interface NSObject ()
- (void)setSampleMask:(NSUInteger)mask;
@end

namespace WebGPU::Metal {

static MTLBlendOperation LEGACY_NODELETE blendOperation(WebGPU::BlendOperation operation)
{
    switch (operation) {
    case WebGPU::BlendOperation::Add:
        return MTLBlendOperationAdd;
    case WebGPU::BlendOperation::Max:
        return MTLBlendOperationMax;
    case WebGPU::BlendOperation::Min:
        return MTLBlendOperationMin;
    case WebGPU::BlendOperation::ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    case WebGPU::BlendOperation::Subtract:
        return MTLBlendOperationSubtract;
    }
}

static MTLBlendFactor LEGACY_NODELETE blendFactor(WebGPU::BlendFactor factor)
{
    switch (factor) {
    case WebGPU::BlendFactor::Constant:
        return MTLBlendFactorBlendColor;
    case WebGPU::BlendFactor::Dst:
        return MTLBlendFactorDestinationColor;
    case WebGPU::BlendFactor::DstAlpha:
        return MTLBlendFactorDestinationAlpha;
    case WebGPU::BlendFactor::One:
        return MTLBlendFactorOne;
    case WebGPU::BlendFactor::OneMinusConstant:
        return MTLBlendFactorOneMinusBlendColor;
    case WebGPU::BlendFactor::OneMinusDst:
        return MTLBlendFactorOneMinusDestinationColor;
    case WebGPU::BlendFactor::OneMinusDstAlpha:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case WebGPU::BlendFactor::OneMinusSrc:
        return MTLBlendFactorOneMinusSourceColor;
    case WebGPU::BlendFactor::Src:
        return MTLBlendFactorSourceColor;
    case WebGPU::BlendFactor::OneMinusSrcAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    case WebGPU::BlendFactor::Zero:
        return MTLBlendFactorZero;
    case WebGPU::BlendFactor::SrcAlpha:
        return MTLBlendFactorSourceAlpha;
    case WebGPU::BlendFactor::SrcAlphaSaturated:
        return MTLBlendFactorSourceAlphaSaturated;
    }
}

static MTLColorWriteMask LEGACY_NODELETE colorWriteMask(OptionSet<WebGPU::ColorWrite> mask)
{
    MTLColorWriteMask mtlMask = MTLColorWriteMaskNone;

    if (mask.contains(WebGPU::ColorWrite::Red))
        mtlMask |= MTLColorWriteMaskRed;
    if (mask.contains(WebGPU::ColorWrite::Green))
        mtlMask |= MTLColorWriteMaskGreen;
    if (mask.contains(WebGPU::ColorWrite::Blue))
        mtlMask |= MTLColorWriteMaskBlue;
    if (mask.contains(WebGPU::ColorWrite::Alpha))
        mtlMask |= MTLColorWriteMaskAlpha;

    return mtlMask;
}

static MTLWinding LEGACY_NODELETE frontFace(WebGPU::FrontFace frontFace)
{
    switch (frontFace) {
    case WebGPU::FrontFace::CW:
        return MTLWindingClockwise;
    case WebGPU::FrontFace::CCW:
        return MTLWindingCounterClockwise;
    }
}

static MTLCullMode LEGACY_NODELETE cullMode(WebGPU::CullMode cullMode)
{
    switch (cullMode) {
    case WebGPU::CullMode::None:
        return MTLCullModeNone;
    case WebGPU::CullMode::Front:
        return MTLCullModeFront;
    case WebGPU::CullMode::Back:
        return MTLCullModeBack;
    }
}

static MTLPrimitiveType LEGACY_NODELETE primitiveType(WebGPU::PrimitiveTopology topology)
{
    switch (topology) {
    case WebGPU::PrimitiveTopology::PointList:
        return MTLPrimitiveTypePoint;
    case WebGPU::PrimitiveTopology::LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case WebGPU::PrimitiveTopology::TriangleList:
        return MTLPrimitiveTypeTriangle;
    case WebGPU::PrimitiveTopology::LineList:
        return MTLPrimitiveTypeLine;
    case WebGPU::PrimitiveTopology::TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    }
}

static MTLPrimitiveTopologyClass LEGACY_NODELETE topologyType(WebGPU::PrimitiveTopology topology)
{
    switch (topology) {
    case WebGPU::PrimitiveTopology::PointList:
        return MTLPrimitiveTopologyClassPoint;
    case WebGPU::PrimitiveTopology::LineStrip:
    case WebGPU::PrimitiveTopology::LineList:
        return MTLPrimitiveTopologyClassLine;
    case WebGPU::PrimitiveTopology::TriangleList:
    case WebGPU::PrimitiveTopology::TriangleStrip:
        return MTLPrimitiveTopologyClassTriangle;
    }
}

static std::optional<MTLIndexType> LEGACY_NODELETE indexType(WebGPU::IndexFormat format)
{
    switch (format) {
    case WebGPU::IndexFormat::Uint16:
        return MTLIndexTypeUInt16;
    case WebGPU::IndexFormat::Uint32:
        return MTLIndexTypeUInt32;
    }
}

bool Device::validateRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor)
{
    // FIXME: Implement this according to the description in
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpurenderpipelinedescriptor

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        if (fragmentDescriptor.targets.size() > limits().maxColorAttachments)
            return false;
    }

    return true;
}

static MTLStencilOperation LEGACY_NODELETE convertToMTLStencilOperation(WebGPU::StencilOperation operation)
{
    switch (operation) {
    case WebGPU::StencilOperation::Keep:
        return MTLStencilOperationKeep;
    case WebGPU::StencilOperation::Zero:
        return MTLStencilOperationZero;
    case WebGPU::StencilOperation::Replace:
        return MTLStencilOperationReplace;
    case WebGPU::StencilOperation::Invert:
        return MTLStencilOperationInvert;
    case WebGPU::StencilOperation::IncrementClamp:
        return MTLStencilOperationIncrementClamp;
    case WebGPU::StencilOperation::DecrementClamp:
        return MTLStencilOperationDecrementClamp;
    case WebGPU::StencilOperation::IncrementWrap:
        return MTLStencilOperationIncrementWrap;
    case WebGPU::StencilOperation::DecrementWrap:
        return MTLStencilOperationDecrementWrap;
    }
}

static MTLCompareFunction LEGACY_NODELETE convertToMTLCompare(WebGPU::CompareFunction comparison)
{
    switch (comparison) {
    case WebGPU::CompareFunction::Never:
        return MTLCompareFunctionNever;
    case WebGPU::CompareFunction::Less:
        return MTLCompareFunctionLess;
    case WebGPU::CompareFunction::LessEqual:
        return MTLCompareFunctionLessEqual;
    case WebGPU::CompareFunction::Greater:
        return MTLCompareFunctionGreater;
    case WebGPU::CompareFunction::GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case WebGPU::CompareFunction::Equal:
        return MTLCompareFunctionEqual;
    case WebGPU::CompareFunction::NotEqual:
        return MTLCompareFunctionNotEqual;
    case WebGPU::CompareFunction::Always:
        return MTLCompareFunctionAlways;
    }
}

static MTLVertexFormat LEGACY_NODELETE vertexFormat(WebGPU::VertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case WebGPU::VertexFormat::Uint8:
        return MTLVertexFormatUChar;
    case WebGPU::VertexFormat::Uint8x2:
        return MTLVertexFormatUChar2;
    case WebGPU::VertexFormat::Uint8x4:
        return MTLVertexFormatUChar4;
    case WebGPU::VertexFormat::Sint8:
        return MTLVertexFormatChar;
    case WebGPU::VertexFormat::Sint8x2:
        return MTLVertexFormatChar2;
    case WebGPU::VertexFormat::Sint8x4:
        return MTLVertexFormatChar4;
    case WebGPU::VertexFormat::Unorm8:
        return MTLVertexFormatUCharNormalized;
    case WebGPU::VertexFormat::Unorm8x2:
        return MTLVertexFormatUChar2Normalized;
    case WebGPU::VertexFormat::Unorm8x4:
        return MTLVertexFormatUChar4Normalized;
    case WebGPU::VertexFormat::Snorm8:
        return MTLVertexFormatCharNormalized;
    case WebGPU::VertexFormat::Snorm8x2:
        return MTLVertexFormatChar2Normalized;
    case WebGPU::VertexFormat::Snorm8x4:
        return MTLVertexFormatChar4Normalized;
    case WebGPU::VertexFormat::Uint16:
        return MTLVertexFormatUShort;
    case WebGPU::VertexFormat::Uint16x2:
        return MTLVertexFormatUShort2;
    case WebGPU::VertexFormat::Uint16x4:
        return MTLVertexFormatUShort4;
    case WebGPU::VertexFormat::Sint16:
        return MTLVertexFormatShort;
    case WebGPU::VertexFormat::Sint16x2:
        return MTLVertexFormatShort2;
    case WebGPU::VertexFormat::Sint16x4:
        return MTLVertexFormatShort4;
    case WebGPU::VertexFormat::Unorm16:
        return MTLVertexFormatUShortNormalized;
    case WebGPU::VertexFormat::Unorm16x2:
        return MTLVertexFormatUShort2Normalized;
    case WebGPU::VertexFormat::Unorm16x4:
        return MTLVertexFormatUShort4Normalized;
    case WebGPU::VertexFormat::Snorm16:
        return MTLVertexFormatShortNormalized;
    case WebGPU::VertexFormat::Snorm16x2:
        return MTLVertexFormatShort2Normalized;
    case WebGPU::VertexFormat::Snorm16x4:
        return MTLVertexFormatShort4Normalized;
    case WebGPU::VertexFormat::Float16:
        return MTLVertexFormatHalf;
    case WebGPU::VertexFormat::Float16x2:
        return MTLVertexFormatHalf2;
    case WebGPU::VertexFormat::Float16x4:
        return MTLVertexFormatHalf4;
    case WebGPU::VertexFormat::Float32:
        return MTLVertexFormatFloat;
    case WebGPU::VertexFormat::Float32x2:
        return MTLVertexFormatFloat2;
    case WebGPU::VertexFormat::Float32x3:
        return MTLVertexFormatFloat3;
    case WebGPU::VertexFormat::Float32x4:
        return MTLVertexFormatFloat4;
    case WebGPU::VertexFormat::Uint32:
        return MTLVertexFormatUInt;
    case WebGPU::VertexFormat::Uint32x2:
        return MTLVertexFormatUInt2;
    case WebGPU::VertexFormat::Uint32x3:
        return MTLVertexFormatUInt3;
    case WebGPU::VertexFormat::Uint32x4:
        return MTLVertexFormatUInt4;
    case WebGPU::VertexFormat::Sint32:
        return MTLVertexFormatInt;
    case WebGPU::VertexFormat::Sint32x2:
        return MTLVertexFormatInt2;
    case WebGPU::VertexFormat::Sint32x3:
        return MTLVertexFormatInt3;
    case WebGPU::VertexFormat::Sint32x4:
        return MTLVertexFormatInt4;
    case WebGPU::VertexFormat::Snorm1010102:
        return MTLVertexFormatInt1010102Normalized;
    case WebGPU::VertexFormat::Unorm1010102:
        return MTLVertexFormatUInt1010102Normalized;
    case WebGPU::VertexFormat::Unorm8x4Bgra:
        return MTLVertexFormatUChar4Normalized_BGRA;
    }
}

static size_t LEGACY_NODELETE vertexFormatSize(WebGPU::VertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case WebGPU::VertexFormat::Uint8:
        return 1;
    case WebGPU::VertexFormat::Uint8x2:
        return 2;
    case WebGPU::VertexFormat::Uint8x4:
        return 4;
    case WebGPU::VertexFormat::Sint8:
        return 1;
    case WebGPU::VertexFormat::Sint8x2:
        return 2;
    case WebGPU::VertexFormat::Sint8x4:
        return 4;
    case WebGPU::VertexFormat::Unorm8:
        return 1;
    case WebGPU::VertexFormat::Unorm8x2:
        return 2;
    case WebGPU::VertexFormat::Unorm8x4:
        return 4;
    case WebGPU::VertexFormat::Snorm8:
        return 1;
    case WebGPU::VertexFormat::Snorm8x2:
        return 2;
    case WebGPU::VertexFormat::Snorm8x4:
        return 4;
    case WebGPU::VertexFormat::Uint16:
        return 2;
    case WebGPU::VertexFormat::Uint16x2:
        return 4;
    case WebGPU::VertexFormat::Uint16x4:
        return 8;
    case WebGPU::VertexFormat::Sint16:
        return 2;
    case WebGPU::VertexFormat::Sint16x2:
        return 4;
    case WebGPU::VertexFormat::Sint16x4:
        return 8;
    case WebGPU::VertexFormat::Unorm16:
        return 2;
    case WebGPU::VertexFormat::Unorm16x2:
        return 4;
    case WebGPU::VertexFormat::Unorm16x4:
        return 8;
    case WebGPU::VertexFormat::Snorm16:
        return 2;
    case WebGPU::VertexFormat::Snorm16x2:
        return 4;
    case WebGPU::VertexFormat::Snorm16x4:
        return 8;
    case WebGPU::VertexFormat::Float16:
        return 2;
    case WebGPU::VertexFormat::Float16x2:
        return 4;
    case WebGPU::VertexFormat::Float16x4:
        return 8;
    case WebGPU::VertexFormat::Float32:
        return 4;
    case WebGPU::VertexFormat::Float32x2:
        return 8;
    case WebGPU::VertexFormat::Float32x3:
        return 12;
    case WebGPU::VertexFormat::Float32x4:
        return 16;
    case WebGPU::VertexFormat::Uint32:
        return 4;
    case WebGPU::VertexFormat::Uint32x2:
        return 8;
    case WebGPU::VertexFormat::Uint32x3:
        return 12;
    case WebGPU::VertexFormat::Uint32x4:
        return 16;
    case WebGPU::VertexFormat::Sint32:
        return 4;
    case WebGPU::VertexFormat::Sint32x2:
        return 8;
    case WebGPU::VertexFormat::Sint32x3:
        return 12;
    case WebGPU::VertexFormat::Sint32x4:
        return 16;
    case WebGPU::VertexFormat::Snorm1010102:
    case WebGPU::VertexFormat::Unorm1010102:
        return 4;
    case WebGPU::VertexFormat::Unorm8x4Bgra:
        return 4;
    }
}

static MTLVertexStepFunction LEGACY_NODELETE stepFunction(WebGPU::VertexStepMode stepMode, auto arrayStride)
{
    if (!arrayStride)
        return MTLVertexStepFunctionConstant;

    switch (stepMode) {
    case WebGPU::VertexStepMode::Vertex:
        return MTLVertexStepFunctionPerVertex;
    case WebGPU::VertexStepMode::Instance:
        return MTLVertexStepFunctionPerInstance;
    }
}

static ASCIILiteral name(WebGPU::VertexFormat format)
{
    switch (format) {
    case WebGPU::VertexFormat::Uint8:
        return "UChar"_s;
    case WebGPU::VertexFormat::Uint8x2:
        return "UChar2"_s;
    case WebGPU::VertexFormat::Uint8x4:
        return "UChar4"_s;
    case WebGPU::VertexFormat::Sint8:
        return "Char"_s;
    case WebGPU::VertexFormat::Sint8x2:
        return "Char2"_s;
    case WebGPU::VertexFormat::Sint8x4:
        return "Char4"_s;
    case WebGPU::VertexFormat::Unorm8:
        return "UCharNormalized"_s;
    case WebGPU::VertexFormat::Unorm8x2:
        return "UChar2Normalized"_s;
    case WebGPU::VertexFormat::Unorm8x4:
        return "UChar4Normalized"_s;
    case WebGPU::VertexFormat::Snorm8:
        return "CharNormalized"_s;
    case WebGPU::VertexFormat::Snorm8x2:
        return "Char2Normalized"_s;
    case WebGPU::VertexFormat::Snorm8x4:
        return "Char4Normalized"_s;
    case WebGPU::VertexFormat::Uint16:
        return "UShort"_s;
    case WebGPU::VertexFormat::Uint16x2:
        return "UShort2"_s;
    case WebGPU::VertexFormat::Uint16x4:
        return "UShort4"_s;
    case WebGPU::VertexFormat::Sint16:
        return "Short"_s;
    case WebGPU::VertexFormat::Sint16x2:
        return "Short2"_s;
    case WebGPU::VertexFormat::Sint16x4:
        return "Short4"_s;
    case WebGPU::VertexFormat::Unorm16:
        return "UShortNormalized"_s;
    case WebGPU::VertexFormat::Unorm16x2:
        return "UShort2Normalized"_s;
    case WebGPU::VertexFormat::Unorm16x4:
        return "UShort4Normalized"_s;
    case WebGPU::VertexFormat::Snorm16:
        return "ShortNormalized"_s;
    case WebGPU::VertexFormat::Snorm16x2:
        return "Short2Normalized"_s;
    case WebGPU::VertexFormat::Snorm16x4:
        return "Short4Normalized"_s;
    case WebGPU::VertexFormat::Float16:
        return "Half"_s;
    case WebGPU::VertexFormat::Float16x2:
        return "Half2"_s;
    case WebGPU::VertexFormat::Float16x4:
        return "Half4"_s;
    case WebGPU::VertexFormat::Float32:
        return "Float"_s;
    case WebGPU::VertexFormat::Float32x2:
        return "Float2"_s;
    case WebGPU::VertexFormat::Float32x3:
        return "Float3"_s;
    case WebGPU::VertexFormat::Float32x4:
        return "Float4"_s;
    case WebGPU::VertexFormat::Uint32:
        return "UInt"_s;
    case WebGPU::VertexFormat::Uint32x2:
        return "UInt2"_s;
    case WebGPU::VertexFormat::Uint32x3:
        return "UInt3"_s;
    case WebGPU::VertexFormat::Uint32x4:
        return "UInt4"_s;
    case WebGPU::VertexFormat::Sint32:
        return "Int"_s;
    case WebGPU::VertexFormat::Sint32x2:
        return "Int2"_s;
    case WebGPU::VertexFormat::Sint32x3:
        return "Int3"_s;
    case WebGPU::VertexFormat::Sint32x4:
        return "Int4"_s;
    case WebGPU::VertexFormat::Snorm1010102:
        return "SInt1010102Normalized"_s;
    case WebGPU::VertexFormat::Unorm1010102:
        return "UInt1010102Normalized"_s;
    case WebGPU::VertexFormat::Unorm8x4Bgra:
        return "Unorm8x4Bgra"_s;
    }
}

enum class VertexFormatType {
    Undefined,
    SignedInt,
    UnsignedInt,
    Float
};

static constexpr VertexFormatType LEGACY_NODELETE formatType(WebGPU::VertexFormat format)
{
    switch (format) {
    case WebGPU::VertexFormat::Uint8:
    case WebGPU::VertexFormat::Uint8x2:
    case WebGPU::VertexFormat::Uint8x4:
    case WebGPU::VertexFormat::Uint16:
    case WebGPU::VertexFormat::Uint16x2:
    case WebGPU::VertexFormat::Uint16x4:
    case WebGPU::VertexFormat::Uint32:
    case WebGPU::VertexFormat::Uint32x2:
    case WebGPU::VertexFormat::Uint32x3:
    case WebGPU::VertexFormat::Uint32x4:
        return VertexFormatType::UnsignedInt;

    case WebGPU::VertexFormat::Sint8:
    case WebGPU::VertexFormat::Sint8x2:
    case WebGPU::VertexFormat::Sint8x4:
    case WebGPU::VertexFormat::Sint16:
    case WebGPU::VertexFormat::Sint16x2:
    case WebGPU::VertexFormat::Sint16x4:
    case WebGPU::VertexFormat::Sint32:
    case WebGPU::VertexFormat::Sint32x2:
    case WebGPU::VertexFormat::Sint32x3:
    case WebGPU::VertexFormat::Sint32x4:
        return VertexFormatType::SignedInt;

    case WebGPU::VertexFormat::Unorm8:
    case WebGPU::VertexFormat::Unorm8x2:
    case WebGPU::VertexFormat::Unorm8x4:
    case WebGPU::VertexFormat::Snorm8:
    case WebGPU::VertexFormat::Snorm8x2:
    case WebGPU::VertexFormat::Snorm8x4:
    case WebGPU::VertexFormat::Unorm16:
    case WebGPU::VertexFormat::Unorm16x2:
    case WebGPU::VertexFormat::Unorm16x4:
    case WebGPU::VertexFormat::Snorm16:
    case WebGPU::VertexFormat::Snorm16x2:
    case WebGPU::VertexFormat::Snorm16x4:
    case WebGPU::VertexFormat::Float16:
    case WebGPU::VertexFormat::Float16x2:
    case WebGPU::VertexFormat::Float16x4:
    case WebGPU::VertexFormat::Float32:
    case WebGPU::VertexFormat::Float32x2:
    case WebGPU::VertexFormat::Float32x3:
    case WebGPU::VertexFormat::Float32x4:
    case WebGPU::VertexFormat::Snorm1010102:
    case WebGPU::VertexFormat::Unorm1010102:
    case WebGPU::VertexFormat::Unorm8x4Bgra:
        return VertexFormatType::Float;

    }
}

static bool LEGACY_NODELETE matchesFormat(const ShaderModule::VertexStageIn& stageIn, uint32_t shaderLocation, WebGPU::VertexFormat format)
{
    auto it = stageIn.find(shaderLocation);
    if (it == stageIn.end())
        return false;

    return formatType(it->value) == formatType(format);
}

static MTLVertexDescriptor *createVertexDescriptor(const WebGPU::VertexState& vertexState, const Limits& limits, const ShaderModule::VertexStageIn& stageIn, RenderPipeline::RequiredBufferIndicesContainer& requiredBufferIndices, NSString** error, ShaderModule::VertexStageIn& outShaderLocations)
{
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];
    Checked<uint32_t> totalAttributeCount = 0;
    ASSERT(error);

    if (vertexState.buffers.size() > limits.maxVertexBuffers) {
        *error = [NSString stringWithFormat:@"vertexBuffer count(%zu) exceeds limit(%u)", vertexState.buffers.size(), limits.maxVertexBuffers];
        return nil;
    }

    ShaderModule::VertexStageIn shaderLocations;
    for (auto [ bufferIndex, optionalBuffer ] : indexedRange(vertexState.buffers)) {
        if (!optionalBuffer)
            continue;
        auto& buffer = *optionalBuffer;

        if (buffer.arrayStride > limits.maxVertexBufferArrayStride || (buffer.arrayStride % 4)) {
            *error = [NSString stringWithFormat:@"buffer.arrayStride(%llu) > limits.maxVertexBufferArrayStride(%u) || (buffer.arrayStride %llu)", buffer.arrayStride, limits.maxVertexBufferArrayStride, buffer.arrayStride];
            return nil;
        }

        if (buffer.attributes.empty())
            continue;

        totalAttributeCount = checkedSum<uint32_t>(totalAttributeCount, buffer.attributes.size());
        if (totalAttributeCount.hasOverflowed()) {
            *error = @"Over 2^32 - 1 attributes in the vertex descriptor, failing due to out-of-memory.";
            return nil;
        }

        auto stride = std::max<NSUInteger>(sizeof(int), buffer.arrayStride);
        RELEASE_ASSERT(!requiredBufferIndices.contains(bufferIndex));
        ASSERT(bufferIndex <= std::numeric_limits<uint32_t>::max() && stride <= std::numeric_limits<uint32_t>::max());

        uint64_t lastStride = 0;
        vertexDescriptor.layouts[bufferIndex].stride = stride;
        auto stepMode = buffer.stepMode;
        vertexDescriptor.layouts[bufferIndex].stepFunction = stepFunction(stepMode, buffer.arrayStride);
        if (vertexDescriptor.layouts[bufferIndex].stepFunction == MTLVertexStepFunctionConstant)
            vertexDescriptor.layouts[bufferIndex].stepRate = 0;
        for (auto& attribute : buffer.attributes) {
            auto attributeFormat = attribute.format;
            auto formatSize = vertexFormatSize(attributeFormat);
            auto offsetPlusFormatSize = checkedSum<uint64_t>(attribute.offset, formatSize);
            if (offsetPlusFormatSize.hasOverflowed()) {
                *error = @"attribute.offset + formatSize > uint64::max()";
                return nil;
            }
            lastStride = std::max<uint64_t>(lastStride, offsetPlusFormatSize.value());
            if (!buffer.arrayStride) {
                if (offsetPlusFormatSize.value() > limits.maxVertexBufferArrayStride) {
                    *error = @"attribute.offset + formatSize > limits.maxVertexBufferArrayStride";
                    return nil;
                }
            } else if (offsetPlusFormatSize.value() > buffer.arrayStride) {
                *error = [NSString stringWithFormat:@"attribute.offset(%llu) + formatSize(%zu) > buffer.arrayStride(%llu)", attribute.offset, formatSize, buffer.arrayStride];
                return nil;
            }

            if (attribute.offset % std::min<size_t>(4, formatSize)) {
                *error = [NSString stringWithFormat:@"attribute.offset(%llu) mod std::min<size_t>(4, formatSize)(%lu) is not zero", attribute.offset, std::min<size_t>(4, formatSize)];
                return nil;
            }

            auto shaderLocation = attribute.shaderLocation;
            if (shaderLocation >= limits.maxVertexAttributes || shaderLocations.contains(shaderLocation)) {
                *error = [NSString stringWithFormat:@"shaderLocation(%u) >= limits.maxVertexAttributes(%u) || shaderLocations.contains(shaderLocation) %d", shaderLocation, limits.maxVertexAttributes, shaderLocations.contains(shaderLocation)];
                return nil;
            }

            shaderLocations.add(shaderLocation, attributeFormat);
            const auto& mtlAttribute = vertexDescriptor.attributes[shaderLocation];
            mtlAttribute.format = vertexFormat(attributeFormat);
            mtlAttribute.bufferIndex = bufferIndex;
            mtlAttribute.offset = attribute.offset;
        }

        ASSERT(!requiredBufferIndices.contains(bufferIndex));
        requiredBufferIndices.add(static_cast<uint32_t>(bufferIndex), RenderPipeline::BufferData {
            .stride = buffer.arrayStride,
            .lastStride = lastStride,
            .stepMode = stepMode
        });
    }

    for (auto& [shaderLocation, attributeFormat] : stageIn) {
        auto formatSize = vertexFormatSize(attributeFormat);
        if (!matchesFormat(shaderLocations, shaderLocation, attributeFormat)) {
            auto it = stageIn.find(shaderLocation);
            ASCIILiteral otherFormat = "undefined"_s;
            if (it != stageIn.end())
                otherFormat = name(it->value);
            *error = [NSString stringWithFormat:@"!matchesFormat(attribute(%d), format(%s), size(%zu), otherFormat(%s)", shaderLocation, name(attributeFormat).characters(), formatSize, otherFormat.characters()];
            return nil;
        }
    }

    if (totalAttributeCount.value() > limits.maxVertexAttributes) {
        *error = @"totalAttributeCount > limits.maxVertexAttributes";
        return nil;
    }
#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
    outShaderLocations = shaderLocations;
#else
    UNUSED_PARAM(outShaderLocations);
#endif

    return vertexDescriptor;
}

static void populateStencilOperation(MTLStencilDescriptor *mtlStencil, const WebGPU::StencilFaceState& stencil, uint32_t stencilReadMask, uint32_t stencilWriteMask)
{
    mtlStencil.stencilCompareFunction =  convertToMTLCompare(stencil.compare);
    mtlStencil.stencilFailureOperation = convertToMTLStencilOperation(stencil.failOp);
    mtlStencil.depthFailureOperation = convertToMTLStencilOperation(stencil.depthFailOp);
    mtlStencil.depthStencilPassOperation = convertToMTLStencilOperation(stencil.passOp);
    mtlStencil.writeMask = stencilWriteMask;
    mtlStencil.readMask = stencilReadMask;
}

static BindGroupLayout::BufferBindingType LEGACY_NODELETE convertBindingType(WGSL::BufferBindingType bindingType)
{
    switch (bindingType) {
    case WGSL::BufferBindingType::Uniform:
        return BindGroupLayout::BufferBindingType::Uniform;
    case WGSL::BufferBindingType::Storage:
        return BindGroupLayout::BufferBindingType::Storage;
    case WGSL::BufferBindingType::ReadOnlyStorage:
        return BindGroupLayout::BufferBindingType::ReadOnlyStorage;
    }
}

static WebGPU::SamplerBindingType LEGACY_NODELETE convertSamplerBindingType(WGSL::SamplerBindingType samplerType)
{
    switch (samplerType) {
    case WGSL::SamplerBindingType::Filtering:
        return WebGPU::SamplerBindingType::Filtering;
    case WGSL::SamplerBindingType::NonFiltering:
        return WebGPU::SamplerBindingType::NonFiltering;
    case WGSL::SamplerBindingType::Comparison:
        return WebGPU::SamplerBindingType::Comparison;
    }
}

static OptionSet<WebGPU::ShaderStage> LEGACY_NODELETE convertVisibility(const OptionSet<WGSL::ShaderStage>& visibility)
{
    OptionSet<WebGPU::ShaderStage> flags;
    if (visibility & WGSL::ShaderStage::Vertex)
        flags |= WebGPU::ShaderStage::Vertex;
    if (visibility & WGSL::ShaderStage::Fragment)
        flags |= WebGPU::ShaderStage::Fragment;
    if (visibility & WGSL::ShaderStage::Compute)
        flags |= WebGPU::ShaderStage::Compute;

    return flags;
}

static WebGPU::TextureSampleType LEGACY_NODELETE convertSampleType(WGSL::TextureSampleType sampleType)
{
    switch (sampleType) {
    case WGSL::TextureSampleType::Float:
        return WebGPU::TextureSampleType::Float;
    case WGSL::TextureSampleType::UnfilterableFloat:
        return WebGPU::TextureSampleType::UnfilterableFloat;
    case WGSL::TextureSampleType::Depth:
        return WebGPU::TextureSampleType::Depth;
    case WGSL::TextureSampleType::SignedInt:
        return WebGPU::TextureSampleType::Sint;
    case WGSL::TextureSampleType::UnsignedInt:
        return WebGPU::TextureSampleType::Uint;
    }
}

static WebGPU::TextureViewDimension LEGACY_NODELETE convertViewDimension(WGSL::TextureViewDimension viewDimension)
{
    switch (viewDimension) {
    case WGSL::TextureViewDimension::OneDimensional:
        return WebGPU::TextureViewDimension::_1d;
    case WGSL::TextureViewDimension::TwoDimensional:
        return WebGPU::TextureViewDimension::_2d;
    case WGSL::TextureViewDimension::TwoDimensionalArray:
        return WebGPU::TextureViewDimension::_2dArray;
    case WGSL::TextureViewDimension::Cube:
        return WebGPU::TextureViewDimension::Cube;
    case WGSL::TextureViewDimension::CubeArray:
        return WebGPU::TextureViewDimension::CubeArray;
    case WGSL::TextureViewDimension::ThreeDimensional:
        return WebGPU::TextureViewDimension::_3d;
    }
}

static WebGPU::StorageTextureAccess LEGACY_NODELETE convertAccess(WGSL::StorageTextureAccess access)
{
    switch (access) {
    case WGSL::StorageTextureAccess::WriteOnly:
        return WebGPU::StorageTextureAccess::WriteOnly;
    case WGSL::StorageTextureAccess::ReadOnly:
        return WebGPU::StorageTextureAccess::ReadOnly;
    case WGSL::StorageTextureAccess::ReadWrite:
        return WebGPU::StorageTextureAccess::ReadWrite;
    }
}

static WebGPU::TextureFormat LEGACY_NODELETE convertFormat(WGSL::TexelFormat format)
{
    switch (format) {
    case WGSL::TexelFormat::BGRA8unorm:
        return WebGPU::TextureFormat::Bgra8unorm;
    case WGSL::TexelFormat::R32float:
        return WebGPU::TextureFormat::R32float;
    case WGSL::TexelFormat::R32sint:
        return WebGPU::TextureFormat::R32sint;
    case WGSL::TexelFormat::R32uint:
        return WebGPU::TextureFormat::R32uint;
    case WGSL::TexelFormat::RG32float:
        return WebGPU::TextureFormat::Rg32float;
    case WGSL::TexelFormat::RG32sint:
        return WebGPU::TextureFormat::Rg32sint;
    case WGSL::TexelFormat::RG32uint:
        return WebGPU::TextureFormat::Rg32uint;
    case WGSL::TexelFormat::RGBA16float:
        return WebGPU::TextureFormat::Rgba16float;
    case WGSL::TexelFormat::RGBA16sint:
        return WebGPU::TextureFormat::Rgba16sint;
    case WGSL::TexelFormat::RGBA16uint:
        return WebGPU::TextureFormat::Rgba16uint;
    case WGSL::TexelFormat::RGBA32float:
        return WebGPU::TextureFormat::Rgba32float;
    case WGSL::TexelFormat::RGBA32sint:
        return WebGPU::TextureFormat::Rgba32sint;
    case WGSL::TexelFormat::RGBA32uint:
        return WebGPU::TextureFormat::Rgba32uint;
    case WGSL::TexelFormat::RGBA8sint:
        return WebGPU::TextureFormat::Rgba8sint;
    case WGSL::TexelFormat::RGBA8snorm:
        return WebGPU::TextureFormat::Rgba8snorm;
    case WGSL::TexelFormat::RGBA8uint:
        return WebGPU::TextureFormat::Rgba8uint;
    case WGSL::TexelFormat::RGBA8unorm:
        return WebGPU::TextureFormat::Rgba8unorm;
    case WGSL::TexelFormat::RG16unorm:
        return WebGPU::TextureFormat::Rg16unorm;
    case WGSL::TexelFormat::RG16snorm:
        return WebGPU::TextureFormat::Rg16snorm;
    case WGSL::TexelFormat::RGBA16unorm:
        return WebGPU::TextureFormat::Rgba16unorm;
    case WGSL::TexelFormat::RGBA16snorm:
        return WebGPU::TextureFormat::Rgba16snorm;
    case WGSL::TexelFormat::R16unorm:
        return WebGPU::TextureFormat::R16unorm;
    case WGSL::TexelFormat::R16snorm:
        return WebGPU::TextureFormat::R16snorm;
    case WGSL::TexelFormat::R16float:
        return WebGPU::TextureFormat::R16float;
    case WGSL::TexelFormat::RG16float:
        return WebGPU::TextureFormat::Rg16float;
    case WGSL::TexelFormat::RGB10A2uint:
        return WebGPU::TextureFormat::Rgb10a2uint;
    case WGSL::TexelFormat::RGB10A2unorm:
        return WebGPU::TextureFormat::Rgb10a2unorm;
    case WGSL::TexelFormat::R16uint:
        return WebGPU::TextureFormat::R16uint;
    case WGSL::TexelFormat::R16sint:
        return WebGPU::TextureFormat::R16sint;
    case WGSL::TexelFormat::R8sint:
        return WebGPU::TextureFormat::R8sint;
    case WGSL::TexelFormat::R8snorm:
        return WebGPU::TextureFormat::R8snorm;
    case WGSL::TexelFormat::R8uint:
        return WebGPU::TextureFormat::R8uint;
    case WGSL::TexelFormat::R8unorm:
        return WebGPU::TextureFormat::R8unorm;
    case WGSL::TexelFormat::RG8sint:
        return WebGPU::TextureFormat::Rg8sint;
    case WGSL::TexelFormat::RG8snorm:
        return WebGPU::TextureFormat::Rg8snorm;
    case WGSL::TexelFormat::RG8uint:
        return WebGPU::TextureFormat::Rg8uint;
    case WGSL::TexelFormat::RG8unorm:
        return WebGPU::TextureFormat::Rg8unorm;
    case WGSL::TexelFormat::RG16uint:
        return WebGPU::TextureFormat::Rg16uint;
    case WGSL::TexelFormat::RG16sint:
        return WebGPU::TextureFormat::Rg16sint;
    case WGSL::TexelFormat::RG11B10ufloat:
        return WebGPU::TextureFormat::Rg11b10ufloat;
    }
}

static BindGroupLayout::Entry::BindingLayout makeBindingLayout(auto& bindingMember, std::optional<BindGroupLayout::BufferBindingType> bufferTypeOverride = std::nullopt, uint64_t bufferSizeForBinding = 0)
{
    using Result = BindGroupLayout::Entry::BindingLayout;
    return WTF::switchOn(bindingMember, [&](const WGSL::BufferBindingLayout& bufferBinding) -> Result {
        return BindGroupLayout::BufferBindingLayout {
            .type = bufferTypeOverride.value_or(convertBindingType(bufferBinding.type)),
            .hasDynamicOffset = bufferBinding.hasDynamicOffset,
            .minBindingSize = bufferBinding.minBindingSize,
            .bufferSizeForBinding = bufferSizeForBinding,
        };
    }, [&](const WGSL::SamplerBindingLayout& sampler) -> Result {
        return BindGroupLayout::SamplerBindingLayout {
            .type = convertSamplerBindingType(sampler.type)
        };
    }, [&](const WGSL::TextureBindingLayout& texture) -> Result {
        return BindGroupLayout::TextureBindingLayout {
            .sampleType = convertSampleType(texture.sampleType),
            .viewDimension = convertViewDimension(texture.viewDimension),
            .multisampled = texture.multisampled
        };
    }, [&](const WGSL::StorageTextureBindingLayout& storageTexture) -> Result {
        return BindGroupLayout::StorageTextureBindingLayout {
            .access = convertAccess(storageTexture.access),
            .format = convertFormat(storageTexture.format),
            .viewDimension = convertViewDimension(storageTexture.viewDimension)
        };
    }, [&](const WGSL::ExternalTextureBindingLayout&) -> Result {
        return BindGroupLayout::ExternalTextureBindingLayout { };
    });
}

NSString* Device::addPipelineLayouts(Vector<Vector<ResolvedBindGroupLayoutEntry>>& pipelineEntries, const std::optional<WGSL::PipelineLayout>& optionalPipelineLayout)
{
    if (!optionalPipelineLayout || !optionalPipelineLayout->bindGroupLayouts.size())
        return nil;

    auto &pipelineLayout = *optionalPipelineLayout;
    uint32_t maxGroupIndex = 0;
    auto& deviceLimits = limits();
    for (auto& bgl : pipelineLayout.bindGroupLayouts)
        maxGroupIndex = std::max<uint32_t>(maxGroupIndex, bgl.group);

    size_t bindGroupLayoutCount = static_cast<size_t>(maxGroupIndex) + 1;
    if (bindGroupLayoutCount > deviceLimits.maxBindGroups || !bindGroupLayoutCount)
        return [NSString stringWithFormat:@"too many bind groups, limit %u, attempted %zu", deviceLimits.maxBindGroups, bindGroupLayoutCount];

    if (pipelineEntries.size() < bindGroupLayoutCount)
        pipelineEntries.grow(bindGroupLayoutCount);

    for (auto& bindGroupLayout : pipelineLayout.bindGroupLayouts) {
        auto& entries = pipelineEntries[bindGroupLayout.group];
        HashMap<String, uint64_t> entryMap;
        // Wrapping the bump would let the array-length entry alias a user binding and defeat the bounds check.
        auto bumpForArrayLength = [&](uint32_t webBinding) -> std::optional<uint32_t> {
            auto checked = checkedSum<uint32_t>(webBinding, limits().maxBindingsPerBindGroup);
            if (checked.hasOverflowed())
                return std::nullopt;
            return checked.value();
        };
        for (auto& entry : bindGroupLayout.entries) {
            auto visibility = convertVisibility(entry.visibility);
            // The entries of a shader have the visibility of its stage.
            auto stage = static_cast<size_t>(shaderStage(visibility));
            ResolvedBindGroupLayoutEntry newEntry;
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=265204 - use a set instead
            bool isArrayLength = false;
            uint32_t webBinding = entry.webBinding;
            if (auto& entryName = entry.name; entryName.length()) {
                if (entryName.endsWith("_ArrayLength"_s)) {
                    auto bumped = bumpForArrayLength(webBinding);
                    if (!bumped)
                        return @"Binding index overflow in auto-generated layouts";
                    webBinding = *bumped;
                    isArrayLength = true;
                }
            }

            if (auto existingBindingIndex = entries.findIf([&](const ResolvedBindGroupLayoutEntry& e) {
                return e.binding == webBinding;
            }); existingBindingIndex != notFound) {
                entries[existingBindingIndex].visibility |= visibility;
                std::span(entries[existingBindingIndex].metalBinding)[stage] = entry.binding;
                if (!BindGroupLayout::equalBindingEntries(entries[existingBindingIndex].bindingLayout, makeBindingLayout(entry.bindingMember)))
                    return @"Binding mismatch in auto-generated layouts";
                entryMap.set(entry.name, webBinding);
                continue;
            }
            uint64_t bufferSizeForBinding = 0;
            std::optional<BindGroupLayout::BufferBindingType> bufferTypeOverride;
            if (auto& entryName = entry.name; entryName.length()) {
                if (isArrayLength) {
                    auto bumped = bumpForArrayLength(webBinding);
                    if (!bumped)
                        return @"Binding index overflow in auto-generated layouts";
                    webBinding = *bumped;
                    bufferTypeOverride = BindGroupLayout::BufferBindingType::ArrayLength;
                    auto shortName = entryName.substring(2, entryName.length() - (sizeof("_ArrayLength") + 1));
                    if (auto it = entryMap.find(shortName); it != entryMap.end())
                        bufferSizeForBinding = it->value;
                } else
                    entryMap.set(entryName, webBinding);
            }

            newEntry.binding = webBinding;
            std::span(newEntry.metalBinding)[stage] = entry.binding;
            newEntry.visibility = visibility;
            newEntry.bindingLayout = makeBindingLayout(entry.bindingMember, bufferTypeOverride, bufferSizeForBinding);

            entries.append(WTF::move(newEntry));
        }
    }

    return nil;
}

Ref<PipelineLayout> Device::generatePipelineLayout(const Vector<Vector<ResolvedBindGroupLayoutEntry>>& bindGroupEntries)
{
    Vector<Ref<WebGPU::BindGroupLayout>> bindGroupLayouts;
    bindGroupLayouts.reserveInitialCapacity(bindGroupEntries.size());
    for (auto& entries : bindGroupEntries) {
        auto layoutEntries = entries;
        auto bindGroupLayout = createBindGroupLayout("getBindGroup() generated layout"_s, WTF::move(layoutEntries), true);
        if (!bindGroupLayout->isValid())
            return PipelineLayout::createInvalid(*this);
        bindGroupLayouts.append(WTF::move(bindGroupLayout));
    }

    auto generatedPipelineLayout = createPipelineLayout(WebGPU::PipelineLayoutDescriptor {
        .label = "generated pipeline layout"_s,
        .bindGroupLayouts = bindGroupLayouts.span(),
    }, true);

    return generatedPipelineLayout;
}

static Vector<std::optional<WebGPU::TextureFormat>> colorTargetFormats(const WebGPU::RenderPipelineDescriptor& descriptor)
{
    if (!descriptor.fragment)
        return { };
    return WTF::map(descriptor.fragment->targets, [](const auto& target) -> std::optional<WebGPU::TextureFormat> {
        if (!target)
            return std::nullopt;
        return target->format;
    });
}

static bool writesStencil(const WebGPU::RenderPipelineDescriptor& descriptor)
{
    auto& depthStencil = descriptor.depthStencil;
    if (!depthStencil || !depthStencil->stencilWriteMask)
        return false;
    const auto& stencilFront = depthStencil->stencilFront;
    const auto& stencilBack = depthStencil->stencilBack;
    const auto& cullMode = descriptor.primitive.cullMode;
    constexpr auto keep = WebGPU::StencilOperation::Keep;
    if (cullMode != WebGPU::CullMode::Front && (stencilFront.passOp != keep || stencilFront.depthFailOp != keep || stencilFront.failOp != keep))
        return true;
    if (cullMode != WebGPU::CullMode::Back && (stencilBack.passOp != keep || stencilBack.depthFailOp != keep || stencilBack.failOp != keep))
        return true;
    return false;
}

static std::pair<Ref<RenderPipeline>, NSString*> returnInvalidRenderPipeline(WebGPU::Metal::Device &object, bool isAsync, NSString* error)
{
    if (!isAsync)
        object.generateAValidationError(error);
    return std::make_pair(RenderPipeline::createInvalid(object), error);
}

static std::pair<Ref<RenderPipeline>, NSString*> returnInvalidRenderPipeline(WebGPU::Metal::Device &object, bool isAsync, String&& error)
{
    return returnInvalidRenderPipeline(object, isAsync, error.createNSString().get());
}

static constexpr ASCIILiteral name(WebGPU::CompareFunction compare)
{
    switch (compare) {
    case WebGPU::CompareFunction::Never: return "never"_s;
    case WebGPU::CompareFunction::Less: return "less"_s;
    case WebGPU::CompareFunction::LessEqual: return "less-equal"_s;
    case WebGPU::CompareFunction::Greater: return "greater"_s;
    case WebGPU::CompareFunction::GreaterEqual: return "greater-equal"_s;
    case WebGPU::CompareFunction::Equal: return "equal"_s;
    case WebGPU::CompareFunction::NotEqual: return "not-equal"_s;
    case WebGPU::CompareFunction::Always: return "always"_s;
    }
}
static constexpr ASCIILiteral name(WebGPU::StencilOperation operation)
{
    switch (operation) {
    case WebGPU::StencilOperation::Keep: return "keep"_s;
    case WebGPU::StencilOperation::Zero: return "zero"_s;
    case WebGPU::StencilOperation::Replace: return "replace"_s;
    case WebGPU::StencilOperation::Invert: return "invert"_s;
    case WebGPU::StencilOperation::IncrementClamp: return "increment-clamp"_s;
    case WebGPU::StencilOperation::DecrementClamp: return "decrement-clamp"_s;
    case WebGPU::StencilOperation::IncrementWrap: return "increment-wrap"_s;
    case WebGPU::StencilOperation::DecrementWrap: return "decrement-wrap"_s;
    }
}

static NSString* errorValidatingDepthStencilState(const WebGPU::DepthStencilState& depthStencil)
{
#define ERROR_STRING(x) ([NSString stringWithFormat:@"Invalid DepthStencilState: %@", x])
    auto format = depthStencil.format;
    if (!Texture::isDepthOrStencilFormat(format))
        return ERROR_STRING(@"Color format passed to depth / stencil format");

    auto depthFormat = Texture::depthOnlyAspectMetalFormat(format);
    if (depthStencil.depthWriteEnabled.value_or(false) || (depthStencil.depthCompare && *depthStencil.depthCompare != WebGPU::CompareFunction::Always)) {
        if (!depthFormat)
            return ERROR_STRING(@"depth-stencil state missing format");
    }

    auto isDefault = ^(const WebGPU::StencilFaceState& s) {
        return s.compare == WebGPU::CompareFunction::Always && s.failOp == WebGPU::StencilOperation::Keep && s.depthFailOp == WebGPU::StencilOperation::Keep && s.passOp == WebGPU::StencilOperation::Keep;
    };
    if (!isDefault(depthStencil.stencilFront) || !isDefault(depthStencil.stencilBack)) {
        if (!Texture::stencilOnlyAspectMetalFormat(format)) {
            NSString *error = [NSString stringWithFormat:@"missing stencil format - stencilFront: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s, stencilBack: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s", name(depthStencil.stencilFront.compare).characters(), name(depthStencil.stencilFront.failOp).characters(), name(depthStencil.stencilFront.depthFailOp).characters(), name(depthStencil.stencilFront.passOp).characters(), name(depthStencil.stencilBack.compare).characters(), name(depthStencil.stencilBack.failOp).characters(), name(depthStencil.stencilBack.depthFailOp).characters(), name(depthStencil.stencilBack.passOp).characters()];
            return ERROR_STRING(error);
        }
    }

    if (depthFormat) {
        if (!depthStencil.depthWriteEnabled)
            return ERROR_STRING(@"depthWrite must be provided");

        if (*depthStencil.depthWriteEnabled || depthStencil.stencilFront.depthFailOp != WebGPU::StencilOperation::Keep || depthStencil.stencilBack.depthFailOp != WebGPU::StencilOperation::Keep) {
            if (!depthStencil.depthCompare)
                return ERROR_STRING(@"Depth compare must be provided");
        }
    }
#undef ERROR_STRING
    return nil;
}

static bool LEGACY_NODELETE hasAlphaChannel(WebGPU::TextureFormat format)
{
    switch (format) {
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::R32float:
    case WebGPU::TextureFormat::R32uint:
    case WebGPU::TextureFormat::R32sint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg16float:
        return false;
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
        return true;
    case WebGPU::TextureFormat::Rg11b10ufloat:
    case WebGPU::TextureFormat::Rgb9e5ufloat:
    case WebGPU::TextureFormat::Rg32float:
    case WebGPU::TextureFormat::Rg32uint:
    case WebGPU::TextureFormat::Rg32sint:
        return false;
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
    case WebGPU::TextureFormat::Rgba32uint:
    case WebGPU::TextureFormat::Rgba32sint:
        return true;
    case WebGPU::TextureFormat::Stencil8:
    case WebGPU::TextureFormat::Depth16unorm:
    case WebGPU::TextureFormat::Depth24plus:
    case WebGPU::TextureFormat::Depth24plusStencil8:
    case WebGPU::TextureFormat::Depth32float:
    case WebGPU::TextureFormat::Depth32floatStencil8:
        return false;
    case WebGPU::TextureFormat::Bc1RgbaUnorm:
    case WebGPU::TextureFormat::Bc1RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc2RgbaUnorm:
    case WebGPU::TextureFormat::Bc2RgbaUnormSRGB:
    case WebGPU::TextureFormat::Bc3RgbaUnorm:
    case WebGPU::TextureFormat::Bc3RgbaUnormSRGB:
        return true;
    case WebGPU::TextureFormat::Bc4RUnorm:
    case WebGPU::TextureFormat::Bc4RSnorm:
    case WebGPU::TextureFormat::Bc5RgUnorm:
    case WebGPU::TextureFormat::Bc5RgSnorm:
    case WebGPU::TextureFormat::Bc6hRgbUfloat:
    case WebGPU::TextureFormat::Bc6hRgbFloat:
        return false;
    case WebGPU::TextureFormat::Bc7RgbaUnorm:
    case WebGPU::TextureFormat::Bc7RgbaUnormSRGB:
        return true;
    case WebGPU::TextureFormat::Etc2Rgb8unorm:
    case WebGPU::TextureFormat::Etc2Rgb8unormSRGB:
        return false;
    case WebGPU::TextureFormat::Etc2Rgb8a1unorm:
    case WebGPU::TextureFormat::Etc2Rgb8a1unormSRGB:
    case WebGPU::TextureFormat::Etc2Rgba8unorm:
    case WebGPU::TextureFormat::Etc2Rgba8unormSRGB:
        return true;
    case WebGPU::TextureFormat::EacR11unorm:
    case WebGPU::TextureFormat::EacR11snorm:
    case WebGPU::TextureFormat::EacRg11unorm:
    case WebGPU::TextureFormat::EacRg11snorm:
    case WebGPU::TextureFormat::Astc4x4Unorm:
    case WebGPU::TextureFormat::Astc4x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x4Unorm:
    case WebGPU::TextureFormat::Astc5x4UnormSRGB:
    case WebGPU::TextureFormat::Astc5x5Unorm:
    case WebGPU::TextureFormat::Astc5x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x5Unorm:
    case WebGPU::TextureFormat::Astc6x5UnormSRGB:
    case WebGPU::TextureFormat::Astc6x6Unorm:
    case WebGPU::TextureFormat::Astc6x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x5Unorm:
    case WebGPU::TextureFormat::Astc8x5UnormSRGB:
    case WebGPU::TextureFormat::Astc8x6Unorm:
    case WebGPU::TextureFormat::Astc8x6UnormSRGB:
    case WebGPU::TextureFormat::Astc8x8Unorm:
    case WebGPU::TextureFormat::Astc8x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x5Unorm:
    case WebGPU::TextureFormat::Astc10x5UnormSRGB:
    case WebGPU::TextureFormat::Astc10x6Unorm:
    case WebGPU::TextureFormat::Astc10x6UnormSRGB:
    case WebGPU::TextureFormat::Astc10x8Unorm:
    case WebGPU::TextureFormat::Astc10x8UnormSRGB:
    case WebGPU::TextureFormat::Astc10x10Unorm:
    case WebGPU::TextureFormat::Astc10x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x10Unorm:
    case WebGPU::TextureFormat::Astc12x10UnormSRGB:
    case WebGPU::TextureFormat::Astc12x12Unorm:
    case WebGPU::TextureFormat::Astc12x12UnormSRGB:
        return false;
    }
}

static bool LEGACY_NODELETE textureFormatAllowedForRetunType(WebGPU::TextureFormat format, MTLDataType dataType, bool readsAlpha)
{
    if (dataType == MTLDataTypeNone)
        return true;

    if (readsAlpha && !(dataType == MTLDataTypeFloat4 || dataType == MTLDataTypeInt4 || dataType == MTLDataTypeUInt4))
        return false;

    switch (format) {
    case WebGPU::TextureFormat::R8snorm:
    case WebGPU::TextureFormat::R8unorm:
    case WebGPU::TextureFormat::R16snorm:
    case WebGPU::TextureFormat::R16unorm:
    case WebGPU::TextureFormat::R16float:
    case WebGPU::TextureFormat::R32float:
        return dataType == MTLDataTypeFloat || dataType == MTLDataTypeFloat2 || dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;

    case WebGPU::TextureFormat::Rg8snorm:
    case WebGPU::TextureFormat::Rg8unorm:
    case WebGPU::TextureFormat::Rg16snorm:
    case WebGPU::TextureFormat::Rg16unorm:
    case WebGPU::TextureFormat::Rg16float:
    case WebGPU::TextureFormat::Rg32float:
        return dataType == MTLDataTypeFloat2 || dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;

    case WebGPU::TextureFormat::Rgba8snorm:
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgb10a2unorm:
    case WebGPU::TextureFormat::Rgba16snorm:
    case WebGPU::TextureFormat::Rgba16unorm:
    case WebGPU::TextureFormat::Rgba16float:
    case WebGPU::TextureFormat::Rgba32float:
        return dataType == MTLDataTypeFloat4;

    case WebGPU::TextureFormat::R8uint:
    case WebGPU::TextureFormat::R16uint:
    case WebGPU::TextureFormat::R32uint:
        return dataType == MTLDataTypeUInt || dataType == MTLDataTypeUInt2 || dataType == MTLDataTypeUInt3 || dataType == MTLDataTypeUInt4;

    case WebGPU::TextureFormat::R8sint:
    case WebGPU::TextureFormat::R16sint:
    case WebGPU::TextureFormat::R32sint:
        return dataType == MTLDataTypeInt || dataType == MTLDataTypeInt2 || dataType == MTLDataTypeInt3 || dataType == MTLDataTypeInt4;

    case WebGPU::TextureFormat::Rg8uint:
    case WebGPU::TextureFormat::Rg16uint:
    case WebGPU::TextureFormat::Rg32uint:
        return dataType == MTLDataTypeUInt2 || dataType == MTLDataTypeUInt3 || dataType == MTLDataTypeUInt4;

    case WebGPU::TextureFormat::Rg8sint:
    case WebGPU::TextureFormat::Rg16sint:
    case WebGPU::TextureFormat::Rg32sint:
        return dataType == MTLDataTypeInt2 || dataType == MTLDataTypeInt3 || dataType == MTLDataTypeInt4;

    case WebGPU::TextureFormat::Rgba8uint:
    case WebGPU::TextureFormat::Rgb10a2uint:
    case WebGPU::TextureFormat::Rgba16uint:
    case WebGPU::TextureFormat::Rgba32uint:
        return dataType == MTLDataTypeUInt4;

    case WebGPU::TextureFormat::Rgba8sint:
    case WebGPU::TextureFormat::Rgba16sint:
    case WebGPU::TextureFormat::Rgba32sint:
        return dataType == MTLDataTypeInt4;

    case WebGPU::TextureFormat::Rg11b10ufloat:
        return dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;
    default:
        return false;
    }
}

static uint32_t LEGACY_NODELETE componentsForDataType(MTLDataType dataType)
{
    switch (dataType) {
    case MTLDataTypeBool:
    case MTLDataTypeInt:
    case MTLDataTypeUInt:
    case MTLDataTypeHalf:
    case MTLDataTypeFloat:
        return 1;
    case MTLDataTypeBool2:
    case MTLDataTypeInt2:
    case MTLDataTypeUInt2:
    case MTLDataTypeHalf2:
    case MTLDataTypeFloat2:
        return 2;
    case MTLDataTypeBool3:
    case MTLDataTypeInt3:
    case MTLDataTypeUInt3:
    case MTLDataTypeHalf3:
    case MTLDataTypeFloat3:
        return 3;
    case MTLDataTypeBool4:
    case MTLDataTypeInt4:
    case MTLDataTypeUInt4:
    case MTLDataTypeHalf4:
    case MTLDataTypeFloat4:
        return 4;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static NSString* errorValidatingInterstageShaderInterfaces(WebGPU::Metal::Device &device, const WebGPU::RenderPipelineDescriptor& descriptor, const ShaderModule::VertexOutputs* vertexOutputs, uint32_t vertexClipDistancesCount, const ShaderModule::FragmentInputs* fragmentInputs, const ShaderModule::FragmentOutputs* fragmentOutputs, const ShaderModule* fragmentModule, auto* fragmentDescriptor)
{
    if (!vertexOutputs)
        return @"vertex shader has no outputs";

    constexpr uint32_t componentsPerVariable = 4;
    constexpr uint32_t componentsPerVariableMinusOne = componentsPerVariable - 1;
    auto maxVertexShaderOutputComponents = device.limits().maxInterStageShaderVariables * componentsPerVariable;
    if (descriptor.primitive.topology == WebGPU::PrimitiveTopology::PointList) {
        if (!maxVertexShaderOutputComponents)
            return @"maxVertexShaderOutputComponents is zero";
        maxVertexShaderOutputComponents -= componentsPerVariable;
    }

    // Per spec: if clip_distances is declared, decrement maxVertexShaderOutputComponents by clipDistancesSize
    if (vertexClipDistancesCount) {
        if (maxVertexShaderOutputComponents < vertexClipDistancesCount)
            return @"clip_distances requires more components than available";
        maxVertexShaderOutputComponents -= ((vertexClipDistancesCount + componentsPerVariableMinusOne) / componentsPerVariable) * componentsPerVariable;
    }

    auto maxInterStageShaderVariables = device.limits().maxInterStageShaderVariables;

    // Per spec: if clip_distances is declared, decrement maxVertexShaderOutputLocation by ceil(clipDistancesSize / 4)
    if (vertexClipDistancesCount) {
        auto clipDistancesVec4Count = (vertexClipDistancesCount + componentsPerVariableMinusOne) / componentsPerVariable;
        if (maxInterStageShaderVariables < clipDistancesVec4Count)
            return @"clip_distances requires more location slots than available";
        maxInterStageShaderVariables -= clipDistancesVec4Count;
    }
    uint32_t vertexScalarComponents = 0;
    for (auto& [location, structMember] : *vertexOutputs) {
        if (location >= maxInterStageShaderVariables)
            return @"location >= maxInterStageShaderVariables";

        vertexScalarComponents += componentsForDataType(structMember.dataType);
    }

    if (vertexScalarComponents > maxVertexShaderOutputComponents)
        return @"vertexScalarComponents > maxVertexShaderOutputComponents";

    if (fragmentModule) {
        auto maxFragmentShaderInputComponents = device.limits().maxInterStageShaderVariables * componentsPerVariable;
        auto decrementByVariable = ^(uint32_t& unsignedValue) {
            if (unsignedValue < componentsPerVariable)
                return false;
            unsignedValue -= componentsPerVariable;
            return true;
        };
        const auto& fragmentEntryPoint = (fragmentDescriptor && !fragmentDescriptor->stage.entryPoint.isNull()) ? fragmentDescriptor->stage.entryPoint : fragmentModule->defaultFragmentEntryPoint();
        if (fragmentModule->usesFrontFacingInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to front facing";
        if (fragmentModule->usesSampleIndexInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to sample index";
        if (fragmentModule->usesSampleMaskInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to sample mask";
        if (fragmentModule->usesPrimitiveIndexInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to primitive index";
        if (fragmentModule->usesSubgroupInvocationIdInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to subgroup invocation id";
        if (fragmentModule->usesSubgroupSizeInInput(fragmentEntryPoint) && !decrementByVariable(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to subgroup size";

        if (fragmentInputs) {
            WGSL::AST::Interpolation defaultInterpolation {
                .type = WGSL::InterpolationType::Perspective,
                .sampling = WGSL::InterpolationSampling::Center
            };
            auto notEqual = ^(const WGSL::AST::Interpolation& interpolateA, const WGSL::AST::Interpolation& interpolateB) {
                return interpolateA.type != interpolateB.type || interpolateA.sampling != interpolateB.sampling;
            };
            uint32_t fragmentScalarComponents = 0;
            for (auto& [location, structMember] : *fragmentInputs) {
                auto it = vertexOutputs->find(location);
                if (it == vertexOutputs->end() || it->value.dataType != structMember.dataType)
                    return @"data type between fragment inputs and vertex outputs do not match";

                fragmentScalarComponents += componentsForDataType(structMember.dataType);
                if (!structMember.interpolation && !it->value.interpolation)
                    continue;
                if (!structMember.interpolation && notEqual(*it->value.interpolation, defaultInterpolation))
                    return @"interpolation attributes do not match";
                if (!it->value.interpolation && notEqual(*structMember.interpolation, defaultInterpolation))
                    return @"interpolation attributes do not match";
                if (notEqual(structMember.interpolation.value_or(defaultInterpolation), it->value.interpolation.value_or(defaultInterpolation)))
                    return @"interpolation attributes do not match";
            }
            if (fragmentScalarComponents > maxFragmentShaderInputComponents)
                return [NSString stringWithFormat:@"fragmentScalarComponents(%u) > maxFragmentShaderInputComponents(%u)", fragmentScalarComponents, maxFragmentShaderInputComponents];
        }

        if (fragmentOutputs) {
            auto maxColorAttachments = device.limits().maxColorAttachments;
            for (auto& [location, _] : *fragmentOutputs) {
                if (location >= maxColorAttachments)
                    return [NSString stringWithFormat:@"location(%u) >= maxColorAttachments(%u)", location, maxColorAttachments];
            }
        }
    }

    return nil;
}

static NSString* errorValidatingVertexStageIn(const ShaderModule::VertexStageIn* stageIn, const Device& device)
{
    if (!stageIn)
        return nil;

    auto maxVertexAttributeLocation = device.limits().maxVertexAttributes;
    HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> shaderLocations;
    for (auto shaderLocation : stageIn->keys()) {
        if (shaderLocation >= maxVertexAttributeLocation)
            return [NSString stringWithFormat:@"Shader location %u exceeds the maximum allowed value of %u", shaderLocation, maxVertexAttributeLocation];
        if (shaderLocations.contains(shaderLocation))
            return [NSString stringWithFormat:@"Shader location %u appears twice", shaderLocation];
        shaderLocations.add(shaderLocation);
    }

    return nil;
}

RefPtr<WebGPU::RenderPipeline> Device::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor)
{
    std::optional<std::pair<Ref<RenderPipeline>, NSString*>> result;
    createRenderPipeline(descriptor, false, nullptr, LibraryCompilation::Synchronous, [&](std::pair<Ref<RenderPipeline>, NSString*>&& pipelineAndError) {
        result = WTF::move(pipelineAndError);
    });
    // LibraryCompilation::Synchronous never defers the completion handler.
    RELEASE_ASSERT(result);
    return WTF::move(result->first);
}

void Device::createRenderPipeline(const WebGPU::RenderPipelineDescriptor& descriptor, bool isAsync, const RenderPipeline* pipelineToReplace, LibraryCompilation libraryCompilation, CompletionHandler<void(std::pair<Ref<RenderPipeline>, NSString*>&&)>&& callback)
{
    if (!validateRenderPipeline(descriptor) || !isValid())
        return callback(returnInvalidRenderPipeline(*this, isAsync, "device or descriptor is not valid"_s));

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
#if ENABLE(WEBGPU_BY_DEFAULT)
    mtlRenderPipelineDescriptor.shaderValidation = shaderValidationState();
#endif

    auto label = descriptor.label.createNSString();
    auto& deviceLimits = limits();

    // The pipeline stores and validates with the C API enums.
    auto primitiveTopology = descriptor.primitive.topology;
    auto stripIndexFormat = descriptor.primitive.stripIndexFormat;

    RefPtr<PipelineLayout> pipelineLayout;
    Vector<Vector<ResolvedBindGroupLayoutEntry>> bindGroupEntries;
    if (pipelineToReplace) {
        pipelineLayout = &pipelineToReplace->pipelineLayout();
        if (!isValidToUseWithDevice(*pipelineLayout, *this))
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Pipeline layout is not valid or created from different device"_s));
    } else if (descriptor.layout) {
        Ref layout = metal(*descriptor.layout);
        if (!isValidToUseWithDevice(layout.get(), *this))
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Pipeline layout is not valid or created from different device"_s));

        if (!layout->isAutoLayout())
            pipelineLayout = layout.ptr();
    }

    const ShaderModule::VertexStageIn* vertexStageIn = nullptr;
    const ShaderModule::VertexOutputs* vertexOutputs = nullptr;
    uint32_t vertexClipDistancesCount = 0;
    BufferBindingSizesForPipeline minimumBufferSizes;
    uint32_t vertexShaderBindingCount = 0;
    std::optional<PreparedLibrary> preparedVertexLibrary;
    std::optional<PreparedLibrary> preparedFragmentLibrary;
    ShaderModule::VertexStageIn shaderLocations;
    {
        Ref vertexModule = metal(descriptor.vertex.stage.module.get());
        if (!vertexModule->isValid() || !vertexModule->ast())
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Vertex module is not valid"_s));
        if (&vertexModule->device() != this)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Vertex module was created with a different device"_s));

        const auto& vertexEntryPoint = descriptor.vertex.stage.entryPoint.isNull() ? vertexModule->defaultVertexEntryPoint() : descriptor.vertex.stage.entryPoint;
        vertexStageIn = vertexModule->stageInTypesForEntryPoint(vertexEntryPoint);
        if (NSString* error = errorValidatingVertexStageIn(vertexStageIn, *this))
            return callback(returnInvalidRenderPipeline(*this, isAsync, error));
        NSError *error = nil;
        preparedVertexLibrary = prepareLibrary(vertexModule, pipelineLayout.get(), vertexEntryPoint, label.get(), descriptor.vertex.stage.constants, minimumBufferSizes, &error);
        if (!preparedVertexLibrary)
            return callback(returnInvalidRenderPipeline(*this, isAsync, error.localizedDescription ?: @"Vertex library failed creation"));

        const auto& entryPointInformation = preparedVertexLibrary->entryPointInformation;
        vertexShaderBindingCount = std::min<uint32_t>(entryPointInformation.bindingCount, std::numeric_limits<uint32_t>::max());
        if (!pipelineLayout) {
            if (NSString* error = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
                return callback(returnInvalidRenderPipeline(*this, isAsync, error));
        }
        if (entryPointInformation.specializationConstants.size() != preparedVertexLibrary->wgslConstantValues.size())
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Vertex function could not be created"_s));
        vertexOutputs = vertexModule->vertexReturnTypeForEntryPoint(vertexEntryPoint);
        vertexClipDistancesCount = vertexModule->clipDistancesCount(vertexEntryPoint);
    }

    bool usesFragDepth = false;
    bool usesSampleMask = false;
    bool hasAtLeastOneColorTarget = false;
    const ShaderModule::FragmentOutputs* fragmentReturnTypes { nullptr };
    const ShaderModule::FragmentInputs* fragmentInputs { nullptr };
    uint32_t colorAttachmentCount = 0;
    RefPtr<ShaderModule> fragmentModule;
    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        fragmentModule = &metal(fragmentDescriptor.stage.module.get());
        if (!fragmentModule->isValid() || !fragmentModule->ast())
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Fragment module is invalid"_s));

        if (&fragmentModule->device() != this)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Fragment module was created with a different device"_s));

        auto fragmentShaderModule = fragmentModule->ast();
        RELEASE_ASSERT(fragmentShaderModule);
        const auto& fragmentEntryPoint = fragmentDescriptor.stage.entryPoint.isNull() ? fragmentModule->defaultFragmentEntryPoint() : fragmentDescriptor.stage.entryPoint;
        usesFragDepth = fragmentModule->usesFragDepth(fragmentEntryPoint);
        usesSampleMask = fragmentModule->usesSampleMaskInOutput(fragmentEntryPoint);

        fragmentInputs = fragmentModule->fragmentInputsForEntryPoint(fragmentEntryPoint);
        fragmentReturnTypes = fragmentModule->fragmentReturnTypeForEntryPoint(fragmentEntryPoint);
        colorAttachmentCount = fragmentDescriptor.targets.size();
    }

    if (NSString* error = errorValidatingInterstageShaderInterfaces(*this, descriptor, vertexOutputs, vertexClipDistancesCount, fragmentInputs, fragmentReturnTypes, fragmentModule.get(), descriptor.fragment ? &*descriptor.fragment : nullptr))
        return callback(returnInvalidRenderPipeline(*this, isAsync, error));

    if (descriptor.fragment) {
        uint32_t bytesPerSample = 0;
        const auto& fragmentDescriptor = *descriptor.fragment;
        for (auto [ i, optionalTargetDescriptor ] : indexedRange(fragmentDescriptor.targets)) {
            if (!optionalTargetDescriptor)
                continue;
            auto& targetDescriptor = *optionalTargetDescriptor;
            auto targetFormat = targetDescriptor.format;

            MTLDataType fragmentFunctionReturnType = MTLDataTypeNone;
            if (fragmentReturnTypes) {
                if (auto it = fragmentReturnTypes->find(i); it != fragmentReturnTypes->end())
                    fragmentFunctionReturnType = it->value;
            }
            const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[i];

            if (Texture::isDepthOrStencilFormat(targetFormat) || !Texture::isRenderableFormat(targetFormat, *this))
                return callback(returnInvalidRenderPipeline(*this, isAsync, "Depth / stencil format passed to color format"_s));

            bytesPerSample = roundUpToMultipleOfNonPowerOfTwo(Texture::renderTargetPixelByteAlignment(targetFormat), bytesPerSample);
            bytesPerSample += Texture::renderTargetPixelByteCost(targetFormat);
            mtlColorAttachment.pixelFormat = Texture::pixelFormat(targetFormat);

            hasAtLeastOneColorTarget = true;
            if (targetDescriptor.writeMask.contains(WebGPU::ColorWrite::Invalid))
                return callback(returnInvalidRenderPipeline(*this, isAsync, "writeMask is invalid"_s));
            if (fragmentFunctionReturnType == MTLDataTypeNone && !targetDescriptor.writeMask.isEmpty())
                return callback(returnInvalidRenderPipeline(*this, isAsync, "writeMask is invalid"_s));
            mtlColorAttachment.writeMask = colorWriteMask(targetDescriptor.writeMask);

            bool readsAlpha = false;
            if (targetDescriptor.blend) {
                if (!Texture::supportsBlending(targetFormat, *this))
                    return callback(returnInvalidRenderPipeline(*this, isAsync, "Color target attempted to use blending on non-blendable format"_s));
                mtlColorAttachment.blendingEnabled = YES;

                const auto& alphaBlend = targetDescriptor.blend->alpha;
                const auto& colorBlend = targetDescriptor.blend->color;
                auto validateBlend = ^(const WebGPU::BlendComponent& blend) {
                    if (blend.operation == WebGPU::BlendOperation::Min || blend.operation == WebGPU::BlendOperation::Max)
                        return blend.srcFactor == WebGPU::BlendFactor::One && blend.dstFactor == WebGPU::BlendFactor::One;
                    return true;
                };
                if (!validateBlend(alphaBlend) || !validateBlend(colorBlend))
                    return callback(returnInvalidRenderPipeline(*this, isAsync, "Blend states are not valid"_s));
                mtlColorAttachment.alphaBlendOperation = blendOperation(alphaBlend.operation);
                mtlColorAttachment.sourceAlphaBlendFactor = blendFactor(alphaBlend.srcFactor);
                mtlColorAttachment.destinationAlphaBlendFactor = blendFactor(alphaBlend.dstFactor);

                mtlColorAttachment.rgbBlendOperation = blendOperation(colorBlend.operation);
                mtlColorAttachment.sourceRGBBlendFactor = blendFactor(colorBlend.srcFactor);
                mtlColorAttachment.destinationRGBBlendFactor = blendFactor(colorBlend.dstFactor);
                auto readsAlphaFactor = [](WebGPU::BlendFactor factor) {
                    return factor == WebGPU::BlendFactor::SrcAlpha || factor == WebGPU::BlendFactor::OneMinusSrcAlpha || factor == WebGPU::BlendFactor::SrcAlphaSaturated;
                };
                readsAlpha = readsAlphaFactor(colorBlend.srcFactor) || readsAlphaFactor(colorBlend.dstFactor);
            } else
                mtlColorAttachment.blendingEnabled = NO;

            if (!textureFormatAllowedForRetunType(targetFormat, fragmentFunctionReturnType, readsAlpha))
                return callback(returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"pipeline creation - color target pixel format(%lu) for location(%zu) is incompatible with shader output data type of %zu", i, mtlColorAttachment.pixelFormat, fragmentFunctionReturnType]));
        }

        if (bytesPerSample > deviceLimits.maxColorAttachmentBytesPerSample)
            return callback(returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"Bytes per sample(%u) exceeded maximum allowed limit(%u)", bytesPerSample, deviceLimits.maxColorAttachmentBytesPerSample]));

        NSError *error = nil;
        const auto& fragmentEntryPoint = fragmentDescriptor.stage.entryPoint.isNull() ? fragmentModule->defaultFragmentEntryPoint() : fragmentDescriptor.stage.entryPoint;
        preparedFragmentLibrary = prepareLibrary(*fragmentModule, pipelineLayout.get(), fragmentEntryPoint, label.get(), fragmentDescriptor.stage.constants, minimumBufferSizes, &error);
        if (!preparedFragmentLibrary)
            return callback(returnInvalidRenderPipeline(*this, isAsync, error.localizedDescription ?: @"Fragment library could not be created"));

        const auto& entryPointInformation = preparedFragmentLibrary->entryPointInformation;
        if (!pipelineLayout) {
            if (NSString* error = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
                return callback(returnInvalidRenderPipeline(*this, isAsync, error));
        }

        if (entryPointInformation.specializationConstants.size() != preparedFragmentLibrary->wgslConstantValues.size())
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Fragment function failed creation"_s));
    }

    MTLDepthStencilDescriptor *depthStencilDescriptor = nil;
    float depthBias = 0.f, depthBiasSlopeScale = 0.f, depthBiasClamp = 0.f;
    if (auto& depthStencil = descriptor.depthStencil) {
        if (NSString *error = errorValidatingDepthStencilState(*depthStencil))
            return callback(returnInvalidRenderPipeline(*this, isAsync, error));

        MTLPixelFormat depthStencilFormat = Texture::pixelFormat(depthStencil->format);
        bool isStencilOnlyFormat = Device::isStencilOnlyFormat(depthStencilFormat);
        mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = isStencilOnlyFormat ? MTLPixelFormatInvalid : depthStencilFormat;
        if (Texture::stencilOnlyAspectMetalFormat(depthStencil->format))
            mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = depthStencilFormat;

        depthStencilDescriptor = [MTLDepthStencilDescriptor new];
        depthStencilDescriptor.depthCompareFunction = depthStencil->depthCompare ? convertToMTLCompare(*depthStencil->depthCompare) : MTLCompareFunctionAlways;
        depthStencilDescriptor.depthWriteEnabled = depthStencil->depthWriteEnabled.value_or(false);
        populateStencilOperation(depthStencilDescriptor.frontFaceStencil, depthStencil->stencilFront, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        populateStencilOperation(depthStencilDescriptor.backFaceStencil, depthStencil->stencilBack, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        depthBias = depthStencil->depthBias;
        depthBiasSlopeScale = depthStencil->depthBiasSlopeScale;
        depthBiasClamp = depthStencil->depthBiasClamp;

        // Depth bias is derived from the slope of the primitive being rasterized, which only points
        // and lines lack, so for those topologies it has to be left at zero.
        if (primitiveTopology != WebGPU::PrimitiveTopology::TriangleList && primitiveTopology != WebGPU::PrimitiveTopology::TriangleStrip) {
            if (depthBias || depthBiasSlopeScale || depthBiasClamp)
                return callback(returnInvalidRenderPipeline(*this, isAsync, "depthBias, depthBiasSlopeScale, and depthBiasClamp must be 0 unless primitive.topology is a triangle topology"_s));
        }
    }

    // A render pipeline needs somewhere to render to. hasAtLeastOneColorTarget can only be set from
    // the fragment state's targets, so a vertex-only pipeline has to bring its own depth-stencil.
    if (!hasAtLeastOneColorTarget && !descriptor.depthStencil)
        return callback(returnInvalidRenderPipeline(*this, isAsync, "No color targets or depth stencil were specified in the descriptor"_s));
    if (usesFragDepth && mtlRenderPipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid)
        return callback(returnInvalidRenderPipeline(*this, isAsync, "Shader writes to frag depth but no depth texture set"_s));

    if (descriptor.multisample.count != 1 && descriptor.multisample.count != 4)
        return callback(returnInvalidRenderPipeline(*this, isAsync, "multisample count must be either 1 or 4"_s));
    mtlRenderPipelineDescriptor.rasterSampleCount = descriptor.multisample.count;
    mtlRenderPipelineDescriptor.alphaToCoverageEnabled = descriptor.multisample.alphaToCoverageEnabled;
    if (descriptor.multisample.alphaToCoverageEnabled) {
        if (usesSampleMask)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Can not use sampleMask with alphaToCoverage"_s));
        if (!descriptor.fragment)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s));
        auto& targets = descriptor.fragment->targets;
        auto* firstTarget = targets.empty() || !targets[0] ? nullptr : &*targets[0];
        if (!firstTarget || !hasAlphaChannel(firstTarget->format) || !Texture::supportsBlending(firstTarget->format, *this))
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s));
        if (descriptor.multisample.count == 1)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires multisampling"_s));
    }

    RELEASE_ASSERT([mtlRenderPipelineDescriptor respondsToSelector:@selector(setSampleMask:)]);
    uint32_t sampleMask = RenderBundleEncoder::defaultSampleMask;
    if (auto mask = descriptor.multisample.mask; mask != sampleMask) {
        if (!usesSampleMask)
            [mtlRenderPipelineDescriptor setSampleMask:mask];
        sampleMask = mask;
    }

    RenderPipeline::RequiredBufferIndicesContainer requiredBufferIndices;
    if (auto buffers = descriptor.vertex.buffers; !buffers.empty()) {
        if (!vertexStageIn)
            return callback(returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"Vertex shader has no stageIn parameters but buffer count was %zu and attribute count was %zu", buffers.size(), buffers[0] ? buffers[0]->attributes.size() : 0]));
        NSString *error = nil;
        MTLVertexDescriptor *vertexDecriptor = createVertexDescriptor(descriptor.vertex, deviceLimits, *vertexStageIn, requiredBufferIndices, &error, shaderLocations);
        if (error)
            return callback(returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"vertex descriptor creation failed %@", error]));

        ASSERT(vertexDecriptor);
        mtlRenderPipelineDescriptor.vertexDescriptor = vertexDecriptor;
    }

    if (vertexStageIn && vertexStageIn->size() && descriptor.vertex.buffers.empty())
        return callback(returnInvalidRenderPipeline(*this, isAsync, @"Vertex descriptor passed zero buffers for stage_in but shader requires buffers for stage_in"));

    MTLDepthClipMode mtlDepthClipMode = MTLDepthClipModeClip;
    if (descriptor.primitive.unclippedDepth) {
        if (!hasFeature(WebGPU::FeatureName::DepthClipControl))
            return callback(returnInvalidRenderPipeline(*this, isAsync, "unclippedDepth used without enabling depth-clip-control feature"_s));

        mtlDepthClipMode = MTLDepthClipModeClamp;
    }

    mtlRenderPipelineDescriptor.inputPrimitiveTopology = topologyType(primitiveTopology);

    // These properties are to be used by the render command encoder, not the render pipeline.
    // Therefore, the render pipeline stores these, and when the render command encoder is assigned
    // a pipeline, the render command encoder can get these information out of the render pipeline.
    if (primitiveTopology != WebGPU::PrimitiveTopology::LineStrip && primitiveTopology != WebGPU::PrimitiveTopology::TriangleStrip) {
        if (descriptor.primitive.stripIndexFormat)
            return callback(returnInvalidRenderPipeline(*this, isAsync, "If primitive.topology is not line-strip or triangle-strip, primitive.stripIndexFormat must be undefined."_s));
    }

    auto mtlPrimitiveType = primitiveType(primitiveTopology);
    auto mtlIndexType = stripIndexFormat ? indexType(*stripIndexFormat) : std::nullopt;
    auto mtlFrontFace = frontFace(descriptor.primitive.frontFace);
    auto mtlCullMode = cullMode(descriptor.primitive.cullMode);

    if (m_pipelineId == Device::maxPipelines) {
        loseTheDevice(WebGPU::DeviceLostReason::Unknown);
        return callback(returnInvalidRenderPipeline(*this, isAsync, @"too many render pipelines"));
    }

    Ref<PipelineLayout> finalPipelineLayout = pipelineLayout ? Ref { *pipelineLayout } : generatePipelineLayout(bindGroupEntries);
    if (!pipelineLayout && !finalPipelineLayout->isValid())
        return callback(returnInvalidRenderPipeline(*this, isAsync, "Generated pipeline layout is not valid"_s));

    // The pipeline is created before its shaders have been compiled: it only holds on to
    // mtlRenderPipelineDescriptor, and its MTLRenderPipelineState is installed (or, for the
    // synchronous path, left to be compiled on demand) once Metal is done with the MSL. Everything
    // which reads `descriptor` has to happen here, before that.
    Ref pipeline = RenderPipeline::create(mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthClipMode, depthStencilDescriptor, WTF::move(finalPipelineLayout), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, mtlRenderPipelineDescriptor, colorAttachmentCount, primitiveTopology, stripIndexFormat, descriptor.multisample.count, !!descriptor.fragment, colorTargetFormats(descriptor), descriptor.depthStencil ? std::optional { descriptor.depthStencil->format } : std::nullopt, writesStencil(descriptor), WTF::move(requiredBufferIndices), WTF::move(minimumBufferSizes), ++m_pipelineId, vertexShaderBindingCount, *this);

    auto vertexCompileRequest = libraryCompileRequest(*preparedVertexLibrary);
    std::optional<LibraryCompileRequest> fragmentCompileRequest;
    if (preparedFragmentLibrary)
        fragmentCompileRequest = libraryCompileRequest(*preparedFragmentLibrary);

    CompletionHandler<void(id<MTLLibrary>, id<MTLLibrary>, NSError *)> finishCreation = [protectedThis = protect(*this), isAsync, suppressErrors = m_supressAllErrors, libraryCompilation, label = WTF::move(label), renderPipelineDescriptor = RetainPtr { mtlRenderPipelineDescriptor }, pipeline = WTF::move(pipeline), vertexLibraryInfo = WTF::move(*preparedVertexLibrary), fragmentLibraryInfo = WTF::move(preparedFragmentLibrary), shaderLocations = WTF::move(shaderLocations), callback = WTF::move(callback)](id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, NSError *libraryError) mutable {
        auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);

        if (!vertexLibrary)
            return callback(returnInvalidRenderPipeline(protectedThis, isAsync, libraryError.localizedDescription ?: @"Vertex library failed creation"));

        auto vertexFunction = createFunction(vertexLibrary, vertexLibraryInfo.entryPointInformation, label.get());
        if (!vertexFunction || vertexFunction.functionType != MTLFunctionTypeVertex)
            return callback(returnInvalidRenderPipeline(protectedThis, isAsync, "Vertex function could not be created"_s));
        renderPipelineDescriptor.get().vertexFunction = vertexFunction;

        if (fragmentLibraryInfo) {
            if (!fragmentLibrary)
                return callback(returnInvalidRenderPipeline(protectedThis, isAsync, libraryError.localizedDescription ?: @"Fragment library could not be created"));

            auto fragmentFunction = createFunction(fragmentLibrary, fragmentLibraryInfo->entryPointInformation, label.get());
            if (!fragmentFunction || fragmentFunction.functionType != MTLFunctionTypeFragment)
                return callback(returnInvalidRenderPipeline(protectedThis, isAsync, "Fragment function failed creation"_s));
            renderPipelineDescriptor.get().fragmentFunction = fragmentFunction;
        }

        NSError *error = nil;
#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
        if (dumpMetalReproCaseRenderPSO(WTF::move(vertexLibraryInfo.msl), renderPipelineDescriptor.get().vertexFunction.name, fragmentLibraryInfo ? WTF::move(fragmentLibraryInfo->msl) : String { }, renderPipelineDescriptor.get().fragmentFunction.name, renderPipelineDescriptor.get(), shaderLocations, protectedThis)) {
            renderPipelineDescriptor.get().supportIndirectCommandBuffers = YES;
            id<MTLRenderPipelineState> renderPipelineState = [protectedThis->m_device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor.get() error:&error];
            UNUSED_PARAM(renderPipelineState);
            clearMetalPSORepro();
        }
#else
        UNUSED_PARAM(shaderLocations);
#endif
        if (error)
            return callback(returnInvalidRenderPipeline(protectedThis, isAsync, error.localizedDescription));

        RefPtr protectedInstance = protectedThis->instance();
        if (libraryCompilation == LibraryCompilation::Synchronous || !protectedInstance)
            return callback(std::make_pair(WTF::move(pipeline), nil));

        // Compile the pipeline state now rather than leaving it to the first draw. This is only an
        // optimization: nothing here can invalidate the pipeline, and if the compile fails the lazy
        // renderPipelineState() path still runs, and reports the failure, at first use.
        createRenderPipelineStateAsync(protectedThis, renderPipelineDescriptor.get(), [pipeline = WTF::move(pipeline), callback = WTF::move(callback)](id<MTLRenderPipelineState> renderPipelineState, NSError *) mutable {
            if (renderPipelineState)
                pipeline->setPrecompiledRenderPipelineState(renderPipelineState);
            callback(std::make_pair(WTF::move(pipeline), nil));
        });
    };

    // Compile the fragment MSL once the vertex MSL is done. The two stages of one pipeline are
    // serialized, but independent pipelines still compile concurrently, which is the point.
    compileLibrary(vertexCompileRequest, libraryCompilation, [protectedThis = protect(*this), libraryCompilation, fragmentCompileRequest = WTF::move(fragmentCompileRequest), finishCreation = WTF::move(finishCreation)](id<MTLLibrary> vertexLibrary, NSError *vertexError) mutable {
        if (!vertexLibrary || !fragmentCompileRequest) {
            finishCreation(vertexLibrary, nil, vertexError);
            return;
        }

        protectedThis->compileLibrary(*fragmentCompileRequest, libraryCompilation, [vertexLibrary, finishCreation = WTF::move(finishCreation)](id<MTLLibrary> fragmentLibrary, NSError *fragmentError) mutable {
            finishCreation(vertexLibrary, fragmentLibrary, fragmentError);
        });
    });
}

static CompletionHandler<void(std::pair<Ref<RenderPipeline>, NSString*>&&)> asyncRenderPipelineCompletion(Device& device, CompletionHandler<void(std::expected<Ref<WebGPU::RenderPipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    return [protectedDevice = protect(device), callback = WTF::move(callback)](std::pair<Ref<RenderPipeline>, NSString*>&& pipelineAndError) mutable {
        auto reportResult = [protectedDevice, callback = WTF::move(callback), pipeline = WTF::move(pipelineAndError.first), message = String { pipelineAndError.second }]() mutable {
            // A lost device makes invalid objects without errors.
            if (protectedDevice->isDestroyed() || pipeline->isValid())
                return callback(Ref<WebGPU::RenderPipeline> { WTF::move(pipeline) });
            callback(makeUnexpected(WebGPU::PipelineError { .reason = WebGPU::PipelineErrorReason::Validation, .message = WTF::move(message) }));
        };

        // Resolve on a later turn of the WebGPU thread, never re-entrantly from the caller.
        if (RefPtr protectedInstance = protectedDevice->instance()) {
            protectedInstance->scheduleWork(WTF::move(reportResult));
            return;
        }
        reportResult();
    };
}

void Device::createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, CompletionHandler<void(std::expected<Ref<WebGPU::RenderPipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    createRenderPipeline(descriptor, true, nullptr, asynchronousIfPossible(), asyncRenderPipelineCompletion(*this, WTF::move(callback)));
}

void Device::createRenderPipelineWithPipelineLayoutFromPipelineAsync(const WebGPU::RenderPipelineDescriptor& descriptor, const WebGPU::RenderPipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<WebGPU::RenderPipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    bool wasErrorReportingPaused = pauseErrorReporting(true);
    createRenderPipeline(descriptor, true, &downcast<RenderPipeline>(pipelineToReplace), asynchronousIfPossible(), asyncRenderPipelineCompletion(*this, WTF::move(callback)));
    pauseErrorReporting(wasErrorReportingPaused);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderPipeline);

RenderPipeline::RenderPipeline(MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, MTLDepthClipMode clipMode, MTLDepthStencilDescriptor *depthStencilDescriptor, Ref<PipelineLayout>&& pipelineLayout, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, uint32_t sampleMask, MTLRenderPipelineDescriptor* renderPipelineDescriptor, uint32_t colorAttachmentCount, WebGPU::PrimitiveTopology primitiveTopology, std::optional<WebGPU::IndexFormat> stripIndexFormat, uint32_t sampleCount, bool hasFragment, Vector<std::optional<WebGPU::TextureFormat>>&& colorTargetFormats, std::optional<WebGPU::TextureFormat> depthStencilFormat, bool writesStencil, RequiredBufferIndicesContainer&& requiredBufferIndices, BufferBindingSizesForPipeline&& minimumBufferSizes, uint64_t uniqueId, uint32_t vertexShaderBindingCount, Device& device)
    : m_device(device)
    , m_primitiveType(primitiveType)
    , m_indexType(indexType)
    , m_frontFace(frontFace)
    , m_cullMode(cullMode)
    , m_clipMode(clipMode)
    , m_depthBias(depthBias)
    , m_depthBiasSlopeScale(depthBiasSlopeScale)
    , m_depthBiasClamp(depthBiasClamp)
    , m_sampleMask(sampleMask)
    , m_renderPipelineDescriptor(renderPipelineDescriptor)
    , m_colorAttachmentCount(colorAttachmentCount)
    , m_depthStencilDescriptor(depthStencilDescriptor)
    , m_depthStencilState(depthStencilDescriptor ? [device.device() newDepthStencilStateWithDescriptor:depthStencilDescriptor] : nil)
    , m_requiredBufferIndices(WTF::move(requiredBufferIndices))
    , m_pipelineLayout(WTF::move(pipelineLayout))
    , m_primitiveTopology(primitiveTopology)
    , m_stripIndexFormat(stripIndexFormat)
    , m_sampleCount(sampleCount)
    , m_hasFragment(hasFragment)
    , m_colorTargetFormats(WTF::move(colorTargetFormats))
    , m_depthStencilFormat(depthStencilFormat)
    , m_minimumBufferSizes(minimumBufferSizes)
    , m_uniqueId(uniqueId)
    , m_vertexShaderBindingCount(vertexShaderBindingCount)
    , m_writesStencil(writesStencil)
{
}

RenderPipeline::RenderPipeline(Device& device)
    : m_device(device)
    , m_pipelineLayout(PipelineLayout::createInvalid(device))
    , m_minimumBufferSizes({ })
{
}

RenderPipeline::~RenderPipeline() = default;

Ref<WebGPU::BindGroupLayout> RenderPipeline::getBindGroupLayout(uint32_t groupIndex)
{
    auto pipelineLayout = m_pipelineLayout;
    auto device = m_device;

    if (!isValid()) {
        device->generateAValidationError("getBindGroupLayout: RenderPipeline is invalid"_s);
        pipelineLayout->makeInvalid();
        return BindGroupLayout::createInvalid(device.get());
    }

    if (groupIndex >= pipelineLayout->numberOfBindGroupLayouts()) {
        if (groupIndex >= device->limits().maxBindGroups) {
            device->generateAValidationError("getBindGroupLayout: groupIndex is out of range"_s);
            pipelineLayout->makeInvalid();
        }
        return BindGroupLayout::createInvalid(device.get());
    }

    return pipelineLayout->bindGroupLayout(groupIndex);
}

void RenderPipeline::setLabel(String&&)
{
    // MTLRenderPipelineState's labels are read-only.
}

id<MTLDepthStencilState> RenderPipeline::depthStencilState() const
{
    return m_depthStencilState;
}

bool RenderPipeline::writesDepth() const
{
    return m_depthStencilDescriptor.depthWriteEnabled;
}

bool RenderPipeline::writesStencil() const
{
    return m_writesStencil;
}

bool RenderPipeline::validateDepthStencilState(bool depthReadOnly, bool stencilReadOnly) const
{
    if (depthReadOnly && writesDepth())
        return false;

    if (stencilReadOnly && writesStencil())
        return false;

    return true;
}

NSString* RenderPipeline::errorValidatingColorDepthStencilTargets(const Vector<TextureOrTextureView>& colorAttachmentViews, const std::optional<TextureOrTextureView>& depthStencilView) const
{
    if (!m_hasFragment) {
        if (colorAttachmentViews.size())
            return @"No fragment shader but render pass has color attachments";
    } else {
        for (size_t i = 0, maxCount = std::max<size_t>(m_colorTargetFormats.size(), colorAttachmentViews.size()); i < maxCount; ++i) {
            auto* attachmentView = i < colorAttachmentViews.size() ? &colorAttachmentViews[i] : nullptr;
            auto descriptorTargetFormat = i < m_colorTargetFormats.size() ? m_colorTargetFormats[i] : std::nullopt;
            if (!attachmentView || !*attachmentView) {
                if (!descriptorTargetFormat)
                    continue;
                return [NSString stringWithFormat:@"No attachment view but descriptorTargetFormat(%s)", Texture::formatToString(*descriptorTargetFormat).characters()];
            }
            if (descriptorTargetFormat != attachmentView->format())
                return [NSString stringWithFormat:@"descriptorTargetFormat(%s) != attachmentView->format(%s)", descriptorTargetFormat ? Texture::formatToString(*descriptorTargetFormat).characters() : "undefined", Texture::formatToString(attachmentView->format()).characters()];
            if (attachmentView->sampleCount() != m_sampleCount)
                return [NSString stringWithFormat:@"attachmentView->sampleCount(%d) != m_sampleCount(%d)", attachmentView->sampleCount(), m_sampleCount];
        }
    }

    if (!m_depthStencilFormat) {
        if (!depthStencilView)
            return nil;

        return @"depthStencil is missing but render pass has a depth stencil attachment";
    }

    if (depthStencilView) {
        if (!*depthStencilView)
            return @"depthStencilAttachment exists but no depthStencilView";
        auto& texture = *depthStencilView;
        if (texture.format() != *m_depthStencilFormat)
            return [NSString stringWithFormat:@"texture.format(%s) != m_depthStencilFormat(%s)", Texture::formatToString(texture.format()).characters(), Texture::formatToString(*m_depthStencilFormat).characters()];
        auto mtlPixelFormat = texture.texture().pixelFormat;
        auto descriptorFormat = *m_depthStencilFormat;
        if (mtlPixelFormat == MTLPixelFormatX32_Stencil8 && descriptorFormat == WebGPU::TextureFormat::Stencil8)
            return @"mtlPixelFormat == MTLPixelFormatX32_Stencil8 && descriptorFormat == WebGPU::TextureFormat::Stencil8";
        if (mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8 && (descriptorFormat == WebGPU::TextureFormat::Depth32float || descriptorFormat == WebGPU::TextureFormat::Depth24plus))
            return @"mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8 && (descriptorFormat == WebGPU::TextureFormat::Depth32float || descriptorFormat == WebGPU::TextureFormat::Depth24plus)";
        if (texture.sampleCount() != m_sampleCount)
            return [NSString stringWithFormat:@"texture.sampleCount(%d) != m_sampleCount(%d)", texture.sampleCount(), m_sampleCount];
    } else
        return [NSString stringWithFormat:@"m_depthStencilFormat(%s) but render pass has no depth stencil attachment", Texture::formatToString(*m_depthStencilFormat).characters()];

    return nil;
}

bool RenderPipeline::validateRenderBundle(bool depthReadOnly, bool stencilReadOnly, uint32_t sampleCount, std::span<const std::optional<WebGPU::TextureFormat>> colorFormats, std::optional<WebGPU::TextureFormat> depthStencilFormat) const
{
    if (!validateDepthStencilState(depthReadOnly, stencilReadOnly))
        return false;

    if (sampleCount != m_sampleCount)
        return false;

    for (size_t i = 0, maxTargetCount = std::max<size_t>(m_colorTargetFormats.size(), colorFormats.size()); i < maxTargetCount; ++i) {
        auto colorFormat = i < colorFormats.size() ? colorFormats[i] : std::nullopt;
        auto descriptorFormat = i < m_colorTargetFormats.size() ? m_colorTargetFormats[i] : std::nullopt;
        if (descriptorFormat != colorFormat)
            return false;
    }

    if (!m_depthStencilFormat)
        return !depthStencilFormat;

    if (depthStencilFormat != *m_depthStencilFormat)
        return false;

    return true;
}

const BufferBindingSizesForBindGroup* RenderPipeline::minimumBufferSizes(uint32_t index) const
{
    auto it = m_minimumBufferSizes.find(index);
    return it == m_minimumBufferSizes.end() ? nullptr : &it->value;
}

RefPtr<RenderPipeline> RenderPipeline::recomputeLastStrideAsStride() const
{
    if (m_lastStrideAsStridePipeline)
        return m_lastStrideAsStridePipeline;

    MTLRenderPipelineDescriptor* clonedRenderPipelineDescriptor = [m_renderPipelineDescriptor copy];

    auto requiredBufferIndices = m_requiredBufferIndices;
    if (auto* vertexDescriptor = clonedRenderPipelineDescriptor.vertexDescriptor) {
        for (auto& [bufferIndex, bufferData] : requiredBufferIndices) {
            vertexDescriptor.layouts[bufferIndex].stride = bufferData.lastStride;
            bufferData.stride = bufferData.lastStride;
        }
    }

    auto minimumBufferSizes = m_minimumBufferSizes;
    m_lastStrideAsStridePipeline = RenderPipeline::create(m_primitiveType, m_indexType, m_frontFace, m_cullMode, m_clipMode, m_depthStencilDescriptor, m_pipelineLayout.copyRef(), m_depthBias, m_depthBiasSlopeScale, m_depthBiasClamp, m_sampleMask, clonedRenderPipelineDescriptor, m_colorAttachmentCount, m_primitiveTopology, m_stripIndexFormat, m_sampleCount, m_hasFragment, Vector { m_colorTargetFormats }, m_depthStencilFormat, m_writesStencil, WTF::move(requiredBufferIndices), WTF::move(minimumBufferSizes), m_uniqueId, m_vertexShaderBindingCount, m_device);

    return m_lastStrideAsStridePipeline;
}

id<MTLRenderPipelineState> RenderPipeline::renderPipelineState() const
{
    if (m_renderPipelineState)
        return m_renderPipelineState;

    m_renderPipelineState = [m_device->device() newRenderPipelineStateWithDescriptor:m_renderPipelineDescriptor error:nil];
    if (!m_renderPipelineState)
        m_device->generateAnOutOfMemoryError("Render pipeline failed compilation likely due to being too complex, please reduce its size"_s);

    return m_renderPipelineState;
}

void RenderPipeline::setPrecompiledRenderPipelineState(id<MTLRenderPipelineState> renderPipelineState)
{
    ASSERT(!m_renderPipelineState);
    m_renderPipelineState = renderPipelineState;
}

id<MTLRenderPipelineState> RenderPipeline::icbRenderPipelineState() const
{
    if (m_renderPipelineState && m_renderPipelineDescriptor.supportIndirectCommandBuffers)
        return m_renderPipelineState;

    m_renderPipelineDescriptor.supportIndirectCommandBuffers = YES;
    m_renderPipelineState = [m_device->device() newRenderPipelineStateWithDescriptor:m_renderPipelineDescriptor error:nil];
    if (!m_renderPipelineState)
        m_device->generateAnOutOfMemoryError("Render pipeline failed compilation likely due to being too complex, please reduce its size"_s);

    return m_renderPipelineState;
}

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs

void LEGACY_NODELETE wgpuRenderPipelineAddRef(WGPURenderPipeline renderPipeline)
{
    WebGPU::Metal::fromAPI(renderPipeline).ref();
}

void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline)
{
    WebGPU::Metal::fromAPI(renderPipeline).deref();
}

WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex)
{
    // Every WebGPU::BindGroupLayout that a WebGPU::Metal::RenderPipeline returns is a WebGPU::Metal::BindGroupLayout.
    Ref bindGroupLayout = downcast<WebGPU::Metal::BindGroupLayout>(protect(WebGPU::Metal::fromAPI(renderPipeline))->getBindGroupLayout(groupIndex).get());
    return WebGPU::Metal::releaseToAPI(WTF::move(bindGroupLayout));
}

void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, WGPUStringView label)
{
    WebGPU::Metal::fromAPI(renderPipeline).setLabel(WebGPU::Metal::fromAPI(label));
}

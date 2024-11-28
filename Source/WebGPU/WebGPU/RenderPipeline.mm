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
#import "Pipeline.h"
#import "RenderBundleEncoder.h"
#import "WGSLShaderModule.h"
#import <wtf/IndexedRange.h>
#import <wtf/TZoneMallocInlines.h>

// FIXME: remove after radar://104903411 or after we place the mask into the last buffer
@interface NSObject ()
- (void)setSampleMask:(NSUInteger)mask;
@end

namespace WebGPU {

static MTLBlendOperation blendOperation(WGPUBlendOperation operation)
{
    switch (operation) {
    case WGPUBlendOperation_Add:
        return MTLBlendOperationAdd;
    case WGPUBlendOperation_Max:
        return MTLBlendOperationMax;
    case WGPUBlendOperation_Min:
        return MTLBlendOperationMin;
    case WGPUBlendOperation_ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    case WGPUBlendOperation_Subtract:
        return MTLBlendOperationSubtract;
    case WGPUBlendOperation_Force32:
        ASSERT_NOT_REACHED();
        return MTLBlendOperationAdd;
    }
}

static MTLBlendFactor blendFactor(WGPUBlendFactor factor)
{
    switch (factor) {
    case WGPUBlendFactor_Constant:
        return MTLBlendFactorBlendColor;
    case WGPUBlendFactor_Dst:
        return MTLBlendFactorDestinationColor;
    case WGPUBlendFactor_DstAlpha:
        return MTLBlendFactorDestinationAlpha;
    case WGPUBlendFactor_One:
        return MTLBlendFactorOne;
    case WGPUBlendFactor_OneMinusConstant:
        return MTLBlendFactorOneMinusBlendColor;
    case WGPUBlendFactor_OneMinusDst:
        return MTLBlendFactorOneMinusDestinationColor;
    case WGPUBlendFactor_OneMinusDstAlpha:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case WGPUBlendFactor_OneMinusSrc:
        return MTLBlendFactorOneMinusSourceColor;
    case WGPUBlendFactor_Src:
        return MTLBlendFactorSourceColor;
    case WGPUBlendFactor_OneMinusSrcAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    case WGPUBlendFactor_Zero:
        return MTLBlendFactorZero;
    case WGPUBlendFactor_SrcAlpha:
        return MTLBlendFactorSourceAlpha;
    case WGPUBlendFactor_SrcAlphaSaturated:
        return MTLBlendFactorSourceAlphaSaturated;
    case WGPUBlendFactor_Force32:
        ASSERT_NOT_REACHED();
        return MTLBlendFactorOne;
    }
}

static MTLColorWriteMask colorWriteMask(WGPUColorWriteMaskFlags mask)
{
    MTLColorWriteMask mtlMask = MTLColorWriteMaskNone;

    if (mask & WGPUColorWriteMask_Red)
        mtlMask |= MTLColorWriteMaskRed;
    if (mask & WGPUColorWriteMask_Green)
        mtlMask |= MTLColorWriteMaskGreen;
    if (mask & WGPUColorWriteMask_Blue)
        mtlMask |= MTLColorWriteMaskBlue;
    if (mask & WGPUColorWriteMask_Alpha)
        mtlMask |= MTLColorWriteMaskAlpha;

    return mtlMask;
}

static MTLWinding frontFace(WGPUFrontFace frontFace)
{
    switch (frontFace) {
    case WGPUFrontFace_CW:
        return MTLWindingClockwise;
    case WGPUFrontFace_CCW:
        return MTLWindingCounterClockwise;
    case WGPUFrontFace_Force32:
        ASSERT_NOT_REACHED();
        return MTLWindingClockwise;
    }
}

static MTLCullMode cullMode(WGPUCullMode cullMode)
{
    switch (cullMode) {
    case WGPUCullMode_None:
        return MTLCullModeNone;
    case WGPUCullMode_Front:
        return MTLCullModeFront;
    case WGPUCullMode_Back:
        return MTLCullModeBack;
    case WGPUCullMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLCullModeNone;
    }
}

static MTLPrimitiveType primitiveType(WGPUPrimitiveTopology topology)
{
    switch (topology) {
    case WGPUPrimitiveTopology_PointList:
        return MTLPrimitiveTypePoint;
    case WGPUPrimitiveTopology_LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case WGPUPrimitiveTopology_TriangleList:
        return MTLPrimitiveTypeTriangle;
    case WGPUPrimitiveTopology_LineList:
        return MTLPrimitiveTypeLine;
    case WGPUPrimitiveTopology_TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    case WGPUPrimitiveTopology_Force32:
        ASSERT_NOT_REACHED();
        return MTLPrimitiveTypeTriangle;
    }
}

static MTLPrimitiveTopologyClass topologyType(WGPUPrimitiveTopology topology)
{
    switch (topology) {
    case WGPUPrimitiveTopology_PointList:
        return MTLPrimitiveTopologyClassPoint;
    case WGPUPrimitiveTopology_LineStrip:
    case WGPUPrimitiveTopology_LineList:
        return MTLPrimitiveTopologyClassLine;
    case WGPUPrimitiveTopology_TriangleList:
    case WGPUPrimitiveTopology_TriangleStrip:
        return MTLPrimitiveTopologyClassTriangle;
    case WGPUPrimitiveTopology_Force32:
        ASSERT_NOT_REACHED();
        return MTLPrimitiveTopologyClassTriangle;
    }
}

static std::optional<MTLIndexType> indexType(WGPUIndexFormat format)
{
    switch (format) {
    case WGPUIndexFormat_Uint16:
        return MTLIndexTypeUInt16;
    case WGPUIndexFormat_Uint32:
        return MTLIndexTypeUInt32;
    case WGPUIndexFormat_Undefined:
        return { };
    case WGPUIndexFormat_Force32:
        ASSERT_NOT_REACHED();
        return { };
    }
}

bool Device::validateRenderPipeline(const WGPURenderPipelineDescriptor& descriptor)
{
    // FIXME: Implement this according to the description in
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpurenderpipelinedescriptor

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        if (fragmentDescriptor.targetCount > limits().maxColorAttachments)
            return false;
    }

    return true;
}

static MTLStencilOperation convertToMTLStencilOperation(WGPUStencilOperation operation)
{
    switch (operation) {
    case WGPUStencilOperation_Keep:
        return MTLStencilOperationKeep;
    case WGPUStencilOperation_Zero:
        return MTLStencilOperationZero;
    case WGPUStencilOperation_Replace:
        return MTLStencilOperationReplace;
    case WGPUStencilOperation_Invert:
        return MTLStencilOperationInvert;
    case WGPUStencilOperation_IncrementClamp:
        return MTLStencilOperationIncrementClamp;
    case WGPUStencilOperation_DecrementClamp:
        return MTLStencilOperationDecrementClamp;
    case WGPUStencilOperation_IncrementWrap:
        return MTLStencilOperationIncrementWrap;
    case WGPUStencilOperation_DecrementWrap:
        return MTLStencilOperationDecrementWrap;
    case WGPUStencilOperation_Force32:
        ASSERT_NOT_REACHED();
        return MTLStencilOperationZero;
    }
}

static MTLCompareFunction convertToMTLCompare(WGPUCompareFunction comparison)
{
    switch (comparison) {
    case WGPUCompareFunction_Never:
        return MTLCompareFunctionNever;
    case WGPUCompareFunction_Less:
        return MTLCompareFunctionLess;
    case WGPUCompareFunction_LessEqual:
        return MTLCompareFunctionLessEqual;
    case WGPUCompareFunction_Greater:
        return MTLCompareFunctionGreater;
    case WGPUCompareFunction_GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case WGPUCompareFunction_Equal:
        return MTLCompareFunctionEqual;
    case WGPUCompareFunction_NotEqual:
        return MTLCompareFunctionNotEqual;
    case WGPUCompareFunction_Undefined:
    case WGPUCompareFunction_Always:
    case WGPUCompareFunction_Force32:
        return MTLCompareFunctionAlways;
    }
}

static MTLVertexFormat vertexFormat(WGPUVertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case WGPUVertexFormat_Uint8x2:
        return MTLVertexFormatUChar2;
    case WGPUVertexFormat_Uint8x4:
        return MTLVertexFormatUChar4;
    case WGPUVertexFormat_Sint8x2:
        return MTLVertexFormatChar2;
    case WGPUVertexFormat_Sint8x4:
        return MTLVertexFormatChar4;
    case WGPUVertexFormat_Unorm8x2:
        return MTLVertexFormatUChar2Normalized;
    case WGPUVertexFormat_Unorm8x4:
        return MTLVertexFormatUChar4Normalized;
    case WGPUVertexFormat_Snorm8x2:
        return MTLVertexFormatChar2Normalized;
    case WGPUVertexFormat_Snorm8x4:
        return MTLVertexFormatChar4Normalized;
    case WGPUVertexFormat_Uint16x2:
        return MTLVertexFormatUShort2;
    case WGPUVertexFormat_Uint16x4:
        return MTLVertexFormatUShort4;
    case WGPUVertexFormat_Sint16x2:
        return MTLVertexFormatShort2;
    case WGPUVertexFormat_Sint16x4:
        return MTLVertexFormatShort4;
    case WGPUVertexFormat_Unorm16x2:
        return MTLVertexFormatUShort2Normalized;
    case WGPUVertexFormat_Unorm16x4:
        return MTLVertexFormatUShort4Normalized;
    case WGPUVertexFormat_Snorm16x2:
        return MTLVertexFormatShort2Normalized;
    case WGPUVertexFormat_Snorm16x4:
        return MTLVertexFormatShort4Normalized;
    case WGPUVertexFormat_Float16x2:
        return MTLVertexFormatHalf2;
    case WGPUVertexFormat_Float16x4:
        return MTLVertexFormatHalf4;
    case WGPUVertexFormat_Float32:
        return MTLVertexFormatFloat;
    case WGPUVertexFormat_Float32x2:
        return MTLVertexFormatFloat2;
    case WGPUVertexFormat_Float32x3:
        return MTLVertexFormatFloat3;
    case WGPUVertexFormat_Float32x4:
        return MTLVertexFormatFloat4;
    case WGPUVertexFormat_Uint32:
        return MTLVertexFormatUInt;
    case WGPUVertexFormat_Uint32x2:
        return MTLVertexFormatUInt2;
    case WGPUVertexFormat_Uint32x3:
        return MTLVertexFormatUInt3;
    case WGPUVertexFormat_Uint32x4:
        return MTLVertexFormatUInt4;
    case WGPUVertexFormat_Sint32:
        return MTLVertexFormatInt;
    case WGPUVertexFormat_Sint32x2:
        return MTLVertexFormatInt2;
    case WGPUVertexFormat_Sint32x3:
        return MTLVertexFormatInt3;
    case WGPUVertexFormat_Sint32x4:
        return MTLVertexFormatInt4;
    case WGPUVertexFormat_Unorm10_10_10_2:
        return MTLVertexFormatUInt1010102Normalized;
    case WGPUVertexFormat_Force32:
    case WGPUVertexFormat_Undefined:
        ASSERT_NOT_REACHED();
        return MTLVertexFormatFloat;
    }
}

static size_t vertexFormatSize(WGPUVertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case WGPUVertexFormat_Uint8x2:
        return 2;
    case WGPUVertexFormat_Uint8x4:
        return 4;
    case WGPUVertexFormat_Sint8x2:
        return 2;
    case WGPUVertexFormat_Sint8x4:
        return 4;
    case WGPUVertexFormat_Unorm8x2:
        return 2;
    case WGPUVertexFormat_Unorm8x4:
        return 4;
    case WGPUVertexFormat_Snorm8x2:
        return 2;
    case WGPUVertexFormat_Snorm8x4:
        return 4;
    case WGPUVertexFormat_Uint16x2:
        return 4;
    case WGPUVertexFormat_Uint16x4:
        return 8;
    case WGPUVertexFormat_Sint16x2:
        return 4;
    case WGPUVertexFormat_Sint16x4:
        return 8;
    case WGPUVertexFormat_Unorm16x2:
        return 4;
    case WGPUVertexFormat_Unorm16x4:
        return 8;
    case WGPUVertexFormat_Snorm16x2:
        return 4;
    case WGPUVertexFormat_Snorm16x4:
        return 8;
    case WGPUVertexFormat_Float16x2:
        return 4;
    case WGPUVertexFormat_Float16x4:
        return 8;
    case WGPUVertexFormat_Float32:
        return 4;
    case WGPUVertexFormat_Float32x2:
        return 8;
    case WGPUVertexFormat_Float32x3:
        return 12;
    case WGPUVertexFormat_Float32x4:
        return 16;
    case WGPUVertexFormat_Uint32:
        return 4;
    case WGPUVertexFormat_Uint32x2:
        return 8;
    case WGPUVertexFormat_Uint32x3:
        return 12;
    case WGPUVertexFormat_Uint32x4:
        return 16;
    case WGPUVertexFormat_Sint32:
        return 4;
    case WGPUVertexFormat_Sint32x2:
        return 8;
    case WGPUVertexFormat_Sint32x3:
        return 12;
    case WGPUVertexFormat_Sint32x4:
        return 16;
    case WGPUVertexFormat_Unorm10_10_10_2:
        return 4;
    case WGPUVertexFormat_Force32:
    case WGPUVertexFormat_Undefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static MTLVertexStepFunction stepFunction(WGPUVertexStepMode stepMode, auto arrayStride)
{
    if (!arrayStride)
        return MTLVertexStepFunctionConstant;

    switch (stepMode) {
    case WGPUVertexStepMode_Vertex:
        return MTLVertexStepFunctionPerVertex;
    case WGPUVertexStepMode_Instance:
        return MTLVertexStepFunctionPerInstance;
    case WGPUVertexStepMode_VertexBufferNotUsed:
        return MTLVertexStepFunctionConstant;
    case WGPUVertexStepMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLVertexStepFunctionPerVertex;
    }
}

static ASCIILiteral name(WGPUVertexFormat format)
{
    switch (format) {
    case WGPUVertexFormat_Uint8x2:
        return "UChar2"_s;
    case WGPUVertexFormat_Uint8x4:
        return "UChar4"_s;
    case WGPUVertexFormat_Sint8x2:
        return "Char2"_s;
    case WGPUVertexFormat_Sint8x4:
        return "Char4"_s;
    case WGPUVertexFormat_Unorm8x2:
        return "UChar2Normalized"_s;
    case WGPUVertexFormat_Unorm8x4:
        return "UChar4Normalized"_s;
    case WGPUVertexFormat_Snorm8x2:
        return "Char2Normalized"_s;
    case WGPUVertexFormat_Snorm8x4:
        return "Char4Normalized"_s;
    case WGPUVertexFormat_Uint16x2:
        return "UShort2"_s;
    case WGPUVertexFormat_Uint16x4:
        return "UShort4"_s;
    case WGPUVertexFormat_Sint16x2:
        return "Short2"_s;
    case WGPUVertexFormat_Sint16x4:
        return "Short4"_s;
    case WGPUVertexFormat_Unorm16x2:
        return "UShort2Normalized"_s;
    case WGPUVertexFormat_Unorm16x4:
        return "UShort4Normalized"_s;
    case WGPUVertexFormat_Snorm16x2:
        return "Short2Normalized"_s;
    case WGPUVertexFormat_Snorm16x4:
        return "Short4Normalized"_s;
    case WGPUVertexFormat_Float16x2:
        return "Half2"_s;
    case WGPUVertexFormat_Float16x4:
        return "Half4"_s;
    case WGPUVertexFormat_Float32:
        return "Float"_s;
    case WGPUVertexFormat_Float32x2:
        return "Float2"_s;
    case WGPUVertexFormat_Float32x3:
        return "Float3"_s;
    case WGPUVertexFormat_Float32x4:
        return "Float4"_s;
    case WGPUVertexFormat_Uint32:
        return "UInt"_s;
    case WGPUVertexFormat_Uint32x2:
        return "UInt2"_s;
    case WGPUVertexFormat_Uint32x3:
        return "UInt3"_s;
    case WGPUVertexFormat_Uint32x4:
        return "UInt4"_s;
    case WGPUVertexFormat_Sint32:
        return "Int"_s;
    case WGPUVertexFormat_Sint32x2:
        return "Int2"_s;
    case WGPUVertexFormat_Sint32x3:
        return "Int3"_s;
    case WGPUVertexFormat_Sint32x4:
        return "Int4"_s;
    case WGPUVertexFormat_Unorm10_10_10_2:
        return "UInt1010102Normalized"_s;
    case WGPUVertexFormat_Force32:
    case WGPUVertexFormat_Undefined:
        ASSERT_NOT_REACHED();
        return "none"_s;
    }
}

enum class WGPUVertexFormatType {
    Undefined,
    SignedInt,
    UnsignedInt,
    Float
};

static constexpr WGPUVertexFormatType formatType(WGPUVertexFormat format)
{
    switch (format) {
    case WGPUVertexFormat_Uint8x2:
    case WGPUVertexFormat_Uint8x4:
    case WGPUVertexFormat_Uint16x2:
    case WGPUVertexFormat_Uint16x4:
    case WGPUVertexFormat_Uint32:
    case WGPUVertexFormat_Uint32x2:
    case WGPUVertexFormat_Uint32x3:
    case WGPUVertexFormat_Uint32x4:
        return WGPUVertexFormatType::UnsignedInt;

    case WGPUVertexFormat_Sint8x2:
    case WGPUVertexFormat_Sint8x4:
    case WGPUVertexFormat_Sint16x2:
    case WGPUVertexFormat_Sint16x4:
    case WGPUVertexFormat_Sint32:
    case WGPUVertexFormat_Sint32x2:
    case WGPUVertexFormat_Sint32x3:
    case WGPUVertexFormat_Sint32x4:
        return WGPUVertexFormatType::SignedInt;

    case WGPUVertexFormat_Unorm8x2:
    case WGPUVertexFormat_Unorm8x4:
    case WGPUVertexFormat_Snorm8x2:
    case WGPUVertexFormat_Snorm8x4:
    case WGPUVertexFormat_Unorm16x2:
    case WGPUVertexFormat_Unorm16x4:
    case WGPUVertexFormat_Snorm16x2:
    case WGPUVertexFormat_Snorm16x4:
    case WGPUVertexFormat_Float16x2:
    case WGPUVertexFormat_Float16x4:
    case WGPUVertexFormat_Float32:
    case WGPUVertexFormat_Float32x2:
    case WGPUVertexFormat_Float32x3:
    case WGPUVertexFormat_Float32x4:
    case WGPUVertexFormat_Unorm10_10_10_2:
        return WGPUVertexFormatType::Float;

    case WGPUVertexFormat_Force32:
        RELEASE_ASSERT_NOT_REACHED();
    case WGPUVertexFormat_Undefined:
        return WGPUVertexFormatType::Undefined;
    }
}

static bool matchesFormat(const ShaderModule::VertexStageIn& stageIn, uint32_t shaderLocation, WGPUVertexFormat format)
{
    auto it = stageIn.find(shaderLocation);
    if (it == stageIn.end())
        return false;

    return formatType(it->value) == formatType(format);
}

static MTLVertexDescriptor *createVertexDescriptor(WGPUVertexState vertexState, const WGPULimits& limits, const ShaderModule::VertexStageIn& stageIn, RenderPipeline::RequiredBufferIndicesContainer& requiredBufferIndices, NSString** error)
{
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];
    Checked<uint32_t> totalAttributeCount = 0;
    ASSERT(error);

    if (vertexState.bufferCount > limits.maxVertexBuffers) {
        *error = [NSString stringWithFormat:@"vertexBuffer count(%zu) exceeds limit(%u)", vertexState.bufferCount, limits.maxVertexBuffers];
        return nil;
    }

    ShaderModule::VertexStageIn shaderLocations;
    for (auto [ bufferIndex, buffer ] : indexedRange(vertexState.buffersSpan())) {
        if (buffer.arrayStride == WGPU_COPY_STRIDE_UNDEFINED)
            continue;

        if (buffer.arrayStride > limits.maxVertexBufferArrayStride || (buffer.arrayStride % 4)) {
            *error = [NSString stringWithFormat:@"buffer.arrayStride(%llu) > limits.maxVertexBufferArrayStride(%u) || (buffer.arrayStride %llu)", buffer.arrayStride, limits.maxVertexBufferArrayStride, buffer.arrayStride];
            return nil;
        }

        if (!buffer.attributeCount)
            continue;

        totalAttributeCount = checkedSum<uint32_t>(totalAttributeCount, buffer.attributeCount);
        if (totalAttributeCount.hasOverflowed()) {
            *error = @"Over 2^32 - 1 attributes in the vertex descriptor, failing due to out-of-memory.";
            return nil;
        }

        auto stride = std::max<NSUInteger>(sizeof(int), buffer.arrayStride);
        RELEASE_ASSERT(!requiredBufferIndices.contains(bufferIndex));
        ASSERT(bufferIndex <= std::numeric_limits<uint32_t>::max() && stride <= std::numeric_limits<uint32_t>::max());

        uint64_t lastStride = 0;
        vertexDescriptor.layouts[bufferIndex].stride = stride;
        vertexDescriptor.layouts[bufferIndex].stepFunction = stepFunction(buffer.stepMode, buffer.arrayStride);
        if (vertexDescriptor.layouts[bufferIndex].stepFunction == MTLVertexStepFunctionConstant)
            vertexDescriptor.layouts[bufferIndex].stepRate = 0;
        for (auto& attribute : buffer.attributesSpan()) {
            auto formatSize = vertexFormatSize(attribute.format);
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
                *error = @"attribute.offset + formatSize > buffer.arrayStride";
                return nil;
            }

            if (attribute.offset % std::min<size_t>(4, formatSize)) {
                *error = @"attribute.offset + formatSize > buffer.arrayStride";
                return nil;
            }

            auto shaderLocation = attribute.shaderLocation;
            if (shaderLocation >= limits.maxVertexAttributes || shaderLocations.contains(shaderLocation)) {
                *error = [NSString stringWithFormat:@"shaderLocation(%u) >= limits.maxVertexAttributes(%u) || shaderLocations.contains(shaderLocation) %d", shaderLocation, limits.maxVertexAttributes, shaderLocations.contains(shaderLocation)];
                return nil;
            }

            shaderLocations.add(shaderLocation, attribute.format);
            const auto& mtlAttribute = vertexDescriptor.attributes[shaderLocation];
            mtlAttribute.format = vertexFormat(attribute.format);
            mtlAttribute.bufferIndex = bufferIndex;
            mtlAttribute.offset = attribute.offset;
        }

        ASSERT(!requiredBufferIndices.contains(bufferIndex));
        requiredBufferIndices.add(static_cast<uint32_t>(bufferIndex), RenderPipeline::BufferData {
            .stride = buffer.arrayStride,
            .lastStride = lastStride,
            .stepMode = buffer.stepMode
        });
    }

    for (auto& [shaderLocation, attributeFormat] : stageIn) {
        auto formatSize = vertexFormatSize(attributeFormat);
        if (!matchesFormat(shaderLocations, shaderLocation, attributeFormat)) {
            auto it = stageIn.find(shaderLocation);
            WGPUVertexFormat otherFormat = WGPUVertexFormat_Undefined;
            if (it != stageIn.end())
                otherFormat = it->value;
            *error = [NSString stringWithFormat:@"!matchesFormat(attribute(%d), format(%s), size(%zu), otherFormat(%d)", shaderLocation, name(attributeFormat).characters(), formatSize, otherFormat];
            return nil;
        }
    }

    if (totalAttributeCount.value() > limits.maxVertexAttributes) {
        *error = @"totalAttributeCount > limits.maxVertexAttributes";
        return nil;
    }

    return vertexDescriptor;
}

static void populateStencilOperation(MTLStencilDescriptor *mtlStencil, const WGPUStencilFaceState& stencil, uint32_t stencilReadMask, uint32_t stencilWriteMask)
{
    mtlStencil.stencilCompareFunction =  convertToMTLCompare(stencil.compare);
    mtlStencil.stencilFailureOperation = convertToMTLStencilOperation(stencil.failOp);
    mtlStencil.depthFailureOperation = convertToMTLStencilOperation(stencil.depthFailOp);
    mtlStencil.depthStencilPassOperation = convertToMTLStencilOperation(stencil.passOp);
    mtlStencil.writeMask = stencilWriteMask;
    mtlStencil.readMask = stencilReadMask;
}

static WGPUBufferBindingType convertBindingType(WGSL::BufferBindingType bindingType)
{
    switch (bindingType) {
    case WGSL::BufferBindingType::Uniform:
        return WGPUBufferBindingType_Uniform;
    case WGSL::BufferBindingType::Storage:
        return WGPUBufferBindingType_Storage;
    case WGSL::BufferBindingType::ReadOnlyStorage:
        return WGPUBufferBindingType_ReadOnlyStorage;
    }
}

static WGPUSamplerBindingType convertSamplerBindingType(WGSL::SamplerBindingType samplerType)
{
    switch (samplerType) {
    case WGSL::SamplerBindingType::Filtering:
        return WGPUSamplerBindingType_Filtering;
    case WGSL::SamplerBindingType::NonFiltering:
        return WGPUSamplerBindingType_NonFiltering;
    case WGSL::SamplerBindingType::Comparison:
        return WGPUSamplerBindingType_Comparison;
    }
}

static WGPUShaderStageFlags convertVisibility(const OptionSet<WGSL::ShaderStage>& visibility)
{
    WGPUShaderStageFlags flags = 0;
    if (visibility & WGSL::ShaderStage::Vertex)
        flags |= WGPUShaderStage_Vertex;
    if (visibility & WGSL::ShaderStage::Fragment)
        flags |= WGPUShaderStage_Fragment;
    if (visibility & WGSL::ShaderStage::Compute)
        flags |= WGPUShaderStage_Compute;

    return flags;
}

static WGPUTextureSampleType convertSampleType(WGSL::TextureSampleType sampleType)
{
    switch (sampleType) {
    case WGSL::TextureSampleType::Float:
        return WGPUTextureSampleType_Float;
    case WGSL::TextureSampleType::UnfilterableFloat:
        return WGPUTextureSampleType_UnfilterableFloat;
    case WGSL::TextureSampleType::Depth:
        return WGPUTextureSampleType_Depth;
    case WGSL::TextureSampleType::SignedInt:
        return WGPUTextureSampleType_Sint;
    case WGSL::TextureSampleType::UnsignedInt:
        return WGPUTextureSampleType_Uint;
    }
}

static WGPUTextureViewDimension convertViewDimension(WGSL::TextureViewDimension viewDimension)
{
    switch (viewDimension) {
    case WGSL::TextureViewDimension::OneDimensional:
        return WGPUTextureViewDimension_1D;
    case WGSL::TextureViewDimension::TwoDimensional:
        return WGPUTextureViewDimension_2D;
    case WGSL::TextureViewDimension::TwoDimensionalArray:
        return WGPUTextureViewDimension_2DArray;
    case WGSL::TextureViewDimension::Cube:
        return WGPUTextureViewDimension_Cube;
    case WGSL::TextureViewDimension::CubeArray:
        return WGPUTextureViewDimension_CubeArray;
    case WGSL::TextureViewDimension::ThreeDimensional:
        return WGPUTextureViewDimension_3D;
    }
}

static WGPUStorageTextureAccess convertAccess(WGSL::StorageTextureAccess access)
{
    switch (access) {
    case WGSL::StorageTextureAccess::WriteOnly:
        return WGPUStorageTextureAccess_WriteOnly;
    case WGSL::StorageTextureAccess::ReadOnly:
        return WGPUStorageTextureAccess_ReadOnly;
    case WGSL::StorageTextureAccess::ReadWrite:
        return WGPUStorageTextureAccess_ReadWrite;
    }
}

static WGPUTextureFormat convertFormat(WGSL::TexelFormat format)
{
    switch (format) {
    case WGSL::TexelFormat::BGRA8unorm:
        return WGPUTextureFormat_BGRA8Unorm;
    case WGSL::TexelFormat::R32float:
        return WGPUTextureFormat_R32Float;
    case WGSL::TexelFormat::R32sint:
        return WGPUTextureFormat_R32Sint;
    case WGSL::TexelFormat::R32uint:
        return WGPUTextureFormat_R32Uint;
    case WGSL::TexelFormat::RG32float:
        return WGPUTextureFormat_RG32Float;
    case WGSL::TexelFormat::RG32sint:
        return WGPUTextureFormat_RG32Sint;
    case WGSL::TexelFormat::RG32uint:
        return WGPUTextureFormat_RG32Uint;
    case WGSL::TexelFormat::RGBA16float:
        return WGPUTextureFormat_RGBA16Float;
    case WGSL::TexelFormat::RGBA16sint:
        return WGPUTextureFormat_RGBA16Sint;
    case WGSL::TexelFormat::RGBA16uint:
        return WGPUTextureFormat_RGBA16Uint;
    case WGSL::TexelFormat::RGBA32float:
        return WGPUTextureFormat_RGBA32Float;
    case WGSL::TexelFormat::RGBA32sint:
        return WGPUTextureFormat_RGBA32Sint;
    case WGSL::TexelFormat::RGBA32uint:
        return WGPUTextureFormat_RGBA32Uint;
    case WGSL::TexelFormat::RGBA8sint:
        return WGPUTextureFormat_RGBA8Sint;
    case WGSL::TexelFormat::RGBA8snorm:
        return WGPUTextureFormat_RGBA8Snorm;
    case WGSL::TexelFormat::RGBA8uint:
        return WGPUTextureFormat_RGBA8Uint;
    case WGSL::TexelFormat::RGBA8unorm:
        return WGPUTextureFormat_RGBA8Unorm;
    }
}

static auto makeBindingLayout(WGPUBindGroupLayoutEntry& newEntry, auto& bindingMember, WGPUBufferBindingType bufferTypeOverride = WGPUBufferBindingType_Undefined, uint64_t bufferSizeForBinding = 0)
{
    using Result = BindGroupLayout::Entry::BindingLayout;
    return WTF::switchOn(bindingMember, [&](const WGSL::BufferBindingLayout& bufferBinding) -> Result {
        return newEntry.buffer = WGPUBufferBindingLayout {
            .nextInChain = nullptr,
            .type = (bufferTypeOverride != WGPUBufferBindingType_Undefined) ? bufferTypeOverride : convertBindingType(bufferBinding.type),
            .hasDynamicOffset = bufferBinding.hasDynamicOffset,
            .minBindingSize = bufferBinding.minBindingSize,
            .bufferSizeForBinding = bufferSizeForBinding,
        };
    }, [&](const WGSL::SamplerBindingLayout& sampler) -> Result {
        return newEntry.sampler = WGPUSamplerBindingLayout {
            .nextInChain = nullptr,
            .type = convertSamplerBindingType(sampler.type)
        };
    }, [&](const WGSL::TextureBindingLayout& texture) -> Result {
        return newEntry.texture = WGPUTextureBindingLayout {
            .nextInChain = nullptr,
            .sampleType = convertSampleType(texture.sampleType),
            .viewDimension = convertViewDimension(texture.viewDimension),
            .multisampled = texture.multisampled
        };
    }, [&](const WGSL::StorageTextureBindingLayout& storageTexture) -> Result {
        return newEntry.storageTexture = WGPUStorageTextureBindingLayout {
            .nextInChain = nullptr,
            .access = convertAccess(storageTexture.access),
            .format = convertFormat(storageTexture.format),
            .viewDimension = convertViewDimension(storageTexture.viewDimension)
        };
    }, [&](const WGSL::ExternalTextureBindingLayout&) -> Result {
        return newEntry.texture = WGPUTextureBindingLayout {
            .nextInChain = nullptr,
            .sampleType = static_cast<WGPUTextureSampleType>(WGPUTextureSampleType_ExternalTexture),
            .viewDimension = WGPUTextureViewDimension_2D,
            .multisampled = false
        };
    });
}

static BindGroupLayout::Entry::BindingLayout toBindingLayout(const WGPUBindGroupLayoutEntry& entry)
{
    BindGroupLayout::Entry::BindingLayout result;
    if (BindGroupLayout::isPresent(entry.buffer))
        result = entry.buffer;
    else if (BindGroupLayout::isPresent(entry.sampler))
        result = entry.sampler;
    else if (BindGroupLayout::isPresent(entry.texture))
        result = entry.texture;
    else if (BindGroupLayout::isPresent(entry.storageTexture))
        result = entry.storageTexture;

    return result;
}

NSString* Device::addPipelineLayouts(Vector<Vector<WGPUBindGroupLayoutEntry>>& pipelineEntries, const std::optional<WGSL::PipelineLayout>& optionalPipelineLayout)
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
        for (auto& entry : bindGroupLayout.entries) {
            auto visibility = convertVisibility(entry.visibility);
            auto stage = visibility / 2;
            WGPUBindGroupLayoutEntry newEntry = { };
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=265204 - use a set instead
            if (auto existingBindingIndex = entries.findIf([&](const WGPUBindGroupLayoutEntry& e) {
                return e.binding == entry.webBinding;
            }); existingBindingIndex != notFound) {
                entries[existingBindingIndex].visibility |= visibility;
                std::span(entries[existingBindingIndex].metalBinding)[stage] = entry.binding;
                if (!BindGroupLayout::equalBindingEntries(toBindingLayout(entries[existingBindingIndex]), makeBindingLayout(newEntry, entry.bindingMember)))
                    return @"Binding mismatch in auto-generated layouts";
                continue;
            }
            uint64_t bufferSizeForBinding = 0;
            WGPUBufferBindingType bufferTypeOverride = WGPUBufferBindingType_Undefined;
            if (auto& entryName = entry.name; entryName.length()) {
                if (entryName.endsWith("_ArrayLength"_s)) {
                    bufferTypeOverride = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_ArrayLength);
                    auto shortName = entryName.substring(2, entryName.length() - (sizeof("_ArrayLength") + 1));
                    if (auto it = entryMap.find(shortName); it != entryMap.end())
                        bufferSizeForBinding = it->value;
                } else
                    entryMap.set(entryName, entry.webBinding);
            }

            newEntry.binding = entry.webBinding;
            std::span(newEntry.metalBinding)[stage] = entry.binding;
            newEntry.visibility = visibility;
            makeBindingLayout(newEntry, entry.bindingMember, bufferTypeOverride, bufferSizeForBinding);

            entries.append(WTFMove(newEntry));
        }
    }

    return nil;
}

Ref<PipelineLayout> Device::generatePipelineLayout(const Vector<Vector<WGPUBindGroupLayoutEntry>> &bindGroupEntries)
{
    Vector<WGPUBindGroupLayout> bindGroupLayouts;
    Vector<Ref<WebGPU::BindGroupLayout>> bindGroupLayoutsRefs;
    bindGroupLayoutsRefs.reserveInitialCapacity(bindGroupEntries.size());
    bindGroupLayouts.reserveInitialCapacity(bindGroupEntries.size());
    for (auto& entries : bindGroupEntries) {
        WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = { };
        bindGroupLayoutDescriptor.label = "getBindGroup() generated layout";
        bindGroupLayoutDescriptor.entryCount = entries.size();
        bindGroupLayoutDescriptor.entries = entries.size() ? &entries[0] : nullptr;
        auto bindGroupLayout = createBindGroupLayout(bindGroupLayoutDescriptor, true);
        if (!bindGroupLayout->isValid())
            return PipelineLayout::createInvalid(*this);
        bindGroupLayoutsRefs.append(WTFMove(bindGroupLayout));
        bindGroupLayouts.append(&bindGroupLayoutsRefs[bindGroupLayoutsRefs.size() - 1].get());
    }

    auto generatedPipelineLayout = createPipelineLayout(WGPUPipelineLayoutDescriptor {
        .nextInChain = nullptr,
        .label = "generated pipeline layout",
        .bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size()),
        .bindGroupLayouts = bindGroupLayouts.size() ? &bindGroupLayouts[0] : nullptr
    }, true);

    return generatedPipelineLayout;
}

static std::pair<Ref<RenderPipeline>, NSString*> returnInvalidRenderPipeline(WebGPU::Device &object, bool isAsync, NSString* error)
{
    if (!isAsync)
        object.generateAValidationError(error);
    return std::make_pair(RenderPipeline::createInvalid(object), error);
}

static std::pair<Ref<RenderPipeline>, NSString*> returnInvalidRenderPipeline(WebGPU::Device &object, bool isAsync, String&& error)
{
    return returnInvalidRenderPipeline(object, isAsync, static_cast<NSString*>(error));
}

static constexpr ASCIILiteral name(WGPUCompareFunction compare)
{
    switch (compare) {
    case WGPUCompareFunction_Undefined: return "undefined"_s;
    case WGPUCompareFunction_Never: return "never"_s;
    case WGPUCompareFunction_Less: return "less"_s;
    case WGPUCompareFunction_LessEqual: return "less-equal"_s;
    case WGPUCompareFunction_Greater: return "greater"_s;
    case WGPUCompareFunction_GreaterEqual: return "greater-equal"_s;
    case WGPUCompareFunction_Equal: return "equal"_s;
    case WGPUCompareFunction_NotEqual: return "not-equal"_s;
    case WGPUCompareFunction_Always: return "always"_s;
    case WGPUCompareFunction_Force32: RELEASE_ASSERT_NOT_REACHED();
    }
}
static constexpr ASCIILiteral name(WGPUStencilOperation operation)
{
    switch (operation) {
    case WGPUStencilOperation_Keep: return "keep"_s;
    case WGPUStencilOperation_Zero: return "zero"_s;
    case WGPUStencilOperation_Replace: return "replace"_s;
    case WGPUStencilOperation_Invert: return "invert"_s;
    case WGPUStencilOperation_IncrementClamp: return "increment-clamp"_s;
    case WGPUStencilOperation_DecrementClamp: return "decrement-clamp"_s;
    case WGPUStencilOperation_IncrementWrap: return "increment-wrap"_s;
    case WGPUStencilOperation_DecrementWrap: return "decrement-wrap"_s;
    case WGPUStencilOperation_Force32: RELEASE_ASSERT_NOT_REACHED();
    }
}

static NSString* errorValidatingDepthStencilState(const WGPUDepthStencilState& depthStencil)
{
#define ERROR_STRING(x) ([NSString stringWithFormat:@"Invalid DepthStencilState: %@", x])
    if (!Texture::isDepthOrStencilFormat(depthStencil.format))
        return ERROR_STRING(@"Color format passed to depth / stencil format");

    auto depthFormat = Texture::depthOnlyAspectMetalFormat(depthStencil.format);
    if ((depthStencil.depthWriteEnabled && *depthStencil.depthWriteEnabled) || (depthStencil.depthCompare != WGPUCompareFunction_Undefined && depthStencil.depthCompare != WGPUCompareFunction_Always)) {
        if (!depthFormat)
            return ERROR_STRING(@"depth-stencil state missing format");
    }

    auto isDefault = ^(const WGPUStencilFaceState& s) {
        return s.compare == WGPUCompareFunction_Always && s.failOp == WGPUStencilOperation_Keep && s.depthFailOp == WGPUStencilOperation_Keep && s.passOp == WGPUStencilOperation_Keep;
    };
    if (!isDefault(depthStencil.stencilFront) || !isDefault(depthStencil.stencilBack)) {
        if (!Texture::stencilOnlyAspectMetalFormat(depthStencil.format)) {
            NSString *error = [NSString stringWithFormat:@"missing stencil format - stencilFront: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s, stencilBack: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s", name(depthStencil.stencilFront.compare).characters(), name(depthStencil.stencilFront.failOp).characters(), name(depthStencil.stencilFront.depthFailOp).characters(), name(depthStencil.stencilFront.passOp).characters(), name(depthStencil.stencilBack.compare).characters(), name(depthStencil.stencilBack.failOp).characters(), name(depthStencil.stencilBack.depthFailOp).characters(), name(depthStencil.stencilBack.passOp).characters()];
            return ERROR_STRING(error);
        }
    }

    if (depthFormat) {
        if (!depthStencil.depthWriteEnabled)
            return ERROR_STRING(@"depthWrite must be provided");

        bool depthWriteEnabled = *depthStencil.depthWriteEnabled;
        if (depthWriteEnabled || depthStencil.stencilFront.depthFailOp != WGPUStencilOperation_Keep || depthStencil.stencilBack.depthFailOp != WGPUStencilOperation_Keep) {
            if (depthStencil.depthCompare == WGPUCompareFunction_Undefined)
                return ERROR_STRING(@"Depth compare must be provided");
        }
    }
#undef ERROR_STRING
    return nil;
}

static bool hasAlphaChannel(WGPUTextureFormat format)
{
    switch (format) {
    case WGPUTextureFormat_Undefined:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG16Float:
        return false;
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGB10A2Unorm:
        return true;
    case WGPUTextureFormat_RG11B10Ufloat:
    case WGPUTextureFormat_RGB9E5Ufloat:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
        return false;
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
        return true;
    case WGPUTextureFormat_Stencil8:
    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth24PlusStencil8:
    case WGPUTextureFormat_Depth32Float:
    case WGPUTextureFormat_Depth32FloatStencil8:
        return false;
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return true;
    case WGPUTextureFormat_BC4RUnorm:
    case WGPUTextureFormat_BC4RSnorm:
    case WGPUTextureFormat_BC5RGUnorm:
    case WGPUTextureFormat_BC5RGSnorm:
    case WGPUTextureFormat_BC6HRGBUfloat:
    case WGPUTextureFormat_BC6HRGBFloat:
        return false;
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return true;
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        return false;
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        return true;
    case WGPUTextureFormat_EACR11Unorm:
    case WGPUTextureFormat_EACR11Snorm:
    case WGPUTextureFormat_EACRG11Unorm:
    case WGPUTextureFormat_EACRG11Snorm:
    case WGPUTextureFormat_ASTC4x4Unorm:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4Unorm:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5Unorm:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5Unorm:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6Unorm:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5Unorm:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6Unorm:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8Unorm:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5Unorm:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6Unorm:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8Unorm:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10Unorm:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10Unorm:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12Unorm:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return false;
    case WGPUTextureFormat_Force32:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool textureFormatAllowedForRetunType(WGPUTextureFormat format, MTLDataType dataType, bool readsAlpha)
{
    if (dataType == MTLDataTypeNone || format == WGPUTextureFormat_Undefined)
        return true;

    if (readsAlpha && !(dataType == MTLDataTypeFloat4 || dataType == MTLDataTypeInt4 || dataType == MTLDataTypeUInt4))
        return false;

    switch (format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_R32Float:
        return dataType == MTLDataTypeFloat || dataType == MTLDataTypeFloat2 || dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;

    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RG32Float:
        return dataType == MTLDataTypeFloat2 || dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;

    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
        return dataType == MTLDataTypeFloat4;

    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R32Uint:
        return dataType == MTLDataTypeUInt || dataType == MTLDataTypeUInt2 || dataType == MTLDataTypeUInt3 || dataType == MTLDataTypeUInt4;

    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R32Sint:
        return dataType == MTLDataTypeInt || dataType == MTLDataTypeInt2 || dataType == MTLDataTypeInt3 || dataType == MTLDataTypeInt4;

    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG32Uint:
        return dataType == MTLDataTypeUInt2 || dataType == MTLDataTypeUInt3 || dataType == MTLDataTypeUInt4;

    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG32Sint:
        return dataType == MTLDataTypeInt2 || dataType == MTLDataTypeInt3 || dataType == MTLDataTypeInt4;

    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA32Uint:
        return dataType == MTLDataTypeUInt4;

    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA32Sint:
        return dataType == MTLDataTypeInt4;

    case WGPUTextureFormat_RG11B10Ufloat:
        return dataType == MTLDataTypeFloat3 || dataType == MTLDataTypeFloat4;
    default:
        return false;
    }
}

static uint32_t componentsForDataType(MTLDataType dataType)
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

static NSString* errorValidatingInterstageShaderInterfaces(WebGPU::Device &device, const WGPURenderPipelineDescriptor& descriptor, const ShaderModule::VertexOutputs* vertexOutputs, const ShaderModule::FragmentInputs* fragmentInputs, const ShaderModule::FragmentOutputs* fragmentOutputs, const ShaderModule* fragmentModule, auto* fragmentDescriptor)
{
    if (!vertexOutputs)
        return @"vertex shader has no outputs";

    auto maxVertexShaderOutputComponents = device.limits().maxInterStageShaderComponents;
    if (descriptor.primitive.topology == WGPUPrimitiveTopology_PointList) {
        if (!maxVertexShaderOutputComponents)
            return @"maxVertexShaderOutputComponents is zero";
        --maxVertexShaderOutputComponents;
    }

    auto maxInterStageShaderVariables = device.limits().maxInterStageShaderVariables;
    uint32_t vertexScalarComponents = 0;
    for (auto& [location, structMember] : *vertexOutputs) {
        if (location >= maxInterStageShaderVariables)
            return @"location >= maxInterStageShaderVariables";

        vertexScalarComponents += componentsForDataType(structMember.dataType);
    }

    if (vertexScalarComponents > maxVertexShaderOutputComponents)
        return @"vertexScalarComponents > maxVertexShaderOutputComponents";

    if (fragmentModule) {
        auto maxFragmentShaderInputComponents = device.limits().maxInterStageShaderComponents;
        auto decrement = ^(uint32_t& unsignedValue) {
            return unsignedValue-- ? true : false;
        };
        const auto& fragmentEntryPoint = (fragmentDescriptor && fragmentDescriptor->entryPoint) ? fromAPI(fragmentDescriptor->entryPoint) : fragmentModule->defaultFragmentEntryPoint();
        if (fragmentModule->usesFrontFacingInInput(fragmentEntryPoint) && !decrement(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to front facing";
        if (fragmentModule->usesSampleIndexInInput(fragmentEntryPoint) && !decrement(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to sample index";
        if (fragmentModule->usesSampleMaskInInput(fragmentEntryPoint) && !decrement(maxFragmentShaderInputComponents))
            return @"maxFragmentShaderInputComponents is less than zero due to sample mask";

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
            for (auto& [location, _] : *fragmentOutputs) {
                if (location >= maxInterStageShaderVariables)
                    return @"location >= maxInterStageShaderVariables";
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

std::pair<Ref<RenderPipeline>, NSString*> Device::createRenderPipeline(const WGPURenderPipelineDescriptor& descriptor, bool isAsync)
{
    if (!validateRenderPipeline(descriptor) || !isValid())
        return returnInvalidRenderPipeline(*this, isAsync, "device or descriptor is not valid"_s);

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
#if ENABLE(WEBGPU_BY_DEFAULT)
    mtlRenderPipelineDescriptor.shaderValidation = shaderValidationState();
#endif

    auto label = fromAPI(descriptor.label);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=249345 don't unconditionally set this to YES
    mtlRenderPipelineDescriptor.supportIndirectCommandBuffers = YES;
    auto& deviceLimits = limits();

    RefPtr<PipelineLayout> pipelineLayout;
    Vector<Vector<WGPUBindGroupLayoutEntry>> bindGroupEntries;
    if (descriptor.layout) {
        Ref layout = WebGPU::protectedFromAPI(descriptor.layout);
        if (!layout->isValid())
            return returnInvalidRenderPipeline(*this, isAsync, "Pipeline layout is not valid"_s);
        if (&layout->device() != this)
            return returnInvalidRenderPipeline(*this, isAsync, "Pipeline layout created from different device"_s);

        if (!layout->isAutoLayout())
            pipelineLayout = layout.ptr();
    }

    const ShaderModule::VertexStageIn* vertexStageIn = nullptr;
    const ShaderModule::VertexOutputs* vertexOutputs = nullptr;
    BufferBindingSizesForPipeline minimumBufferSizes;
    {
        if (descriptor.vertex.nextInChain)
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex module has an invalid chain"_s);

        Ref vertexModule = WebGPU::protectedFromAPI(descriptor.vertex.module);
        if (!vertexModule->isValid() || !vertexModule->ast())
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex module is not valid"_s);
        if (&vertexModule->device() != this)
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex module was created with a different device"_s);

        const auto& vertexEntryPoint = descriptor.vertex.entryPoint ? fromAPI(descriptor.vertex.entryPoint) : vertexModule->defaultVertexEntryPoint();
        vertexStageIn = vertexModule->stageInTypesForEntryPoint(vertexEntryPoint);
        if (NSString* error = errorValidatingVertexStageIn(vertexStageIn, *this))
            return returnInvalidRenderPipeline(*this, isAsync, error);
        NSError *error = nil;
        auto libraryCreationResult = createLibrary(m_device, vertexModule, pipelineLayout.get(), vertexEntryPoint, label, descriptor.vertex.constantsSpan(), minimumBufferSizes, &error);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync, error.localizedDescription ?: @"Vertex library failed creation");

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout) {
            if (NSString* error = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
                return returnInvalidRenderPipeline(*this, isAsync, error);
        }
        auto vertexFunction = createFunction(libraryCreationResult->library, entryPointInformation, label);
        if (!vertexFunction || vertexFunction.functionType != MTLFunctionTypeVertex || entryPointInformation.specializationConstants.size() != libraryCreationResult->wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex function could not be created"_s);
        mtlRenderPipelineDescriptor.vertexFunction = vertexFunction;
        vertexOutputs = vertexModule->vertexReturnTypeForEntryPoint(vertexEntryPoint);
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

        if (fragmentDescriptor.nextInChain)
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment has extra chain"_s);

        fragmentModule = WebGPU::protectedFromAPI(fragmentDescriptor.module).ptr();
        if (!fragmentModule->isValid() || !fragmentModule->ast())
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment module is invalid"_s);

        if (&fragmentModule->device() != this)
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment module was created with a different device"_s);

        auto fragmentShaderModule = fragmentModule->ast();
        RELEASE_ASSERT(fragmentShaderModule);
        const auto& fragmentEntryPoint = fragmentDescriptor.entryPoint ? fromAPI(fragmentDescriptor.entryPoint) : fragmentModule->defaultFragmentEntryPoint();
        usesFragDepth = fragmentModule->usesFragDepth(fragmentEntryPoint);
        usesSampleMask = fragmentModule->usesSampleMaskInOutput(fragmentEntryPoint);
        NSError *error = nil;
        auto libraryCreationResult = createLibrary(m_device, *fragmentModule, pipelineLayout.get(), fragmentEntryPoint, label, fragmentDescriptor.constantsSpan(), minimumBufferSizes, &error);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync, error.localizedDescription ?: @"Fragment library could not be created");

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout) {
            if (NSString* error = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
                return returnInvalidRenderPipeline(*this, isAsync, error);
        }

        auto fragmentFunction = createFunction(libraryCreationResult->library, entryPointInformation, label);
        if (!fragmentFunction || fragmentFunction.functionType != MTLFunctionTypeFragment || entryPointInformation.specializationConstants.size() != libraryCreationResult->wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment function failed creation"_s);
        mtlRenderPipelineDescriptor.fragmentFunction = fragmentFunction;

        uint32_t bytesPerSample = 0;
        fragmentInputs = fragmentModule->fragmentInputsForEntryPoint(fragmentEntryPoint);
        fragmentReturnTypes = fragmentModule->fragmentReturnTypeForEntryPoint(fragmentEntryPoint);
        colorAttachmentCount = fragmentDescriptor.targetCount;
        for (auto [ i, targetDescriptor ] : indexedRange(fragmentDescriptor.targetsSpan())) {
            if (targetDescriptor.format == WGPUTextureFormat_Undefined)
                continue;

            MTLDataType fragmentFunctionReturnType = MTLDataTypeNone;
            if (fragmentReturnTypes) {
                if (auto it = fragmentReturnTypes->find(i); it != fragmentReturnTypes->end())
                    fragmentFunctionReturnType = it->value;
            }
            const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[i];

            if (Texture::isDepthOrStencilFormat(targetDescriptor.format) || !Texture::isRenderableFormat(targetDescriptor.format, *this))
                return returnInvalidRenderPipeline(*this, isAsync, "Depth / stencil format passed to color format"_s);

            bytesPerSample = roundUpToMultipleOfNonPowerOfTwo(Texture::renderTargetPixelByteAlignment(targetDescriptor.format), bytesPerSample);
            bytesPerSample += Texture::renderTargetPixelByteCost(targetDescriptor.format);
            mtlColorAttachment.pixelFormat = Texture::pixelFormat(targetDescriptor.format);

            hasAtLeastOneColorTarget = true;
            if (targetDescriptor.writeMask > WGPUColorWriteMask_All || (fragmentFunctionReturnType == MTLDataTypeNone && targetDescriptor.writeMask))
                return returnInvalidRenderPipeline(*this, isAsync, "writeMask is invalid"_s);
            mtlColorAttachment.writeMask = colorWriteMask(targetDescriptor.writeMask);

            bool readsAlpha = false;
            if (targetDescriptor.blend) {
                if (!Texture::supportsBlending(targetDescriptor.format, *this))
                    return returnInvalidRenderPipeline(*this, isAsync, "Color target attempted to use blending on non-blendable format"_s);
                mtlColorAttachment.blendingEnabled = YES;

                const auto& alphaBlend = targetDescriptor.blend->alpha;
                const auto& colorBlend = targetDescriptor.blend->color;
                auto validateBlend = ^(const WGPUBlendComponent& blend) {
                    if (blend.operation == WGPUBlendOperation_Min || blend.operation == WGPUBlendOperation_Max)
                        return blend.srcFactor == WGPUBlendFactor_One && blend.dstFactor == WGPUBlendFactor_One;
                    return true;
                };
                if (!validateBlend(alphaBlend) || !validateBlend(colorBlend))
                    return returnInvalidRenderPipeline(*this, isAsync, "Blend states are not valid"_s);
                mtlColorAttachment.alphaBlendOperation = blendOperation(alphaBlend.operation);
                mtlColorAttachment.sourceAlphaBlendFactor = blendFactor(alphaBlend.srcFactor);
                mtlColorAttachment.destinationAlphaBlendFactor = blendFactor(alphaBlend.dstFactor);

                mtlColorAttachment.rgbBlendOperation = blendOperation(colorBlend.operation);
                mtlColorAttachment.sourceRGBBlendFactor = blendFactor(colorBlend.srcFactor);
                mtlColorAttachment.destinationRGBBlendFactor = blendFactor(colorBlend.dstFactor);
                readsAlpha = colorBlend.srcFactor == WGPUBlendFactor_SrcAlpha || colorBlend.srcFactor == WGPUBlendFactor_OneMinusSrcAlpha || colorBlend.srcFactor == WGPUBlendFactor_SrcAlphaSaturated || colorBlend.dstFactor == WGPUBlendFactor_SrcAlpha || colorBlend.dstFactor == WGPUBlendFactor_OneMinusSrcAlpha || colorBlend.dstFactor == WGPUBlendFactor_SrcAlphaSaturated;
            } else
                mtlColorAttachment.blendingEnabled = NO;

            if (!textureFormatAllowedForRetunType(targetDescriptor.format, fragmentFunctionReturnType, readsAlpha))
                return returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"pipeline creation - color target pixel format(%lu) for location(%zu) is incompatible with shader output data type of %zu", i, mtlColorAttachment.pixelFormat, fragmentFunctionReturnType]);
        }

        if (bytesPerSample > deviceLimits.maxColorAttachmentBytesPerSample)
            return returnInvalidRenderPipeline(*this, isAsync, "Bytes per sample exceeded maximum allowed limit"_s);
    }

    MTLDepthStencilDescriptor *depthStencilDescriptor = nil;
    float depthBias = 0.f, depthBiasSlopeScale = 0.f, depthBiasClamp = 0.f;
    if (auto depthStencil = descriptor.depthStencil) {
        if (NSString *error = errorValidatingDepthStencilState(*depthStencil))
            return returnInvalidRenderPipeline(*this, isAsync, error);

        MTLPixelFormat depthStencilFormat = Texture::pixelFormat(depthStencil->format);
        bool isStencilOnlyFormat = Device::isStencilOnlyFormat(depthStencilFormat);
        mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = isStencilOnlyFormat ? MTLPixelFormatInvalid : depthStencilFormat;
        if (Texture::stencilOnlyAspectMetalFormat(depthStencil->format))
            mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = depthStencilFormat;

        depthStencilDescriptor = [MTLDepthStencilDescriptor new];
        depthStencilDescriptor.depthCompareFunction = convertToMTLCompare(depthStencil->depthCompare);
        depthStencilDescriptor.depthWriteEnabled = depthStencil->depthWriteEnabled.value_or(false);
        populateStencilOperation(depthStencilDescriptor.frontFaceStencil, depthStencil->stencilFront, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        populateStencilOperation(depthStencilDescriptor.backFaceStencil, depthStencil->stencilBack, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        depthBias = depthStencil->depthBias;
        depthBiasSlopeScale = depthStencil->depthBiasSlopeScale;
        depthBiasClamp = depthStencil->depthBiasClamp;
    }

    if (descriptor.fragment && !hasAtLeastOneColorTarget && !descriptor.depthStencil)
        return returnInvalidRenderPipeline(*this, isAsync, "No color targets or depth stencil were specified in the descriptor"_s);
    if (usesFragDepth && mtlRenderPipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid)
        return returnInvalidRenderPipeline(*this, isAsync, "Shader writes to frag depth but no depth texture set"_s);

    if (descriptor.multisample.count != 1 && descriptor.multisample.count != 4)
        return returnInvalidRenderPipeline(*this, isAsync, "multisample count must be either 1 or 4"_s);
    mtlRenderPipelineDescriptor.rasterSampleCount = descriptor.multisample.count;
    mtlRenderPipelineDescriptor.alphaToCoverageEnabled = descriptor.multisample.alphaToCoverageEnabled;
    if (descriptor.multisample.alphaToCoverageEnabled) {
        if (usesSampleMask)
            return returnInvalidRenderPipeline(*this, isAsync, "Can not use sampleMask with alphaToCoverage"_s);
        if (!descriptor.fragment)
            return returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s);
        if (!descriptor.fragment->targetCount || !hasAlphaChannel(descriptor.fragment->targets[0].format) || !Texture::supportsBlending(descriptor.fragment->targets[0].format, *this))
            return returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s);
        if (descriptor.multisample.count == 1)
            return returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires multisampling"_s);
    }
    RELEASE_ASSERT([mtlRenderPipelineDescriptor respondsToSelector:@selector(setSampleMask:)]);
    uint32_t sampleMask = RenderBundleEncoder::defaultSampleMask;
    if (auto mask = descriptor.multisample.mask; mask != sampleMask) {
        [mtlRenderPipelineDescriptor setSampleMask:mask];
        sampleMask = mask;
    }

    if (NSString* error = errorValidatingInterstageShaderInterfaces(*this, descriptor, vertexOutputs, fragmentInputs, fragmentReturnTypes, fragmentModule.get(), descriptor.fragment))
        return returnInvalidRenderPipeline(*this, isAsync, error);

    RenderPipeline::RequiredBufferIndicesContainer requiredBufferIndices;
    if (descriptor.vertex.bufferCount) {
        if (!vertexStageIn)
            return returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"Vertex shader has no stageIn parameters but buffer count was %zu and attribute count was %zu", descriptor.vertex.bufferCount, descriptor.vertex.buffers[0].attributeCount]);
        NSString *error = nil;
        MTLVertexDescriptor *vertexDecriptor = createVertexDescriptor(descriptor.vertex, deviceLimits, *vertexStageIn, requiredBufferIndices, &error);
        if (error)
            return returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"vertex descriptor creation failed %@", error]);

        ASSERT(vertexDecriptor);
        mtlRenderPipelineDescriptor.vertexDescriptor = vertexDecriptor;
    }

    MTLDepthClipMode mtlDepthClipMode = MTLDepthClipModeClip;
    if (descriptor.primitive.nextInChain) {
        if (!hasFeature(WGPUFeatureName_DepthClipControl) || descriptor.primitive.nextInChain->sType != WGPUSType_PrimitiveDepthClipControl || descriptor.primitive.nextInChain->next)
            return returnInvalidRenderPipeline(*this, isAsync, "unclippedDepth used without enabling depth-clip-contro feature"_s);

        auto* depthClipControl = reinterpret_cast<const WGPUPrimitiveDepthClipControl*>(descriptor.primitive.nextInChain);

        if (depthClipControl->unclippedDepth)
            mtlDepthClipMode = MTLDepthClipModeClamp;
    }

    mtlRenderPipelineDescriptor.inputPrimitiveTopology = topologyType(descriptor.primitive.topology);

    // These properties are to be used by the render command encoder, not the render pipeline.
    // Therefore, the render pipeline stores these, and when the render command encoder is assigned
    // a pipeline, the render command encoder can get these information out of the render pipeline.
    auto primitiveTopology = descriptor.primitive.topology;
    if (primitiveTopology != WGPUPrimitiveTopology_LineStrip && primitiveTopology != WGPUPrimitiveTopology_TriangleStrip) {
        if (descriptor.primitive.stripIndexFormat != WGPUIndexFormat_Undefined)
            return returnInvalidRenderPipeline(*this, isAsync, "If primitive.topology is not line-strip or triangle-strip, primitive.stripIndexFormat must be undefined."_s);
    }

    auto mtlPrimitiveType = primitiveType(primitiveTopology);
    auto mtlIndexType = indexType(descriptor.primitive.stripIndexFormat);
    auto mtlFrontFace = frontFace(descriptor.primitive.frontFace);
    auto mtlCullMode = cullMode(descriptor.primitive.cullMode);

    NSError *error = nil;
    id<MTLRenderPipelineState> renderPipelineState = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    if (error || !renderPipelineState)
        return returnInvalidRenderPipeline(*this, isAsync, error.localizedDescription);

    if (!pipelineLayout) {
        auto generatedPipelineLayout = generatePipelineLayout(bindGroupEntries);
        if (!generatedPipelineLayout->isValid())
            return returnInvalidRenderPipeline(*this, isAsync, "Generated pipeline layout is not valid"_s);

        return std::make_pair(RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthClipMode, depthStencilDescriptor, WTFMove(generatedPipelineLayout), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, mtlRenderPipelineDescriptor, colorAttachmentCount, descriptor, WTFMove(requiredBufferIndices), WTFMove(minimumBufferSizes), *this), nil);
    }

    return std::make_pair(RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthClipMode, depthStencilDescriptor, const_cast<PipelineLayout&>(*pipelineLayout), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, mtlRenderPipelineDescriptor, colorAttachmentCount, descriptor, WTFMove(requiredBufferIndices), WTFMove(minimumBufferSizes), *this), nil);
}

void Device::createRenderPipelineAsync(const WGPURenderPipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, String&& message)>&& callback)
{
    auto pipelineAndError = createRenderPipeline(descriptor, true);
    if (auto inst = instance(); inst.get()) {
        inst->scheduleWork([protectedThis = Ref { *this }, pipeline = WTFMove(pipelineAndError.first), callback = WTFMove(callback), error = WTFMove(pipelineAndError.second)]() mutable {
            callback((protectedThis->isDestroyed() || pipeline->isValid()) ? WGPUCreatePipelineAsyncStatus_Success : WGPUCreatePipelineAsyncStatus_ValidationError, WTFMove(pipeline), WTFMove(error));
        });
    } else
        callback(WGPUCreatePipelineAsyncStatus_ValidationError, WTFMove(pipelineAndError.first), WTFMove(pipelineAndError.second));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderPipeline);

RenderPipeline::RenderPipeline(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, MTLDepthClipMode clipMode, MTLDepthStencilDescriptor *depthStencilDescriptor, Ref<PipelineLayout>&& pipelineLayout, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, uint32_t sampleMask, MTLRenderPipelineDescriptor* renderPipelineDescriptor, uint32_t colorAttachmentCount, const WGPURenderPipelineDescriptor& descriptor, RequiredBufferIndicesContainer&& requiredBufferIndices, BufferBindingSizesForPipeline&& minimumBufferSizes, Device& device)
    : m_renderPipelineState(renderPipelineState)
    , m_device(device)
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
    , m_requiredBufferIndices(WTFMove(requiredBufferIndices))
    , m_pipelineLayout(WTFMove(pipelineLayout))
    , m_descriptor(descriptor)
    , m_descriptorDepthStencil(descriptor.depthStencil ? *descriptor.depthStencil : WGPUDepthStencilState())
    , m_descriptorFragment(descriptor.fragment ? *descriptor.fragment : WGPUFragmentState())
    , m_descriptorTargets(descriptor.fragment && descriptor.fragment->targetCount ? Vector<WGPUColorTargetState>(std::span { descriptor.fragment->targets, descriptor.fragment->targetCount }) : Vector<WGPUColorTargetState>())
    , m_minimumBufferSizes(minimumBufferSizes)
{
    if (descriptor.depthStencil)
        m_descriptor.depthStencil = &m_descriptorDepthStencil;
    if (m_descriptorTargets.size())
        m_descriptorFragment.targets = &m_descriptorTargets[0];
    if (descriptor.fragment)
        m_descriptor.fragment = &m_descriptorFragment;

    if (auto* depthStencil = descriptor.depthStencil; depthStencil && depthStencil->stencilWriteMask) {
        const auto& stencilFront = depthStencil->stencilFront;
        const auto& stencilBack = depthStencil->stencilBack;
        const auto& cullMode = descriptor.primitive.cullMode;
        if (cullMode != WGPUCullMode_Front && (stencilFront.passOp != WGPUStencilOperation_Keep || stencilFront.depthFailOp != WGPUStencilOperation_Keep || stencilFront.failOp != WGPUStencilOperation_Keep))
            m_writesStencil = true;
        else if (cullMode != WGPUCullMode_Back && (stencilBack.passOp != WGPUStencilOperation_Keep || stencilBack.depthFailOp != WGPUStencilOperation_Keep || stencilBack.failOp != WGPUStencilOperation_Keep))
            m_writesStencil = true;
    }
}

RenderPipeline::RenderPipeline(Device& device)
    : m_device(device)
    , m_pipelineLayout(PipelineLayout::createInvalid(device))
    , m_minimumBufferSizes({ })
{
}

RenderPipeline::~RenderPipeline() = default;

Ref<BindGroupLayout> RenderPipeline::getBindGroupLayout(uint32_t groupIndex)
{
    auto pipelineLayout = m_pipelineLayout;
    auto device = m_device;

    if (!isValid()) {
        device->generateAValidationError("getBindGroupLayout: RenderPipeline is invalid"_s);
        pipelineLayout->makeInvalid();
        return BindGroupLayout::createInvalid(device.get());
    }

    if (groupIndex >= pipelineLayout->numberOfBindGroupLayouts()) {
        device->generateAValidationError("getBindGroupLayout: groupIndex is out of range"_s);
        pipelineLayout->makeInvalid();
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

bool RenderPipeline::colorDepthStencilTargetsMatch(const WGPURenderPassDescriptor& descriptor, const Vector<RefPtr<TextureView>>& colorAttachmentViews, const RefPtr<TextureView>& depthStencilView) const
{
    if (!m_descriptor.fragment) {
        if (descriptor.colorAttachmentCount)
            return false;
    } else {
        for (size_t i = 0, maxCount = std::max<size_t>(m_descriptorTargets.size(), colorAttachmentViews.size()); i < maxCount; ++i) {
            RefPtr attachmentView = i < colorAttachmentViews.size() ? colorAttachmentViews[i] : nullptr;
            auto descriptorTargetFormat = i < m_descriptorTargets.size() ? m_descriptorTargets[i].format : WGPUTextureFormat_Undefined;
            if (!attachmentView) {
                if (descriptorTargetFormat == WGPUTextureFormat_Undefined)
                    continue;
                return false;
            }
            if (descriptorTargetFormat != attachmentView->format())
                return false;
            if (attachmentView->sampleCount() != m_descriptor.multisample.count)
                return false;
        }
    }

    if (!m_descriptor.depthStencil) {
        if (!descriptor.depthStencilAttachment)
            return true;

        return false;
    }

    if (auto* depthStencil = descriptor.depthStencilAttachment) {
        if (!depthStencilView)
            return false;
        auto& texture = *depthStencilView.get();
        if (texture.format() != m_descriptor.depthStencil->format)
            return false;
        auto mtlPixelFormat = texture.texture().pixelFormat;
        auto descriptorFormat = m_descriptor.depthStencil->format;
        if (mtlPixelFormat == MTLPixelFormatX32_Stencil8 && descriptorFormat == WGPUTextureFormat_Stencil8)
            return false;
        if (mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8 && (descriptorFormat == WGPUTextureFormat_Depth32Float || descriptorFormat == WGPUTextureFormat_Depth24Plus))
            return false;
        if (texture.sampleCount() != m_descriptor.multisample.count)
            return false;
    } else if (m_descriptor.depthStencil->format != WGPUTextureFormat_Undefined)
        return false;

    return true;
}

bool RenderPipeline::validateRenderBundle(const WGPURenderBundleEncoderDescriptor& descriptor) const
{
    if (!validateDepthStencilState(descriptor.depthReadOnly, descriptor.stencilReadOnly))
        return false;

    if (descriptor.sampleCount != m_descriptor.multisample.count)
        return false;

    if (!m_descriptor.fragment) {
        if (descriptor.colorFormatCount || descriptor.depthStencilFormat != WGPUTextureFormat_Undefined)
            return false;

        return true;
    }

    auto& fragment = *m_descriptor.fragment;
    for (size_t i = 0, maxTargetCount = std::max<size_t>(fragment.targetCount, descriptor.colorFormatCount); i < maxTargetCount; ++i) {
        auto colorFormat = i < descriptor.colorFormatCount ? descriptor.colorFormatsSpan()[i] : WGPUTextureFormat_Undefined;
        auto descriptorFormat = i < m_descriptorTargets.size() ? m_descriptorTargets[i].format : WGPUTextureFormat_Undefined;
        if (descriptorFormat != colorFormat)
            return false;
    }

    if (!m_descriptor.depthStencil) {
        if (descriptor.depthStencilFormat == WGPUTextureFormat_Undefined)
            return true;

        return false;
    }

    if (descriptor.depthStencilFormat != m_descriptor.depthStencil->format)
        return false;

    return true;
}

const BufferBindingSizesForBindGroup* RenderPipeline::minimumBufferSizes(uint32_t index) const
{
    auto it = m_minimumBufferSizes.find(index);
    return it == m_minimumBufferSizes.end() ? nullptr : &it->value;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderPipelineReference(WGPURenderPipeline renderPipeline)
{
    WebGPU::fromAPI(renderPipeline).ref();
}

void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline)
{
    WebGPU::fromAPI(renderPipeline).deref();
}

WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(renderPipeline)->getBindGroupLayout(groupIndex));
}

void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, const char* label)
{
    WebGPU::protectedFromAPI(renderPipeline)->setLabel(WebGPU::fromAPI(label));
}

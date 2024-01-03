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

static MTLVertexDescriptor *createVertexDescriptor(WGPUVertexState vertexState)
{
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];

    for (size_t bufferIndex = 0; bufferIndex < vertexState.bufferCount; ++bufferIndex) {
        auto& buffer = vertexState.buffers[bufferIndex];
        if (buffer.arrayStride == WGPU_COPY_STRIDE_UNDEFINED)
            continue;

        if (!buffer.attributeCount)
            continue;

        vertexDescriptor.layouts[bufferIndex].stride = std::max<NSUInteger>(sizeof(int), buffer.arrayStride);
        vertexDescriptor.layouts[bufferIndex].stepFunction = stepFunction(buffer.stepMode, buffer.arrayStride);
        if (vertexDescriptor.layouts[bufferIndex].stepFunction == MTLVertexStepFunctionConstant)
            vertexDescriptor.layouts[bufferIndex].stepRate = 0;
        for (size_t i = 0; i < buffer.attributeCount; ++i) {
            auto& attribute = buffer.attributes[i];
            const auto& mtlAttribute = vertexDescriptor.attributes[attribute.shaderLocation];
            mtlAttribute.format = vertexFormat(attribute.format);
            mtlAttribute.bufferIndex = bufferIndex;
            mtlAttribute.offset = attribute.offset;
        }
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

void Device::addPipelineLayouts(Vector<Vector<WGPUBindGroupLayoutEntry>>& pipelineEntries, const std::optional<WGSL::PipelineLayout>& optionalPipelineLayout)
{
    if (!optionalPipelineLayout)
        return;

    auto &pipelineLayout = *optionalPipelineLayout;
    size_t pipelineLayoutCount = pipelineLayout.bindGroupLayouts.size();
    if (pipelineEntries.size() < pipelineLayoutCount)
        pipelineEntries.grow(pipelineLayoutCount);

    for (size_t pipelineLayoutIndex = 0; pipelineLayoutIndex < pipelineLayoutCount; ++pipelineLayoutIndex) {
        auto& bindGroupLayout = pipelineLayout.bindGroupLayouts[pipelineLayoutIndex];
        auto& entries = pipelineEntries[pipelineLayoutIndex];
        HashMap<String, uint64_t> entryMap;
        for (auto& entry : bindGroupLayout.entries) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=265204 - use a set instead
            if (auto existingBindingIndex = entries.findIf([&](const WGPUBindGroupLayoutEntry& e) {
                return e.binding == entry.webBinding;
            }); existingBindingIndex != notFound) {
                entries[existingBindingIndex].visibility |= convertVisibility(entry.visibility);
                continue;
            }
            WGPUBindGroupLayoutEntry newEntry = { };
            uint64_t minBindingSize = 0;
            WGPUBufferBindingType bufferTypeOverride = WGPUBufferBindingType_Undefined;
            auto& entryName = entry.name;
            if (entryName.endsWith("_ArrayLength"_s)) {
                bufferTypeOverride = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_ArrayLength);
                auto shortName = entryName.substring(2, entryName.length() - (sizeof("_ArrayLength") + 1));
                minBindingSize = entryMap.find(shortName)->value;
            } else
                entryMap.set(entryName, entry.binding);

            newEntry.binding = entry.webBinding;
            newEntry.metalBinding = entry.binding;
            newEntry.visibility = convertVisibility(entry.visibility);
            WTF::switchOn(entry.bindingMember, [&](const WGSL::BufferBindingLayout& bufferBinding) {
                newEntry.buffer = WGPUBufferBindingLayout {
                    .nextInChain = nullptr,
                    .type = (bufferTypeOverride != WGPUBufferBindingType_Undefined) ? bufferTypeOverride : convertBindingType(bufferBinding.type),
                    .hasDynamicOffset = bufferBinding.hasDynamicOffset,
                    .minBindingSize = minBindingSize ?: bufferBinding.minBindingSize,
                };
            }, [&](const WGSL::SamplerBindingLayout& sampler) {
                newEntry.sampler = WGPUSamplerBindingLayout {
                    .nextInChain = nullptr,
                    .type = convertSamplerBindingType(sampler.type)
                };
            }, [&](const WGSL::TextureBindingLayout& texture) {
                newEntry.texture = WGPUTextureBindingLayout {
                    .nextInChain = nullptr,
                    .sampleType = convertSampleType(texture.sampleType),
                    .viewDimension = convertViewDimension(texture.viewDimension),
                    .multisampled = texture.multisampled
                };
            }, [&](const WGSL::StorageTextureBindingLayout& storageTexture) {
                newEntry.storageTexture = WGPUStorageTextureBindingLayout {
                    .nextInChain = nullptr,
                    .access = convertAccess(storageTexture.access),
                    .format = convertFormat(storageTexture.format),
                    .viewDimension = convertViewDimension(storageTexture.viewDimension)
                };
            }, [&](const WGSL::ExternalTextureBindingLayout&) {
                newEntry.texture = WGPUTextureBindingLayout {
                    .nextInChain = nullptr,
                    .sampleType = static_cast<WGPUTextureSampleType>(WGPUTextureSampleType_ExternalTexture),
                    .viewDimension = WGPUTextureViewDimension_2D,
                    .multisampled = false
                };
            });

            entries.append(WTFMove(newEntry));
        }
    }
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
        bindGroupLayoutsRefs.append(createBindGroupLayout(bindGroupLayoutDescriptor, true));
        bindGroupLayouts.append(&bindGroupLayoutsRefs[bindGroupLayoutsRefs.size() - 1].get());
    }

    auto generatedPipelineLayout = createPipelineLayout(WGPUPipelineLayoutDescriptor {
        .nextInChain = nullptr,
        .label = "generated pipeline layout",
        .bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size()),
        .bindGroupLayouts = bindGroupLayouts.size() ? &bindGroupLayouts[0] : nullptr
    });

    return generatedPipelineLayout;
}

static Ref<RenderPipeline> returnInvalidRenderPipeline(WebGPU::Device &object, bool isAsync, String&& error)
{
    if (!isAsync)
        object.generateAValidationError(WTFMove(error));
    return RenderPipeline::createInvalid(object);
}

static constexpr const char* name(WGPUCompareFunction compare)
{
    switch (compare) {
    case WGPUCompareFunction_Undefined: return "undefined";
    case WGPUCompareFunction_Never: return "never";
    case WGPUCompareFunction_Less: return "less";
    case WGPUCompareFunction_LessEqual: return "less-equal";
    case WGPUCompareFunction_Greater: return "greater";
    case WGPUCompareFunction_GreaterEqual: return "greater-equal";
    case WGPUCompareFunction_Equal: return "equal";
    case WGPUCompareFunction_NotEqual: return "not-equal";
    case WGPUCompareFunction_Always: return "always";
    case WGPUCompareFunction_Force32: RELEASE_ASSERT_NOT_REACHED();
    }
}
static constexpr const char* name(WGPUStencilOperation operation)
{
    switch (operation) {
    case WGPUStencilOperation_Keep: return "keep";
    case WGPUStencilOperation_Zero: return "zero";
    case WGPUStencilOperation_Replace: return "replace";
    case WGPUStencilOperation_Invert: return "invert";
    case WGPUStencilOperation_IncrementClamp: return "increment-clamp";
    case WGPUStencilOperation_DecrementClamp: return "decrement-clamp";
    case WGPUStencilOperation_IncrementWrap: return "increment-wrap";
    case WGPUStencilOperation_DecrementWrap: return "decrement-wrap";
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
            NSString *error = [NSString stringWithFormat:@"missing stencil format - stencilFront: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s, stencilBack: compare = %s, failOp = %s, depthFailOp = %s, passOp = %s", name(depthStencil.stencilFront.compare), name(depthStencil.stencilFront.failOp), name(depthStencil.stencilFront.depthFailOp), name(depthStencil.stencilFront.passOp), name(depthStencil.stencilBack.compare), name(depthStencil.stencilBack.failOp), name(depthStencil.stencilBack.depthFailOp), name(depthStencil.stencilBack.passOp)];
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

Ref<RenderPipeline> Device::createRenderPipeline(const WGPURenderPipelineDescriptor& descriptor, bool isAsync)
{
    if (!validateRenderPipeline(descriptor) || !isValid())
        return returnInvalidRenderPipeline(*this, isAsync, "device or descriptor is not valid"_s);

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    auto label = fromAPI(descriptor.label);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=249345 don't unconditionally set this to YES
    mtlRenderPipelineDescriptor.supportIndirectCommandBuffers = YES;

    const PipelineLayout* pipelineLayout = nullptr;
    Vector<Vector<WGPUBindGroupLayoutEntry>> bindGroupEntries;
    if (descriptor.layout) {
        if (auto& layout = WebGPU::fromAPI(descriptor.layout); layout.isValid() && !layout.isAutoLayout()) {
            pipelineLayout = &layout;
            if (pipelineLayout && &pipelineLayout->device() != this)
                return returnInvalidRenderPipeline(*this, isAsync, "Pipeline layout is not valid"_s);
        }
    }

    std::optional<PipelineLayout> vertexPipelineLayout { std::nullopt };
    {
        if (descriptor.vertex.nextInChain)
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex module has an invalid chain"_s);

        const auto& vertexModule = WebGPU::fromAPI(descriptor.vertex.module);
        if (!vertexModule.isValid())
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex module is not valid"_s);

        const auto& vertexFunctionName = fromAPI(descriptor.vertex.entryPoint);
        auto libraryCreationResult = createLibrary(m_device, vertexModule, pipelineLayout, vertexFunctionName.length() ? vertexFunctionName : vertexModule.defaultVertexEntryPoint(), label);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex library failed creation"_s);

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout)
            addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout);
        auto [constantValues, wgslConstantValues] = createConstantValues(descriptor.vertex.constantCount, descriptor.vertex.constants, entryPointInformation, vertexModule);
        auto vertexFunction = createFunction(libraryCreationResult->library, entryPointInformation, constantValues, label);
        if (!vertexFunction || vertexFunction.functionType != MTLFunctionTypeVertex || entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync, "Vertex function could not be created"_s);
        mtlRenderPipelineDescriptor.vertexFunction = vertexFunction;
    }

    std::optional<PipelineLayout> fragmentPipelineLayout { std::nullopt };
    bool usesFragDepth = false;
    bool usesSampleMask = false;
    bool hasAtLeastOneColorTarget = false;

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        if (fragmentDescriptor.nextInChain)
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment has extra chain"_s);

        const auto& fragmentModule = WebGPU::fromAPI(fragmentDescriptor.module);
        if (!fragmentModule.isValid() || !fragmentModule.ast())
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment module is invalid"_s);

        usesFragDepth = fragmentModule.ast()->usesFragDepth();
        usesSampleMask = fragmentModule.ast()->usesSampleMask();
        const auto& fragmentFunctionName = fromAPI(fragmentDescriptor.entryPoint);

        auto fragmentEntryPoint = fragmentFunctionName.length() ? fragmentFunctionName : fragmentModule.defaultFragmentEntryPoint();
        auto libraryCreationResult = createLibrary(m_device, fragmentModule, pipelineLayout, fragmentEntryPoint, label);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment library could not be created"_s);

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout)
            addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout);

        auto [constantValues, wgslConstantValues] = createConstantValues(fragmentDescriptor.constantCount, fragmentDescriptor.constants, entryPointInformation, fragmentModule);
        auto fragmentFunction = createFunction(libraryCreationResult->library, entryPointInformation, constantValues, label);
        if (!fragmentFunction || fragmentFunction.functionType != MTLFunctionTypeFragment || entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync, "Fragment function failed creation"_s);
        mtlRenderPipelineDescriptor.fragmentFunction = fragmentFunction;

        uint32_t bytesPerSample = 0;
        const auto& returnTypes = fragmentModule.returnTypeForEntryPoint(fragmentEntryPoint);
        for (uint32_t i = 0; i < fragmentDescriptor.targetCount; ++i) {
            const auto& targetDescriptor = fragmentDescriptor.targets[i];
            if (targetDescriptor.format == WGPUTextureFormat_Undefined)
                continue;

            MTLDataType fragmentFunctionReturnType = MTLDataTypeNone;
            if (returnTypes) {
                if (auto it = returnTypes->find(i); it != returnTypes->end())
                    fragmentFunctionReturnType = it->value;
            }
            const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[i];

            if (Texture::isDepthOrStencilFormat(targetDescriptor.format) || !Texture::isRenderableFormat(targetDescriptor.format, *this))
                return returnInvalidRenderPipeline(*this, isAsync, "Depth / stencil format passed to color format"_s);

            bytesPerSample = roundUpToMultipleOf(Texture::renderTargetPixelByteAlignment(targetDescriptor.format), bytesPerSample);
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
                return returnInvalidRenderPipeline(*this, isAsync, [NSString stringWithFormat:@"pipeline creation - color target pixel format(%u) for location(%zu) is incompatible with shader output data type of %zu", i, mtlColorAttachment.pixelFormat, fragmentFunctionReturnType]);

        }

        if (bytesPerSample > limits().maxColorAttachmentBytesPerSample)
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

    mtlRenderPipelineDescriptor.rasterSampleCount = descriptor.multisample.count ?: 1;
    mtlRenderPipelineDescriptor.alphaToCoverageEnabled = descriptor.multisample.alphaToCoverageEnabled;
    if (descriptor.multisample.alphaToCoverageEnabled) {
        if (usesSampleMask)
            return returnInvalidRenderPipeline(*this, isAsync, "Can not use sampleMask with alphaToCoverage"_s);
        if (!descriptor.fragment)
            return returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s);
        if (!descriptor.fragment->targetCount || !hasAlphaChannel(descriptor.fragment->targets[0].format))
            return returnInvalidRenderPipeline(*this, isAsync, "Using alphaToCoverage requires a fragment state"_s);
    }
    RELEASE_ASSERT([mtlRenderPipelineDescriptor respondsToSelector:@selector(setSampleMask:)]);
    uint32_t sampleMask = RenderBundleEncoder::defaultSampleMask;
    if (auto mask = descriptor.multisample.mask; mask != sampleMask) {
        [mtlRenderPipelineDescriptor setSampleMask:mask];
        sampleMask = mask;
    }

    if (descriptor.vertex.bufferCount)
        mtlRenderPipelineDescriptor.vertexDescriptor = createVertexDescriptor(descriptor.vertex);

    MTLDepthClipMode mtlDepthClipMode = MTLDepthClipModeClip;
    if (descriptor.primitive.nextInChain) {
        if (!hasFeature(WGPUFeatureName_DepthClipControl) || descriptor.primitive.nextInChain->sType != WGPUSType_PrimitiveDepthClipControl || descriptor.primitive.nextInChain->next)
            return RenderPipeline::createInvalid(*this);

        auto* depthClipControl = reinterpret_cast<const WGPUPrimitiveDepthClipControl*>(descriptor.primitive.nextInChain);

        if (depthClipControl->unclippedDepth)
            mtlDepthClipMode = MTLDepthClipModeClamp;
    }

    mtlRenderPipelineDescriptor.inputPrimitiveTopology = topologyType(descriptor.primitive.topology);

    // These properties are to be used by the render command encoder, not the render pipeline.
    // Therefore, the render pipeline stores these, and when the render command encoder is assigned
    // a pipeline, the render command encoder can get these information out of the render pipeline.
    auto mtlPrimitiveType = primitiveType(descriptor.primitive.topology);
    auto mtlIndexType = indexType(descriptor.primitive.stripIndexFormat);
    auto mtlFrontFace = frontFace(descriptor.primitive.frontFace);
    auto mtlCullMode = cullMode(descriptor.primitive.cullMode);

    NSError *error = nil;
    id<MTLRenderPipelineState> renderPipelineState = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    if (error || !renderPipelineState)
        return RenderPipeline::createInvalid(*this);

    if (!pipelineLayout)
        return RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthClipMode, depthStencilDescriptor, generatePipelineLayout(bindGroupEntries), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, *this);

    return RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthClipMode, depthStencilDescriptor, const_cast<PipelineLayout&>(*pipelineLayout), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, *this);
}

void Device::createRenderPipelineAsync(const WGPURenderPipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, String&& message)>&& callback)
{
    auto pipeline = createRenderPipeline(descriptor, true);
    instance().scheduleWork([protectedThis = Ref { *this }, pipeline, callback = WTFMove(callback)]() mutable {
        callback(pipeline->isValid() ? WGPUCreatePipelineAsyncStatus_Success : WGPUCreatePipelineAsyncStatus_ValidationError, WTFMove(pipeline), { });
    });
}

RenderPipeline::RenderPipeline(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, MTLDepthClipMode clipMode, MTLDepthStencilDescriptor *depthStencilDescriptor, Ref<PipelineLayout>&& pipelineLayout, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, uint32_t sampleMask, Device& device)
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
    , m_depthStencilDescriptor(depthStencilDescriptor)
    , m_depthStencilState(depthStencilDescriptor ? [device.device() newDepthStencilStateWithDescriptor:depthStencilDescriptor] : nil)
    , m_pipelineLayout(WTFMove(pipelineLayout))
{
}

RenderPipeline::RenderPipeline(Device& device)
    : m_device(device)
    , m_pipelineLayout(PipelineLayout::createInvalid(device))
{
}

RenderPipeline::~RenderPipeline() = default;

RefPtr<BindGroupLayout> RenderPipeline::getBindGroupLayout(uint32_t groupIndex)
{
    if (!isValid()) {
        m_device->generateAValidationError("getBindGroupLayout: RenderPipeline is invalid"_s);
        m_pipelineLayout->makeInvalid();
        return nullptr;
    }

    if (groupIndex >= m_pipelineLayout->numberOfBindGroupLayouts()) {
        m_device->generateAValidationError("getBindGroupLayout: groupIndex is out of range"_s);
        m_pipelineLayout->makeInvalid();
        return nullptr;
    }

    return &m_pipelineLayout->bindGroupLayout(groupIndex);
}

void RenderPipeline::setLabel(String&&)
{
    // MTLRenderPipelineState's labels are read-only.
}

id<MTLDepthStencilState> RenderPipeline::depthStencilState() const
{
    return m_depthStencilState;
}

bool RenderPipeline::validateDepthStencilState(bool depthReadOnly, bool stencilReadOnly) const
{
    if (depthReadOnly && m_depthStencilDescriptor.depthWriteEnabled)
        return false;

    if (stencilReadOnly && (m_depthStencilDescriptor.frontFaceStencil.writeMask || m_depthStencilDescriptor.backFaceStencil.writeMask))
        return false;

    return true;
}

PipelineLayout& RenderPipeline::pipelineLayout() const
{
    return m_pipelineLayout;
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
    return WebGPU::releaseToAPI(WebGPU::fromAPI(renderPipeline).getBindGroupLayout(groupIndex));
}

void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, const char* label)
{
    WebGPU::fromAPI(renderPipeline).setLabel(WebGPU::fromAPI(label));
}

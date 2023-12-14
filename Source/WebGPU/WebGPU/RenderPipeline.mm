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

static Ref<RenderPipeline> returnInvalidRenderPipeline(WebGPU::Device &object, bool isAsync)
{
    if (!isAsync)
        object.generateAValidationError("createRenderPipeline failed"_s);
    return RenderPipeline::createInvalid(object);
}

Ref<RenderPipeline> Device::createRenderPipeline(const WGPURenderPipelineDescriptor& descriptor, bool isAsync)
{
    if (!validateRenderPipeline(descriptor) || !isValid())
        return RenderPipeline::createInvalid(*this);

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
                return returnInvalidRenderPipeline(*this, isAsync);
        }
    }

    std::optional<PipelineLayout> vertexPipelineLayout { std::nullopt };
    {
        if (descriptor.vertex.nextInChain)
            return RenderPipeline::createInvalid(*this);

        const auto& vertexModule = WebGPU::fromAPI(descriptor.vertex.module);
        if (!vertexModule.isValid())
            return returnInvalidRenderPipeline(*this, isAsync);

        const auto& vertexFunctionName = fromAPI(descriptor.vertex.entryPoint);
        auto libraryCreationResult = createLibrary(m_device, vertexModule, pipelineLayout, vertexFunctionName.length() ? vertexFunctionName : vertexModule.defaultVertexEntryPoint(), label);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync);

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout)
            addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout);
        auto [constantValues, wgslConstantValues] = createConstantValues(descriptor.vertex.constantCount, descriptor.vertex.constants, entryPointInformation);
        auto vertexFunction = createFunction(libraryCreationResult->library, entryPointInformation, constantValues, label);
        if (!vertexFunction || vertexFunction.functionType != MTLFunctionTypeVertex || entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync);
        mtlRenderPipelineDescriptor.vertexFunction = vertexFunction;
    }

    std::optional<PipelineLayout> fragmentPipelineLayout { std::nullopt };
    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        if (fragmentDescriptor.nextInChain)
            return RenderPipeline::createInvalid(*this);

        const auto& fragmentModule = WebGPU::fromAPI(fragmentDescriptor.module);
        if (!fragmentModule.isValid())
            return returnInvalidRenderPipeline(*this, isAsync);

        const auto& fragmentFunctionName = fromAPI(fragmentDescriptor.entryPoint);

        auto libraryCreationResult = createLibrary(m_device, fragmentModule, pipelineLayout, fragmentFunctionName.length() ? fragmentFunctionName : fragmentModule.defaultFragmentEntryPoint(), label);
        if (!libraryCreationResult)
            return returnInvalidRenderPipeline(*this, isAsync);

        const auto& entryPointInformation = libraryCreationResult->entryPointInformation;
        if (!pipelineLayout)
            addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout);

        auto [constantValues, wgslConstantValues] = createConstantValues(fragmentDescriptor.constantCount, fragmentDescriptor.constants, entryPointInformation);
        auto fragmentFunction = createFunction(libraryCreationResult->library, entryPointInformation, constantValues, label);
        if (!fragmentFunction || fragmentFunction.functionType != MTLFunctionTypeFragment || entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
            return returnInvalidRenderPipeline(*this, isAsync);
        mtlRenderPipelineDescriptor.fragmentFunction = fragmentFunction;

        for (uint32_t i = 0; i < fragmentDescriptor.targetCount; ++i) {
            const auto& targetDescriptor = fragmentDescriptor.targets[i];
            if (targetDescriptor.format == WGPUTextureFormat_Undefined)
                continue;

            const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[i];

            mtlColorAttachment.pixelFormat = Texture::pixelFormat(targetDescriptor.format);

            mtlColorAttachment.writeMask = colorWriteMask(targetDescriptor.writeMask);

            if (targetDescriptor.blend) {
                mtlColorAttachment.blendingEnabled = YES;

                const auto& alphaBlend = targetDescriptor.blend->alpha;
                mtlColorAttachment.alphaBlendOperation = blendOperation(alphaBlend.operation);
                mtlColorAttachment.sourceAlphaBlendFactor = blendFactor(alphaBlend.srcFactor);
                mtlColorAttachment.destinationAlphaBlendFactor = blendFactor(alphaBlend.dstFactor);

                const auto& colorBlend = targetDescriptor.blend->color;
                mtlColorAttachment.rgbBlendOperation = blendOperation(colorBlend.operation);
                mtlColorAttachment.sourceRGBBlendFactor = blendFactor(colorBlend.srcFactor);
                mtlColorAttachment.destinationRGBBlendFactor = blendFactor(colorBlend.dstFactor);
            } else
                mtlColorAttachment.blendingEnabled = NO;
        }
    }

    MTLDepthStencilDescriptor *depthStencilDescriptor = nil;
    float depthBias = 0.f, depthBiasSlopeScale = 0.f, depthBiasClamp = 0.f;
    if (auto depthStencil = descriptor.depthStencil) {
        MTLPixelFormat depthStencilFormat = Texture::pixelFormat(depthStencil->format);
        bool isStencilOnlyFormat = Device::isStencilOnlyFormat(depthStencilFormat);
        mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = isStencilOnlyFormat ? MTLPixelFormatInvalid : depthStencilFormat;
        if (Texture::stencilOnlyAspectMetalFormat(depthStencil->format))
            mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = depthStencilFormat;

        depthStencilDescriptor = [MTLDepthStencilDescriptor new];
        depthStencilDescriptor.depthCompareFunction = convertToMTLCompare(depthStencil->depthCompare);
        depthStencilDescriptor.depthWriteEnabled = depthStencil->depthWriteEnabled;
        populateStencilOperation(depthStencilDescriptor.frontFaceStencil, depthStencil->stencilFront, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        populateStencilOperation(depthStencilDescriptor.backFaceStencil, depthStencil->stencilBack, depthStencil->stencilReadMask, depthStencil->stencilWriteMask);
        depthBias = depthStencil->depthBias;
        depthBiasSlopeScale = depthStencil->depthBiasSlopeScale;
        depthBiasClamp = depthStencil->depthBiasClamp;
    }

    mtlRenderPipelineDescriptor.rasterSampleCount = descriptor.multisample.count ?: 1;
    mtlRenderPipelineDescriptor.alphaToCoverageEnabled = descriptor.multisample.alphaToCoverageEnabled;
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

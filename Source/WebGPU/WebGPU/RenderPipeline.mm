/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
        return MTLPrimitiveTopologyClassLine;
    case WGPUPrimitiveTopology_TriangleList:
    case WGPUPrimitiveTopology_LineList:
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

    // FIXME: we additionally reject valid descriptors using unimplemented features.
    // Remove these checks once we support them.

    UNUSED_PARAM(descriptor);
    // Does not support module constants in vertex shaders
    if (descriptor.vertex.constantCount)
        return false;

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        // Does not support module constants in fragment shaders
        if (fragmentDescriptor.constantCount)
            return false;
    }

    return true;
}

static MTLCompareFunction convertToMTLCompare(WGPUCompareFunction comparison)
{
    switch (comparison) {
    case WGPUCompareFunction_Undefined:
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
    case WGPUCompareFunction_Always:
    case WGPUCompareFunction_Force32:
        return MTLCompareFunctionAlways;
    }
}

static MTLVertexFormat vertexFormat(WGPUVertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case WGPUVertexFormat_Uint8x2:
        return MTLVertexFormatUInt2;
    case WGPUVertexFormat_Uint8x4:
        return MTLVertexFormatUInt4;
    case WGPUVertexFormat_Sint8x2:
        return MTLVertexFormatInt2;
    case WGPUVertexFormat_Sint8x4:
        return MTLVertexFormatInt4;
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
    case WGPUVertexFormat_Force32:
    case WGPUVertexFormat_Undefined:
        ASSERT_NOT_REACHED();
        return MTLVertexFormatFloat;
    }
}

static MTLVertexStepFunction stepFunction(WGPUVertexStepMode stepMode)
{
    switch (stepMode) {
    case WGPUVertexStepMode_Vertex:
        return MTLVertexStepFunctionPerVertex;
    case WGPUVertexStepMode_Instance:
        return MTLVertexStepFunctionPerInstance;
    case WGPUVertexStepMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLVertexStepFunctionPerVertex;
    }
}

static MTLVertexDescriptor *createVertexDescriptor(WGPUVertexState vertexState)
{
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];

    for (size_t bufferIndex = 0; bufferIndex < vertexState.bufferCount; ++bufferIndex) {
        auto& buffer = vertexState.buffers[bufferIndex];
        vertexDescriptor.layouts[bufferIndex].stride = buffer.arrayStride;
        vertexDescriptor.layouts[bufferIndex].stepFunction = stepFunction(buffer.stepMode);
        // FIXME: need to assign stepRate with per-instance data?
        for (size_t i = 0; i < buffer.attributeCount; ++i) {
            auto& attribute = buffer.attributes[i];
            const auto& mtlAttribute = vertexDescriptor.attributes[i];
            mtlAttribute.format = vertexFormat(attribute.format);
            mtlAttribute.bufferIndex = bufferIndex;
            mtlAttribute.offset = attribute.offset;
        }
    }

    return vertexDescriptor;
}

Ref<RenderPipeline> Device::createRenderPipeline(const WGPURenderPipelineDescriptor& descriptor)
{
    if (!validateRenderPipeline(descriptor))
        return RenderPipeline::createInvalid(*this);

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=249345 don't unconditionally set this to YES
    mtlRenderPipelineDescriptor.supportIndirectCommandBuffers = YES;

    {
        if (descriptor.vertex.nextInChain)
            return RenderPipeline::createInvalid(*this);

        const auto& vertexModule = WebGPU::fromAPI(descriptor.vertex.module);
        const auto& vertexFunctionName = String::fromLatin1(descriptor.vertex.entryPoint);

        const auto vertexFunction = vertexModule.getNamedFunction(vertexFunctionName);
        if (!vertexFunction)
            return RenderPipeline::createInvalid(*this);

        mtlRenderPipelineDescriptor.vertexFunction = vertexFunction;
    }

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        if (fragmentDescriptor.nextInChain)
            return RenderPipeline::createInvalid(*this);

        const auto& fragmentModule = WebGPU::fromAPI(fragmentDescriptor.module);
        const auto& fragmentFunctionName = String::fromLatin1(fragmentDescriptor.entryPoint);

        const auto fragmentFunction = fragmentModule.getNamedFunction(fragmentFunctionName);

        if (!fragmentFunction)
            return RenderPipeline::createInvalid(*this);

        mtlRenderPipelineDescriptor.fragmentFunction = fragmentFunction;

        for (uint32_t i = 0; i < fragmentDescriptor.targetCount; ++i) {
            const auto& targetDescriptor = fragmentDescriptor.targets[i];
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

    id<MTLDepthStencilState> mtlDepthStencilState = nil;
    if (auto depthStencil = descriptor.depthStencil) {
        mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = Texture::pixelFormat(depthStencil->format);

        auto depthStencilState = [MTLDepthStencilDescriptor new];
        depthStencilState.depthCompareFunction = convertToMTLCompare(depthStencil->depthCompare);
        depthStencilState.depthWriteEnabled = depthStencil->depthWriteEnabled;
        // FIXME: set stencil state
        mtlDepthStencilState = [m_device newDepthStencilStateWithDescriptor:depthStencilState];
    }

    mtlRenderPipelineDescriptor.rasterSampleCount = descriptor.multisample.count ?: 1;
    mtlRenderPipelineDescriptor.alphaToCoverageEnabled = descriptor.multisample.alphaToCoverageEnabled;

    if (descriptor.vertex.bufferCount)
        mtlRenderPipelineDescriptor.vertexDescriptor = createVertexDescriptor(descriptor.vertex);

    if (descriptor.primitive.nextInChain)
        return RenderPipeline::createInvalid(*this);

    mtlRenderPipelineDescriptor.inputPrimitiveTopology = topologyType(descriptor.primitive.topology);

    // These properties are to be used by the render command encoder, not the render pipeline.
    // Therefore, the render pipeline stores these, and when the render command encoder is assigned
    // a pipeline, the render command encoder can get these information out of the render pipeline.
    auto mtlPrimitiveType = primitiveType(descriptor.primitive.topology);
    auto mtlIndexType = indexType(descriptor.primitive.stripIndexFormat);
    auto mtlFrontFace = frontFace(descriptor.primitive.frontFace);
    auto mtlCullMode = cullMode(descriptor.primitive.cullMode);

    // FIXME: GPUPrimitiveState.unclippedDepth

    id<MTLRenderPipelineState> renderPipelineState = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:nil];
    if (!renderPipelineState)
        return RenderPipeline::createInvalid(*this);

    return RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, mtlDepthStencilState, descriptor.vertex.bufferCount, *this);
}

void Device::createRenderPipelineAsync(const WGPURenderPipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, String&& message)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(descriptor);
    instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback(WGPUCreatePipelineAsyncStatus_Error, RenderPipeline::createInvalid(strongThis), { });
    });
}

RenderPipeline::RenderPipeline(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, id<MTLDepthStencilState> depthStencilState, uint32_t vertexShaderInputBufferCount, Device& device)
    : m_renderPipelineState(renderPipelineState)
    , m_device(device)
    , m_primitiveType(primitiveType)
    , m_indexType(indexType)
    , m_frontFace(frontFace)
    , m_cullMode(cullMode)
    , m_depthStencilState(depthStencilState)
    , m_vertexShaderInputBufferCount(vertexShaderInputBufferCount)
{
}

RenderPipeline::RenderPipeline(Device& device)
    : m_device(device)
{
}

RenderPipeline::~RenderPipeline() = default;

BindGroupLayout* RenderPipeline::getBindGroupLayout(uint32_t groupIndex)
{
    UNUSED_PARAM(groupIndex);
    return nullptr;
}

void RenderPipeline::setLabel(String&&)
{
    // MTLRenderPipelineState's labels are read-only.
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline)
{
    WebGPU::fromAPI(renderPipeline).deref();
}

WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex)
{
    return WebGPU::fromAPI(renderPipeline).getBindGroupLayout(groupIndex);
}

void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, const char* label)
{
    WebGPU::fromAPI(renderPipeline).setLabel(WebGPU::fromAPI(label));
}

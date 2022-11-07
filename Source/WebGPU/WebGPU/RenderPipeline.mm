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

    // Does not support module constants in vertex shaders
    if (descriptor.vertex.constantCount)
        return false;

    // Does not support vertex shader input buffers
    if (descriptor.vertex.bufferCount)
        return false;

    if (descriptor.fragment) {
        const auto& fragmentDescriptor = *descriptor.fragment;

        // Does not support module constants in fragment shaders
        if (fragmentDescriptor.constantCount)
            return false;
    }

    // Does not support depth stencils.
    if (descriptor.depthStencil)
        return false;

    // Does not support multisampling
    if (descriptor.multisample.count > 1)
        return false;

    if (descriptor.multisample.alphaToCoverageEnabled)
        return false;

    return true;
}

Ref<RenderPipeline> Device::createRenderPipeline(const WGPURenderPipelineDescriptor& descriptor)
{
    if (!validateRenderPipeline(descriptor))
        return RenderPipeline::createInvalid(*this);

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];

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

    if (descriptor.primitive.nextInChain)
        return RenderPipeline::createInvalid(*this);

    // FIXME: need to set inputPrimitiveTopology based on GPUPrimitiveState.topology?

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

    return RenderPipeline::create(renderPipelineState, mtlPrimitiveType, mtlIndexType, mtlFrontFace, mtlCullMode, *this);
}

void Device::createRenderPipelineAsync(const WGPURenderPipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, String&& message)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(descriptor);
    instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback(WGPUCreatePipelineAsyncStatus_Error, RenderPipeline::createInvalid(strongThis), { });
    });
}

RenderPipeline::RenderPipeline(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, Device& device)
    : m_renderPipelineState(renderPipelineState)
    , m_device(device)
    , m_primitiveType(primitiveType)
    , m_indexType(indexType)
    , m_frontFace(frontFace)
    , m_cullMode(cullMode)
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

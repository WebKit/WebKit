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
#import "RenderPassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "QuerySet.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

namespace WebGPU {

RenderPassEncoder::RenderPassEncoder(id<MTLRenderCommandEncoder> renderCommandEncoder, Device& device)
    : m_renderCommandEncoder(renderCommandEncoder)
    , m_device(device)
{
}

RenderPassEncoder::RenderPassEncoder(Device& device)
    : m_device(device)
{
}

RenderPassEncoder::~RenderPassEncoder()
{
    // FIXME: Metal driver requires the command encoder to end before being destroyed.
    // Might have to explicitly end encoding here if the user forgets to?
}

void RenderPassEncoder::beginOcclusionQuery(uint32_t queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void RenderPassEncoder::beginPipelineStatisticsQuery(const QuerySet& querySet, uint32_t queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RenderPassEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-draw
    [m_renderCommandEncoder
        drawPrimitives:m_primitiveType
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    UNUSED_PARAM(indexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstIndex);
    UNUSED_PARAM(baseVertex);
    UNUSED_PARAM(firstInstance);
}

void RenderPassEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RenderPassEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RenderPassEncoder::endOcclusionQuery()
{

}

void RenderPassEncoder::endPass()
{
    [m_renderCommandEncoder endEncoding];
}

void RenderPassEncoder::endPipelineStatisticsQuery()
{

}

void RenderPassEncoder::executeBundles(Vector<std::reference_wrapper<const RenderBundle>>&& bundles)
{
    UNUSED_PARAM(bundles);
}

void RenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    [m_renderCommandEncoder insertDebugSignpost:markerLabel];
}

bool RenderPassEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderPassEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    [m_renderCommandEncoder popDebugGroup];
}

void RenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_renderCommandEncoder pushDebugGroup:groupLabel];
}

void RenderPassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);

    for (const auto& resource : group.resources())
        [m_renderCommandEncoder useResource:resource.mtlResource usage:resource.usage stages:resource.renderStages];

    [m_renderCommandEncoder setVertexBuffer:group.vertexArgumentBuffer() offset:0 atIndex:groupIndex];
    [m_renderCommandEncoder setFragmentBuffer:group.fragmentArgumentBuffer() offset:0 atIndex:groupIndex];
}

void RenderPassEncoder::setBlendConstant(const WGPUColor& color)
{
    UNUSED_PARAM(color);
}

void RenderPassEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(format);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RenderPassEncoder::setPipeline(const RenderPipeline& pipeline)
{
    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-setpipeline.
    m_primitiveType = pipeline.primitiveType();

    [m_renderCommandEncoder setRenderPipelineState:pipeline.renderPipelineState()];
    if (pipeline.depthStencilState())
        [m_renderCommandEncoder setDepthStencilState:pipeline.depthStencilState()];
    [m_renderCommandEncoder setCullMode:pipeline.cullMode()];
    [m_renderCommandEncoder setFrontFacingWinding:pipeline.frontFace()];
}

void RenderPassEncoder::setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void RenderPassEncoder::setStencilReference(uint32_t reference)
{
    UNUSED_PARAM(reference);
}

void RenderPassEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    [m_renderCommandEncoder setVertexBuffer:buffer.buffer() offset:offset atIndex:slot];
}

void RenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    [m_renderCommandEncoder setViewport: { x, y, width, height, minDepth, maxDepth } ];
}

void RenderPassEncoder::setLabel(String&& label)
{
    m_renderCommandEncoder.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).deref();
}

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex)
{
    WebGPU::fromAPI(renderPassEncoder).beginOcclusionQuery(queryIndex);
}

void wgpuRenderPassEncoderBeginPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    WebGPU::fromAPI(renderPassEncoder).beginPipelineStatisticsQuery(WebGPU::fromAPI(querySet), queryIndex);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endOcclusionQuery();
}

void wgpuRenderPassEncoderEndPass(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endPass();
}

void wgpuRenderPassEncoderEndPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endPipelineStatisticsQuery();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, uint32_t bundlesCount, const WGPURenderBundle* bundles)
{
    Vector<std::reference_wrapper<const WebGPU::RenderBundle>> bundlesToForward;
    for (uint32_t i = 0; i < bundlesCount; ++i)
        bundlesToForward.append(WebGPU::fromAPI(bundles[i]));
    WebGPU::fromAPI(renderPassEncoder).executeBundles(WTFMove(bundlesToForward));
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderPassEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).popDebugGroup();
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderPassEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(renderPassEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color)
{
    WebGPU::fromAPI(renderPassEncoder).setBlendConstant(*color);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderPassEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderPassEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    WebGPU::fromAPI(renderPassEncoder).setScissorRect(x, y, width, height);
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference)
{
    WebGPU::fromAPI(renderPassEncoder).setStencilReference(reference);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderPassEncoder).setVertexBuffer(slot, WebGPU::fromAPI(buffer), offset, size);
}

void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    WebGPU::fromAPI(renderPassEncoder).setViewport(x, y, width, height, minDepth, maxDepth);
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, const char* label)
{
    WebGPU::fromAPI(renderPassEncoder).setLabel(WebGPU::fromAPI(label));
}

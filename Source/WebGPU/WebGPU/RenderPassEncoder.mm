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

#import "BindGroup.h"
#import "Buffer.h"
#import "QuerySet.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

namespace WebGPU {

RenderPassEncoder::RenderPassEncoder(id<MTLRenderCommandEncoder> renderCommandEncoder)
    : m_renderCommandEncoder(renderCommandEncoder)
{
}

RenderPassEncoder::~RenderPassEncoder() = default;

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
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
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

}

void RenderPassEncoder::endPipelineStatisticsQuery()
{

}

void RenderPassEncoder::executeBundles(Vector<std::reference_wrapper<const RenderBundle>>&& bundles)
{
    UNUSED_PARAM(bundles);
}

void RenderPassEncoder::insertDebugMarker(const char* markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RenderPassEncoder::popDebugGroup()
{

}

void RenderPassEncoder::pushDebugGroup(const char* groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RenderPassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    UNUSED_PARAM(groupIndex);
    UNUSED_PARAM(group);
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);
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
    UNUSED_PARAM(pipeline);
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
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(minDepth);
    UNUSED_PARAM(maxDepth);
}

void RenderPassEncoder::setLabel(const char* label)
{
    m_renderCommandEncoder.label = [NSString stringWithCString:label encoding:NSUTF8StringEncoding];
}

} // namespace WebGPU

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder)
{
    delete renderPassEncoder;
}

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex)
{
    renderPassEncoder->renderPassEncoder->beginOcclusionQuery(queryIndex);
}

void wgpuRenderPassEncoderBeginPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    renderPassEncoder->renderPassEncoder->beginPipelineStatisticsQuery(querySet->querySet, queryIndex);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    renderPassEncoder->renderPassEncoder->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    renderPassEncoder->renderPassEncoder->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    renderPassEncoder->renderPassEncoder->drawIndexedIndirect(indirectBuffer->buffer, indirectOffset);
}

void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    renderPassEncoder->renderPassEncoder->drawIndirect(indirectBuffer->buffer, indirectOffset);
}

void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder)
{
    renderPassEncoder->renderPassEncoder->endOcclusionQuery();
}

void wgpuRenderPassEncoderEndPass(WGPURenderPassEncoder renderPassEncoder)
{
    renderPassEncoder->renderPassEncoder->endPass();
}

void wgpuRenderPassEncoderEndPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder)
{
    renderPassEncoder->renderPassEncoder->endPipelineStatisticsQuery();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, uint32_t bundlesCount, const WGPURenderBundle* bundles)
{
    Vector<std::reference_wrapper<const WebGPU::RenderBundle>> bundlesToForward;
    for (uint32_t i = 0; i < bundlesCount; ++i)
        bundlesToForward.append(bundles[i]->renderBundle);
    renderPassEncoder->renderPassEncoder->executeBundles(WTFMove(bundlesToForward));
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, const char* markerLabel)
{
    renderPassEncoder->renderPassEncoder->insertDebugMarker(markerLabel);
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder)
{
    renderPassEncoder->renderPassEncoder->popDebugGroup();
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, const char* groupLabel)
{
    renderPassEncoder->renderPassEncoder->pushDebugGroup(groupLabel);
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    renderPassEncoder->renderPassEncoder->setBindGroup(groupIndex, group->bindGroup, dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color)
{
    renderPassEncoder->renderPassEncoder->setBlendConstant(*color);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    renderPassEncoder->renderPassEncoder->setIndexBuffer(buffer->buffer, format, offset, size);
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline)
{
    renderPassEncoder->renderPassEncoder->setPipeline(pipeline->renderPipeline);
}

void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    renderPassEncoder->renderPassEncoder->setScissorRect(x, y, width, height);
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference)
{
    renderPassEncoder->renderPassEncoder->setStencilReference(reference);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    renderPassEncoder->renderPassEncoder->setVertexBuffer(slot, buffer->buffer, offset, size);
}

void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    renderPassEncoder->renderPassEncoder->setViewport(x, y, width, height, minDepth, maxDepth);
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, const char* label)
{
    renderPassEncoder->renderPassEncoder->setLabel(label);
}

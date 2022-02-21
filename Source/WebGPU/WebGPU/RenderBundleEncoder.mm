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
#import "RenderBundleEncoder.h"

#import "BindGroup.h"
#import "Buffer.h"
#import "Device.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"
#import "WebGPUExt.h"

namespace WebGPU {

RefPtr<RenderBundleEncoder> Device::createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderBundleEncoder::create();
}

RenderBundleEncoder::RenderBundleEncoder() = default;

RenderBundleEncoder::~RenderBundleEncoder() = default;

void RenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void RenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    UNUSED_PARAM(indexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstIndex);
    UNUSED_PARAM(baseVertex);
    UNUSED_PARAM(firstInstance);
}

void RenderBundleEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RenderBundleEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

RefPtr<RenderBundle> RenderBundleEncoder::finish(const WGPURenderBundleDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderBundle::create();
}

void RenderBundleEncoder::insertDebugMarker(const char* markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RenderBundleEncoder::popDebugGroup()
{

}

void RenderBundleEncoder::pushDebugGroup(const char* groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RenderBundleEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    UNUSED_PARAM(groupIndex);
    UNUSED_PARAM(group);
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);
}

void RenderBundleEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(format);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RenderBundleEncoder::setPipeline(const RenderPipeline& pipeline)
{
    UNUSED_PARAM(pipeline);
}

void RenderBundleEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RenderBundleEncoder::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder)
{
    delete renderBundleEncoder;
}

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    renderBundleEncoder->renderBundleEncoder->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    renderBundleEncoder->renderBundleEncoder->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    renderBundleEncoder->renderBundleEncoder->drawIndexedIndirect(indirectBuffer->buffer, indirectOffset);
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    renderBundleEncoder->renderBundleEncoder->drawIndirect(indirectBuffer->buffer, indirectOffset);
}

WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, const WGPURenderBundleDescriptor* descriptor)
{
    auto result = renderBundleEncoder->renderBundleEncoder->finish(descriptor);
    return result ? new WGPURenderBundleImpl { result.releaseNonNull() } : nullptr;
}

void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, const char* markerLabel)
{
    renderBundleEncoder->renderBundleEncoder->insertDebugMarker(markerLabel);
}

void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder)
{
    renderBundleEncoder->renderBundleEncoder->popDebugGroup();
}

void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, const char* groupLabel)
{
    renderBundleEncoder->renderBundleEncoder->pushDebugGroup(groupLabel);
}

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    renderBundleEncoder->renderBundleEncoder->setBindGroup(groupIndex, group->bindGroup, dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    renderBundleEncoder->renderBundleEncoder->setIndexBuffer(buffer->buffer, format, offset, size);
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline)
{
    renderBundleEncoder->renderBundleEncoder->setPipeline(pipeline->renderPipeline);
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    renderBundleEncoder->renderBundleEncoder->setVertexBuffer(slot, buffer->buffer, offset, size);
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, const char* label)
{
    renderBundleEncoder->renderBundleEncoder->setLabel(label);
}

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

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "Device.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

namespace WebGPU {

Ref<RenderBundleEncoder> Device::createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderBundleEncoder::createInvalid(*this);
}

RenderBundleEncoder::RenderBundleEncoder(id<MTLIndirectCommandBuffer> indirectCommandBuffer, Device& device)
    : m_indirectCommandBuffer(indirectCommandBuffer)
    , m_device(device)
{
}

RenderBundleEncoder::RenderBundleEncoder(Device& device)
    : m_device(device)
{
}

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

Ref<RenderBundle> RenderBundleEncoder::finish(const WGPURenderBundleDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderBundle::createInvalid(m_device);
}

void RenderBundleEncoder::insertDebugMarker(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    // MTLIndirectCommandBuffers don't support debug commands.
}

bool RenderBundleEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderBundleEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::pushDebugGroup(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
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

void RenderBundleEncoder::setLabel(String&& label)
{
    m_indirectCommandBuffer.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).deref();
}

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, const WGPURenderBundleDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(renderBundleEncoder).finish(*descriptor));
}

void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).popDebugGroup();
}

void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(renderBundleEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderBundleEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setVertexBuffer(slot, WebGPU::fromAPI(buffer), offset, size);
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, const char* label)
{
    WebGPU::fromAPI(renderBundleEncoder).setLabel(WebGPU::fromAPI(label));
}

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteRenderPassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteRenderPassEncoderProxy::RemoteRenderPassEncoderProxy(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

RemoteRenderPassEncoderProxy::~RemoteRenderPassEncoderProxy()
{
}

void RemoteRenderPassEncoderProxy::setPipeline(const PAL::WebGPU::RenderPipeline& renderPipeline)
{
    UNUSED_PARAM(renderPipeline);
}

void RemoteRenderPassEncoderProxy::setIndexBuffer(const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::IndexFormat indexFormat, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(indexFormat);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderPassEncoderProxy::setVertexBuffer(PAL::WebGPU::Index32 slot, const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderPassEncoderProxy::draw(PAL::WebGPU::Size32 vertexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstVertex, PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderPassEncoderProxy::drawIndexed(PAL::WebGPU::Size32 indexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstIndex,
    PAL::WebGPU::SignedOffset32 baseVertex,
    PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(indexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstIndex);
    UNUSED_PARAM(baseVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderPassEncoderProxy::drawIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderPassEncoderProxy::drawIndexedIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectBuffer);
}

void RemoteRenderPassEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsets);
}

void RemoteRenderPassEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    PAL::WebGPU::Size64 dynamicOffsetsDataStart,
    PAL::WebGPU::Size32 dynamicOffsetsDataLength)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsetsArrayBuffer);
    UNUSED_PARAM(dynamicOffsetsArrayBufferLength);
    UNUSED_PARAM(dynamicOffsetsDataStart);
    UNUSED_PARAM(dynamicOffsetsDataLength);
}

void RemoteRenderPassEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteRenderPassEncoderProxy::popDebugGroup()
{
}

void RemoteRenderPassEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteRenderPassEncoderProxy::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(minDepth);
    UNUSED_PARAM(maxDepth);
}

void RemoteRenderPassEncoderProxy::setScissorRect(PAL::WebGPU::IntegerCoordinate x, PAL::WebGPU::IntegerCoordinate y,
    PAL::WebGPU::IntegerCoordinate width, PAL::WebGPU::IntegerCoordinate height)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void RemoteRenderPassEncoderProxy::setBlendConstant(PAL::WebGPU::Color color)
{
    UNUSED_PARAM(color);
}

void RemoteRenderPassEncoderProxy::setStencilReference(PAL::WebGPU::StencilValue stencilValue)
{
    UNUSED_PARAM(stencilValue);
}

void RemoteRenderPassEncoderProxy::beginOcclusionQuery(PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void RemoteRenderPassEncoderProxy::endOcclusionQuery()
{
}

void RemoteRenderPassEncoderProxy::beginPipelineStatisticsQuery(const PAL::WebGPU::QuerySet& querySet, PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RemoteRenderPassEncoderProxy::endPipelineStatisticsQuery()
{
}

void RemoteRenderPassEncoderProxy::executeBundles(Vector<std::reference_wrapper<PAL::WebGPU::RenderBundle>>&& renderBundles)
{
    UNUSED_PARAM(renderBundles);
}

void RemoteRenderPassEncoderProxy::endPass()
{
}

void RemoteRenderPassEncoderProxy::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

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
#include "RemoteRenderPassEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPURenderPassEncoder.h>

namespace WebKit {

RemoteRenderPassEncoder::RemoteRenderPassEncoder(PAL::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap)
    : m_backing(renderPassEncoder)
    , m_objectHeap(objectHeap)
{
}

RemoteRenderPassEncoder::~RemoteRenderPassEncoder()
{
}

void RemoteRenderPassEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    UNUSED_PARAM(renderPipeline);
}

void RemoteRenderPassEncoder::setIndexBuffer(WebGPUIdentifier buffer, PAL::WebGPU::IndexFormat indexFormat, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(indexFormat);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderPassEncoder::setVertexBuffer(PAL::WebGPU::Index32 slot, WebGPUIdentifier buffer, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderPassEncoder::draw(PAL::WebGPU::Size32 vertexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstVertex, PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderPassEncoder::drawIndexed(PAL::WebGPU::Size32 indexCount, PAL::WebGPU::Size32 instanceCount,
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

void RemoteRenderPassEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderPassEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectBuffer);
}

void RemoteRenderPassEncoder::setBindGroup(PAL::WebGPU::Index32 index, WebGPUIdentifier bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsets);
}

void RemoteRenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteRenderPassEncoder::popDebugGroup()
{
}

void RemoteRenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteRenderPassEncoder::setViewport(float x, float y,
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

void RemoteRenderPassEncoder::setScissorRect(PAL::WebGPU::IntegerCoordinate x, PAL::WebGPU::IntegerCoordinate y,
    PAL::WebGPU::IntegerCoordinate width, PAL::WebGPU::IntegerCoordinate height)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void RemoteRenderPassEncoder::setBlendConstant(WebGPU::Color color)
{
    UNUSED_PARAM(color);
}

void RemoteRenderPassEncoder::setStencilReference(PAL::WebGPU::StencilValue stencilValue)
{
    UNUSED_PARAM(stencilValue);
}

void RemoteRenderPassEncoder::beginOcclusionQuery(PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void RemoteRenderPassEncoder::endOcclusionQuery()
{
}

void RemoteRenderPassEncoder::beginPipelineStatisticsQuery(WebGPUIdentifier querySet, PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RemoteRenderPassEncoder::endPipelineStatisticsQuery()
{
}

void RemoteRenderPassEncoder::executeBundles(Vector<WebGPUIdentifier>&& renderBundles)
{
    UNUSED_PARAM(renderBundles);
}

void RemoteRenderPassEncoder::endPass()
{
}

void RemoteRenderPassEncoder::setLabel(String&& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

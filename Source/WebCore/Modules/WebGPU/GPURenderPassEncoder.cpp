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

#pragma once

#include "config.h"
#include "GPURenderPassEncoder.h"

#include "GPUBuffer.h"

namespace WebCore {

String GPURenderPassEncoder::label() const
{
    return StringImpl::empty();
}

void GPURenderPassEncoder::setLabel(String&&)
{
}

void GPURenderPassEncoder::setPipeline(const GPURenderPipeline&)
{
}

void GPURenderPassEncoder::setIndexBuffer(const GPUBuffer&, GPUIndexFormat, GPUSize64 offset, std::optional<GPUSize64> size)
{
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void GPURenderPassEncoder::setVertexBuffer(GPUIndex32 slot, const GPUBuffer&, GPUSize64 offset, std::optional<GPUSize64> size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void GPURenderPassEncoder::draw(GPUSize32 vertexCount, GPUSize32 instanceCount,
    GPUSize32 firstVertex, GPUSize32 firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void GPURenderPassEncoder::drawIndexed(GPUSize32 indexCount, GPUSize32 instanceCount,
    GPUSize32 firstIndex,
    GPUSignedOffset32 baseVertex,
    GPUSize32 firstInstance)
{
    UNUSED_PARAM(indexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstIndex);
    UNUSED_PARAM(baseVertex);
    UNUSED_PARAM(firstInstance);
}

void GPURenderPassEncoder::drawIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void GPURenderPassEncoder::drawIndexedIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void GPURenderPassEncoder::setBindGroup(GPUIndex32, const GPUBindGroup&,
    std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets)
{
    UNUSED_PARAM(dynamicOffsets);
}

void GPURenderPassEncoder::setBindGroup(GPUIndex32, const GPUBindGroup&,
    const Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    UNUSED_PARAM(dynamicOffsetsData);
    UNUSED_PARAM(dynamicOffsetsDataStart);
    UNUSED_PARAM(dynamicOffsetsDataLength);
}

void GPURenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void GPURenderPassEncoder::popDebugGroup()
{
}

void GPURenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void GPURenderPassEncoder::setViewport(float x, float y,
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

void GPURenderPassEncoder::setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
    GPUIntegerCoordinate width, GPUIntegerCoordinate height)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void GPURenderPassEncoder::setBlendConstant(GPUColor)
{
}

void GPURenderPassEncoder::setStencilReference(GPUStencilValue)
{
}

void GPURenderPassEncoder::beginOcclusionQuery(GPUSize32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void GPURenderPassEncoder::endOcclusionQuery()
{
}

void GPURenderPassEncoder::beginPipelineStatisticsQuery(const GPUQuerySet&, GPUSize32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void GPURenderPassEncoder::endPipelineStatisticsQuery()
{
}

void GPURenderPassEncoder::executeBundles(Vector<RefPtr<GPURenderBundle>>&&)
{
}

void GPURenderPassEncoder::endPass()
{
}

}

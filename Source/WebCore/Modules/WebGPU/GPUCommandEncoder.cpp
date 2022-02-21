/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "GPUCommandEncoder.h"

#include "GPUBuffer.h"
#include "GPUQuerySet.h"

namespace WebCore {

String GPUCommandEncoder::label() const
{
    return m_backing->label();
}

void GPUCommandEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

Ref<GPURenderPassEncoder> GPUCommandEncoder::beginRenderPass(const GPURenderPassDescriptor& renderPassDescriptor)
{
    return GPURenderPassEncoder::create(m_backing->beginRenderPass(renderPassDescriptor.convertToBacking()));
}

Ref<GPUComputePassEncoder> GPUCommandEncoder::beginComputePass(const std::optional<GPUComputePassDescriptor>& computePassDescriptor)
{
    return GPUComputePassEncoder::create(m_backing->beginComputePass(computePassDescriptor ? std::optional { computePassDescriptor->convertToBacking() } : std::nullopt));
}

void GPUCommandEncoder::copyBufferToBuffer(
    const GPUBuffer& source,
    GPUSize64 sourceOffset,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset,
    GPUSize64 size)
{
    m_backing->copyBufferToBuffer(source.backing(), sourceOffset, destination.backing(), destinationOffset, size);
}

void GPUCommandEncoder::copyBufferToTexture(
    const GPUImageCopyBuffer& source,
    const GPUImageCopyTexture& destination,
    const GPUExtent3D& copySize)
{
    m_backing->copyBufferToTexture(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}

void GPUCommandEncoder::copyTextureToBuffer(
    const GPUImageCopyTexture& source,
    const GPUImageCopyBuffer& destination,
    const GPUExtent3D& copySize)
{
    m_backing->copyTextureToBuffer(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}

void GPUCommandEncoder::copyTextureToTexture(
    const GPUImageCopyTexture& source,
    const GPUImageCopyTexture& destination,
    const GPUExtent3D& copySize)
{
    m_backing->copyTextureToTexture(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}


void GPUCommandEncoder::clearBuffer(
    const GPUBuffer& buffer,
    GPUSize64 offset,
    std::optional<GPUSize64> size)
{
    m_backing->clearBuffer(buffer.backing(), offset, size);
}

void GPUCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void GPUCommandEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void GPUCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

void GPUCommandEncoder::writeTimestamp(const GPUQuerySet& querySet, GPUSize32 queryIndex)
{
    m_backing->writeTimestamp(querySet.backing(), queryIndex);
}

void GPUCommandEncoder::resolveQuerySet(
    const GPUQuerySet& querySet,
    GPUSize32 firstQuery,
    GPUSize32 queryCount,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset)
{
    m_backing->resolveQuerySet(querySet.backing(), firstQuery, queryCount, destination.backing(), destinationOffset);
}

static PAL::WebGPU::CommandBufferDescriptor convertToBacking(const std::optional<GPUCommandBufferDescriptor>& commandBufferDescriptor)
{
    if (!commandBufferDescriptor)
        return { };

    return commandBufferDescriptor->convertToBacking();
}

Ref<GPUCommandBuffer> GPUCommandEncoder::finish(const std::optional<GPUCommandBufferDescriptor>& commandBufferDescriptor)
{
    return GPUCommandBuffer::create(m_backing->finish(convertToBacking(commandBufferDescriptor)));
}

}

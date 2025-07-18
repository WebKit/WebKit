/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "GPUCommandBuffer.h"
#include "GPUQuerySet.h"
#include "WebGPUDevice.h"

namespace WebCore {

GPUCommandEncoder::GPUCommandEncoder(Ref<WebGPU::CommandEncoder>&& backing, WebGPU::Device& device)
    : m_backing(WTFMove(backing))
    , m_device(&device)
{
}

String GPUCommandEncoder::label() const
{
    return m_backing->label();
}

void GPUCommandEncoder::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

ExceptionOr<Ref<GPURenderPassEncoder>> GPUCommandEncoder::beginRenderPass(const GPURenderPassDescriptor& renderPassDescriptor)
{
    RefPtr encoder = protectedBacking()->beginRenderPass(renderPassDescriptor.convertToBacking());
    RefPtr device = m_device.get();
    if (!encoder || !device)
        return Exception { ExceptionCode::InvalidStateError, "GPUCommandEncoder.beginRenderPass: Unable to begin render pass."_s };
    return GPURenderPassEncoder::create(encoder.releaseNonNull(), *device);
}

ExceptionOr<Ref<GPUComputePassEncoder>> GPUCommandEncoder::beginComputePass(const std::optional<GPUComputePassDescriptor>& computePassDescriptor)
{
    RefPtr computePass = protectedBacking()->beginComputePass(computePassDescriptor ? std::optional { computePassDescriptor->convertToBacking() } : std::nullopt);
    RefPtr device = m_device.get();
    if (!computePass || !device)
        return Exception { ExceptionCode::InvalidStateError, "GPUCommandEncoder.beginComputePass: Unable to begin compute pass."_s };
    return GPUComputePassEncoder::create(computePass.releaseNonNull(), *device);
}

void GPUCommandEncoder::copyBufferToBuffer(
    const GPUBuffer& source,
    const GPUBuffer& destination,
    std::optional<GPUSize64> size)
{
    return copyBufferToBuffer(source, 0u, destination, 0u, size);
}

void GPUCommandEncoder::copyBufferToBuffer(
    const GPUBuffer& source,
    GPUSize64 sourceOffset,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset,
    std::optional<GPUSize64> size)
{
    protectedBacking()->copyBufferToBuffer(source.backing(), sourceOffset, destination.backing(), destinationOffset, size.value_or(sourceOffset < source.size() ? source.size() - sourceOffset : 0u));
}

void GPUCommandEncoder::copyBufferToTexture(
    const GPUImageCopyBuffer& source,
    const GPUImageCopyTexture& destination,
    const GPUExtent3D& copySize)
{
    protectedBacking()->copyBufferToTexture(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}

void GPUCommandEncoder::copyTextureToBuffer(
    const GPUImageCopyTexture& source,
    const GPUImageCopyBuffer& destination,
    const GPUExtent3D& copySize)
{
    protectedBacking()->copyTextureToBuffer(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}

void GPUCommandEncoder::copyTextureToTexture(
    const GPUImageCopyTexture& source,
    const GPUImageCopyTexture& destination,
    const GPUExtent3D& copySize)
{
    protectedBacking()->copyTextureToTexture(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}


void GPUCommandEncoder::clearBuffer(
    const GPUBuffer& buffer,
    std::optional<GPUSize64> offset,
    std::optional<GPUSize64> size)
{
    protectedBacking()->clearBuffer(buffer.backing(), offset.value_or(0), size);
}

void GPUCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    protectedBacking()->pushDebugGroup(WTFMove(groupLabel));
}

void GPUCommandEncoder::popDebugGroup()
{
    protectedBacking()->popDebugGroup();
}

void GPUCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    protectedBacking()->insertDebugMarker(WTFMove(markerLabel));
}

void GPUCommandEncoder::writeTimestamp(const GPUQuerySet& querySet, GPUSize32 queryIndex)
{
    protectedBacking()->writeTimestamp(querySet.backing(), queryIndex);
}

void GPUCommandEncoder::resolveQuerySet(
    const GPUQuerySet& querySet,
    GPUSize32 firstQuery,
    GPUSize32 queryCount,
    const GPUBuffer& destination,
    GPUSize64 destinationOffset)
{
    protectedBacking()->resolveQuerySet(querySet.backing(), firstQuery, queryCount, destination.backing(), destinationOffset);
}

static WebGPU::CommandBufferDescriptor convertToBacking(const std::optional<GPUCommandBufferDescriptor>& commandBufferDescriptor)
{
    if (!commandBufferDescriptor)
        return { };

    return commandBufferDescriptor->convertToBacking();
}

ExceptionOr<Ref<GPUCommandBuffer>> GPUCommandEncoder::finish(const std::optional<GPUCommandBufferDescriptor>& commandBufferDescriptor)
{
    RefPtr buffer = protectedBacking()->finish(convertToBacking(commandBufferDescriptor));
    if (!buffer)
        return Exception { ExceptionCode::InvalidStateError, "GPUCommandEncoder.finish: Unable to finish."_s };
    return GPUCommandBuffer::create(buffer.releaseNonNull(), *this);
}

void GPUCommandEncoder::setBacking(WebGPU::CommandEncoder& newBacking)
{
    m_backing = newBacking;
}

}

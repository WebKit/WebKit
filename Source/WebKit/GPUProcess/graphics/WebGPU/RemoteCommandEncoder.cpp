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
#include "RemoteCommandEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoderMessages.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteRenderPassEncoder.h"
#include "StreamServerConnection.h"
#include "WebGPUComputePassDescriptor.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPUCommandEncoder.h>

namespace WebKit {

RemoteCommandEncoder::RemoteCommandEncoder(PAL::WebGPU::CommandEncoder& commandEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(commandEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteCommandEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteCommandEncoder::~RemoteCommandEncoder() = default;

void RemoteCommandEncoder::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteCommandEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteCommandEncoder::beginRenderPass(const WebGPU::RenderPassDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto renderPassEncoder = m_backing->beginRenderPass(*convertedDescriptor);
    auto remoteRenderPassEncoder = RemoteRenderPassEncoder::create(renderPassEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteRenderPassEncoder);
}

void RemoteCommandEncoder::beginComputePass(const std::optional<WebGPU::ComputePassDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    std::optional<PAL::WebGPU::ComputePassDescriptor> convertedDescriptor;
    if (descriptor) {
        auto resultDescriptor = m_objectHeap.convertFromBacking(*descriptor);
        ASSERT(resultDescriptor);
        convertedDescriptor = WTFMove(resultDescriptor);
        if (!convertedDescriptor)
            return;
    }

    auto computePassEncoder = m_backing->beginComputePass(*convertedDescriptor);
    auto computeRenderPassEncoder = RemoteComputePassEncoder::create(computePassEncoder, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, computeRenderPassEncoder);
}

void RemoteCommandEncoder::copyBufferToBuffer(
    WebGPUIdentifier source,
    PAL::WebGPU::Size64 sourceOffset,
    WebGPUIdentifier destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    auto convertedSource = m_objectHeap.convertBufferFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap.convertBufferFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    m_backing->copyBufferToBuffer(*convertedSource, sourceOffset, *convertedDestination, destinationOffset, size);
}

void RemoteCommandEncoder::copyBufferToTexture(
    const WebGPU::ImageCopyBuffer& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap.convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap.convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_objectHeap.convertFromBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    m_backing->copyBufferToTexture(*convertedSource, *convertedDestination, *convertedCopySize);
}

void RemoteCommandEncoder::copyTextureToBuffer(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyBuffer& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap.convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap.convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_objectHeap.convertFromBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    m_backing->copyTextureToBuffer(*convertedSource, *convertedDestination, *convertedCopySize);
}

void RemoteCommandEncoder::copyTextureToTexture(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap.convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap.convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_objectHeap.convertFromBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    m_backing->copyTextureToTexture(*convertedSource, *convertedDestination, *convertedCopySize);
}

void RemoteCommandEncoder::clearBuffer(
    WebGPUIdentifier buffer,
    PAL::WebGPU::Size64 offset,
    std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_objectHeap.convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    m_backing->clearBuffer(*convertedBuffer, offset, size);
}

void RemoteCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void RemoteCommandEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void RemoteCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

void RemoteCommandEncoder::writeTimestamp(WebGPUIdentifier querySet, PAL::WebGPU::Size32 queryIndex)
{
    auto convertedQuerySet = m_objectHeap.convertQuerySetFromBacking(querySet);
    ASSERT(convertedQuerySet);
    if (!convertedQuerySet)
        return;

    m_backing->writeTimestamp(*convertedQuerySet, queryIndex);
}

void RemoteCommandEncoder::resolveQuerySet(
    WebGPUIdentifier querySet,
    PAL::WebGPU::Size32 firstQuery,
    PAL::WebGPU::Size32 queryCount,
    WebGPUIdentifier destination,
    PAL::WebGPU::Size64 destinationOffset)
{
    auto convertedQuerySet = m_objectHeap.convertQuerySetFromBacking(querySet);
    ASSERT(convertedQuerySet);
    auto convertedDestination = m_objectHeap.convertBufferFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedQuerySet || !convertedDestination)
        return;

    m_backing->resolveQuerySet(*convertedQuerySet, firstQuery, queryCount, *convertedDestination, destinationOffset);
}

void RemoteCommandEncoder::finish(const WebGPU::CommandBufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto commandBuffer = m_backing->finish(*convertedDescriptor);
    auto remoteCommandBuffer = RemoteCommandBuffer::create(commandBuffer, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteCommandBuffer);
}

void RemoteCommandEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

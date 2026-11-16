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
#include "RemoteCommandEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoderMessages.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteRenderPassEncoder.h"
#include "StreamServerConnection.h"
#include "WebGPUComputePassDescriptor.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_streamConnection)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCommandEncoder);

RemoteCommandEncoder::RemoteCommandEncoder(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, ::WebGPU::CommandEncoder& commandEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(commandEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteCommandEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteCommandEncoder::~RemoteCommandEncoder() = default;

void RemoteCommandEncoder::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteCommandEncoder::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteCommandEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteCommandEncoder::beginRenderPass(const WebGPU::RenderPassDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    Vector<std::optional<::WebGPU::RenderPassColorAttachment>> colorAttachments;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor, colorAttachments);
    MESSAGE_CHECK(convertedDescriptor);

    auto renderPassEncoder = protect(m_backing)->beginRenderPass(*convertedDescriptor);
    MESSAGE_CHECK(renderPassEncoder);
    auto remoteRenderPassEncoder = RemoteRenderPassEncoder::create(*renderPassEncoder, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteRenderPassEncoder);
}

void RemoteCommandEncoder::beginComputePass(const std::optional<WebGPU::ComputePassDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    std::optional<::WebGPU::ComputePassDescriptor> convertedDescriptor;
    if (descriptor) {
        auto resultDescriptor = objectHeap->convertFromBacking(*descriptor);
        MESSAGE_CHECK(resultDescriptor);
        convertedDescriptor = WTF::move(resultDescriptor);
    }

    auto computePassEncoder = protect(m_backing)->beginComputePass(convertedDescriptor);
    MESSAGE_CHECK(computePassEncoder);
    auto computeRenderPassEncoder = RemoteComputePassEncoder::create(*computePassEncoder, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, computeRenderPassEncoder);
}

void RemoteCommandEncoder::copyBufferToBuffer(
    WebGPUIdentifier source,
    uint64_t sourceOffset,
    WebGPUIdentifier destination,
    uint64_t destinationOffset,
    uint64_t size)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedSource = objectHeap->convertBufferFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = objectHeap->convertBufferFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    protect(m_backing)->copyBufferToBuffer(protect(*convertedSource), sourceOffset, protect(*convertedDestination), destinationOffset, size);
}

void RemoteCommandEncoder::copyBufferToTexture(
    const WebGPU::ImageCopyBuffer& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap->convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    protect(m_backing)->copyBufferToTexture(*convertedSource, *convertedDestination, copySize);
}

void RemoteCommandEncoder::copyTextureToBuffer(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyBuffer& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap->convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    protect(m_backing)->copyTextureToBuffer(*convertedSource, *convertedDestination, copySize);
}

void RemoteCommandEncoder::copyTextureToTexture(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_objectHeap->convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedSource || !convertedDestination)
        return;

    protect(m_backing)->copyTextureToTexture(*convertedSource, *convertedDestination, copySize);
}

void RemoteCommandEncoder::clearBuffer(
    WebGPUIdentifier buffer,
    uint64_t offset,
    std::optional<uint64_t> size)
{
    auto convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->clearBuffer(protect(*convertedBuffer), offset, size);
}

void RemoteCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    protect(m_backing)->pushDebugGroup(WTF::move(groupLabel));
}

void RemoteCommandEncoder::popDebugGroup()
{
    protect(m_backing)->popDebugGroup();
}

void RemoteCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    protect(m_backing)->insertDebugMarker(WTF::move(markerLabel));
}

void RemoteCommandEncoder::writeTimestamp(WebGPUIdentifier querySet, uint32_t queryIndex)
{
    auto convertedQuerySet = protect(m_objectHeap)->convertQuerySetFromBacking(querySet);
    ASSERT(convertedQuerySet);
    if (!convertedQuerySet)
        return;

    protect(m_backing)->writeTimestamp(protect(*convertedQuerySet), queryIndex);
}

void RemoteCommandEncoder::resolveQuerySet(
    WebGPUIdentifier querySet,
    uint32_t firstQuery,
    uint32_t queryCount,
    WebGPUIdentifier destination,
    uint64_t destinationOffset)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedQuerySet = objectHeap->convertQuerySetFromBacking(querySet);
    ASSERT(convertedQuerySet);
    auto convertedDestination = objectHeap->convertBufferFromBacking(destination);
    ASSERT(convertedDestination);
    if (!convertedQuerySet || !convertedDestination)
        return;

    protect(m_backing)->resolveQuerySet(protect(*convertedQuerySet), firstQuery, queryCount, protect(*convertedDestination), destinationOffset);
}

void RemoteCommandEncoder::finish(const WebGPU::CommandBufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto commandBuffer = protect(m_backing)->finish(descriptor);
    MESSAGE_CHECK(commandBuffer);
    auto remoteCommandBuffer = RemoteCommandBuffer::create(*commandBuffer, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteCommandBuffer);
}

void RemoteCommandEncoder::setLabel(String&& label)
{
    protect(m_backing)->setLabel(WTF::move(label));
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)

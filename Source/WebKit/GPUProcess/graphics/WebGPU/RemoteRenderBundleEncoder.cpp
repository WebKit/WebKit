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
#include "RemoteRenderBundleEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderBundle.h"
#include "RemoteRenderBundleEncoderMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPURenderBundle.h>
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoder.h>

namespace WebKit {

RemoteRenderBundleEncoder::RemoteRenderBundleEncoder(PAL::WebGPU::RenderBundleEncoder& renderBundleEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(renderBundleEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderBundleEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteRenderBundleEncoder::~RemoteRenderBundleEncoder() = default;

void RemoteRenderBundleEncoder::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteRenderBundleEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteRenderBundleEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    auto convertedRenderPipeline = m_objectHeap.convertRenderPipelineFromBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    m_backing->setPipeline(*convertedRenderPipeline);
}

void RemoteRenderBundleEncoder::setIndexBuffer(WebGPUIdentifier buffer, PAL::WebGPU::IndexFormat indexFormat, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_objectHeap.convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    m_backing->setIndexBuffer(*convertedBuffer, indexFormat, offset, size);
}

void RemoteRenderBundleEncoder::setVertexBuffer(PAL::WebGPU::Index32 slot, WebGPUIdentifier buffer, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_objectHeap.convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    m_backing->setVertexBuffer(slot, *convertedBuffer, offset, size);
}

void RemoteRenderBundleEncoder::draw(PAL::WebGPU::Size32 vertexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstVertex, std::optional<PAL::WebGPU::Size32> firstInstance)
{
    m_backing->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RemoteRenderBundleEncoder::drawIndexed(PAL::WebGPU::Size32 indexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstIndex,
    std::optional<PAL::WebGPU::SignedOffset32> baseVertex,
    std::optional<PAL::WebGPU::Size32> firstInstance)
{
    m_backing->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RemoteRenderBundleEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_objectHeap.convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    m_backing->drawIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderBundleEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_objectHeap.convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    m_backing->drawIndexedIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderBundleEncoder::setBindGroup(PAL::WebGPU::Index32 index, WebGPUIdentifier bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    auto convertedBindGroup = m_objectHeap.convertBindGroupFromBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    m_backing->setBindGroup(index, *convertedBindGroup, WTFMove(dynamicOffsets));
}

void RemoteRenderBundleEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void RemoteRenderBundleEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void RemoteRenderBundleEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

void RemoteRenderBundleEncoder::finish(const WebGPU::RenderBundleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    auto convertedDescriptor = m_objectHeap.convertFromBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto renderBundle = m_backing->finish(*convertedDescriptor);
    auto remoteRenderBundle = RemoteRenderBundle::create(renderBundle, m_objectHeap, m_streamConnection.copyRef(), identifier);
    m_objectHeap.addObject(identifier, remoteRenderBundle);
}

void RemoteRenderBundleEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

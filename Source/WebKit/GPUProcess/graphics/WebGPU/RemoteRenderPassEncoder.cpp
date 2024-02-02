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
#include "RemoteRenderPassEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderPassEncoderMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPURenderPassEncoder.h>

namespace WebKit {

RemoteRenderPassEncoder::RemoteRenderPassEncoder(WebCore::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(renderPassEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteRenderPassEncoder::~RemoteRenderPassEncoder() = default;

void RemoteRenderPassEncoder::destruct()
{
    m_objectHeap.removeObject(m_identifier);
}

void RemoteRenderPassEncoder::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteRenderPassEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    auto convertedRenderPipeline = m_objectHeap.convertRenderPipelineFromBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    m_backing->setPipeline(*convertedRenderPipeline);
}

void RemoteRenderPassEncoder::setIndexBuffer(WebGPUIdentifier buffer, WebCore::WebGPU::IndexFormat indexFormat, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64> size)
{
    auto convertedBuffer = m_objectHeap.convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    m_backing->setIndexBuffer(*convertedBuffer, indexFormat, offset, size);
}

void RemoteRenderPassEncoder::setVertexBuffer(WebCore::WebGPU::Index32 slot, WebGPUIdentifier buffer, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64> size)
{
    auto convertedBuffer = m_objectHeap.convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    m_backing->setVertexBuffer(slot, convertedBuffer, offset, size);
}

void RemoteRenderPassEncoder::unsetVertexBuffer(WebCore::WebGPU::Index32 slot, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64> size)
{
    m_backing->setVertexBuffer(slot, nullptr, offset, size);
}

void RemoteRenderPassEncoder::draw(WebCore::WebGPU::Size32 vertexCount, std::optional<WebCore::WebGPU::Size32> instanceCount,
    std::optional<WebCore::WebGPU::Size32> firstVertex, std::optional<WebCore::WebGPU::Size32> firstInstance)
{
    m_backing->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndexed(WebCore::WebGPU::Size32 indexCount,
    std::optional<WebCore::WebGPU::Size32> instanceCount,
    std::optional<WebCore::WebGPU::Size32> firstIndex,
    std::optional<WebCore::WebGPU::SignedOffset32> baseVertex,
    std::optional<WebCore::WebGPU::Size32> firstInstance)
{
    m_backing->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_objectHeap.convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    m_backing->drawIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderPassEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_objectHeap.convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    m_backing->drawIndexedIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderPassEncoder::setBindGroup(WebCore::WebGPU::Index32 index, WebGPUIdentifier bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    auto convertedBindGroup = m_objectHeap.convertBindGroupFromBacking(bindGroup);
    if (!convertedBindGroup)
        return;

    m_backing->setBindGroup(index, *convertedBindGroup, WTFMove(dynamicOffsets));
}

void RemoteRenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void RemoteRenderPassEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void RemoteRenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

void RemoteRenderPassEncoder::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    m_backing->setViewport(x, y, width, height, minDepth, maxDepth);
}

void RemoteRenderPassEncoder::setScissorRect(WebCore::WebGPU::IntegerCoordinate x, WebCore::WebGPU::IntegerCoordinate y,
    WebCore::WebGPU::IntegerCoordinate width, WebCore::WebGPU::IntegerCoordinate height)
{
    m_backing->setScissorRect(x, y, width, height);
}

void RemoteRenderPassEncoder::setBlendConstant(WebGPU::Color color)
{
    auto convertedColor = m_objectHeap.convertFromBacking(color);
    ASSERT(convertedColor);
    if (!convertedColor)
        return;

    m_backing->setBlendConstant(*convertedColor);
}

void RemoteRenderPassEncoder::setStencilReference(WebCore::WebGPU::StencilValue stencilValue)
{
    m_backing->setStencilReference(stencilValue);
}

void RemoteRenderPassEncoder::beginOcclusionQuery(WebCore::WebGPU::Size32 queryIndex)
{
    m_backing->beginOcclusionQuery(queryIndex);
}

void RemoteRenderPassEncoder::endOcclusionQuery()
{
    m_backing->endOcclusionQuery();
}

void RemoteRenderPassEncoder::executeBundles(Vector<WebGPUIdentifier>&& renderBundles)
{
    Vector<std::reference_wrapper<WebCore::WebGPU::RenderBundle>> convertedBundles;
    convertedBundles.reserveInitialCapacity(renderBundles.size());
    for (WebGPUIdentifier identifier : renderBundles) {
        auto convertedBundle = m_objectHeap.convertRenderBundleFromBacking(identifier);
        ASSERT(convertedBundle);
        if (!convertedBundle)
            return;
        convertedBundles.append(*convertedBundle);
    }
    m_backing->executeBundles(WTFMove(convertedBundles));
}

void RemoteRenderPassEncoder::end()
{
    m_backing->end();
}

void RemoteRenderPassEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

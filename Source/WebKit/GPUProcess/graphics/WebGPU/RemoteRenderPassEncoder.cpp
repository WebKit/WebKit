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
#include <WebCore/WebGPUBindGroup.h>
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPURenderBundle.h>
#include <WebCore/WebGPURenderPassEncoder.h>
#include <WebCore/WebGPURenderPipeline.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderPassEncoder);

RemoteRenderPassEncoder::RemoteRenderPassEncoder(WebCore::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(renderPassEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    Ref { m_streamConnection }->startReceivingMessages(*this, Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteRenderPassEncoder::~RemoteRenderPassEncoder() = default;

void RemoteRenderPassEncoder::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteRenderPassEncoder::stopListeningForIPC()
{
    Ref { m_streamConnection }->stopReceivingMessages(Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteRenderPassEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    auto convertedRenderPipeline = protect(m_objectHeap)->convertRenderPipelineFromBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    protect(m_backing)->setPipeline(*convertedRenderPipeline);
}

void RemoteRenderPassEncoder::setIndexBuffer(WebGPUIdentifier buffer, WebCore::WebGPU::IndexFormat indexFormat, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size)
{
    auto convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setIndexBuffer(*convertedBuffer, indexFormat, offset, size);
}

void RemoteRenderPassEncoder::setVertexBuffer(WebCore::WebGPU::Index32 slot, WebGPUIdentifier buffer, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size)
{
    RefPtr convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer).get();
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setVertexBuffer(slot, convertedBuffer.get(), offset, size);
}

void RemoteRenderPassEncoder::unsetVertexBuffer(WebCore::WebGPU::Index32 slot, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size)
{
    protect(m_backing)->setVertexBuffer(slot, nullptr, offset, size);
}

void RemoteRenderPassEncoder::draw(WebCore::WebGPU::Size32 vertexCount, WebCore::WebGPU::Size32 instanceCount,
    WebCore::WebGPU::Size32 firstVertex, WebCore::WebGPU::Size32 firstInstance)
{
    protect(m_backing)->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndexed(WebCore::WebGPU::Size32 indexCount,
    WebCore::WebGPU::Size32 instanceCount,
    WebCore::WebGPU::Size32 firstIndex,
    WebCore::WebGPU::SignedOffset32 baseVertex,
    WebCore::WebGPU::Size32 firstInstance)
{
    protect(m_backing)->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderPassEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndexedIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteRenderPassEncoder::setBindGroup(WebCore::WebGPU::Index32 index, std::optional<WebGPUIdentifier> bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    if (!bindGroup) {
        protect(m_backing)->setBindGroup(index, nullptr, WTF::move(dynamicOffsets));
        return;
    }

    RefPtr convertedBindGroup = protect(m_objectHeap)->convertBindGroupFromBacking(*bindGroup).get();
    if (!convertedBindGroup)
        return;

    protect(m_backing)->setBindGroup(index, convertedBindGroup.get(), WTF::move(dynamicOffsets));
}

void RemoteRenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    protect(m_backing)->pushDebugGroup(WTF::move(groupLabel));
}

void RemoteRenderPassEncoder::popDebugGroup()
{
    protect(m_backing)->popDebugGroup();
}

void RemoteRenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    protect(m_backing)->insertDebugMarker(WTF::move(markerLabel));
}

void RemoteRenderPassEncoder::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    protect(m_backing)->setViewport(x, y, width, height, minDepth, maxDepth);
}

void RemoteRenderPassEncoder::setScissorRect(WebCore::WebGPU::IntegerCoordinate x, WebCore::WebGPU::IntegerCoordinate y,
    WebCore::WebGPU::IntegerCoordinate width, WebCore::WebGPU::IntegerCoordinate height)
{
    protect(m_backing)->setScissorRect(x, y, width, height);
}

void RemoteRenderPassEncoder::setBlendConstant(WebGPU::Color color)
{
    auto convertedColor = protect(m_objectHeap)->convertFromBacking(color);
    ASSERT(convertedColor);
    if (!convertedColor)
        return;

    protect(m_backing)->setBlendConstant(*convertedColor);
}

void RemoteRenderPassEncoder::setStencilReference(WebCore::WebGPU::StencilValue stencilValue)
{
    protect(m_backing)->setStencilReference(stencilValue);
}

void RemoteRenderPassEncoder::beginOcclusionQuery(WebCore::WebGPU::Size32 queryIndex)
{
    protect(m_backing)->beginOcclusionQuery(queryIndex);
}

void RemoteRenderPassEncoder::endOcclusionQuery()
{
    protect(m_backing)->endOcclusionQuery();
}

void RemoteRenderPassEncoder::executeBundles(Vector<WebGPUIdentifier>&& renderBundles)
{
    Vector<Ref<WebCore::WebGPU::RenderBundle>> convertedBundles;
    convertedBundles.reserveInitialCapacity(renderBundles.size());
    for (WebGPUIdentifier identifier : renderBundles) {
        auto convertedBundle = protect(m_objectHeap)->convertRenderBundleFromBacking(identifier);
        ASSERT(convertedBundle);
        if (!convertedBundle)
            return;
        convertedBundles.append(*convertedBundle);
    }
    protect(m_backing)->executeBundles(WTF::move(convertedBundles));
}

void RemoteRenderPassEncoder::end()
{
    protect(m_backing)->end();
}

void RemoteRenderPassEncoder::setLabel(String&& label)
{
    protect(m_backing)->setLabel(WTF::move(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

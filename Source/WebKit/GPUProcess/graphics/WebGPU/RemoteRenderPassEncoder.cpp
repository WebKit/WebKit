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
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderPassEncoder);

RemoteRenderPassEncoder::RemoteRenderPassEncoder(::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(renderPassEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteRenderPassEncoder::~RemoteRenderPassEncoder() = default;

void RemoteRenderPassEncoder::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteRenderPassEncoder::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteRenderPassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteRenderPassEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    auto convertedRenderPipeline = protect(m_objectHeap)->convertRenderPipelineFromBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    protect(m_backing)->setPipeline(protect(*convertedRenderPipeline));
}

void RemoteRenderPassEncoder::setIndexBuffer(WebGPUIdentifier buffer, ::WebGPU::IndexFormat indexFormat, uint64_t offset, std::optional<uint64_t> size)
{
    auto convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setIndexBuffer(protect(*convertedBuffer), indexFormat, offset, size);
}

void RemoteRenderPassEncoder::setVertexBuffer(uint32_t slot, WebGPUIdentifier buffer, uint64_t offset, std::optional<uint64_t> size)
{
    RefPtr convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setVertexBuffer(slot, convertedBuffer.get(), offset, size);
}

void RemoteRenderPassEncoder::unsetVertexBuffer(uint32_t slot, uint64_t offset, std::optional<uint64_t> size)
{
    protect(m_backing)->setVertexBuffer(slot, nullptr, offset, size);
}

void RemoteRenderPassEncoder::draw(uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance)
{
    protect(m_backing)->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndexed(uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t baseVertex,
    uint32_t firstInstance)
{
    protect(m_backing)->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RemoteRenderPassEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndirect(protect(*convertedIndirectBuffer), indirectOffset);
}

void RemoteRenderPassEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndexedIndirect(protect(*convertedIndirectBuffer), indirectOffset);
}

void RemoteRenderPassEncoder::setBindGroup(uint32_t index, std::optional<WebGPUIdentifier> bindGroup,
    std::optional<Vector<uint32_t>>&& dynamicOffsets)
{
    if (!bindGroup) {
        protect(m_backing)->setBindGroup(index, nullptr, dynamicOffsets ? std::optional { dynamicOffsets->span() } : std::nullopt);
        return;
    }

    RefPtr convertedBindGroup = protect(m_objectHeap)->convertBindGroupFromBacking(*bindGroup);
    if (!convertedBindGroup)
        return;

    protect(m_backing)->setBindGroup(index, convertedBindGroup.get(), dynamicOffsets ? std::optional { dynamicOffsets->span() } : std::nullopt);
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

void RemoteRenderPassEncoder::setScissorRect(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    protect(m_backing)->setScissorRect(x, y, width, height);
}

void RemoteRenderPassEncoder::setBlendConstant(WebGPU::Color color)
{
    protect(m_backing)->setBlendConstant(color);
}

void RemoteRenderPassEncoder::setStencilReference(uint32_t stencilValue)
{
    protect(m_backing)->setStencilReference(stencilValue);
}

void RemoteRenderPassEncoder::beginOcclusionQuery(uint32_t queryIndex)
{
    protect(m_backing)->beginOcclusionQuery(queryIndex);
}

void RemoteRenderPassEncoder::endOcclusionQuery()
{
    protect(m_backing)->endOcclusionQuery();
}

void RemoteRenderPassEncoder::executeBundles(Vector<WebGPUIdentifier>&& renderBundles)
{
    Vector<Ref<::WebGPU::RenderBundle>> convertedBundles;
    convertedBundles.reserveInitialCapacity(renderBundles.size());
    for (WebGPUIdentifier identifier : renderBundles) {
        auto convertedBundle = protect(m_objectHeap)->convertRenderBundleFromBacking(identifier);
        ASSERT(convertedBundle);
        if (!convertedBundle)
            return;
        convertedBundles.append(protect(*convertedBundle));
    }
    protect(m_backing)->executeBundles(convertedBundles.span());
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

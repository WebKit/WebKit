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
#include "RemoteRenderBundleEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteRenderBundle.h"
#include "RemoteRenderBundleEncoderMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_streamConnection)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderBundleEncoder);

RemoteRenderBundleEncoder::RemoteRenderBundleEncoder(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, ::WebGPU::RenderBundleEncoder& renderBundleEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(renderBundleEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteRenderBundleEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteRenderBundleEncoder::~RemoteRenderBundleEncoder() = default;

void RemoteRenderBundleEncoder::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteRenderBundleEncoder::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteRenderBundleEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteRenderBundleEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    auto convertedRenderPipeline = protect(m_objectHeap)->convertRenderPipelineFromBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    protect(m_backing)->setPipeline(protect(*convertedRenderPipeline));
}

void RemoteRenderBundleEncoder::setIndexBuffer(WebGPUIdentifier buffer, ::WebGPU::IndexFormat indexFormat, uint64_t offset, std::optional<uint64_t> size)
{
    RefPtr convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setIndexBuffer(protect(*convertedBuffer), indexFormat, offset, size);
}

void RemoteRenderBundleEncoder::setVertexBuffer(uint32_t slot, WebGPUIdentifier buffer, uint64_t offset, std::optional<uint64_t> size)
{
    RefPtr convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->setVertexBuffer(slot, convertedBuffer.get(), offset, size);
}

void RemoteRenderBundleEncoder::unsetVertexBuffer(uint32_t slot, uint64_t offset, std::optional<uint64_t> size)
{
    protect(m_backing)->setVertexBuffer(slot, nullptr, offset, size);
}

void RemoteRenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance)
{
    protect(m_backing)->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RemoteRenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t baseVertex,
    uint32_t firstInstance)
{
    protect(m_backing)->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RemoteRenderBundleEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset)
{
    RefPtr convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndirect(protect(*convertedIndirectBuffer), indirectOffset);
}

void RemoteRenderBundleEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset)
{
    RefPtr convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->drawIndexedIndirect(protect(*convertedIndirectBuffer), indirectOffset);
}

void RemoteRenderBundleEncoder::setBindGroup(uint32_t index, std::optional<WebGPUIdentifier> bindGroup,
    std::optional<Vector<uint32_t>>&& dynamicOffsets)
{
    if (!bindGroup) {
        protect(m_backing)->setBindGroup(index, nullptr, dynamicOffsets ? std::optional { dynamicOffsets->span() } : std::nullopt);
        return;
    }

    RefPtr convertedBindGroup = protect(m_objectHeap)->convertBindGroupFromBacking(*bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    protect(m_backing)->setBindGroup(index, convertedBindGroup.get(), dynamicOffsets ? std::optional { dynamicOffsets->span() } : std::nullopt);
}

void RemoteRenderBundleEncoder::pushDebugGroup(String&& groupLabel)
{
    protect(m_backing)->pushDebugGroup(WTF::move(groupLabel));
}

void RemoteRenderBundleEncoder::popDebugGroup()
{
    protect(m_backing)->popDebugGroup();
}

void RemoteRenderBundleEncoder::insertDebugMarker(String&& markerLabel)
{
    protect(m_backing)->insertDebugMarker(WTF::move(markerLabel));
}

void RemoteRenderBundleEncoder::finish(const WebGPU::RenderBundleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    Ref objectHeap = m_objectHeap.get();
    auto renderBundle = protect(m_backing)->finish(descriptor);
    MESSAGE_CHECK(renderBundle);
    auto remoteRenderBundle = RemoteRenderBundle::create(*renderBundle, objectHeap, protect(m_streamConnection), protect(m_gpu), identifier);
    objectHeap->addObject(identifier, remoteRenderBundle);
}

void RemoteRenderBundleEncoder::setLabel(String&& label)
{
    protect(m_backing)->setLabel(WTF::move(label));
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)

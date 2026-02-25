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
#include "RemoteComputePassEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteComputePassEncoderMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUBindGroup.h>
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPUComputePassEncoder.h>
#include <WebCore/WebGPUComputePipeline.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteComputePassEncoder);

RemoteComputePassEncoder::RemoteComputePassEncoder(WebCore::WebGPU::ComputePassEncoder& computePassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(computePassEncoder)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteComputePassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteComputePassEncoder::~RemoteComputePassEncoder() = default;

void RemoteComputePassEncoder::destruct()
{
    protectedObjectHeap()->removeObject(m_identifier);
}

void RemoteComputePassEncoder::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemoteComputePassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteComputePassEncoder::setPipeline(WebGPUIdentifier computePipeline)
{
    auto convertedComputePipeline = protectedObjectHeap()->convertComputePipelineFromBacking(computePipeline);
    ASSERT(convertedComputePipeline);
    if (!convertedComputePipeline)
        return;

    protectedBacking()->setPipeline(*convertedComputePipeline);
}

void RemoteComputePassEncoder::dispatch(WebCore::WebGPU::Size32 workgroupCountX, WebCore::WebGPU::Size32 workgroupCountY, WebCore::WebGPU::Size32 workgroupCountZ)
{
    protectedBacking()->dispatch(workgroupCountX, workgroupCountY, workgroupCountZ);
}

void RemoteComputePassEncoder::dispatchIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protectedObjectHeap()->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protectedBacking()->dispatchIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteComputePassEncoder::end()
{
    protectedBacking()->end();
}

void RemoteComputePassEncoder::setBindGroup(WebCore::WebGPU::Index32 index, std::optional<WebGPUIdentifier> bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& offsets)
{
    if (!bindGroup) {
        protectedBacking()->setBindGroup(index, nullptr, WTF::move(offsets));
        return;
    }

    RefPtr convertedBindGroup = protectedObjectHeap()->convertBindGroupFromBacking(*bindGroup).get();
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    protectedBacking()->setBindGroup(index, convertedBindGroup.get(), WTF::move(offsets));
}

void RemoteComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    protectedBacking()->pushDebugGroup(WTF::move(groupLabel));
}

void RemoteComputePassEncoder::popDebugGroup()
{
    protectedBacking()->popDebugGroup();
}

void RemoteComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    protectedBacking()->insertDebugMarker(WTF::move(markerLabel));
}

void RemoteComputePassEncoder::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTF::move(label));
}

Ref<WebCore::WebGPU::ComputePassEncoder> RemoteComputePassEncoder::protectedBacking()
{
    return m_backing;
}

Ref<IPC::StreamServerConnection> RemoteComputePassEncoder::protectedStreamConnection() const
{
    return m_streamConnection;
}

Ref<WebGPU::ObjectHeap> RemoteComputePassEncoder::protectedObjectHeap() const
{
    return m_objectHeap;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

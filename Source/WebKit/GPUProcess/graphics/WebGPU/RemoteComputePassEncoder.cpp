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
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteComputePassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

RemoteComputePassEncoder::~RemoteComputePassEncoder() = default;

void RemoteComputePassEncoder::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteComputePassEncoder::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteComputePassEncoder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteComputePassEncoder::setPipeline(WebGPUIdentifier computePipeline)
{
    auto convertedComputePipeline = protect(m_objectHeap)->convertComputePipelineFromBacking(computePipeline);
    ASSERT(convertedComputePipeline);
    if (!convertedComputePipeline)
        return;

    protect(m_backing)->setPipeline(*convertedComputePipeline);
}

void RemoteComputePassEncoder::dispatch(WebCore::WebGPU::Size32 workgroupCountX, WebCore::WebGPU::Size32 workgroupCountY, WebCore::WebGPU::Size32 workgroupCountZ)
{
    protect(m_backing)->dispatch(workgroupCountX, workgroupCountY, workgroupCountZ);
}

void RemoteComputePassEncoder::dispatchIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protect(m_objectHeap)->convertBufferFromBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    protect(m_backing)->dispatchIndirect(*convertedIndirectBuffer, indirectOffset);
}

void RemoteComputePassEncoder::end()
{
    protect(m_backing)->end();
}

void RemoteComputePassEncoder::setBindGroup(WebCore::WebGPU::Index32 index, std::optional<WebGPUIdentifier> bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& offsets)
{
    if (!bindGroup) {
        protect(m_backing)->setBindGroup(index, nullptr, WTF::move(offsets));
        return;
    }

    RefPtr convertedBindGroup = protect(m_objectHeap)->convertBindGroupFromBacking(*bindGroup).get();
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    protect(m_backing)->setBindGroup(index, convertedBindGroup.get(), WTF::move(offsets));
}

void RemoteComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    protect(m_backing)->pushDebugGroup(WTF::move(groupLabel));
}

void RemoteComputePassEncoder::popDebugGroup()
{
    protect(m_backing)->popDebugGroup();
}

void RemoteComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    protect(m_backing)->insertDebugMarker(WTF::move(markerLabel));
}

void RemoteComputePassEncoder::setLabel(String&& label)
{
    protect(m_backing)->setLabel(WTF::move(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "RemoteComputePassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteComputePassEncoderMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteComputePassEncoderProxy);

RemoteComputePassEncoderProxy::RemoteComputePassEncoderProxy(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(parent.root())
{
}

RemoteComputePassEncoderProxy::~RemoteComputePassEncoderProxy()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setPipeline(const ::WebGPU::ComputePipeline& computePipeline)
{
    auto convertedComputePipeline = m_convertToBackingContext->convertToBacking(computePipeline);

    auto sendResult = send(Messages::RemoteComputePassEncoder::SetPipeline(convertedComputePipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::dispatch(uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::Dispatch(workgroupCountX, workgroupCountY, workgroupCountZ));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::dispatchIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteComputePassEncoder::DispatchIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::end()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::End());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setBindGroup(uint32_t index, const ::WebGPU::BindGroup* bindGroup, std::optional<std::span<const uint32_t>> dynamicOffsets)
{
    std::optional<WebGPUIdentifier> convertedBindGroup;
    if (bindGroup)
        convertedBindGroup = m_convertToBackingContext->convertToBacking(*bindGroup);

    auto sendResult = send(Messages::RemoteComputePassEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets ? std::optional { Vector<uint32_t>(*dynamicOffsets) } : std::nullopt));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::PushDebugGroup(WTF::move(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::InsertDebugMarker(WTF::move(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteComputePassEncoderProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

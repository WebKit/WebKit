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
#include "RemoteComputePassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteComputePassEncoderMessages.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteComputePassEncoderProxy::RemoteComputePassEncoderProxy(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteComputePassEncoderProxy::~RemoteComputePassEncoderProxy()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setPipeline(const WebCore::WebGPU::ComputePipeline& computePipeline)
{
    auto convertedComputePipeline = m_convertToBackingContext->convertToBacking(computePipeline);
    ASSERT(convertedComputePipeline);
    if (!convertedComputePipeline)
        return;

    auto sendResult = send(Messages::RemoteComputePassEncoder::SetPipeline(convertedComputePipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::dispatch(WebCore::WebGPU::Size32 workgroupCountX, WebCore::WebGPU::Size32 workgroupCountY, WebCore::WebGPU::Size32 workgroupCountZ)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::Dispatch(workgroupCountX, workgroupCountY, workgroupCountZ));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::dispatchIndirect(const WebCore::WebGPU::Buffer& indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    auto sendResult = send(Messages::RemoteComputePassEncoder::DispatchIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::end()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::End());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setBindGroup(WebCore::WebGPU::Index32 index, const WebCore::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& offsets)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteComputePassEncoder::SetBindGroup(index, convertedBindGroup, WTFMove(offsets)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setBindGroup(WebCore::WebGPU::Index32 index, const WebCore::WebGPU::BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    WebCore::WebGPU::Size64 dynamicOffsetsDataStart,
    WebCore::WebGPU::Size32 dynamicOffsetsDataLength)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteComputePassEncoder::SetBindGroup(index, convertedBindGroup, Vector<WebCore::WebGPU::BufferDynamicOffset>(dynamicOffsetsArrayBuffer + dynamicOffsetsDataStart, dynamicOffsetsDataLength)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::PushDebugGroup(WTFMove(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::InsertDebugMarker(WTFMove(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteComputePassEncoderProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteComputePassEncoder::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

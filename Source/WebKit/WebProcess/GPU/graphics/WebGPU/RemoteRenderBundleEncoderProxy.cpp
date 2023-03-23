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
#include "RemoteRenderBundleEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderBundleEncoderMessages.h"
#include "RemoteRenderBundleProxy.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteRenderBundleEncoderProxy::RemoteRenderBundleEncoderProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteRenderBundleEncoderProxy::~RemoteRenderBundleEncoderProxy()
{
}

void RemoteRenderBundleEncoderProxy::setPipeline(const PAL::WebGPU::RenderPipeline& renderPipeline)
{
    auto convertedRenderPipeline = m_convertToBackingContext->convertToBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetPipeline(convertedRenderPipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setIndexBuffer(const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::IndexFormat indexFormat, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetIndexBuffer(convertedBuffer, indexFormat, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setVertexBuffer(PAL::WebGPU::Index32 slot, const PAL::WebGPU::Buffer& buffer, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetVertexBuffer(slot, convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::draw(PAL::WebGPU::Size32 vertexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstVertex, std::optional<PAL::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::Draw(vertexCount, instanceCount, firstVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndexed(PAL::WebGPU::Size32 indexCount,
    std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstIndex,
    std::optional<PAL::WebGPU::SignedOffset32> baseVertex,
    std::optional<PAL::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndexedIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndexedIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    PAL::WebGPU::Size64 dynamicOffsetsDataStart,
    PAL::WebGPU::Size32 dynamicOffsetsDataLength)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetBindGroup(index, convertedBindGroup, Vector<PAL::WebGPU::BufferDynamicOffset>(dynamicOffsetsArrayBuffer + dynamicOffsetsDataStart, dynamicOffsetsDataLength)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::PushDebugGroup(WTFMove(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::InsertDebugMarker(WTFMove(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

Ref<PAL::WebGPU::RenderBundle> RemoteRenderBundleEncoderProxy::finish(const PAL::WebGPU::RenderBundleDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderBundleProxy::create(m_parent, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::Finish(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderBundleProxy::create(m_parent, m_convertToBackingContext, identifier);
}

void RemoteRenderBundleEncoderProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

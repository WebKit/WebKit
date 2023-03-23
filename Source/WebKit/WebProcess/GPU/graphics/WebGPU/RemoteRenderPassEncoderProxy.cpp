/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "RemoteRenderPassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderPassEncoderMessages.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteRenderPassEncoderProxy::RemoteRenderPassEncoderProxy(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteRenderPassEncoderProxy::~RemoteRenderPassEncoderProxy()
{
}

void RemoteRenderPassEncoderProxy::setPipeline(const PAL::WebGPU::RenderPipeline& renderPipeline)
{
    auto convertedRenderPipeline = m_convertToBackingContext->convertToBacking(renderPipeline);
    ASSERT(convertedRenderPipeline);
    if (!convertedRenderPipeline)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetPipeline(convertedRenderPipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setIndexBuffer(const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::IndexFormat indexFormat, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetIndexBuffer(convertedBuffer, indexFormat, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setVertexBuffer(PAL::WebGPU::Index32 slot, const PAL::WebGPU::Buffer& buffer, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetVertexBuffer(slot, convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::draw(PAL::WebGPU::Size32 vertexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstVertex, std::optional<PAL::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::Draw(vertexCount, instanceCount, firstVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexed(PAL::WebGPU::Size32 indexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
    std::optional<PAL::WebGPU::Size32> firstIndex,
    std::optional<PAL::WebGPU::SignedOffset32> baseVertex,
    std::optional<PAL::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexedIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);
    ASSERT(convertedIndirectBuffer);
    if (!convertedIndirectBuffer)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexedIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    PAL::WebGPU::Size64 dynamicOffsetsDataStart,
    PAL::WebGPU::Size32 dynamicOffsetsDataLength)
{
    auto convertedBindGroup = m_convertToBackingContext->convertToBacking(bindGroup);
    ASSERT(convertedBindGroup);
    if (!convertedBindGroup)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBindGroup(index, convertedBindGroup, Vector<PAL::WebGPU::BufferDynamicOffset>(dynamicOffsetsArrayBuffer + dynamicOffsetsDataStart, dynamicOffsetsDataLength)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::PushDebugGroup(WTFMove(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::InsertDebugMarker(WTFMove(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetViewport(x, y, width, height, minDepth, maxDepth));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setScissorRect(PAL::WebGPU::IntegerCoordinate x, PAL::WebGPU::IntegerCoordinate y,
    PAL::WebGPU::IntegerCoordinate width, PAL::WebGPU::IntegerCoordinate height)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetScissorRect(x, y, width, height));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBlendConstant(PAL::WebGPU::Color color)
{
    auto convertedColor = m_convertToBackingContext->convertToBacking(color);
    ASSERT(convertedColor);
    if (!convertedColor)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBlendConstant(*convertedColor));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setStencilReference(PAL::WebGPU::StencilValue stencilValue)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetStencilReference(stencilValue));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::beginOcclusionQuery(PAL::WebGPU::Size32 queryIndex)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::BeginOcclusionQuery(queryIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::endOcclusionQuery()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::EndOcclusionQuery());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::executeBundles(Vector<std::reference_wrapper<PAL::WebGPU::RenderBundle>>&& renderBundles)
{
    Vector<WebGPUIdentifier> convertedRenderBundles;
    convertedRenderBundles.reserveInitialCapacity(renderBundles.size());
    for (auto renderBundle : renderBundles) {
        auto convertedRenderBundle = m_convertToBackingContext->convertToBacking(renderBundle);
        ASSERT(convertedRenderBundle);
        if (!convertedRenderBundle)
            return;
        convertedRenderBundles.uncheckedAppend(convertedRenderBundle);
    }

    auto sendResult = send(Messages::RemoteRenderPassEncoder::ExecuteBundles(WTFMove(convertedRenderBundles)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::end()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::End());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

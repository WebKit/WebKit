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
#include "RemoteRenderPassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderPassEncoderMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderPassEncoderProxy);

RemoteRenderPassEncoderProxy::RemoteRenderPassEncoderProxy(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(parent.root())
{
}

RemoteRenderPassEncoderProxy::~RemoteRenderPassEncoderProxy()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setPipeline(const WebCore::WebGPU::RenderPipeline& renderPipeline)
{
    auto convertedRenderPipeline = protectedConvertToBackingContext()->convertToBacking(renderPipeline);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetPipeline(convertedRenderPipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setIndexBuffer(const WebCore::WebGPU::Buffer& buffer, WebCore::WebGPU::IndexFormat indexFormat, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64> size)
{
    auto convertedBuffer = protectedConvertToBackingContext()->convertToBacking(buffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetIndexBuffer(convertedBuffer, indexFormat, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setVertexBuffer(WebCore::WebGPU::Index32 slot, const WebCore::WebGPU::Buffer* buffer, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64> size)
{
    if (!buffer) {
        auto sendResult = send(Messages::RemoteRenderPassEncoder::UnsetVertexBuffer(slot, offset, size));
        UNUSED_VARIABLE(sendResult);
        return;
    }

    auto convertedBuffer = protectedConvertToBackingContext()->convertToBacking(*buffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetVertexBuffer(slot, convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::draw(WebCore::WebGPU::Size32 vertexCount, std::optional<WebCore::WebGPU::Size32> instanceCount,
    std::optional<WebCore::WebGPU::Size32> firstVertex, std::optional<WebCore::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::Draw(vertexCount, instanceCount, firstVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexed(WebCore::WebGPU::Size32 indexCount, std::optional<WebCore::WebGPU::Size32> instanceCount,
    std::optional<WebCore::WebGPU::Size32> firstIndex,
    std::optional<WebCore::WebGPU::SignedOffset32> baseVertex,
    std::optional<WebCore::WebGPU::Size32> firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndirect(const WebCore::WebGPU::Buffer& indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protectedConvertToBackingContext()->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexedIndirect(const WebCore::WebGPU::Buffer& indirectBuffer, WebCore::WebGPU::Size64 indirectOffset)
{
    auto convertedIndirectBuffer = protectedConvertToBackingContext()->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexedIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBindGroup(WebCore::WebGPU::Index32 index, const WebCore::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    auto convertedBindGroup = protectedConvertToBackingContext()->convertToBacking(bindGroup);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBindGroup(WebCore::WebGPU::Index32 index, const WebCore::WebGPU::BindGroup& bindGroup,
    std::span<const uint32_t> dynamicOffsetsArrayBuffer,
    WebCore::WebGPU::Size64 dynamicOffsetsDataStart,
    WebCore::WebGPU::Size32 dynamicOffsetsDataLength)
{
    auto convertedBindGroup = protectedConvertToBackingContext()->convertToBacking(bindGroup);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBindGroup(index, convertedBindGroup, Vector<WebCore::WebGPU::BufferDynamicOffset>(dynamicOffsetsArrayBuffer.subspan(dynamicOffsetsDataStart, dynamicOffsetsDataLength))));
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

void RemoteRenderPassEncoderProxy::setScissorRect(WebCore::WebGPU::IntegerCoordinate x, WebCore::WebGPU::IntegerCoordinate y,
    WebCore::WebGPU::IntegerCoordinate width, WebCore::WebGPU::IntegerCoordinate height)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetScissorRect(x, y, width, height));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBlendConstant(WebCore::WebGPU::Color color)
{
    auto convertedColor = protectedConvertToBackingContext()->convertToBacking(color);
    ASSERT(convertedColor);
    if (!convertedColor)
        return;

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBlendConstant(*convertedColor));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setStencilReference(WebCore::WebGPU::StencilValue stencilValue)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetStencilReference(stencilValue));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::beginOcclusionQuery(WebCore::WebGPU::Size32 queryIndex)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::BeginOcclusionQuery(queryIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::endOcclusionQuery()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::EndOcclusionQuery());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::executeBundles(Vector<Ref<WebCore::WebGPU::RenderBundle>>&& renderBundles)
{
    auto convertedRenderBundles = WTF::compactMap(renderBundles, [&](auto& renderBundle) -> std::optional<WebGPUIdentifier> {
        return protectedConvertToBackingContext()->convertToBacking(renderBundle);
    });

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

Ref<ConvertToBackingContext> RemoteRenderPassEncoderProxy::protectedConvertToBackingContext() const
{
    return m_convertToBackingContext;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

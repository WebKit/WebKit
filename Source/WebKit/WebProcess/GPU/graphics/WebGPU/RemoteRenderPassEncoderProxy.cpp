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
#include "RemoteRenderPassEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderPassEncoderMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUCppAPI.h>
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

void RemoteRenderPassEncoderProxy::setPipeline(const ::WebGPU::RenderPipeline& renderPipeline)
{
    auto convertedRenderPipeline = m_convertToBackingContext->convertToBacking(renderPipeline);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetPipeline(convertedRenderPipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setIndexBuffer(const ::WebGPU::Buffer& buffer, ::WebGPU::IndexFormat indexFormat, uint64_t offset, std::optional<uint64_t> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetIndexBuffer(convertedBuffer, indexFormat, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setVertexBuffer(uint32_t slot, const ::WebGPU::Buffer* buffer, uint64_t offset, std::optional<uint64_t> size)
{
    if (!buffer) {
        auto sendResult = send(Messages::RemoteRenderPassEncoder::UnsetVertexBuffer(slot, offset, size));
        UNUSED_VARIABLE(sendResult);
        return;
    }

    auto convertedBuffer = m_convertToBackingContext->convertToBacking(*buffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetVertexBuffer(slot, convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::draw(uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::Draw(vertexCount, instanceCount, firstVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t baseVertex,
    uint32_t firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::drawIndexedIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::DrawIndexedIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBindGroup(uint32_t index, const ::WebGPU::BindGroup* bindGroup, std::optional<std::span<const uint32_t>> dynamicOffsets)
{
    std::optional<WebGPUIdentifier> convertedBindGroup;
    if (bindGroup)
        convertedBindGroup = m_convertToBackingContext->convertToBacking(*bindGroup);

    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets ? std::optional { Vector<uint32_t>(*dynamicOffsets) } : std::nullopt));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::PushDebugGroup(WTF::move(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::InsertDebugMarker(WTF::move(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetViewport(x, y, width, height, minDepth, maxDepth));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setScissorRect(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetScissorRect(x, y, width, height));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setBlendConstant(const ::WebGPU::Color& color)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetBlendConstant(color));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setStencilReference(uint32_t stencilValue)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetStencilReference(stencilValue));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::beginOcclusionQuery(uint32_t queryIndex)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::BeginOcclusionQuery(queryIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::endOcclusionQuery()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::EndOcclusionQuery());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::executeBundles(std::span<const Ref<::WebGPU::RenderBundle>> renderBundles)
{
    auto convertedRenderBundles = WTF::compactMap(renderBundles, [&](auto& renderBundle) -> std::optional<WebGPUIdentifier> {
        return m_convertToBackingContext->convertToBacking(renderBundle);
    });

    auto sendResult = send(Messages::RemoteRenderPassEncoder::ExecuteBundles(WTF::move(convertedRenderBundles)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::end()
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::End());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderPassEncoderProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteRenderPassEncoder::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteRenderPassEncoderProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

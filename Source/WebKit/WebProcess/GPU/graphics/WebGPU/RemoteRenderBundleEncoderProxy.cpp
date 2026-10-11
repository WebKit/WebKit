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
#include "RemoteRenderBundleEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteRenderBundleEncoderMessages.h"
#include "RemoteRenderBundleProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderBundleEncoderProxy);

RemoteRenderBundleEncoderProxy::RemoteRenderBundleEncoderProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteRenderBundleEncoderProxy::~RemoteRenderBundleEncoderProxy()
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setPipeline(const ::WebGPU::RenderPipeline& renderPipeline)
{
    auto convertedRenderPipeline = m_convertToBackingContext->convertToBacking(renderPipeline);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetPipeline(convertedRenderPipeline));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setIndexBuffer(const ::WebGPU::Buffer& buffer, ::WebGPU::IndexFormat indexFormat, uint64_t offset, std::optional<uint64_t> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetIndexBuffer(convertedBuffer, indexFormat, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setVertexBuffer(uint32_t slot, const ::WebGPU::Buffer* buffer, uint64_t offset, std::optional<uint64_t> size)
{
    if (!buffer) {
        auto sendResult = send(Messages::RemoteRenderBundleEncoder::UnsetVertexBuffer(slot, offset, size));
        UNUSED_VARIABLE(sendResult);
        return;
    }
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(*buffer);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetVertexBuffer(slot, convertedBuffer, offset, size));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::draw(uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::Draw(vertexCount, instanceCount, firstVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndexed(uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t baseVertex,
    uint32_t firstInstance)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::drawIndexedIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset)
{
    auto convertedIndirectBuffer = m_convertToBackingContext->convertToBacking(indirectBuffer);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::DrawIndexedIndirect(convertedIndirectBuffer, indirectOffset));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::setBindGroup(uint32_t index, const ::WebGPU::BindGroup* bindGroup, std::optional<std::span<const uint32_t>> dynamicOffsets)
{
    std::optional<WebGPUIdentifier> convertedBindGroup;
    if (bindGroup)
        convertedBindGroup = m_convertToBackingContext->convertToBacking(*bindGroup);

    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetBindGroup(index, convertedBindGroup, dynamicOffsets ? std::optional { Vector<uint32_t>(*dynamicOffsets) } : std::nullopt));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::PushDebugGroup(WTF::move(groupLabel)));
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::popDebugGroup()
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::PopDebugGroup());
    UNUSED_VARIABLE(sendResult);
}

void RemoteRenderBundleEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::InsertDebugMarker(WTF::move(markerLabel)));
    UNUSED_VARIABLE(sendResult);
}

RefPtr<::WebGPU::RenderBundle> RemoteRenderBundleEncoderProxy::finish(const ::WebGPU::RenderBundleDescriptor& descriptor)
{
    Ref convertToBackingContext = m_convertToBackingContext;
    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::Finish(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteRenderBundleProxy::create(m_parent, convertToBackingContext, identifier);
}

void RemoteRenderBundleEncoderProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteRenderBundleEncoder::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteRenderBundleEncoderProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

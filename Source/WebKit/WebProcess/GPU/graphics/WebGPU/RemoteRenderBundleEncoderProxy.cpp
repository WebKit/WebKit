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

#include "RemoteRenderBundleProxy.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteRenderBundleEncoderProxy::RemoteRenderBundleEncoderProxy(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

RemoteRenderBundleEncoderProxy::~RemoteRenderBundleEncoderProxy()
{
}

void RemoteRenderBundleEncoderProxy::setPipeline(const PAL::WebGPU::RenderPipeline& renderPipeline)
{
    UNUSED_PARAM(renderPipeline);
}

void RemoteRenderBundleEncoderProxy::setIndexBuffer(const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::IndexFormat indexFormat, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(indexFormat);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderBundleEncoderProxy::setVertexBuffer(PAL::WebGPU::Index32 slot, const PAL::WebGPU::Buffer& buffer, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderBundleEncoderProxy::draw(PAL::WebGPU::Size32 vertexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstVertex, PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderBundleEncoderProxy::drawIndexed(PAL::WebGPU::Size32 indexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstIndex,
    PAL::WebGPU::SignedOffset32 baseVertex,
    PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(indexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstIndex);
    UNUSED_PARAM(baseVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderBundleEncoderProxy::drawIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderBundleEncoderProxy::drawIndexedIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderBundleEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsets);
}

void RemoteRenderBundleEncoderProxy::setBindGroup(PAL::WebGPU::Index32 index, const PAL::WebGPU::BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    PAL::WebGPU::Size64 dynamicOffsetsDataStart,
    PAL::WebGPU::Size32 dynamicOffsetsDataLength)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsetsArrayBuffer);
    UNUSED_PARAM(dynamicOffsetsArrayBufferLength);
    UNUSED_PARAM(dynamicOffsetsDataStart);
    UNUSED_PARAM(dynamicOffsetsDataLength);
}

void RemoteRenderBundleEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteRenderBundleEncoderProxy::popDebugGroup()
{
}

void RemoteRenderBundleEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

Ref<PAL::WebGPU::RenderBundle> RemoteRenderBundleEncoderProxy::finish(const PAL::WebGPU::RenderBundleDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RemoteRenderBundleProxy::create(m_convertToBackingContext);
}

void RemoteRenderBundleEncoderProxy::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

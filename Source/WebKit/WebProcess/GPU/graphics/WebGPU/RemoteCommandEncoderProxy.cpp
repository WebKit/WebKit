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
#include "RemoteCommandEncoderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteCommandBufferProxy.h"
#include "RemoteComputePassEncoderProxy.h"
#include "RemoteRenderPassEncoderProxy.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteCommandEncoderProxy::RemoteCommandEncoderProxy(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

RemoteCommandEncoderProxy::~RemoteCommandEncoderProxy()
{
}

Ref<PAL::WebGPU::RenderPassEncoder> RemoteCommandEncoderProxy::beginRenderPass(const PAL::WebGPU::RenderPassDescriptor& descriptor)
{
    return RemoteRenderPassEncoderProxy::create(m_convertToBackingContext);
}

Ref<PAL::WebGPU::ComputePassEncoder> RemoteCommandEncoderProxy::beginComputePass(const std::optional<PAL::WebGPU::ComputePassDescriptor>& descriptor)
{
    return RemoteComputePassEncoderProxy::create(m_convertToBackingContext);
}

void RemoteCommandEncoderProxy::copyBufferToBuffer(
    const PAL::WebGPU::Buffer& source,
    PAL::WebGPU::Size64 sourceOffset,
    const PAL::WebGPU::Buffer& destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(sourceOffset);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void RemoteCommandEncoderProxy::copyBufferToTexture(
    const PAL::WebGPU::ImageCopyBuffer& source,
    const PAL::WebGPU::ImageCopyTexture& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoderProxy::copyTextureToBuffer(
    const PAL::WebGPU::ImageCopyTexture& source,
    const PAL::WebGPU::ImageCopyBuffer& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoderProxy::copyTextureToTexture(
    const PAL::WebGPU::ImageCopyTexture& source,
    const PAL::WebGPU::ImageCopyTexture& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoderProxy::fillBuffer(
    const PAL::WebGPU::Buffer& destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void RemoteCommandEncoderProxy::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteCommandEncoderProxy::popDebugGroup()
{
}

void RemoteCommandEncoderProxy::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteCommandEncoderProxy::writeTimestamp(const PAL::WebGPU::QuerySet& querySet, PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RemoteCommandEncoderProxy::resolveQuerySet(
    const PAL::WebGPU::QuerySet& querySet,
    PAL::WebGPU::Size32 firstQuery,
    PAL::WebGPU::Size32 queryCount,
    const PAL::WebGPU::Buffer& destination,
    PAL::WebGPU::Size64 destinationOffset)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(firstQuery);
    UNUSED_PARAM(queryCount);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
}

Ref<PAL::WebGPU::CommandBuffer> RemoteCommandEncoderProxy::finish(const PAL::WebGPU::CommandBufferDescriptor& descriptor)
{
    return RemoteCommandBufferProxy::create(m_convertToBackingContext);
}

void RemoteCommandEncoderProxy::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)

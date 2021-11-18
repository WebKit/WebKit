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
#include "RemoteCommandEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUComputePassDescriptor.h"
#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPUCommandEncoder.h>

namespace WebKit {

RemoteCommandEncoder::RemoteCommandEncoder(PAL::WebGPU::CommandEncoder& commandEncoder, WebGPU::ObjectHeap& objectHeap)
    : m_backing(commandEncoder)
    , m_objectHeap(objectHeap)
{
}

RemoteCommandEncoder::~RemoteCommandEncoder()
{
}

void RemoteCommandEncoder::beginRenderPass(const WebGPU::RenderPassDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteCommandEncoder::beginComputePass(const std::optional<WebGPU::ComputePassDescriptor>& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteCommandEncoder::copyBufferToBuffer(
    WebGPUIdentifier source,
    PAL::WebGPU::Size64 sourceOffset,
    WebGPUIdentifier destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(sourceOffset);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void RemoteCommandEncoder::copyBufferToTexture(
    const WebGPU::ImageCopyBuffer& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoder::copyTextureToBuffer(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyBuffer& destination,
    const WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoder::copyTextureToTexture(
    const WebGPU::ImageCopyTexture& source,
    const WebGPU::ImageCopyTexture& destination,
    const WebGPU::Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void RemoteCommandEncoder::fillBuffer(
    WebGPUIdentifier destination,
    PAL::WebGPU::Size64 destinationOffset,
    PAL::WebGPU::Size64 size)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void RemoteCommandEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteCommandEncoder::popDebugGroup()
{
}

void RemoteCommandEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteCommandEncoder::writeTimestamp(WebGPUIdentifier querySet, PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RemoteCommandEncoder::resolveQuerySet(
    WebGPUIdentifier querySet,
    PAL::WebGPU::Size32 firstQuery,
    PAL::WebGPU::Size32 queryCount,
    WebGPUIdentifier destination,
    PAL::WebGPU::Size64 destinationOffset)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(firstQuery);
    UNUSED_PARAM(queryCount);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
}

void RemoteCommandEncoder::finish(const WebGPU::CommandBufferDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteCommandEncoder::setLabel(String&& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

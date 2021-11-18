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
#include "RemoteRenderBundleEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoder.h>

namespace WebKit {

RemoteRenderBundleEncoder::RemoteRenderBundleEncoder(PAL::WebGPU::RenderBundleEncoder& renderBundleEncoder, WebGPU::ObjectHeap& objectHeap)
    : m_backing(renderBundleEncoder)
    , m_objectHeap(objectHeap)
{
}

RemoteRenderBundleEncoder::~RemoteRenderBundleEncoder()
{
}

void RemoteRenderBundleEncoder::setPipeline(WebGPUIdentifier renderPipeline)
{
    UNUSED_PARAM(renderPipeline);
}

void RemoteRenderBundleEncoder::setIndexBuffer(WebGPUIdentifier buffer, PAL::WebGPU::IndexFormat indexFormat, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(indexFormat);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderBundleEncoder::setVertexBuffer(PAL::WebGPU::Index32 slot, WebGPUIdentifier buffer, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> size)
{
    UNUSED_PARAM(slot);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

void RemoteRenderBundleEncoder::draw(PAL::WebGPU::Size32 vertexCount, PAL::WebGPU::Size32 instanceCount,
    PAL::WebGPU::Size32 firstVertex, PAL::WebGPU::Size32 firstInstance)
{
    UNUSED_PARAM(vertexCount);
    UNUSED_PARAM(instanceCount);
    UNUSED_PARAM(firstVertex);
    UNUSED_PARAM(firstInstance);
}

void RemoteRenderBundleEncoder::drawIndexed(PAL::WebGPU::Size32 indexCount, PAL::WebGPU::Size32 instanceCount,
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

void RemoteRenderBundleEncoder::drawIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderBundleEncoder::drawIndexedIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteRenderBundleEncoder::setBindGroup(PAL::WebGPU::Index32 index, WebGPUIdentifier bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(dynamicOffsets);
}

void RemoteRenderBundleEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteRenderBundleEncoder::popDebugGroup()
{
}

void RemoteRenderBundleEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteRenderBundleEncoder::finish(const WebGPU::RenderBundleDescriptor& descriptor, WebGPUIdentifier identifier)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(identifier);
}

void RemoteRenderBundleEncoder::setLabel(String&& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

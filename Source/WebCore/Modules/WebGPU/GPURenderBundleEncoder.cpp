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
#include "GPURenderBundleEncoder.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPURenderBundle.h"
#include "GPURenderPipeline.h"

namespace WebCore {

String GPURenderBundleEncoder::label() const
{
    return m_backing->label();
}

void GPURenderBundleEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPURenderBundleEncoder::setPipeline(const GPURenderPipeline& renderPipeline)
{
    m_backing->setPipeline(renderPipeline.backing());
}

void GPURenderBundleEncoder::setIndexBuffer(const GPUBuffer& buffer, GPUIndexFormat indexFormat, GPUSize64 offset, std::optional<GPUSize64> size)
{
    m_backing->setIndexBuffer(buffer.backing(), convertToBacking(indexFormat), offset, size);
}

void GPURenderBundleEncoder::setVertexBuffer(GPUIndex32 slot, const GPUBuffer& buffer, GPUSize64 offset, std::optional<GPUSize64> size)
{
    m_backing->setVertexBuffer(slot, buffer.backing(), offset, size);
}

void GPURenderBundleEncoder::draw(GPUSize32 vertexCount, GPUSize32 instanceCount,
    GPUSize32 firstVertex, GPUSize32 firstInstance)
{
    m_backing->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndexed(GPUSize32 indexCount, GPUSize32 instanceCount,
    GPUSize32 firstIndex,
    GPUSignedOffset32 baseVertex,
    GPUSize32 firstInstance)
{
    m_backing->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    m_backing->drawIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderBundleEncoder::drawIndexedIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    m_backing->drawIndexedIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderBundleEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup& bindGroup,
    std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets)
{
    m_backing->setBindGroup(index, bindGroup.backing(), WTFMove(dynamicOffsets));
}

void GPURenderBundleEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup& bindGroup,
    const Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    m_backing->setBindGroup(index, bindGroup.backing(), dynamicOffsetsData.data(), dynamicOffsetsData.length(), dynamicOffsetsDataStart, dynamicOffsetsDataLength);
}

void GPURenderBundleEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void GPURenderBundleEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void GPURenderBundleEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

static PAL::WebGPU::RenderBundleDescriptor convertToBacking(const std::optional<GPURenderBundleDescriptor>& renderBundleDescriptor)
{
    if (!renderBundleDescriptor)
        return { };
    return renderBundleDescriptor->convertToBacking();
}

Ref<GPURenderBundle> GPURenderBundleEncoder::finish(const std::optional<GPURenderBundleDescriptor>& renderBundleDescriptor)
{
    return GPURenderBundle::create(m_backing->finish(convertToBacking(renderBundleDescriptor)));
}

}

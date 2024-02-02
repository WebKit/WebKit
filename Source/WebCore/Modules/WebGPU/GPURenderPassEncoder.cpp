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
#include "GPURenderPassEncoder.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPUQuerySet.h"
#include "GPURenderBundle.h"
#include "GPURenderPipeline.h"

namespace WebCore {

String GPURenderPassEncoder::label() const
{
    return m_backing->label();
}

void GPURenderPassEncoder::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPURenderPassEncoder::setPipeline(const GPURenderPipeline& renderPipeline)
{
    m_backing->setPipeline(renderPipeline.backing());
}

void GPURenderPassEncoder::setIndexBuffer(const GPUBuffer& buffer, GPUIndexFormat indexFormat, std::optional<GPUSize64> offset, std::optional<GPUSize64> size)
{
    m_backing->setIndexBuffer(buffer.backing(), convertToBacking(indexFormat), offset, size);
}

void GPURenderPassEncoder::setVertexBuffer(GPUIndex32 slot, const GPUBuffer* buffer, std::optional<GPUSize64> offset, std::optional<GPUSize64> size)
{
    m_backing->setVertexBuffer(slot, buffer ? &buffer->backing() : nullptr, offset, size);
}

void GPURenderPassEncoder::draw(GPUSize32 vertexCount, std::optional<GPUSize32> instanceCount,
    std::optional<GPUSize32> firstVertex, std::optional<GPUSize32> firstInstance)
{
    m_backing->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndexed(GPUSize32 indexCount, std::optional<GPUSize32> instanceCount,
    std::optional<GPUSize32> firstIndex,
    std::optional<GPUSignedOffset32> baseVertex,
    std::optional<GPUSize32> firstInstance)
{
    m_backing->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    m_backing->drawIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderPassEncoder::drawIndexedIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    m_backing->drawIndexedIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderPassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup& bindGroup,
    std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets)
{
    m_backing->setBindGroup(index, bindGroup.backing(), WTFMove(dynamicOffsets));
}

ExceptionOr<void> GPURenderPassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup& bindGroup,
    const Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    auto offset = checkedSum<uint64_t>(dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    if (offset.hasOverflowed() || offset > dynamicOffsetsData.length())
        return Exception { ExceptionCode::RangeError, "dynamic offsets overflowed"_s };

    m_backing->setBindGroup(index, bindGroup.backing(), dynamicOffsetsData.data(), dynamicOffsetsData.length(), dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    return { };
}

void GPURenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    m_backing->pushDebugGroup(WTFMove(groupLabel));
}

void GPURenderPassEncoder::popDebugGroup()
{
    m_backing->popDebugGroup();
}

void GPURenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    m_backing->insertDebugMarker(WTFMove(markerLabel));
}

void GPURenderPassEncoder::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    m_backing->setViewport(x, y, width, height, minDepth, maxDepth);
}

void GPURenderPassEncoder::setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
    GPUIntegerCoordinate width, GPUIntegerCoordinate height)
{
    m_backing->setScissorRect(x, y, width, height);
}

void GPURenderPassEncoder::setBlendConstant(GPUColor color)
{
    m_backing->setBlendConstant(convertToBacking(color));
}

void GPURenderPassEncoder::setStencilReference(GPUStencilValue stencilValue)
{
    m_backing->setStencilReference(stencilValue);
}

void GPURenderPassEncoder::beginOcclusionQuery(GPUSize32 queryIndex)
{
    m_backing->beginOcclusionQuery(queryIndex);
}

void GPURenderPassEncoder::endOcclusionQuery()
{
    m_backing->endOcclusionQuery();
}

void GPURenderPassEncoder::executeBundles(Vector<RefPtr<GPURenderBundle>>&& bundles)
{
    auto result = WTF::compactMap(bundles, [](auto& bundle) -> std::optional<std::reference_wrapper<WebGPU::RenderBundle>> {
        if (bundle)
            return bundle->backing();
        return std::nullopt;
    });
    m_backing->executeBundles(WTFMove(result));
}

void GPURenderPassEncoder::end()
{
    m_backing->end();
}

}

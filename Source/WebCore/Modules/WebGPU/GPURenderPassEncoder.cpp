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
#include "WebGPUDevice.h"

namespace WebCore {

GPURenderPassEncoder::GPURenderPassEncoder(Ref<WebGPU::RenderPassEncoder>&& backing, WebGPU::Device& device)
    : m_backing(WTFMove(backing))
    , m_device(&device)
{
}

String GPURenderPassEncoder::label() const
{
    return m_backing->label();
}

void GPURenderPassEncoder::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

void GPURenderPassEncoder::setPipeline(const GPURenderPipeline& renderPipeline)
{
    protectedBacking()->setPipeline(renderPipeline.backing());
}

void GPURenderPassEncoder::setIndexBuffer(const GPUBuffer& buffer, GPUIndexFormat indexFormat, std::optional<GPUSize64> offset, std::optional<GPUSize64> size)
{
    protectedBacking()->setIndexBuffer(buffer.backing(), convertToBacking(indexFormat), offset, size);
}

void GPURenderPassEncoder::setVertexBuffer(GPUIndex32 slot, const GPUBuffer* buffer, std::optional<GPUSize64> offset, std::optional<GPUSize64> size)
{
    protectedBacking()->setVertexBuffer(slot, buffer ? &buffer->backing() : nullptr, offset, size);
}

void GPURenderPassEncoder::draw(GPUSize32 vertexCount, std::optional<GPUSize32> instanceCount,
    std::optional<GPUSize32> firstVertex, std::optional<GPUSize32> firstInstance)
{
    protectedBacking()->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndexed(GPUSize32 indexCount, std::optional<GPUSize32> instanceCount,
    std::optional<GPUSize32> firstIndex,
    std::optional<GPUSignedOffset32> baseVertex,
    std::optional<GPUSize32> firstInstance)
{
    protectedBacking()->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    protectedBacking()->drawIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderPassEncoder::drawIndexedIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    protectedBacking()->drawIndexedIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPURenderPassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup* bindGroup,
    std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets)
{
    protectedBacking()->setBindGroup(index, bindGroup ? &bindGroup->backing() : nullptr, WTFMove(dynamicOffsets));
}

ExceptionOr<void> GPURenderPassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup* bindGroup,
    const Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    auto offset = checkedSum<uint64_t>(dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    if (offset.hasOverflowed() || offset > dynamicOffsetsData.length())
        return Exception { ExceptionCode::RangeError, "dynamic offsets overflowed"_s };

    protectedBacking()->setBindGroup(index, bindGroup ? &bindGroup->backing() : nullptr, dynamicOffsetsData.typedSpan(), dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    return { };
}

void GPURenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    protectedBacking()->pushDebugGroup(WTFMove(groupLabel));
}

void GPURenderPassEncoder::popDebugGroup()
{
    protectedBacking()->popDebugGroup();
}

void GPURenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    protectedBacking()->insertDebugMarker(WTFMove(markerLabel));
}

void GPURenderPassEncoder::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    protectedBacking()->setViewport(x, y, width, height, minDepth, maxDepth);
}

void GPURenderPassEncoder::setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
    GPUIntegerCoordinate width, GPUIntegerCoordinate height)
{
    protectedBacking()->setScissorRect(x, y, width, height);
}

void GPURenderPassEncoder::setBlendConstant(GPUColor color)
{
    protectedBacking()->setBlendConstant(convertToBacking(color));
}

void GPURenderPassEncoder::setStencilReference(GPUStencilValue stencilValue)
{
    protectedBacking()->setStencilReference(stencilValue);
}

void GPURenderPassEncoder::beginOcclusionQuery(GPUSize32 queryIndex)
{
    protectedBacking()->beginOcclusionQuery(queryIndex);
}

void GPURenderPassEncoder::endOcclusionQuery()
{
    protectedBacking()->endOcclusionQuery();
}

void GPURenderPassEncoder::executeBundles(Vector<Ref<GPURenderBundle>>&& bundles)
{
    auto result = WTF::map(bundles, [](auto& bundle) -> Ref<WebGPU::RenderBundle> {
        return bundle->backing();
    });
    protectedBacking()->executeBundles(WTFMove(result));
}

void GPURenderPassEncoder::end()
{
    protectedBacking()->end();
    if (RefPtr device = m_device.get())
        m_backing = device->invalidRenderPassEncoder();
}

}

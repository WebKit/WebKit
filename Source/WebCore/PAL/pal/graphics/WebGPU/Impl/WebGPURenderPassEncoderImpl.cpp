/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "WebGPURenderPassEncoderImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBindGroupImpl.h"
#include "WebGPUBufferImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUQuerySetImpl.h"
#include "WebGPURenderBundleImpl.h"
#include "WebGPURenderPipelineImpl.h"
#include <WebGPU/WebGPUExt.h>

namespace PAL::WebGPU {

RenderPassEncoderImpl::RenderPassEncoderImpl(WGPURenderPassEncoder renderPassEncoder, ConvertToBackingContext& convertToBackingContext)
    : m_backing(renderPassEncoder)
    , m_convertToBackingContext(convertToBackingContext)
{
}

RenderPassEncoderImpl::~RenderPassEncoderImpl()
{
    wgpuRenderPassEncoderRelease(m_backing);
}

void RenderPassEncoderImpl::setPipeline(const RenderPipeline& renderPipeline)
{
    wgpuRenderPassEncoderSetPipeline(m_backing, m_convertToBackingContext->convertToBacking(renderPipeline));
}

void RenderPassEncoderImpl::setIndexBuffer(const Buffer& buffer, IndexFormat indexFormat, std::optional<Size64> offset, std::optional<Size64> size)
{
    wgpuRenderPassEncoderSetIndexBuffer(m_backing, m_convertToBackingContext->convertToBacking(buffer), m_convertToBackingContext->convertToBacking(indexFormat), offset.value_or(0), size.value_or(WGPU_WHOLE_SIZE));
}

void RenderPassEncoderImpl::setVertexBuffer(Index32 slot, const Buffer& buffer, std::optional<Size64> offset, std::optional<Size64> size)
{
    wgpuRenderPassEncoderSetVertexBuffer(m_backing, slot, m_convertToBackingContext->convertToBacking(buffer), offset.value_or(0), size.value_or(WGPU_WHOLE_SIZE));
}

void RenderPassEncoderImpl::draw(Size32 vertexCount, std::optional<Size32> instanceCount,
    std::optional<Size32> firstVertex, std::optional<Size32> firstInstance)
{
    wgpuRenderPassEncoderDraw(m_backing, vertexCount, instanceCount.value_or(1), firstVertex.value_or(0), firstInstance.value_or(0));
}

void RenderPassEncoderImpl::drawIndexed(Size32 indexCount, std::optional<Size32> instanceCount,
    std::optional<Size32> firstIndex,
    std::optional<SignedOffset32> baseVertex,
    std::optional<Size32> firstInstance)
{
    wgpuRenderPassEncoderDrawIndexed(m_backing, indexCount, instanceCount.value_or(1), firstIndex.value_or(0), baseVertex.value_or(0), firstInstance.value_or(0));
}

void RenderPassEncoderImpl::drawIndirect(const Buffer& indirectBuffer, Size64 indirectOffset)
{
    wgpuRenderPassEncoderDrawIndirect(m_backing, m_convertToBackingContext->convertToBacking(indirectBuffer), indirectOffset);
}

void RenderPassEncoderImpl::drawIndexedIndirect(const Buffer& indirectBuffer, Size64 indirectOffset)
{
    wgpuRenderPassEncoderDrawIndexedIndirect(m_backing, m_convertToBackingContext->convertToBacking(indirectBuffer), indirectOffset);
}

void RenderPassEncoderImpl::setBindGroup(Index32 index, const BindGroup& bindGroup,
    std::optional<Vector<BufferDynamicOffset>>&& dynamicOffsets)
{
    auto backingOffsets = valueOrDefault(dynamicOffsets);
    wgpuRenderPassEncoderSetBindGroup(m_backing, index, m_convertToBackingContext->convertToBacking(bindGroup), backingOffsets.size(), backingOffsets.data());
}

void RenderPassEncoderImpl::setBindGroup(Index32 index, const BindGroup& bindGroup,
    const uint32_t* dynamicOffsetsArrayBuffer,
    size_t dynamicOffsetsArrayBufferLength,
    Size64 dynamicOffsetsDataStart,
    Size32 dynamicOffsetsDataLength)
{
    UNUSED_PARAM(dynamicOffsetsArrayBufferLength);
    // FIXME: Use checked algebra.
    wgpuRenderPassEncoderSetBindGroup(m_backing, index, m_convertToBackingContext->convertToBacking(bindGroup), dynamicOffsetsDataLength, dynamicOffsetsArrayBuffer + dynamicOffsetsDataStart);
}

void RenderPassEncoderImpl::pushDebugGroup(String&& groupLabel)
{
    wgpuRenderPassEncoderPushDebugGroup(m_backing, groupLabel.utf8().data());
}

void RenderPassEncoderImpl::popDebugGroup()
{
    wgpuRenderPassEncoderPopDebugGroup(m_backing);
}

void RenderPassEncoderImpl::insertDebugMarker(String&& markerLabel)
{
    wgpuRenderPassEncoderInsertDebugMarker(m_backing, markerLabel.utf8().data());
}

void RenderPassEncoderImpl::setViewport(float x, float y,
    float width, float height,
    float minDepth, float maxDepth)
{
    wgpuRenderPassEncoderSetViewport(m_backing, x, y, width, height, minDepth, maxDepth);
}

void RenderPassEncoderImpl::setScissorRect(IntegerCoordinate x, IntegerCoordinate y,
    IntegerCoordinate width, IntegerCoordinate height)
{
    wgpuRenderPassEncoderSetScissorRect(m_backing, x, y, width, height);
}

void RenderPassEncoderImpl::setBlendConstant(Color color)
{
    auto backingColor = m_convertToBackingContext->convertToBacking(color);

    wgpuRenderPassEncoderSetBlendConstant(m_backing, &backingColor);
}

void RenderPassEncoderImpl::setStencilReference(StencilValue stencilValue)
{
    wgpuRenderPassEncoderSetStencilReference(m_backing, stencilValue);
}

void RenderPassEncoderImpl::beginOcclusionQuery(Size32 queryIndex)
{
    wgpuRenderPassEncoderBeginOcclusionQuery(m_backing, queryIndex);
}

void RenderPassEncoderImpl::endOcclusionQuery()
{
    wgpuRenderPassEncoderEndOcclusionQuery(m_backing);
}

void RenderPassEncoderImpl::executeBundles(Vector<std::reference_wrapper<RenderBundle>>&& renderBundles)
{
    auto backingBundles = renderBundles.map([this] (auto renderBundle) {
        return m_convertToBackingContext->convertToBacking(renderBundle.get());
    });

    wgpuRenderPassEncoderExecuteBundles(m_backing, backingBundles.size(), backingBundles.data());
}

void RenderPassEncoderImpl::end()
{
    wgpuRenderPassEncoderEnd(m_backing);
}

void RenderPassEncoderImpl::setLabelInternal(const String& label)
{
    wgpuRenderPassEncoderSetLabel(m_backing, label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)

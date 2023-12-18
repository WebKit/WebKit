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
#include "WebGPURenderBundleEncoderImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBindGroupImpl.h"
#include "WebGPUBufferImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPURenderBundleImpl.h"
#include "WebGPURenderPipelineImpl.h"
#include <WebGPU/WebGPUExt.h>

namespace WebCore::WebGPU {

RenderBundleEncoderImpl::RenderBundleEncoderImpl(WebGPUPtr<WGPURenderBundleEncoder>&& renderBundleEncoder, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(renderBundleEncoder))
    , m_convertToBackingContext(convertToBackingContext)
{
}

RenderBundleEncoderImpl::~RenderBundleEncoderImpl() = default;

void RenderBundleEncoderImpl::setPipeline(const RenderPipeline& renderPipeline)
{
    wgpuRenderBundleEncoderSetPipeline(m_backing.get(), m_convertToBackingContext->convertToBacking(renderPipeline));
}

void RenderBundleEncoderImpl::setIndexBuffer(const Buffer& buffer, IndexFormat indexFormat, std::optional<Size64> offset, std::optional<Size64> size)
{
    wgpuRenderBundleEncoderSetIndexBuffer(m_backing.get(), m_convertToBackingContext->convertToBacking(buffer), m_convertToBackingContext->convertToBacking(indexFormat), offset.value_or(0), size.value_or(WGPU_WHOLE_SIZE));
}

void RenderBundleEncoderImpl::setVertexBuffer(Index32 slot, const Buffer& buffer, std::optional<Size64> offset, std::optional<Size64> size)
{
    wgpuRenderBundleEncoderSetVertexBuffer(m_backing.get(), slot, m_convertToBackingContext->convertToBacking(buffer), offset.value_or(0), size.value_or(WGPU_WHOLE_SIZE));
}

void RenderBundleEncoderImpl::draw(Size32 vertexCount, std::optional<Size32> instanceCount,
    std::optional<Size32> firstVertex, std::optional<Size32> firstInstance)
{
    wgpuRenderBundleEncoderDraw(m_backing.get(), vertexCount, instanceCount.value_or(1), firstVertex.value_or(0), firstInstance.value_or(0));
}

void RenderBundleEncoderImpl::drawIndexed(Size32 indexCount, std::optional<Size32> instanceCount,
    std::optional<Size32> firstIndex,
    std::optional<SignedOffset32> baseVertex,
    std::optional<Size32> firstInstance)
{
    wgpuRenderBundleEncoderDrawIndexed(m_backing.get(), indexCount, instanceCount.value_or(1), firstIndex.value_or(0), baseVertex.value_or(0), firstInstance.value_or(0));
}

void RenderBundleEncoderImpl::drawIndirect(const Buffer& indirectBuffer, Size64 indirectOffset)
{
    wgpuRenderBundleEncoderDrawIndirect(m_backing.get(), m_convertToBackingContext->convertToBacking(indirectBuffer), indirectOffset);
}

void RenderBundleEncoderImpl::drawIndexedIndirect(const Buffer& indirectBuffer, Size64 indirectOffset)
{
    wgpuRenderBundleEncoderDrawIndexedIndirect(m_backing.get(), m_convertToBackingContext->convertToBacking(indirectBuffer), indirectOffset);
}

void RenderBundleEncoderImpl::setBindGroup(Index32 index, const BindGroup& bindGroup,
    std::optional<Vector<BufferDynamicOffset>>&& dynamicOffsets)
{
    auto backingOffsets = valueOrDefault(dynamicOffsets);
    wgpuRenderBundleEncoderSetBindGroupWithDynamicOffsets(m_backing.get(), index, m_convertToBackingContext->convertToBacking(bindGroup), WTFMove(dynamicOffsets));
}

void RenderBundleEncoderImpl::setBindGroup(Index32, const BindGroup&,
    const uint32_t*,
    size_t,
    Size64,
    Size32)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RenderBundleEncoderImpl::pushDebugGroup(String&& groupLabel)
{
    wgpuRenderBundleEncoderPushDebugGroup(m_backing.get(), groupLabel.utf8().data());
}

void RenderBundleEncoderImpl::popDebugGroup()
{
    wgpuRenderBundleEncoderPopDebugGroup(m_backing.get());
}

void RenderBundleEncoderImpl::insertDebugMarker(String&& markerLabel)
{
    wgpuRenderBundleEncoderInsertDebugMarker(m_backing.get(), markerLabel.utf8().data());
}

Ref<RenderBundle> RenderBundleEncoderImpl::finish(const RenderBundleDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPURenderBundleDescriptor backingDescriptor {
        nullptr,
        label.data(),
    };

    return RenderBundleImpl::create(adoptWebGPU(wgpuRenderBundleEncoderFinish(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

void RenderBundleEncoderImpl::setLabelInternal(const String& label)
{
    wgpuRenderBundleEncoderSetLabel(m_backing.get(), label.utf8().data());
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)

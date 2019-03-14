/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebGPUCommandBuffer.h"

#if ENABLE(WEBGPU)

#include "GPURenderPassDescriptor.h"
#include "GPURenderPassEncoder.h"
#include "WebGPUBuffer.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPassEncoder.h"
#include "WebGPUTexture.h"
#include <wtf/Optional.h>

namespace WebCore {

Optional<GPUBufferCopyView> WebGPUBufferCopyView::tryCreateGPUBufferCopyView() const
{
    if (!buffer || !buffer->buffer()) {
        LOG(WebGPU, "GPUCommandEncoder: Invalid buffer for copy!");
        return WTF::nullopt;
    }

    // FIXME: Add Web GPU validation.

    return GPUBufferCopyView { buffer->buffer().releaseNonNull(), *this };
}

Optional<GPUTextureCopyView> WebGPUTextureCopyView::tryCreateGPUTextureCopyView() const
{
    if (!texture || !texture->texture()) {
        LOG(WebGPU, "GPUCommandEncoder: Invalid texture for copy!");
        return WTF::nullopt;
    }

    // FIXME: Add Web GPU validation.

    return GPUTextureCopyView { texture->texture().releaseNonNull(), *this };
}

Ref<WebGPUCommandBuffer> WebGPUCommandBuffer::create(Ref<GPUCommandBuffer>&& buffer)
{
    return adoptRef(*new WebGPUCommandBuffer(WTFMove(buffer)));
}

WebGPUCommandBuffer::WebGPUCommandBuffer(Ref<GPUCommandBuffer>&& buffer)
    : m_commandBuffer(WTFMove(buffer))
{
}

RefPtr<WebGPURenderPassEncoder> WebGPUCommandBuffer::beginRenderPass(WebGPURenderPassDescriptor&& descriptor)
{
    auto gpuDescriptor = descriptor.tryCreateGPURenderPassDescriptor();
    if (!gpuDescriptor)
        return nullptr;

    if (auto encoder = GPURenderPassEncoder::tryCreate(m_commandBuffer.copyRef(), WTFMove(*gpuDescriptor)))
        return WebGPURenderPassEncoder::create(*this, encoder.releaseNonNull());
    return nullptr;
}

void WebGPUCommandBuffer::copyBufferToBuffer(const WebGPUBuffer& src, unsigned long srcOffset, const WebGPUBuffer& dst, unsigned long dstOffset, unsigned long size)
{
    if (!src.buffer() || !dst.buffer()) {
        LOG(WebGPU, "GPUCommandBuffer::copyBufferToBuffer(): Invalid GPUBuffer!");
        return;
    }

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyBufferToBuffer(src.buffer().releaseNonNull(), srcOffset, dst.buffer().releaseNonNull(), dstOffset, size);
}

void WebGPUCommandBuffer::copyBufferToTexture(const WebGPUBufferCopyView& srcBuffer, const WebGPUTextureCopyView& dstTexture, const GPUExtent3D& size)
{
    auto gpuBufferView = srcBuffer.tryCreateGPUBufferCopyView();
    auto gpuTextureView = dstTexture.tryCreateGPUTextureCopyView();

    if (!gpuBufferView || !gpuTextureView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyBufferToTexture(WTFMove(*gpuBufferView), WTFMove(*gpuTextureView), size);
}

void WebGPUCommandBuffer::copyTextureToBuffer(const WebGPUTextureCopyView& srcTexture, const WebGPUBufferCopyView& dstBuffer, const GPUExtent3D& size)
{
    auto gpuTextureView = srcTexture.tryCreateGPUTextureCopyView();
    auto gpuBufferView = dstBuffer.tryCreateGPUBufferCopyView();

    if (!gpuTextureView || !gpuBufferView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyTextureToBuffer(WTFMove(*gpuTextureView), WTFMove(*gpuBufferView), size);
}

void WebGPUCommandBuffer::copyTextureToTexture(const WebGPUTextureCopyView& src, const WebGPUTextureCopyView& dst, const GPUExtent3D& size)
{
    auto gpuSrcView = src.tryCreateGPUTextureCopyView();
    auto gpuDstView = dst.tryCreateGPUTextureCopyView();

    if (!gpuSrcView || !gpuDstView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyTextureToTexture(WTFMove(*gpuSrcView), WTFMove(*gpuDstView), size);
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)

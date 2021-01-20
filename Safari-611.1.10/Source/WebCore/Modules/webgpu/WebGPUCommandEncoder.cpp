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
#include "WebGPUCommandEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUComputePassEncoder.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPassEncoder.h"
#include "Logging.h"
#include "WebGPUBuffer.h"
#include "WebGPUComputePassEncoder.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPassEncoder.h"
#include "WebGPUTexture.h"
#include <wtf/Optional.h>

namespace WebCore {

Optional<GPUBufferCopyView> WebGPUBufferCopyView::tryCreateGPUBufferCopyView() const
{
    if (!buffer || !buffer->buffer()) {
        LOG(WebGPU, "WebGPUCommandEncoder: Invalid buffer for copy!");
        return WTF::nullopt;
    }

    // FIXME: Add Web GPU validation.

    return GPUBufferCopyView { makeRef(*buffer->buffer()), *this };
}

Optional<GPUTextureCopyView> WebGPUTextureCopyView::tryCreateGPUTextureCopyView() const
{
    if (!texture || !texture->texture()) {
        LOG(WebGPU, "WebGPUCommandEncoder: Invalid texture for copy!");
        return WTF::nullopt;
    }

    // FIXME: Add Web GPU validation.

    return GPUTextureCopyView { makeRef(*texture->texture()), *this };
}

Ref<WebGPUCommandEncoder> WebGPUCommandEncoder::create(RefPtr<GPUCommandBuffer>&& buffer)
{
    return adoptRef(*new WebGPUCommandEncoder(WTFMove(buffer)));
}

WebGPUCommandEncoder::WebGPUCommandEncoder(RefPtr<GPUCommandBuffer>&& buffer)
    : m_commandBuffer(WTFMove(buffer))
{
}

Ref<WebGPURenderPassEncoder> WebGPUCommandEncoder::beginRenderPass(const WebGPURenderPassDescriptor& descriptor)
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::beginRenderPass(): Invalid operation!");
        return WebGPURenderPassEncoder::create(nullptr);
    }
    auto gpuDescriptor = descriptor.tryCreateGPURenderPassDescriptor();
    if (!gpuDescriptor)
        return WebGPURenderPassEncoder::create(nullptr);

    auto encoder = GPURenderPassEncoder::tryCreate(makeRef(*m_commandBuffer), WTFMove(*gpuDescriptor));
    return WebGPURenderPassEncoder::create(WTFMove(encoder));
}

Ref<WebGPUComputePassEncoder> WebGPUCommandEncoder::beginComputePass()
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::beginComputePass(): Invalid operation!");
        return WebGPUComputePassEncoder::create(nullptr);
    }
    auto encoder = GPUComputePassEncoder::tryCreate(makeRef(*m_commandBuffer));
    return WebGPUComputePassEncoder::create(WTFMove(encoder));
}

void WebGPUCommandEncoder::copyBufferToBuffer(WebGPUBuffer& src, uint64_t srcOffset, WebGPUBuffer& dst, uint64_t dstOffset, uint64_t size)
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::copyBufferToBuffer(): Invalid operation!");
        return;
    }
    if (!src.buffer() || !dst.buffer()) {
        LOG(WebGPU, "WebGPUCommandEncoder::copyBufferToBuffer(): Invalid GPUBuffer!");
        return;
    }

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyBufferToBuffer(makeRef(*src.buffer()), srcOffset, makeRef(*dst.buffer()), dstOffset, size);
}

void WebGPUCommandEncoder::copyBufferToTexture(const WebGPUBufferCopyView& srcBuffer, const WebGPUTextureCopyView& dstTexture, const GPUExtent3D& size)
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::copyBufferToTexture(): Invalid operation!");
        return;
    }
    auto gpuBufferView = srcBuffer.tryCreateGPUBufferCopyView();
    auto gpuTextureView = dstTexture.tryCreateGPUTextureCopyView();

    if (!gpuBufferView || !gpuTextureView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyBufferToTexture(WTFMove(*gpuBufferView), WTFMove(*gpuTextureView), size);
}

void WebGPUCommandEncoder::copyTextureToBuffer(const WebGPUTextureCopyView& srcTexture, const WebGPUBufferCopyView& dstBuffer, const GPUExtent3D& size)
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::copyTextureToBuffer(): Invalid operation!");
        return;
    }
    auto gpuTextureView = srcTexture.tryCreateGPUTextureCopyView();
    auto gpuBufferView = dstBuffer.tryCreateGPUBufferCopyView();

    if (!gpuTextureView || !gpuBufferView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyTextureToBuffer(WTFMove(*gpuTextureView), WTFMove(*gpuBufferView), size);
}

void WebGPUCommandEncoder::copyTextureToTexture(const WebGPUTextureCopyView& src, const WebGPUTextureCopyView& dst, const GPUExtent3D& size)
{
    if (!m_commandBuffer) {
        LOG(WebGPU, "WebGPUCommandEncoder::copyTextureToTexture(): Invalid operation!");
        return;
    }
    auto gpuSrcView = src.tryCreateGPUTextureCopyView();
    auto gpuDstView = dst.tryCreateGPUTextureCopyView();

    if (!gpuSrcView || !gpuDstView)
        return;

    // FIXME: Add Web GPU validation.

    m_commandBuffer->copyTextureToTexture(WTFMove(*gpuSrcView), WTFMove(*gpuDstView), size);
}
    
Ref<WebGPUCommandBuffer> WebGPUCommandEncoder::finish()
{
    if (!m_commandBuffer || m_commandBuffer->isEncodingPass()) {
        LOG(WebGPU, "WebGPUCommandEncoder::finish(): Invalid operation!");
        return WebGPUCommandBuffer::create(nullptr);
    }
    // Passes the referenced GPUCommandBuffer to the WebGPUCommandBuffer, invalidating this WebGPUCommandEncoder.
    return WebGPUCommandBuffer::create(m_commandBuffer.releaseNonNull());
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)

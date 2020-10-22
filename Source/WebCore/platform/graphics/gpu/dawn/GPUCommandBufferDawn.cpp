/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "GPUCommandBuffer.h"

#if ENABLE(WEBGPU)

namespace WebCore {

RefPtr<GPUCommandBuffer> GPUCommandBuffer::tryCreate(const GPUDevice& device)
{
    return nullptr;
}

GPUCommandBuffer::GPUCommandBuffer(PlatformCommandBufferSmartPtr&& buffer)
    : m_platformCommandBuffer(WTFMove(buffer))
{
}

void GPUCommandBuffer::copyBufferToBuffer(Ref<GPUBuffer>&& src, uint64_t srcOffset, Ref<GPUBuffer>&& dst, uint64_t dstOffset, uint64_t size)
{
}

void GPUCommandBuffer::copyBufferToTexture(GPUBufferCopyView&& srcBuffer, GPUTextureCopyView&& dstTexture, const GPUExtent3D& size)
{
}

void GPUCommandBuffer::copyTextureToBuffer(GPUTextureCopyView&& srcTexture, GPUBufferCopyView&& dstBuffer, const GPUExtent3D& size)
{
}

void GPUCommandBuffer::copyTextureToTexture(GPUTextureCopyView&& src, GPUTextureCopyView&& dst, const GPUExtent3D& size)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)

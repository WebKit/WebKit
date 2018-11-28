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

#include "GPUCommandBuffer.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPassEncoder.h"
#include "Logging.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPassEncoder.h"

namespace WebCore {

RefPtr<WebGPUCommandBuffer> WebGPUCommandBuffer::create(RefPtr<GPUCommandBuffer>&& buffer)
{
    if (!buffer)
        return nullptr;

    return adoptRef(new WebGPUCommandBuffer(buffer.releaseNonNull()));
}

WebGPUCommandBuffer::WebGPUCommandBuffer(Ref<GPUCommandBuffer>&& buffer)
    : m_commandBuffer(WTFMove(buffer))
{
}

RefPtr<WebGPURenderPassEncoder> WebGPUCommandBuffer::beginRenderPass(WebGPURenderPassDescriptor&& descriptor)
{
    // FIXME: Improve error checking as WebGPURenderPassDescriptor is implemented.
    if (!descriptor.attachment) {
        LOG(WebGPU, "WebGPUCommandBuffer::create(): No attachment specified for WebGPURenderPassDescriptor!");
        return nullptr;
    }
    
    auto encoder = GPURenderPassEncoder::create(m_commandBuffer.get(), GPURenderPassDescriptor { descriptor.attachment->texture() });

    if (!encoder)
        return nullptr;

    return WebGPURenderPassEncoder::create(encoder.releaseNonNull());
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)

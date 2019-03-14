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

#pragma once

#if ENABLE(WEBGPU)

#include "GPUCommandBuffer.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class WebGPUBuffer;
class WebGPURenderPassEncoder;
class WebGPUTexture;

struct GPUExtent3D;
struct WebGPURenderPassDescriptor;

struct WebGPUBufferCopyView : GPUBufferCopyViewBase {
    Optional<GPUBufferCopyView> tryCreateGPUBufferCopyView() const;

    RefPtr<WebGPUBuffer> buffer;
};

struct WebGPUTextureCopyView : GPUTextureCopyViewBase {
    Optional<GPUTextureCopyView> tryCreateGPUTextureCopyView() const;

    RefPtr<WebGPUTexture> texture;
};

class WebGPUCommandBuffer : public RefCounted<WebGPUCommandBuffer> {
public:
    static Ref<WebGPUCommandBuffer> create(Ref<GPUCommandBuffer>&&);

    GPUCommandBuffer& commandBuffer() const { return m_commandBuffer.get(); }

    RefPtr<WebGPURenderPassEncoder> beginRenderPass(WebGPURenderPassDescriptor&&);
    void copyBufferToBuffer(const WebGPUBuffer&, unsigned long srcOffset, const WebGPUBuffer&, unsigned long dstOffset, unsigned long size);
    void copyBufferToTexture(const WebGPUBufferCopyView&, const WebGPUTextureCopyView&, const GPUExtent3D&);
    void copyTextureToBuffer(const WebGPUTextureCopyView&, const WebGPUBufferCopyView&, const GPUExtent3D&);
    void copyTextureToTexture(const WebGPUTextureCopyView&, const WebGPUTextureCopyView&, const GPUExtent3D&);

private:
    WebGPUCommandBuffer(Ref<GPUCommandBuffer>&&);

    Ref<GPUCommandBuffer> m_commandBuffer;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

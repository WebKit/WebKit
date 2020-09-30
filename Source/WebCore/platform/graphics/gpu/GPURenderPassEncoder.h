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

#include "GPUProgrammablePassEncoder.h"
#include "GPURenderPipeline.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if USE(METAL)
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_PROTOCOL(MTLRenderCommandEncoder);
#endif

namespace WebCore {

class GPUBuffer;
class GPUCommandBuffer;

struct GPUColor;
struct GPURenderPassDescriptor;

#if USE(METAL)
using PlatformRenderPassEncoder = MTLRenderCommandEncoder;
using PlatformRenderPassEncoderSmartPtr = RetainPtr<MTLRenderCommandEncoder>;
#endif

class GPURenderPassEncoder : public GPUProgrammablePassEncoder {
public:
    static RefPtr<GPURenderPassEncoder> tryCreate(Ref<GPUCommandBuffer>&&, GPURenderPassDescriptor&&);

    void setPipeline(Ref<const GPURenderPipeline>&&);
    void setBlendColor(const GPUColor&);
    void setViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
    void setScissorRect(unsigned x, unsigned y, unsigned width, unsigned height);
    void setIndexBuffer(GPUBuffer&, uint64_t offset);
    void setVertexBuffers(unsigned index, const Vector<Ref<GPUBuffer>>&, const Vector<uint64_t>& offsets);
    void draw(unsigned vertexCount, unsigned instanceCount, unsigned firstVertex, unsigned firstInstance);
    void drawIndexed(unsigned indexCount, unsigned instanceCount, unsigned firstIndex, int baseVertex, unsigned firstInstance);

private:
    GPURenderPassEncoder(Ref<GPUCommandBuffer>&&, PlatformRenderPassEncoderSmartPtr&&);
    ~GPURenderPassEncoder() { endPass(); }

    // GPUProgrammablePassEncoder
    const PlatformProgrammablePassEncoder* platformPassEncoder() const final;
    void invalidateEncoder() final { m_platformRenderPassEncoder = nullptr; }
#if USE(METAL)
    void useResource(const MTLResource *, unsigned usage) final;
    void setVertexBuffer(const MTLBuffer *, NSUInteger offset, unsigned index) final;
    void setFragmentBuffer(const MTLBuffer *, NSUInteger offset, unsigned index) final;

    RefPtr<GPUBuffer> m_indexBuffer;
    uint64_t m_indexBufferOffset;
#endif // USE(METAL)

    PlatformRenderPassEncoderSmartPtr m_platformRenderPassEncoder;
    RefPtr<const GPURenderPipeline> m_pipeline;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

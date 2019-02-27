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
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_PROTOCOL(MTLRenderCommandEncoder);

namespace WebCore {

class GPUBuffer;
class GPUCommandBuffer;

struct GPURenderPassDescriptor;

using PlatformRenderPassEncoder = MTLRenderCommandEncoder;
using PlatformRenderPassEncoderSmartPtr = RetainPtr<MTLRenderCommandEncoder>;

class GPURenderPassEncoder : public GPUProgrammablePassEncoder {
public:
    static RefPtr<GPURenderPassEncoder> tryCreate(Ref<GPUCommandBuffer>&&, GPURenderPassDescriptor&&);

    void setPipeline(Ref<GPURenderPipeline>&&) final;

    void setVertexBuffers(unsigned long, Vector<Ref<GPUBuffer>>&&, Vector<unsigned long long>&&);
    void draw(unsigned long vertexCount, unsigned long instanceCount, unsigned long firstVertex, unsigned long firstInstance);

private:
    GPURenderPassEncoder(Ref<GPUCommandBuffer>&&, PlatformRenderPassEncoderSmartPtr&&);
    ~GPURenderPassEncoder() { endPass(); } // Ensure that encoding has ended before release.

    PlatformProgrammablePassEncoder* platformPassEncoder() const final;

#if USE(METAL)
    // GPUProgrammablePassEncoder
    void useResource(MTLResource *, unsigned long usage) final;
    void setVertexBuffer(MTLBuffer *, unsigned long offset, unsigned long index) final;
    void setFragmentBuffer(MTLBuffer *, unsigned long offset, unsigned long index) final;
#endif // USE(METAL)

    PlatformRenderPassEncoderSmartPtr m_platformRenderPassEncoder;
    RefPtr<GPURenderPipeline> m_pipeline;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

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

#include "GPUBindGroupBinding.h"
#include <wtf/RefCounted.h>

#if USE(METAL)
OBJC_PROTOCOL(MTLBuffer);
OBJC_PROTOCOL(MTLCommandEncoder);
OBJC_PROTOCOL(MTLResource);
#endif

namespace WebCore {

class GPUBindGroup;
class GPUCommandBuffer;
class GPURenderPipeline;

using PlatformProgrammablePassEncoder = MTLCommandEncoder;

class GPUProgrammablePassEncoder : public RefCounted<GPUProgrammablePassEncoder> {
public:
    virtual ~GPUProgrammablePassEncoder() = default;

    virtual void endPass();
    void setBindGroup(unsigned, GPUBindGroup&);
    virtual void setPipeline(Ref<GPURenderPipeline>&&) = 0;

protected:
    GPUProgrammablePassEncoder(Ref<GPUCommandBuffer>&&);

    GPUCommandBuffer& commandBuffer() const { return m_commandBuffer.get(); }
    virtual PlatformProgrammablePassEncoder* platformPassEncoder() const = 0;

private:
#if USE(METAL)
    virtual void useResource(MTLResource *, unsigned) = 0;

    // Render command encoder methods.
    virtual void setVertexBuffer(MTLBuffer *, unsigned, unsigned) { }
    virtual void setFragmentBuffer(MTLBuffer *, unsigned, unsigned) { }
#endif // USE(METAL)

    Ref<GPUCommandBuffer> m_commandBuffer;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

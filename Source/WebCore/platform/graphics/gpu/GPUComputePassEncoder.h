/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "GPUComputePipeline.h"
#include "GPUProgrammablePassEncoder.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(METAL)
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_PROTOCOL(MTLComputeCommandEncoder);
#endif

namespace WebCore {

#if USE(METAL)
using PlatformComputePassEncoder = MTLComputeCommandEncoder;
using PlatformComputePassEncoderSmartPtr = RetainPtr<MTLComputeCommandEncoder>;
#endif

class GPUComputePassEncoder : public GPUProgrammablePassEncoder {
public:
    static RefPtr<GPUComputePassEncoder> tryCreate(Ref<GPUCommandBuffer>&&);

    void setPipeline(Ref<const GPUComputePipeline>&&);
    void dispatch(unsigned x, unsigned y, unsigned z);

private:
    GPUComputePassEncoder(Ref<GPUCommandBuffer>&&, PlatformComputePassEncoderSmartPtr&&);
    ~GPUComputePassEncoder() { endPass(); }

    // GPUProgrammablePassEncoder
    const PlatformProgrammablePassEncoder* platformPassEncoder() const final;
    void invalidateEncoder() final { m_platformComputePassEncoder = nullptr; }
#if USE(METAL)
    void useResource(const MTLResource *, unsigned usage) final;
    void setComputeBuffer(const MTLBuffer *, NSUInteger offset, unsigned index) final;
#endif

    PlatformComputePassEncoderSmartPtr m_platformComputePassEncoder;
    RefPtr<const GPUComputePipeline> m_pipeline;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

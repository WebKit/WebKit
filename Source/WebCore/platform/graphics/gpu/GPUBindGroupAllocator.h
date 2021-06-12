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

#include "GPUBuffer.h"
#include "GPUPlatformTypes.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class GPUErrorScopes;

class GPUBindGroupAllocator : public RefCounted<GPUBindGroupAllocator> {
public:
    static Ref<GPUBindGroupAllocator> create(GPUErrorScopes&);

    struct ArgumentBufferOffsets {
        std::optional<PlatformGPUBufferOffset> vertex;
        std::optional<PlatformGPUBufferOffset> fragment;
        std::optional<PlatformGPUBufferOffset> compute;
    };

#if USE(METAL)
    std::optional<ArgumentBufferOffsets> allocateAndSetEncoders(MTLArgumentEncoder *vertex, MTLArgumentEncoder *fragment, MTLArgumentEncoder *compute);
#endif

    void tryReset();

    const PlatformBuffer* argumentBuffer() const { return m_argumentBuffer.get(); }

private:
    explicit GPUBindGroupAllocator(GPUErrorScopes&);

    bool reallocate(PlatformGPUBufferOffset);

    PlatformBufferSmartPtr m_argumentBuffer;
    PlatformGPUBufferOffset m_lastOffset { 0 };

    Ref<GPUErrorScopes> m_errorScopes;
    int m_numBindGroups { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

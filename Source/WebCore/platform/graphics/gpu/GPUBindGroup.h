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

#include "GPUBindGroupAllocator.h"
#include "GPUBuffer.h"
#include "GPUTexture.h"
#include <utility>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(METAL)
#include <objc/NSObjCRuntime.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_PROTOCOL(MTLBuffer);
#endif

namespace WebCore {

struct GPUBindGroupDescriptor;

#if USE(METAL)
using ArgumentBuffer = std::pair<const MTLBuffer *, const GPUBindGroupAllocator::ArgumentBufferOffsets&>;
#endif

class GPUBindGroup : public RefCounted<GPUBindGroup> {
public:
    static RefPtr<GPUBindGroup> tryCreate(const GPUBindGroupDescriptor&, GPUBindGroupAllocator&);

    ~GPUBindGroup();
    
#if USE(METAL)
    const ArgumentBuffer argumentBuffer() const { return { m_allocator->argumentBuffer(), m_argumentBufferOffsets }; }
#endif
    const HashSet<Ref<GPUBuffer>>& boundBuffers() const { return m_boundBuffers; }
    const HashSet<Ref<GPUTexture>>& boundTextures() const { return m_boundTextures; }

private:
#if USE(METAL)
    GPUBindGroup(GPUBindGroupAllocator::ArgumentBufferOffsets&&, GPUBindGroupAllocator&, HashSet<Ref<GPUBuffer>>&&, HashSet<Ref<GPUTexture>>&&);
    
    GPUBindGroupAllocator::ArgumentBufferOffsets m_argumentBufferOffsets;
    Ref<GPUBindGroupAllocator> m_allocator;
#endif
    HashSet<Ref<GPUBuffer>> m_boundBuffers;
    HashSet<Ref<GPUTexture>> m_boundTextures;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)

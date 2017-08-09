/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yuichiro Kikura (y.kikura@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGPU)

#include "DOMPromiseProxy.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS MTLCommandBuffer;
#endif

namespace WebCore {

class GPUCommandQueue;
class GPUComputeCommandEncoder;
class GPUDrawable;
class GPURenderCommandEncoder;
class GPURenderPassDescriptor;

class GPUCommandBuffer : public RefCounted<GPUCommandBuffer> {
public:
    static RefPtr<GPUCommandBuffer> create(GPUCommandQueue*);
    WEBCORE_EXPORT ~GPUCommandBuffer();

    WEBCORE_EXPORT void commit();
    WEBCORE_EXPORT void presentDrawable(GPUDrawable*);

    WEBCORE_EXPORT RefPtr<GPURenderCommandEncoder> createRenderCommandEncoder(GPURenderPassDescriptor*);
    WEBCORE_EXPORT RefPtr<GPUComputeCommandEncoder> createComputeCommandEncoder();
    WEBCORE_EXPORT DOMPromiseProxy<IDLVoid>& completed();

#if PLATFORM(COCOA)
    WEBCORE_EXPORT MTLCommandBuffer *platformCommandBuffer();
#endif

private:
    GPUCommandBuffer(GPUCommandQueue*);
    
#if PLATFORM(COCOA)
    RetainPtr<MTLCommandBuffer> m_commandBuffer;
#endif

    DOMPromiseProxy<IDLVoid> m_completedPromise;
};
    
} // namespace WebCore
#endif

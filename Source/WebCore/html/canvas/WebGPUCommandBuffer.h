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
#include "GPUCommandBuffer.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebGPUComputeCommandEncoder;
class WebGPUDrawable;
class WebGPURenderCommandEncoder;
class WebGPURenderPassDescriptor;

class WebGPUCommandBuffer : public RefCounted<WebGPUCommandBuffer> {
public:
    ~WebGPUCommandBuffer();
    static Ref<WebGPUCommandBuffer> create(const GPUCommandQueue&);

    void commit();
    void presentDrawable(WebGPUDrawable&);

    Ref<WebGPURenderCommandEncoder> createRenderCommandEncoderWithDescriptor(WebGPURenderPassDescriptor&);
    Ref<WebGPUComputeCommandEncoder> createComputeCommandEncoder();

    DOMPromiseProxy<IDLVoid>& completed();

    const GPUCommandBuffer& buffer() const { return m_buffer; }

private:
    explicit WebGPUCommandBuffer(const GPUCommandQueue&);

    GPUCommandBuffer m_buffer;
    DOMPromiseProxy<IDLVoid> m_completed;
};

} // namespace WebCore

#endif

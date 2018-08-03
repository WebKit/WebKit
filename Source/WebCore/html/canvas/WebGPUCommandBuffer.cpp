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

#include "config.h"
#include "WebGPUCommandBuffer.h"

#if ENABLE(WEBGPU)

#include "GPUCommandBuffer.h"
#include "GPUCommandQueue.h"
#include "Logging.h"
#include "WebGPUComputeCommandEncoder.h"
#include "WebGPUDrawable.h"
#include "WebGPURenderCommandEncoder.h"
#include "WebGPURenderPassDescriptor.h"

namespace WebCore {

Ref<WebGPUCommandBuffer> WebGPUCommandBuffer::create(const GPUCommandQueue& queue)
{
    return adoptRef(*new WebGPUCommandBuffer(queue));
}

WebGPUCommandBuffer::WebGPUCommandBuffer(const GPUCommandQueue& queue)
    : m_buffer { queue, [this] () { m_completed.resolve(); } }
{
    LOG(WebGPU, "WebGPUCommandBuffer::WebGPUCommandBuffer()");
}

WebGPUCommandBuffer::~WebGPUCommandBuffer()
{
    LOG(WebGPU, "WebGPUCommandBuffer::~WebGPUCommandBuffer()");
}

void WebGPUCommandBuffer::commit()
{
    LOG(WebGPU, "WebGPUCommandBuffer::commit()");
    m_buffer.commit();
}

void WebGPUCommandBuffer::presentDrawable(WebGPUDrawable& drawable)
{
    LOG(WebGPU, "WebGPUCommandBuffer::presentDrawable()");
    m_buffer.presentDrawable(drawable.drawable());
}

Ref<WebGPURenderCommandEncoder> WebGPUCommandBuffer::createRenderCommandEncoderWithDescriptor(WebGPURenderPassDescriptor& descriptor)
{
    return WebGPURenderCommandEncoder::create(GPURenderCommandEncoder { m_buffer, descriptor.descriptor() });
}

Ref<WebGPUComputeCommandEncoder> WebGPUCommandBuffer::createComputeCommandEncoder()
{
    return WebGPUComputeCommandEncoder::create(GPUComputeCommandEncoder { m_buffer });
}

DOMPromiseProxy<IDLVoid>& WebGPUCommandBuffer::completed()
{
    return m_completed;
}

} // namespace WebCore

#endif

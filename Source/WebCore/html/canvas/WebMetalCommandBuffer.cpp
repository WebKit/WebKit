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
#include "WebMetalCommandBuffer.h"

#if ENABLE(WEBMETAL)

#include "GPULegacyCommandBuffer.h"
#include "GPULegacyCommandQueue.h"
#include "Logging.h"
#include "WebMetalComputeCommandEncoder.h"
#include "WebMetalDrawable.h"
#include "WebMetalRenderCommandEncoder.h"
#include "WebMetalRenderPassDescriptor.h"

namespace WebCore {

Ref<WebMetalCommandBuffer> WebMetalCommandBuffer::create(const GPULegacyCommandQueue& queue)
{
    return adoptRef(*new WebMetalCommandBuffer(queue));
}

WebMetalCommandBuffer::WebMetalCommandBuffer(const GPULegacyCommandQueue& queue)
    : m_buffer { queue, [this] () { m_completed.resolve(); } }
{
    LOG(WebMetal, "WebMetalCommandBuffer::WebMetalCommandBuffer()");
}

WebMetalCommandBuffer::~WebMetalCommandBuffer()
{
    LOG(WebMetal, "WebMetalCommandBuffer::~WebMetalCommandBuffer()");
}

void WebMetalCommandBuffer::commit()
{
    LOG(WebMetal, "WebMetalCommandBuffer::commit()");
    m_buffer.commit();
}

void WebMetalCommandBuffer::presentDrawable(WebMetalDrawable& drawable)
{
    LOG(WebMetal, "WebMetalCommandBuffer::presentDrawable()");
    m_buffer.presentDrawable(drawable.drawable());
}

Ref<WebMetalRenderCommandEncoder> WebMetalCommandBuffer::createRenderCommandEncoderWithDescriptor(WebMetalRenderPassDescriptor& descriptor)
{
    return WebMetalRenderCommandEncoder::create(GPULegacyRenderCommandEncoder { m_buffer, descriptor.descriptor() });
}

Ref<WebMetalComputeCommandEncoder> WebMetalCommandBuffer::createComputeCommandEncoder()
{
    return WebMetalComputeCommandEncoder::create(GPULegacyComputeCommandEncoder { m_buffer });
}

DOMPromiseProxy<IDLVoid>& WebMetalCommandBuffer::completed()
{
    return m_completed;
}

} // namespace WebCore

#endif

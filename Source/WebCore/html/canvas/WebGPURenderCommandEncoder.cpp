/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebGPURenderCommandEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUCommandBuffer.h"
#include "GPURenderPassDescriptor.h"
#include "WebGPUBuffer.h"
#include "WebGPUCommandBuffer.h"
#include "WebGPUDepthStencilState.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPipelineState.h"

namespace WebCore {

Ref<WebGPURenderCommandEncoder> WebGPURenderCommandEncoder::create(GPURenderCommandEncoder&& encoder)
{
    return adoptRef(*new WebGPURenderCommandEncoder(WTFMove(encoder)));
}

WebGPURenderCommandEncoder::WebGPURenderCommandEncoder(GPURenderCommandEncoder&& encoder)
    : m_encoder { WTFMove(encoder) }
{
}

WebGPURenderCommandEncoder::~WebGPURenderCommandEncoder() = default;

void WebGPURenderCommandEncoder::setRenderPipelineState(WebGPURenderPipelineState& pipelineState)
{
    m_encoder.setRenderPipelineState(pipelineState.state());
}

void WebGPURenderCommandEncoder::setDepthStencilState(WebGPUDepthStencilState& depthStencilState)
{
    m_encoder.setDepthStencilState(depthStencilState.state());
}

void WebGPURenderCommandEncoder::setVertexBuffer(WebGPUBuffer& buffer, unsigned offset, unsigned index)
{
    m_encoder.setVertexBuffer(buffer.buffer(), offset, index);
}

void WebGPURenderCommandEncoder::setFragmentBuffer(WebGPUBuffer& buffer, unsigned offset, unsigned index)
{
    m_encoder.setFragmentBuffer(buffer.buffer(), offset, index);
}

void WebGPURenderCommandEncoder::drawPrimitives(unsigned type, unsigned start, unsigned count)
{
    m_encoder.drawPrimitives(type, start, count);
}

void WebGPURenderCommandEncoder::endEncoding()
{
    return m_encoder.endEncoding();
}

} // namespace WebCore

#endif

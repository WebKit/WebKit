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
#include "WebMetalRenderCommandEncoder.h"

#if ENABLE(WEBMETAL)

#include "GPULegacyCommandBuffer.h"
#include "GPULegacyRenderPassDescriptor.h"
#include "WebMetalBuffer.h"
#include "WebMetalCommandBuffer.h"
#include "WebMetalDepthStencilState.h"
#include "WebMetalRenderPassDescriptor.h"
#include "WebMetalRenderPipelineState.h"

namespace WebCore {

Ref<WebMetalRenderCommandEncoder> WebMetalRenderCommandEncoder::create(GPULegacyRenderCommandEncoder&& encoder)
{
    return adoptRef(*new WebMetalRenderCommandEncoder(WTFMove(encoder)));
}

WebMetalRenderCommandEncoder::WebMetalRenderCommandEncoder(GPULegacyRenderCommandEncoder&& encoder)
    : m_encoder { WTFMove(encoder) }
{
}

WebMetalRenderCommandEncoder::~WebMetalRenderCommandEncoder() = default;

void WebMetalRenderCommandEncoder::setRenderPipelineState(WebMetalRenderPipelineState& pipelineState)
{
    m_encoder.setRenderPipelineState(pipelineState.state());
}

void WebMetalRenderCommandEncoder::setDepthStencilState(WebMetalDepthStencilState& depthStencilState)
{
    m_encoder.setDepthStencilState(depthStencilState.state());
}

void WebMetalRenderCommandEncoder::setVertexBuffer(WebMetalBuffer& buffer, unsigned offset, unsigned index)
{
    m_encoder.setVertexBuffer(buffer.buffer(), offset, index);
}

void WebMetalRenderCommandEncoder::setFragmentBuffer(WebMetalBuffer& buffer, unsigned offset, unsigned index)
{
    m_encoder.setFragmentBuffer(buffer.buffer(), offset, index);
}

void WebMetalRenderCommandEncoder::drawPrimitives(unsigned type, unsigned start, unsigned count)
{
    m_encoder.drawPrimitives(type, start, count);
}

void WebMetalRenderCommandEncoder::endEncoding()
{
    return m_encoder.endEncoding();
}

} // namespace WebCore

#endif

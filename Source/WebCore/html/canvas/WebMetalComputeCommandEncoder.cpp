/*
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
#include "WebMetalComputeCommandEncoder.h"

#if ENABLE(WEBMETAL)

#include "GPUCommandBuffer.h"
#include "GPUComputeCommandEncoder.h"
#include "GPUSize.h"
#include "WebMetalBuffer.h"
#include "WebMetalCommandBuffer.h"
#include "WebMetalComputePipelineState.h"

namespace WebCore {

static inline GPUSize GPUSizeMake(WebMetalSize size)
{
    return { size.width, size.height, size.depth };
}

Ref<WebMetalComputeCommandEncoder> WebMetalComputeCommandEncoder::create(GPUComputeCommandEncoder&& encoder)
{
    return adoptRef(*new WebMetalComputeCommandEncoder(WTFMove(encoder)));
}
    
WebMetalComputeCommandEncoder::WebMetalComputeCommandEncoder(GPUComputeCommandEncoder&& encoder)
    : m_encoder { WTFMove(encoder) }
{
}

void WebMetalComputeCommandEncoder::setComputePipelineState(WebMetalComputePipelineState& pipelineState)
{
    m_encoder.setComputePipelineState(pipelineState.state());
}

void WebMetalComputeCommandEncoder::setBuffer(WebMetalBuffer& buffer, unsigned offset, unsigned index)
{
    m_encoder.setBuffer(buffer.buffer(), offset, index);
}

void WebMetalComputeCommandEncoder::dispatch(WebMetalSize threadgroupsPerGrid, WebMetalSize threadsPerThreadgroup)
{
    m_encoder.dispatch(GPUSizeMake(threadgroupsPerGrid), GPUSizeMake(threadsPerThreadgroup));
}

void WebMetalComputeCommandEncoder::endEncoding()
{
    m_encoder.endEncoding();
}

} // namespace WebCore

#endif

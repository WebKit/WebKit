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
#include "WebGPUComputeCommandEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUCommandBuffer.h"
#include "GPUComputeCommandEncoder.h"
#include "GPUSize.h"
#include "WebGPUBuffer.h"
#include "WebGPUCommandBuffer.h"
#include "WebGPUComputePipelineState.h"
#include "WebGPURenderingContext.h"

namespace WebCore {
    
inline GPUSize GPUSizeMake(WebGPUSize size)
{
    return {size.width, size.height, size.depth};
}

Ref<WebGPUComputeCommandEncoder> WebGPUComputeCommandEncoder::create(WebGPURenderingContext* context, WebGPUCommandBuffer* buffer)
{
    return adoptRef(*new WebGPUComputeCommandEncoder(context, buffer));
}
    
WebGPUComputeCommandEncoder::WebGPUComputeCommandEncoder(WebGPURenderingContext* context, WebGPUCommandBuffer* buffer)
    : WebGPUObject(context)
{
    m_computeCommandEncoder = buffer->commandBuffer()->createComputeCommandEncoder();
}
    
WebGPUComputeCommandEncoder::~WebGPUComputeCommandEncoder() = default;
    
void WebGPUComputeCommandEncoder::setComputePipelineState(WebGPUComputePipelineState& pipelineState)
{
    if (!m_computeCommandEncoder)
        return;
    m_computeCommandEncoder->setComputePipelineState(pipelineState.computePipelineState());
}
    
void WebGPUComputeCommandEncoder::setBuffer(WebGPUBuffer& buffer, unsigned offset, unsigned index)
{
    if (!m_computeCommandEncoder)
        return;
    m_computeCommandEncoder->setBuffer(buffer.buffer(), offset, index);
}
    
void WebGPUComputeCommandEncoder::dispatch(WebGPUSize threadgroupsPerGrid, WebGPUSize threadsPerThreadgroup)
{
    if (!m_computeCommandEncoder)
        return;
    m_computeCommandEncoder->dispatch(GPUSizeMake(threadgroupsPerGrid), GPUSizeMake(threadsPerThreadgroup));
}
    
void WebGPUComputeCommandEncoder::endEncoding()
{
    if (!m_computeCommandEncoder)
        return;
    m_computeCommandEncoder->endEncoding();
}
    
} // namespace WebCore

#endif

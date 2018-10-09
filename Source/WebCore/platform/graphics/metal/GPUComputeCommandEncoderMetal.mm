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

#import "config.h"
#import "GPUComputeCommandEncoder.h"

#if ENABLE(WEBMETAL)

#import "GPUBuffer.h"
#import "GPUCommandBuffer.h"
#import "GPUComputePipelineState.h"
#import "GPUSize.h"
#import "Logging.h"
#import <Metal/Metal.h>

namespace WebCore {

static inline MTLSize MTLSizeMake(GPUSize size)
{
    return { size.width, size.height, size.depth };
}

GPUComputeCommandEncoder::GPUComputeCommandEncoder(const GPUCommandBuffer& buffer)
    : m_metal { [buffer.metal() computeCommandEncoder] }
{
    LOG(WebMetal, "GPUComputeCommandEncoder::GPUComputeCommandEncoder()");
}

void GPUComputeCommandEncoder::setComputePipelineState(const GPUComputePipelineState& computePipelineState) const
{
    if (!computePipelineState.metal())
        return;

    [m_metal setComputePipelineState:computePipelineState.metal()];
}
    
void GPUComputeCommandEncoder::setBuffer(const GPUBuffer& buffer, unsigned offset, unsigned index) const
{
    if (!buffer.metal())
        return;

    [m_metal setBuffer:buffer.metal() offset:offset atIndex:index];
}
    
void GPUComputeCommandEncoder::dispatch(GPUSize threadgroupsPerGrid, GPUSize threadsPerThreadgroup) const
{
    [m_metal dispatchThreadgroups:MTLSizeMake(threadgroupsPerGrid) threadsPerThreadgroup:MTLSizeMake(threadsPerThreadgroup)];
}

void GPUComputeCommandEncoder::endEncoding() const
{
    [m_metal endEncoding];
}

} // namespace WebCore

#endif

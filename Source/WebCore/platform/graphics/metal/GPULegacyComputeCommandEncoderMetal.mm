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
#import "GPULegacyComputeCommandEncoder.h"

#if ENABLE(WEBMETAL)

#import "GPULegacyBuffer.h"
#import "GPULegacyCommandBuffer.h"
#import "GPULegacyComputePipelineState.h"
#import "GPULegacySize.h"
#import "Logging.h"
#import <Metal/Metal.h>

namespace WebCore {

static inline MTLSize MTLSizeMake(GPULegacySize size)
{
    return { size.width, size.height, size.depth };
}

GPULegacyComputeCommandEncoder::GPULegacyComputeCommandEncoder(const GPULegacyCommandBuffer& buffer)
    : m_metal { [buffer.metal() computeCommandEncoder] }
{
    LOG(WebMetal, "GPULegacyComputeCommandEncoder::GPULegacyComputeCommandEncoder()");
}

void GPULegacyComputeCommandEncoder::setComputePipelineState(const GPULegacyComputePipelineState& computePipelineState) const
{
    if (!computePipelineState.metal())
        return;

    [m_metal setComputePipelineState:computePipelineState.metal()];
}
    
void GPULegacyComputeCommandEncoder::setBuffer(const GPULegacyBuffer& buffer, unsigned offset, unsigned index) const
{
    if (!buffer.metal())
        return;

    [m_metal setBuffer:buffer.metal() offset:offset atIndex:index];
}
    
void GPULegacyComputeCommandEncoder::dispatch(GPULegacySize threadgroupsPerGrid, GPULegacySize threadsPerThreadgroup) const
{
    [m_metal dispatchThreadgroups:MTLSizeMake(threadgroupsPerGrid) threadsPerThreadgroup:MTLSizeMake(threadsPerThreadgroup)];
}

void GPULegacyComputeCommandEncoder::endEncoding() const
{
    [m_metal endEncoding];
}

} // namespace WebCore

#endif

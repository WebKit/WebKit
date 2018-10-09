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

#import "config.h"
#import "GPURenderCommandEncoder.h"

#if ENABLE(WEBMETAL)

#import "GPUBuffer.h"
#import "GPUCommandBuffer.h"
#import "GPUDepthStencilState.h"
#import "GPURenderPassDescriptor.h"
#import "GPURenderPipelineState.h"
#import "Logging.h"
#import <Metal/Metal.h>

namespace WebCore {

GPURenderCommandEncoder::GPURenderCommandEncoder(const GPUCommandBuffer& buffer, const GPURenderPassDescriptor& descriptor)
{
    LOG(WebMetal, "GPURenderCommandEncoder::GPURenderCommandEncoder()");

    if (!descriptor.metal())
        return;

    m_metal = [buffer.metal() renderCommandEncoderWithDescriptor:descriptor.metal()];
}

void GPURenderCommandEncoder::setRenderPipelineState(const GPURenderPipelineState& renderPipelineState) const
{
    if (!renderPipelineState.metal())
        return;

    [m_metal setRenderPipelineState:renderPipelineState.metal()];
}

void GPURenderCommandEncoder::setDepthStencilState(const GPUDepthStencilState& depthStencilState) const
{
    if (!depthStencilState.metal())
        return;

    [m_metal setDepthStencilState:depthStencilState.metal()];
}

void GPURenderCommandEncoder::setVertexBuffer(const GPUBuffer& buffer, unsigned offset, unsigned index) const
{
    if (!buffer.metal())
        return;

    [m_metal setVertexBuffer:buffer.metal() offset:offset atIndex:index];
}

void GPURenderCommandEncoder::setFragmentBuffer(const GPUBuffer& buffer, unsigned offset, unsigned index) const
{
    if (!buffer.metal())
        return;

    [m_metal setFragmentBuffer:buffer.metal() offset:offset atIndex:index];
}

void GPURenderCommandEncoder::drawPrimitives(unsigned type, unsigned start, unsigned count) const
{
    // FIXME: Why do we need a specially optimized path for count of 0?
    if (!count)
        return;

    [m_metal drawPrimitives:static_cast<MTLPrimitiveType>(type) vertexStart:start vertexCount:count];
}

void GPURenderCommandEncoder::endEncoding() const
{
    [m_metal endEncoding];
}

} // namespace WebCore

#endif

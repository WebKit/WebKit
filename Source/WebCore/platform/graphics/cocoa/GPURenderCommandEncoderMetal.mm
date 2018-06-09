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

#if ENABLE(WEBGPU)

#import "GPUBuffer.h"
#import "GPUCommandBuffer.h"
#import "GPUDepthStencilState.h"
#import "GPURenderPassDescriptor.h"
#import "GPURenderPipelineState.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPURenderCommandEncoder::GPURenderCommandEncoder(GPUCommandBuffer* buffer, GPURenderPassDescriptor* descriptor)
{
    LOG(WebGPU, "GPURenderCommandEncoder::GPURenderCommandEncoder()");

    if (!buffer || !buffer->platformCommandBuffer() || !descriptor || !descriptor->platformRenderPassDescriptor())
        return;

    m_renderCommandEncoder = (MTLRenderCommandEncoder *)[buffer->platformCommandBuffer() renderCommandEncoderWithDescriptor:descriptor->platformRenderPassDescriptor()];
}

void GPURenderCommandEncoder::setRenderPipelineState(GPURenderPipelineState* renderPipelineState)
{
    if (!m_renderCommandEncoder || !renderPipelineState)
        return;

    // We need to cast to MTLRenderCommandEncoder explicitly because the compiler gets
    // confused by a protocol with a similar signature.
    [(id<MTLRenderCommandEncoder>)m_renderCommandEncoder.get() setRenderPipelineState:static_cast<id<MTLRenderPipelineState>>(renderPipelineState->platformRenderPipelineState())];
}

void GPURenderCommandEncoder::setDepthStencilState(GPUDepthStencilState* depthStencilState)
{
    if (!m_renderCommandEncoder || !depthStencilState)
        return;

    [m_renderCommandEncoder setDepthStencilState:static_cast<id<MTLDepthStencilState>>(depthStencilState->platformDepthStencilState())];
}

void GPURenderCommandEncoder::setVertexBuffer(GPUBuffer* buffer, unsigned offset, unsigned index)
{
    if (!m_renderCommandEncoder || !buffer)
        return;

    [m_renderCommandEncoder setVertexBuffer:static_cast<id<MTLBuffer>>(buffer->platformBuffer()) offset:offset atIndex:index];
}

void GPURenderCommandEncoder::setFragmentBuffer(GPUBuffer* buffer, unsigned offset, unsigned index)
{
    if (!m_renderCommandEncoder || !buffer)
        return;

    [m_renderCommandEncoder setFragmentBuffer:static_cast<id<MTLBuffer>>(buffer->platformBuffer()) offset:offset atIndex:index];
}

void GPURenderCommandEncoder::drawPrimitives(unsigned type, unsigned start, unsigned count)
{
    if (!m_renderCommandEncoder || !count)
        return;

    [m_renderCommandEncoder drawPrimitives:static_cast<MTLPrimitiveType>(type) vertexStart:start vertexCount:count];
}

void GPURenderCommandEncoder::endEncoding()
{
    [m_renderCommandEncoder endEncoding];
}

MTLRenderCommandEncoder *GPURenderCommandEncoder::platformRenderCommandEncoder()
{
    return m_renderCommandEncoder.get();
}

} // namespace WebCore

#endif

/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "GPURenderPassEncoder.h"

#if ENABLE(WEBGPU)

#import "GPUBuffer.h"
#import "GPUCommandBuffer.h"
#import "GPURenderPassDescriptor.h"
#import "GPURenderPipeline.h"
#import "Logging.h"
#import "WHLSLVertexBufferIndexCalculator.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPURenderPassEncoder> GPURenderPassEncoder::create(const GPUCommandBuffer& buffer, GPURenderPassDescriptor&& descriptor)
{
    PlatformRenderPassEncoderSmartPtr mtlEncoder;

    // FIXME: Default to colorAttachments[0] and this loadOp, storeOp for now.
    const auto& attachmentDescriptor = descriptor.colorAttachments[0];
    const auto& color = attachmentDescriptor.clearColor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto mtlDescriptor = adoptNS([MTLRenderPassDescriptor new]);

    mtlDescriptor.get().colorAttachments[0].texture = attachmentDescriptor.attachment->platformTexture();
    mtlDescriptor.get().colorAttachments[0].loadAction = MTLLoadActionClear;
    mtlDescriptor.get().colorAttachments[0].storeAction = MTLStoreActionStore;
    mtlDescriptor.get().colorAttachments[0].clearColor = MTLClearColorMake(color.r, color.g, color.b, color.a);

    mtlEncoder = retainPtr([buffer.platformCommandBuffer() renderCommandEncoderWithDescriptor:mtlDescriptor.get()]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::create(): Unable to create MTLRenderCommandEncoder!");
        return nullptr;
    }

    return adoptRef(new GPURenderPassEncoder(WTFMove(mtlEncoder)));
}

GPURenderPassEncoder::GPURenderPassEncoder(PlatformRenderPassEncoderSmartPtr&& encoder)
    : m_platformRenderPassEncoder(WTFMove(encoder))
{
}

PlatformProgrammablePassEncoder *GPURenderPassEncoder::platformPassEncoder() const
{
    return m_platformRenderPassEncoder.get();
}

void GPURenderPassEncoder::setPipeline(Ref<GPURenderPipeline>&& pipeline)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (pipeline->depthStencilState())
        [m_platformRenderPassEncoder setDepthStencilState:pipeline->depthStencilState()];

    [m_platformRenderPassEncoder setRenderPipelineState:pipeline->platformRenderPipeline()];

    END_BLOCK_OBJC_EXCEPTIONS;

    m_pipeline = WTFMove(pipeline);
}

void GPURenderPassEncoder::setVertexBuffers(unsigned long index, Vector<Ref<const GPUBuffer>>&& buffers, Vector<unsigned long long>&& offsets)
{
    ASSERT(buffers.size() && offsets.size() == buffers.size());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto mtlBuffers = buffers.map([] (const auto& buffer) {
        return buffer->platformBuffer();
    });

    auto indexRanges = NSMakeRange(WHLSL::Metal::calculateVertexBufferIndex(index), buffers.size());

    [m_platformRenderPassEncoder setVertexBuffers:mtlBuffers.data() offsets:(const NSUInteger *)offsets.data() withRange:indexRanges];

    END_BLOCK_OBJC_EXCEPTIONS;
}

static MTLPrimitiveType primitiveTypeForGPUPrimitiveTopology(PrimitiveTopology type)
{
    switch (type) {
    case PrimitiveTopology::PointList:
        return MTLPrimitiveTypePoint;
    case PrimitiveTopology::LineList:
        return MTLPrimitiveTypeLine;
    case PrimitiveTopology::LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case PrimitiveTopology::TriangleList:
        return MTLPrimitiveTypeTriangle;
    case PrimitiveTopology::TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    }
}

void GPURenderPassEncoder::draw(unsigned long vertexCount, unsigned long instanceCount, unsigned long firstVertex, unsigned long firstInstance)
{
    [m_platformRenderPassEncoder 
        drawPrimitives:primitiveTypeForGPUPrimitiveTopology(m_pipeline->primitiveTopology())
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

#if USE(METAL)

void GPURenderPassEncoder::useResource(MTLResource *resource, unsigned long usage)
{
    [m_platformRenderPassEncoder useResource:resource usage:usage];
}

void GPURenderPassEncoder::setVertexBuffer(MTLBuffer *buffer, unsigned long offset, unsigned long index)
{
    [m_platformRenderPassEncoder setVertexBuffer:buffer offset:offset atIndex:index];
}

void GPURenderPassEncoder::setFragmentBuffer(MTLBuffer *buffer, unsigned long offset, unsigned long index)
{
    [m_platformRenderPassEncoder setFragmentBuffer:buffer offset:offset atIndex:index];
}

#endif // USE(METAL)

} // namespace WebCore

#endif // ENABLE(WEBGPU)

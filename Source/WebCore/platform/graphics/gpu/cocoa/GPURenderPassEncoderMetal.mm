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

#import "GPUCommandBuffer.h"
#import "GPURenderPassDescriptor.h"
#import "GPURenderPipeline.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPURenderPassEncoder> GPURenderPassEncoder::create(const GPUCommandBuffer& buffer, GPURenderPassDescriptor&& descriptor)
{
    PlatformRenderPassEncoderSmartPtr mtlEncoder;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto mtlDescriptor = adoptNS([MTLRenderPassDescriptor new]);

    // FIXME: Default to colorAttachments[0] and this loadOp, storeOp, clearColor for now.
    mtlDescriptor.get().colorAttachments[0].texture = descriptor.attachment->platformTexture();
    mtlDescriptor.get().colorAttachments[0].loadAction = MTLLoadActionClear;
    mtlDescriptor.get().colorAttachments[0].storeAction = MTLStoreActionStore;
    mtlDescriptor.get().colorAttachments[0].clearColor = MTLClearColorMake(0.35, 0.65, 0.85, 1.0);

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
    [m_platformRenderPassEncoder setRenderPipelineState:pipeline->platformRenderPipeline()];
    m_pipeline = WTFMove(pipeline);
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

} // namespace WebCore

#endif // ENABLE(WEBGPU)

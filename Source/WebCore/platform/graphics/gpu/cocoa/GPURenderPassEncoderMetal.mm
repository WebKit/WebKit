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

static MTLLoadAction loadActionForGPULoadOp(GPULoadOp op)
{
    switch (op) {
    case GPULoadOp::Clear:
        return MTLLoadActionClear;
    case GPULoadOp::Load:
        return MTLLoadActionLoad;
    }

    ASSERT_NOT_REACHED();
}

static MTLStoreAction storeActionForGPUStoreOp(GPUStoreOp op)
{
    switch (op) {
    case GPUStoreOp::Store:
        return MTLStoreActionStore;
    }

    ASSERT_NOT_REACHED();
}

static bool populateMtlColorAttachmentsArray(MTLRenderPassColorAttachmentDescriptorArray *array, const Vector<GPURenderPassColorAttachmentDescriptor>& descriptors, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

    for (unsigned i = 0; i < descriptors.size(); ++i) {
        const auto& descriptor = descriptors[i];
        if (!descriptor.attachment->platformTexture()) {
            LOG(WebGPU, "%s: Invalid MTLTexture for color attachment %u!", functionName, i);
            return false;
        }
        const auto& color = descriptor.clearColor;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;

        auto mtlAttachment = retainPtr([array objectAtIndexedSubscript:i]);
        [mtlAttachment setTexture:descriptor.attachment->platformTexture()];
        [mtlAttachment setClearColor:MTLClearColorMake(color.r, color.g, color.b, color.a)];
        [mtlAttachment setLoadAction:loadActionForGPULoadOp(descriptor.loadOp)];
        [mtlAttachment setStoreAction:storeActionForGPUStoreOp(descriptor.storeOp)];

        END_BLOCK_OBJC_EXCEPTIONS;
    }

    return true;
}

static bool populateMtlDepthStencilAttachment(MTLRenderPassDepthAttachmentDescriptor *mtlAttachment, const GPURenderPassDepthStencilAttachmentDescriptor& descriptor, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

    if (!descriptor.attachment->platformTexture()) {
        LOG(WebGPU, "%s: Invalid MTLTexture for depth attachment!", functionName);
        return false;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [mtlAttachment setTexture:descriptor.attachment->platformTexture()];
    [mtlAttachment setClearDepth:descriptor.clearDepth];
    [mtlAttachment setLoadAction:loadActionForGPULoadOp(descriptor.depthLoadOp)];
    [mtlAttachment setStoreAction:storeActionForGPUStoreOp(descriptor.depthStoreOp)];

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

static void useAttachments(GPUCommandBuffer& buffer, GPURenderPassDescriptor&& descriptor)
{
    for (auto& colorAttachment : descriptor.colorAttachments)
        buffer.useTexture(WTFMove(colorAttachment.attachment));
    if (descriptor.depthStencilAttachment)
        buffer.useTexture(WTFMove((*descriptor.depthStencilAttachment).attachment));
}

RefPtr<GPURenderPassEncoder> GPURenderPassEncoder::tryCreate(Ref<GPUCommandBuffer>&& buffer, GPURenderPassDescriptor&& descriptor)
{
    const char* const functionName = "GPURenderPassEncoder::tryCreate()";

    // Only one command encoder may be active at a time.
    if (buffer->isEncodingPass()) {
        LOG(WebGPU, "%s: Existing pass encoder must be ended first!");
        return nullptr;
    }

    RetainPtr<MTLRenderPassDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLRenderPassDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Unable to create MTLRenderPassDescriptor!", functionName);
        return nullptr;
    }

    if (!populateMtlColorAttachmentsArray(mtlDescriptor.get().colorAttachments, descriptor.colorAttachments, functionName))
        return nullptr;

    if (descriptor.depthStencilAttachment
        && !populateMtlDepthStencilAttachment(mtlDescriptor.get().depthAttachment, *descriptor.depthStencilAttachment, functionName))
        return nullptr;

    buffer->endBlitEncoding();

    RetainPtr<MTLRenderCommandEncoder> mtlEncoder;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlEncoder = [buffer->platformCommandBuffer() renderCommandEncoderWithDescriptor:mtlDescriptor.get()];

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlEncoder) {
        LOG(WebGPU, "%s: Unable to create MTLRenderCommandEncoder!", functionName);
        return nullptr;
    }

    // All is well; ensure GPUCommandBuffer is aware of new attachments.
    useAttachments(buffer, WTFMove(descriptor));
    
    return adoptRef(new GPURenderPassEncoder(WTFMove(buffer), WTFMove(mtlEncoder)));
}

GPURenderPassEncoder::GPURenderPassEncoder(Ref<GPUCommandBuffer>&& commandBuffer, RetainPtr<MTLRenderCommandEncoder>&& encoder)
    : GPUProgrammablePassEncoder(WTFMove(commandBuffer))
    , m_platformRenderPassEncoder(WTFMove(encoder))
{
}

MTLCommandEncoder *GPURenderPassEncoder::platformPassEncoder() const
{
    return m_platformRenderPassEncoder.get();
}

void GPURenderPassEncoder::endPass()
{
    if (!m_platformRenderPassEncoder)
        return;
    GPUProgrammablePassEncoder::endPass();
    m_platformRenderPassEncoder = nullptr;
}

void GPURenderPassEncoder::setPipeline(Ref<GPURenderPipeline>&& pipeline)
{
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setPipeline(): Invalid operation: Encoding is ended!");
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (pipeline->depthStencilState())
        [m_platformRenderPassEncoder setDepthStencilState:pipeline->depthStencilState()];

    [m_platformRenderPassEncoder setRenderPipelineState:pipeline->platformRenderPipeline()];

    END_BLOCK_OBJC_EXCEPTIONS;

    m_pipeline = WTFMove(pipeline);
}

void GPURenderPassEncoder::setVertexBuffers(unsigned long index, Vector<Ref<GPUBuffer>>&& buffers, Vector<unsigned long long>&& offsets)
{
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setVertexBuffers(): Invalid operation: Encoding is ended!");
        return;
    }

    ASSERT(buffers.size() && offsets.size() == buffers.size());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto mtlBuffers = buffers.map([this] (auto& buffer) {
        commandBuffer().useBuffer(buffer.copyRef());
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
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::draw(): Invalid operation: Encoding is ended!");
        return;
    }

    if (!m_pipeline) {
        LOG(WebGPU, "GPURenderPassEncoder::draw(): No valid GPURenderPipeline found!");
        return;
    }

    [m_platformRenderPassEncoder 
        drawPrimitives:primitiveTypeForGPUPrimitiveTopology(m_pipeline->primitiveTopology())
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

#if USE(METAL)

void GPURenderPassEncoder::useResource(MTLResource *resource, unsigned usage)
{
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder: Invalid operation: Encoding is ended!");
        return;
    }

    [m_platformRenderPassEncoder useResource:resource usage:usage];
}

void GPURenderPassEncoder::setVertexBuffer(MTLBuffer *buffer, unsigned offset, unsigned index)
{
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder: Invalid operation: Encoding is ended!");
        return;
    }

    [m_platformRenderPassEncoder setVertexBuffer:buffer offset:offset atIndex:index];
}

void GPURenderPassEncoder::setFragmentBuffer(MTLBuffer *buffer, unsigned offset, unsigned index)
{
    if (!m_platformRenderPassEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder: Invalid operation: Encoding is ended!");
        return;
    }

    [m_platformRenderPassEncoder setFragmentBuffer:buffer offset:offset atIndex:index];
}

#endif // USE(METAL)

} // namespace WebCore

#endif // ENABLE(WEBGPU)

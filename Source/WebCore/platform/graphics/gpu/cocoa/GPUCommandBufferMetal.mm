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
#import "GPUCommandBuffer.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUExtent3D.h"
#import "GPUQueue.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CheckedArithmetic.h>

namespace WebCore {

RefPtr<GPUCommandBuffer> GPUCommandBuffer::tryCreate(const GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUCommandBuffer::create(): Invalid GPUDevice!");
        return nullptr;
    }

    auto gpuCommandQueue = device.tryGetQueue();
    if (!gpuCommandQueue)
        return nullptr;

    auto mtlQueue = gpuCommandQueue->platformQueue();

    RetainPtr<MTLCommandBuffer> buffer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    buffer = [mtlQueue commandBuffer];

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!buffer) {
        LOG(WebGPU, "GPUCommandBuffer::create(): Unable to create MTLCommandBuffer!");
        return nullptr;
    }

    return adoptRef(new GPUCommandBuffer(WTFMove(buffer)));
}

GPUCommandBuffer::GPUCommandBuffer(RetainPtr<MTLCommandBuffer>&& buffer)
    : m_platformCommandBuffer(WTFMove(buffer))
{
}

GPUCommandBuffer::~GPUCommandBuffer()
{
    endBlitEncoding();
}

MTLBlitCommandEncoder *GPUCommandBuffer::blitEncoder() const
{
    return m_blitEncoder ? m_blitEncoder.get() : (m_blitEncoder = [m_platformCommandBuffer blitCommandEncoder]).get();
}

void GPUCommandBuffer::endBlitEncoding()
{
    if (!m_blitEncoder)
        return;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_blitEncoder endEncoding];
    END_BLOCK_OBJC_EXCEPTIONS;
    m_blitEncoder = nullptr;
}

void GPUCommandBuffer::copyBufferToBuffer(Ref<GPUBuffer>&& src, uint64_t srcOffset, Ref<GPUBuffer>&& dst, uint64_t dstOffset, uint64_t size)
{
    if (isEncodingPass() || !src->isTransferSource() || !dst->isTransferDestination()) {
        LOG(WebGPU, "GPUCommandBuffer::copyBufferToBuffer(): Invalid operation!");
        return;
    }

#if PLATFORM(MAC)
    if (size % 4 || srcOffset % 4 || dstOffset % 4) {
        LOG(WebGPU, "GPUCommandBuffer::copyBufferToBuffer(): Copy must be aligned to a multiple of 4 bytes!");
        return;
    }
#endif

    // This call ensures that size, offset, and size + offset can safely fit in NSUInteger regardless of platform.
    auto srcLength = checkedSum<NSUInteger>(size, srcOffset);
    auto dstLength = checkedSum<NSUInteger>(size, dstOffset);
    if (srcLength.hasOverflowed() || dstLength.hasOverflowed()
        || srcLength.unsafeGet() > src->byteLength() || dstLength.unsafeGet() > dst->byteLength()) {
        LOG(WebGPU, "GPUCommandBuffer::copyBufferToBuffer(): Invalid offset or copy size!");
        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // These casts are safe due to earlier checkedSum() checks.
    [blitEncoder()
        copyFromBuffer:src->platformBuffer()
        sourceOffset:static_cast<NSUInteger>(srcOffset)
        toBuffer:dst->platformBuffer()
        destinationOffset:static_cast<NSUInteger>(dstOffset)
        size:static_cast<NSUInteger>(size)];

    END_BLOCK_OBJC_EXCEPTIONS;

    useBuffer(WTFMove(src));
    useBuffer(WTFMove(dst));
}

void GPUCommandBuffer::copyBufferToTexture(GPUBufferCopyView&& srcBuffer, GPUTextureCopyView&& dstTexture, const GPUExtent3D& size)
{
    if (isEncodingPass() || !srcBuffer.buffer->isTransferSource() || !dstTexture.texture->isTransferDestination()) {
        LOG(WebGPU, "GPUComandBuffer::copyBufferToTexture(): Invalid operation!");
        return;
    }

    // MTLBuffer size (NSUInteger) is 32 bits on some platforms.
    NSUInteger sourceOffset = 0;
    if (!WTF::convertSafely(srcBuffer.offset, sourceOffset)) {
        LOG(WebGPU, "GPUCommandBuffer::copyBufferToTexture(): Source offset is too large!");
        return;
    }

    // FIXME: Add Metal validation.

    // GPUBufferCopyView::offset: The location must be aligned to the size of the destination texture's pixel format. The value must be a multiple of the destination texture's pixel size, in bytes.

    // GPUBufferCopyView::rowPitch: The value must be a multiple of the destination texture's pixel size, in bytes. The value must be less than or equal to 32,767 multiplied by the destination texture’s pixel size.

    // GPUBufferCopyView::imageHeight: The value must be a multiple of the destination texture's pixel size, in bytes.

    // GPUExtent3D: When you copy to a 1D texture, height and depth must be 1. When you copy to a 2D texture, depth must be 1.

    // GPUTextureCopyView::texture: The value must not be a framebufferOnly texture and must not have a PVRTC pixel format.

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [blitEncoder()
        copyFromBuffer:srcBuffer.buffer->platformBuffer()
        sourceOffset:sourceOffset
        sourceBytesPerRow:srcBuffer.rowPitch
        sourceBytesPerImage:srcBuffer.imageHeight
        sourceSize:MTLSizeMake(size.width, size.height, size.depth)
        toTexture:dstTexture.texture->platformTexture()
        destinationSlice:dstTexture.arrayLayer
        destinationLevel:dstTexture.mipLevel
        destinationOrigin:MTLOriginMake(dstTexture.origin.x, dstTexture.origin.y, dstTexture.origin.z)];

    END_BLOCK_OBJC_EXCEPTIONS;

    useBuffer(WTFMove(srcBuffer.buffer));
    useTexture(WTFMove(dstTexture.texture));
}

void GPUCommandBuffer::copyTextureToBuffer(GPUTextureCopyView&& srcTexture, GPUBufferCopyView&& dstBuffer, const GPUExtent3D& size)
{
    if (isEncodingPass() || !srcTexture.texture->isTransferSource() || !dstBuffer.buffer->isTransferDestination()) {
        LOG(WebGPU, "GPUCommandBuffer::copyTextureToBuffer(): Invalid operation!");
        return;
    }
    // MTLBuffer size (NSUInteger) is 32 bits on some platforms.
    NSUInteger destinationOffset = 0;
    if (!WTF::convertSafely(dstBuffer.offset, destinationOffset)) {
        LOG(WebGPU, "GPUCommandBuffer::copyTextureToBuffer(): Destination offset is too large!");
        return;
    }

    // FIXME: Add Metal validation?

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [blitEncoder()
        copyFromTexture:srcTexture.texture->platformTexture()
        sourceSlice:srcTexture.arrayLayer
        sourceLevel:srcTexture.mipLevel
        sourceOrigin:MTLOriginMake(srcTexture.origin.x, srcTexture.origin.y, srcTexture.origin.z)
        sourceSize:MTLSizeMake(size.width, size.height, size.depth)
        toBuffer:dstBuffer.buffer->platformBuffer()
        destinationOffset:destinationOffset
        destinationBytesPerRow:dstBuffer.rowPitch
        destinationBytesPerImage:dstBuffer.imageHeight];

    END_BLOCK_OBJC_EXCEPTIONS;

    useTexture(WTFMove(srcTexture.texture));
    useBuffer(WTFMove(dstBuffer.buffer));
}

void GPUCommandBuffer::copyTextureToTexture(GPUTextureCopyView&& src, GPUTextureCopyView&& dst, const GPUExtent3D& size)
{
    if (isEncodingPass() || !src.texture->isTransferSource() || !dst.texture->isTransferDestination()) {
        LOG(WebGPU, "GPUCommandBuffer::copyTextureToTexture(): Invalid operation!");
        return;
    }

    // FIXME: Add Metal validation?

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [blitEncoder()
        copyFromTexture:src.texture->platformTexture()
        sourceSlice:src.arrayLayer
        sourceLevel:src.mipLevel
        sourceOrigin:MTLOriginMake(src.origin.x, src.origin.y, src.origin.z)
        sourceSize:MTLSizeMake(size.width, size.height, size.depth)
        toTexture:dst.texture->platformTexture()
        destinationSlice:src.arrayLayer
        destinationLevel:src.mipLevel
        destinationOrigin:MTLOriginMake(dst.origin.x, dst.origin.y, dst.origin.z)];

    END_BLOCK_OBJC_EXCEPTIONS;

    useTexture(WTFMove(src.texture));
    useTexture(WTFMove(dst.texture));
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)

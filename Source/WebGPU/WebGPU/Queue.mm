/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "Queue.h"

#import "APIConversions.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "Device.h"
#import "IsValidToUseWith.h"
#import "Texture.h"
#import <wtf/CheckedArithmetic.h>

namespace WebGPU {

Queue::Queue(id<MTLCommandQueue> commandQueue, Device& device)
    : m_commandQueue(commandQueue)
    , m_device(device)
{
}

Queue::Queue(Device& device)
    : m_device(device)
{
}

Queue::~Queue()
{
    // If we're not idle, then there's a pending completed handler to be run,
    // but the completed handler should have retained us,
    // which means we shouldn't be being destroyed.
    // So we must be idle.
    ASSERT(isIdle());

    // We can't actually call finalizeBlitCommandEncoder() here because, if there are pending copies,
    // that would cause them to be committed, which ends up retaining this in the completed handler.
    // It's actually fine, though, because we can just drop any pending copies on the floor.
    // If the queue is being destroyed, this is unobservable.
    if (m_blitCommandEncoder)
        [m_blitCommandEncoder endEncoding];
}

void Queue::ensureBlitCommandEncoder()
{
    if (m_blitCommandEncoder)
        return;

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    commandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    m_commandBuffer = [m_commandQueue commandBufferWithDescriptor:commandBufferDescriptor];
    m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
}

void Queue::finalizeBlitCommandEncoder()
{
    if (m_blitCommandEncoder) {
        [m_blitCommandEncoder endEncoding];
        commitMTLCommandBuffer(m_commandBuffer);
        m_blitCommandEncoder = nil;
        m_commandBuffer = nil;
    }
}

void Queue::onSubmittedWorkDone(uint64_t, CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-onsubmittedworkdone

    ASSERT(m_submittedCommandBufferCount >= m_completedCommandBufferCount);

    finalizeBlitCommandEncoder();

    if (isIdle()) {
        scheduleWork([callback = WTFMove(callback)]() mutable {
            callback(WGPUQueueWorkDoneStatus_Success);
        });
        return;
    }

    auto& callbacks = m_onSubmittedWorkDoneCallbacks.add(m_submittedCommandBufferCount, OnSubmittedWorkDoneCallbacks()).iterator->value;
    callbacks.append(WTFMove(callback));
}

bool Queue::validateSubmit(const Vector<std::reference_wrapper<const CommandBuffer>>& commands) const
{
    for (auto command : commands) {
        if (!isValidToUseWith(command.get(), *this))
            return false;
    }

    // FIXME: "Every GPUBuffer referenced in any element of commandBuffers is in the "unmapped" buffer state."

    // FIXME: "Every GPUQuerySet referenced in a command in any element of commandBuffers is in the available state."
    // FIXME: "For occlusion queries, occlusionQuerySet in beginRenderPass() does not constitute a reference, while beginOcclusionQuery() does."

    // There's only one queue right now, so there is no need to make sure that the command buffers are being submitted to the correct queue.

    return true;
}

void Queue::commitMTLCommandBuffer(id<MTLCommandBuffer> commandBuffer)
{
    ASSERT(commandBuffer.commandQueue == m_commandQueue);
    [commandBuffer addCompletedHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer>) {
        protectedThis->scheduleWork(CompletionHandler<void(void)>([protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_completedCommandBufferCount);
            for (auto& callback : protectedThis->m_onSubmittedWorkDoneCallbacks.take(protectedThis->m_completedCommandBufferCount))
                callback(WGPUQueueWorkDoneStatus_Success);
        }, CompletionHandlerCallThread::AnyThread));
    }];

    [commandBuffer commit];
    ++m_submittedCommandBufferCount;

    if ([MTLCaptureManager sharedCaptureManager].isCapturing)
        [[MTLCaptureManager sharedCaptureManager] stopCapture];
}

void Queue::submit(Vector<std::reference_wrapper<const CommandBuffer>>&& commands)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-submit

    if (!validateSubmit(commands)) {
        m_device.generateAValidationError("Validation failure."_s);
        return;
    }

    finalizeBlitCommandEncoder();

    for (auto commandBuffer : commands)
        commitMTLCommandBuffer(commandBuffer.get().commandBuffer());
}

static bool validateWriteBufferInitial(size_t size)
{
    if (size % 4)
        return false;

    return true;
}

bool Queue::validateWriteBuffer(const Buffer& buffer, uint64_t bufferOffset, size_t size) const
{
    if (!isValidToUseWith(buffer, *this))
        return false;

    if (buffer.state() != Buffer::State::Unmapped)
        return false;

    if (!(buffer.usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (bufferOffset % 4)
        return false;

    auto end = checkedSum<uint64_t>(bufferOffset, size);
    if (end.hasOverflowed() || end.value() > buffer.size())
        return false;

    return true;
}

void Queue::writeBuffer(const Buffer& buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writebuffer

    if (!validateWriteBufferInitial(size)) {
        // FIXME: "throw OperationError and stop."
        return;
    }

    if (!validateWriteBuffer(buffer, bufferOffset, size)) {
        m_device.generateAValidationError("Validation failure."_s);
        return;
    }

    if (!size)
        return;

    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    if (isIdle()) {
        switch (buffer.buffer().storageMode) {
        case MTLStorageModeShared:
            memcpy(static_cast<char*>(buffer.buffer().contents) + bufferOffset, data, size);
            return;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        case MTLStorageModeManaged:
            memcpy(static_cast<char*>(buffer.buffer().contents) + bufferOffset, data, size);
            [buffer.buffer() didModifyRange:NSMakeRange(bufferOffset, size)];
            return;
#endif
        case MTLStorageModePrivate:
            // The only way to get data into a private resource is to tell the GPU to copy it in.
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    }

    ensureBlitCommandEncoder();
    // FIXME(PERFORMANCE): Suballocate, so the common case doesn't need to hit the kernel.
    // FIXME(PERFORMANCE): Should this temporary buffer really be shared?
    id<MTLBuffer> temporaryBuffer = [m_device.device() newBufferWithBytes:data length:static_cast<NSUInteger>(size) options:MTLResourceStorageModeShared];
    if (!temporaryBuffer)
        return;
    [m_blitCommandEncoder
        copyFromBuffer:temporaryBuffer
        sourceOffset:0
        toBuffer:buffer.buffer()
        destinationOffset:static_cast<NSUInteger>(bufferOffset)
        size:static_cast<NSUInteger>(size)];
}

static bool validateWriteTexture(const WGPUImageCopyTexture& destination, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size, size_t dataByteSize, const WGPUTextureDescriptor& textureDesc)
{
    if (!Texture::validateImageCopyTexture(destination, size))
        return false;

    if (!(textureDesc.usage & WGPUTextureUsage_CopyDst))
        return false;

    if (textureDesc.sampleCount != 1)
        return false;

    if (!Texture::validateTextureCopyRange(destination, size))
        return false;

    if (!Texture::refersToSingleAspect(textureDesc.format, destination.aspect))
        return false;

    if (!Texture::isValidImageCopyDestination(textureDesc.format, destination.aspect))
        return false;

    auto aspectSpecificFormat = textureDesc.format;

    if (Texture::isDepthOrStencilFormat(textureDesc.format))
        aspectSpecificFormat = Texture::aspectSpecificFormat(textureDesc.format, destination.aspect);

    if (!Texture::validateLinearTextureData(dataLayout, dataByteSize, aspectSpecificFormat, size))
        return false;

    return true;
}

void Queue::writeTexture(const WGPUImageCopyTexture& destination, const void* data, size_t dataSize, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size)
{
    if (destination.nextInChain || dataLayout.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writetexture

    auto dataByteSize = dataSize;

    const auto& textureDesc = fromAPI(destination.texture).descriptor();

    if (!validateWriteTexture(destination, dataLayout, size, dataByteSize, textureDesc)) {
        m_device.generateAValidationError("Validation failure."_s);
        return;
    }

    if (!dataSize)
        return;

    NSUInteger bytesPerRow = dataLayout.bytesPerRow;

    NSUInteger bytesPerImage = dataLayout.bytesPerRow * dataLayout.rowsPerImage;

    MTLBlitOption options = MTLBlitOptionNone;
    switch (destination.aspect) {
    case WGPUTextureAspect_All:
        options = MTLBlitOptionNone;
        break;
    case WGPUTextureAspect_StencilOnly:
        options = MTLBlitOptionStencilFromDepthStencil;
        break;
    case WGPUTextureAspect_DepthOnly:
        options = MTLBlitOptionDepthFromDepthStencil;
        break;
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    if (isIdle() && options == MTLBlitOptionNone) {
        switch (fromAPI(destination.texture).texture().storageMode) {
        case MTLStorageModeShared:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        case MTLStorageModeManaged:
#endif
            {
                switch (textureDesc.dimension) {
                case WGPUTextureDimension_1D: {
                    auto region = MTLRegionMake1D(destination.origin.x, size.width);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset + layer * bytesPerImage);
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [fromAPI(destination.texture).texture()
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:static_cast<const char*>(data) + sourceOffset
                            bytesPerRow:bytesPerRow
                            bytesPerImage:bytesPerImage];
                    }
                    break;
                }
                case WGPUTextureDimension_2D: {
                    auto region = MTLRegionMake2D(destination.origin.x, destination.origin.y, size.width, size.height);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset + layer * bytesPerImage);
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [fromAPI(destination.texture).texture()
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:static_cast<const char*>(data) + sourceOffset
                            bytesPerRow:bytesPerRow
                            bytesPerImage:bytesPerImage];
                    }
                    break;
                }
                case WGPUTextureDimension_3D: {
                    auto region = MTLRegionMake3D(destination.origin.x, destination.origin.y, destination.origin.z, size.width, size.height, size.depthOrArrayLayers);
                    auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset);
                    [fromAPI(destination.texture).texture()
                        replaceRegion:region
                        mipmapLevel:destination.mipLevel
                        slice:0
                        withBytes:static_cast<const char*>(data) + sourceOffset
                        bytesPerRow:bytesPerRow
                        bytesPerImage:bytesPerImage];
                    break;
                }
                case WGPUTextureDimension_Force32:
                    ASSERT_NOT_REACHED();
                    return;
                }
                return;
            }
        case MTLStorageModePrivate:
            // The only way to get data into a private resource is to tell the GPU to copy it in.
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    }

    ensureBlitCommandEncoder();
    // FIXME(PERFORMANCE): Suballocate, so the common case doesn't need to hit the kernel.
    // FIXME(PERFORMANCE): Should this temporary buffer really be shared?
    id<MTLBuffer> temporaryBuffer = [m_device.device() newBufferWithBytes:static_cast<const char*>(data) + dataLayout.offset length:static_cast<NSUInteger>(dataByteSize) options:MTLResourceStorageModeShared];
    if (!temporaryBuffer)
        return;

    auto logicalSize = fromAPI(destination.texture).logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = std::min(size.width, logicalSize.width);
    auto heightForMetal = std::min(size.height, logicalSize.height);
    auto depthForMetal = std::min(size.depthOrArrayLayers, logicalSize.depthOrArrayLayers);

    switch (textureDesc.dimension) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, 1, 1);
        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            NSUInteger sourceOffset = layer * bytesPerImage;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:bytesPerRow
                sourceBytesPerImage:bytesPerImage
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 1);
        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            NSUInteger sourceOffset = layer * bytesPerImage;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:bytesPerRow
                sourceBytesPerImage:bytesPerImage
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);
        NSUInteger sourceOffset = 0;
        [m_blitCommandEncoder
            copyFromBuffer:temporaryBuffer
            sourceOffset:sourceOffset
            sourceBytesPerRow:bytesPerRow
            sourceBytesPerImage:bytesPerImage
            sourceSize:sourceSize
            toTexture:fromAPI(destination.texture).texture()
            destinationSlice:0
            destinationLevel:destination.mipLevel
            destinationOrigin:destinationOrigin
            options:options];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

void Queue::setLabel(String&& label)
{
    m_commandQueue.label = label;
}

void Queue::scheduleWork(Instance::WorkItem&& workItem)
{
    m_device.instance().scheduleWork(WTFMove(workItem));
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuQueueRelease(WGPUQueue queue)
{
    WebGPU::fromAPI(queue).deref();
}

void wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneCallback callback, void* userdata)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone(signalValue, [callback, userdata](WGPUQueueWorkDoneStatus status) {
        callback(status, userdata);
    });
}

void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneBlockCallback callback)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone(signalValue, [callback = WTFMove(callback)](WGPUQueueWorkDoneStatus status) {
        callback(status);
    });
}

void wgpuQueueSubmit(WGPUQueue queue, uint32_t commandCount, const WGPUCommandBuffer* commands)
{
    Vector<std::reference_wrapper<const WebGPU::CommandBuffer>> commandsToForward;
    for (uint32_t i = 0; i < commandCount; ++i)
        commandsToForward.append(WebGPU::fromAPI(commands[i]));
    WebGPU::fromAPI(queue).submit(WTFMove(commandsToForward));
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    WebGPU::fromAPI(queue).writeBuffer(WebGPU::fromAPI(buffer), bufferOffset, data, size);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, const void* data, size_t dataSize, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    WebGPU::fromAPI(queue).writeTexture(*destination, data, dataSize, *dataLayout, *writeSize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    WebGPU::fromAPI(queue).setLabel(WebGPU::fromAPI(label));
}

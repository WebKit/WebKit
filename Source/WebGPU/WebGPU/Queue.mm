/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "CommandEncoder.h"
#import "Device.h"
#import "IsValidToUseWith.h"
#import "Texture.h"
#import <wtf/CheckedArithmetic.h>

namespace WebGPU {

constexpr static auto largeBufferSize = 32 * 1024 * 1024;

Queue::Queue(id<MTLCommandQueue> commandQueue, Device& device)
    : m_commandQueue(commandQueue)
    , m_device(device)
{
    m_pendingCommandBuffers = [NSMutableSet set];
    m_createdNotCommittedBuffers = [NSMutableOrderedSet orderedSet];
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
    m_commandBuffer = commandBufferWithDescriptor(commandBufferDescriptor);
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

id<MTLCommandBuffer> Queue::commandBufferWithDescriptor(MTLCommandBufferDescriptor* descriptor)
{
    constexpr auto maxCommandBufferCount = 64;
    if (m_createdNotCommittedBuffers.count >= maxCommandBufferCount) {
        id<MTLCommandBuffer> buffer = [m_createdNotCommittedBuffers objectAtIndex:0];
        [m_createdNotCommittedBuffers removeObjectAtIndex:0];
        commitMTLCommandBuffer(buffer);
        [buffer waitUntilCompleted];
    }

    id<MTLCommandBuffer> buffer = [m_commandQueue commandBufferWithDescriptor:descriptor];
    [m_createdNotCommittedBuffers addObject:buffer];
    return buffer;
}

void Queue::onSubmittedWorkDone(CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback)
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

void Queue::onSubmittedWorkScheduled(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(m_submittedCommandBufferCount >= m_scheduledCommandBufferCount);

    finalizeBlitCommandEncoder();

    if (isSchedulingIdle()) {
        scheduleWork([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
        return;
    }

    auto& callbacks = m_onSubmittedWorkScheduledCallbacks.add(m_submittedCommandBufferCount, OnSubmittedWorkScheduledCallbacks()).iterator->value;
    callbacks.append(WTFMove(completionHandler));
}

bool Queue::validateSubmit(const Vector<std::reference_wrapper<CommandBuffer>>& commands) const
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
    if (!commandBuffer || commandBuffer.status >= MTLCommandBufferStatusCommitted)
        return;

    ASSERT(commandBuffer.commandQueue == m_commandQueue);
    [commandBuffer addScheduledHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer>) {
        protectedThis->scheduleWork(CompletionHandler<void(void)>([protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_scheduledCommandBufferCount);
            for (auto& callback : protectedThis->m_onSubmittedWorkScheduledCallbacks.take(protectedThis->m_scheduledCommandBufferCount))
                callback();
        }, CompletionHandlerCallThread::AnyThread));
    }];
    [commandBuffer addCompletedHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer> commandBuffer) {
        protectedThis->scheduleWork(CompletionHandler<void(void)>([commandBuffer, protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_completedCommandBufferCount);
            [protectedThis->m_pendingCommandBuffers removeObject:commandBuffer];
            for (auto& callback : protectedThis->m_onSubmittedWorkDoneCallbacks.take(protectedThis->m_completedCommandBufferCount))
                callback(WGPUQueueWorkDoneStatus_Success);
        }, CompletionHandlerCallThread::AnyThread));
    }];

    [m_pendingCommandBuffers addObject:commandBuffer];
    [commandBuffer commit];
    [m_createdNotCommittedBuffers removeObject:commandBuffer];
    ++m_submittedCommandBufferCount;
}

void Queue::submit(Vector<std::reference_wrapper<CommandBuffer>>&& commands)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-submit

    if (!validateSubmit(commands)) {
        m_device.generateAValidationError("Validation failure."_s);
        return;
    }

    finalizeBlitCommandEncoder();

    NSMutableArray<id<MTLCommandBuffer>> *commandBuffersToSubmit = [NSMutableArray arrayWithCapacity:commands.size()];
    for (auto commandBuffer : commands) {
        auto& command = commandBuffer.get();
        if (id<MTLCommandBuffer> mtlBuffer = command.commandBuffer())
            [commandBuffersToSubmit addObject:mtlBuffer];
        else {
            m_device.generateAValidationError("Command buffer appears twice."_s);
            return;
        }
        command.makeInvalid();
    }

    for (id<MTLCommandBuffer> commandBuffer in commandBuffersToSubmit)
        commitMTLCommandBuffer(commandBuffer);

    if ([MTLCaptureManager sharedCaptureManager].isCapturing && m_device.shouldStopCaptureAfterSubmit())
        [[MTLCaptureManager sharedCaptureManager] stopCapture];
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

    auto bufferState = buffer.state();
    if (bufferState != Buffer::State::Unmapped && bufferState != Buffer::State::MappedAtCreation)
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

void Queue::waitUntilIdle()
{
    finalizeBlitCommandEncoder();
    for (id<MTLCommandBuffer> commandBuffer in m_pendingCommandBuffers)
        [commandBuffer waitUntilCompleted];
}

void Queue::writeBuffer(const Buffer& buffer, uint64_t bufferOffset, void* data, size_t size)
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

    writeBuffer(buffer.buffer(), bufferOffset, data, size);
}

void Queue::writeBuffer(id<MTLBuffer> buffer, uint64_t bufferOffset, void* data, size_t size)
{
    ensureBlitCommandEncoder();
    // FIXME(PERFORMANCE): Suballocate, so the common case doesn't need to hit the kernel.
    // FIXME(PERFORMANCE): Should this temporary buffer really be shared?
    bool noCopy = size >= largeBufferSize;
    id<MTLBuffer> temporaryBuffer = noCopy ? [m_device.device() newBufferWithBytesNoCopy:data length:static_cast<NSUInteger>(size) options:MTLResourceStorageModeShared deallocator:nil] : [m_device.device() newBufferWithBytes:data length:static_cast<NSUInteger>(size) options:MTLResourceStorageModeShared];
    if (!temporaryBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    [m_blitCommandEncoder
        copyFromBuffer:temporaryBuffer
        sourceOffset:0
        toBuffer:buffer
        destinationOffset:static_cast<NSUInteger>(bufferOffset)
        size:static_cast<NSUInteger>(size)];

    if (noCopy)
        finalizeBlitCommandEncoder();
}

bool Queue::isIdle() const
{
    return m_submittedCommandBufferCount == m_completedCommandBufferCount && !m_blitCommandEncoder;
}

static bool validateWriteTexture(const WGPUImageCopyTexture& destination, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size, size_t dataByteSize, const Texture& texture)
{
    if (!Texture::validateImageCopyTexture(destination, size))
        return false;

    if (!(texture.usage() & WGPUTextureUsage_CopyDst))
        return false;

    if (texture.sampleCount() != 1)
        return false;

    if (!Texture::validateTextureCopyRange(destination, size))
        return false;

    if (!Texture::refersToSingleAspect(texture.format(), destination.aspect))
        return false;

    auto aspectSpecificFormat = texture.format();

    if (Texture::isDepthOrStencilFormat(texture.format())) {
        if (!Texture::isValidDepthStencilCopyDestination(texture.format(), destination.aspect))
            return false;

        aspectSpecificFormat = Texture::aspectSpecificFormat(texture.format(), destination.aspect);
    }

    if (!Texture::validateLinearTextureData(dataLayout, dataByteSize, aspectSpecificFormat, size))
        return false;

    return true;
}

void Queue::clearTexture(const WGPUImageCopyTexture& destination, NSUInteger slice)
{
    ensureBlitCommandEncoder();
    CommandEncoder::clearTexture(destination, slice, m_device.device(), m_blitCommandEncoder);
}

void Queue::writeTexture(const WGPUImageCopyTexture& destination, void* data, size_t dataSize, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size)
{
    if (destination.nextInChain || dataLayout.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writetexture

    auto dataByteSize = dataSize;

    auto& texture = fromAPI(destination.texture);
    auto textureFormat = texture.format();
    if (Texture::isDepthOrStencilFormat(textureFormat))
        textureFormat = Texture::aspectSpecificFormat(textureFormat, destination.aspect);

    uint32_t blockSize = Texture::texelBlockSize(textureFormat);

    if (!validateWriteTexture(destination, dataLayout, size, dataByteSize, texture)) {
        m_device.generateAValidationError("Validation failure."_s);
        return;
    }

    if (!dataSize)
        return;

    auto logicalSize = texture.logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = std::min(size.width, logicalSize.width);
    if (!widthForMetal)
        return;

    auto heightForMetal = std::min(size.height, logicalSize.height);
    auto depthForMetal = std::min(size.depthOrArrayLayers, logicalSize.depthOrArrayLayers);

    NSUInteger bytesPerRow = dataLayout.bytesPerRow;
    if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        bytesPerRow = size.height ? (dataSize / size.height) : dataSize;

    NSUInteger rowsPerImage = (dataLayout.rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED) ? size.height : dataLayout.rowsPerImage;
    NSUInteger bytesPerImage = bytesPerRow * rowsPerImage;

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

    id<MTLTexture> mtlTexture = texture.texture();
    auto textureDimension = texture.dimension();
    uint32_t sliceCount = textureDimension == WGPUTextureDimension_3D ? 1 : size.depthOrArrayLayers;
    for (uint32_t layer = 0; layer < sliceCount; ++layer) {
        NSUInteger destinationSlice = textureDimension == WGPUTextureDimension_3D ? 0 : (destination.origin.z + layer);
        if (!texture.previouslyCleared(destination.mipLevel, destinationSlice)) {
            if (widthForMetal == logicalSize.width && heightForMetal == logicalSize.height)
                texture.setPreviouslyCleared(destination.mipLevel, destinationSlice);
            else if (!texture.previouslyCleared(destination.mipLevel, destinationSlice))
                clearTexture(destination, destinationSlice);
        }
        texture.setPreviouslyCleared(destination.mipLevel, destinationSlice);
    }

    NSUInteger maxRowBytes = textureDimension == WGPUTextureDimension_3D ? (2048 * blockSize) : bytesPerRow;
    bool isCompressed = Texture::isCompressedFormat(textureFormat);
    auto blockHeight = Texture::texelBlockHeight(textureFormat);
    auto blockWidth = Texture::texelBlockWidth(textureFormat);
    if (!isCompressed && (bytesPerRow % blockSize || (bytesPerRow > maxRowBytes))) {
        WGPUExtent3D newSize {
            .width = size.width,
            .height = isCompressed ? blockSize : blockHeight,
            .depthOrArrayLayers = 1
        };

        if (textureDimension != WGPUTextureDimension_1D && (heightForMetal > newSize.height || depthForMetal > newSize.depthOrArrayLayers)) {
            WGPUTextureDataLayout newDataLayout {
                .nextInChain = nullptr,
                .offset = 0,
                .bytesPerRow = std::min<uint32_t>(maxRowBytes, dataLayout.bytesPerRow),
                .rowsPerImage = newSize.height
            };

            for (uint32_t z = 0, endZ = std::max<uint32_t>(1, depthForMetal); z < endZ; ++z) {
                WGPUImageCopyTexture newDestination = destination;
                newDestination.origin.z = destination.origin.z + z;
                for (uint32_t y = 0, endY = std::max<uint32_t>(1, heightForMetal); y < endY; y += newSize.height) {
                    newDestination.origin.y = destination.origin.y + y;
                    if (newDestination.origin.y + newSize.height > logicalSize.height)
                        newSize.height = static_cast<uint32_t>(newDestination.origin.y + newSize.height - logicalSize.height);

                    for (uint32_t x = 0; x < widthForMetal; x += maxRowBytes) {
                        newDestination.origin.x = destination.origin.x + x;
                        writeTexture(newDestination, static_cast<uint8_t*>(data) + x + y * bytesPerRow + z * bytesPerImage, bytesPerRow * newSize.height, newDataLayout, newSize);
                    }
                }
            }
            return;
        }

        ASSERT(heightForMetal == 1);
        bytesPerRow = 0;
        bytesPerImage = 0;
    }

    Vector<uint8_t> newData;
    const auto newBytesPerRow = blockSize * ((widthForMetal / blockWidth) + ((widthForMetal % blockWidth) ? 1 : 0));
    if (isCompressed && newBytesPerRow != bytesPerRow && (widthForMetal == logicalSize.width && heightForMetal == logicalSize.height)) {

        auto maxY = std::max<size_t>(blockHeight, heightForMetal) / blockHeight;
        auto newBytesPerImage = newBytesPerRow * std::max<size_t>(blockHeight, logicalSize.height) / blockHeight;
        auto maxZ = std::max<size_t>(1, size.depthOrArrayLayers);
        newData.resize(newBytesPerImage * maxZ);
        memset(&newData[0], 0, newData.size());
        for (size_t z = 0; z < maxZ; ++z) {
            for (size_t y = 0; y < maxY; ++y) {
                auto sourceBytes = static_cast<const uint8_t*>(data) + y * bytesPerRow + z * bytesPerImage;
                RELEASE_ASSERT(y * bytesPerRow + z * bytesPerImage + newBytesPerRow <= dataByteSize);
                auto destBytes = &newData[0] + y * newBytesPerRow + z * newBytesPerImage;
                memcpy(destBytes, sourceBytes, newBytesPerRow);
            }
        }

        bytesPerRow = newBytesPerRow;
        dataByteSize = newData.size();
        bytesPerImage = newBytesPerImage;
        data = &newData[0];
    }

    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    if (isIdle() && options == MTLBlitOptionNone) {
        switch (mtlTexture.storageMode) {
        case MTLStorageModeShared:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        case MTLStorageModeManaged:
#endif
            {
                switch (textureDimension) {
                case WGPUTextureDimension_1D: {
                    if (!size.width)
                        return;

                    auto region = MTLRegionMake1D(destination.origin.x, widthForMetal);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset + layer * bytesPerImage);
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:static_cast<const char*>(data) + sourceOffset
                            bytesPerRow:0
                            bytesPerImage:0];
                    }
                    break;
                }
                case WGPUTextureDimension_2D: {
                    if (!size.width || !size.height)
                        return;

                    auto region = MTLRegionMake2D(destination.origin.x, destination.origin.y, widthForMetal, heightForMetal);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset + layer * bytesPerImage);
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:static_cast<const char*>(data) + sourceOffset
                            bytesPerRow:bytesPerRow
                            bytesPerImage:0];
                    }
                    break;
                }
                case WGPUTextureDimension_3D: {
                    if (!size.width || !size.height || !size.depthOrArrayLayers)
                        return;

                    auto region = MTLRegionMake3D(destination.origin.x, destination.origin.y, destination.origin.z, widthForMetal, heightForMetal, depthForMetal);
                    auto sourceOffset = static_cast<NSUInteger>(dataLayout.offset);
                    [mtlTexture
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
    auto newBufferSize = static_cast<NSUInteger>(dataByteSize - dataLayout.offset);
    bool noCopy = newBufferSize >= largeBufferSize;
    id<MTLBuffer> temporaryBuffer = noCopy ? [m_device.device() newBufferWithBytesNoCopy:static_cast<char*>(data) + dataLayout.offset length:newBufferSize options:MTLResourceStorageModeShared deallocator:nil] : [m_device.device() newBufferWithBytes:static_cast<char*>(data) + dataLayout.offset length:newBufferSize options:MTLResourceStorageModeShared];
    if (!temporaryBuffer)
        return;

    switch (texture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);

        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            NSUInteger sourceOffset = layer * bytesPerImage;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:0
                sourceBytesPerImage:0
                sourceSize:sourceSize
                toTexture:mtlTexture
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
        if (!widthForMetal || !heightForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);
        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            NSUInteger sourceOffset = layer * bytesPerImage;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:bytesPerRow
                sourceBytesPerImage:0
                sourceSize:sourceSize
                toTexture:mtlTexture
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
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;

        NSUInteger sourceOffset = 0;
        [m_blitCommandEncoder
            copyFromBuffer:temporaryBuffer
            sourceOffset:sourceOffset
            sourceBytesPerRow:bytesPerRow
            sourceBytesPerImage:bytesPerImage
            sourceSize:sourceSize
            toTexture:mtlTexture
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

    if (noCopy)
        finalizeBlitCommandEncoder();
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

void wgpuQueueReference(WGPUQueue queue)
{
    WebGPU::fromAPI(queue).ref();
}

void wgpuQueueRelease(WGPUQueue queue)
{
    WebGPU::fromAPI(queue).deref();
}

void wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallback callback, void* userdata)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone([callback, userdata](WGPUQueueWorkDoneStatus status) {
        callback(status, userdata);
    });
}

void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, WGPUQueueWorkDoneBlockCallback callback)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone([callback = WebGPU::fromAPI(WTFMove(callback))](WGPUQueueWorkDoneStatus status) {
        callback(status);
    });
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands)
{
    Vector<std::reference_wrapper<WebGPU::CommandBuffer>> commandsToForward;
    for (uint32_t i = 0; i < commandCount; ++i)
        commandsToForward.append(WebGPU::fromAPI(commands[i]));
    WebGPU::fromAPI(queue).submit(WTFMove(commandsToForward));
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, void* data, size_t size)
{
    WebGPU::fromAPI(queue).writeBuffer(WebGPU::fromAPI(buffer), bufferOffset, data, size);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, void* data, size_t dataSize, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    WebGPU::fromAPI(queue).writeTexture(*destination, data, dataSize, *dataLayout, *writeSize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    WebGPU::fromAPI(queue).setLabel(WebGPU::fromAPI(label));
}

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
#import "MetalSPI.h"
#import "Texture.h"
#import "TextureView.h"
#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"
#endif
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU {

constexpr static auto largeBufferSize = 32 * 1024 * 1024;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Queue);

Queue::Queue(id<MTLCommandQueue> commandQueue, Device& device)
    : m_commandQueue(commandQueue)
    , m_device(device)
{
    m_createdNotCommittedBuffers = [NSMutableOrderedSet orderedSet];
    m_openCommandEncoders = [NSMapTable strongToStrongObjectsMapTable];
}

Queue::Queue(Device& device)
    : m_device(device)
{
}

Queue::~Queue()
{
    // We can't actually call finalizeBlitCommandEncoder() here because, if there are pending copies,
    // that would cause them to be committed, which ends up retaining this in the completed handler.
    // It's actually fine, though, because we can just drop any pending copies on the floor.
    // If the queue is being destroyed, this is unobservable.
    if (m_blitCommandEncoder)
        endEncoding(m_blitCommandEncoder, m_commandBuffer);
}

id<MTLBlitCommandEncoder> Queue::ensureBlitCommandEncoder()
{
    if (m_blitCommandEncoder && m_blitCommandEncoder == encoderForBuffer(m_commandBuffer))
        return m_blitCommandEncoder;

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    commandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    auto blitCommandBuffer = commandBufferWithDescriptor(commandBufferDescriptor);
    m_commandBuffer = blitCommandBuffer;
    m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
    setEncoderForBuffer(m_commandBuffer, m_blitCommandEncoder);
    return m_blitCommandEncoder;
}

void Queue::finalizeBlitCommandEncoder()
{
    if (m_blitCommandEncoder) {
        endEncoding(m_blitCommandEncoder, m_commandBuffer);
        commitMTLCommandBuffer(m_commandBuffer);
        m_blitCommandEncoder = nil;
        m_commandBuffer = nil;
    }
}

void Queue::endEncoding(id<MTLCommandEncoder> commandEncoder, id<MTLCommandBuffer> commandBuffer) const
{
    id<MTLCommandEncoder> currentEncoder = encoderForBuffer(commandBuffer);
    if (currentEncoder != commandEncoder)
        return;

    [currentEncoder endEncoding];
    [m_openCommandEncoders removeObjectForKey:commandBuffer];
}

id<MTLCommandEncoder> Queue::encoderForBuffer(id<MTLCommandBuffer> commandBuffer) const
{
    if (!commandBuffer)
        return nil;

    return [m_openCommandEncoders objectForKey:commandBuffer];
}

void Queue::setEncoderForBuffer(id<MTLCommandBuffer> commandBuffer, id<MTLCommandEncoder> commandEncoder)
{
    if (!commandBuffer)
        return;

    endEncoding(encoderForBuffer(commandBuffer), commandBuffer);
    if (!commandEncoder)
        [m_openCommandEncoders removeObjectForKey:commandBuffer];
    else
        [m_openCommandEncoders setObject:commandEncoder forKey:commandBuffer];
}

id<MTLCommandBuffer> Queue::commandBufferWithDescriptor(MTLCommandBufferDescriptor* descriptor)
{
    if (!isValid())
        return nil;

    constexpr auto maxCommandBufferCount = 1000;
    auto devicePtr = m_device.get();
    if (m_createdNotCommittedBuffers.count >= maxCommandBufferCount) {
        if (devicePtr)
            devicePtr->loseTheDevice(WGPUDeviceLostReason_Destroyed);
        return nil;
    }

    id<MTLCommandBuffer> buffer = [m_commandQueue commandBufferWithDescriptor:descriptor];
    if (buffer)
        [m_createdNotCommittedBuffers addObject:buffer];

    return buffer;
}

void Queue::makeInvalid()
{
    m_commandQueue = nil;
    for (auto& [_, callbackVector] : m_onSubmittedWorkScheduledCallbacks) {
        for (auto& callback : callbackVector)
            callback();
    }
    for (auto& [_, callbackVector] : m_onSubmittedWorkDoneCallbacks) {
        for (auto& callback : callbackVector)
            callback(WGPUQueueWorkDoneStatus_DeviceLost);
    }

    m_onSubmittedWorkScheduledCallbacks.clear();
    m_onSubmittedWorkDoneCallbacks.clear();

    while (m_createdNotCommittedBuffers.count)
        removeMTLCommandBuffer(m_createdNotCommittedBuffers.firstObject);

    m_createdNotCommittedBuffers = nil;
    m_openCommandEncoders = nil;
}

void Queue::onSubmittedWorkDone(CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-onsubmittedworkdone
    auto devicePtr = m_device.get();
    if (!devicePtr || !devicePtr->isValid() || devicePtr->isLost()) {
        callback(WGPUQueueWorkDoneStatus_DeviceLost);
        return;
    }

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

void Queue::onSubmittedWorkScheduled(Function<void()>&& completionHandler)
{
    ASSERT(m_submittedCommandBufferCount >= m_scheduledCommandBufferCount);
    auto devicePtr = m_device.get();
    if (!devicePtr || !devicePtr->isValid() || devicePtr->isLost()) {
        completionHandler();
        return;
    }

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

NSString* Queue::errorValidatingSubmit(const Vector<std::reference_wrapper<CommandBuffer>>& commands) const
{
    for (auto command : commands) {
        auto& commandBuffer = command.get();
        if (!isValidToUseWith(commandBuffer, *this) || commandBuffer.bufferMapCount() || commandBuffer.commandBuffer().status >= MTLCommandBufferStatusCommitted)
            return commandBuffer.lastError() ?: @"Validation failure.";
    }

    // FIXME: "Every GPUQuerySet referenced in a command in any element of commandBuffers is in the available state."
    // FIXME: "For occlusion queries, occlusionQuerySet in beginRenderPass() does not constitute a reference, while beginOcclusionQuery() does."

    // There's only one queue right now, so there is no need to make sure that the command buffers are being submitted to the correct queue.

    return nil;
}

void Queue::removeMTLCommandBuffer(id<MTLCommandBuffer> commandBuffer)
{
    if (!commandBuffer)
        return;

    id<MTLCommandEncoder> existingEncoder = encoderForBuffer(commandBuffer);
    endEncoding(existingEncoder, commandBuffer);
    removeMTLCommandBufferInternal(commandBuffer);
}

void Queue::removeMTLCommandBufferInternal(id<MTLCommandBuffer> commandBuffer)
{
    [m_openCommandEncoders removeObjectForKey:commandBuffer];
    [m_createdNotCommittedBuffers removeObject:commandBuffer];
}

void Queue::commitMTLCommandBuffer(id<MTLCommandBuffer> commandBuffer)
{
    if (!commandBuffer || commandBuffer.status >= MTLCommandBufferStatusCommitted || !isValid()) {
        removeMTLCommandBuffer(commandBuffer);
        return;
    }

    ASSERT(commandBuffer.commandQueue == m_commandQueue);
    [commandBuffer addScheduledHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer>) {
        auto device = protectedThis->m_device.get();
        if (!device || !device->device())
            return;
        protectedThis->scheduleWork([protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_scheduledCommandBufferCount);
            for (auto& callback : protectedThis->m_onSubmittedWorkScheduledCallbacks.take(protectedThis->m_scheduledCommandBufferCount))
                callback();
        });
    }];
    [commandBuffer addCompletedHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer> mtlCommandBuffer) {
        auto device = protectedThis->m_device.get();
        if (!device || !device->device())
            return;
        MTLCommandBufferStatus status = mtlCommandBuffer.status;
        bool loseTheDevice = false;
        if (NSError *error = mtlCommandBuffer.error; status != MTLCommandBufferStatusCompleted)
            loseTheDevice = !error || error.code != MTLCommandBufferErrorNotPermitted;

        protectedThis->scheduleWork([loseTheDevice, protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_completedCommandBufferCount);
            for (auto& callback : protectedThis->m_onSubmittedWorkDoneCallbacks.take(protectedThis->m_completedCommandBufferCount))
                callback(WGPUQueueWorkDoneStatus_Success);
            if (loseTheDevice) {
                auto device = protectedThis->m_device.get();
                if (device)
                    device->loseTheDevice(WGPUDeviceLostReason_Undefined);
            }
        });
    }];

    [commandBuffer commit];
    removeMTLCommandBufferInternal(commandBuffer);
    ++m_submittedCommandBufferCount;
}

static void invalidateCommandBuffers(Vector<std::reference_wrapper<CommandBuffer>>&& commands, auto&& makeInvalidFunc)
{
    for (auto commandBuffer : commands)
        makeInvalidFunc(commandBuffer.get());
}

void Queue::submit(Vector<std::reference_wrapper<CommandBuffer>>&& commands)
{
    auto device = m_device.get();
    if (!device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-submit
    if (NSString* error = errorValidatingSubmit(commands)) {
        device->generateAValidationError(error ?: @"Validation failure.");
        return invalidateCommandBuffers(WTFMove(commands), ^(CommandBuffer& command) {
            command.makeInvalid(command.lastError() ?: error);
        });
    }

    finalizeBlitCommandEncoder();

    NSMutableOrderedSet<id<MTLCommandBuffer>> *commandBuffersToSubmit = [NSMutableOrderedSet orderedSetWithCapacity:commands.size()];
    NSString* validationError = nil;
    for (auto commandBuffer : commands) {
        auto& command = commandBuffer.get();
        if (id<MTLCommandBuffer> mtlBuffer = command.commandBuffer(); mtlBuffer && ![commandBuffersToSubmit containsObject:mtlBuffer])
            [commandBuffersToSubmit addObject:mtlBuffer];
        else {
            validationError = command.lastError() ?: @"Command buffer appears twice.";
            break;
        }
    }

    invalidateCommandBuffers(WTFMove(commands), ^(CommandBuffer& command) {
        validationError ? command.makeInvalid(command.lastError() ?: validationError) : command.makeInvalidDueToCommit(@"command buffer was submitted");
    });
    if (validationError) {
        device->generateAValidationError(@"Command buffer appears twice.");
        return;
    }

    for (id<MTLCommandBuffer> commandBuffer in commandBuffersToSubmit)
        commitMTLCommandBuffer(commandBuffer);

    if ([MTLCaptureManager sharedCaptureManager].isCapturing && device->shouldStopCaptureAfterSubmit())
        [[MTLCaptureManager sharedCaptureManager] stopCapture];
}

bool Queue::validateWriteBuffer(const Buffer& buffer, uint64_t bufferOffset, size_t size) const
{
    if (!isValidToUseWith(buffer, *this))
        return false;

    auto bufferState = buffer.state();
    if (bufferState != Buffer::State::Unmapped)
        return false;

    if (!(buffer.usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (size % 4)
        return false;

    if (bufferOffset % 4)
        return false;

    auto end = checkedSum<uint64_t>(bufferOffset, size);
    if (end.hasOverflowed() || end.value() > buffer.currentSize())
        return false;

    return true;
}

void Queue::writeBuffer(Buffer& buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    auto device = m_device.get();
    if (!device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writebuffer

    if (!validateWriteBuffer(buffer, bufferOffset, data.size()) || !isValidToUseWith(buffer, *this)) {
        device->generateAValidationError("Validation failure."_s);
        return;
    }

    if (data.empty())
        return;

    if (buffer.isDestroyed()) {
        device->generateAValidationError("GPUQueue.writeBuffer: destination buffer is destroyed"_s);
        return;
    }

    buffer.indirectBufferInvalidated();
    auto bufferSpan = std::span { static_cast<uint8_t*>(buffer.buffer().contents), buffer.buffer().length };
    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    if (isIdle()) {
        switch (buffer.buffer().storageMode) {
        case MTLStorageModeShared:
            memcpySpan(bufferSpan.subspan(bufferOffset, data.size()), data);
            return;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        case MTLStorageModeManaged:
            memcpySpan(bufferSpan.subspan(bufferOffset, data.size()), data);
            [buffer.buffer() didModifyRange:NSMakeRange(bufferOffset, data.size())];
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
#if ENABLE(WEBGPU_SWIFT)
    WebGPU::writeBuffer(this, &buffer, bufferOffset, data);
#else
    writeBuffer(buffer.buffer(), bufferOffset, data);
#endif
}

void Queue::writeBuffer(id<MTLBuffer> buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    auto device = m_device.get();
    if (!device)
        return;

    ensureBlitCommandEncoder();
    // FIXME(PERFORMANCE): Suballocate, so the common case doesn't need to hit the kernel.
    // FIXME(PERFORMANCE): Should this temporary buffer really be shared?
    bool noCopy = data.size() >= largeBufferSize;
    id<MTLBuffer> temporaryBuffer = noCopy ? device->newBufferWithBytesNoCopy(data.data(), data.size(), MTLResourceStorageModeShared) : device->newBufferWithBytes(data.data(), data.size(), MTLResourceStorageModeShared);
    if (!temporaryBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    [m_blitCommandEncoder
        copyFromBuffer:temporaryBuffer
        sourceOffset:0
        toBuffer:buffer
        destinationOffset:bufferOffset
        size:data.size()];

    if (noCopy)
        finalizeBlitCommandEncoder();
}

bool Queue::isIdle() const
{
    return m_submittedCommandBufferCount == m_completedCommandBufferCount && !m_blitCommandEncoder;
}

NSString* Queue::errorValidatingWriteTexture(const WGPUImageCopyTexture& destination, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size, size_t dataByteSize, const Texture& texture) const
{
#define ERROR_STRING(x) [NSString stringWithFormat:@"GPUQueue.writeTexture: %@", x]
    if (!isValidToUseWith(texture, *this))
        return ERROR_STRING(@"destination texture is not valid");

    if (NSString* error = Texture::errorValidatingImageCopyTexture(destination, size))
        return ERROR_STRING(error);

    if (!(texture.usage() & WGPUTextureUsage_CopyDst))
        return ERROR_STRING(@"texture usage does not contain CopyDst");

    if (texture.sampleCount() != 1)
        return ERROR_STRING(@"destinationTexture sampleCount is not 1");

    if (NSString* error = Texture::errorValidatingTextureCopyRange(destination, size))
        return ERROR_STRING(error);

    if (!Texture::refersToSingleAspect(texture.format(), destination.aspect))
        return ERROR_STRING(@"refersToSingleAspect failed");

    auto aspectSpecificFormat = texture.format();

    if (Texture::isDepthOrStencilFormat(texture.format())) {
        if (!Texture::isValidDepthStencilCopyDestination(texture.format(), destination.aspect))
            return ERROR_STRING(@"isValidDepthStencilCopyDestination failed");

        aspectSpecificFormat = Texture::aspectSpecificFormat(texture.format(), destination.aspect);
    }

    if (!Texture::validateLinearTextureData(dataLayout, dataByteSize, aspectSpecificFormat, size))
        return ERROR_STRING(@"validateLinearTextureData failed");

#undef ERROR_STRING
    return nil;
}

const Device& Queue::device() const
{
    auto device = m_device.get();
    RELEASE_ASSERT(device);
    return *device;
}

void Queue::clearTextureIfNeeded(const WGPUImageCopyTexture& destination, NSUInteger slice)
{
    auto device = m_device.get();
    if (!device)
        return;

    auto& texture = fromAPI(destination.texture);
    if (texture.isDestroyed()) {
        device->generateAValidationError("GPUQueue.clearTexture: destination texture is destroyed"_s);
        return;
    }

    ensureBlitCommandEncoder();
    CommandEncoder::clearTextureIfNeeded(destination, slice, *device, m_blitCommandEncoder);
}

bool Queue::writeWillCompletelyClear(WGPUTextureDimension textureDimension, uint32_t widthForMetal, uint32_t logicalSizeWidth, uint32_t heightForMetal, uint32_t logicalSizeHeight, uint32_t depthForMetal, uint32_t logicalSizeDepthOrArrayLayers)
{
    switch (textureDimension) {
    case WGPUTextureDimension_1D:
        return widthForMetal == logicalSizeWidth;
    case WGPUTextureDimension_2D:
        return widthForMetal == logicalSizeWidth && heightForMetal == logicalSizeHeight;
    case WGPUTextureDimension_3D:
        return widthForMetal == logicalSizeWidth && heightForMetal == logicalSizeHeight && depthForMetal == logicalSizeDepthOrArrayLayers;
    case WGPUTextureDimension_Force32:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void Queue::writeTexture(const WGPUImageCopyTexture& destination, std::span<uint8_t> data, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& size, bool skipValidation)
{
    auto device = m_device.get();
    if (destination.nextInChain || dataLayout.nextInChain || !device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writetexture

    auto dataByteSize = data.size();
    auto& texture = fromAPI(destination.texture);
    if (texture.isDestroyed()) {
        device->generateAValidationError("GPUQueue.writeTexture: destination texture is destroyed"_s);
        return;
    }

    auto textureFormat = texture.format();
    if (Texture::isDepthOrStencilFormat(textureFormat)) {
        textureFormat = Texture::aspectSpecificFormat(textureFormat, destination.aspect);
        if (textureFormat == WGPUTextureFormat_Undefined) {
            device->generateAValidationError("Invalid depth-stencil format"_s);
            return;
        }
    }

    if (!skipValidation) {
        if (NSString* error = errorValidatingWriteTexture(destination, dataLayout, size, dataByteSize, texture)) {
            device->generateAValidationError(error);
            return;
        }
    }

    if (data.empty() || !dataByteSize || dataByteSize <= dataLayout.offset)
        return;

    uint32_t blockSize = Texture::texelBlockSize(textureFormat);
    auto logicalSize = texture.logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = logicalSize.width < destination.origin.x ? 0 : std::min(size.width, logicalSize.width - destination.origin.x);
    if (!widthForMetal)
        return;

    auto heightForMetal = logicalSize.height < destination.origin.y ? 0 : std::min(size.height, logicalSize.height - destination.origin.y);
    auto depthForMetal = logicalSize.depthOrArrayLayers < destination.origin.z ? 0 : std::min(size.depthOrArrayLayers, logicalSize.depthOrArrayLayers - destination.origin.z);

    NSUInteger bytesPerRow = dataLayout.bytesPerRow;
    if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        bytesPerRow = std::max<uint32_t>(size.height ? (data.size() / size.height) : data.size(), Texture::bytesPerRow(textureFormat, widthForMetal, texture.sampleCount()));

    switch (texture.dimension()) {
    case WGPUTextureDimension_1D:
        bytesPerRow = std::min<uint32_t>(bytesPerRow, blockSize * device->limits().maxTextureDimension1D);
        break;
    case WGPUTextureDimension_2D:
    case WGPUTextureDimension_3D:
        bytesPerRow = std::min<uint32_t>(bytesPerRow, blockSize * device->limits().maxTextureDimension2D);
        break;
    case WGPUTextureDimension_Force32:
        break;
    }

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
    bool clearWasNeeded = false;
    for (uint32_t layer = 0; layer < sliceCount; ++layer) {
        NSUInteger destinationSlice = textureDimension == WGPUTextureDimension_3D ? 0 : (destination.origin.z + layer);
        if (!texture.previouslyCleared(destination.mipLevel, destinationSlice)) {
            if (writeWillCompletelyClear(textureDimension, widthForMetal, logicalSize.width, heightForMetal, logicalSize.height, depthForMetal, logicalSize.depthOrArrayLayers))
                texture.setPreviouslyCleared(destination.mipLevel, destinationSlice);
            else {
                clearWasNeeded = true;
                clearTextureIfNeeded(destination, destinationSlice);
            }
        }
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

            uint32_t blockWidth = Texture::texelBlockWidth(textureFormat);
            auto widthInBlocks = blockWidth ? (widthForMetal / blockWidth) : 0;
            auto bytesInLastRow = checkedProduct<uint64_t>(blockSize, widthInBlocks);
            if (bytesInLastRow.hasOverflowed())
                return;

            for (uint32_t z = 0, endZ = std::max<uint32_t>(1, depthForMetal); z < endZ; ++z) {
                WGPUImageCopyTexture newDestination = destination;
                newDestination.origin.z = destination.origin.z + z;
                for (uint32_t y = 0, endY = textureDimension == WGPUTextureDimension_1D ? std::max<uint32_t>(1, heightForMetal) : heightForMetal; y < endY; y += newSize.height) {
                    newDestination.origin.y = destination.origin.y + y;
                    if (newDestination.origin.y + newSize.height > logicalSize.height)
                        newSize.height = static_cast<uint32_t>(newDestination.origin.y + newSize.height - logicalSize.height);

                    for (uint32_t x = 0; x < widthForMetal; x += maxRowBytes) {
                        newDestination.origin.x = destination.origin.x + x;
                        auto offset = x + y * bytesPerRow + z * bytesPerImage;
                        auto size = (y + 1 == endY) ? bytesInLastRow.value() : (bytesPerRow * newSize.height);
                        if (offset + size > data.size())
                            return;

                        writeTexture(newDestination, data.subspan(offset, size), newDataLayout, newSize);
                    }
                }
            }
            return;
        }

        bytesPerRow = 0;
        bytesPerImage = 0;
    }

    switch (textureDimension) {
    case WGPUTextureDimension_1D:
        if (!widthForMetal)
            return;
        break;
    case WGPUTextureDimension_2D:
        if (!widthForMetal || !heightForMetal)
            return;
        break;
    case WGPUTextureDimension_3D:
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;
        break;
    case WGPUTextureDimension_Force32:
        return;
    }

    Vector<uint8_t> newData;
    const auto newBytesPerRow = blockSize * ((widthForMetal / blockWidth) + ((widthForMetal % blockWidth) ? 1 : 0));
    auto dataLayoutOffset = dataLayout.offset;
    const bool widthMismatch = newBytesPerRow != bytesPerRow && widthForMetal == logicalSize.width && heightForMetal == logicalSize.height;
    const bool multipleOfBlockSize = bytesPerRow % blockSize;
    if (isCompressed && (widthMismatch || multipleOfBlockSize)) {

        auto maxY = std::max<size_t>(blockHeight, heightForMetal) / blockHeight;
        auto newBytesPerImage = newBytesPerRow * std::max<size_t>(blockHeight, logicalSize.height / blockHeight);
        auto maxZ = std::max<size_t>(1, size.depthOrArrayLayers);
        newData.resize(newBytesPerImage * maxZ);
        memset(&newData[0], 0, newData.size());
        dataLayoutOffset = 0;

        auto verticalOffset = checkedProduct<uint64_t>(maxY ? (maxY - 1) : 0, bytesPerRow);
        ASSERT(maxZ);
        auto depthOffset = checkedProduct<uint64_t>(maxZ - 1, bytesPerImage);
        auto maxResult = checkedSum<uint64_t>(verticalOffset.value(), depthOffset.value(), newBytesPerRow);
        if (verticalOffset.hasOverflowed() || depthOffset.hasOverflowed() || maxResult.hasOverflowed()) {
            device->generateAValidationError("Result overflows uin64_t"_s);
            return;
        }

        if (maxY) {
            if ((maxY - 1) * newBytesPerRow + (maxZ - 1) * newBytesPerImage + newBytesPerRow > newData.size()
                || (maxY - 1) * bytesPerRow + (maxZ - 1) * bytesPerImage + dataLayout.offset + newBytesPerRow > data.size()) {
                auto y = (maxY - 1);
                auto z = (maxZ - 1);
                device->generateAValidationError([NSString stringWithFormat:@"y(%zu) * newBytesPerRow(%u) + z(%zu) * newBytesPerImage(%lu) + newBytesPerRow(%u) > newData.size()(%zu) || y(%zu) * bytesPerRow(%lu) + z(%zu) * bytesPerImage(%lu) + newBytesPerRow(%u) > dataSize(%zu), copySize %u, %u, %u, textureSize %u, %u, %u, offset %llu", y, newBytesPerRow, z, newBytesPerImage, newBytesPerRow, newData.size(), y, static_cast<unsigned long>(bytesPerRow), z, static_cast<unsigned long>(bytesPerImage), newBytesPerRow, data.size(), widthForMetal, heightForMetal, depthForMetal, logicalSize.width, logicalSize.height, logicalSize.depthOrArrayLayers, dataLayout.offset]);
                return;
            }
        }

        auto newDataSpan = newData.mutableSpan();
        for (size_t z = 0; z < maxZ; ++z) {
            for (size_t y = 0; y < maxY; ++y) {
                auto sourceBytesSpan = data.subspan(y * bytesPerRow + z * bytesPerImage + dataLayout.offset, newBytesPerRow);
                auto destBytesSpan = newDataSpan.subspan(y * newBytesPerRow + z * newBytesPerImage, newBytesPerRow);
                memcpySpan(destBytesSpan, sourceBytesSpan);
            }
        }

        bytesPerRow = newBytesPerRow;
        dataByteSize = newData.size();
        bytesPerImage = newBytesPerImage;
        data = newData.mutableSpan();
    }

    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    if (isIdle() && options == MTLBlitOptionNone && !clearWasNeeded) {
        switch (mtlTexture.storageMode) {
        case MTLStorageModeShared:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        case MTLStorageModeManaged:
#endif
            {
                switch (textureDimension) {
                case WGPUTextureDimension_1D: {
                    if (!widthForMetal)
                        return;

                    auto region = MTLRegionMake1D(destination.origin.x, widthForMetal);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayoutOffset + layer * bytesPerImage);
                        if (sourceOffset % blockSize)
                            continue;
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:byteCast<char>(data.data()) + sourceOffset
                            bytesPerRow:0
                            bytesPerImage:0];
                    }
                    break;
                }
                case WGPUTextureDimension_2D: {
                    if (!widthForMetal || !heightForMetal)
                        return;

                    auto region = MTLRegionMake2D(destination.origin.x, destination.origin.y, widthForMetal, heightForMetal);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto sourceOffset = static_cast<NSUInteger>(dataLayoutOffset + layer * bytesPerImage);
                        if (sourceOffset % blockSize)
                            continue;
                        NSUInteger destinationSlice = destination.origin.z + layer;
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:byteCast<char>(data.data()) + sourceOffset
                            bytesPerRow:bytesPerRow
                            bytesPerImage:0];
                    }
                    break;
                }
                case WGPUTextureDimension_3D: {
                    if (!widthForMetal || !heightForMetal || !depthForMetal)
                        return;

                    auto region = MTLRegionMake3D(destination.origin.x, destination.origin.y, destination.origin.z, widthForMetal, heightForMetal, depthForMetal);
                    auto sourceOffset = static_cast<NSUInteger>(dataLayoutOffset);
                    if (sourceOffset % blockSize)
                        break;
                    [mtlTexture
                        replaceRegion:region
                        mipmapLevel:destination.mipLevel
                        slice:0
                        withBytes:byteCast<char>(data.data()) + sourceOffset
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
    NSUInteger newBufferSize = dataByteSize - dataLayoutOffset;
    bool noCopy = newBufferSize >= largeBufferSize;
    id<MTLBuffer> temporaryBuffer = noCopy ? device->newBufferWithBytesNoCopy(byteCast<char>(data.data()) + dataLayoutOffset, static_cast<NSUInteger>(newBufferSize), MTLResourceStorageModeShared) : device->newBufferWithBytes(byteCast<char>(data.data()) + dataLayoutOffset, static_cast<NSUInteger>(newBufferSize), MTLResourceStorageModeShared);
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
            if (sourceOffset + widthForMetal * blockSize > temporaryBuffer.length)
                continue;
            if (sourceOffset % blockSize)
                continue;

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
        if (!widthForMetal || !heightForMetal || bytesPerRow < Texture::bytesPerRow(textureFormat, widthForMetal, texture.sampleCount()))
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);
        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            NSUInteger sourceOffset = layer * bytesPerImage;
            NSUInteger destinationSlice = destination.origin.z + layer;
            if (sourceOffset % blockSize)
                continue;
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
        if (!widthForMetal || !heightForMetal || !depthForMetal || bytesPerRow < Texture::bytesPerRow(textureFormat, widthForMetal, texture.sampleCount()))
            return;

        [m_blitCommandEncoder
            copyFromBuffer:temporaryBuffer
            sourceOffset:0
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
    auto device = m_device.get();
    if (!device)
        return;

    if (auto inst = device->instance(); inst.get())
        inst->scheduleWork(WTFMove(workItem));
}

void Queue::clearTextureViewIfNeeded(TextureView& textureView)
{
    auto devicePtr = m_device.get();
    if (!devicePtr)
        return;

    auto& parentTexture = textureView.apiParentTexture();
    for (uint32_t slice = 0; slice < textureView.arrayLayerCount(); ++slice) {
        for (uint32_t mipLevel = 0; mipLevel < textureView.mipLevelCount(); ++mipLevel) {
            auto parentMipLevel = textureView.baseMipLevel() + mipLevel;
            auto parentSlice = textureView.baseArrayLayer() + slice;
            if (parentTexture.previouslyCleared(parentMipLevel, parentSlice))
                continue;

            CommandEncoder::clearTextureIfNeeded(parentTexture, parentMipLevel, parentSlice, *devicePtr, ensureBlitCommandEncoder());
        }
    }
    finalizeBlitCommandEncoder();
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

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    WebGPU::fromAPI(queue).writeBuffer(WebGPU::fromAPI(buffer), bufferOffset, data);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, std::span<uint8_t> data, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    WebGPU::fromAPI(queue).writeTexture(*destination, data, *dataLayout, *writeSize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    WebGPU::fromAPI(queue).setLabel(WebGPU::fromAPI(label));
}

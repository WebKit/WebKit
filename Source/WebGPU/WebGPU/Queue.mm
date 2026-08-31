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
#import <simd/simd.h>
#import <wtf/Borrow.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBGPU_SWIFT)
#import "CxxBridging.h"
#import <WebGPU/CxxBridgingPublic.h>
#import <WebGPU/WGPUTextureImpl.h>
#import "WebGPUSwift-Generated.h"
#endif

namespace WebGPU {

constexpr static auto largeBufferSize = WGPU_LARGE_BUFFER_SIZE;
constexpr bool skipMemoryAttribution = true;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Queue);

Queue::Queue(id<MTLCommandQueue> commandQueue, Adapter& adapter, Device& device)
    : m_commandQueue(commandQueue)
    , m_device(device)
    , m_instance(adapter.weakInstance())
{
    m_createdNotCommittedBuffers = [NSMutableOrderedSet orderedSet];
    m_openCommandEncoders = [NSMapTable strongToStrongObjectsMapTable];
    m_retainedCounterSampleBuffers = [NSMutableDictionary dictionary];
}

Queue::Queue(Adapter& adapter, Device& device)
    : m_device(device)
    , m_instance(adapter.weakInstance())
{
}

Queue::~Queue()
{
    // We can't actually call finalizeBlitCommandEncoder() here because, if there are pending copies,
    // that would cause them to be committed, which ends up retaining this in the completed handler.
    // It's actually fine, though, because we can just drop any pending copies on the floor.
    // If the queue is being destroyed, this is unobservable.
    id<MTLCommandEncoder> stagingEncoder = m_blitCommandEncoder;
    if (!stagingEncoder)
        stagingEncoder = m_stagedCopyEncoder;
    if (stagingEncoder)
        endEncoding(stagingEncoder, m_commandBuffer);
}

id<MTLBlitCommandEncoder> Queue::ensureBlitCommandEncoder()
{
    if (m_blitCommandEncoder && m_blitCommandEncoder == encoderForBuffer(m_commandBuffer))
        return m_blitCommandEncoder;

    if (m_stagedCopyEncoder && m_stagedCopyEncoder == encoderForBuffer(m_commandBuffer)) {
        // Switch encoder types on the open staging command buffer rather than minting a second one.
        endEncoding(m_stagedCopyEncoder, m_commandBuffer);
        m_stagedCopyEncoder = nil;
        m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
        if (!m_blitCommandEncoder) {
            // See ensureStagedCopyEncoder(): committing beats orphaning the work already encoded.
            commitMTLCommandBuffer(m_commandBuffer);
            m_commandBuffer = nil;
            return nil;
        }
        setEncoderForBuffer(m_commandBuffer, m_blitCommandEncoder);
        return m_blitCommandEncoder;
    }

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    auto blitCommandBuffer = commandBufferWithDescriptor(commandBufferDescriptor);
    m_commandBuffer = blitCommandBuffer;
    m_stagedCopyEncoder = nil;
    m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
    setEncoderForBuffer(m_commandBuffer, m_blitCommandEncoder);
    return m_blitCommandEncoder;
}

id<MTLComputeCommandEncoder> Queue::ensureStagedCopyEncoder()
{
    if (m_stagedCopyEncoder && m_stagedCopyEncoder == encoderForBuffer(m_commandBuffer))
        return m_stagedCopyEncoder;

    if (m_blitCommandEncoder && m_blitCommandEncoder == encoderForBuffer(m_commandBuffer)) {
        // Switch encoder types on the open staging command buffer rather than minting a second one.
        endEncoding(m_blitCommandEncoder, m_commandBuffer);
        m_blitCommandEncoder = nil;
        m_stagedCopyEncoder = [m_commandBuffer computeCommandEncoder];
        if (!m_stagedCopyEncoder) {
            // Encoder creation failed after the previous encoder was ended, so the staging command
            // buffer now holds real work with nothing open on it. finalizeBlitCommandEncoder() keys
            // off the encoders, so leaving it here would orphan it and drop that work; commit it.
            commitMTLCommandBuffer(m_commandBuffer);
            m_commandBuffer = nil;
            return nil;
        }
        setEncoderForBuffer(m_commandBuffer, m_stagedCopyEncoder);
        return m_stagedCopyEncoder;
    }

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    auto stagingCommandBuffer = commandBufferWithDescriptor(commandBufferDescriptor);
    m_commandBuffer = stagingCommandBuffer;
    m_blitCommandEncoder = nil;
    m_stagedCopyEncoder = [m_commandBuffer computeCommandEncoder];
    setEncoderForBuffer(m_commandBuffer, m_stagedCopyEncoder);
    return m_stagedCopyEncoder;
}

void Queue::finalizeBlitCommandEncoder()
{
    // Finalizes the queue-owned staging command buffer, whichever encoder type is
    // currently open on it (blit, or the compute staged-copy encoder).
    if (m_blitCommandEncoder || m_stagedCopyEncoder) {
        id<MTLCommandEncoder> stagingEncoder = m_blitCommandEncoder;
        if (!stagingEncoder)
            stagingEncoder = m_stagedCopyEncoder;
        endEncoding(stagingEncoder, m_commandBuffer);
        commitMTLCommandBuffer(m_commandBuffer);
        m_blitCommandEncoder = nil;
        m_stagedCopyEncoder = nil;
        m_commandBuffer = nil;
    } else if (m_commandBuffer) {
        // A staging command buffer with no encoder open: reachable if encoder creation failed. Its
        // already-encoded work must still reach the GPU in order, so commit rather than drop.
        commitMTLCommandBuffer(m_commandBuffer);
        m_commandBuffer = nil;
    }
}

// Host mirror of the MSL `WebKitStagedCopyArgs` declared in the kernel source below. The two
// declarations are adjacent deliberately: they are a hand-maintained ABI, and getting the field
// widths wrong produces silently wrong offsets rather than a compile error. MSL `ulong` is 64-bit,
// so the static_asserts below pin size and every offset.
struct StagedCopyArguments {
    uint64_t sourceWordOffset;
    uint64_t destinationWordOffset;
    uint64_t wordCount;
};
static_assert(sizeof(StagedCopyArguments) == 24, "StagedCopyArguments must match MSL WebKitStagedCopyArgs (3 x ulong)");
static_assert(!offsetof(StagedCopyArguments, sourceWordOffset), "sourceWordOffset must be at offset 0");
static_assert(offsetof(StagedCopyArguments, destinationWordOffset) == 8, "destinationWordOffset must be at offset 8");
static_assert(offsetof(StagedCopyArguments, wordCount) == 16, "wordCount must be at offset 16");

id<MTLComputePipelineState> Queue::stagedCopyPipelineState()
{
    if (m_stagedCopyPipelineState || m_stagedCopyPipelineCreationFailed)
        return m_stagedCopyPipelineState;

    auto device = m_device.get();
    id<MTLDevice> mtlDevice = device ? device->device() : nil;
    if (!mtlDevice)
        return nil;

    NSError *error = nil;
    /* NOLINT */ id<MTLLibrary> library = [mtlDevice newLibraryWithSource:@R"(#include <metal_stdlib>
using namespace metal;
struct WebKitStagedCopyArgs {
    ulong sourceWordOffset;
    ulong destinationWordOffset;
    ulong wordCount;
};
[[kernel]] void csStagedBufferCopy(device const uint* source [[buffer(0)]], device uint* destination [[buffer(1)]], constant WebKitStagedCopyArgs& args [[buffer(2)]], uint threadIndex [[thread_position_in_grid]], uint threadsPerGrid [[threads_per_grid]])
{
    for (ulong i = threadIndex; i < args.wordCount; i += threadsPerGrid)
        destination[args.destinationWordOffset + i] = source[args.sourceWordOffset + i];
})" options:nil error:&error];
    id<MTLFunction> function = error ? nil : [library newFunctionWithName:@"csStagedBufferCopy"];
    if (function)
        m_stagedCopyPipelineState = [mtlDevice newComputePipelineStateWithFunction:function error:&error];
    if (!m_stagedCopyPipelineState) {
        m_stagedCopyPipelineCreationFailed = true;
        WTFLogAlways("WebGPU: staged-copy compute pipeline creation failed (%@); staged writes will fall back to blit", error); // NOLINT
    }
    return m_stagedCopyPipelineState;
}

void Queue::endEncoding(id<MTLCommandEncoder> commandEncoder, id<MTLCommandBuffer> commandBuffer) const
{
    id<MTLCommandEncoder> currentEncoder = encoderForBuffer(commandBuffer);
    if (!currentEncoder || currentEncoder != commandEncoder)
        return;

    [currentEncoder endEncoding];
    if (RefPtr device = m_device.get())
        device->resolveTimestampsForBuffer(commandBuffer);
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

    if (auto instance = m_instance.get()) {
        if (auto device = m_device.get())
            instance->retainDevice(*device, buffer);
    }
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
        scheduleWork([callback = WTF::move(callback)]() mutable {
            callback(WGPUQueueWorkDoneStatus_Success);
        });
        return;
    }

    auto& callbacks = m_onSubmittedWorkDoneCallbacks.add(m_submittedCommandBufferCount, OnSubmittedWorkDoneCallbacks()).iterator->value;
    callbacks.append(WTF::move(callback));
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
        scheduleWork([completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
        return;
    }

    auto& callbacks = m_onSubmittedWorkScheduledCallbacks.add(m_submittedCommandBufferCount, OnSubmittedWorkScheduledCallbacks()).iterator->value;
    callbacks.append(WTF::move(completionHandler));
}

NSString* Queue::errorValidatingSubmit(const Vector<Ref<WebGPU::CommandBuffer>>& commands) const
{
    for (Ref command : commands) {
        if (!isValidToUseWith(command.get(), *this) || command->bufferMapCount() || command->commandBuffer().status >= MTLCommandBufferStatusCommitted)
            return command->lastError() ?: @"Validation failure.";
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

void Queue::waitForAllCommitedWorkToComplete()
{
    id<MTLCommandBuffer> commandBuffer = nil;
    do {
        {
            Locker locker { m_committedNotCompletedBuffersLock };
            commandBuffer = m_committedNotCompletedBuffers.firstObject;
        }
        [commandBuffer waitUntilCompleted];
    } while (commandBuffer);
}

template<unsigned errorCode>
[[noreturn]] void crashGPUProcess(NSError* error, NSError* underlyingError)
{
    WTFLogAlways("Encountered fatal command buffer error %@, underlying error %@", error, underlyingError); // NOLINT
    RELEASE_ASSERT_NOT_REACHED();
}

void Queue::commitMTLCommandBuffer(id<MTLCommandBuffer> commandBuffer)
{
    if (!commandBuffer || commandBuffer.status >= MTLCommandBufferStatusCommitted || !isValid()) {
        removeMTLCommandBuffer(commandBuffer);
        return;
    }

    ASSERT(commandBuffer.commandQueue == m_commandQueue);
    [commandBuffer addScheduledHandler:[protectedThis = protect(*this)](id<MTLCommandBuffer>) {
        protectedThis->scheduleWork([protectedThis = protectedThis.copyRef()]() {
            ++(protectedThis->m_scheduledCommandBufferCount);
            for (auto& callback : protectedThis->m_onSubmittedWorkScheduledCallbacks.take(protectedThis->m_scheduledCommandBufferCount))
                callback();
        });
    }];
    [commandBuffer addCompletedHandler:[protectedThis = protect(*this)](id<MTLCommandBuffer> mtlCommandBuffer) {
        MTLCommandBufferStatus status = mtlCommandBuffer.status;
        bool loseTheDevice = false;
        if (NSError *error = mtlCommandBuffer.error; status != MTLCommandBufferStatusCompleted) {
            loseTheDevice = !error || error.code != MTLCommandBufferErrorNotPermitted;
            if (loseTheDevice) {
                NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
                if (underlyingError.code == 0x10a || underlyingError.code == 0x5)
                    loseTheDevice = false;
                else {
#define makeCase(N) case N: crashGPUProcess<N>(error, underlyingError);
                    switch (underlyingError.code) {
                        makeCase(8); // kIOGPUCommandBufferCallbackErrorOutOfMemory = 8,
                        makeCase(9); // kIOGPUCommandBufferCallbackErrorInvalidResource = 9,
                        makeCase(10); // kIOGPUCommandBufferCallbackErrorInvalidInput = 10,
                        makeCase(11); // kIOGPUCommandBufferCallbackErrorPageFault = 11,
                        makeCase(16); // kIOGPUCommandBufferCallbackErrorProtectionViolation = 16,
                        makeCase(17); // kIOGPUCommandBufferCallbackErrorStackOverflow = 17,
                        break;
                    default:
                        break;
                    }
#undef makeCase
                    WTFLogAlways("Encountered non-fatal command buffer error %@, underlying error %@", error, underlyingError); // NOLINT
                }
            }
        }

        {
            Locker locker { protectedThis->m_committedNotCompletedBuffersLock };
            [protectedThis->m_committedNotCompletedBuffers removeObject:mtlCommandBuffer];
        }

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
    {
        Locker locker { m_committedNotCompletedBuffersLock };
        [m_committedNotCompletedBuffers addObject:commandBuffer];
    }
    removeMTLCommandBufferInternal(commandBuffer);
    ++m_submittedCommandBufferCount;
}

static void invalidateCommandBuffers(Vector<Ref<WebGPU::CommandBuffer>>&& commands, auto&& makeInvalidFunc)
{
    for (auto commandBuffer : commands)
        makeInvalidFunc(commandBuffer.get());
}

void Queue::submit(Vector<Ref<WebGPU::CommandBuffer>>&& commands)
{
    auto device = m_device.get();
    if (!device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-submit
    if (NSString* error = errorValidatingSubmit(commands)) {
        device->generateAValidationError(error ?: @"Validation failure.");
        return invalidateCommandBuffers(WTF::move(commands), ^(CommandBuffer& command) {
            command.makeInvalid(command.lastError() ?: error);
        });
    }

    finalizeBlitCommandEncoder();

    NSMutableOrderedSet<id<MTLCommandBuffer>> *commandBuffersToSubmit = [NSMutableOrderedSet orderedSetWithCapacity:commands.size()];
    HashMap<void*, Ref<CommandBuffer>> metalCommandBuffersReverseMap;
    NSString* validationError = nil;
    for (Ref command : commands) {
        if (id<MTLCommandBuffer> mtlBuffer = command->commandBuffer(); mtlBuffer && ![commandBuffersToSubmit containsObject:mtlBuffer]) {
            [commandBuffersToSubmit addObject:mtlBuffer];
            metalCommandBuffersReverseMap.set((__bridge void*)mtlBuffer, WTF::move(command));
        } else {
            validationError = command->lastError() ?: @"Command buffer appears twice.";
            break;
        }
    }

    invalidateCommandBuffers(WTF::move(commands), ^(CommandBuffer& command) {
        validationError ? command.makeInvalid(command.lastError() ?: validationError) : command.makeInvalidDueToCommit(@"command buffer was submitted");
    });
    if (validationError) {
        device->generateAValidationError(@"Command buffer appears twice.");
        return;
    }

    for (id<MTLCommandBuffer> commandBuffer in commandBuffersToSubmit) {
        RefPtr apiCommandBuffer = metalCommandBuffersReverseMap.get((__bridge void*)commandBuffer);
#if ASSERT_ENABLED
        if (!apiCommandBuffer)
            ASSERT_NOT_REACHED("Always expect command buffer in the container");
#endif
        apiCommandBuffer->preCommitHandler();
        commitMTLCommandBuffer(commandBuffer);
        apiCommandBuffer->postCommitHandler();
    }

    if ([MTLCaptureManager sharedCaptureManager].isCapturing && device->shouldStopCaptureAfterSubmit())
        [[MTLCaptureManager sharedCaptureManager] stopCapture];
}

uint64_t Queue::retainCounterSampleBuffer(CommandEncoder& encoder)
{
    auto encoderHandle = encoder.uniqueId();
    [m_retainedCounterSampleBuffers setObject:encoder.timestampBuffers() forKey:[NSNumber numberWithUnsignedLongLong:encoderHandle]];
    return encoderHandle;
}

void Queue::releaseCounterSampleBuffer(uint64_t encoderHandle)
{
    scheduleWork([protectedThis = protect(*this), encoderHandle]() {
        [protectedThis->m_retainedCounterSampleBuffers removeObjectForKey:[NSNumber numberWithUnsignedLongLong:encoderHandle]];
    });
}

void Queue::retainTimestampsForOneUpdate(NSMutableSet<id<MTLCounterSampleBuffer>> *timestamps)
{
    // Workaround for rdar://143905417
    if (!timestamps)
        return;

    scheduleWork([protectedThis = protect(*this), timestamps]() {
        UNUSED_PARAM(timestamps);
    });
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

void Queue::synchronizeResourceAndWait(id<MTLBuffer> buffer)
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (buffer.storageMode != MTLStorageModeManaged)
        return;

    ensureBlitCommandEncoder();
    [m_blitCommandEncoder synchronizeResource:buffer];
    ALLOW_DEPRECATED_DECLARATIONS_END
    id<MTLCommandBuffer> commandBuffer = m_commandBuffer;
    finalizeBlitCommandEncoder();
    [commandBuffer waitUntilCompleted];
#else
    UNUSED_PARAM(buffer);
#endif
}

id<MTLIndirectCommandBuffer> Queue::trimICB(id<MTLIndirectCommandBuffer> dest, id<MTLIndirectCommandBuffer> src, NSUInteger newSize)
{
    ensureBlitCommandEncoder();
    [m_blitCommandEncoder copyIndirectCommandBuffer:src sourceRange:NSMakeRange(0, newSize) destination:dest destinationIndex:0];

    return dest;
}

static std::pair<uint32_t, uint16_t> NODELETE maxIndexValueSlow(std::span<uint8_t> data)
{
    auto lengthUint32 = data.size() / 4;
    std::span<uint32_t> dataUint = unsafeMakeSpan(static_cast<uint32_t*>(static_cast<void*>(data.data())), lengthUint32);
    std::span<uint16_t> dataUshort = unsafeMakeSpan(static_cast<uint16_t*>(static_cast<void*>(data.data())), lengthUint32 * 2);
    uint32_t maxValue = 0;
    for (uint32_t dataUintV : dataUint) {
        if (maxValue < dataUintV)
            maxValue = dataUintV;
    }
    uint16_t maxUshort = 0;
    for (uint16_t dataUshortV : dataUshort) {
        if (maxUshort < dataUshortV)
            maxUshort = dataUshortV;
    }
    return std::make_pair(maxValue, maxUshort);
}

static std::pair<uint32_t, uint16_t> maxIndexValue(std::span<uint8_t> data)
{
    constexpr auto blockSize = 64;
    auto divResult = std::div(data.size(), blockSize);
    auto lengthUint32 = divResult.quot;
    if (!lengthUint32 || reinterpret_cast<uint64_t>(data.data()) % 64)
        return maxIndexValueSlow(data);

    std::span<simd::uint16> dataUint = unsafeMakeSpan(static_cast<simd::uint16*>(static_cast<void*>(data.data())), lengthUint32);
    std::span<simd::ushort32> dataUshort = unsafeMakeSpan(static_cast<simd::ushort32*>(static_cast<void*>(data.data())), lengthUint32);
    simd::uint16 maxValue = dataUint.front();
    simd::ushort32 maxUshort = dataUshort.front();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpsabi"
    for (auto dataUintV : dataUint)
        maxValue = simd_max(maxValue, dataUintV);
    for (auto dataUshortV : dataUshort)
        maxUshort = simd_max(maxUshort, dataUshortV);

    auto result = std::make_pair(simd_reduce_max(maxValue), simd_reduce_max(maxUshort));
#pragma clang diagnostic pop

    if (divResult.rem) {
        auto slowResult = maxIndexValueSlow(data.subspan(blockSize * divResult.quot));
        result.first = std::max(result.first, slowResult.first);
        result.second = std::max(result.second, slowResult.second);
    }
    return result;
}

void Queue::writeBuffer(Buffer& buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    auto device = m_device.get();
    if (!device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writebuffer

    auto dataSize = data.size();
    if (!validateWriteBuffer(buffer, bufferOffset, dataSize) || !isValidToUseWith(buffer, *this)) {
        device->generateAValidationError("Validation failure."_s);
        return;
    }

    if (data.empty())
        return;

    if (buffer.isDestroyed()) {
        device->generateAValidationError("GPUQueue.writeBuffer: destination buffer is destroyed"_s);
        return;
    }

    // FIXME(PERFORMANCE): Instead of checking whether or not the whole queue is idle,
    // we could detect whether this specific resource is idle, if we tracked every resource.
    bool needsInvalidation = true;

    if (dataSize < 16*KB && (buffer.usage() & WGPUBufferUsage_Index) && !(buffer.usage() & WGPUBufferUsage_Indirect)) {
        auto maxUnsignedUshortValue = maxIndexValue(data);
        if (!buffer.needsIndexValidation(maxUnsignedUshortValue.first, maxUnsignedUshortValue.second)) {
            needsInvalidation = false;
            buffer.indexBufferContentsModified(maxUnsignedUshortValue.first, maxUnsignedUshortValue.second);
        }
    }
    if (needsInvalidation)
        buffer.indirectBufferInvalidated();
    if (isIdle()) {
        switch (buffer.buffer().storageMode) {
        case MTLStorageModeShared:
            SUPPRESS_UNCOUNTED_ARG memcpySpan(borrow(buffer)->getBufferContents().subspan(bufferOffset, data.size()), data);
            return;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        case MTLStorageModeManaged:
            SUPPRESS_UNCOUNTED_ARG memcpySpan(borrow(buffer)->getBufferContents().subspan(bufferOffset, data.size()), data);
            [buffer.buffer() didModifyRange:NSMakeRange(bufferOffset, data.size())];
            return;
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
        case MTLStorageModePrivate:
            // The only way to get data into a private resource is to tell the GPU to copy it in.
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    }
    writeBuffer(buffer.buffer(), bufferOffset, data);
}

static std::span<uint8_t> span(id<MTLBuffer> buffer)
{
    return unsafeMakeSpan(static_cast<uint8_t*>(buffer.contents), buffer.length);
}

std::pair<id<MTLBuffer>, uint64_t> Queue::newTemporaryBufferWithBytes(std::span<uint8_t> dataSpan, bool noCopy)
{
    auto device = m_device.get();
    if (!device)
        return std::make_pair(nil, 0ull);

    auto dataSize = dataSpan.size();
    auto data = dataSpan.data();
    if (noCopy)
        return std::make_pair(device->newBufferWithBytesNoCopy(data, dataSize, MTLResourceStorageModeShared, skipMemoryAttribution), 0ull);

    if (!m_temporaryBuffer || m_temporaryBufferOffset + dataSize > m_temporaryBuffer.length) {
        m_temporaryBuffer = device->safeCreateBuffer(std::max(dataSize, 64 * KB), skipMemoryAttribution);
        m_temporaryBufferOffset = 0;
    }

    auto priorOffset = m_temporaryBufferOffset;
    m_temporaryBufferOffset += WTF::roundUpToMultipleOf(64, dataSize);
    memcpySpan(span(m_temporaryBuffer).subspan(priorOffset), dataSpan);
    return std::make_pair(m_temporaryBuffer, priorOffset);
}

void Queue::writeBuffer(id<MTLBuffer> buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
#if ENABLE(WEBGPU_SWIFT)
    if (isWebGPUSwiftEnabled()) {
        queueWriteBuffer(this, buffer, bufferOffset, data);
        return;
    }
#endif

    stageBufferWrite(buffer, bufferOffset, data);
}

void Queue::stageBufferWrite(id<MTLBuffer> buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    auto device = m_device.get();
    if (!device || !buffer || data.empty())
        return;

    bool noCopy = data.size() >= largeBufferSize;
    auto bufferWithOffset = newTemporaryBufferWithBytes(data, noCopy);
    id<MTLBuffer> temporaryBuffer = bufferWithOffset.first;
    uint64_t temporaryBufferOffset = bufferWithOffset.second;
    if (!temporaryBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    encodeStagedCopy(temporaryBuffer, temporaryBufferOffset, buffer, bufferOffset, data.size(), noCopy);
}

// Encodes an already-staged copy, choosing the channel it rides. Split out from stageBufferWrite so
// the Swift entry point can share the channel decision without converting spans across the language
// boundary (Queue.swift).
//
// Encode the staged copy on the compute channel when possible. Standalone blit-only staging command
// buffers interleaved with user compute submissions intermittently deadlock the AGX blit/DMA channel
// (the driver reports kIOGPUCommandBufferCallbackErrorHang on the staging command buffer and
// discards neighboring command buffers as innocent victims), losing the device. Encoding the same
// copies as compute dispatches keeps the commit points, queue ordering, and hazard tracking
// identical while keeping the per-frame hot path off the blit channel entirely. GPUQueue.writeBuffer
// validation guarantees 4-byte alignment of both offset and size, and the staging suballocator is
// 64-byte aligned, so the word-copy kernel covers every spec-reachable write; the blit encoder
// remains as the fallback (and for writeTexture, clearBuffer, and internal copies).
void Queue::encodeStagedCopy(id<MTLBuffer> temporaryBuffer, uint64_t temporaryBufferOffset, id<MTLBuffer> buffer, uint64_t bufferOffset, uint64_t size, bool finalizeAfterCopy)
{
    if (!temporaryBuffer || !buffer || !size)
        return;

    // The copy kernel's loop is bounded by wordCount alone, so a bad range would write out of
    // bounds rather than fault. Check both sides here, by subtraction so the sums cannot overflow.
    if (bufferOffset > buffer.length || size > buffer.length - bufferOffset)
        return;
    if (temporaryBufferOffset > temporaryBuffer.length || size > temporaryBuffer.length - temporaryBufferOffset)
        return;

    bool wordAligned = !(bufferOffset % sizeof(uint32_t)) && !(size % sizeof(uint32_t)) && !(temporaryBufferOffset % sizeof(uint32_t));
    id<MTLComputePipelineState> pipelineState = wordAligned ? stagedCopyPipelineState() : nil;
    // A null encoder here means creation failed, not that compute is unavailable; fall through to
    // the blit encoder rather than dropping the copy, which would leave the destination stale.
    id<MTLComputeCommandEncoder> stagedCopyEncoder = pipelineState ? ensureStagedCopyEncoder() : nil;
    if (stagedCopyEncoder) {
        StagedCopyArguments args {
            temporaryBufferOffset / sizeof(uint32_t),
            bufferOffset / sizeof(uint32_t),
            size / sizeof(uint32_t)
        };
        [stagedCopyEncoder setComputePipelineState:pipelineState];
        [stagedCopyEncoder setBuffer:temporaryBuffer offset:0 atIndex:0];
        [stagedCopyEncoder setBuffer:buffer offset:0 atIndex:1];
        [stagedCopyEncoder setBytes:&args length:sizeof(args) atIndex:2];
        auto threadsPerThreadgroup = std::min<uint64_t>(256, pipelineState.maxTotalThreadsPerThreadgroup);
        auto threadgroups = std::min<uint64_t>((args.wordCount + threadsPerThreadgroup - 1) / threadsPerThreadgroup, 2048);
        [stagedCopyEncoder dispatchThreadgroups:MTLSizeMake(static_cast<NSUInteger>(threadgroups), 1, 1) threadsPerThreadgroup:MTLSizeMake(static_cast<NSUInteger>(threadsPerThreadgroup), 1, 1)];

        if (finalizeAfterCopy)
            finalizeBlitCommandEncoder();
        return;
    }

    ensureBlitCommandEncoder();
    if (!m_blitCommandEncoder)
        return;

    [m_blitCommandEncoder
        copyFromBuffer:temporaryBuffer
        sourceOffset:temporaryBufferOffset
        toBuffer:buffer
        destinationOffset:bufferOffset
        size:size];

    if (finalizeAfterCopy)
        finalizeBlitCommandEncoder();
}

void Queue::clearBuffer(id<MTLBuffer> buffer, NSUInteger offset, NSUInteger size)
{
    if (offset > buffer.length)
        return;

    ensureBlitCommandEncoder();
    auto lengthMinusOffset = buffer.length - offset;
    [m_blitCommandEncoder fillBuffer:buffer range:NSMakeRange(offset, std::min<NSUInteger>(lengthMinusOffset, size)) value:0];
}

bool Queue::isIdle() const
{
    return m_submittedCommandBufferCount == m_completedCommandBufferCount && !m_blitCommandEncoder && !m_stagedCopyEncoder;
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

    if (NSString* errorString = Texture::errorValidatingLinearTextureData(dataLayout, dataByteSize, aspectSpecificFormat, size))
        return ERROR_STRING(errorString);

#undef ERROR_STRING
    return nil;
}

const Device& Queue::device() const
{
    auto device = m_device.get();
    RELEASE_ASSERT(device);
    return *device.unsafeGet();
}

void Queue::clearTextureIfNeeded(const WGPUImageCopyTexture& destination, NSUInteger slice)
{
    auto device = m_device.get();
    if (!device)
        return;

    Ref texture = fromAPI(destination.texture);
    if (texture->isDestroyed()) {
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
    if (!device)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-writetexture

    auto dataByteSize = data.size();
    Ref texture = fromAPI(destination.texture);
    if (texture->isDestroyed()) {
        device->generateAValidationError("GPUQueue.writeTexture: destination texture is destroyed"_s);
        return;
    }

    auto textureFormat = texture->format();
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
    auto logicalSize = texture->logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = logicalSize.width < destination.origin.x ? 0 : std::min(size.width, logicalSize.width - destination.origin.x);
    if (!widthForMetal)
        return;

    auto heightForMetal = logicalSize.height < destination.origin.y ? 0 : std::min(size.height, logicalSize.height - destination.origin.y);
    auto depthForMetal = logicalSize.depthOrArrayLayers < destination.origin.z ? 0 : std::min(size.depthOrArrayLayers, logicalSize.depthOrArrayLayers - destination.origin.z);

    NSUInteger bytesPerRow = dataLayout.bytesPerRow;
    if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        bytesPerRow = std::max<uint32_t>(size.height ? (data.size() / size.height) : data.size(), Texture::bytesPerRow(textureFormat, widthForMetal, texture->sampleCount()));

    switch (texture->dimension()) {
    case WGPUTextureDimension_1D: {
        auto blockSizeTimes1DTextureLimit = checkedProduct<uint32_t>(blockSize, device->limits().maxTextureDimension1D);
        bytesPerRow = blockSizeTimes1DTextureLimit.hasOverflowed() ? bytesPerRow : std::min<uint32_t>(bytesPerRow, blockSizeTimes1DTextureLimit.value());
    }break;
    case WGPUTextureDimension_2D:
    case WGPUTextureDimension_3D: {
        auto blockSizeTimes2DTextureLimit = checkedProduct<uint32_t>(blockSize, device->limits().maxTextureDimension2D);
        bytesPerRow = blockSizeTimes2DTextureLimit.hasOverflowed() ? bytesPerRow : std::min<uint32_t>(bytesPerRow, blockSizeTimes2DTextureLimit.value());
    } break;
    case WGPUTextureDimension_Force32:
        break;
    }

    NSUInteger rowsPerImage = (dataLayout.rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED) ? size.height : dataLayout.rowsPerImage;
    auto checkedBytesPerImage = checkedProduct<uint32_t>(bytesPerRow, rowsPerImage);
    if (checkedBytesPerImage.hasOverflowed())
        return;
    NSUInteger bytesPerImage = checkedBytesPerImage.value();

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

    id<MTLTexture> mtlTexture = texture->texture();
    auto textureDimension = texture->dimension();
    uint32_t sliceCount = textureDimension == WGPUTextureDimension_3D ? 1 : size.depthOrArrayLayers;
    bool clearWasNeeded = false;
    for (uint32_t layer = 0; layer < sliceCount; ++layer) {
        auto checkedDestinationSlice = checkedSum<uint32_t>(destination.origin.z, layer);
        if (checkedDestinationSlice.hasOverflowed())
            return;
        NSUInteger destinationSlice = textureDimension == WGPUTextureDimension_3D ? 0 : checkedDestinationSlice.value();
        if (!texture->previouslyCleared(destination.mipLevel, destinationSlice)) {
            if (writeWillCompletelyClear(textureDimension, widthForMetal, logicalSize.width, heightForMetal, logicalSize.height, depthForMetal, logicalSize.depthOrArrayLayers))
                texture->setPreviouslyCleared(destination.mipLevel, destinationSlice);
            else {
                clearWasNeeded = true;
                clearTextureIfNeeded(destination, destinationSlice);
            }
        }
    }

    auto checkedBlockSizeTimes2048 = checkedProduct<uint32_t>(2048, blockSize);
    if (checkedBlockSizeTimes2048.hasOverflowed())
        return;
    NSUInteger maxRowBytes = textureDimension == WGPUTextureDimension_3D ? checkedBlockSizeTimes2048.value() : bytesPerRow;
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
                auto checkedNewDestinationOriginZ = checkedSum<uint32_t>(destination.origin.z, z);
                if (checkedNewDestinationOriginZ.hasOverflowed())
                    return;
                newDestination.origin.z = checkedNewDestinationOriginZ.value();
                for (uint32_t y = 0, endY = textureDimension == WGPUTextureDimension_1D ? std::max<uint32_t>(1, heightForMetal) : heightForMetal; y < endY; ) {
                    auto checkedDestinationOriginYPlusY = checkedSum<uint32_t>(destination.origin.y, y);
                    if (checkedDestinationOriginYPlusY.hasOverflowed())
                        return;
                    newDestination.origin.y = checkedDestinationOriginYPlusY.value();
                    auto checkedNewDestinationOriginYPlusHeight = checkedSum<uint32_t>(newDestination.origin.y, newSize.height);
                    if (checkedNewDestinationOriginYPlusHeight.value() > logicalSize.height)
                        newSize.height = static_cast<uint32_t>(checkedNewDestinationOriginYPlusHeight.value() - logicalSize.height);

                    auto checkedBytesPerRowTimesHeight = checkedProduct<uint32_t>(bytesPerRow, newSize.height);
                    if (checkedBytesPerRowTimesHeight.hasOverflowed())
                        return;
                    auto size = (y + 1 == endY) ? bytesInLastRow.value() : checkedBytesPerRowTimesHeight.value();
                    for (uint32_t x = 0; x < widthForMetal; ) {
                        auto checkedDestinationOriginXPlusX = checkedSum<uint32_t>(destination.origin.x, x);
                        if (checkedDestinationOriginXPlusX.hasOverflowed())
                            return;
                        newDestination.origin.x = checkedDestinationOriginXPlusX.value();
                        auto checkedYTimesBytesPerRow = checkedProduct<uint32_t>(y, bytesPerRow);
                        auto checkedZTimesBytesPerImage = checkedProduct<uint32_t>(z, bytesPerImage);
                        if (checkedYTimesBytesPerRow.hasOverflowed() || checkedZTimesBytesPerImage.hasOverflowed())
                            return;
                        auto checkedXPlusYPlusZ = checkedSum<uint32_t>(x, checkedYTimesBytesPerRow.value(), checkedZTimesBytesPerImage.value());
                        if (checkedXPlusYPlusZ.hasOverflowed())
                            return;
                        auto offset = checkedXPlusYPlusZ.value();
                        auto checkedOffsetPlusSize = checkedSum<uint32_t>(offset, size);
                        if (checkedOffsetPlusSize.hasOverflowed() || checkedOffsetPlusSize.value() > data.size())
                            return;

                        writeTexture(newDestination, data.subspan(offset, size), newDataLayout, newSize);
                        auto checkedXPlusMaxRowBytes = checkedSum<uint32_t>(x, maxRowBytes);
                        if (checkedXPlusMaxRowBytes.hasOverflowed())
                            return;
                        x = checkedXPlusMaxRowBytes.value();
                    }
                    auto checkedYPlusHeight = checkedSum<uint32_t>(y, newSize.height);
                    if (checkedYPlusHeight.hasOverflowed())
                        return;
                    y = checkedYPlusHeight.value();
                }
            }
            return;
        }

        bytesPerRow = 0;
        bytesPerImage = 0;
    }

    switch (textureDimension) {
    case WGPUTextureDimension_1D:
        if (!widthForMetal || !heightForMetal)
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
    auto checkedNewBytesPerRow = checkedProduct<uint32_t>(blockSize, ((widthForMetal / blockWidth) + ((widthForMetal % blockWidth) ? 1 : 0)));
    if (checkedNewBytesPerRow.hasOverflowed())
        return;
    const auto newBytesPerRow = checkedNewBytesPerRow.value();
    auto dataLayoutOffset = dataLayout.offset;
    const bool widthMismatch = newBytesPerRow != bytesPerRow && widthForMetal == logicalSize.width && heightForMetal == logicalSize.height;
    const bool multipleOfBlockSize = bytesPerRow % blockSize;
    if (isCompressed && (widthMismatch || multipleOfBlockSize)) {

        const auto maxY = std::max<size_t>(blockHeight, heightForMetal) / blockHeight;
        auto checkedNewBytesPerImage = checkedProduct<uint32_t>(newBytesPerRow, std::max<size_t>(blockHeight, logicalSize.height / blockHeight + (logicalSize.height % blockHeight ? 1 : 0)));
        if (checkedNewBytesPerImage.hasOverflowed())
            return;
        auto newBytesPerImage = checkedNewBytesPerImage.value();
        const auto maxZ = std::max<size_t>(1, size.depthOrArrayLayers);
        auto checkedNewBytesPerImageTimesMaxZ = checkedProduct<uint32_t>(newBytesPerImage, maxZ);
        if (checkedNewBytesPerImageTimesMaxZ.hasOverflowed())
            return;
        newData = Vector<uint8_t>(FillWith { }, checkedNewBytesPerImageTimesMaxZ.value(), 0);
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
            auto maxYMinus1TimesNewBytesPerRow = checkedProduct<uint32_t>((maxY - 1), newBytesPerRow);
            auto maxZMinus1TimesNewBytesPerImage = checkedProduct<uint32_t>((maxZ - 1), newBytesPerImage);
            auto maxYMinus1TimesBytesPerRow = checkedProduct<uint32_t>((maxY - 1), bytesPerRow);
            auto maxZMinus1TimesBytesPerImage = checkedProduct<uint32_t>((maxZ - 1), bytesPerImage);
            if (maxYMinus1TimesNewBytesPerRow.hasOverflowed() || maxZMinus1TimesNewBytesPerImage.hasOverflowed() || maxYMinus1TimesBytesPerRow.hasOverflowed() || maxZMinus1TimesBytesPerImage.hasOverflowed())
                return;
            auto checkedNewBytesSum = checkedSum<uint32_t>(maxYMinus1TimesNewBytesPerRow.value(), maxZMinus1TimesNewBytesPerImage.value(), newBytesPerRow);
            auto checkedBytesSum = checkedSum<uint32_t>(maxYMinus1TimesBytesPerRow.value(), maxZMinus1TimesBytesPerImage.value(), dataLayout.offset, newBytesPerRow);
            if (checkedNewBytesSum.hasOverflowed() || checkedBytesSum.hasOverflowed())
                return;
            if (checkedNewBytesSum.value() > newData.size() || checkedBytesSum.value() > data.size()) {
                auto y = (maxY - 1);
                auto z = (maxZ - 1);
                device->generateAValidationError([NSString stringWithFormat:@"y(%zu) * newBytesPerRow(%u) + z(%zu) * newBytesPerImage(%u) + newBytesPerRow(%u) > newData.size()(%zu) || y(%zu) * bytesPerRow(%lu) + z(%zu) * bytesPerImage(%lu) + newBytesPerRow(%u) > dataSize(%zu), copySize %u, %u, %u, textureSize %u, %u, %u, offset %llu", y, newBytesPerRow, z, newBytesPerImage, newBytesPerRow, newData.size(), y, static_cast<unsigned long>(bytesPerRow), z, static_cast<unsigned long>(bytesPerImage), newBytesPerRow, data.size(), widthForMetal, heightForMetal, depthForMetal, logicalSize.width, logicalSize.height, logicalSize.depthOrArrayLayers, dataLayout.offset]);
                return;
            }
        }

        auto newDataSpan = newData.mutableSpan();
        for (size_t z = 0; z < maxZ; ++z) {
            for (size_t y = 0; y < maxY; ++y) {
                auto yTimesBytesPerRow = checkedProduct<uint32_t>(y, bytesPerRow);
                auto yTimesNewBytesPerRow = checkedProduct<uint32_t>(y, newBytesPerRow);
                auto zTimesBytesPerImage = checkedProduct<uint32_t>(z, bytesPerImage);
                auto zTimesNewBytesPerImage = checkedProduct<uint32_t>(z, newBytesPerImage);
                if (yTimesBytesPerRow.hasOverflowed() || yTimesNewBytesPerRow.hasOverflowed() || zTimesBytesPerImage.hasOverflowed() || zTimesNewBytesPerImage.hasOverflowed())
                    return;
                auto checkedYPlusZPlusOffset = checkedSum<uint32_t>(yTimesBytesPerRow.value(), zTimesBytesPerImage.value(), dataLayout.offset);
                auto checkedNewYPlusNewZ = checkedSum<uint32_t>(yTimesNewBytesPerRow.value(), zTimesNewBytesPerImage.value());
                if (checkedYPlusZPlusOffset.hasOverflowed() || checkedNewYPlusNewZ.hasOverflowed())
                    return;
                auto sourceBytesSpan = data.subspan(checkedYPlusZPlusOffset.value(), newBytesPerRow);
                auto destBytesSpan = newDataSpan.subspan(checkedNewYPlusNewZ.value(), newBytesPerRow);
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
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        case MTLStorageModeManaged:
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
            {
                switch (textureDimension) {
                case WGPUTextureDimension_1D: {
                    if (!widthForMetal || !heightForMetal)
                        return;

                    auto region = MTLRegionMake1D(destination.origin.x, widthForMetal);
                    for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
                        auto checkedLayerTimesBytesPerImage = checkedProduct<uint32_t>(layer, bytesPerImage);
                        if (checkedLayerTimesBytesPerImage.hasOverflowed())
                            return;
                        auto checkedDataLayoutOffsetPlusSum = checkedSum<uint32_t>(dataLayoutOffset, checkedLayerTimesBytesPerImage.value());
                        if (checkedDataLayoutOffsetPlusSum.hasOverflowed())
                            return;
                        auto sourceOffset = static_cast<NSUInteger>(checkedDataLayoutOffsetPlusSum.value());
                        if (sourceOffset % blockSize)
                            continue;
                        auto checkedDestinationSlice = checkedSum<NSUInteger>(destination.origin.z, layer);
                        if (checkedDestinationSlice.hasOverflowed())
                            return;
                        NSUInteger destinationSlice = checkedDestinationSlice.value();
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:byteCast<char>(data.subspan(sourceOffset).data())
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
                        auto layerTimesBytesPerImage = checkedProduct<uint32_t>(layer, bytesPerImage);
                        if (layerTimesBytesPerImage.hasOverflowed())
                            return;

                        auto checkedSourceOffset = checkedSum<NSUInteger>(dataLayoutOffset, layerTimesBytesPerImage.value());
                        if (checkedSourceOffset.hasOverflowed())
                            return;
                        auto sourceOffset = checkedSourceOffset.value();
                        if (sourceOffset % blockSize)
                            continue;
                        auto checkedDestinationSlice = checkedSum<NSUInteger>(destination.origin.z, layer);
                        if (checkedDestinationSlice.hasOverflowed())
                            return;
                        NSUInteger destinationSlice = checkedDestinationSlice.value();
                        [mtlTexture
                            replaceRegion:region
                            mipmapLevel:destination.mipLevel
                            slice:destinationSlice
                            withBytes:byteCast<char>(data.subspan(sourceOffset).data())
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
                        withBytes:byteCast<char>(data.subspan(sourceOffset).data())
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
    auto temporaryBufferWithOffset = newTemporaryBufferWithBytes(data.subspan(dataLayoutOffset), noCopy);
    id<MTLBuffer> temporaryBuffer = temporaryBufferWithOffset.first;
    auto temporaryBufferOffset = temporaryBufferWithOffset.second;
    if (!temporaryBuffer)
        return;
    ASSERT(temporaryBuffer.length >= newBufferSize);

    switch (texture->dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal || !heightForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);

        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            auto checkedSourceOffset = checkedProduct<NSUInteger>(layer, bytesPerImage);
            if (checkedSourceOffset.hasOverflowed())
                return;
            NSUInteger sourceOffset = checkedSourceOffset.value();
            auto checkedDestinationSlice = checkedSum<NSUInteger>(destination.origin.z, layer);
            if (checkedDestinationSlice.hasOverflowed())
                return;
            NSUInteger destinationSlice = checkedDestinationSlice.value();
            auto widthTimesBlockSize = checkedProduct<NSUInteger>(widthForMetal, blockSize);
            if (widthTimesBlockSize.hasOverflowed())
                return;
            auto sourceOffsetSum = checkedSum<NSUInteger>(sourceOffset, widthTimesBlockSize.value());
            if (sourceOffsetSum.hasOverflowed())
                return;
            if (sourceOffsetSum.value() > newBufferSize)
                continue;
            if (sourceOffset % blockSize)
                continue;

            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset + temporaryBufferOffset
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
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        if (!widthForMetal || !heightForMetal || (bytesPerRow && bytesPerRow < Texture::bytesPerRow(textureFormat, widthForMetal, texture->sampleCount())))
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);
        for (uint32_t layer = 0; layer < size.depthOrArrayLayers; ++layer) {
            auto layerTimesBytesPerImage = checkedProduct<NSUInteger>(layer, bytesPerImage);
            if (layerTimesBytesPerImage.hasOverflowed())
                return;
            NSUInteger sourceOffset = layerTimesBytesPerImage.value();
            auto checkedDestinationSlice = checkedSum<NSUInteger>(destination.origin.z, layer);
            if (checkedDestinationSlice.hasOverflowed())
                return;
            NSUInteger destinationSlice = checkedDestinationSlice.value();
            if (sourceOffset % blockSize)
                continue;
            [m_blitCommandEncoder
                copyFromBuffer:temporaryBuffer
                sourceOffset:sourceOffset + temporaryBufferOffset
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
        if (!widthForMetal || !heightForMetal || !depthForMetal || (bytesPerRow && bytesPerRow < Texture::bytesPerRow(textureFormat, widthForMetal, texture->sampleCount())))
            return;

        [m_blitCommandEncoder
            copyFromBuffer:temporaryBuffer
            sourceOffset:temporaryBufferOffset
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

    if (noCopy) {
        if (!newData.isEmpty()) {
            // The MTLBuffer above was created with newBufferWithBytesNoCopy and aliases newData's storage; keep that storage alive until the GPU has consumed it.
            __block Vector<uint8_t> retainedNewData = WTF::move(newData);
            [m_commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
                retainedNewData = { };
            }];
        }
        finalizeBlitCommandEncoder();
    }
}

// Host mirror of the MSL `WebKitCopyExternalImageArgs` declared in the shader source below, and a
// hand-maintained ABI in the same way StagedCopyArguments above is, so the static_asserts pin the size
// and every offset. Every field is a scalar or an array of scalars: MSL lays those out exactly as C
// does, while its vector and matrix types carry alignment rules of their own.
struct CopyExternalImageArguments {
    uint32_t sourceOriginX;
    uint32_t sourceOriginY;
    uint32_t destinationOriginX;
    uint32_t destinationOriginY;
    uint32_t copyWidth;
    uint32_t copyHeight;
    uint32_t sourceMaxX;
    uint32_t sourceMaxY;
    uint32_t flags;
    std::array<float, 9> colorMatrix; // Row-major.
    // A video frame's simd::float3x2 crop and simd::float4x3 YCbCr matrices, flattened column-major.
    // Flattened because a simd float3 column is padded to 16 bytes, so neither matrix has a size the
    // arithmetic below would guess right.
    std::array<float, 6> uvRemapMatrix;
    std::array<float, 12> ycbcrMatrix;
};
static_assert(sizeof(CopyExternalImageArguments) == 144, "CopyExternalImageArguments must match MSL WebKitCopyExternalImageArgs (9 x uint + 27 x float)");
static_assert(!offsetof(CopyExternalImageArguments, sourceOriginX), "sourceOriginX must be at offset 0");
static_assert(offsetof(CopyExternalImageArguments, sourceOriginY) == 4, "sourceOriginY must be at offset 4");
static_assert(offsetof(CopyExternalImageArguments, destinationOriginX) == 8, "destinationOriginX must be at offset 8");
static_assert(offsetof(CopyExternalImageArguments, destinationOriginY) == 12, "destinationOriginY must be at offset 12");
static_assert(offsetof(CopyExternalImageArguments, copyWidth) == 16, "copyWidth must be at offset 16");
static_assert(offsetof(CopyExternalImageArguments, copyHeight) == 20, "copyHeight must be at offset 20");
static_assert(offsetof(CopyExternalImageArguments, sourceMaxX) == 24, "sourceMaxX must be at offset 24");
static_assert(offsetof(CopyExternalImageArguments, sourceMaxY) == 28, "sourceMaxY must be at offset 28");
static_assert(offsetof(CopyExternalImageArguments, flags) == 32, "flags must be at offset 32");
static_assert(offsetof(CopyExternalImageArguments, colorMatrix) == 36, "colorMatrix must be at offset 36");
static_assert(offsetof(CopyExternalImageArguments, uvRemapMatrix) == 72, "uvRemapMatrix must be at offset 72");
static_assert(offsetof(CopyExternalImageArguments, ycbcrMatrix) == 96, "ycbcrMatrix must be at offset 96");

// Mirrors the `webKitCopyExternalImage*` constants in the shader source below.
enum CopyExternalImageFlags : uint32_t {
    CopyExternalImageFlipY = 1 << 0,
    CopyExternalImageForceOpaqueAlpha = 1 << 1,
    CopyExternalImageUnpremultiplySource = 1 << 2,
    CopyExternalImagePremultiplyDestination = 1 << 3,
    CopyExternalImageDecodeSourceTransferFunction = 1 << 4,
    CopyExternalImageEncodeDestinationTransferFunction = 1 << 5,
    CopyExternalImageCancelSRGBDestinationEncoding = 1 << 6,
    CopyExternalImageSourceIsVideoFrame = 1 << 7,
};

// https://drafts.csswg.org/css-color-4/#color-conversion-code, in row-major order and applied to
// linear-light values.
static constexpr std::array<float, 9> identityColorMatrix { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
static constexpr std::array<float, 9> sRGBToDisplayP3ColorMatrix {
    0.8224621f, 0.1775380f, 0.f,
    0.0331941f, 0.9668058f, 0.f,
    0.0170827f, 0.0723974f, 0.9105199f
};
static constexpr std::array<float, 9> displayP3ToSRGBColorMatrix {
    1.2249401f, -0.2249404f, 0.f,
    -0.0420569f, 1.0420571f, 0.f,
    -0.0196376f, -0.0786361f, 1.0982735f
};

// Video is tagged with primaries of its own, which are usually neither of the two colour spaces
// copyExternalImageToTexture names. Derived from each set of primaries against the same D65 white
// point the two matrices above use, so that a video which really is BT.709 still yields the identity.
static constexpr std::array<float, 9> smpteCToSRGBColorMatrix {
    0.9395421f, 0.0501814f, 0.0102766f,
    0.0177722f, 0.9657929f, 0.0164349f,
    -0.0016216f, -0.0043697f, 1.0059913f
};
static constexpr std::array<float, 9> smpteCToDisplayP3ColorMatrix {
    0.7758929f, 0.2127372f, 0.0113699f,
    0.0483696f, 0.9353999f, 0.0162305f,
    0.0158600f, 0.0667994f, 0.9173406f
};
static constexpr std::array<float, 9> ebu3213ToSRGBColorMatrix {
    1.0440432f, -0.0440432f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.0117934f, 0.9882066f
};
static constexpr std::array<float, 9> ebu3213ToDisplayP3ColorMatrix {
    0.8586858f, 0.1413142f, 0.f,
    0.0346562f, 0.9653438f, 0.f,
    0.0178350f, 0.0823832f, 0.8997818f
};
static constexpr std::array<float, 9> bt2020ToSRGBColorMatrix {
    1.6604910f, -0.5876411f, -0.0728499f,
    -0.1245505f, 1.1328999f, -0.0083494f,
    -0.0181508f, -0.1005789f, 1.1187297f
};
static constexpr std::array<float, 9> bt2020ToDisplayP3ColorMatrix {
    1.3435783f, -0.2821797f, -0.0613986f,
    -0.0652975f, 1.0757879f, -0.0104905f,
    0.0028218f, -0.0195985f, 1.0167767f
};

// Leaves the frame's coordinates alone, which is what a frame whose clean aperture is its whole coded
// extent needs, and what the IOSurface path - which never samples - is given.
static constexpr std::array<float, 6> identityUVRemapMatrix { 1, 0, 0, 1, 0, 0 };

// The inverse of a frame's display transform, in normalized coordinates and flattened the same way
// the crop matrix is: a coordinate in the presented image maps back to the frame pixel which holds
// it. The forward transform mirrors x, when the frame is mirrored, and then rotates clockwise, so the
// inverse un-rotates and then un-mirrors.
static std::array<float, 6> inverseDisplayTransformMatrix(WGPUVideoFrameRotation rotation, bool isMirrored)
{
    std::array<float, 6> matrix = identityUVRemapMatrix;
    switch (rotation) {
    case WGPUVideoFrameRotation_None:
        break;
    case WGPUVideoFrameRotation_Right:
        matrix = { 0, -1, 1, 0, 0, 1 };
        break;
    case WGPUVideoFrameRotation_UpsideDown:
        matrix = { -1, 0, 0, -1, 1, 1 };
        break;
    case WGPUVideoFrameRotation_Left:
        matrix = { 0, 1, -1, 0, 1, 0 };
        break;
    }

    if (isMirrored) {
        // Reflect only the coordinate the mirror moved, about the middle of the frame.
        matrix[0] = -matrix[0];
        matrix[2] = -matrix[2];
        matrix[4] = 1 - matrix[4];
    }

    return matrix;
}

// The affine which applies inner and then outer, in that same flattened layout.
static std::array<float, 6> concatenatedAffineTransforms(const std::array<float, 6>& outer, const std::array<float, 6>& inner)
{
    return {
        outer[0] * inner[0] + outer[2] * inner[1],
        outer[1] * inner[0] + outer[3] * inner[1],
        outer[0] * inner[2] + outer[2] * inner[3],
        outer[1] * inner[2] + outer[3] * inner[3],
        outer[0] * inner[4] + outer[2] * inner[5] + outer[4],
        outer[1] * inner[4] + outer[3] * inner[5] + outer[5],
    };
}

enum class CopyExternalImageSourcePrimaries : uint8_t {
    Srgb, // ITU-R BT.709, whose primaries sRGB shares.
    DisplayP3,
    SmpteC, // ITU-R BT.601 525-line, as NTSC video is tagged.
    Ebu3213, // ITU-R BT.601 625-line, as PAL video is tagged.
    Bt2020,
};

static CopyExternalImageSourcePrimaries sourcePrimariesForPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    RetainPtr primaries = adoptCF(static_cast<CFStringRef>(CVBufferCopyAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey, nil)));
    if (!primaries)
        return CopyExternalImageSourcePrimaries::Srgb;

    if (CFEqual(primaries.get(), kCVImageBufferColorPrimaries_SMPTE_C))
        return CopyExternalImageSourcePrimaries::SmpteC;
    if (CFEqual(primaries.get(), kCVImageBufferColorPrimaries_EBU_3213))
        return CopyExternalImageSourcePrimaries::Ebu3213;
    if (CFEqual(primaries.get(), kCVImageBufferColorPrimaries_ITU_R_2020))
        return CopyExternalImageSourcePrimaries::Bt2020;
    // The DCI white point is not D65, but WebKit treats DCI-P3 content as Display P3 elsewhere too.
    if (CFEqual(primaries.get(), kCVImageBufferColorPrimaries_P3_D65) || CFEqual(primaries.get(), kCVImageBufferColorPrimaries_DCI_P3))
        return CopyExternalImageSourcePrimaries::DisplayP3;

    // ITU-R BT.709, and anything unrecognized: leaving the values alone is what the readback path this
    // replaced did as well.
    return CopyExternalImageSourcePrimaries::Srgb;
}

static std::array<float, 9> colorMatrixBetweenPrimaries(CopyExternalImageSourcePrimaries source, bool destinationIsDisplayP3)
{
    switch (source) {
    case CopyExternalImageSourcePrimaries::Srgb:
        return destinationIsDisplayP3 ? sRGBToDisplayP3ColorMatrix : identityColorMatrix;
    case CopyExternalImageSourcePrimaries::DisplayP3:
        return destinationIsDisplayP3 ? identityColorMatrix : displayP3ToSRGBColorMatrix;
    case CopyExternalImageSourcePrimaries::SmpteC:
        return destinationIsDisplayP3 ? smpteCToDisplayP3ColorMatrix : smpteCToSRGBColorMatrix;
    case CopyExternalImageSourcePrimaries::Ebu3213:
        return destinationIsDisplayP3 ? ebu3213ToDisplayP3ColorMatrix : ebu3213ToSRGBColorMatrix;
    case CopyExternalImageSourcePrimaries::Bt2020:
        return destinationIsDisplayP3 ? bt2020ToDisplayP3ColorMatrix : bt2020ToSRGBColorMatrix;
    }

    ASSERT_NOT_REACHED();
    return identityColorMatrix;
}

// simd matrix columns are padded, so the arguments buffer carries them as plain floats instead.
static std::array<float, 6> flattenedColumns(const simd::float3x2& matrix)
{
    return { matrix.columns[0].x, matrix.columns[0].y,
        matrix.columns[1].x, matrix.columns[1].y,
        matrix.columns[2].x, matrix.columns[2].y };
}

static std::array<float, 12> flattenedColumns(const simd::float4x3& matrix)
{
    return { matrix.columns[0].x, matrix.columns[0].y, matrix.columns[0].z,
        matrix.columns[1].x, matrix.columns[1].y, matrix.columns[1].z,
        matrix.columns[2].x, matrix.columns[2].y, matrix.columns[2].z,
        matrix.columns[3].x, matrix.columns[3].y, matrix.columns[3].z };
}

id<MTLRenderPipelineState> Queue::copyExternalImagePipelineState(MTLPixelFormat pixelFormat)
{
    if (m_copyExternalImagePipelineCreationFailed || pixelFormat == MTLPixelFormatInvalid)
        return nil;

    if (id<MTLRenderPipelineState> cachedPipelineState = [m_copyExternalImagePipelineStates objectForKey:@(pixelFormat)])
        return cachedPipelineState;

    auto device = m_device.get();
    id<MTLDevice> mtlDevice = device ? device->device() : nil;
    if (!mtlDevice)
        return nil;

    NSError *error = nil;
    /* NOLINT */ id<MTLLibrary> library = [mtlDevice newLibraryWithSource:@R"(#include <metal_stdlib>
using namespace metal;
struct WebKitCopyExternalImageArgs {
    uint sourceOriginX;
    uint sourceOriginY;
    uint destinationOriginX;
    uint destinationOriginY;
    uint copyWidth;
    uint copyHeight;
    uint sourceMaxX;
    uint sourceMaxY;
    uint flags;
    float colorMatrix[9];
    float uvRemapMatrix[6];
    float ycbcrMatrix[12];
};
constant uint webKitCopyExternalImageFlipY = 1;
constant uint webKitCopyExternalImageForceOpaqueAlpha = 2;
constant uint webKitCopyExternalImageUnpremultiplySource = 4;
constant uint webKitCopyExternalImagePremultiplyDestination = 8;
constant uint webKitCopyExternalImageDecodeSourceTransferFunction = 16;
constant uint webKitCopyExternalImageEncodeDestinationTransferFunction = 32;
constant uint webKitCopyExternalImageCancelSRGBDestinationEncoding = 64;
constant uint webKitCopyExternalImageSourceIsVideoFrame = 128;
// A frame's chroma plane is half resolution, so it has to be filtered rather than read, and the crop
// the coordinates go through can leave them just outside the plane at the edges.
constexpr sampler webKitCopyExternalImageFrameSampler(coord::normalized, address::clamp_to_edge, filter::linear);
// Sign-preserving, so that the out-of-[0, 1] values an rgba16float surface can hold survive.
float3 webKitSRGBToLinear(float3 encoded)
{
    float3 magnitude = abs(encoded);
    return sign(encoded) * select(pow((magnitude + 0.055f) / 1.055f, 2.4f), magnitude / 12.92f, magnitude <= 0.04045f);
}
float3 webKitLinearToSRGB(float3 linearLight)
{
    float3 magnitude = abs(linearLight);
    return sign(linearLight) * select(1.055f * pow(magnitude, 1.f / 2.4f) - 0.055f, magnitude * 12.92f, magnitude <= 0.0031308f);
}
struct WebKitCopyExternalImageVertexOut {
    float4 position [[position]];
};
[[vertex]] WebKitCopyExternalImageVertexOut vsCopyExternalImage(uint vertexID [[vertex_id]])
{
    // A triangle strip covering all of NDC. The viewport is what restricts rasterization to the
    // destination rectangle, so the vertex stage needs none of the copy parameters.
    WebKitCopyExternalImageVertexOut out;
    out.position = float4((vertexID & 1) ? 1.f : -1.f, (vertexID & 2) ? 1.f : -1.f, 0.f, 1.f);
    return out;
}
[[fragment]] float4 fsCopyExternalImage(WebKitCopyExternalImageVertexOut in [[stage_in]], texture2d<float> source [[texture(0)]], texture2d<float> sourceSecondPlane [[texture(1)]], constant WebKitCopyExternalImageArgs& args [[buffer(0)]])
{
    uint2 copyPosition = uint2(in.position.xy) - uint2(args.destinationOriginX, args.destinationOriginY);
    uint sourceY = (args.flags & webKitCopyExternalImageFlipY) ? (args.copyHeight - 1 - copyPosition.y) : copyPosition.y;
    // Clamped because a caller-supplied rectangle reaching past the end of the source must read the
    // edge rather than whatever the surface's padding holds.
    uint2 sourcePosition = min(uint2(args.sourceOriginX + copyPosition.x, args.sourceOriginY + sourceY), uint2(args.sourceMaxX, args.sourceMaxY));
    float4 color;
    if (args.flags & webKitCopyExternalImageSourceIsVideoFrame) {
        // A decoded frame's two planes, sampled the way textureSampleBaseClampToEdge samples an
        // external texture: the frame's own crop applied to normalized coordinates, then out of its
        // YCbCr space. A single-plane frame arrives as a swizzled view of the first plane paired with
        // the identity matrix, so both kinds of frame take this one path.
        float2 frameCoordinates = (float2(sourcePosition) + 0.5f) / float2(args.sourceMaxX + 1, args.sourceMaxY + 1);
        float2 croppedCoordinates = float2(
            args.uvRemapMatrix[0] * frameCoordinates.x + args.uvRemapMatrix[2] * frameCoordinates.y + args.uvRemapMatrix[4],
            args.uvRemapMatrix[1] * frameCoordinates.x + args.uvRemapMatrix[3] * frameCoordinates.y + args.uvRemapMatrix[5]);
        float3 ycbcr = float3(source.sample(webKitCopyExternalImageFrameSampler, croppedCoordinates).r, sourceSecondPlane.sample(webKitCopyExternalImageFrameSampler, croppedCoordinates).rg);
        // Column-major, matching how the host flattened it, and affine: the fourth column is added.
        color = float4(
            args.ycbcrMatrix[0] * ycbcr.x + args.ycbcrMatrix[3] * ycbcr.y + args.ycbcrMatrix[6] * ycbcr.z + args.ycbcrMatrix[9],
            args.ycbcrMatrix[1] * ycbcr.x + args.ycbcrMatrix[4] * ycbcr.y + args.ycbcrMatrix[7] * ycbcr.z + args.ycbcrMatrix[10],
            args.ycbcrMatrix[2] * ycbcr.x + args.ycbcrMatrix[5] * ycbcr.y + args.ycbcrMatrix[8] * ycbcr.z + args.ycbcrMatrix[11],
            1.f);
    } else {
        // One source texel per destination texel, so read() rather than sample(): no filtering, and no
        // sampler state to get wrong.
        color = source.read(sourcePosition);
    }
    if (args.flags & webKitCopyExternalImageForceOpaqueAlpha)
        color.a = 1.f;
    if ((args.flags & webKitCopyExternalImageUnpremultiplySource) && color.a > 0.f)
        color.rgb /= color.a;
    if (args.flags & webKitCopyExternalImageDecodeSourceTransferFunction)
        color.rgb = webKitSRGBToLinear(color.rgb);
    // Explicit dot products, so that the host's row-major storage order is unambiguous. The matrix is
    // the identity when the source and destination share primaries, which leaves the values alone.
    float3 rgb = color.rgb;
    color.r = args.colorMatrix[0] * rgb.r + args.colorMatrix[1] * rgb.g + args.colorMatrix[2] * rgb.b;
    color.g = args.colorMatrix[3] * rgb.r + args.colorMatrix[4] * rgb.g + args.colorMatrix[5] * rgb.b;
    color.b = args.colorMatrix[6] * rgb.r + args.colorMatrix[7] * rgb.g + args.colorMatrix[8] * rgb.b;
    if (args.flags & webKitCopyExternalImageEncodeDestinationTransferFunction)
        color.rgb = webKitLinearToSRGB(color.rgb);
    if (args.flags & webKitCopyExternalImagePremultiplyDestination)
        color.rgb *= color.a;
    // Metal applies the sRGB transfer function on the way into an _sRGB attachment. Undo it here, so
    // that the texels stored are the ones writeTexture would have stored for the same copy.
    if (args.flags & webKitCopyExternalImageCancelSRGBDestinationEncoding)
        color.rgb = webKitSRGBToLinear(color.rgb);
    return color;
})" options:nil error:&error];
    id<MTLFunction> vertexFunction = error ? nil : [library newFunctionWithName:@"vsCopyExternalImage"];
    id<MTLFunction> fragmentFunction = vertexFunction ? [library newFunctionWithName:@"fsCopyExternalImage"] : nil;
    id<MTLRenderPipelineState> pipelineState = nil;
    if (fragmentFunction) {
        auto *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
        pipelineState = [mtlDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    }

    if (!pipelineState) {
        m_copyExternalImagePipelineCreationFailed = true;
        WTFLogAlways("WebGPU: copyExternalImageToTexture render pipeline creation failed (%@)", error); // NOLINT
        return nil;
    }

    if (!m_copyExternalImagePipelineStates)
        m_copyExternalImagePipelineStates = [NSMutableDictionary dictionary];
    [m_copyExternalImagePipelineStates setObject:pipelineState forKey:@(pixelFormat)];
    return pipelineState;
}

// https://gpuweb.github.io/gpuweb/#dom-gpuqueue-copyexternalimagetotexture asks for a colour format
// which is renderable, because the copy below is performed by rendering into the destination, and whose
// channels are normalized or floating point, because the fragment stage writes floats. Which formats
// are renderable depends on the features the device enabled - texture-formats-tier1 and
// rg11b10ufloat-renderable both widen the set - so ask Texture rather than keeping a list here.
static bool isValidCopyExternalImageDestinationFormat(WGPUTextureFormat format, const Device& device)
{
    if (!Texture::isColorRenderableFormat(format, device))
        return false;

    switch (format) {
    // A render pass can target these, but the fragment stage below writes floats, not integers.
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGB10A2Uint:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_RGBA32Uint:
    case WGPUTextureFormat_RGBA32Sint:
    // texture-formats-tier1 makes these renderable, but the spec excludes them all the same.
    case WGPUTextureFormat_R8Snorm:
    case WGPUTextureFormat_R16Snorm:
    case WGPUTextureFormat_RG8Snorm:
    case WGPUTextureFormat_RG16Snorm:
    case WGPUTextureFormat_RGBA8Snorm:
    case WGPUTextureFormat_RGBA16Snorm:
        return false;
    default:
        return true;
    }
}

NSString* Queue::errorValidatingCopyExternalImageToTexture(const WGPUImageCopyTextureTagged& destination, const WGPUExtent3D& copySize, const Texture& texture) const
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopytexturetagged
#define ERROR_STRING(x) [NSString stringWithFormat:@"GPUQueue.copyExternalImageToTexture: %@", x]
    if (!isValidToUseWith(texture, *this))
        return ERROR_STRING(@"destination texture is not valid");

    WGPUImageCopyTexture untaggedDestination {
        .texture = destination.texture,
        .mipLevel = destination.mipLevel,
        .origin = destination.origin,
        .aspect = destination.aspect,
    };

    if (NSString* error = Texture::errorValidatingImageCopyTexture(untaggedDestination, copySize))
        return ERROR_STRING(error);

    if (NSString* error = Texture::errorValidatingTextureCopyRange(untaggedDestination, copySize))
        return ERROR_STRING(error);

    if (copySize.depthOrArrayLayers > 1)
        return ERROR_STRING(@"copySize.depthOrArrayLayers is greater than 1");

    if (destination.aspect != WGPUTextureAspect_All)
        return ERROR_STRING(@"destination aspect is not All");

    if (texture.dimension() != WGPUTextureDimension_2D)
        return ERROR_STRING(@"destination texture dimension is not 2D");

    if (texture.sampleCount() != 1)
        return ERROR_STRING(@"destination texture sampleCount is not 1");

    if (!(texture.usage() & WGPUTextureUsage_CopyDst))
        return ERROR_STRING(@"destination texture usage does not contain CopyDst");

    // The copy is performed by rendering into the destination, which is why the spec asks for this on
    // top of CopyDst.
    if (!(texture.usage() & WGPUTextureUsage_RenderAttachment))
        return ERROR_STRING(@"destination texture usage does not contain RenderAttachment");

    if (!isValidCopyExternalImageDestinationFormat(texture.format(), device()))
        return ERROR_STRING(@"destination texture format cannot be a copyExternalImageToTexture destination");

#undef ERROR_STRING
    return nil;
}

void Queue::copyExternalImageToTexture(const WGPUImageCopyExternalImage& source, const WGPUImageCopyTextureTagged& destination, const WGPUExtent3D& copySize)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-copyexternalimagetotexture
    auto device = m_device.get();
    if (!device)
        return;

    Ref texture = fromAPI(destination.texture);
    if (texture->isDestroyed()) {
        device->generateAValidationError("GPUQueue.copyExternalImageToTexture: destination texture is destroyed"_s);
        return;
    }

    if (NSString* error = errorValidatingCopyExternalImageToTexture(destination, copySize, texture)) {
        device->generateAValidationError(error);
        return;
    }

    if (!source.source && !source.pixelBuffer) {
        device->generateAValidationError("GPUQueue.copyExternalImageToTexture: source image is not valid"_s);
        return;
    }

    // errorValidatingTextureCopyRange() has already rejected a copy reaching past the destination mip
    // level, so this only ever clamps away an empty copy.
    auto logicalSize = texture->logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = logicalSize.width < destination.origin.x ? 0 : std::min(copySize.width, logicalSize.width - destination.origin.x);
    auto heightForMetal = logicalSize.height < destination.origin.y ? 0 : std::min(copySize.height, logicalSize.height - destination.origin.y);
    if (!widthForMetal || !heightForMetal)
        return;

    auto destinationFormat = texture->format();
    id<MTLRenderPipelineState> pipelineState = copyExternalImagePipelineState(Texture::pixelFormat(destinationFormat));
    if (!pipelineState)
        return;

    auto isLinear = [](WGPUColorSpace colorSpace) {
        return colorSpace == SRGBLinear || colorSpace == DisplayP3Linear;
    };
    auto isDisplayP3 = [](WGPUColorSpace colorSpace) {
        return colorSpace == DisplayP3 || colorSpace == DisplayP3Linear;
    };

    uint32_t flags = 0;
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    id<MTLTexture> sourceTexture = nil;
    // The chroma plane of a decoded video frame, or a swizzled view of a single-plane frame's own
    // texture. Bound by the IOSurface path below too, which never reads it, because Metal validates
    // every texture the fragment function declares.
    id<MTLTexture> sourceSecondPlaneTexture = nil;
    auto uvRemapMatrix = identityUVRemapMatrix;
    std::array<float, 12> ycbcrMatrix { };
    auto sourcePrimaries = isDisplayP3(source.colorSpace) ? CopyExternalImageSourcePrimaries::DisplayP3 : CopyExternalImageSourcePrimaries::Srgb;

    if (source.pixelBuffer) {
        // The frame's planes are wrapped in MTLTextures exactly the way importExternalTexture() wraps
        // them, so a video reaches the destination without its pixels ever leaving the GPU.
        auto frame = device->createExternalTextureFromPixelBuffer(source.pixelBuffer, source.colorSpace);
        sourceTexture = frame.texture0;
        sourceSecondPlaneTexture = frame.texture1;
        if (!sourceTexture || !sourceSecondPlaneTexture)
            return;

        // The frame's clean aperture, which is both the extent script sees and the extent the crop
        // matrix normalizes against, rather than the larger extent the planes may be coded at.
        auto cleanSize = CVImageBufferGetCleanRect(source.pixelBuffer).size;
        sourceWidth = static_cast<uint32_t>(cleanSize.width);
        sourceHeight = static_cast<uint32_t>(cleanSize.height);
        // A quarter turn presents the frame's extent transposed, and both the copy's coordinates and
        // the origin script asked to copy from are in the presented image.
        if (source.pixelBufferRotation == WGPUVideoFrameRotation_Right || source.pixelBufferRotation == WGPUVideoFrameRotation_Left)
            std::swap(sourceWidth, sourceHeight);
        if (!sourceWidth || !sourceHeight)
            return;

        // Sampling happens in the presented image's coordinates, so undo the display transform before
        // the frame's own crop, which is expressed in its stored coordinates. Both are affine, so
        // composing them here costs the shader nothing.
        uvRemapMatrix = concatenatedAffineTransforms(flattenedColumns(frame.uvRemappingMatrix), inverseDisplayTransformMatrix(source.pixelBufferRotation, source.pixelBufferIsMirrored));
        ycbcrMatrix = flattenedColumns(frame.colorSpaceConversionMatrix);
        // A frame's primaries are its own, and are usually neither of the two the caller can name.
        sourcePrimaries = sourcePrimariesForPixelBuffer(source.pixelBuffer);
        flags |= CopyExternalImageSourceIsVideoFrame;
    } else {
        auto surfaceWidth = static_cast<uint32_t>(IOSurfaceGetWidth(source.source));
        auto surfaceHeight = static_cast<uint32_t>(IOSurfaceGetHeight(source.source));
        // The surface can be larger than the image it backs, so the reachable extent is the smaller of
        // the two. Nothing past it may be read: it is either padding or another image's pixels.
        sourceWidth = std::min(surfaceWidth, source.sourceWidth);
        sourceHeight = std::min(surfaceHeight, source.sourceHeight);
        if (!sourceWidth || !sourceHeight)
            return;

        auto sourcePixelFormat = Texture::pixelFormat(source.sourceFormat);
        if (sourcePixelFormat == MTLPixelFormatInvalid)
            return;

        auto *sourceTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:sourcePixelFormat width:surfaceWidth height:surfaceHeight mipmapped:NO];
        sourceTextureDescriptor.usage = MTLTextureUsageShaderRead;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if ENABLE(WEBGPU_BY_DEFAULT)
        sourceTextureDescriptor.storageMode = device->hasUnifiedMemory() ? MTLStorageModeShared : MTLStorageModeManaged;
#else
        sourceTextureDescriptor.storageMode = MTLStorageModeManaged;
#endif
ALLOW_DEPRECATED_DECLARATIONS_END
#else
        sourceTextureDescriptor.storageMode = MTLStorageModeShared;
#endif
        sourceTexture = device->newTextureWithDescriptor(sourceTextureDescriptor, source.source);
        if (!sourceTexture)
            return;
        sourceSecondPlaneTexture = sourceTexture;
    }

    if (source.flipY)
        flags |= CopyExternalImageFlipY;
    if (!source.hasAlpha)
        flags |= CopyExternalImageForceOpaqueAlpha;

    auto colorMatrix = colorMatrixBetweenPrimaries(sourcePrimaries, isDisplayP3(destination.colorSpace));
    bool primariesDiffer = colorMatrix != identityColorMatrix;
    bool transferFunctionsDiffer = isLinear(source.colorSpace) != isLinear(destination.colorSpace);
    bool convertsColor = primariesDiffer || transferFunctionsDiffer;
    if (convertsColor) {
        // Primaries are converted in linear light, so the source is decoded and the destination
        // re-encoded around the matrix. Either step is skipped when that end is already linear.
        if (!isLinear(source.colorSpace))
            flags |= CopyExternalImageDecodeSourceTransferFunction;
        if (!isLinear(destination.colorSpace))
            flags |= CopyExternalImageEncodeDestinationTransferFunction;
    }

    // Only unpremultiply when something downstream needs unpremultiplied values: a colour conversion,
    // or an unpremultiplied destination. Two premultiplied ends with no conversion is a passthrough.
    if (source.premultipliedAlpha && (convertsColor || !destination.premultipliedAlpha))
        flags |= CopyExternalImageUnpremultiplySource;
    if (destination.premultipliedAlpha && (convertsColor || !source.premultipliedAlpha))
        flags |= CopyExternalImagePremultiplyDestination;

    if (Texture::removeSRGBSuffix(destinationFormat) != destinationFormat)
        flags |= CopyExternalImageCancelSRGBDestinationEncoding;

    // A 2D destination, per the validation above, so origin.z is an array layer and never a depth
    // plane, and errorValidatingTextureCopyRange() has bounded it.
    NSUInteger destinationSlice = destination.origin.z;

    if (!texture->previouslyCleared(destination.mipLevel, destinationSlice)) {
        if (writeWillCompletelyClear(WGPUTextureDimension_2D, widthForMetal, logicalSize.width, heightForMetal, logicalSize.height, 1, logicalSize.depthOrArrayLayers))
            texture->setPreviouslyCleared(destination.mipLevel, destinationSlice);
        else {
            WGPUImageCopyTexture untaggedDestination {
                .texture = destination.texture,
                .mipLevel = destination.mipLevel,
                .origin = destination.origin,
                .aspect = destination.aspect,
            };
            clearTextureIfNeeded(untaggedDestination, destinationSlice);
        }
    }

    // The clear above, if there was one, is encoded on the queue's staging command buffer. The render
    // pass has to follow it on that same buffer, which means closing whichever encoder is open on it.
    ensureBlitCommandEncoder();
    id<MTLCommandBuffer> commandBuffer = m_commandBuffer;
    if (!commandBuffer)
        return;
    if (m_blitCommandEncoder) {
        endEncoding(m_blitCommandEncoder, commandBuffer);
        m_blitCommandEncoder = nil;
    }
    if (m_stagedCopyEncoder) {
        endEncoding(m_stagedCopyEncoder, commandBuffer);
        m_stagedCopyEncoder = nil;
    }

    auto *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = texture->texture();
    renderPassDescriptor.colorAttachments[0].level = destination.mipLevel;
    renderPassDescriptor.colorAttachments[0].slice = destinationSlice;
    // The copy may cover only part of the level, and the rest has to survive it.
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    if (!renderCommandEncoder) {
        // The clear is real work already encoded on this command buffer, and finalizeBlitCommandEncoder()
        // keys off the encoders, so leaving it here would orphan it. Commit instead.
        finalizeBlitCommandEncoder();
        return;
    }
    setEncoderForBuffer(commandBuffer, renderCommandEncoder);

    CopyExternalImageArguments arguments {
        .sourceOriginX = source.originX,
        .sourceOriginY = source.originY,
        .destinationOriginX = destination.origin.x,
        .destinationOriginY = destination.origin.y,
        .copyWidth = widthForMetal,
        .copyHeight = heightForMetal,
        .sourceMaxX = sourceWidth - 1,
        .sourceMaxY = sourceHeight - 1,
        .flags = flags,
        .colorMatrix = colorMatrix,
        .uvRemapMatrix = uvRemapMatrix,
        .ycbcrMatrix = ycbcrMatrix,
    };

    // The viewport is what confines the full-NDC quad to the destination rectangle, and it is also
    // what makes [[position]] in the fragment shader land on the destination texel being written.
    MTLViewport viewport {
        static_cast<double>(destination.origin.x),
        static_cast<double>(destination.origin.y),
        static_cast<double>(widthForMetal),
        static_cast<double>(heightForMetal),
        0.0,
        1.0
    };

    [renderCommandEncoder setRenderPipelineState:pipelineState];
    [renderCommandEncoder setViewport:viewport];
    [renderCommandEncoder setFragmentTexture:sourceTexture atIndex:0];
    [renderCommandEncoder setFragmentTexture:sourceSecondPlaneTexture atIndex:1];
    [renderCommandEncoder setFragmentBytes:&arguments length:sizeof(arguments) atIndex:0];
    [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    endEncoding(renderCommandEncoder, commandBuffer);
    // ensureBlitCommandEncoder() would otherwise mint a fresh staging buffer over this one, because it
    // keys off the blit and staged-copy encoders, both of which are nil now. Commit while it can.
    finalizeBlitCommandEncoder();
}

void Queue::setLabel(String&& label)
{
    m_commandQueue.label = label.createNSString().get();
}

void Queue::scheduleWork(Instance::WorkItem&& workItem)
{
    if (auto instance = m_instance.get())
        instance->scheduleWork(WTF::move(workItem));
}

void Queue::clearTextureViewIfNeeded(TextureView& textureView)
{
    Ref parentTexture = textureView.apiParentTexture();
    return clearTextureIfNeeded(parentTexture.get(), textureView.mipLevelCount(), textureView.arrayLayerCount(), textureView.baseMipLevel(), textureView.baseArrayLayer());
}

void Queue::clearTextureViewIfNeeded(Texture& texture)
{
    return clearTextureIfNeeded(texture, texture.mipLevelCount(), texture.arrayLayerCount(), 0, 0);
}

void Queue::clearTextureIfNeeded(Texture& parentTexture, uint32_t mipLevelCount, uint32_t arrayLayerCount, uint32_t baseMipLevel, uint32_t baseArrayLayer)
{
    auto devicePtr = m_device.get();
    if (!devicePtr)
        return;

    for (uint32_t slice = 0; slice < arrayLayerCount; ++slice) {
        for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
            auto checkedParentMipLevel = checkedSum<uint32_t>(baseMipLevel, mipLevel);
            auto checkedParentSlice = checkedSum<uint32_t>(baseArrayLayer, slice);
            if (checkedParentMipLevel.hasOverflowed() || checkedParentSlice.hasOverflowed())
                return;
            auto parentMipLevel = checkedParentMipLevel.value();
            auto parentSlice = checkedParentSlice.value();
            if (parentTexture.previouslyCleared(parentMipLevel, parentSlice))
                continue;

            CommandEncoder::clearTextureIfNeeded(parentTexture, parentMipLevel, parentSlice, *devicePtr, ensureBlitCommandEncoder());
        }
    }
    finalizeBlitCommandEncoder();
}

id<MTLDevice> Queue::metalDevice() const
{
    return device().device();
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void NODELETE wgpuQueueReference(WGPUQueue queue)
{
    WebGPU::fromAPI(queue).ref();
}

void wgpuQueueRelease(WGPUQueue queue)
{
    WebGPU::fromAPI(queue).deref();
}

void wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallback callback, void* userdata)
{
    protect(WebGPU::fromAPI(queue))->onSubmittedWorkDone([callback, userdata](WGPUQueueWorkDoneStatus status) {
        callback(status, userdata);
    });
}

void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, WGPUQueueWorkDoneBlockCallback callback)
{
    protect(WebGPU::fromAPI(queue))->onSubmittedWorkDone([callback = WebGPU::fromAPI(WTF::move(callback))](WGPUQueueWorkDoneStatus status) {
        callback(status);
    });
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands)
{
    Vector<Ref<WebGPU::CommandBuffer>> commandsToForward;
    for (auto& command : unsafeMakeSpan(commands, commandCount))
        commandsToForward.append(protect(WebGPU::fromAPI(command)));
    protect(WebGPU::fromAPI(queue))->submit(WTF::move(commandsToForward));
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, std::span<uint8_t> data)
{
    protect(WebGPU::fromAPI(queue))->writeBuffer(protect(WebGPU::fromAPI(buffer)), bufferOffset, data);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, std::span<uint8_t> data, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    protect(WebGPU::fromAPI(queue))->writeTexture(*destination, data, *dataLayout, *writeSize);
}

void wgpuQueueCopyExternalImageToTexture(WGPUQueue queue, const WGPUImageCopyExternalImage* source, const WGPUImageCopyTextureTagged* destination, const WGPUExtent3D* copySize)
{
    protect(WebGPU::fromAPI(queue))->copyExternalImageToTexture(*source, *destination, *copySize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    protect(WebGPU::fromAPI(queue))->setLabel(WebGPU::fromAPI(label));
}

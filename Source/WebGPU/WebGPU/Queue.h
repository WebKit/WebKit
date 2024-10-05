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

#pragma once

#import "Instance.h"
#import <Metal/Metal.h>
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/Ref.h>
#import <wtf/RetainReleaseSwift.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>

struct WGPUQueueImpl {
};

namespace WebGPU {

class Buffer;
class CommandBuffer;
class Device;
class Texture;
class TextureView;

// https://gpuweb.github.io/gpuweb/#gpuqueue
// A device owns its default queue, not the other way around.
class Queue : public WGPUQueueImpl, public ThreadSafeRefCounted<Queue> {
    WTF_MAKE_TZONE_ALLOCATED(Queue);
public:
    static Ref<Queue> create(id<MTLCommandQueue> commandQueue, Device& device)
    {
        return adoptRef(*new Queue(commandQueue, device));
    }
    static Ref<Queue> createInvalid(Device& device)
    {
        return adoptRef(*new Queue(device));
    }

    ~Queue();

    void onSubmittedWorkDone(CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback);
    void submit(Vector<std::reference_wrapper<CommandBuffer>>&& commands);
    void writeBuffer(Buffer&, uint64_t bufferOffset, std::span<uint8_t> data);
    void writeBuffer(id<MTLBuffer>, uint64_t bufferOffset, std::span<uint8_t> data);
    void writeTexture(const WGPUImageCopyTexture& destination, std::span<uint8_t> data, const WGPUTextureDataLayout&, const WGPUExtent3D& writeSize, bool skipValidation = false);
    void setLabel(String&&);

    void onSubmittedWorkScheduled(Function<void()>&&);

    bool isValid() const { return m_commandQueue; }
    void makeInvalid();
    void setCommittedSignalEvent(id<MTLSharedEvent>, size_t frameIndex);

    const Device& device() const SWIFT_RETURNS_INDEPENDENT_VALUE;
    void clearTextureIfNeeded(const WGPUImageCopyTexture&, NSUInteger);
    id<MTLCommandBuffer> commandBufferWithDescriptor(MTLCommandBufferDescriptor*);
    void commitMTLCommandBuffer(id<MTLCommandBuffer>);
    void removeMTLCommandBuffer(id<MTLCommandBuffer>);
    void setEncoderForBuffer(id<MTLCommandBuffer>, id<MTLCommandEncoder>);
    id<MTLCommandEncoder> encoderForBuffer(id<MTLCommandBuffer>) const;
    void clearTextureViewIfNeeded(TextureView&);
    static bool writeWillCompletelyClear(WGPUTextureDimension, uint32_t widthForMetal, uint32_t logicalSizeWidth, uint32_t heightForMetal, uint32_t logicalSizeHeight, uint32_t depthForMetal, uint32_t logicalSizeDepthOrArrayLayers);
    void endEncoding(id<MTLCommandEncoder>, id<MTLCommandBuffer>) const;

    id<MTLBlitCommandEncoder> ensureBlitCommandEncoder();
    void finalizeBlitCommandEncoder();

private:
    Queue(id<MTLCommandQueue>, Device&);
    Queue(Device&);

    NSString* errorValidatingSubmit(const Vector<std::reference_wrapper<CommandBuffer>>&) const;
    bool validateWriteBuffer(const Buffer&, uint64_t bufferOffset, size_t) const;


    bool isIdle() const;
    bool isSchedulingIdle() const { return m_submittedCommandBufferCount == m_scheduledCommandBufferCount; }
    void removeMTLCommandBufferInternal(id<MTLCommandBuffer>);

    // This can be called on a background thread.
    void scheduleWork(Instance::WorkItem&&);
    NSString* errorValidatingWriteTexture(const WGPUImageCopyTexture&, const WGPUTextureDataLayout&, const WGPUExtent3D&, size_t, const Texture&) const;

    id<MTLCommandQueue> m_commandQueue { nil };
    id<MTLCommandBuffer> m_commandBuffer { nil };
    id<MTLBlitCommandEncoder> m_blitCommandEncoder { nil };
    ThreadSafeWeakPtr<Device> m_device; // The only kind of queues that exist right now are default queues, which are owned by Devices.

    uint64_t m_submittedCommandBufferCount { 0 };
    uint64_t m_completedCommandBufferCount { 0 };
    uint64_t m_scheduledCommandBufferCount { 0 };
    using OnSubmittedWorkScheduledCallbacks = Vector<WTF::Function<void()>>;
    HashMap<uint64_t, OnSubmittedWorkScheduledCallbacks, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_onSubmittedWorkScheduledCallbacks;
    using OnSubmittedWorkDoneCallbacks = Vector<WTF::Function<void(WGPUQueueWorkDoneStatus)>>;
    HashMap<uint64_t, OnSubmittedWorkDoneCallbacks, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_onSubmittedWorkDoneCallbacks;
    NSMutableOrderedSet<id<MTLCommandBuffer>> *m_createdNotCommittedBuffers { nil };
    NSMapTable<id<MTLCommandBuffer>, id<MTLCommandEncoder>> *m_openCommandEncoders;
} SWIFT_SHARED_REFERENCE(retainQueue, releaseQueue);

} // namespace WebGPU

inline void retainQueue(WebGPU::Queue* obj)
{
    WTF::retainThreadSafeRefCounted(obj);
}

inline void releaseQueue(WebGPU::Queue* obj)
{
    WTF::releaseThreadSafeRefCounted(obj);
}


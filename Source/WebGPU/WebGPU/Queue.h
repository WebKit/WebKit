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
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/Ref.h>
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

// https://gpuweb.github.io/gpuweb/#gpuqueue
// A device owns its default queue, not the other way around.
class Queue : public WGPUQueueImpl, public ThreadSafeRefCounted<Queue> {
    WTF_MAKE_FAST_ALLOCATED;
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
    void writeBuffer(const Buffer&, uint64_t bufferOffset, void* data, size_t);
    void writeBuffer(id<MTLBuffer>, uint64_t bufferOffset, void* data, size_t);
    void writeTexture(const WGPUImageCopyTexture& destination, void* data, size_t dataSize, const WGPUTextureDataLayout&, const WGPUExtent3D& writeSize);
    void setLabel(String&&);

    void onSubmittedWorkScheduled(CompletionHandler<void()>&&);

    bool isValid() const { return m_commandQueue; }
    void makeInvalid() { m_commandQueue = nil; }

    const Device& device() const;
    void waitUntilIdle();
    void clearTexture(const WGPUImageCopyTexture&, NSUInteger);
    id<MTLCommandBuffer> commandBufferWithDescriptor(MTLCommandBufferDescriptor*);
    void commitMTLCommandBuffer(id<MTLCommandBuffer>);
    void setEncoderForBuffer(id<MTLCommandBuffer>, id<MTLCommandEncoder>);
    id<MTLCommandEncoder> encoderForBuffer(id<MTLCommandBuffer>) const;

private:
    Queue(id<MTLCommandQueue>, Device&);
    Queue(Device&);

    NSString* errorValidatingSubmit(const Vector<std::reference_wrapper<CommandBuffer>>&) const;
    bool validateWriteBuffer(const Buffer&, uint64_t bufferOffset, size_t) const;

    void ensureBlitCommandEncoder();
    void finalizeBlitCommandEncoder();

    bool isIdle() const;
    bool isSchedulingIdle() const { return m_submittedCommandBufferCount == m_scheduledCommandBufferCount; }

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
    NSMutableSet<id<MTLCommandBuffer>> *m_pendingCommandBuffers { nil };
    NSMutableOrderedSet<id<MTLCommandBuffer>> *m_createdNotCommittedBuffers { nil };
    NSMapTable<id<MTLCommandBuffer>, id<MTLCommandEncoder>> *m_openCommandEncoders;
};

} // namespace WebGPU

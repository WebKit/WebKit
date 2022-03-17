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

namespace WebGPU {

class Buffer;
class CommandBuffer;
class Device;

class Queue : public ThreadSafeRefCounted<Queue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Queue> create(id<MTLCommandQueue> commandQueue, Device& device)
    {
        return adoptRef(*new Queue(commandQueue, device));
    }

    ~Queue();

    void onSubmittedWorkDone(uint64_t signalValue, CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback);
    void submit(Vector<std::reference_wrapper<const CommandBuffer>>&& commands);
    void writeBuffer(const Buffer&, uint64_t bufferOffset, const void* data, size_t);
    void writeTexture(const WGPUImageCopyTexture& destination, const void* data, size_t dataSize, const WGPUTextureDataLayout&, const WGPUExtent3D& writeSize);
    void setLabel(String&&);

    id<MTLCommandQueue> commandQueue() const { return m_commandQueue; }

private:
    Queue(id<MTLCommandQueue>, Device&);

    bool validateSubmit() const;

    // This can be called on a background thread.
    void scheduleWork(Instance::WorkItem&&);

    const id<MTLCommandQueue> m_commandQueue { nil };
    const Device& m_device; // The only kind of queues that exist right now are default queues, which are owned by Devices.

    uint64_t m_submittedCommandBufferCount { 0 };
    uint64_t m_completedCommandBufferCount { 0 };
    using OnSubmittedWorkDoneCallbacks = Vector<WTF::Function<void(WGPUQueueWorkDoneStatus)>>;
    HashMap<uint64_t, OnSubmittedWorkDoneCallbacks> m_onSubmittedWorkDoneCallbacks;
};

} // namespace WebGPU

struct WGPUQueueImpl {
    Ref<WebGPU::Queue> queue;
};

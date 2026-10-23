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

#pragma once

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <wtf/CompletionHandler.h>
#import <wtf/Condition.h>
#import <wtf/Deque.h>
#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/MachSendRight.h>
#import <wtf/Ref.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/ThreadSafetyAnalysis.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>


namespace WTF {
class MachSendRight;
}

namespace WebGPU::Metal {

class Adapter;
class CommandBuffer;
class Device;
class PresentationContext;
class Texture;

// https://gpuweb.github.io/gpuweb/#gpu
class Instance final : public WebGPU::Instance {
    WTF_MAKE_TZONE_ALLOCATED(Instance);
public:
    static Ref<Instance> create(WebGPU::InstanceDescriptor&&);
    static Ref<Instance> createInvalid()
    {
        return adoptRef(*new Instance());
    }

    virtual ~Instance();

    void processEvents();
    void requestAdapter(const WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<WebGPU::Adapter>&&)>&&) final;
    RefPtr<WebGPU::PresentationContext> createPresentationContext(const WebGPU::PresentationContextDescriptor&) final;

    void setLabel(String&&) final { }
    bool isValid() const final { return m_isValid; }
    void retainDevice(Device&, id<MTLCommandBuffer>);
    void retainCommandBuffer(CommandBuffer&, id<MTLCommandBuffer>);
    void releaseCompletedCommandBuffers();
    void waitForCommandBufferCompletions();

    // This can be called on a background thread.
    using WorkItem = Function<void()>;
    void scheduleWork(WorkItem&&);
    const std::optional<const MachSendRight>& LEGACY_NODELETE webProcessID() const;
    id<MTLDevice> device() const;

    // The futures of the C API. A future completes when the callback it stands for has run.
    uint64_t createFuture();
    void completeFuture(uint64_t);
    // Runs the pending work until one of the futures has completed, and calls didComplete with
    // the index of each completed one, which is then forgotten. False when the timeout passes
    // first. This can be called on a background thread.
    bool waitForAnyFuture(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete);

private:
    Instance(Function<void(WorkItem&&)>&& scheduleWork, const WTF::MachSendRight* webProcessResourceOwner);
    explicit Instance();

    // This can be called on a background thread.
    void defaultScheduleWork(WorkItem&&);
    bool anyFutureCompleted(std::span<const uint64_t> futures) const WTF_REQUIRES_LOCK(m_lock);

    // This can be used on a background thread.
    Deque<WorkItem> m_pendingWork WTF_GUARDED_BY_LOCK(m_lock);
    uint64_t m_nextFuture WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_completedFutures WTF_GUARDED_BY_LOCK(m_lock);
    // Signalled when work is appended to m_pendingWork or a future completes.
    Condition m_condition;
    using CommandBufferContainer = Vector<WeakObjCPtr<id<MTLCommandBuffer>>>;
    HashMap<Ref<Device>, CommandBufferContainer> retainedDeviceInstances;
    Vector<std::pair<Ref<CommandBuffer>, WeakObjCPtr<id<MTLCommandBuffer>>>> m_retainedCommandBufferInstances;
    const std::optional<const MachSendRight> m_webProcessID;
    // Null when the work waits in m_pendingWork for processEvents().
    const Function<void(WorkItem&&)> m_scheduleWork;
    Lock m_lock;
    bool m_isValid { true };
};

} // namespace WebGPU::Metal

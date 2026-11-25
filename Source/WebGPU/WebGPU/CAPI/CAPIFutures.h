/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This header requires ARC. Include it only from files compiled with -fobjc-arc.
#endif

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <span>
#import <wtf/Condition.h>
#import <wtf/Deque.h>
#import <wtf/Function.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/Seconds.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/ThreadSafetyAnalysis.h>
#import <wtf/TypeCasts.h>

namespace WebGPU::CAPI {

// The futures of the C API on an instance of the C++ API, and the work that
// wgpuInstanceProcessEvents() and wgpuInstanceWaitAny() run. The instance keeps it as its client
// state, so every instance has one, whichever implementation created the instance. This can be used
// on any thread.
//
// The futures complete when their callbacks have run, in one of these ways:
// - The implementation runs the completion handler on another thread, or schedules it with the
//   scheduleWork of the instance, which wgpuCreateInstance() points to scheduleWork() here. This is
//   the Metal implementation.
// - The completion handler runs only with the help of the waiting thread, through
//   WebGPU::Instance::dispatchEvents(). This is the Web Process implementation, which dispatches the
//   replies from the GPU Process.
class EventManager final : public InstanceClientState {
public:
    // The event manager of the instance, which is created the first time.
    static Ref<EventManager> ensure(WebGPU::Instance&);
    // An event manager for an instance that is about to be created, so that the instance can
    // schedule its work into it. attach() gives it to the instance.
    static Ref<EventManager> create();
    void attach(WebGPU::Instance&);

    uint64_t createFuture();
    void completeFuture(uint64_t future);

    // The work runs in processEvents() or waitAny(), on the thread that calls them.
    void scheduleWork(Function<void()>&&);
    // Dispatches the completions that the instance has received, and runs the scheduled work.
    void processEvents();

    enum class WaitStatus : uint8_t { Completed, TimedOut, Failed };
    // Dispatches the completions of the instance and runs the scheduled work until one of the futures
    // has completed, and calls didComplete with the index of each completed one, which is then
    // forgotten. Failed when the instance cannot dispatch its completions.
    WaitStatus waitAny(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete);

private:
    EventManager() = default;

    RefPtr<WebGPU::Instance> instance() const;
    void setInstance(WebGPU::Instance&);
    void runScheduledWork();
    // Dispatches the completions that the instance has received. False when it cannot.
    bool dispatchReceivedEvents();
    bool anyFutureCompleted(std::span<const uint64_t> futures) const WTF_REQUIRES_LOCK(m_lock);

    mutable Lock m_lock;
    // Signalled when work is scheduled or a future completes.
    Condition m_condition;
    // Weak, because the instance keeps this.
    ThreadSafeWeakPtr<WebGPU::Instance> m_instance WTF_GUARDED_BY_LOCK(m_lock);
    Deque<Function<void()>> m_scheduledWork WTF_GUARDED_BY_LOCK(m_lock);
    uint64_t m_nextFuture WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    HashSet<uint64_t> m_completedFutures WTF_GUARDED_BY_LOCK(m_lock);
};

// A future of the C API on an instance, for one callback. complete() is called after the callback
// has run. Without an instance, the future is WGPU_FUTURE_INIT, which nothing waits on. A future
// keeps its instance alive, so that the instance can still schedule the completion.
class Future {
public:
    explicit Future(RefPtr<WebGPU::Instance>&&);

    WGPUFuture future() const { return { m_id }; }
    void complete() const;

private:
    const RefPtr<WebGPU::Instance> m_instance;
    const RefPtr<EventManager> m_events;
    const uint64_t m_id { 0 };
};

} // namespace WebGPU::CAPI

// Only the C API sets the client state of an instance, and it sets an EventManager.
SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::CAPI::EventManager)
    static bool isType(const WebGPU::InstanceClientState&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

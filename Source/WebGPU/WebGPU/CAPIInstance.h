/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <atomic>
#import <span>
#import <wtf/Condition.h>
#import <wtf/Deque.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/Seconds.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/ThreadSafetyAnalysis.h>

// The WebGPU C API (webgpu.h) keeps its state in WebGPU::CAPI, over the WebGPU C++ API. These files
// see only the C++ API, never the implementation that created its objects.

namespace WebGPU::CAPI {

// The futures of an instance of the C API, and the work that wgpuInstanceProcessEvents() and
// wgpuInstanceWaitAny() run. A future completes when the callback it stands for has been called.
// This can be used on any thread.
class EventManager final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<EventManager> {
public:
    static Ref<EventManager> create() { return adoptRef(*new EventManager); }

    uint64_t createFuture();
    void completeFuture(uint64_t);

    // The work runs in the next processEvents(), or at once after shutDown().
    void scheduleWork(Function<void()>&&);
    void processEvents();
    // Runs the pending work until one of the futures has completed, and calls didComplete with the
    // index of each completed one, which is then forgotten. False when the timeout passes first.
    bool waitAny(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete);

    // Calls the cancellation of a future when the event manager is shut down, unless the future is
    // removed first.
    void addCancellation(uint64_t future, Function<void()>&&);
    void removeCancellation(uint64_t future);

    // For the release of the last reference to the instance. As Dawn's event manager does, it calls
    // the cancellations of the callbacks that the instance can no longer call. Then it runs the
    // pending work, so that every completion handler of the C++ API is called; the callbacks of the
    // C API that were cancelled are not called again.
    void shutDown();

private:
    EventManager() = default;

    bool anyFutureCompleted(std::span<const uint64_t> futures) const WTF_REQUIRES_LOCK(m_lock);

    Lock m_lock;
    // Signalled when work is appended to m_pendingWork or a future completes.
    Condition m_condition;
    Deque<Function<void()>> m_pendingWork WTF_GUARDED_BY_LOCK(m_lock);
    uint64_t m_nextFuture WTF_GUARDED_BY_LOCK(m_lock) { 1 };
    HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_completedFutures WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<uint64_t, Function<void()>, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_cancellations WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isShutDown WTF_GUARDED_BY_LOCK(m_lock) { false };
};

// What a WGPUInstance points to: the instance of the C++ API, and the events of the C API. The C++
// instance schedules its work into the event manager. Adapters and devices do not keep the
// instance alive, so releasing the last reference to the instance destroys it.
class Instance final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Instance> {
public:
    // Null when the C++ API cannot create an instance.
    static RefPtr<Instance> create();
    ~Instance();

    WebGPU::Instance& instance() const { return m_instance; }
    EventManager& events() const { return m_events; }

private:
    Instance(Ref<EventManager>&&, Ref<WebGPU::Instance>&&);

    const Ref<EventManager> m_events;
    const Ref<WebGPU::Instance> m_instance;
};

inline Instance& fromAPI(WGPUInstance handle)
{
    return *reinterpret_cast<Instance*>(handle);
}

inline WGPUInstance toAPI(Instance& instance)
{
    return reinterpret_cast<WGPUInstance>(&instance);
}

// The handle of a new reference, for the C API.
inline WGPUInstance releaseToAPI(Ref<Instance>&& instance)
{
    return toAPI(instance.leakRef());
}

// The state that the copies of a Future share.
class FutureState final : public ThreadSafeRefCounted<FutureState> {
public:
    static Ref<FutureState> create(RefPtr<EventManager>&& events) { return adoptRef(*new FutureState(WTF::move(events))); }

    uint64_t id() const { return m_id; }
    // True for the first claim only, so that the callback is called once.
    bool claim() { return !m_isClaimed.exchange(true); }
    // Forgets the cancellation, and completes the future.
    void didComplete();

private:
    explicit FutureState(RefPtr<EventManager>&&);

    const ThreadSafeWeakPtr<EventManager> m_events;
    const uint64_t m_id { 0 };
    std::atomic<bool> m_isClaimed { false };
};

// A future of the C API on an instance, for one callback. As in Dawn, the callback is called once:
// with the result, by complete(), or, unless its mode is WGPUCallbackMode_AllowSpontaneous, with the
// cancelled status when the last reference to the instance is released first. A later complete()
// does not call it again. Without an instance, the future is WGPU_FUTURE_INIT, which nothing waits
// on, and complete() always calls the callback. A future does not keep its instance alive.
class Future {
public:
    // callCancelledCallback calls the callback with the cancelled status.
    Future(const Instance*, WGPUCallbackMode, Function<void()>&& callCancelledCallback);

    WGPUFuture future() const { return { m_state->id() }; }

    // Calls the callback with callCallback, unless it was cancelled, and completes the future. False
    // when the callback was cancelled: the caller then releases the objects it was to hand out.
    template<typename CallCallback>
    bool complete(NOESCAPE CallCallback&& callCallback) const
    {
        bool isClaimed = m_state->claim();
        if (isClaimed)
            callCallback();
        m_state->didComplete();
        return isClaimed;
    }

private:
    Ref<FutureState> m_state;
};

} // namespace WebGPU::CAPI

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

#import "config.h"
#import "CAPIFutures.h"

#import <wtf/IndexedRange.h>
#import <wtf/MonotonicTime.h>

namespace WebGPU::CAPI {

Ref<EventManager> EventManager::create()
{
    return adoptRef(*new EventManager);
}

Ref<EventManager> EventManager::ensure(WebGPU::Instance& instance)
{
    Ref state = instance.ensureClientState([&] -> Ref<InstanceClientState> {
        Ref events = EventManager::create();
        events->setInstance(instance);
        return events;
    });
    return downcast<EventManager>(WTF::move(state));
}

void EventManager::attach(WebGPU::Instance& instance)
{
    setInstance(instance);
    Ref state = instance.ensureClientState([&] -> Ref<InstanceClientState> {
        return Ref<InstanceClientState> { *this };
    });
    ASSERT_UNUSED(state, state.ptr() == this);
}

RefPtr<WebGPU::Instance> EventManager::instance() const
{
    Locker locker { m_lock };
    return m_instance.get();
}

void EventManager::setInstance(WebGPU::Instance& instance)
{
    Locker locker { m_lock };
    m_instance = instance;
}

uint64_t EventManager::createFuture()
{
    Locker locker { m_lock };
    return m_nextFuture++;
}

void EventManager::completeFuture(uint64_t future)
{
    Locker locker { m_lock };
    m_completedFutures.add(future);
    m_condition.notifyAll();
}

void EventManager::scheduleWork(Function<void()>&& work)
{
    Locker locker { m_lock };
    m_scheduledWork.append(WTF::move(work));
    m_condition.notifyAll();
}

void EventManager::runScheduledWork()
{
    while (true) {
        Deque<Function<void()>> work;
        {
            Locker locker { m_lock };
            std::swap(m_scheduledWork, work);
        }
        if (work.isEmpty())
            return;
        for (auto& workItem : work)
            workItem();
    }
}

bool EventManager::dispatchReceivedEvents()
{
    RefPtr instance = this->instance();
    if (!instance)
        return true;
    // A zero timeout dispatches the completions that have arrived, one per call.
    while (true) {
        switch (instance->dispatchEvents(0_s)) {
        case DispatchEventsResult::Dispatched:
            continue;
        case DispatchEventsResult::NotNeeded:
        case DispatchEventsResult::TimedOut:
            return true;
        case DispatchEventsResult::Failed:
            return false;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void EventManager::processEvents()
{
    // An instance that cannot dispatch its completions leaves its futures pending: processEvents()
    // reports nothing.
    dispatchReceivedEvents();
    runScheduledWork();
}

auto EventManager::waitAny(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete) -> WaitStatus
{
    auto deadline = MonotonicTime::timePointFromNow(timeout);
    RefPtr instance = this->instance();
    // A completion that was dispatched may be of another future. The wait goes on past the deadline
    // until no more completions have arrived, so that a zero timeout dispatches all of them.
    bool canTimeOut = false;
    while (true) {
        runScheduledWork();
        {
            Locker locker { m_lock };
            if (anyFutureCompleted(futures)) {
                for (auto [index, future] : indexedRange(futures)) {
                    if (m_completedFutures.remove(future))
                        didComplete(index);
                }
                return WaitStatus::Completed;
            }
        }

        auto now = MonotonicTime::now();
        if (canTimeOut && now >= deadline)
            return WaitStatus::TimedOut;
        // The instance runs the completion handlers that need the help of this thread, which complete
        // the futures, so this runs outside the lock.
        switch (instance ? instance->dispatchEvents(std::max(deadline - now, 0_s)) : DispatchEventsResult::NotNeeded) {
        case DispatchEventsResult::Dispatched:
            canTimeOut = false;
            continue;
        case DispatchEventsResult::TimedOut:
            canTimeOut = true;
            continue;
        case DispatchEventsResult::Failed:
            return WaitStatus::Failed;
        case DispatchEventsResult::NotNeeded:
            break;
        }

        // The completions arrive as scheduled work, or on other threads.
        Locker locker { m_lock };
        if (m_scheduledWork.isEmpty() && !m_condition.waitUntil(m_lock, deadline, [&] WTF_REQUIRES_LOCK(m_lock) { return !m_scheduledWork.isEmpty() || anyFutureCompleted(futures); }))
            return WaitStatus::TimedOut;
    }
}

bool EventManager::anyFutureCompleted(std::span<const uint64_t> futures) const
{
    for (auto future : futures) {
        if (m_completedFutures.contains(future))
            return true;
    }
    return false;
}

Future::Future(RefPtr<WebGPU::Instance>&& instance)
    : m_instance(WTF::move(instance))
    , m_events(m_instance ? RefPtr { EventManager::ensure(*m_instance) } : nullptr)
    , m_id(m_events ? m_events->createFuture() : 0)
{
}

void Future::complete() const
{
    if (RefPtr events = m_events)
        events->completeFuture(m_id);
}

} // namespace WebGPU::CAPI

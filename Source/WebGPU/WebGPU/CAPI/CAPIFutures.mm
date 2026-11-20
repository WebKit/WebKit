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

#import <optional>
#import <wtf/IndexedRange.h>
#import <wtf/MonotonicTime.h>
#import <wtf/Vector.h>

namespace WebGPU::CAPI {

Ref<EventManager> EventManager::create()
{
    return adoptRef(*new EventManager);
}

Ref<EventManager> EventManager::ensure(WebGPU::Instance& instance)
{
    Ref state = instance.ensureClientState([] -> Ref<InstanceClientState> {
        return EventManager::create();
    });
    return downcast<EventManager>(WTF::move(state));
}

void EventManager::attach(WebGPU::Instance& instance)
{
    Ref state = instance.ensureClientState([&] -> Ref<InstanceClientState> {
        return Ref<InstanceClientState> { *this };
    });
    ASSERT_UNUSED(state, state.ptr() == this);
}

uint64_t EventManager::createFuture()
{
    Locker locker { m_lock };
    auto future = m_nextFuture++;
    m_pendingFutures.add(future, nullptr);
    return future;
}

void EventManager::setWaiter(uint64_t future, CompletionWaiter&& waiter)
{
    if (!waiter)
        return;
    // The caller destroys the waiter of a future that has completed, outside the lock.
    Locker locker { m_lock };
    auto it = m_pendingFutures.find(future);
    if (it != m_pendingFutures.end())
        it->value = WTF::move(waiter);
}

void EventManager::completeFuture(uint64_t future)
{
    CompletionWaiter waiter;
    {
        Locker locker { m_lock };
        waiter = m_pendingFutures.take(future);
        m_completedFutures.add(future);
        m_condition.notifyAll();
    }
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

bool EventManager::callWaiter(uint64_t future, Seconds timeout)
{
    // The waiter dispatches the completion, which completes the future, so it runs outside the lock.
    CompletionWaiter waiter;
    {
        Locker locker { m_lock };
        auto it = m_pendingFutures.find(future);
        if (it == m_pendingFutures.end() || !it->value)
            return true;
        waiter = WTF::move(it->value);
    }
    bool succeeded = waiter(timeout);
    {
        Locker locker { m_lock };
        auto it = m_pendingFutures.find(future);
        if (it != m_pendingFutures.end() && !it->value)
            it->value = WTF::move(waiter);
    }
    return succeeded;
}

void EventManager::processEvents()
{
    Vector<uint64_t> futuresWithWaiters;
    {
        Locker locker { m_lock };
        for (auto& entry : m_pendingFutures) {
            if (entry.value)
                futuresWithWaiters.append(entry.key);
        }
    }
    // A waiter that fails leaves its future pending: processEvents() reports nothing.
    for (auto future : futuresWithWaiters)
        callWaiter(future, 0_s);
    runScheduledWork();
}

auto EventManager::waitAny(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete) -> WaitStatus
{
    auto deadline = MonotonicTime::timePointFromNow(timeout);
    bool waitFailed = false;
    while (true) {
        // A zero timeout dispatches the completions that have arrived.
        for (auto future : futures)
            waitFailed |= !callWaiter(future, 0_s);
        runScheduledWork();

        std::optional<uint64_t> futureToWaitFor;
        {
            Locker locker { m_lock };
            if (anyFutureCompleted(futures)) {
                for (auto [index, future] : indexedRange(futures)) {
                    if (m_completedFutures.remove(future))
                        didComplete(index);
                }
                return WaitStatus::Completed;
            }
            if (waitFailed)
                return WaitStatus::Failed;
            for (auto future : futures) {
                if (hasWaiter(future)) {
                    futureToWaitFor = future;
                    break;
                }
            }
            // Without waiters, the completions arrive as scheduled work or on other threads.
            if (!futureToWaitFor) {
                if (m_scheduledWork.isEmpty() && !m_condition.waitUntil(m_lock, deadline, [&] WTF_REQUIRES_LOCK(m_lock) { return !m_scheduledWork.isEmpty() || anyFutureCompleted(futures); }))
                    return WaitStatus::TimedOut;
                continue;
            }
        }
        auto now = MonotonicTime::now();
        if (now >= deadline)
            return WaitStatus::TimedOut;
        waitFailed |= !callWaiter(*futureToWaitFor, deadline - now);
    }
}

bool EventManager::hasWaiter(uint64_t future) const
{
    auto it = m_pendingFutures.find(future);
    return it != m_pendingFutures.end() && it->value;
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

void Future::setWaiter(CompletionWaiter&& waiter) const
{
    if (RefPtr events = m_events)
        events->setWaiter(m_id, WTF::move(waiter));
}

void Future::complete() const
{
    if (RefPtr events = m_events)
        events->completeFuture(m_id);
}

} // namespace WebGPU::CAPI

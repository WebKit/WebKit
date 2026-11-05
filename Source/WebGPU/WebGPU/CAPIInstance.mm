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

#import "config.h"
#import "CAPIInstance.h"

#import <wtf/IndexedRange.h>
#import <wtf/MonotonicTime.h>

namespace WebGPU::CAPI {

uint64_t EventManager::createFuture()
{
    Locker locker(m_lock);
    return m_nextFuture++;
}

void EventManager::completeFuture(uint64_t future)
{
    Locker locker(m_lock);
    m_completedFutures.add(future);
    m_condition.notifyAll();
}

void EventManager::scheduleWork(Function<void()>&& work)
{
    {
        Locker locker(m_lock);
        if (!m_isShutDown) {
            m_pendingWork.append(WTF::move(work));
            m_condition.notifyAll();
            return;
        }
    }
    work();
}

void EventManager::processEvents()
{
    while (true) {
        Deque<Function<void()>> work;
        {
            Locker locker(m_lock);
            std::swap(m_pendingWork, work);
        }
        if (work.isEmpty())
            return;
        for (auto& workItem : work)
            workItem();
    }
}

bool EventManager::waitAny(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete)
{
    auto deadline = MonotonicTime::timePointFromNow(timeout);
    while (true) {
        processEvents();
        {
            Locker locker(m_lock);
            if (anyFutureCompleted(futures)) {
                for (auto [index, future] : indexedRange(futures)) {
                    if (m_completedFutures.remove(future))
                        didComplete(index);
                }
                return true;
            }
            if (m_pendingWork.isEmpty() && !m_condition.waitUntil(m_lock, deadline, [&] WTF_REQUIRES_LOCK(m_lock) { return !m_pendingWork.isEmpty() || anyFutureCompleted(futures); }))
                return false;
        }
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

void EventManager::addCancellation(uint64_t future, Function<void()>&& cancellation)
{
    Locker locker(m_lock);
    m_cancellations.add(future, WTF::move(cancellation));
}

void EventManager::removeCancellation(uint64_t future)
{
    // The cancellation can hold the last reference to an event, so it is destroyed outside the lock.
    Function<void()> cancellation;
    {
        Locker locker(m_lock);
        cancellation = m_cancellations.take(future);
    }
}

void EventManager::shutDown()
{
    decltype(m_cancellations) cancellations;
    {
        Locker locker(m_lock);
        m_isShutDown = true;
        cancellations = std::exchange(m_cancellations, { });
    }
    for (auto& cancellation : cancellations.values())
        cancellation();
    processEvents();
}

RefPtr<Instance> Instance::create()
{
    Ref events = EventManager::create();
    RefPtr instance = WebGPU::createInstance({
        .scheduleWork = [events](Function<void()>&& work) {
            events->scheduleWork(WTF::move(work));
        },
    });
    if (!instance)
        return nullptr;
    return adoptRef(*new Instance(WTF::move(events), instance.releaseNonNull()));
}

Instance::Instance(Ref<EventManager>&& events, Ref<WebGPU::Instance>&& instance)
    : m_events(WTF::move(events))
    , m_instance(WTF::move(instance))
{
}

Instance::~Instance()
{
    m_events->shutDown();
}

FutureState::FutureState(RefPtr<EventManager>&& events)
    : m_events(events)
    , m_id(events ? events->createFuture() : 0)
{
}

void FutureState::didComplete()
{
    if (RefPtr events = m_events) {
        events->removeCancellation(m_id);
        events->completeFuture(m_id);
    }
}

Future::Future(const Instance* instance, WGPUCallbackMode mode, Function<void()>&& callCancelledCallback)
    : m_state(FutureState::create(instance ? RefPtr { &instance->events() } : nullptr))
{
    if (!instance || mode == WGPUCallbackMode_AllowSpontaneous)
        return;
    protect(instance->events())->addCancellation(m_state->id(), [state = m_state.copyRef(), callCancelledCallback = WTF::move(callCancelledCallback)] {
        if (state->claim())
            callCancelledCallback();
    });
}

} // namespace WebGPU::CAPI

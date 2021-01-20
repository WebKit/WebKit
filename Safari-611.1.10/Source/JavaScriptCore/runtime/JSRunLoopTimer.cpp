/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSRunLoopTimer.h"

#include "IncrementalSweeper.h"
#include "VM.h"
#include <mutex>
#include <wtf/NoTailCalls.h>

#if USE(GLIB_EVENT_LOOP)
#include <glib.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace JSC {

static inline JSRunLoopTimer::Manager::EpochTime epochTime(Seconds delay)
{
    return MonotonicTime::now().secondsSinceEpoch() + delay;
}

JSRunLoopTimer::Manager::PerVMData::PerVMData(Manager& manager, RunLoop& runLoop)
    : runLoop(runLoop)
    , timer(makeUnique<RunLoop::Timer<Manager>>(runLoop, &manager, &JSRunLoopTimer::Manager::timerDidFireCallback))
{
#if USE(GLIB_EVENT_LOOP)
    timer->setPriority(RunLoopSourcePriority::JavascriptTimer);
    timer->setName("[JavaScriptCore] JSRunLoopTimer");
#endif
}

void JSRunLoopTimer::Manager::timerDidFireCallback()
{
    timerDidFire();
}

JSRunLoopTimer::Manager::PerVMData::~PerVMData()
{
    // Because RunLoop::Timer is not reference counted, we need to deallocate it
    // on the same thread on which it fires; otherwise, we might deallocate it
    // while it's firing.
    runLoop->dispatch([timer = WTFMove(timer)] {
    });
}

void JSRunLoopTimer::Manager::timerDidFire()
{
    Vector<Ref<JSRunLoopTimer>> timersToFire;

    {
        auto locker = holdLock(m_lock);
        RunLoop* currentRunLoop = &RunLoop::current();
        EpochTime nowEpochTime = epochTime(0_s);
        for (auto& entry : m_mapping) {
            PerVMData& data = *entry.value;
            if (data.runLoop.ptr() != currentRunLoop)
                continue;
            
            EpochTime scheduleTime = epochTime(s_decade);
            for (size_t i = 0; i < data.timers.size(); ++i) {
                {
                    auto& pair = data.timers[i];
                    if (pair.second > nowEpochTime) {
                        scheduleTime = std::min(pair.second, scheduleTime);
                        continue;
                    }
                    auto& last = data.timers.last();
                    if (&last != &pair)
                        std::swap(pair, last);
                    --i;
                }

                auto pair = data.timers.takeLast();
                timersToFire.append(WTFMove(pair.first));
            }

            data.timer->startOneShot(std::max(0_s, scheduleTime - MonotonicTime::now().secondsSinceEpoch()));
        }
    }

    for (auto& timer : timersToFire)
        timer->timerDidFire();
}

JSRunLoopTimer::Manager& JSRunLoopTimer::Manager::shared()
{
    static Manager* manager;
    static std::once_flag once;
    std::call_once(once, [&] {
        manager = new Manager;
    });
    return *manager;
}

void JSRunLoopTimer::Manager::registerVM(VM& vm)
{
    auto data = makeUnique<PerVMData>(*this, vm.runLoop());

    auto locker = holdLock(m_lock);
    auto addResult = m_mapping.add({ vm.apiLock() }, WTFMove(data));
    RELEASE_ASSERT(addResult.isNewEntry);
}

void JSRunLoopTimer::Manager::unregisterVM(VM& vm)
{
    auto locker = holdLock(m_lock);

    auto iter = m_mapping.find({ vm.apiLock() });
    RELEASE_ASSERT(iter != m_mapping.end());
    m_mapping.remove(iter);
}

void JSRunLoopTimer::Manager::scheduleTimer(JSRunLoopTimer& timer, Seconds delay)
{
    EpochTime fireEpochTime = epochTime(delay);

    auto locker = holdLock(m_lock);
    auto iter = m_mapping.find(timer.m_apiLock);
    RELEASE_ASSERT(iter != m_mapping.end()); // We don't allow calling this after the VM dies.

    PerVMData& data = *iter->value;
    EpochTime scheduleTime = fireEpochTime;
    bool found = false;
    for (auto& entry : data.timers) {
        if (entry.first.ptr() == &timer) {
            entry.second = fireEpochTime;
            found = true;
        }
        scheduleTime = std::min(scheduleTime, entry.second);
    }

    if (!found)
        data.timers.append({ timer, fireEpochTime });

    data.timer->startOneShot(std::max(0_s, scheduleTime - MonotonicTime::now().secondsSinceEpoch()));
}

void JSRunLoopTimer::Manager::cancelTimer(JSRunLoopTimer& timer)
{
    auto locker = holdLock(m_lock);
    auto iter = m_mapping.find(timer.m_apiLock);
    if (iter == m_mapping.end()) {
        // It's trivial to allow this to be called after the VM dies, so we allow for it.
        return;
    }

    PerVMData& data = *iter->value;
    EpochTime scheduleTime = epochTime(s_decade);
    for (unsigned i = 0; i < data.timers.size(); ++i) {
        {
            auto& entry = data.timers[i];
            if (entry.first.ptr() == &timer) {
                RELEASE_ASSERT(timer.refCount() >= 2); // If we remove it from the entry below, we should not be the last thing pointing to it!
                auto& last = data.timers.last();
                if (&last != &entry)
                    std::swap(entry, last);
                data.timers.removeLast();
                i--;
                continue;
            }
        }

        scheduleTime = std::min(scheduleTime, data.timers[i].second);
    }

    data.timer->startOneShot(std::max(0_s, scheduleTime - MonotonicTime::now().secondsSinceEpoch()));
}

Optional<Seconds> JSRunLoopTimer::Manager::timeUntilFire(JSRunLoopTimer& timer)
{
    auto locker = holdLock(m_lock);
    auto iter = m_mapping.find(timer.m_apiLock);
    RELEASE_ASSERT(iter != m_mapping.end()); // We only allow this to be called with a live VM.

    PerVMData& data = *iter->value;
    for (auto& entry : data.timers) {
        if (entry.first.ptr() == &timer) {
            EpochTime nowEpochTime = epochTime(0_s);
            return entry.second - nowEpochTime;
        }
    }

    return WTF::nullopt;
}

void JSRunLoopTimer::timerDidFire()
{
    NO_TAIL_CALLS();

    {
        auto locker = holdLock(m_lock);
        if (!m_isScheduled) {
            // We raced between this callback being called and cancel() being called.
            // That's fine, we just don't do anything here.
            return;
        }
    }

    auto locker = holdLock(m_apiLock.get());
    RefPtr<VM> vm = m_apiLock->vm();
    if (!vm) {
        // The VM has been destroyed, so we should just give up.
        return;
    }

    doWork(*vm);
}

JSRunLoopTimer::JSRunLoopTimer(VM& vm)
    : m_apiLock(vm.apiLock())
{
}

JSRunLoopTimer::~JSRunLoopTimer()
{
}

Optional<Seconds> JSRunLoopTimer::timeUntilFire()
{
    return Manager::shared().timeUntilFire(*this);
}

void JSRunLoopTimer::setTimeUntilFire(Seconds intervalInSeconds)
{
    {
        auto locker = holdLock(m_lock);
        m_isScheduled = true;
        Manager::shared().scheduleTimer(*this, intervalInSeconds);
    }

    auto locker = holdLock(m_timerCallbacksLock);
    for (auto& task : m_timerSetCallbacks)
        task->run();
}

void JSRunLoopTimer::cancelTimer()
{
    auto locker = holdLock(m_lock);
    m_isScheduled = false;
    Manager::shared().cancelTimer(*this);
}

void JSRunLoopTimer::addTimerSetNotification(TimerNotificationCallback callback)
{
    auto locker = holdLock(m_timerCallbacksLock);
    m_timerSetCallbacks.add(callback);
}

void JSRunLoopTimer::removeTimerSetNotification(TimerNotificationCallback callback)
{
    auto locker = holdLock(m_timerCallbacksLock);
    m_timerSetCallbacks.remove(callback);
}

} // namespace JSC

/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "ThreadTimers.h"

#include "MainThreadSharedTimer.h"
#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "Timer.h"
#include <wtf/MainThread.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

namespace WebCore {

// Fire timers for this length of time, and then quit to let the run loop process user input events.
static constexpr auto maxDurationOfFiringTimers { 16_ms };

// Timers are created, started and fired on the same thread, and each thread has its own ThreadTimers
// copy to keep the heap and a set of currently firing timers.

ThreadTimers::ThreadTimers()
{
    if (isUIThread())
        setSharedTimer(&MainThreadSharedTimer::singleton());
}

// A worker thread may initialize SharedTimer after some timers are created.
// Also, SharedTimer can be replaced with nullptr before all timers are destroyed.
void ThreadTimers::setSharedTimer(SharedTimer* sharedTimer)
{
    if (m_sharedTimer) {
        m_sharedTimer->setFiredFunction(nullptr);
        m_sharedTimer->stop();
        m_pendingSharedTimerFireTime = MonotonicTime { };
    }
    
    m_sharedTimer = sharedTimer;
    
    if (sharedTimer) {
        m_sharedTimer->setFiredFunction([] { threadGlobalData().threadTimers().sharedTimerFiredInternal(); });
        updateSharedTimer();
    }
}

void ThreadTimers::updateSharedTimer()
{
    if (!m_sharedTimer)
        return;

    while (!m_timerHeap.isEmpty() && !m_timerHeap.first()->hasTimer()) {
        ASSERT_NOT_REACHED();
        TimerBase::heapDeleteNullMin(m_timerHeap);
    }
    ASSERT(m_timerHeap.isEmpty() || m_timerHeap.first()->hasTimer());

    if (m_firingTimers || m_timerHeap.isEmpty()) {
        m_pendingSharedTimerFireTime = MonotonicTime { };
        m_sharedTimer->stop();
    } else {
        MonotonicTime nextFireTime = m_timerHeap.first()->time;
        MonotonicTime currentMonotonicTime = MonotonicTime::now();
        if (m_pendingSharedTimerFireTime) {
            // No need to restart the timer if both the pending fire time and the new fire time are in the past.
            if (m_pendingSharedTimerFireTime <= currentMonotonicTime && nextFireTime <= currentMonotonicTime)
                return;
        } 
        m_pendingSharedTimerFireTime = nextFireTime;
        m_sharedTimer->setFireInterval(std::max(nextFireTime - currentMonotonicTime, 0_s));
    }
}

void ThreadTimers::sharedTimerFiredInternal()
{
    ASSERT(isMainThread() || (!isWebThread() && !isUIThread()));
    // Do a re-entrancy check.
    if (m_firingTimers)
        return;
    m_firingTimers = true;
    m_pendingSharedTimerFireTime = MonotonicTime { };

    MonotonicTime fireTime = MonotonicTime::now();
    MonotonicTime timeToQuit = fireTime + maxDurationOfFiringTimers;

    while (!m_timerHeap.isEmpty()) {
        Ref<ThreadTimerHeapItem> item = *m_timerHeap.first();
        ASSERT(item->hasTimer());
        if (!item->hasTimer()) {
            TimerBase::heapDeleteNullMin(m_timerHeap);
            continue;
        }

        if (item->time > fireTime)
            break;

        auto& timer = item->timer();
        Seconds interval = timer.repeatInterval();
        timer.setNextFireTime(interval ? fireTime + interval : MonotonicTime { });

        // Once the timer has been fired, it may be deleted, so do nothing else with it after this point.
        item->timer().fired();

        // Catch the case where the timer asked timers to fire in a nested event loop, or we are over time limit.
        if (!m_firingTimers || timeToQuit < MonotonicTime::now())
            break;

        if (m_shouldBreakFireLoopForRenderingUpdate)
            break;
    }

    m_firingTimers = false;
    m_shouldBreakFireLoopForRenderingUpdate = false;

    updateSharedTimer();
}

void ThreadTimers::fireTimersInNestedEventLoop()
{
    // Reset the reentrancy guard so the timers can fire again.
    m_firingTimers = false;

    if (m_sharedTimer) {
        m_sharedTimer->invalidate();
        m_pendingSharedTimerFireTime = MonotonicTime { };
    }

    updateSharedTimer();
}

void ThreadTimers::breakFireLoopForRenderingUpdate()
{
    if (!m_firingTimers)
        return;
    m_shouldBreakFireLoopForRenderingUpdate = true;
}

} // namespace WebCore


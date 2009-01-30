/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "Timer.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

// Timers are created, started and fired on the same thread, and each thread has its own ThreadTimers
// copy to keep the heap and a set of currently firing timers.

static MainThreadSharedTimer* mainThreadSharedTimer()
{
    static MainThreadSharedTimer* timer = new MainThreadSharedTimer;
    return timer;
}

ThreadTimers::ThreadTimers()
    : m_sharedTimer(0)
    , m_firingTimers(false)
{
    if (isMainThread())
        setSharedTimer(mainThreadSharedTimer());
}

// A worker thread may initialize SharedTimer after some timers are created.
// Also, SharedTimer can be replaced with 0 before all timers are destroyed.
void ThreadTimers::setSharedTimer(SharedTimer* sharedTimer)
{
    if (m_sharedTimer) {
        m_sharedTimer->setFiredFunction(0);
        m_sharedTimer->stop();
    }
    
    m_sharedTimer = sharedTimer;
    
    if (sharedTimer) {
        m_sharedTimer->setFiredFunction(ThreadTimers::sharedTimerFired);
        updateSharedTimer();
    }
}

void ThreadTimers::updateSharedTimer()
{
    if (!m_sharedTimer)
        return;
        
    if (m_firingTimers || m_timerHeap.isEmpty())
        m_sharedTimer->stop();
    else
        m_sharedTimer->setFireTime(m_timerHeap.first()->m_nextFireTime);
}


void ThreadTimers::collectFiringTimers(double fireTime, Vector<TimerBase*>& firingTimers)
{
    while (!m_timerHeap.isEmpty() && m_timerHeap.first()->m_nextFireTime <= fireTime) {
        TimerBase* timer = m_timerHeap.first();
        firingTimers.append(timer);
        m_timersReadyToFire.add(timer);
        timer->m_nextFireTime = 0;
        timer->heapDeleteMin();
    }
}

void ThreadTimers::fireTimers(double fireTime, const Vector<TimerBase*>& firingTimers)
{
    size_t size = firingTimers.size();
    for (size_t i = 0; i != size; ++i) {
        TimerBase* timer = firingTimers[i];

        // If not in the set, this timer has been deleted or re-scheduled in another timer's fired function.
        // So either we don't want to fire it at all or we will fire it next time the shared timer goes off.
        // It might even have been deleted; that's OK because we won't do anything else with the pointer.
        if (!m_timersReadyToFire.contains(timer))
            continue;

        // Setting the next fire time has a side effect of removing the timer from the firing timers set.
        double interval = timer->repeatInterval();
        timer->setNextFireTime(interval ? fireTime + interval : 0);

        // Once the timer has been fired, it may be deleted, so do nothing else with it after this point.
        timer->fired();

        // Catch the case where the timer asked timers to fire in a nested event loop.
        if (!m_firingTimers)
            break;
    }
}

void ThreadTimers::sharedTimerFired()
{
    // Redirect to non-static method.
    threadGlobalData().threadTimers().sharedTimerFiredInternal();
}

void ThreadTimers::sharedTimerFiredInternal()
{
    // Do a re-entrancy check.
    if (m_firingTimers)
        return;
    m_firingTimers = true;

    double fireTime = currentTime();
    Vector<TimerBase*> firingTimers;

    // m_timersReadyToFire will initially contain the same set as firingTimers, but
    // as timers fire some mat become re-scheduled or deleted. They get removed from
    // m_timersReadyToFire so we can avoid firing them.
    ASSERT(m_timersReadyToFire.isEmpty());

    collectFiringTimers(fireTime, firingTimers);
    fireTimers(fireTime, firingTimers);

    m_timersReadyToFire.clear();
    m_firingTimers = false;

    updateSharedTimer();
}

void ThreadTimers::fireTimersInNestedEventLoop()
{
    // Reset the reentrancy guard so the timers can fire again.
    m_firingTimers = false;
    m_timersReadyToFire.clear();
    updateSharedTimer();
}

} // namespace WebCore


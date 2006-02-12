/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "Timer.h"

#import <kxmlcore/Assertions.h>
#import <kxmlcore/HashSet.h>

namespace WebCore {

static bool deferringTimers;
static HashSet<TimerBase*>* deferredTimers;
static CFRunLoopTimerRef fireDeferredTimer;

static void timerFired(CFRunLoopTimerRef, void* p)
{
    TimerBase* timer = static_cast<TimerBase*>(p);
    if (!deferringTimers)
        timer->fire();
    else {
        deferredTimers = new HashSet<TimerBase*>;
        deferredTimers->add(timer);
    }
}

static void fireDeferred(CFRunLoopTimerRef, void*)
{
    CFRunLoopTimerInvalidate(fireDeferredTimer);
    CFRelease(fireDeferredTimer);
    fireDeferredTimer = 0;

    if (!deferredTimers)
        return;

    HashSet<TimerBase*> timers = *deferredTimers;
    HashSet<TimerBase*>::iterator end = timers.end();
    for (HashSet<TimerBase*>::iterator it = timers.begin(); !deferringTimers && it != end; ++it) {
        TimerBase* timer = *it;
        HashSet<TimerBase*>::iterator j = deferredTimers->find(timer);
        if (j != deferredTimers->end()) {
            deferredTimers->remove(j);
            timer->fire();
        }
    }
}

TimerBase::TimerBase()
    : m_runLoopTimer(0)
{
}

TimerBase::~TimerBase()
{
    stop();
}

void TimerBase::start(double nextFireInterval, double repeatInterval)
{
    stop();
    CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
    m_runLoopTimer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + nextFireInterval,
        repeatInterval, 0, 0, timerFired, &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_runLoopTimer, kCFRunLoopDefaultMode);
}

void TimerBase::startRepeating(double repeatInterval)
{
    ASSERT(repeatInterval > 0);
    start(repeatInterval, repeatInterval);
}

void TimerBase::startOneShot(double interval)
{
    start(interval, 0);
}

void TimerBase::stop()
{
    if (deferredTimers)
        deferredTimers->remove(this);

    if (!m_runLoopTimer)
        return;

    CFRunLoopTimerInvalidate(m_runLoopTimer);
    CFRelease(m_runLoopTimer);
    m_runLoopTimer = 0;
}

bool TimerBase::isActive() const
{
    return m_runLoopTimer;
}

double TimerBase::nextFireInterval() const
{
    if (!m_runLoopTimer)
        return 0;
    double nextFireDate = CFRunLoopTimerGetNextFireDate(m_runLoopTimer);
    double currentTime = CFAbsoluteTimeGetCurrent();
    if (nextFireDate < currentTime)
        return 0;
    return nextFireDate - currentTime;
}

double TimerBase::repeatInterval() const
{
    return m_runLoopTimer ? CFRunLoopTimerGetInterval(m_runLoopTimer) : 0;
}

void TimerBase::fire()
{
    ASSERT(!deferringTimers);
    ASSERT(!deferredTimers || !deferredTimers->contains(this));

    if (m_runLoopTimer && (!CFRunLoopTimerIsValid(m_runLoopTimer) || !CFRunLoopTimerDoesRepeat(m_runLoopTimer))) {
        CFRunLoopTimerInvalidate(m_runLoopTimer);
        CFRelease(m_runLoopTimer);
        m_runLoopTimer = 0;
    }

    // Note: This call may destroy the timer object, so be sure not to touch any fields afterward.
    fired();
}

bool isDeferringTimers()
{
    return deferringTimers;
}

void setDeferringTimers(bool shouldDefer)
{
    if (shouldDefer == deferringTimers)
        return;

    if (shouldDefer) {
        deferringTimers = true;
        if (fireDeferredTimer) {
            CFRunLoopTimerInvalidate(fireDeferredTimer);
            CFRelease(fireDeferredTimer);
            fireDeferredTimer = 0;
        }
    } else {
        deferringTimers = false;
        ASSERT(!fireDeferredTimer);
        fireDeferredTimer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent(), 0, 0, 0, fireDeferred, 0);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), fireDeferredTimer, kCFRunLoopDefaultMode);
    }
}

}

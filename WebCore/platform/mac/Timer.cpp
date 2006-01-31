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

namespace WebCore {

static void timerFired(CFRunLoopTimerRef, void* p)
{
    static_cast<TimerBase*>(p)->fire();
}

TimerBase::TimerBase(TimerBaseFiredFunction f)
    : m_function(f), m_runLoopTimer(0)
{
}

void TimerBase::start(double nextFireTime, double repeatInterval)
{
    stop();
    CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
    m_runLoopTimer = CFRunLoopTimerCreate(0, nextFireTime - kCFAbsoluteTimeIntervalSince1970,
        repeatInterval, 0, 0, timerFired, &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_runLoopTimer, kCFRunLoopDefaultMode);
}

void TimerBase::startRepeating(double repeatInterval)
{
    start(CFAbsoluteTimeGetCurrent() + repeatInterval + kCFAbsoluteTimeIntervalSince1970, repeatInterval);
}

void TimerBase::startOneShot(double interval)
{
    start(CFAbsoluteTimeGetCurrent() + interval + kCFAbsoluteTimeIntervalSince1970, 0);
}

void TimerBase::stop()
{
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

double TimerBase::nextFireTime() const
{
    return m_runLoopTimer ? CFRunLoopTimerGetNextFireDate(m_runLoopTimer) : 0;
}

double TimerBase::repeatInterval() const
{
    return m_runLoopTimer ? CFRunLoopTimerGetInterval(m_runLoopTimer) : 0;
}

void TimerBase::fire()
{
    if (m_runLoopTimer && !CFRunLoopTimerIsValid(m_runLoopTimer)) {
        CFRunLoopTimerInvalidate(m_runLoopTimer);
        CFRelease(m_runLoopTimer);
        m_runLoopTimer = 0;
    }

    // Note: This call may destroy the timer object, so be sure not to touch any fields afterward.
    m_function(this);
}

}

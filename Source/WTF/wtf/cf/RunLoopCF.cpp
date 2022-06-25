/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/RunLoop.h>

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <wtf/AutodrainedPool.h>
#include <wtf/SchedulePair.h>

namespace WTF {

static RetainPtr<CFRunLoopTimerRef> createTimer(Seconds interval, bool repeat, void(*timerFired)(CFRunLoopTimerRef, void*), void* info)
{
    CFRunLoopTimerContext context = { 0, info, 0, 0, 0 };
    Seconds repeatInterval = repeat ? interval : 0_s;
    return adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + interval.seconds(), repeatInterval.seconds(), 0, 0, timerFired, &context));
}

void RunLoop::performWork(void* context)
{
    AutodrainedPool pool;
    static_cast<RunLoop*>(context)->performWork();
}

RunLoop::RunLoop()
    : m_runLoop(CFRunLoopGetCurrent())
{
    CFRunLoopSourceContext context = { 0, this, 0, 0, 0, 0, 0, 0, 0, performWork };
    m_runLoopSource = adoptCF(CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context));
    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopCommonModes);
}

RunLoop::~RunLoop()
{
    CFRunLoopSourceInvalidate(m_runLoopSource.get());
}

void RunLoop::wakeUp()
{
    CFRunLoopSourceSignal(m_runLoopSource.get());
    CFRunLoopWakeUp(m_runLoop.get());
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode mode)
{
    CFTimeInterval timeInterval = 0.05;
    CFRunLoopRunInMode(mode, timeInterval, true);
    return CycleResult::Continue;
}

void RunLoop::run()
{
    AutodrainedPool pool;
    CFRunLoopRun();
}

void RunLoop::stop()
{
    ASSERT(m_runLoop == CFRunLoopGetCurrent());
    CFRunLoopStop(m_runLoop.get());
}

void RunLoop::dispatch(const SchedulePairHashSet& schedulePairs, Function<void()>&& function)
{
    auto timer = createTimer(0_s, false, [] (CFRunLoopTimerRef timer, void* context) {
        AutodrainedPool pool;

        CFRunLoopTimerInvalidate(timer);

        auto function = adopt(static_cast<Function<void()>::Impl*>(context));
        function();
    }, function.leak());

    for (auto& schedulePair : schedulePairs)
        CFRunLoopAddTimer(schedulePair->runLoop(), timer.get(), schedulePair->mode());
}

// RunLoop::Timer

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(Seconds interval, bool repeat)
{
    if (m_timer)
        stop();

    m_timer = createTimer(interval, repeat, [] (CFRunLoopTimerRef cfTimer, void* context) {
        AutodrainedPool pool;

        auto timer = static_cast<TimerBase*>(context);
        if (!CFRunLoopTimerDoesRepeat(cfTimer))
            CFRunLoopTimerInvalidate(cfTimer);

        timer->fired();
    }, this);
    CFRunLoopAddTimer(m_runLoop->m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

void RunLoop::TimerBase::stop()
{
    if (!m_timer)
        return;
    
    CFRunLoopTimerInvalidate(m_timer.get());
    m_timer = nullptr;
}

bool RunLoop::TimerBase::isActive() const
{
    return m_timer && CFRunLoopTimerIsValid(m_timer.get());
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    if (isActive())
        return std::max<Seconds>(Seconds { CFRunLoopTimerGetNextFireDate(m_timer.get()) - CFAbsoluteTimeGetCurrent() }, 0_s);
    return 0_s;
}

} // namespace WTF

/*
 * Copyright (C) 2010, 2012 Apple Inc. All rights reserved.
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
#include <mach/mach.h>
#include <wtf/AutodrainedPool.h>

namespace WTF {

void RunLoop::performWork(CFMachPortRef, void*, CFIndex, void* info)
{
    AutodrainedPool pool;
    static_cast<RunLoop*>(info)->performWork();
}

RunLoop::RunLoop()
    : m_runLoop(CFRunLoopGetCurrent())
{
    CFMachPortContext context = { 0, this, nullptr, nullptr, nullptr };
    m_port = adoptCF(CFMachPortCreate(kCFAllocatorDefault, performWork, &context, nullptr));
    m_runLoopSource = adoptCF(CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_port.get(), 0));
    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopCommonModes);
}

RunLoop::~RunLoop()
{
    CFMachPortInvalidate(m_port.get());
    CFRunLoopSourceInvalidate(m_runLoopSource.get());
}

void RunLoop::runForDuration(Seconds duration)
{
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, duration.seconds(), true);
}

void RunLoop::wakeUp()
{
    mach_msg_header_t header;
    header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    header.msgh_size = sizeof(mach_msg_header_t);
    header.msgh_remote_port = CFMachPortGetPort(m_port.get());
    header.msgh_local_port = MACH_PORT_NULL;
    header.msgh_id = 0;
    mach_msg_return_t result = mach_msg(&header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, header.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
    RELEASE_ASSERT(result == MACH_MSG_SUCCESS || result == MACH_SEND_TIMED_OUT);
    if (result == MACH_SEND_TIMED_OUT)
        mach_msg_destroy(&header);
}

RunLoop::CycleResult RunLoop::cycle(const String& mode)
{
    CFTimeInterval timeInterval = 0.05;
    CFRunLoopRunInMode(mode.isNull() ? kCFRunLoopDefaultMode : mode.createCFString().get(), timeInterval, true);
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

// RunLoop::Timer

void RunLoop::TimerBase::timerFired(CFRunLoopTimerRef, void* context)
{
    TimerBase* timer = static_cast<TimerBase*>(context);

    AutodrainedPool pool;
    timer->fired();
}

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(Seconds nextFireInterval, bool repeat)
{
    if (m_timer)
        stop();
    
    CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
    Seconds repeatInterval = repeat ? nextFireInterval : 0_s;
    m_timer = adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + nextFireInterval.seconds(), repeatInterval.seconds(), 0, 0, timerFired, &context));
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

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "RunLoop.h"

#include "AutodrainedPool.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

namespace WebCore {
    
static RunLoop* s_mainRunLoop;

void RunLoop::initializeMainRunLoop()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_mainRunLoop = new RunLoop(CFRunLoopGetMain());
    });
}

RunLoop* RunLoop::current()
{
    if (pthread_main_np())
        return RunLoop::main();
    
    DEFINE_STATIC_LOCAL(WTF::ThreadSpecific<RunLoop>, runLoopData, ());
    return &*runLoopData;
}

RunLoop* RunLoop::main()
{
    ASSERT(s_mainRunLoop);
    return s_mainRunLoop;
}

void RunLoop::performWork(void* context)
{
    // Wrap main thread in an Autorelease pool. Sending messages can call 
    // into objc code and accumulate memory.  
    if (current() == main()) {
        AutodrainedPool pool;
        static_cast<RunLoop*>(context)->performWork();
    } else
        static_cast<RunLoop*>(context)->performWork();
}

RunLoop::RunLoop()
    : m_runLoop(CFRunLoopGetCurrent())
    , m_nestingLevel(0)
{
    CFRunLoopSourceContext context = { 0, this, 0, 0, 0, 0, 0, 0, 0, performWork };
    m_runLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(m_runLoop, m_runLoopSource, kCFRunLoopCommonModes);
}

RunLoop::RunLoop(CFRunLoopRef runLoop)
    : m_runLoop(runLoop)
    , m_nestingLevel(0)
{
    CFRunLoopSourceContext context = { 0, this, 0, 0, 0, 0, 0, 0, 0, performWork };
    m_runLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(m_runLoop, m_runLoopSource, kCFRunLoopCommonModes);
}

RunLoop::~RunLoop()
{
    // FIXME: Tear down the work item queue here.
    CFRunLoopSourceInvalidate(m_runLoopSource);
    CFRelease(m_runLoopSource);
}

void RunLoop::runForDuration(double duration)
{
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, duration, true);
}

void RunLoop::wakeUp()
{
    CFRunLoopSourceSignal(m_runLoopSource);
    CFRunLoopWakeUp(m_runLoop);
}

#if !PLATFORM(MAC) || PLATFORM(IOS)
void RunLoop::run()
{
    current()->m_nestingLevel++;
    
    {
        AutodrainedPool pool;
        CFRunLoopRun();
    }
    
    current()->m_nestingLevel--;
}

void RunLoop::stop()
{
    ASSERT(m_runLoop == CFRunLoopGetCurrent());
    CFRunLoopStop(m_runLoop);
}
#endif

// RunLoop::Timer

void RunLoop::TimerBase::timerFired(CFRunLoopTimerRef, void* context)
{
    TimerBase* timer = static_cast<TimerBase*>(context);
    
    // Wrap main thread in an Autorelease pool. The timer can call 
    // into objc code and accumulate memory outside of the main event loop.
    if (current() == main()) {
        AutodrainedPool pool;
        timer->fired();
    } else
        timer->fired();
}

RunLoop::TimerBase::TimerBase(RunLoop* runLoop)
    : m_runLoop(runLoop)
    , m_timer(0)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(double nextFireInterval, bool repeat)
{
    if (m_timer)
        stop();
    
    CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
    CFTimeInterval repeatInterval = repeat ? nextFireInterval : 0;
    m_timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + nextFireInterval, repeatInterval, 0, 0, timerFired, &context);
    CFRunLoopAddTimer(m_runLoop->m_runLoop, m_timer, kCFRunLoopCommonModes);
}

void RunLoop::TimerBase::stop()
{
    if (!m_timer)
        return;
    
    CFRunLoopTimerInvalidate(m_timer);
    CFRelease(m_timer);
    m_timer = 0;
}

bool RunLoop::TimerBase::isActive() const
{
    return m_timer && CFRunLoopTimerIsValid(m_timer);
}

} // namespace WebCore

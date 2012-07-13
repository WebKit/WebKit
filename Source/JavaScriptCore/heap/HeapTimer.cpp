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
#include "HeapTimer.h"

#include "APIShims.h"
#include "JSObject.h"
#include "JSString.h"
#include "ScopeChain.h"
#include <wtf/Threading.h>

namespace JSC {

#if USE(CF)
    
const CFTimeInterval HeapTimer::s_decade = 60 * 60 * 24 * 365 * 10;

HeapTimer::HeapTimer(JSGlobalData* globalData, CFRunLoopRef runLoop)
    : m_globalData(globalData)
    , m_runLoop(runLoop)
{
    memset(&m_context, 0, sizeof(CFRunLoopTimerContext));
    m_context.info = this;
    m_timer.adoptCF(CFRunLoopTimerCreate(0, s_decade, s_decade, 0, 0, HeapTimer::timerDidFire, &m_context));
    CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

HeapTimer::~HeapTimer()
{
    CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    CFRunLoopTimerInvalidate(m_timer.get());
}

void HeapTimer::synchronize()
{
    if (CFRunLoopGetCurrent() == m_runLoop.get())
        return;
    CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    m_runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

void HeapTimer::invalidate()
{
    m_globalData = 0;
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() - s_decade);
}

void HeapTimer::didStartVMShutdown()
{
    if (CFRunLoopGetCurrent() == m_runLoop.get()) {
        invalidate();
        delete this;
        return;
    }
    ASSERT(!m_globalData->apiLock().currentThreadIsHoldingLock());
    MutexLocker locker(m_shutdownMutex);
    invalidate();
}

void HeapTimer::timerDidFire(CFRunLoopTimerRef, void* info)
{
    HeapTimer* agent = static_cast<HeapTimer*>(info);
    agent->m_shutdownMutex.lock();
    if (!agent->m_globalData) {
        agent->m_shutdownMutex.unlock();
        delete agent;
        return;
    }
    {
        // We don't ref here to prevent us from resurrecting the ref count of a "dead" JSGlobalData.
        APIEntryShim shim(agent->m_globalData, APIEntryShimWithoutLock::DontRefGlobalData);
        agent->doWork();
    }
    agent->m_shutdownMutex.unlock();
}

#elif PLATFORM(BLACKBERRY)

HeapTimer::HeapTimer(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_timer(this, &HeapTimer::timerDidFire)
{
}

HeapTimer::~HeapTimer()
{
}

void HeapTimer::timerDidFire()
{
    doWork();
}

void HeapTimer::synchronize()
{
}

void HeapTimer::invalidate()
{
}

void HeapTimer::didStartVMShutdown()
{
    delete this;
}

#else

HeapTimer::HeapTimer(JSGlobalData* globalData)
    : m_globalData(globalData)
{
}

HeapTimer::~HeapTimer()
{
}

void HeapTimer::didStartVMShutdown()
{
    delete this;
}

void HeapTimer::synchronize()
{
}

void HeapTimer::invalidate()
{
}

#endif
    

} // namespace JSC

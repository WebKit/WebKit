/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GCActivityCallback.h"

#include "Heap.h"
#include "JSLock.h"
#include "JSObject.h"
#include "VM.h"

#include <wtf/RetainPtr.h>
#include <wtf/WTFThreadData.h>

#if PLATFORM(EFL)
#include <wtf/MainThread.h>
#endif

namespace JSC {

bool GCActivityCallback::s_shouldCreateGCTimer = true;

#if USE(CF) || PLATFORM(EFL)

const double timerSlop = 2.0; // Fudge factor to avoid performance cost of resetting timer.

#if USE(CF)
GCActivityCallback::GCActivityCallback(Heap* heap)
    : GCActivityCallback(heap->vm(), CFRunLoopGetCurrent())
{
}

GCActivityCallback::GCActivityCallback(Heap* heap, CFRunLoopRef runLoop)
    : GCActivityCallback(heap->vm(), runLoop)
{
}
#elif PLATFORM(EFL)
GCActivityCallback::GCActivityCallback(Heap* heap)
    : GCActivityCallback(heap->vm(), WTF::isMainThread())
{
}
#endif

void GCActivityCallback::doWork()
{
    Heap* heap = &m_vm->heap;
    if (!isEnabled())
        return;
    
    JSLockHolder locker(m_vm);
    if (heap->isDeferred()) {
        scheduleTimer(0);
        return;
    }

    doCollection();
}

#if USE(CF)
void GCActivityCallback::scheduleTimer(double newDelay)
{
    if (newDelay * timerSlop > m_delay)
        return;
    double delta = m_delay - newDelay;
    m_delay = newDelay;
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFRunLoopTimerGetNextFireDate(m_timer.get()) - delta);
}

void GCActivityCallback::cancelTimer()
{
    m_delay = s_decade;
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade);
}
#elif PLATFORM(EFL)
void GCActivityCallback::scheduleTimer(double newDelay)
{
    if (newDelay * timerSlop > m_delay)
        return;

    stop();
    m_delay = newDelay;
    
    ASSERT(!m_timer);
    m_timer = add(newDelay, this);
}

void GCActivityCallback::cancelTimer()
{
    m_delay = s_hour;
    stop();
}
#endif

void GCActivityCallback::didAllocate(size_t bytes)
{
#if PLATFORM(EFL)
    if (!isEnabled())
        return;

    ASSERT(WTF::isMainThread());
#endif

    // The first byte allocated in an allocation cycle will report 0 bytes to didAllocate. 
    // We pretend it's one byte so that we don't ignore this allocation entirely.
    if (!bytes)
        bytes = 1;
    double bytesExpectedToReclaim = static_cast<double>(bytes) * deathRate();
    double newDelay = lastGCLength() / gcTimeSlice(bytesExpectedToReclaim);
    scheduleTimer(newDelay);
}

void GCActivityCallback::willCollect()
{
    cancelTimer();
}

void GCActivityCallback::cancel()
{
    cancelTimer();
}

#else

GCActivityCallback::GCActivityCallback(Heap* heap)
    : GCActivityCallback(heap->vm())
{
}

void GCActivityCallback::doWork()
{
}

void GCActivityCallback::didAllocate(size_t)
{
}

void GCActivityCallback::willCollect()
{
}

void GCActivityCallback::cancel()
{
}

#endif

}


/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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

#include "GCActivityCallback.h"
#include "IncrementalSweeper.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSCInlines.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

#if USE(GLIB)
#include <glib.h>
#endif

namespace JSC {

void HeapTimer::timerDidFire()
{
    m_apiLock->lock();

    RefPtr<VM> vm = m_apiLock->vm();
    if (!vm) {
        // The VM has been destroyed, so we should just give up.
        m_apiLock->unlock();
        return;
    }

    {
        JSLockHolder locker(vm.get());
        doWork();
    }

    m_apiLock->unlock();
}

#if USE(CF)
    
const CFTimeInterval HeapTimer::s_decade = 60 * 60 * 24 * 365 * 10;

HeapTimer::HeapTimer(VM* vm)
    : m_vm(vm)
    , m_apiLock(&vm->apiLock())
{
    setRunLoop(vm->heap.runLoop());
}

void HeapTimer::setRunLoop(CFRunLoopRef runLoop)
{
    if (m_runLoop) {
        CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
        CFRunLoopTimerInvalidate(m_timer.get());
        m_runLoop.clear();
        m_timer.clear();
    }
    
    if (runLoop) {
        m_runLoop = runLoop;
        memset(&m_context, 0, sizeof(CFRunLoopTimerContext));
        m_context.info = this;
        m_timer = adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, s_decade, s_decade, 0, 0, HeapTimer::timerDidFireCallback, &m_context));
        CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    }
}

HeapTimer::~HeapTimer()
{
    setRunLoop(0);
}

void HeapTimer::timerDidFireCallback(CFRunLoopTimerRef, void* contextPtr)
{
    static_cast<JSRunLoopTimer*>(contextPtr)->timerDidFire();
}

void HeapTimer::scheduleTimer(double intervalInSeconds)
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + intervalInSeconds);
    m_isScheduled = true;
}

void HeapTimer::cancelTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade);
    m_isScheduled = false;
}

#elif USE(GLIB)

const long HeapTimer::s_decade = 60 * 60 * 24 * 365 * 10;

static GSourceFuncs heapTimerSourceFunctions = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource*, GSourceFunc callback, gpointer userData) -> gboolean
    {
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

HeapTimer::HeapTimer(VM* vm)
    : m_vm(vm)
    , m_apiLock(&vm->apiLock())
    , m_timer(adoptGRef(g_source_new(&heapTimerSourceFunctions, sizeof(GSource))))
{
    g_source_set_name(m_timer.get(), "[JavaScriptCore] HeapTimer");
    g_source_set_callback(m_timer.get(), [](gpointer userData) -> gboolean {
        auto& heapTimer = *static_cast<HeapTimer*>(userData);
        g_source_set_ready_time(heapTimer.m_timer.get(), g_get_monotonic_time() + HeapTimer::s_decade * G_USEC_PER_SEC);
        heapTimer.timerDidFire();
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(m_timer.get(), g_main_context_get_thread_default());
}

HeapTimer::~HeapTimer()
{
    g_source_destroy(m_timer.get());
}

void HeapTimer::scheduleTimer(double intervalInSeconds)
{
    g_source_set_ready_time(m_timer.get(), g_get_monotonic_time() + intervalInSeconds * G_USEC_PER_SEC);
    m_isScheduled = true;
}

void HeapTimer::cancelTimer()
{
    g_source_set_ready_time(m_timer.get(), g_get_monotonic_time() + s_decade * G_USEC_PER_SEC);
    m_isScheduled = false;
}
#else
HeapTimer::HeapTimer(VM* vm)
    : m_vm(vm)
{
}

HeapTimer::~HeapTimer()
{
}

void HeapTimer::scheduleTimer(double)
{
}

void HeapTimer::cancelTimer()
{
}
#endif
    

} // namespace JSC

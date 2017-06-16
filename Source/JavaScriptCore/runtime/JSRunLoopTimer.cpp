/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "JSRunLoopTimer.h"

#include "GCActivityCallback.h"
#include "IncrementalSweeper.h"
#include "JSCInlines.h"
#include "JSObject.h"
#include "JSString.h"

#include <wtf/MainThread.h>
#include <wtf/Threading.h>

#if USE(GLIB_EVENT_LOOP)
#include <glib.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace JSC {

const Seconds JSRunLoopTimer::s_decade { 60 * 60 * 24 * 365 * 10 };

void JSRunLoopTimer::timerDidFire()
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

JSRunLoopTimer::JSRunLoopTimer(VM* vm)
    : m_vm(vm)
    , m_apiLock(&vm->apiLock())
{
    m_vm->registerRunLoopTimer(this);
}

void JSRunLoopTimer::setRunLoop(CFRunLoopRef runLoop)
{
    if (m_runLoop) {
        CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
        CFRunLoopTimerInvalidate(m_timer.get());
        m_runLoop.clear();
        m_timer.clear();
    }

    m_runLoop = runLoop;
    if (runLoop) {
        memset(&m_context, 0, sizeof(CFRunLoopTimerContext));
        m_context.info = this;
        m_timer = adoptCF(CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + s_decade.seconds(), s_decade.seconds(), 0, 0, JSRunLoopTimer::timerDidFireCallback, &m_context));
        CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    }
}

JSRunLoopTimer::~JSRunLoopTimer()
{
    m_vm->unregisterRunLoopTimer(this);
}

void JSRunLoopTimer::timerDidFireCallback(CFRunLoopTimerRef, void* contextPtr)
{
    static_cast<JSRunLoopTimer*>(contextPtr)->timerDidFire();
}

void JSRunLoopTimer::scheduleTimer(Seconds intervalInSeconds)
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + intervalInSeconds.seconds());
    m_isScheduled = true;
    auto locker = holdLock(m_timerCallbacksLock);
    for (auto& task : m_timerSetCallbacks)
        task->run();
}

void JSRunLoopTimer::cancelTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade.seconds());
    m_isScheduled = false;
}

#else

JSRunLoopTimer::JSRunLoopTimer(VM* vm)
    : m_vm(vm)
    , m_apiLock(&vm->apiLock())
    , m_timer(RunLoop::current(), this, &JSRunLoopTimer::timerDidFireCallback)
{
#if USE(GLIB_EVENT_LOOP)
    m_timer.setPriority(RunLoopSourcePriority::JavascriptTimer);
    m_timer.setName("[JavaScriptCore] JSRunLoopTimer");
#endif
    m_timer.startOneShot(s_decade);
}

JSRunLoopTimer::~JSRunLoopTimer()
{
}

void JSRunLoopTimer::timerDidFireCallback()
{
    m_timer.startOneShot(s_decade);
    timerDidFire();
}

void JSRunLoopTimer::scheduleTimer(Seconds intervalInSeconds)
{
    m_timer.startOneShot(intervalInSeconds);
    m_isScheduled = true;

    auto locker = holdLock(m_timerCallbacksLock);
    for (auto& task : m_timerSetCallbacks)
        task->run();
}

void JSRunLoopTimer::cancelTimer()
{
    m_timer.startOneShot(s_decade);
    m_isScheduled = false;
}

#endif

void JSRunLoopTimer::addTimerSetNotification(TimerNotificationCallback callback)
{
    auto locker = holdLock(m_timerCallbacksLock);
    m_timerSetCallbacks.add(callback);
}

void JSRunLoopTimer::removeTimerSetNotification(TimerNotificationCallback callback)
{
    auto locker = holdLock(m_timerCallbacksLock);
    m_timerSetCallbacks.remove(callback);
}

} // namespace JSC

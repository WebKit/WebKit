/*
 * Copyright (C) 2012, 2015-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class JSLock;
class VM;

class JSRunLoopTimer : public ThreadSafeRefCounted<JSRunLoopTimer> {
public:
    typedef void TimerNotificationType();
    using TimerNotificationCallback = RefPtr<WTF::SharedTask<TimerNotificationType>>;

    JSRunLoopTimer(VM*);
#if USE(CF)
    static void timerDidFireCallback(CFRunLoopTimerRef, void*);
#else
    void timerDidFireCallback();
#endif

    JS_EXPORT_PRIVATE virtual ~JSRunLoopTimer();
    virtual void doWork() = 0;

    void scheduleTimer(Seconds intervalInSeconds);
    void cancelTimer();
    bool isScheduled() const { return m_isScheduled; }

    // Note: The only thing the timer notification callback cannot do is
    // call scheduleTimer(). This will cause a deadlock. It would not
    // be hard to make this work, however, there are no clients that need
    // this behavior. We should implement it only if we find that we need it.
    JS_EXPORT_PRIVATE void addTimerSetNotification(TimerNotificationCallback);
    JS_EXPORT_PRIVATE void removeTimerSetNotification(TimerNotificationCallback);

#if USE(CF)
    JS_EXPORT_PRIVATE void setRunLoop(CFRunLoopRef);
#endif // USE(CF)

protected:
    VM* m_vm;

    static const Seconds s_decade;

    RefPtr<JSLock> m_apiLock;
    bool m_isScheduled { false };
#if USE(CF)
    RetainPtr<CFRunLoopTimerRef> m_timer;
    RetainPtr<CFRunLoopRef> m_runLoop;

    CFRunLoopTimerContext m_context;

    Lock m_shutdownMutex;
#else
    RunLoop::Timer<JSRunLoopTimer> m_timer;
#endif

    Lock m_timerCallbacksLock;
    HashSet<TimerNotificationCallback> m_timerSetCallbacks;
    
private:
    void timerDidFire();
};
    
} // namespace JSC

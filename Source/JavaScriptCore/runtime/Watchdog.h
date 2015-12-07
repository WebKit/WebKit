/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#ifndef Watchdog_h
#define Watchdog_h

#include <wtf/Lock.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace JSC {

class ExecState;
class VM;

class Watchdog : public WTF::ThreadSafeRefCounted<Watchdog> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Scope;

    Watchdog();

    typedef bool (*ShouldTerminateCallback)(ExecState*, void* data1, void* data2);
    void setTimeLimit(std::chrono::microseconds limit, ShouldTerminateCallback = 0, void* data1 = 0, void* data2 = 0);
    JS_EXPORT_PRIVATE void terminateSoon();

    bool shouldTerminate(ExecState* exec)
    {
        if (!m_timerDidFire)
            return false;
        return shouldTerminateSlow(exec);
    }

    bool hasTimeLimit();
    void enteredVM();
    void exitedVM();

    void* timerDidFireAddress() { return &m_timerDidFire; }

    static const std::chrono::microseconds noTimeLimit;

private:
    void startTimer(LockHolder&, std::chrono::microseconds timeLimit);
    void stopTimer(LockHolder&);

    bool shouldTerminateSlow(ExecState*);

    // m_timerDidFire indicates whether the timer fired. The Watchdog
    // still needs to check if the allowed CPU time has elapsed. If so, then
    // the Watchdog fires and m_didFire will be set.
    // NOTE: m_timerDidFire is only set by the platform specific timer
    // (probably from another thread) but is only cleared in the script thread.
    bool m_timerDidFire;

    std::chrono::microseconds m_timeLimit;

    std::chrono::microseconds m_cpuDeadline;
    std::chrono::microseconds m_wallClockDeadline;

    // Writes to m_timerDidFire and m_timeLimit, and Reads+Writes to m_cpuDeadline and m_wallClockDeadline
    // must be guarded by this lock.
    Lock m_lock;

    bool m_hasEnteredVM { false };

    ShouldTerminateCallback m_callback;
    void* m_callbackData1;
    void* m_callbackData2;

    Ref<WorkQueue> m_timerQueue;
    std::function<void ()> m_timerHandler;

    friend class LLIntOffsetsExtractor;
};

} // namespace JSC

#endif // Watchdog_h

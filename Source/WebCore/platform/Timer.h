/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#include "ThreadTimers.h"
#include <functional>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/Seconds.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

namespace WebCore {

class TimerBase {
    WTF_MAKE_NONCOPYABLE(TimerBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT TimerBase();
    WEBCORE_EXPORT virtual ~TimerBase();

    WEBCORE_EXPORT void start(Seconds nextFireInterval, Seconds repeatInterval);

    void startRepeating(Seconds repeatInterval) { start(repeatInterval, repeatInterval); }
    void startOneShot(Seconds interval) { start(interval, 0_s); }

    WEBCORE_EXPORT void stop();
    bool isActive() const;

    Seconds nextFireInterval() const;
    Seconds nextUnalignedFireInterval() const;
    Seconds repeatInterval() const { return m_repeatInterval; }

    void augmentFireInterval(Seconds delta) { setNextFireTime(m_heapItem->time + delta); }
    void augmentRepeatInterval(Seconds delta) { augmentFireInterval(delta); m_repeatInterval += delta; }

    void didChangeAlignmentInterval();

    static void fireTimersInNestedEventLoop();

protected:
    MonotonicTime nextFireTime() const { return m_heapItem ? m_heapItem->time : MonotonicTime { }; }
    void setNextFireTime(MonotonicTime);

    void assertThreadSafety()
    {
        ASSERT(canAccessThreadLocalDataForThread(m_thread.get()));
    }

private:
    virtual void fired() = 0;

    virtual Optional<MonotonicTime> alignedFireTime(MonotonicTime) const { return WTF::nullopt; }

    void checkConsistency() const;
    void checkHeapIndex() const;

    bool inHeap() const { return m_heapItem && m_heapItem->isInHeap(); }

    bool hasValidHeapPosition() const;
    void updateHeapIfNeeded(MonotonicTime oldTime);

    void heapDecreaseKey();
    void heapDelete();
    void heapDeleteMin();
    void heapIncreaseKey();
    void heapInsert();
    void heapPop();
    void heapPopMin();
    static void heapDeleteNullMin(ThreadTimerHeap&);

    MonotonicTime m_unalignedNextFireTime; // m_nextFireTime not considering alignment interval
    Seconds m_repeatInterval; // 0 if not repeating
    bool m_wasDeleted { false };

    RefPtr<ThreadTimerHeapItem> m_heapItem;
    Ref<Thread> m_thread { Thread::current() };

    friend class ThreadTimers;
    friend class TimerHeapLessThanFunction;
    friend class TimerHeapReference;
};


class Timer : public TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template <typename TimerFiredClass, typename TimerFiredBaseClass>
    Timer(TimerFiredClass& object, void (TimerFiredBaseClass::*function)())
        : m_function(std::bind(function, &object))
    {
    }

    Timer(WTF::Function<void ()>&& function)
        : m_function(WTFMove(function))
    {
    }

private:
    void fired() override
    {
        m_function();
    }
    
    WTF::Function<void ()> m_function;
};

inline bool TimerBase::isActive() const
{
    // FIXME: Write this in terms of USE(WEB_THREAD) instead of PLATFORM(IOS_FAMILY).
#if !PLATFORM(IOS_FAMILY)
    ASSERT(m_thread.ptr() == &Thread::current());
#else
    ASSERT(WebThreadIsCurrent() || pthread_main_np() || m_thread.ptr() == &Thread::current());
#endif // PLATFORM(IOS_FAMILY)
    return static_cast<bool>(nextFireTime());
}

// ResettableOneShotTimer is a optimization for timers that need to be delayed frequently.
//
// Changing the deadline of a timer is not a cheap operation. When it is done frequently, it can
// affect performance.
//
// With ResettableOneShotTimer, calling restart() does not change the underlying timer.
// Instead, when the underlying timer fires, a new dealine is set for "delay" seconds.
//
// When a ResettableOneShotTimer of 5 seconds is restarted before the deadline, the total
// time before the function is called is 10 seconds.
//
// If a timer is unfrequently reset, or if it is usually stopped before the deadline,
// prefer WebCore::Timer to avoid idle wake-up.
class ResettableOneShotTimer : protected TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename TimerFiredClass>
    ResettableOneShotTimer(TimerFiredClass& object, void (TimerFiredClass::*function)(), Seconds delay)
        : ResettableOneShotTimer(std::bind(function, &object), delay)
    {
    }

    ResettableOneShotTimer(WTF::Function<void()>&& function, Seconds delay)
        : m_function(WTFMove(function))
        , m_delay(delay)
        , m_shouldRestartWhenTimerFires(false)
    {
    }

    void restart()
    {
        // Setting this boolean is much more efficient than calling startOneShot
        // again, which might result in rescheduling the system timer which
        // can be quite expensive.

        if (isActive()) {
            m_shouldRestartWhenTimerFires = true;
            return;
        }
        startOneShot(m_delay);
    }

    void stop()
    {
        m_shouldRestartWhenTimerFires = false;
        TimerBase::stop();
    }

    using TimerBase::isActive;

private:
    void fired() override
    {
        if (m_shouldRestartWhenTimerFires) {
            m_shouldRestartWhenTimerFires = false;
            startOneShot(m_delay);
            return;
        }

        m_function();
    }

    WTF::Function<void ()> m_function;

    Seconds m_delay;
    bool m_shouldRestartWhenTimerFires;
};

// DeferrableOneShotTimer is a optimization for timers that need to change the deadline frequently.
//
// With DeferrableOneShotTimer, if the new deadline is later than the original deadline, the timer
// is not reset. Instead, the timer fires and schedule a new timer for the remaining time.
//
// DeferrableOneShotTimer supports absolute deadlines.
// If a timer of 5 seconds is restarted after 2 seconds, the total time will be 7 seconds.
//
// Restarting a DeferrableOneShotTimer is more expensive than restarting a ResettableOneShotTimer.
// If accumulating the delay is not important, prefer ResettableOneShotTimer.
class WEBCORE_EXPORT DeferrableOneShotTimer : private TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DeferrableOneShotTimer(WTF::Function<void()>&&);
    ~DeferrableOneShotTimer();

    void startOneShot(Seconds interval);
    void stop();

private:
    void fired() override;

    WTF::Function<void()> m_function;
    MonotonicTime m_restartFireTime;
};

}

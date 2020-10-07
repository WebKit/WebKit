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

    WEBCORE_EXPORT Seconds nextFireInterval() const;
    Seconds nextUnalignedFireInterval() const;
    Seconds repeatInterval() const { return m_repeatInterval; }

    void augmentFireInterval(Seconds delta) { setNextFireTime(m_heapItem->time + delta); }
    void augmentRepeatInterval(Seconds delta) { augmentFireInterval(delta); m_repeatInterval += delta; }

    void didChangeAlignmentInterval();

    static void fireTimersInNestedEventLoop();

private:
    virtual void fired() = 0;

    virtual Optional<MonotonicTime> alignedFireTime(MonotonicTime) const { return WTF::nullopt; }

    void checkConsistency() const;
    void checkHeapIndex() const;

    void setNextFireTime(MonotonicTime);

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

    MonotonicTime nextFireTime() const { return m_heapItem ? m_heapItem->time : MonotonicTime { }; }

    MonotonicTime m_unalignedNextFireTime; // m_nextFireTime not considering alignment interval
    Seconds m_repeatInterval; // 0 if not repeating

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

class DeferrableOneShotTimer : protected TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename TimerFiredClass>
    DeferrableOneShotTimer(TimerFiredClass& object, void (TimerFiredClass::*function)(), Seconds delay)
        : DeferrableOneShotTimer(std::bind(function, &object), delay)
    {
    }

    DeferrableOneShotTimer(WTF::Function<void ()>&& function, Seconds delay)
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

class DeferrableTaskTimer final : private TimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DeferrableTaskTimer() = default;

    void doTask(Function<void()>&&, Seconds);
    void cancel();
    bool isActive() const { return TimerBase::isActive(); }

private:
    void fired() final;

    Function<void()> m_function;
};

inline void DeferrableTaskTimer::fired()
{
    std::exchange(m_function, { })();
}

inline void DeferrableTaskTimer::doTask(Function<void()>&& function, Seconds delay)
{
    ASSERT(!isActive());
    ASSERT(!m_function);
    m_function = WTFMove(function);
    startOneShot(delay);
}

inline void DeferrableTaskTimer::cancel()
{
    std::exchange(m_function, { });
    stop();
}

}

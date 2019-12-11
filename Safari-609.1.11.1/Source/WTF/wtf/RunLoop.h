/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#pragma once

#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WTF {

#if USE(CF)
using RunLoopMode = CFStringRef;
#define DefaultRunLoopMode kCFRunLoopDefaultMode
#else
using RunLoopMode = unsigned;
#define DefaultRunLoopMode 0
#endif

class RunLoop : public FunctionDispatcher {
    WTF_MAKE_NONCOPYABLE(RunLoop);
public:
    // Must be called from the main thread (except for the Mac platform, where it
    // can be called from any thread).
    WTF_EXPORT_PRIVATE static void initializeMainRunLoop();

    WTF_EXPORT_PRIVATE static RunLoop& current();
    WTF_EXPORT_PRIVATE static RunLoop& main();
    WTF_EXPORT_PRIVATE static bool isMain();
    ~RunLoop();

    void dispatch(Function<void()>&&) override;

    WTF_EXPORT_PRIVATE static void run();
    WTF_EXPORT_PRIVATE void stop();
    WTF_EXPORT_PRIVATE void wakeUp();

    enum class CycleResult { Continue, Stop };
    WTF_EXPORT_PRIVATE CycleResult static cycle(RunLoopMode = DefaultRunLoopMode);

#if USE(COCOA_EVENT_LOOP)
    WTF_EXPORT_PRIVATE void runForDuration(Seconds duration);
#endif

#if USE(GLIB_EVENT_LOOP)
    WTF_EXPORT_PRIVATE GMainContext* mainContext() const { return m_mainContext.get(); }
#endif

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    // Run the single iteration of the RunLoop. It consumes the pending tasks and expired timers, but it won't be blocked.
    WTF_EXPORT_PRIVATE static void iterate();
#endif

#if USE(WINDOWS_EVENT_LOOP)
    static void registerRunLoopMessageWindowClass();
#endif

#if USE(GLIB_EVENT_LOOP) || USE(GENERIC_EVENT_LOOP)
    WTF_EXPORT_PRIVATE void dispatchAfter(Seconds, Function<void()>&&);
#endif

    class TimerBase {
        WTF_MAKE_FAST_ALLOCATED;
        friend class RunLoop;
    public:
        WTF_EXPORT_PRIVATE explicit TimerBase(RunLoop&);
        WTF_EXPORT_PRIVATE virtual ~TimerBase();

        void startRepeating(Seconds repeatInterval) { startInternal(repeatInterval, true); }
        void startOneShot(Seconds interval) { startInternal(interval, false); }

        WTF_EXPORT_PRIVATE void stop();
        WTF_EXPORT_PRIVATE bool isActive() const;
        WTF_EXPORT_PRIVATE Seconds secondsUntilFire() const;

        virtual void fired() = 0;

#if USE(GLIB_EVENT_LOOP)
        void setName(const char*);
        void setPriority(int);
#endif

    private:
        void startInternal(Seconds nextFireInterval, bool repeat)
        {
            start(std::max(nextFireInterval, 0_s), repeat);
        }

        WTF_EXPORT_PRIVATE void start(Seconds nextFireInterval, bool repeat);

        Ref<RunLoop> m_runLoop;

#if USE(WINDOWS_EVENT_LOOP)
        bool isActive(const AbstractLocker&) const;
        void timerFired();
        MonotonicTime m_nextFireDate;
        Seconds m_interval;
        bool m_isRepeating { false };
        bool m_isActive { false };
#elif USE(COCOA_EVENT_LOOP)
        static void timerFired(CFRunLoopTimerRef, void*);
        RetainPtr<CFRunLoopTimerRef> m_timer;
#elif USE(GLIB_EVENT_LOOP)
        void updateReadyTime();
        GRefPtr<GSource> m_source;
        bool m_isRepeating { false };
        Seconds m_fireInterval { 0 };
#elif USE(GENERIC_EVENT_LOOP)
        bool isActive(const AbstractLocker&) const;
        void stop(const AbstractLocker&);

        class ScheduledTask;
        RefPtr<ScheduledTask> m_scheduledTask;
#endif
    };

    template <typename TimerFiredClass>
    class Timer : public TimerBase {
    public:
        typedef void (TimerFiredClass::*TimerFiredFunction)();

        Timer(RunLoop& runLoop, TimerFiredClass* o, TimerFiredFunction f)
            : TimerBase(runLoop)
            , m_function(f)
            , m_object(o)
        {
        }

    private:
        void fired() override { (m_object->*m_function)(); }

        // This order should be maintained due to MSVC bug.
        // http://computer-programming-forum.com/7-vc.net/6fbc30265f860ad1.htm
        TimerFiredFunction m_function;
        TimerFiredClass* m_object;
    };

    class Holder;

private:
    RunLoop();

    void performWork();

    Lock m_functionQueueLock;
    Deque<Function<void()>> m_functionQueue;

#if USE(WINDOWS_EVENT_LOOP)
    static LRESULT CALLBACK RunLoopWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    HWND m_runLoopMessageWindow;

    Lock m_loopLock;
#elif USE(COCOA_EVENT_LOOP)
    static void performWork(void*);
    RetainPtr<CFRunLoopRef> m_runLoop;
    RetainPtr<CFRunLoopSourceRef> m_runLoopSource;
#elif USE(GLIB_EVENT_LOOP)
    GRefPtr<GMainContext> m_mainContext;
    Vector<GRefPtr<GMainLoop>> m_mainLoops;
    GRefPtr<GSource> m_source;
#elif USE(GENERIC_EVENT_LOOP)
    void schedule(Ref<TimerBase::ScheduledTask>&&);
    void schedule(const AbstractLocker&, Ref<TimerBase::ScheduledTask>&&);
    void wakeUp(const AbstractLocker&);
    void scheduleAndWakeUp(const AbstractLocker&, Ref<TimerBase::ScheduledTask>&&);

    enum class RunMode {
        Iterate,
        Drain
    };

    enum class Status {
        Clear,
        Stopping,
    };
    void runImpl(RunMode);
    bool populateTasks(RunMode, Status&, Deque<RefPtr<TimerBase::ScheduledTask>>&);

    friend class TimerBase;

    Lock m_loopLock;
    Condition m_readyToRun;
    Condition m_stopCondition;
    Vector<RefPtr<TimerBase::ScheduledTask>> m_schedules;
    Vector<Status*> m_mainLoops;
    bool m_shutdown { false };
    bool m_pendingTasks { false };
#endif
};

} // namespace WTF

using WTF::RunLoop;
using WTF::RunLoopMode;

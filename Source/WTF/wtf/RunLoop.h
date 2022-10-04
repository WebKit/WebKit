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

#include <functional>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Observer.h>
#include <wtf/RetainPtr.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSafetyAnalysis.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/GRefPtr.h>
#endif

#if USE(GENERIC_EVENT_LOOP)
#include <wtf/RedBlackTree.h>
#endif

namespace WTF {

#if USE(COCOA_EVENT_LOOP)
class SchedulePair;
struct SchedulePairHash;
using SchedulePairHashSet = HashSet<RefPtr<SchedulePair>, SchedulePairHash>;
#endif

#if USE(CF)
using RunLoopMode = CFStringRef;
#define DefaultRunLoopMode kCFRunLoopDefaultMode
#else
using RunLoopMode = unsigned;
#define DefaultRunLoopMode 0
#endif

class WTF_CAPABILITY("is current") RunLoop final : public FunctionDispatcher, public ThreadSafeRefCounted<RunLoop> {
    WTF_MAKE_NONCOPYABLE(RunLoop);
public:
    // Must be called from the main thread.
    WTF_EXPORT_PRIVATE static void initializeMain();
#if USE(WEB_THREAD)
    WTF_EXPORT_PRIVATE static void initializeWeb();
#endif

    WTF_EXPORT_PRIVATE static RunLoop& current();
    WTF_EXPORT_PRIVATE static RunLoop& main();
#if USE(WEB_THREAD)
    WTF_EXPORT_PRIVATE static RunLoop& web();
    WTF_EXPORT_PRIVATE static RunLoop* webIfExists();
#endif
    WTF_EXPORT_PRIVATE static Ref<RunLoop> create(const char* threadName, ThreadType = ThreadType::Unknown, Thread::QOS = Thread::QOS::UserInitiated);

    static bool isMain() { return main().isCurrent(); }
    WTF_EXPORT_PRIVATE bool isCurrent() const;
    WTF_EXPORT_PRIVATE ~RunLoop() final;

    WTF_EXPORT_PRIVATE void dispatch(Function<void()>&&) final;
    WTF_EXPORT_PRIVATE void dispatchAfter(Seconds, Function<void()>&&);
#if USE(COCOA_EVENT_LOOP)
    WTF_EXPORT_PRIVATE static void dispatch(const SchedulePairHashSet&, Function<void()>&&);
#endif

    WTF_EXPORT_PRIVATE static void run();
    WTF_EXPORT_PRIVATE void stop();
    WTF_EXPORT_PRIVATE void wakeUp();

    WTF_EXPORT_PRIVATE void suspendFunctionDispatchForCurrentCycle();

    enum class CycleResult { Continue, Stop };
    WTF_EXPORT_PRIVATE CycleResult static cycle(RunLoopMode = DefaultRunLoopMode);

    WTF_EXPORT_PRIVATE void threadWillExit();

#if USE(GLIB_EVENT_LOOP)
    WTF_EXPORT_PRIVATE GMainContext* mainContext() const { return m_mainContext.get(); }
    enum class Event { WillDispatch, DidDispatch };
    using Observer = WTF::Observer<void(Event, const String&)>;
    WTF_EXPORT_PRIVATE void observe(const Observer&);
#endif

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    WTF_EXPORT_PRIVATE static void setWakeUpCallback(WTF::Function<void()>&&);
#endif

#if USE(WINDOWS_EVENT_LOOP)
    static void registerRunLoopMessageWindowClass();
#endif

    class TimerBase {
        WTF_MAKE_FAST_ALLOCATED;
        friend class RunLoop;
    public:
        WTF_EXPORT_PRIVATE explicit TimerBase(RunLoop&);
        WTF_EXPORT_PRIVATE virtual ~TimerBase();

        void startRepeating(Seconds interval) { start(std::max(interval, 0_s), true); }
        void startOneShot(Seconds interval) { start(std::max(interval, 0_s), false); }

        WTF_EXPORT_PRIVATE void stop();
        WTF_EXPORT_PRIVATE bool isActive() const;
        WTF_EXPORT_PRIVATE Seconds secondsUntilFire() const;

        virtual void fired() = 0;

#if USE(GLIB_EVENT_LOOP)
        WTF_EXPORT_PRIVATE void setName(const char*);
        WTF_EXPORT_PRIVATE void setPriority(int);
#endif

    private:
        WTF_EXPORT_PRIVATE void start(Seconds interval, bool repeat);

        Ref<RunLoop> m_runLoop;

#if USE(WINDOWS_EVENT_LOOP)
        bool isActiveWithLock() const WTF_REQUIRES_LOCK(m_runLoop->m_loopLock);
        void timerFired();
        MonotonicTime m_nextFireDate;
        Seconds m_interval;
        bool m_isRepeating { false };
        bool m_isActive { false };
#elif USE(COCOA_EVENT_LOOP)
        RetainPtr<CFRunLoopTimerRef> m_timer;
#elif USE(GLIB_EVENT_LOOP)
        void updateReadyTime();
        GRefPtr<GSource> m_source;
        bool m_isRepeating { false };
        Seconds m_interval { 0 };
#elif USE(GENERIC_EVENT_LOOP)
        bool isActiveWithLock() const WTF_REQUIRES_LOCK(m_runLoop->m_loopLock);
        void stopWithLock() WTF_REQUIRES_LOCK(m_runLoop->m_loopLock);

        class ScheduledTask;
        Ref<ScheduledTask> m_scheduledTask;
#endif
    };

    template <typename TimerFiredClass>
    class Timer : public TimerBase {
    public:
        typedef void (TimerFiredClass::*TimerFiredFunction)();

        Timer(RunLoop& runLoop, TimerFiredClass* o, TimerFiredFunction f)
            : TimerBase(runLoop)
            , m_function(std::bind(f, o))
        {
        }

        Timer(RunLoop& runLoop, Function<void ()>&& function)
            : TimerBase(runLoop)
            , m_function(WTFMove(function))
        {
        }

    private:
        void fired() override { m_function(); }

        Function<void()> m_function;
    };

private:
    class Holder;
    static ThreadSpecific<Holder>& runLoopHolder();

    class DispatchTimer final : public TimerBase {
    public:
        DispatchTimer(RunLoop& runLoop)
            : TimerBase(runLoop)
        {
        }

        void setFunction(Function<void()>&& function)
        {
            m_function = WTFMove(function);
        }
    private:
        void fired() final { m_function(); }

        Function<void()> m_function;
    };

    RunLoop();

    void performWork();

    Deque<Function<void()>> m_currentIteration;

    Lock m_nextIterationLock;
    Deque<Function<void()>> m_nextIteration WTF_GUARDED_BY_LOCK(m_nextIterationLock);

    bool m_isFunctionDispatchSuspended { false };
    bool m_hasSuspendedFunctions { false };

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
    void notify(Event, const char*);

    static GSourceFuncs s_runLoopSourceFunctions;
    GRefPtr<GMainContext> m_mainContext;
    Vector<GRefPtr<GMainLoop>> m_mainLoops;
    GRefPtr<GSource> m_source;
    WeakHashSet<Observer> m_observers;
#elif USE(GENERIC_EVENT_LOOP)
    void scheduleWithLock(TimerBase::ScheduledTask&) WTF_REQUIRES_LOCK(m_loopLock);
    void unscheduleWithLock(TimerBase::ScheduledTask&) WTF_REQUIRES_LOCK(m_loopLock);
    void wakeUpWithLock() WTF_REQUIRES_LOCK(m_loopLock);

    enum class RunMode {
        Iterate,
        Drain
    };

    enum class Status {
        Clear,
        Stopping,
    };
    void runImpl(RunMode);
    bool populateTasks(RunMode, Status&, Deque<Ref<TimerBase::ScheduledTask>>&);

    friend class TimerBase;

    Lock m_loopLock;
    Condition m_readyToRun;
    Condition m_stopCondition;
    RedBlackTree<TimerBase::ScheduledTask, MonotonicTime> m_schedules;
    Vector<Status*> m_mainLoops;
    bool m_shutdown { false };
    bool m_pendingTasks { false };
#endif

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    Function<void()> m_wakeUpCallback;
#endif
};

inline void assertIsCurrent(const RunLoop& runLoop) WTF_ASSERTS_ACQUIRED_CAPABILITY(runLoop)
{
    ASSERT_UNUSED(runLoop, runLoop.isCurrent());
}

} // namespace WTF

using WTF::RunLoop;
using WTF::RunLoopMode;
using WTF::assertIsCurrent;

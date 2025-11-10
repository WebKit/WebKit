/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
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
#include <wtf/CheckedPtr.h>
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
#include <wtf/TypeTraits.h>
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

#if USE(GLIB_EVENT_LOOP)
class ActivityObserver;
#endif

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

#if !PLATFORM(COCOA)
// Classes that offer Timers should be ref-counted of CanMakeCheckedPtr. Please do not add new exceptions.
template<typename T> struct IsDeprecatedTimerSmartPointerException : std::false_type { };
#endif

class WTF_CAPABILITY("is current") RunLoop final : public GuaranteedSerialFunctionDispatcher {
    WTF_MAKE_NONCOPYABLE(RunLoop);
public:
    // Must be called from the main thread.
    WTF_EXPORT_PRIVATE static void initializeMain();
#if USE(WEB_THREAD)
    WTF_EXPORT_PRIVATE static void initializeWeb();
#endif

    WTF_EXPORT_PRIVATE static RunLoop& currentSingleton();
    WTF_EXPORT_PRIVATE static RunLoop& mainSingleton();
#if USE(WEB_THREAD)
    WTF_EXPORT_PRIVATE static RunLoop& webSingleton();
    WTF_EXPORT_PRIVATE static RunLoop* webIfExists();
#endif
    WTF_EXPORT_PRIVATE static Ref<RunLoop> create(ASCIILiteral threadName, ThreadType = ThreadType::Unknown, Thread::QOS = Thread::QOS::UserInitiated);

    static bool isMain() { return mainSingleton().isCurrent(); }
    WTF_EXPORT_PRIVATE bool isCurrent() const final;
    WTF_EXPORT_PRIVATE ~RunLoop() final;

    WTF_EXPORT_PRIVATE void dispatch(Function<void()>&&) final;
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

    enum class Activity : uint8_t {
        BeforeWaiting   = 1 << 0,
        Entry           = 1 << 1,
        Exit            = 1 << 2,
        AfterWaiting    = 1 << 3
    };

#if USE(GLIB_EVENT_LOOP)
    WTF_EXPORT_PRIVATE GMainContext* mainContext() const { return m_mainContext.get(); }

    // Event observers (only used for PageTimelineAgent)
    enum class Event : bool {
        WillDispatch,
        DidDispatch
    };

    using EventObserver = Observer<void(Event, const String&)>;
    WTF_EXPORT_PRIVATE void observeEvent(const EventObserver&);

    WTF_EXPORT_PRIVATE void observeActivity(const Ref<ActivityObserver>&);
    WTF_EXPORT_PRIVATE void unobserveActivity(const Ref<ActivityObserver>&);
#endif

#if USE(GENERIC_EVENT_LOOP) || USE(WINDOWS_EVENT_LOOP)
    WTF_EXPORT_PRIVATE static void setWakeUpCallback(WTF::Function<void()>&&);
#endif

#if USE(WINDOWS_EVENT_LOOP)
    static void registerRunLoopMessageWindowClass();
#endif

    class TimerBase {
        friend class RunLoop;
    public:
        WTF_EXPORT_PRIVATE explicit TimerBase(Ref<RunLoop>&&, ASCIILiteral description);
        WTF_EXPORT_PRIVATE virtual ~TimerBase();

        void startRepeating(Seconds interval) { start(std::max(interval, 0_s), true); }
        void startOneShot(Seconds interval) { start(std::max(interval, 0_s), false); }

        WTF_EXPORT_PRIVATE void stop();
        WTF_EXPORT_PRIVATE bool isActive() const;
        WTF_EXPORT_PRIVATE Seconds secondsUntilFire() const;

        virtual void fired() = 0;

#if USE(GLIB_EVENT_LOOP)
        WTF_EXPORT_PRIVATE void setPriority(int);
#endif

        const ASCIILiteral& description() const { return m_description; }

    private:
        WTF_EXPORT_PRIVATE void start(Seconds interval, bool repeat);

        const Ref<RunLoop> m_runLoop;
        ASCIILiteral m_description;

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
        const Ref<ScheduledTask> m_scheduledTask;
#endif
    };

    class Timer : public TimerBase {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Timer);
    public:
        template <typename TimerFiredClass>
        requires (WTF::HasThreadSafeWeakPtrFunctions<TimerFiredClass>::value)
        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, TimerFiredClass* object, void (TimerFiredClass::*function)())
            : Timer(WTFMove(runLoop), description, [weakObject = ThreadSafeWeakPtr { *object }, function] {
                if (RefPtr object = weakObject.get())
                    (object.get()->*function)();
            })
        {
        }

        template <typename TimerFiredClass>
        requires (!WTF::HasThreadSafeWeakPtrFunctions<TimerFiredClass>::value && WTF::HasWeakPtrFunctions<TimerFiredClass>::value && WTF::HasRefPtrMemberFunctions<TimerFiredClass>::value)
        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, TimerFiredClass* object, void (TimerFiredClass::*function)())
            : Timer(WTFMove(runLoop), description, [weakObject = WeakPtr { *object }, function] {
                if (RefPtr object = weakObject.get())
                    (object.get()->*function)();
            })
        {
        }

        template <typename TimerFiredClass>
        requires (!WTF::HasThreadSafeWeakPtrFunctions<TimerFiredClass>::value && WTF::HasWeakPtrFunctions<TimerFiredClass>::value && !WTF::HasRefPtrMemberFunctions<TimerFiredClass>::value && WTF::HasCheckedPtrMemberFunctions<TimerFiredClass>::value)
        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, TimerFiredClass* object, void (TimerFiredClass::*function)())
            : Timer(WTFMove(runLoop), description, [weakObject = WeakPtr { *object }, function] {
                if (CheckedPtr object = weakObject.get())
                    (object.get()->*function)();
            })
        {
        }

        template <typename TimerFiredClass>
        requires (!WTF::HasThreadSafeWeakPtrFunctions<TimerFiredClass>::value && !WTF::HasWeakPtrFunctions<TimerFiredClass>::value && WTF::HasCheckedPtrMemberFunctions<TimerFiredClass>::value)
        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, TimerFiredClass* object, void (TimerFiredClass::*function)())
            : Timer(WTFMove(runLoop), description, [object = CheckedRef { *object }, function] {
                (object.ptr()->*function)();
            })
        {
        }

        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, Function<void ()>&& function)
            : TimerBase(WTFMove(runLoop), description)
            , m_function(WTFMove(function))
        {
        }

#if !PLATFORM(COCOA)
        // FIXME: This constructor isn't as safe as the other ones and should be removed.
        template <typename TimerFiredClass>
        requires (!WTF::HasRefPtrMemberFunctions<TimerFiredClass>::value && !WTF::HasCheckedPtrMemberFunctions<TimerFiredClass>::value)
        Timer(Ref<RunLoop>&& runLoop, ASCIILiteral description, TimerFiredClass* object, void (TimerFiredClass::*function)())
            : Timer(WTFMove(runLoop), description, std::bind(function, object))
        {
            static_assert(IsDeprecatedTimerSmartPointerException<TimerFiredClass>::value, "Classes using RunLoop::Timer should either be RefCounted or CanMakeCheckedPtr");
        }
#endif

    private:
        void fired() override { m_function(); }

        Function<void()> m_function;
    };

    class DispatchTimer final : public TimerBase, public ThreadSafeRefCounted<DispatchTimer> {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(DispatchTimer);
    public:
        DispatchTimer(RunLoop& runLoop)
            : TimerBase(runLoop, "DispatchTimer"_s)
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

    WTF_EXPORT_PRIVATE Ref<RunLoop::DispatchTimer> dispatchAfter(Seconds, Function<void()>&&);

    WTF_EXPORT_PRIVATE String listActiveTimersForLogging() const;

private:
    class Holder;
    static ThreadSpecific<Holder>& runLoopHolder();

    RunLoop();

    void performWork();

    void registerTimer(TimerBase&);
    void unregisterTimer(TimerBase&);

    mutable Lock m_registeredTimerLock;
    HashSet<TimerBase *> m_registeredTimers;

    Deque<Function<void()>> m_currentIteration;

    Lock m_nextIterationLock;
    Deque<Function<void()>> m_nextIteration WTF_GUARDED_BY_LOCK(m_nextIterationLock);

    bool m_isFunctionDispatchSuspended { false };
    bool m_hasSuspendedFunctions { false };

#if USE(WINDOWS_EVENT_LOOP)
    static LRESULT CALLBACK RunLoopWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    HWND m_runLoopMessageWindow;
    UncheckedKeyHashSet<UINT_PTR> m_liveTimers;

    Lock m_loopLock;
#elif USE(COCOA_EVENT_LOOP)
    static void performWork(void*);
    const RetainPtr<CFRunLoopRef> m_runLoop;
    const RetainPtr<CFRunLoopSourceRef> m_runLoopSource;
#elif USE(GLIB_EVENT_LOOP)
    void notifyEvent(Event, const char*);
    void notifyActivity(Activity);

    void runGLibMainLoop();

    enum class MayBlock { Yes, No };
    void runGLibMainLoopIteration(MayBlock);

    static GSourceFuncs s_runLoopSourceFunctions;
    GRefPtr<GMainContext> m_mainContext;
    GRefPtr<GSource> m_source;

    Lock m_eventObserversLock;
    WeakHashSet<EventObserver> m_eventObservers WTF_GUARDED_BY_LOCK(m_eventObserversLock);

    using ActivityObservers = Vector<Ref<ActivityObserver>, 4>;
    Lock m_activityObserversLock;
    ActivityObservers m_activityObservers WTF_GUARDED_BY_LOCK(m_activityObserversLock);

    static constexpr size_t s_pollFDsCapacity = 64;
    Vector<GPollFD, s_pollFDsCapacity> m_pollFDs;

    int m_nestedLoopLevel { 0 };
    std::atomic<bool> m_shouldStop { false };
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

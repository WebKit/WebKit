/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/RunLoop.h>

#import <algorithm>
#import <mach/mach.h>
#import <memory>
#import <wtf/MainThread.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

static const CFStringRef privateRunLoopMode = CFSTR("TestWebKitAPI.RunLoopCocoa.PrivateMode");

static bool waitUntilMainRunLoopIsWaiting()
{
    auto deadline = MonotonicTime::now() + 5_s;
    while (!CFRunLoopIsWaiting(CFRunLoopGetMain())) {
        if (MonotonicTime::now() > deadline)
            return false;
        usleep(100);
    }
    return true;
}

static void drainMainRunLoop()
{
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource) { }
}

static unsigned queuedWakeUpPortMessageCount()
{
    mach_port_status_t status { };
    mach_msg_type_number_t count = MACH_PORT_RECEIVE_STATUS_COUNT;
    if (mach_port_get_attributes(mach_task_self(), RunLoop::mainSingleton().wakeUpPortForTesting(), MACH_PORT_RECEIVE_STATUS, reinterpret_cast<mach_port_info_t>(&status), &count) != KERN_SUCCESS)
        return 0;
    return status.mps_msgcount;
}

// Keeps privateRunLoopMode non-empty so CFRunLoopRunInMode sleeps until timeout.
class PrivateModeTimer {
public:
    PrivateModeTimer()
    {
        m_timer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 1000, 0, 0, 0, ^(CFRunLoopTimerRef) { }));
        CFRunLoopAddTimer(CFRunLoopGetMain(), m_timer.get(), privateRunLoopMode);
    }

    ~PrivateModeTimer()
    {
        CFRunLoopTimerInvalidate(m_timer.get());
    }

private:
    RetainPtr<CFRunLoopTimerRef> m_timer;
};

class ActivityRecorder {
public:
    ActivityRecorder()
        : m_observer(adoptCF(CFRunLoopObserverCreateWithHandler(kCFAllocatorDefault, kCFRunLoopAllActivities, true, 0, ^(CFRunLoopObserverRef, CFRunLoopActivity activity) {
            m_activities.append(activity);
        })))
    {
        CFRunLoopAddObserver(CFRunLoopGetMain(), m_observer.get(), kCFRunLoopCommonModes);
    }

    ~ActivityRecorder()
    {
        CFRunLoopObserverInvalidate(m_observer.get());
    }

    // Version 1 sources run immediately after kCFRunLoopAfterWaiting, whereas version 0 sources
    // run after another BeforeTimers/BeforeSources pass.
    bool isRunningRightAfterWaking() const { return !m_activities.isEmpty() && m_activities.last() == kCFRunLoopAfterWaiting; }

    size_t count(CFRunLoopActivity activity) const
    {
        return std::count(m_activities.begin(), m_activities.end(), activity);
    }

private:
    RetainPtr<CFRunLoopObserverRef> m_observer;
    Vector<CFRunLoopActivity> m_activities;
};

static Ref<Thread> dispatchToMainWhenItIsWaiting(Function<void()>&& function)
{
    return Thread::create("RunLoopCocoa"_s, [function = WTF::move(function)] mutable {
        EXPECT_TRUE(waitUntilMainRunLoopIsWaiting());
        RunLoop::mainSingleton().dispatch(WTF::move(function));
    });
}

TEST(WTF_RunLoopCocoa, WakeSleepingMainRunLoop)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    ActivityRecorder recorder;
    bool ran = false;
    bool ranRightAfterWaking = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        ran = true;
        ranRightAfterWaking = recorder.isRunningRightAfterWaking();
        CFRunLoopStop(CFRunLoopGetMain());
    });
    EXPECT_EQ(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, false), kCFRunLoopRunStopped);
    thread->waitForCompletion();
    EXPECT_TRUE(ran);
    EXPECT_TRUE(ranRightAfterWaking);
}

TEST(WTF_RunLoopCocoa, ReturnAfterSourceHandled)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    bool ran = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        ran = true;
    });
    EXPECT_EQ(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true), kCFRunLoopRunHandledSource);
    EXPECT_TRUE(ran);
    thread->waitForCompletion();
}

TEST(WTF_RunLoopCocoa, ReturnAfterSourceHandledNSRunLoop)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    bool ran = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        ran = true;
    });
    auto start = MonotonicTime::now();
    EXPECT_TRUE([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:5]]);
    EXPECT_TRUE(ran);
    EXPECT_LT((MonotonicTime::now() - start).seconds(), 4);
    thread->waitForCompletion();
}

TEST(WTF_RunLoopCocoa, CycleRunsDispatchedFunction)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    bool ran = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        ran = true;
    });
    for (unsigned i = 0; i < 200 && !ran; ++i)
        RunLoop::cycle();
    EXPECT_TRUE(ran);
    thread->waitForCompletion();
}

TEST(WTF_RunLoopCocoa, OneEntryAndExitPerWake)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    ActivityRecorder recorder;
    constexpr unsigned wakeCount = 20;
    unsigned ranCount = 0;
    for (unsigned i = 0; i < wakeCount; ++i) {
        auto thread = dispatchToMainWhenItIsWaiting([&] {
            ++ranCount;
        });
        EXPECT_TRUE([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:5]]);
        EXPECT_EQ(ranCount, i + 1);
        thread->waitForCompletion();
    }
    EXPECT_EQ(recorder.count(kCFRunLoopEntry), wakeCount);
    EXPECT_EQ(recorder.count(kCFRunLoopExit), wakeCount);
}

TEST(WTF_RunLoopCocoa, NotRunInPrivateMode)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    PrivateModeTimer timer;
    bool ran = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        ran = true;
    });
    EXPECT_EQ(CFRunLoopRunInMode(privateRunLoopMode, 0.5, false), kCFRunLoopRunTimedOut);
    thread->waitForCompletion();
    EXPECT_FALSE(ran);
    for (unsigned i = 0; i < 10 && !ran; ++i)
        EXPECT_EQ(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true), kCFRunLoopRunHandledSource);
    EXPECT_TRUE(ran);
}

TEST(WTF_RunLoopCocoa, NestedRunWaitsForCrossThreadDispatch)
{
    enum class Step : uint8_t { OuterStart, QueuedBehindOuter, Inner, OuterEnd };
    WTF::initializeMainThread();
    drainMainRunLoop();
    Vector<Step> steps;
    RefPtr<Thread> innerThread;
    bool outerDone = false;
    std::atomic<bool> queuedBehindOuter { false };
    auto outerThread = Thread::create("RunLoopCocoa"_s, [&] {
        EXPECT_TRUE(waitUntilMainRunLoopIsWaiting());
        RunLoop::mainSingleton().dispatch([&] {
            steps.append(Step::OuterStart);
            while (!queuedBehindOuter)
                Thread::yield();
            bool innerRan = false;
            innerThread = dispatchToMainWhenItIsWaiting([&] {
                steps.append(Step::Inner);
                innerRan = true;
            });
            while (!innerRan)
                EXPECT_EQ(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true), kCFRunLoopRunHandledSource);
            steps.append(Step::OuterEnd);
            outerDone = true;
        });
        RunLoop::mainSingleton().dispatch([&] {
            steps.append(Step::QueuedBehindOuter);
        });
        queuedBehindOuter = true;
    });
    while (!outerDone)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true);
    outerThread->waitForCompletion();
    innerThread->waitForCompletion();
    EXPECT_EQ(steps, (Vector<Step> { Step::OuterStart, Step::QueuedBehindOuter, Step::Inner, Step::OuterEnd }));
}

TEST(WTF_RunLoopCocoa, SameThreadWakeDoesNotUseWakeUpPort)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    ASSERT_NE(RunLoop::mainSingleton().wakeUpPortForTesting(), mach_port_t { MACH_PORT_NULL });
    bool ran = false;
    auto thread = dispatchToMainWhenItIsWaiting([&] {
        RunLoop::mainSingleton().dispatch([&] {
            ran = true;
        });
        EXPECT_EQ(queuedWakeUpPortMessageCount(), 0u);
    });
    while (!ran)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true);
    thread->waitForCompletion();

    ran = false;
    RunLoop::mainSingleton().dispatch([&] {
        ran = true;
    });
    EXPECT_EQ(queuedWakeUpPortMessageCount(), 0u);
    while (!ran)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true);
}

TEST(WTF_RunLoopCocoa, WakeUpWhileWakeUpIsQueued)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    mach_port_t port = RunLoop::mainSingleton().wakeUpPortForTesting();
    ASSERT_NE(port, mach_port_t { MACH_PORT_NULL });
    mach_port_urefs_t sendRightsBefore = 0;
    EXPECT_EQ(mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &sendRightsBefore), KERN_SUCCESS);

    PrivateModeTimer timer;
    std::atomic<bool> done { false };
    unsigned queuedMessageCount = 0;
    bool ran = false;
    auto thread = Thread::create("RunLoopCocoa"_s, [&] {
        EXPECT_TRUE(waitUntilMainRunLoopIsWaiting());
        for (unsigned i = 0; i < 10000; ++i)
            RunLoop::mainSingleton().wakeUp();
        RunLoop::mainSingleton().dispatch([&] {
            ran = true;
        });
        queuedMessageCount = queuedWakeUpPortMessageCount();
        done = true;
    });
    while (!done)
        CFRunLoopRunInMode(privateRunLoopMode, 0.05, false);
    thread->waitForCompletion();
    EXPECT_FALSE(ran);
    EXPECT_EQ(queuedMessageCount, 1u);

    mach_port_urefs_t sendRights = 0;
    EXPECT_EQ(mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &sendRights), KERN_SUCCESS);
    EXPECT_EQ(sendRights, sendRightsBefore);

    EXPECT_EQ(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, true), kCFRunLoopRunHandledSource);
    EXPECT_TRUE(ran);
    EXPECT_EQ(queuedWakeUpPortMessageCount(), 0u);
    EXPECT_EQ(mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &sendRights), KERN_SUCCESS);
    EXPECT_EQ(sendRights, sendRightsBefore);
}

TEST(WTF_RunLoopCocoa, InterleavedCallOnMainThreadAndDispatchRunInOrder)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    constexpr unsigned functionCount = 2000;
    auto order = std::make_shared<Vector<unsigned>>();
    auto thread = Thread::create("RunLoopCocoa"_s, [order] {
        for (unsigned i = 0; i < functionCount; ++i) {
            auto function = [order, i] {
                order->append(i);
            };
            if (i % 2)
                callOnMainThread(WTF::move(function));
            else
                RunLoop::mainSingleton().dispatch(WTF::move(function));
            if (!(i % 50))
                usleep(200);
        }
    });
    auto deadline = MonotonicTime::now() + 10_s;
    while (order->size() < functionCount && MonotonicTime::now() < deadline)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    thread->waitForCompletion();
    ASSERT_EQ(order->size(), functionCount);
    for (unsigned i = 0; i < functionCount; ++i)
        EXPECT_EQ(order->at(i), i);
}

TEST(WTF_RunLoopCocoa, NoLostWakeUps)
{
    WTF::initializeMainThread();
    drainMainRunLoop();
    constexpr unsigned threadCount = 8;
    constexpr unsigned dispatchesPerThread = 100000;
    constexpr unsigned totalDispatches = threadCount * dispatchesPerThread;

    struct State {
        std::atomic<unsigned> dispatched { 0 };
        unsigned executed { 0 };
    };
    auto state = std::make_shared<State>();

    Vector<Ref<Thread>> threads;
    for (unsigned i = 0; i < threadCount; ++i) {
        threads.append(Thread::create("RunLoopCocoa"_s, [state, i] {
            for (unsigned j = 0; j < dispatchesPerThread; ++j) {
                RunLoop::mainSingleton().dispatch([state, i, j] {
                    ++state->executed;
                    if (!i && !(j % 1009))
                        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
                    if (i == 1 && !(j % 997))
                        RunLoop::mainSingleton().suspendFunctionDispatchForCurrentCycle();
                });
                ++state->dispatched;
                if (!(j % 64))
                    usleep((j % 1024) ? 10 : 500);
            }
        }));
    }

    RetainPtr timer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 0.001, 0.001, 0, 0, ^(CFRunLoopTimerRef) { }));
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer.get(), kCFRunLoopCommonModes);

    bool stalled = false;
    unsigned lastExecuted = 0;
    auto lastProgress = MonotonicTime::now();
    for (unsigned iteration = 0; state->executed < totalDispatches; ++iteration) {
        switch (iteration % 6) {
        case 0:
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
            break;
        case 1:
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, false);
            break;
        case 2:
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            break;
        case 3: {
            auto busyUntil = MonotonicTime::now() + 50_us;
            while (MonotonicTime::now() < busyUntil) { }
            break;
        }
        case 4:
            RunLoop::cycle();
            break;
        case 5:
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
            break;
        }

        auto now = MonotonicTime::now();
        if (state->executed != lastExecuted) {
            lastExecuted = state->executed;
            lastProgress = now;
        } else if (state->executed < state->dispatched && now - lastProgress > 1_s) {
            stalled = true;
            break;
        }
    }
    EXPECT_FALSE(stalled);

    for (auto& thread : threads)
        thread->waitForCompletion();
    CFRunLoopTimerInvalidate(timer.get());

    if (stalled)
        RunLoop::mainSingleton().wakeUp();
    auto deadline = MonotonicTime::now() + 10_s;
    while (state->executed < totalDispatches && MonotonicTime::now() < deadline)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    EXPECT_EQ(state->executed, totalDispatches);
}

} // namespace TestWebKitAPI

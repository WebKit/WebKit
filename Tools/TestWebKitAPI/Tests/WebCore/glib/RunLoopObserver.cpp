/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "Test.h"
#include "Utilities.h"

#include <WebCore/PlatformExportMacros.h>
#include <WebCore/RunLoopObserver.h>
#include <atomic>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

using namespace WTF;
using namespace WebCore;

// ============================================================================
// 1. RunLoopObserver lifecycle tests
// ============================================================================

TEST(RunLoopObserver, Schedule)
{
    WTF::initializeMainThread();

    bool observerCalled = false;
    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        observerCalled = true;
    });

    EXPECT_FALSE(observer->isScheduled());

    observer->schedule();
    EXPECT_TRUE(observer->isScheduled());

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_TRUE(observerCalled);
    EXPECT_TRUE(observer->isScheduled());

    observer->invalidate();
    EXPECT_FALSE(observer->isScheduled());
}

TEST(RunLoopObserver, Invalidate)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;
    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);

    EXPECT_EQ(observerCallCount, 1u);

    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);

    EXPECT_EQ(observerCallCount, 2u);

    observer->invalidate();
    EXPECT_FALSE(observer->isScheduled());

    bool done3 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done3 = true;
    });
    Util::run(&done3);

    EXPECT_EQ(observerCallCount, 2u);
}

TEST(RunLoopObserver, MultipleSchedule)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;
    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);

    EXPECT_EQ(observerCallCount, 1u);

    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);

    EXPECT_EQ(observerCallCount, 2u);

    observer->invalidate();
    EXPECT_FALSE(observer->isScheduled());

    bool done3 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done3 = true;
    });
    Util::run(&done3);

    EXPECT_EQ(observerCallCount, 2u);
}

TEST(RunLoopObserver, MultipleInvalidate)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;
    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);

    EXPECT_EQ(observerCallCount, 1u);

    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);

    EXPECT_EQ(observerCallCount, 2u);

    observer->invalidate();
    EXPECT_FALSE(observer->isScheduled());

    observer->invalidate();
    EXPECT_FALSE(observer->isScheduled());

    bool done3 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done3 = true;
    });
    Util::run(&done3);

    EXPECT_EQ(observerCallCount, 2u);
}

TEST(RunLoopObserver, Destruction)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;
    {
        auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
            ++observerCallCount;
        });

        observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
        EXPECT_TRUE(observer->isScheduled());
    }

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_EQ(observerCallCount, 0u);
}

// ============================================================================
// 2. Repeating vs. one-shot tests
// ============================================================================

TEST(RunLoopObserver, Repeating)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    }, RunLoopObserver::Type::Repeating);

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);
    EXPECT_EQ(observerCallCount, 1u);

    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);
    EXPECT_EQ(observerCallCount, 2u);

    bool done3 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done3 = true;
    });

    Util::run(&done3);
    EXPECT_EQ(observerCallCount, 3u);

    observer->invalidate();
}

TEST(RunLoopObserver, OneShot)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    }, RunLoopObserver::Type::OneShot);

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);
    EXPECT_EQ(observerCallCount, 1u);

    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);
    EXPECT_EQ(observerCallCount, 1u);

    bool done3 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done3 = true;
    });

    Util::run(&done3);
    EXPECT_EQ(observerCallCount, 1u);

    observer->invalidate();
}

// ============================================================================
// 3. Activity type coverage tests
// ============================================================================
TEST(RunLoopObserver, DefaultActivities)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    // Schedule with default activities (BeforeWaiting | Exit)
    observer->schedule();

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    // With default activities, only the BeforeWaiting observer should fire.
    EXPECT_EQ(observerCallCount, 1u);

    observer->invalidate();
}

TEST(RunLoopObserver, ActivityEntry)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::Entry });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    // Entry observer doesn't fire if we only iterate the run loop.
    EXPECT_EQ(observerCallCount, 0u);

    observer->invalidate();
}

TEST(RunLoopObserver, ActivityExit)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::Exit });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    // Exit observer doesn't fire if we only iterate the run loop.
    EXPECT_EQ(observerCallCount, 0u);

    observer->invalidate();
}

TEST(RunLoopObserver, ActivityBeforeWaiting)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_EQ(observerCallCount, 1u);

    observer->invalidate();
}

TEST(RunLoopObserver, ActivityAfterWaiting)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::AfterWaiting });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_EQ(observerCallCount, 1u);

    observer->invalidate();
}

TEST(RunLoopObserver, ActivityCombination)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting, RunLoop::Activity::Exit });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    // Exit observer doesn't fire if we only iterate the run loop.
    EXPECT_EQ(observerCallCount, 1u);

    observer->invalidate();
}

// ============================================================================
// 4. Edge cases tests
// ============================================================================

TEST(RunLoopObserver, RemovesSelfDuringCallback)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    std::unique_ptr<RunLoopObserver> observer;
    observer = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
        if (observerCallCount == 1) {
            // Invalidate self during first callback
            observer->invalidate();
        }
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);
    EXPECT_EQ(observerCallCount, 1u);

    // Run again to verify observer doesn't fire after self-invalidation
    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);
    EXPECT_EQ(observerCallCount, 1u);
}

TEST(RunLoopObserver, AddsNewObserverDuringCallback)
{
    WTF::initializeMainThread();

    unsigned observerCallCount1 = 0;
    unsigned observerCallCount2 = 0;
    std::unique_ptr<RunLoopObserver> observer2;

    auto observer1 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount1;
        if (observerCallCount1 == 1) {
            // Create and schedule a new observer during first callback
            observer2 = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
                ++observerCallCount2;
            });
            observer2->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
        }
    });

    observer1->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);
    EXPECT_EQ(observerCallCount1, 1u);
    EXPECT_EQ(observerCallCount2, 0u);

    // Run again to verify the newly added observer fires
    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);
    EXPECT_EQ(observerCallCount1, 2u);
    EXPECT_EQ(observerCallCount2, 1u);

    observer1->invalidate();
    if (observer2)
        observer2->invalidate();
}

TEST(RunLoopObserver, AcrossMultipleIterations)
{
    WTF::initializeMainThread();

    unsigned observerCallCount = 0;

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    for (unsigned i = 0; i < 5; ++i) {
        bool done = false;
        RunLoop::currentSingleton().dispatch([&] {
            done = true;
        });
        Util::run(&done);
    }

    EXPECT_EQ(observerCallCount, 5u);

    observer->invalidate();
}

// ============================================================================
// 5. WellKnownOrder tests
// ============================================================================

TEST(RunLoopObserver, WellKnownOrderValues)
{
    WTF::initializeMainThread();

    Vector<unsigned> observerExecutionOrder;

    auto observer1 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [&observerExecutionOrder] {
        observerExecutionOrder.append(static_cast<int>(RunLoopObserver::WellKnownOrder::GraphicsCommit));
    });

    auto observer2 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::RenderingUpdate, [&observerExecutionOrder] {
        observerExecutionOrder.append(static_cast<int>(RunLoopObserver::WellKnownOrder::RenderingUpdate));
    });

    auto observer3 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&observerExecutionOrder] {
        observerExecutionOrder.append(static_cast<int>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate));
    });

    observer1->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    observer2->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    observer3->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_EQ(observerExecutionOrder.size(), 3u);
    EXPECT_LE(observerExecutionOrder[0], observerExecutionOrder[1]);
    EXPECT_LE(observerExecutionOrder[1], observerExecutionOrder[2]);

    observer1->invalidate();
    observer2->invalidate();
    observer3->invalidate();
}

TEST(RunLoopObserver, DifferentWellKnownOrderValues)
{
    WTF::initializeMainThread();

    unsigned observerCallCount1 = 0;
    unsigned observerCallCount2 = 0;
    unsigned observerCallCount3 = 0;
    unsigned observerCallCount4 = 0;

    auto observer1 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::InspectorFrameBegin, [&] {
        ++observerCallCount1;
    });

    auto observer2 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::InspectorFrameBegin, [&] {
        ++observerCallCount2;
    });

    auto observer3 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::InspectorFrameEnd, [&] {
        ++observerCallCount3;
    });

    auto observer4 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::OpportunisticTask, [&] {
        ++observerCallCount4;
    });

    observer1->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    observer2->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    observer3->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    observer4->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });

    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_EQ(observerCallCount1, 1u);
    EXPECT_EQ(observerCallCount2, 1u);
    EXPECT_EQ(observerCallCount3, 1u);
    EXPECT_EQ(observerCallCount4, 1u);

    observer1->invalidate();
    observer2->invalidate();
    observer3->invalidate();
    observer4->invalidate();
}

// ============================================================================
// 6. Threading tests
// ============================================================================

// Helper class to manage a secondary RunLoop in a separate thread
class SecondaryRunLoopThread {
public:
    SecondaryRunLoopThread()
        : m_runLoop(RunLoop::create("SecondaryRunLoopThread"_s, ThreadType::Graphics))
    {
        BinarySemaphore semaphore;
        m_runLoop->dispatch([this, &semaphore] {
            m_threadId = &Thread::currentSingleton();
            semaphore.signal();
        });
        semaphore.wait();
    }

    ~SecondaryRunLoopThread()
    {
        BinarySemaphore semaphore;
        m_runLoop->dispatch([&semaphore] {
            RunLoop::currentSingleton().stop();
            semaphore.signal();
        });
        semaphore.wait();
    }

    RefPtr<RunLoop> runLoop() const { return m_runLoop; }
    Thread* threadId() const { return m_threadId; }

    void dispatch(Function<void()>&& function)
    {
        m_runLoop->dispatch(WTF::move(function));
    }

private:
    RefPtr<RunLoop> m_runLoop;
    Thread* m_threadId { nullptr };
};

TEST(RunLoopObserver, CrossThread_ScheduleOnMainLoopFromSecondaryThread)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };
    std::atomic<Thread*> observerThread { nullptr };
    Thread* mainThread = &Thread::currentSingleton();

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
        observerThread.store(&Thread::currentSingleton());
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule observer from secondary thread, but attach it to main loop
    BinarySemaphore scheduleSemaphore;
    secondaryThread.dispatch([&] {
        observer->schedule(RunLoop::mainSingleton(), { RunLoop::Activity::BeforeWaiting });
        scheduleSemaphore.signal();
    });
    scheduleSemaphore.wait();

    EXPECT_TRUE(observer->isScheduled());

    // Run main loop and verify observer fires on main thread
    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    EXPECT_GE(observerCallCount.load(), 1u);
    EXPECT_EQ(observerThread.load(), mainThread);

    observer->invalidate();
}

TEST(RunLoopObserver, CrossThread_ScheduleOnSecondaryLoopFromSecondaryThread)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };
    std::atomic<Thread*> observerThread { nullptr };

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
        observerThread.store(&Thread::currentSingleton());
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule observer from secondary thread to secondary loop
    BinarySemaphore scheduleSemaphore;
    secondaryThread.dispatch([&] {
        observer->schedule(secondaryThread.runLoop(), { RunLoop::Activity::BeforeWaiting });
        scheduleSemaphore.signal();
    });
    scheduleSemaphore.wait();

    EXPECT_TRUE(observer->isScheduled());

    // Dispatch a task on secondary thread and wait for it
    // Note: BeforeWaiting fires AFTER the dispatched task completes, when the RunLoop
    // prepares to wait again. We need two dispatches to ensure BeforeWaiting has fired.
    BinarySemaphore doneSemaphore1;
    secondaryThread.dispatch([&] {
        doneSemaphore1.signal();
    });
    doneSemaphore1.wait();

    BinarySemaphore doneSemaphore2;
    secondaryThread.dispatch([&] {
        doneSemaphore2.signal();
    });
    doneSemaphore2.wait();

    EXPECT_GE(observerCallCount.load(), 1u);
    EXPECT_EQ(observerThread.load(), secondaryThread.threadId());

    observer->invalidate();
}

TEST(RunLoopObserver, CrossThread_ScheduleOnSecondaryLoopFromMainThread)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };
    std::atomic<Thread*> observerThread { nullptr };

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
        observerThread.store(&Thread::currentSingleton());
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule observer from main thread to secondary loop
    observer->schedule(secondaryThread.runLoop(), { RunLoop::Activity::BeforeWaiting });

    EXPECT_TRUE(observer->isScheduled());

    // Dispatch a task on secondary thread and wait for it
    // Note: BeforeWaiting fires AFTER the dispatched task completes, when the RunLoop
    // prepares to wait again. We need two dispatches to ensure BeforeWaiting has fired:
    // 1st dispatch: triggers RunLoop iteration, task runs, then BeforeWaiting fires
    // 2nd dispatch: confirms BeforeWaiting from previous iteration has completed
    BinarySemaphore doneSemaphore1;
    secondaryThread.dispatch([&] {
        doneSemaphore1.signal();
    });
    doneSemaphore1.wait();

    BinarySemaphore doneSemaphore2;
    secondaryThread.dispatch([&] {
        doneSemaphore2.signal();
    });
    doneSemaphore2.wait();

    EXPECT_GE(observerCallCount.load(), 1u);
    EXPECT_EQ(observerThread.load(), secondaryThread.threadId());

    observer->invalidate();
}

TEST(RunLoopObserver, CrossThread_TwoObserversBothOnMainLoop)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount1 { 0 };
    std::atomic<unsigned> observerCallCount2 { 0 };
    std::atomic<Thread*> observerThread1 { nullptr };
    std::atomic<Thread*> observerThread2 { nullptr };
    Thread* mainThread = &Thread::currentSingleton();

    auto observer1 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::GraphicsCommit, [&] {
        ++observerCallCount1;
        observerThread1.store(&Thread::currentSingleton());
    });

    auto observer2 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount2;
        observerThread2.store(&Thread::currentSingleton());
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule first observer from main thread
    observer1->schedule(RunLoop::mainSingleton(), { RunLoop::Activity::BeforeWaiting });

    // Schedule second observer from secondary thread to main loop
    BinarySemaphore scheduleSemaphore;
    secondaryThread.dispatch([&] {
        observer2->schedule(RunLoop::mainSingleton(), { RunLoop::Activity::BeforeWaiting });
        scheduleSemaphore.signal();
    });
    scheduleSemaphore.wait();

    EXPECT_TRUE(observer1->isScheduled());
    EXPECT_TRUE(observer2->isScheduled());

    // Run main loop
    bool done = false;
    RunLoop::currentSingleton().dispatch([&] {
        done = true;
    });

    Util::run(&done);

    // Both observers should fire on main thread
    EXPECT_GE(observerCallCount1.load(), 1u);
    EXPECT_GE(observerCallCount2.load(), 1u);
    EXPECT_EQ(observerThread1.load(), mainThread);
    EXPECT_EQ(observerThread2.load(), mainThread);

    observer1->invalidate();
    observer2->invalidate();
}

TEST(RunLoopObserver, CrossThread_TwoObserversOnDifferentLoops)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount1 { 0 };
    std::atomic<unsigned> observerCallCount2 { 0 };
    std::atomic<Thread*> observerThread1 { nullptr };
    std::atomic<Thread*> observerThread2 { nullptr };
    Thread* mainThread = &Thread::currentSingleton();

    auto observer1 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount1;
        observerThread1.store(&Thread::currentSingleton());
    });

    auto observer2 = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount2;
        observerThread2.store(&Thread::currentSingleton());
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule observer1 to main loop
    observer1->schedule(RunLoop::mainSingleton(), { RunLoop::Activity::BeforeWaiting });

    // Schedule observer2 to secondary loop
    observer2->schedule(secondaryThread.runLoop(), { RunLoop::Activity::BeforeWaiting });

    EXPECT_TRUE(observer1->isScheduled());
    EXPECT_TRUE(observer2->isScheduled());

    // Run main loop
    bool doneMain = false;
    RunLoop::currentSingleton().dispatch([&] {
        doneMain = true;
    });

    Util::run(&doneMain);

    // Also dispatch on secondary thread
    // Note: BeforeWaiting fires AFTER the dispatched task completes, when the RunLoop
    // prepares to wait again. We need two dispatches to ensure BeforeWaiting has fired.
    BinarySemaphore doneSecondary1;
    secondaryThread.dispatch([&] {
        doneSecondary1.signal();
    });
    doneSecondary1.wait();

    BinarySemaphore doneSecondary2;
    secondaryThread.dispatch([&] {
        doneSecondary2.signal();
    });
    doneSecondary2.wait();

    // Observer1 should fire on main thread, observer2 on secondary
    EXPECT_GE(observerCallCount1.load(), 1u);
    EXPECT_GE(observerCallCount2.load(), 1u);
    EXPECT_EQ(observerThread1.load(), mainThread);
    EXPECT_EQ(observerThread2.load(), secondaryThread.threadId());

    observer1->invalidate();
    observer2->invalidate();
}

TEST(RunLoopObserver, CrossThread_InvalidateFromDifferentThread)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    observer->schedule(nullptr, { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    // Fire observer once
    bool done1 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done1 = true;
    });

    Util::run(&done1);

    unsigned callCountBeforeInvalidate = observerCallCount.load();
    EXPECT_GE(callCountBeforeInvalidate, 1u);

    SecondaryRunLoopThread secondaryThread;

    // Invalidate from secondary thread
    BinarySemaphore invalidateSemaphore;
    secondaryThread.dispatch([&] {
        observer->invalidate();
        invalidateSemaphore.signal();
    });
    invalidateSemaphore.wait();

    EXPECT_FALSE(observer->isScheduled());

    // Run main loop again
    bool done2 = false;
    RunLoop::currentSingleton().dispatch([&] {
        done2 = true;
    });

    Util::run(&done2);

    // Observer should not fire anymore
    EXPECT_EQ(observerCallCount.load(), callCountBeforeInvalidate);
}

TEST(RunLoopObserver, CrossThread_OneShotObserverOnSecondaryLoop)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };
    std::atomic<Thread*> observerThread { nullptr };

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
        observerThread.store(&Thread::currentSingleton());
    }, RunLoopObserver::Type::OneShot);

    SecondaryRunLoopThread secondaryThread;

    // Schedule one-shot observer on secondary loop
    observer->schedule(secondaryThread.runLoop(), { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    // Dispatch multiple tasks on secondary thread
    BinarySemaphore done1;
    secondaryThread.dispatch([&] {
        done1.signal();
    });
    done1.wait();

    BinarySemaphore done2;
    secondaryThread.dispatch([&] {
        done2.signal();
    });
    done2.wait();

    BinarySemaphore done3;
    secondaryThread.dispatch([&] {
        done3.signal();
    });
    done3.wait();

    // Observer should have fired exactly once
    EXPECT_EQ(observerCallCount.load(), 1u);
    EXPECT_EQ(observerThread.load(), secondaryThread.threadId());

    observer->invalidate();
}

TEST(RunLoopObserver, CrossThread_MultipleScheduleAttempts)
{
    WTF::initializeMainThread();

    std::atomic<unsigned> observerCallCount { 0 };

    auto observer = makeUniqueRef<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [&] {
        ++observerCallCount;
    });

    SecondaryRunLoopThread secondaryThread;

    // Schedule observer on secondary loop
    observer->schedule(secondaryThread.runLoop(), { RunLoop::Activity::BeforeWaiting });
    EXPECT_TRUE(observer->isScheduled());

    // Try to schedule again from different thread (should be ignored)
    BinarySemaphore scheduleSemaphore;
    secondaryThread.dispatch([&] {
        observer->schedule(RunLoop::mainSingleton(), { RunLoop::Activity::BeforeWaiting });
        scheduleSemaphore.signal();
    });
    scheduleSemaphore.wait();

    // Observer should still be scheduled (first schedule wins)
    EXPECT_TRUE(observer->isScheduled());

    // Dispatch on secondary thread and verify observer fires there
    // Note: BeforeWaiting fires AFTER the dispatched task completes, when the RunLoop
    // prepares to wait again. We need two dispatches to ensure BeforeWaiting has fired.
    BinarySemaphore doneSemaphore1;
    secondaryThread.dispatch([&] {
        doneSemaphore1.signal();
    });
    doneSemaphore1.wait();

    BinarySemaphore doneSemaphore2;
    secondaryThread.dispatch([&] {
        doneSemaphore2.signal();
    });
    doneSemaphore2.wait();

    EXPECT_GE(observerCallCount.load(), 1u);

    // Run main loop - observer should NOT fire here
    unsigned countBeforeMainLoop = observerCallCount.load();
    bool doneMain = false;
    RunLoop::currentSingleton().dispatch([&] {
        doneMain = true;
    });

    Util::run(&doneMain);

    // Count should be unchanged since observer is on secondary loop
    EXPECT_EQ(observerCallCount.load(), countBeforeMainLoop);

    observer->invalidate();
}

} // namespace TestWebKitAPI

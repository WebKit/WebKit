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
#include <wtf/glib/ActivityObserver.h>

#include "Test.h"
#include <wtf/RunLoop.h>

namespace TestWebKitAPI {

using namespace WTF;

static void dispatchChecker(bool& done)
{
    RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
        if (done)
            RunLoop::currentSingleton().stop();
        else {
            // Re-dispatch to check again after current work completes
            dispatchChecker(done);
        }
    });
}

// Helper function to run a test while the RunLoop is already active
static void runTestWhileRunLoopIsActive(Function<void()>&& testFunction, bool& done)
{
    ASSERT(!done);

    // Schedule test to run immediately after RunLoop starts (0ms delay)
    RunLoop::currentSingleton().dispatchAfter(0_ms, [testFunction = WTFMove(testFunction)] {
        testFunction();
    });

    // Start checker to stop RunLoop when test completes
    dispatchChecker(done);
    RunLoop::run();
}

// ============================================================================
// 1. Basic ActivityObserver tests
// ============================================================================

TEST(WTF_ActivityObserver, Create)
{
    WTF::initializeMainThread();

    bool observerCalled = false;
    RefPtr observer = ActivityObserver::create(RunLoop::currentSingleton(), true, 10, { RunLoop::Activity::BeforeWaiting }, [&] {
        observerCalled = true;
    });

    EXPECT_TRUE(observer.get());
    EXPECT_EQ(observer->order(), 10u);
    EXPECT_FALSE(observer->activities().contains(RunLoop::Activity::AfterWaiting));
    EXPECT_FALSE(observer->activities().contains(RunLoop::Activity::Entry));
    EXPECT_FALSE(observer->activities().contains(RunLoop::Activity::Exit));
    EXPECT_TRUE(observer->activities().contains(RunLoop::Activity::BeforeWaiting));
    EXPECT_FALSE(observerCalled);

    // Manually trigger notification
    observer->start();
    observer->notify();
    observer->stop();
    EXPECT_TRUE(observerCalled);
}

TEST(WTF_ActivityObserver, MatchingActivity)
{
    WTF::initializeMainThread();

    bool done = false;

    bool observerCalled = false;
    RefPtr<ActivityObserver> observer;

    runTestWhileRunLoopIsActive([&] {
        observer = ActivityObserver::create(RunLoop::currentSingleton(), true, 1, { RunLoop::Activity::BeforeWaiting }, [&] {
            observerCalled = true;
        });

        observer->start();

        RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
            done = true;
            observer->stop();
        });
    }, done);

    EXPECT_TRUE(observerCalled);
}

TEST(WTF_ActivityObserver, NonMatchingActivity)
{
    WTF::initializeMainThread();

    bool done = false;

    bool lateObserverCalled = false;
    RefPtr<ActivityObserver> lateObserver;

    // This observer will fire upon run loop entry, as we observe the Entry activity.
    bool earlyObserverCalled = false;
    RefPtr<ActivityObserver> earlyObserver = ActivityObserver::create(RunLoop::currentSingleton(), true, 1, { RunLoop::Activity::Entry }, [&] {
        earlyObserverCalled = true;
    });

    earlyObserver->start();

    runTestWhileRunLoopIsActive([&] {
        lateObserver = ActivityObserver::create(RunLoop::currentSingleton(), true, 1, { RunLoop::Activity::Entry }, [&] {
            lateObserverCalled = true;
        });

        lateObserver->start();

        RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
            done = true;
            earlyObserver->stop();
            lateObserver->stop();
        });
    }, done);

    EXPECT_TRUE(earlyObserverCalled);
    EXPECT_FALSE(lateObserverCalled);
}

TEST(WTF_ActivityObserver, MultipleActivities)
{
    WTF::initializeMainThread();

    bool done = false;

    unsigned observerCallCount = 0;
    RefPtr<ActivityObserver> observer;

    runTestWhileRunLoopIsActive([&] {
        observer = ActivityObserver::create(RunLoop::currentSingleton(), true, 1, { RunLoop::Activity::BeforeWaiting }, [&] {
            ++observerCallCount;
        });

        observer->start();

        RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
            done = true;
            observer->stop();
        });
    }, done);

    EXPECT_GT(observerCallCount, 0u);
}

// ============================================================================
// 2. Observer ordering tests
// ============================================================================

TEST(WTF_ActivityObserver, Ordering)
{
    WTF::initializeMainThread();

    bool done = false;

    Vector<unsigned> observerExecutionOrder;
    RefPtr<ActivityObserver> observer1;
    RefPtr<ActivityObserver> observer2;
    RefPtr<ActivityObserver> observer3;

    runTestWhileRunLoopIsActive([&] {
        observer1 = ActivityObserver::create(RunLoop::currentSingleton(), true, 30, { RunLoop::Activity::BeforeWaiting }, [&] {
            observerExecutionOrder.append(30);
        });

        observer2 = ActivityObserver::create(RunLoop::currentSingleton(), true, 10, { RunLoop::Activity::BeforeWaiting }, [&] {
            observerExecutionOrder.append(10);
        });

        observer3 = ActivityObserver::create(RunLoop::currentSingleton(), true, 20, { RunLoop::Activity::BeforeWaiting }, [&] {
            observerExecutionOrder.append(20);
        });

        observer1->start();
        observer2->start();
        observer3->start();

        RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
            done = true;
            observer1->stop();
            observer2->stop();
            observer3->stop();
        });
    }, done);

    EXPECT_EQ(observerExecutionOrder.size(), 3u);
    EXPECT_EQ(observerExecutionOrder[0], 10u);
    EXPECT_EQ(observerExecutionOrder[1], 20u);
    EXPECT_EQ(observerExecutionOrder[2], 30u);
}

TEST(WTF_ActivityObserver, SameOrder)
{
    WTF::initializeMainThread();

    bool done = false;

    unsigned observerCallCount1 = 0;
    unsigned observerCallCount2 = 0;
    RefPtr<ActivityObserver> observer1;
    RefPtr<ActivityObserver> observer2;

    runTestWhileRunLoopIsActive([&] {
        observer1 = ActivityObserver::create(RunLoop::currentSingleton(), true, 10, { RunLoop::Activity::BeforeWaiting }, [&] {
            ++observerCallCount1;
        });

        observer2 = ActivityObserver::create(RunLoop::currentSingleton(), true, 10, { RunLoop::Activity::Entry, RunLoop::Activity::BeforeWaiting }, [&] {
            ++observerCallCount2;
        });

        observer1->start();
        observer2->start();

        RunLoop::currentSingleton().dispatchAfter(0_ms, [&] {
            done = true;
            observer1->stop();
            observer2->stop();
        });
    }, done);

    EXPECT_EQ(observerCallCount1, 1u);
    EXPECT_EQ(observerCallCount2, 1u);
}

} // namespace TestWebKitAPI

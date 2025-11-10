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

} // namespace TestWebKitAPI

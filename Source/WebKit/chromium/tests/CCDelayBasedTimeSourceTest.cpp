/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCDelayBasedTimeSource.h"

#include "CCSchedulerTestCommon.h"
#include "cc/CCThread.h"
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace WTF;
using namespace WebCore;
using namespace WebKitTests;

namespace {

TEST(CCDelayBasedTimeSourceTest, TaskPostedAndTickCalled)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);

    timer->setMonotonicallyIncreasingTimeMs(0);
    timer->setActive(true);
    EXPECT_TRUE(timer->active());
    EXPECT_TRUE(thread.hasPendingTask());

    timer->setMonotonicallyIncreasingTimeMs(16);
    thread.runPendingTask();
    EXPECT_TRUE(timer->active());
    EXPECT_TRUE(client.tickCalled());
}

TEST(CCDelayBasedTimeSource, TickNotCalledWithTaskPosted)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    EXPECT_TRUE(thread.hasPendingTask());
    timer->setActive(false);
    thread.runPendingTask();
    EXPECT_FALSE(client.tickCalled());
}

TEST(CCDelayBasedTimeSource, StartTwiceEnqueuesOneTask)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    EXPECT_TRUE(thread.hasPendingTask());
    thread.reset();
    timer->setActive(true);
    EXPECT_FALSE(thread.hasPendingTask());
}

TEST(CCDelayBasedTimeSource, StartWhenRunningDoesntTick)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    thread.runPendingTask();
    thread.reset();
    timer->setActive(true);
    EXPECT_FALSE(thread.hasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenExactlyOnRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    double interval = 1000.0 / 60.0;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());

    timer->setMonotonicallyIncreasingTimeMs(interval);
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());
}

// At 60Hz, when the tick returns at slightly after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    double interval = 1000.0 / 60.0;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());

    timer->setMonotonicallyIncreasingTimeMs(interval + 0.0001);
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenExactlyTwiceAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    double interval = 1000.0 / 60.0;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());

    timer->setMonotonicallyIncreasingTimeMs(2*interval);
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterTwiceRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    double interval = 1000.0 / 60.0;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());

    timer->setMonotonicallyIncreasingTimeMs(2*interval + 0.0001);
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenHalfAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    double interval = 1000.0 / 60.0;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval, &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelay());

    timer->setMonotonicallyIncreasingTimeMs(interval + interval * 0.5);
    thread.runPendingTask();

    EXPECT_EQ(8, thread.pendingDelay());
}


TEST(CCDelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise)
{
    int numIterations = 1000;

    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);
    timer->setActive(true);

    double totalFrameTime = 0;
    for (int i = 0; i < numIterations; ++i) {
        long long delay = thread.pendingDelay();

        // accumulate the "delay"
        totalFrameTime += delay;

        // Run the callback exactly when asked
        double now = timer->monotonicallyIncreasingTimeMs() + delay;
        timer->setMonotonicallyIncreasingTimeMs(now);
        thread.runPendingTask();
    }
    double averageInterval = totalFrameTime / static_cast<double>(numIterations);
    EXPECT_NEAR(1000.0 / 60.0, averageInterval, 0.1);
}

TEST(CCDelayBasedTimeSource, TestDeactivateWhilePending)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);
    timer->setActive(true); // Should post a task.
    timer->setActive(false);
    timer.clear();
    thread.runPendingTask(); // Should run the posted task without crashing.
}

TEST(CCDelayBasedTimeSource, TestDeactivateAndReactivateBeforeNextTickTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);

    // Should run the activate task, and pick up a new timebase.
    timer->setActive(true);
    timer->setMonotonicallyIncreasingTimeMs(0);
    thread.runPendingTask();

    // Stop the timer
    timer->setActive(false);

    // Task will be pending anyway, run it
    thread.runPendingTask();

    // Start the timer again, but before the next tick time the timer previously
    // planned on using. That same tick time should still be targeted.
    timer->setMonotonicallyIncreasingTimeMs(4);
    timer->setActive(true);
    EXPECT_EQ(12, thread.pendingDelay());
}

TEST(CCDelayBasedTimeSource, TestDeactivateAndReactivateAfterNextTickTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(1000.0 / 60.0, &thread);
    timer->setClient(&client);

    // Should run the activate task, and pick up a new timebase.
    timer->setActive(true);
    timer->setMonotonicallyIncreasingTimeMs(0);
    thread.runPendingTask();

    // Stop the timer
    timer->setActive(false);

    // Task will be pending anyway, run it
    thread.runPendingTask();

    // Start the timer again, but before the next tick time the timer previously
    // planned on using. That same tick time should still be targeted.
    timer->setMonotonicallyIncreasingTimeMs(20);
    timer->setActive(true);
    EXPECT_EQ(13, thread.pendingDelay());
}

}

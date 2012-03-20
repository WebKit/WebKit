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

#include "cc/CCFrameRateController.h"

#include "CCSchedulerTestCommon.h"
#include <gtest/gtest.h>

using namespace WTF;
using namespace WebCore;
using namespace WebKitTests;

namespace {

class FakeCCFrameRateControllerClient : public WebCore::CCFrameRateControllerClient {
public:
    FakeCCFrameRateControllerClient() { reset(); }

    void reset() { m_vsyncTicked = false; }
    bool vsyncTicked() const { return m_vsyncTicked; }

    virtual void vsyncTick() { m_vsyncTicked = true; }

protected:
    bool m_vsyncTicked;
};


TEST(CCFrameRateControllerTest, TestFrameThrottling_ImmediateAck)
{
    FakeCCThread thread;
    FakeCCFrameRateControllerClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timeSource = FakeCCDelayBasedTimeSource::create(1.0 / 60.0, &thread);
    CCFrameRateController controller(timeSource);

    controller.setClient(&client);
    controller.setActive(true);

    double elapsed = 0; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += thread.pendingDelayMs() / 1000.0;
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.didBeginFrame();

    // Tell the controller the frame ended 5ms later
    timeSource->setMonotonicallyIncreasingTime(timeSource->monotonicallyIncreasingTime() + 0.005);
    controller.didFinishFrame();

    // Trigger another frame, make sure vsync runs again
    elapsed += thread.pendingDelayMs() / 1000.0;
    EXPECT_TRUE(elapsed >= timeSource->monotonicallyIncreasingTime()); // Sanity check that previous code didn't move time backward.
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

TEST(CCFrameRateControllerTest, TestFrameThrottling_TwoFramesInFlight)
{
    FakeCCThread thread;
    FakeCCFrameRateControllerClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timeSource = FakeCCDelayBasedTimeSource::create(1.0 / 60.0, &thread);
    CCFrameRateController controller(timeSource);

    controller.setClient(&client);
    controller.setActive(true);
    controller.setMaxFramesPending(2);

    double elapsed = 0; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += thread.pendingDelayMs() / 1000.0;
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.didBeginFrame();

    // Trigger another frame, make sure vsync callback runs again
    elapsed += thread.pendingDelayMs() / 1000.0;
    EXPECT_TRUE(elapsed >= timeSource->monotonicallyIncreasingTime()); // Sanity check that previous code didn't move time backward.
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew, again.
    controller.didBeginFrame();

    // Trigger another frame. Since two frames are pending, we should not draw.
    elapsed += thread.pendingDelayMs() / 1000.0;
    EXPECT_TRUE(elapsed >= timeSource->monotonicallyIncreasingTime()); // Sanity check that previous code didn't move time backward.
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_FALSE(client.vsyncTicked());

    // Tell the controller the first frame ended 5ms later
    timeSource->setMonotonicallyIncreasingTime(timeSource->monotonicallyIncreasingTime() + 0.005);
    controller.didFinishFrame();

    // Tick should not have been called
    EXPECT_FALSE(client.vsyncTicked());

    // Trigger yet another frame. Since one frames is pending, another vsync callback should run.
    elapsed += thread.pendingDelayMs() / 1000.0;
    EXPECT_TRUE(elapsed >= timeSource->monotonicallyIncreasingTime()); // Sanity check that previous code didn't move time backward.
    timeSource->setMonotonicallyIncreasingTime(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

}

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

#include "config.h"
#include <WebCore/WebGPUFramePacer.h>
#include <wtf/MonotonicTime.h>

using WebCore::FramesPerSecond;
using WebCore::WebGPUFramePacer;

namespace TestWebKitAPI {

static MonotonicTime feedFrames(WebGPUFramePacer& pacer, MonotonicTime start, double frameCostMs, unsigned count)
{
    auto t = start;
    for (unsigned i = 0; i < count; ++i) {
        t = t + Seconds::fromMilliseconds(frameCostMs);
        pacer.recordFrame(Seconds::fromMilliseconds(frameCostMs), t);
    }
    return t;
}

static constexpr FramesPerSecond k60Hz = 60;

TEST(WebGPUFramePacer, NoRateBeforeWarmUp)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto start = MonotonicTime() + Seconds(1);
    auto now = feedFrames(pacer, start, 45.0, 4);
    EXPECT_FALSE(pacer.preferredFramesPerSecond(now).has_value());
}

TEST(WebGPUFramePacer, FullRefreshContentIsNotPaced)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 1000.0 / 60.0, 40);
    EXPECT_FALSE(pacer.preferredFramesPerSecond(now).has_value());
}

TEST(WebGPUFramePacer, ConvergesTo20For45msFrames)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    auto rate = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, LocksInFirstRateImmediatelyAfterWarmUp)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    // The first sustainable rate should engage as soon as the sample window is
    // warmed up (8 samples), rather than ramping down one rung per confirmation
    // window, so the startup jitter window is as short as possible.
    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 8);
    auto rate = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, ConvergesTo12For83msFrames)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 83.0, 60);
    auto rate = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(12));
}

TEST(WebGPUFramePacer, ConvergedRateIsStable)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto start = MonotonicTime() + Seconds(1);
    auto now = feedFrames(pacer, start, 45.0, 60);
    auto first = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(first.has_value());

    for (unsigned i = 0; i < 60; ++i) {
        now = feedFrames(pacer, now, 45.0, 1);
        auto rate = pacer.preferredFramesPerSecond(now);
        ASSERT_TRUE(rate.has_value());
        EXPECT_EQ(*rate, *first);
    }
}

TEST(WebGPUFramePacer, SingleSpikeDoesNotStepDown)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    auto before = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(*before, FramesPerSecond(20));

    now = feedFrames(pacer, now, 200.0, 1);
    auto after = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, SustainedOverloadStepsDown)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    ASSERT_EQ(*pacer.preferredFramesPerSecond(now), FramesPerSecond(20));

    now = feedFrames(pacer, now, 83.0, 60);
    auto after = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, FramesPerSecond(12));
}

TEST(WebGPUFramePacer, IdleCanvasStopsPacing)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    ASSERT_TRUE(pacer.preferredFramesPerSecond(now).has_value());

    auto later = now + Seconds(1);
    EXPECT_FALSE(pacer.preferredFramesPerSecond(later).has_value());
}

TEST(WebGPUFramePacer, DivisorLadderFollows120HzDisplay)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(120);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    auto rate = pacer.preferredFramesPerSecond(now);
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, ClosedLoopDoesNotRatchet)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    const auto trueCost = Seconds::fromMilliseconds(25.0);
    auto now = MonotonicTime() + Seconds(1);

    std::optional<FramesPerSecond> rate;
    for (unsigned i = 0; i < 400; ++i) {
        auto paced = pacer.preferredFramesPerSecond(now);
        double periodMs = paced ? 1000.0 / *paced : 1000.0 / 60.0;
        now = now + Seconds::fromMilliseconds(periodMs);
        pacer.recordFrame(trueCost, now);
        rate = pacer.preferredFramesPerSecond(now);
    }

    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(30));
}

TEST(WebGPUFramePacer, RecoversWhenWorkloadEases)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = feedFrames(pacer, MonotonicTime() + Seconds(1), 45.0, 60);
    ASSERT_TRUE(pacer.preferredFramesPerSecond(now).has_value());
    EXPECT_EQ(*pacer.preferredFramesPerSecond(now), FramesPerSecond(20));

    now = feedFrames(pacer, now, 12.0, 200);
    EXPECT_FALSE(pacer.preferredFramesPerSecond(now).has_value());
}

} // namespace TestWebKitAPI

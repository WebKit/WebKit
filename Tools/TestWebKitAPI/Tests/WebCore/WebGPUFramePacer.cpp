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
#include <algorithm>
#include <wtf/Deque.h>
#include <wtf/MonotonicTime.h>

using WebCore::FramesPerSecond;
using WebCore::WebGPUFramePacer;

namespace TestWebKitAPI {

static constexpr FramesPerSecond k60Hz = 60;

// Drives a pacer the way the presentation context drives the real one: the canvas presents at whatever
// rate the pacer asks for, the GPU executes submitted frames one at a time, and a present blocks until
// the frame from two presents ago has finished.
class GPUTimeline {
public:
    explicit GPUTimeline(FramesPerSecond displayRate)
        : m_displayRate(displayRate)
    {
        m_pacer.setDisplayNominalFramesPerSecond(displayRate);
    }

    // minimumPresentIntervalMs models a page that cannot present as fast as the pacer would allow.
    void present(double gpuCostMs, unsigned count, double minimumPresentIntervalMs = 0)
    {
        for (unsigned i = 0; i < count; ++i)
            presentOnce(Seconds::fromMilliseconds(gpuCostMs), Seconds::fromMilliseconds(minimumPresentIntervalMs));
    }

    std::optional<FramesPerSecond> rate() const { return m_pacer.preferredFramesPerSecond(m_now); }
    std::optional<FramesPerSecond> rateAfter(Seconds idle) const { return m_pacer.preferredFramesPerSecond(m_now + idle); }

private:
    static constexpr size_t maximumInFlightFrames = 2;

    struct InFlightFrame {
        MonotonicTime completion;
        Seconds cost;
    };

    void presentOnce(Seconds gpuCost, Seconds minimumPresentInterval)
    {
        auto paced = m_pacer.preferredFramesPerSecond(m_now);
        m_now = m_now + std::max(Seconds { 1.0 / (paced ? *paced : m_displayRate) }, minimumPresentInterval);

        std::optional<InFlightFrame> drained;
        Seconds stall;
        if (m_inFlight.size() >= maximumInFlightFrames) {
            drained = m_inFlight.takeFirst();
            if (drained->completion > m_now) {
                stall = drained->completion - m_now;
                m_now = drained->completion;
            }
        }

        m_gpuIdleAt = std::max(m_now, m_gpuIdleAt) + gpuCost;
        m_inFlight.append({ m_gpuIdleAt, gpuCost });

        m_pacer.recordFrame(drained ? drained->cost : 0_s, stall, m_now);
    }

    WebGPUFramePacer m_pacer;
    FramesPerSecond m_displayRate;
    MonotonicTime m_now { MonotonicTime() + Seconds(1) };
    MonotonicTime m_gpuIdleAt { MonotonicTime() + Seconds(1) };
    Deque<InFlightFrame> m_inFlight;
};

TEST(WebGPUFramePacer, CheapContentIsNotPaced)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(2.0, 120);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, FullRefreshContentIsNotPaced)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(1000.0 / 60.0, 120);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, ConvergesTo20For45msFrames)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    auto rate = timeline.rate();
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, ConvergesTo12For83msFrames)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(83.0, 60);
    auto rate = timeline.rate();
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(12));
}

TEST(WebGPUFramePacer, ConvergedRateIsStable)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    auto first = timeline.rate();
    ASSERT_TRUE(first.has_value());

    for (unsigned i = 0; i < 120; ++i) {
        timeline.present(45.0, 1);
        auto rate = timeline.rate();
        ASSERT_TRUE(rate.has_value());
        EXPECT_EQ(*rate, *first);
    }
}

TEST(WebGPUFramePacer, ClosedLoopDoesNotRatchet)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(25.0, 400);
    auto rate = timeline.rate();
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(30));
}

TEST(WebGPUFramePacer, SingleSpikeDoesNotPaceFullRateContent)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(4.0, 60);
    ASSERT_FALSE(timeline.rate().has_value());

    // One slow frame backs presents up for many frames afterwards, but the frames themselves are cheap.
    timeline.present(200.0, 1);
    timeline.present(4.0, 60);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, SingleSpikeDoesNotStepDown)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    auto before = timeline.rate();
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(*before, FramesPerSecond(20));

    timeline.present(200.0, 1);
    timeline.present(45.0, 8);
    auto after = timeline.rate();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, SustainedOverloadStepsDown)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    ASSERT_EQ(*timeline.rate(), FramesPerSecond(20));

    timeline.present(83.0, 60);
    auto after = timeline.rate();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, FramesPerSecond(12));
}

TEST(WebGPUFramePacer, BurstOfSlowFramesRecovers)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(2.0, 60);
    ASSERT_FALSE(timeline.rate().has_value());

    // The regression: eight frames of about 80 ms of GPU work lowered the rate to a divisor of the
    // refresh rate and it never came back.
    timeline.present(80.0, 8);
    auto during = timeline.rate();
    ASSERT_TRUE(during.has_value());
    EXPECT_EQ(*during, FramesPerSecond(12));

    timeline.present(2.0, 40);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, RecoversWhenWorkloadEases)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    ASSERT_TRUE(timeline.rate().has_value());
    EXPECT_EQ(*timeline.rate(), FramesPerSecond(20));

    timeline.present(12.0, 60);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, ExpensiveFramesThatKeepUpAreNotPaced)
{
    GPUTimeline timeline(k60Hz);
    // A page whose script holds it to 10 fps leaves the GPU 100 ms for a 45 ms frame, so presents never wait.
    timeline.present(45.0, 120, 100.0);
    EXPECT_FALSE(timeline.rate().has_value());
}

TEST(WebGPUFramePacer, IdleCanvasStopsPacing)
{
    GPUTimeline timeline(k60Hz);
    timeline.present(45.0, 60);
    ASSERT_TRUE(timeline.rate().has_value());

    EXPECT_FALSE(timeline.rateAfter(Seconds(1)).has_value());
}

TEST(WebGPUFramePacer, DivisorLadderFollows120HzDisplay)
{
    GPUTimeline timeline(120);
    timeline.present(45.0, 60);
    auto rate = timeline.rate();
    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(*rate, FramesPerSecond(20));
}

TEST(WebGPUFramePacer, ResetReturnsToUnpaced)
{
    WebGPUFramePacer pacer;
    pacer.setDisplayNominalFramesPerSecond(k60Hz);

    auto now = MonotonicTime() + Seconds(1);
    for (unsigned i = 0; i < 8; ++i) {
        now = now + Seconds::fromMilliseconds(16.0);
        pacer.recordFrame(83_ms, 60_ms, now);
    }
    ASSERT_TRUE(pacer.preferredFramesPerSecond(now).has_value());

    pacer.reset();
    EXPECT_FALSE(pacer.preferredFramesPerSecond(now).has_value());
}

} // namespace TestWebKitAPI

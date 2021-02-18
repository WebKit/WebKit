/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <WebCore/AnimationFrameRate.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(AnimationFrameRate, framesPerSecondNearestFullSpeed)
{
    ASSERT_EQ(framesPerSecondNearestFullSpeed(240), FramesPerSecond(60));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(144), FramesPerSecond(72));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(120), FramesPerSecond(60));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(90), FramesPerSecond(90));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(60), FramesPerSecond(60));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(48), FramesPerSecond(48));
    ASSERT_EQ(framesPerSecondNearestFullSpeed(30), FramesPerSecond(30));
}

TEST(AnimationFrameRate, preferredFrameIntervalWithUnspecifiedNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    Optional<FramesPerSecond> unspecifiedNominalFramesPerSecond;
    ASSERT_EQ(preferredFrameInterval(noThrottling, unspecifiedNominalFramesPerSecond), FullSpeedAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, unspecifiedNominalFramesPerSecond), HalfSpeedThrottlingAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, unspecifiedNominalFramesPerSecond), HalfSpeedThrottlingAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, unspecifiedNominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWithFullSpeedNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    ASSERT_EQ(preferredFrameInterval(noThrottling, FullSpeedFramesPerSecond), FullSpeedAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, FullSpeedFramesPerSecond), HalfSpeedThrottlingAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, FullSpeedFramesPerSecond), HalfSpeedThrottlingAnimationInterval);
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, FullSpeedFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWith144FPSNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    FramesPerSecond nominalFramesPerSecond = 144;
    ASSERT_EQ(preferredFrameInterval(noThrottling, nominalFramesPerSecond), Seconds(1.0 / 72));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, nominalFramesPerSecond), Seconds(2.0 / 72));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::VisuallyIdle }, nominalFramesPerSecond), Seconds(2.0 / 72));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, nominalFramesPerSecond), Seconds(2.0 / 72));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, nominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWith120FPSNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    FramesPerSecond nominalFramesPerSecond = 120;
    ASSERT_EQ(preferredFrameInterval(noThrottling, nominalFramesPerSecond), Seconds(1.0 / 60));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, nominalFramesPerSecond), Seconds(2.0 / 60));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::VisuallyIdle }, nominalFramesPerSecond), Seconds(2.0 / 60));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, nominalFramesPerSecond), Seconds(2.0 / 60));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, nominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWith90FPSNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    FramesPerSecond nominalFramesPerSecond = 90;
    ASSERT_EQ(preferredFrameInterval(noThrottling, nominalFramesPerSecond), Seconds(1.0 / 90));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, nominalFramesPerSecond), Seconds(2.0 / 90));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::VisuallyIdle }, nominalFramesPerSecond), Seconds(2.0 / 90));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, nominalFramesPerSecond), Seconds(2.0 / 90));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, nominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWith48FPSNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    FramesPerSecond nominalFramesPerSecond = 48;
    ASSERT_EQ(preferredFrameInterval(noThrottling, nominalFramesPerSecond), Seconds(1.0 / 48));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, nominalFramesPerSecond), Seconds(2.0 / 48));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::VisuallyIdle }, nominalFramesPerSecond), Seconds(2.0 / 48));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, nominalFramesPerSecond), Seconds(2.0 / 48));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, nominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFrameIntervalWith30FPSNominalFramesPerSecond)
{
    OptionSet<ThrottlingReason> noThrottling;
    FramesPerSecond nominalFramesPerSecond = 30;
    ASSERT_EQ(preferredFrameInterval(noThrottling, nominalFramesPerSecond), Seconds(1.0 / 30));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::LowPowerMode }, nominalFramesPerSecond), Seconds(2.0 / 30));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::VisuallyIdle }, nominalFramesPerSecond), Seconds(2.0 / 30));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::NonInteractedCrossOriginFrame }, nominalFramesPerSecond), Seconds(2.0 / 30));
    ASSERT_EQ(preferredFrameInterval({ ThrottlingReason::OutsideViewport }, nominalFramesPerSecond), AggressiveThrottlingAnimationInterval);
}

TEST(AnimationFrameRate, preferredFramesPerSecond)
{
    ASSERT_EQ(preferredFramesPerSecond(FullSpeedAnimationInterval), FullSpeedFramesPerSecond);
    ASSERT_EQ(preferredFramesPerSecond(HalfSpeedThrottlingAnimationInterval), HalfSpeedThrottlingFramesPerSecond);
    ASSERT_EQ(preferredFramesPerSecond(Seconds(1.0 / 60)), FramesPerSecond(60));
}

}

/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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


#include "config.h"

#if ENABLE(WEB_AUDIO)
#include <WebCore/AudioParamTimeline.h>
#include <cmath>

using namespace WebCore;
using WTF::Seconds;

static float expInterp(float v1, float v2, Seconds t, Seconds t1, Seconds t2)
{
    double ratio = static_cast<double>(v2) / static_cast<double>(v1);
    double alpha = (t - t1) / (t2 - t1);
    return static_cast<float>(static_cast<double>(v1) * std::pow(ratio, alpha));
}

TEST(WebCore, AudioParamTimeline_OppositeSigns_PositiveToNegative)
{
    const float value1 = 2.0f;
    const float value2 = -1.0f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 1.0 };

    float r = AudioParamTimeline::exponentialRampAtTime(Seconds { 0.5 }, value1, time1, value2, time2);
    EXPECT_FLOAT_EQ(r, value1);
}

TEST(WebCore, AudioParamTimeline_OppositeSigns_NegativeToPositive)
{
    const float value1 = -5.0f;
    const float value2 = 3.0f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 1.0 };

    float r = AudioParamTimeline::exponentialRampAtTime(Seconds { 0.3 }, value1, time1, value2, time2);
    EXPECT_FLOAT_EQ(r, value1);
}

TEST(WebCore, AudioParamTimeline_OppositeSigns_MultipleTimes)
{
    const float value1 = 4.0f;
    const float value2 = -2.0f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 2.0 };

    EXPECT_FLOAT_EQ(AudioParamTimeline::exponentialRampAtTime(time1, value1, time1, value2, time2), value1);
    EXPECT_FLOAT_EQ(AudioParamTimeline::exponentialRampAtTime(Seconds { 0.5 }, value1, time1, value2, time2), value1);
    EXPECT_FLOAT_EQ(AudioParamTimeline::exponentialRampAtTime(Seconds { 1.5 }, value1, time1, value2, time2), value1);
    EXPECT_FLOAT_EQ(AudioParamTimeline::exponentialRampAtTime(time2, value1, time1, value2, time2), value1);
}

TEST(WebCore, AudioParamTimeline_OppositeSigns_SmallMagnitudes)
{
    const float value1 = 0.001f;
    const float value2 = -0.001f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 1.0 };

    float r = AudioParamTimeline::exponentialRampAtTime(Seconds { 0.7 }, value1, time1, value2, time2);
    EXPECT_NEAR(r, value1, 1e-7f);
}

TEST(WebCore, AudioParamTimeline_OppositeSigns_LargeMagnitudes)
{
    const float value1 = -1000.0f;
    const float value2 = 500.0f;
    const Seconds time1 { 0.5 };
    const Seconds time2 { 1.5 };

    float r = AudioParamTimeline::exponentialRampAtTime(Seconds { 0.9 }, value1, time1, value2, time2);
    EXPECT_FLOAT_EQ(r, value1);
}

TEST(WebCore, AudioParamTimeline_SameSign_PositiveFollowsExponential)
{
    const float value1 = 1.0f;
    const float value2 = 4.0f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 1.0 };
    const Seconds t { 0.5 };

    float r = AudioParamTimeline::exponentialRampAtTime(t, value1, time1, value2, time2);
    float expected = expInterp(value1, value2, t, time1, time2);
    EXPECT_NEAR(r, expected, 1e-3f);
}

TEST(WebCore, AudioParamTimeline_SameSign_NegativeFollowsExponential)
{
    const float value1 = -2.0f;
    const float value2 = -8.0f;
    const Seconds time1 { 0.0 };
    const Seconds time2 { 1.0 };
    const Seconds t { 0.5 };

    float r = AudioParamTimeline::exponentialRampAtTime(t, value1, time1, value2, time2);
    float expected = expInterp(value1, value2, t, time1, time2);
    EXPECT_NEAR(r, expected, 1e-3f);
}

#endif // ENABLE(WEB_AUDIO)

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include <public/WebFloatAnimationCurve.h>

#include "CCTimingFunction.h"

#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

// Tests that a float animation with one keyframe works as expected.
TEST(WebFloatAnimationCurveTest, OneFloatKeyframe)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 2), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve.getValue(-1));
    EXPECT_FLOAT_EQ(2, curve.getValue(0));
    EXPECT_FLOAT_EQ(2, curve.getValue(0.5));
    EXPECT_FLOAT_EQ(2, curve.getValue(1));
    EXPECT_FLOAT_EQ(2, curve.getValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(WebFloatAnimationCurveTest, TwoFloatKeyframe)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 4), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve.getValue(-1));
    EXPECT_FLOAT_EQ(2, curve.getValue(0));
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve.getValue(1));
    EXPECT_FLOAT_EQ(4, curve.getValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(WebFloatAnimationCurveTest, ThreeFloatKeyframe)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 4), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(2, 8), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve.getValue(-1));
    EXPECT_FLOAT_EQ(2, curve.getValue(0));
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve.getValue(1));
    EXPECT_FLOAT_EQ(6, curve.getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve.getValue(2));
    EXPECT_FLOAT_EQ(8, curve.getValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(WebFloatAnimationCurveTest, RepeatedFloatKeyTimes)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 4), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 4), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 6), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(2, 6), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(4, curve.getValue(-1));
    EXPECT_FLOAT_EQ(4, curve.getValue(0));
    EXPECT_FLOAT_EQ(4, curve.getValue(0.5));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    float value = curve.getValue(1);
    EXPECT_TRUE(value >= 4 && value <= 6);

    EXPECT_FLOAT_EQ(6, curve.getValue(1.5));
    EXPECT_FLOAT_EQ(6, curve.getValue(2));
    EXPECT_FLOAT_EQ(6, curve.getValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(WebFloatAnimationCurveTest, UnsortedKeyframes)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(2, 8), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(0, 2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 4), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(2, curve.getValue(-1));
    EXPECT_FLOAT_EQ(2, curve.getValue(0));
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve.getValue(1));
    EXPECT_FLOAT_EQ(6, curve.getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve.getValue(2));
    EXPECT_FLOAT_EQ(8, curve.getValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebFloatAnimationCurveTest, CubicBezierTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), 0.25, 0, 0.75, 1);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(0, curve.getValue(0));
    EXPECT_LT(0, curve.getValue(0.25));
    EXPECT_GT(0.25, curve.getValue(0.25));
    EXPECT_FLOAT_EQ(0.5, curve.getValue(0.5));
    EXPECT_LT(0.75, curve.getValue(0.75));
    EXPECT_GT(1, curve.getValue(0.75));
    EXPECT_FLOAT_EQ(1, curve.getValue(1));
}

// Tests that an ease timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), WebAnimationCurve::TimingFunctionTypeEase);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

// Tests using a linear timing function.
TEST(WebFloatAnimationCurveTest, LinearTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(time, curve.getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), WebAnimationCurve::TimingFunctionTypeEaseIn);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseInTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseOutTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), WebAnimationCurve::TimingFunctionTypeEaseOut);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInOutTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0), WebAnimationCurve::TimingFunctionTypeEaseInOut);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseInOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, CustomBezierTimingFunction)
{
    WebFloatAnimationCurve curve;
    double x1 = 0.3;
    double y1 = 0.2;
    double x2 = 0.8;
    double y2 = 0.7;
    curve.add(WebFloatKeyframe(0, 0), x1, y1, x2, y2);
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCCubicBezierTimingFunction::create(x1, y1, x2, y2));
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

// Tests that the default timing function is indeed ease.
TEST(WebFloatAnimationCurveTest, DefaultTimingFunction)
{
    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, 0));
    curve.add(WebFloatKeyframe(1, 1), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time));
    }
}

} // namespace

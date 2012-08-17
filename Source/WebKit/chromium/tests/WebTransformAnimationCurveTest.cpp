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

#include <public/WebTransformAnimationCurve.h>

#include "CCTimingFunction.h"

#include <gtest/gtest.h>
#include <public/WebTransformOperations.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

// Tests that a transform animation with one keyframe works as expected.
TEST(WebTransformAnimationCurveTest, OneTransformKeyframe)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations;
    operations.appendTranslate(2, 0, 0);
    curve.add(WebTransformKeyframe(0, operations), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(2, curve.getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(0).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(0.5).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(1).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(2).m41());
}

// Tests that a transform animation with two keyframes works as expected.
TEST(WebTransformAnimationCurveTest, TwoTransformKeyframe)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve.getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(1).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(2).m41());
}

// Tests that a transform animation with three keyframes works as expected.
TEST(WebTransformAnimationCurveTest, ThreeTransformKeyframe)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebKit::WebTransformOperations operations3;
    operations3.appendTranslate(8, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(2, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(2, curve.getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(1).m41());
    EXPECT_FLOAT_EQ(6, curve.getValue(1.5).m41());
    EXPECT_FLOAT_EQ(8, curve.getValue(2).m41());
    EXPECT_FLOAT_EQ(8, curve.getValue(3).m41());
}

// Tests that a transform animation with multiple keys at a given time works sanely.
TEST(WebTransformAnimationCurveTest, RepeatedTransformKeyTimes)
{
    // A step function.
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(4, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebKit::WebTransformOperations operations3;
    operations3.appendTranslate(6, 0, 0);
    WebKit::WebTransformOperations operations4;
    operations4.appendTranslate(6, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(2, operations4), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(4, curve.getValue(-1).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(0).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(0.5).m41());

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    WebTransformationMatrix value = curve.getValue(1);
    EXPECT_TRUE(value.m41() >= 4 && value.m41() <= 6);

    EXPECT_FLOAT_EQ(6, curve.getValue(1.5).m41());
    EXPECT_FLOAT_EQ(6, curve.getValue(2).m41());
    EXPECT_FLOAT_EQ(6, curve.getValue(3).m41());
}

// Tests that the keyframes may be added out of order.
TEST(WebTransformAnimationCurveTest, UnsortedKeyframes)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(2, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(4, 0, 0);
    WebKit::WebTransformOperations operations3;
    operations3.appendTranslate(8, 0, 0);
    curve.add(WebTransformKeyframe(2, operations3), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    EXPECT_FLOAT_EQ(2, curve.getValue(-1).m41());
    EXPECT_FLOAT_EQ(2, curve.getValue(0).m41());
    EXPECT_FLOAT_EQ(3, curve.getValue(0.5).m41());
    EXPECT_FLOAT_EQ(4, curve.getValue(1).m41());
    EXPECT_FLOAT_EQ(6, curve.getValue(1.5).m41());
    EXPECT_FLOAT_EQ(8, curve.getValue(2).m41());
    EXPECT_FLOAT_EQ(8, curve.getValue(3).m41());
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebTransformAnimationCurveTest, CubicBezierTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), 0.25, 0, 0.75, 1);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);
    EXPECT_FLOAT_EQ(0, curve.getValue(0).m41());
    EXPECT_LT(0, curve.getValue(0.25).m41());
    EXPECT_GT(0.25, curve.getValue(0.25).m41());
    EXPECT_FLOAT_EQ(0.5, curve.getValue(0.5).m41());
    EXPECT_LT(0.75, curve.getValue(0.75).m41());
    EXPECT_GT(1, curve.getValue(0.75).m41());
    EXPECT_FLOAT_EQ(1, curve.getValue(1).m41());
}

// Tests that an ease timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEase);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

// Tests using a linear timing function.
TEST(WebTransformAnimationCurveTest, LinearTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeLinear);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(time, curve.getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseInTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseIn);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseInTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseOutTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseOut);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, EaseInOutTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), WebAnimationCurve::TimingFunctionTypeEaseInOut);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseInOutTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

// Tests that an ease in timing function works as expected.
TEST(WebTransformAnimationCurveTest, CustomBezierTimingFunction)
{
    WebTransformAnimationCurve curve;
    double x1 = 0.3;
    double y1 = 0.2;
    double x2 = 0.8;
    double y2 = 0.7;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1), x1, y1, x2, y2);
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCCubicBezierTimingFunction::create(x1, y1, x2, y2));
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

// Tests that the default timing function is indeed ease.
TEST(WebTransformAnimationCurveTest, DefaultTimingFunction)
{
    WebTransformAnimationCurve curve;
    WebKit::WebTransformOperations operations1;
    operations1.appendTranslate(0, 0, 0);
    WebKit::WebTransformOperations operations2;
    operations2.appendTranslate(1, 0, 0);
    curve.add(WebTransformKeyframe(0, operations1));
    curve.add(WebTransformKeyframe(1, operations2), WebAnimationCurve::TimingFunctionTypeLinear);

    OwnPtr<WebCore::CCTimingFunction> timingFunction(WebCore::CCEaseTimingFunction::create());
    for (int i = 0; i <= 4; ++i) {
        const double time = i * 0.25;
        EXPECT_FLOAT_EQ(timingFunction->getValue(time), curve.getValue(time).m41());
    }
}

} // namespace

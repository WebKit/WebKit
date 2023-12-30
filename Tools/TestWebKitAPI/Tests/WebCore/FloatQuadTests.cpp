/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2017-2021 Google Inc. All rights reserved.
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

#include <WebCore/FloatQuad.h>
#include <WebCore/GeometryUtilities.h>
#include <wtf/text/TextStream.h>

namespace TestWebKitAPI {
using namespace WebCore;

static bool areApproximatelyEqual(float a, float b)
{
    return std::abs(a - b) < 0.001;
}

static void expectRotatedRect(const FloatPoint& center, const FloatSize& size, float angle, const RotatedRect& info)
{
    areApproximatelyEqual(info.angleInRadians, angle);
    areApproximatelyEqual(info.center.x(), center.x());
    areApproximatelyEqual(info.center.y(), center.y());
    areApproximatelyEqual(info.size.width(), size.width());
    areApproximatelyEqual(info.size.height(), size.height());
}

TEST(FloatQuad, RotatedBoundingRectForPoint)
{
    expectRotatedRect({ 1, -5 }, { 0, 0 }, 0, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(1, -5),
        FloatPoint(1, -5),
        FloatPoint(1, -5),
        FloatPoint(1, -5)
    }));
}

TEST(FloatQuad, RotatedBoundingRectForLineSegment)
{
    expectRotatedRect({ 0, -5 }, { 20, 0 }, 0, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(-10, -5),
        FloatPoint(10, -5),
        FloatPoint(10, -5),
        FloatPoint(-10, -5)
    }));
}

TEST(FloatQuad, RotatedBoundingRectForTriangle)
{
    expectRotatedRect({ 10, 0 }, { 40, 20 }, 0, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(-10, -10),
        FloatPoint(30, 0),
        FloatPoint(30, 0),
        FloatPoint(-10, 10)
    }));
}

TEST(FloatQuad, RotatedBoundingRectForTrapezoid)
{
    expectRotatedRect({ 25, 15 }, { 28.284, 14.142 }, 5.498, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(10, 20),
        FloatPoint(20, 10),
        FloatPoint(40, 10),
        FloatPoint(20, 30)
    }));

    expectRotatedRect({ 20, 25 }, { 26.83, 13.42 }, 1.107, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(20, 10),
        FloatPoint(30, 30),
        FloatPoint(20, 40),
        FloatPoint(10, 20)
    }));
}

TEST(FloatQuad, RotatedBoundingRectWithMinimumAngle)
{
    expectRotatedRect({ 20, -14.95 }, { 20, 10.1 }, 0, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(10, -20),
        FloatPoint(30, -19.9),
        FloatPoint(30, -9.9),
        FloatPoint(10, -10)
    }, 0.01));

    expectRotatedRect({ 20, -15.05 }, { 20, 10.1 }, 0, rotatedBoundingRectWithMinimumAngleOfRotation({
        FloatPoint(10, -20),
        FloatPoint(30, -20.1),
        FloatPoint(30, -10.1),
        FloatPoint(10, -10)
    }, 0.01));
}

static void checkIsEmpty(bool expectation, FloatQuad&& quad)
{
    EXPECT_TRUE(expectation == quad.isEmpty());
    if (expectation == quad.isEmpty())
        return;

    TextStream stream;
    stream << quad;
    WTFLogAlways("Expected quad: %s to be %s", stream.release().utf8().data(), expectation ? "empty" : "non-empty");
}

TEST(FloatQuad, IsEmpty)
{
    checkIsEmpty(true, { // Line segment.
        FloatPoint(0, 0),
        FloatPoint(0, 0),
        FloatPoint(0, 3),
        FloatPoint(0, 3),
    });

    checkIsEmpty(false, { // Triangle.
        FloatPoint(0, 1),
        FloatPoint(0, 0),
        FloatPoint(3, 3),
        FloatPoint(3, 3),
    });

    checkIsEmpty(false, { // Another triangle.
        FloatPoint(0, 0),
        FloatPoint(0, 0),
        FloatPoint(3, 3),
        FloatPoint(0, 3),
    });

    checkIsEmpty(true, { // Single point.
        FloatPoint(100, 100),
        FloatPoint(100, 100),
        FloatPoint(100, 100),
        FloatPoint(100, 100),
    });

    checkIsEmpty(true, { // Two line segments.
        FloatPoint(0, 0),
        FloatPoint(100, 100),
        FloatPoint(0, 0),
        FloatPoint(50, -100),
    });

    checkIsEmpty(true, { // 4 distinct but colinear points.
        FloatPoint(-1, -1),
        FloatPoint(0, 0),
        FloatPoint(1, 1),
        FloatPoint(3, 3),
    });
}

TEST(FloatQuad, BoundingBox)
{
    FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13), FloatPoint(17, 19));
    FloatRect rect = quad.boundingBox();
    ASSERT_EQ(rect.x(), 2);
    ASSERT_EQ(rect.y(), 3);
    ASSERT_EQ(rect.width(), 17 - 2);
    ASSERT_EQ(rect.height(), 19 - 3);
}

TEST(FloatQuad, BoundingBoxSaturateInf)
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    FloatQuad quad(FloatPoint(-inf, 3), FloatPoint(5, inf), FloatPoint(11, 13), FloatPoint(17, 19));
    FloatRect rect = quad.boundingBox();
    ASSERT_EQ(rect.x(), std::numeric_limits<int>::min());
    ASSERT_EQ(rect.y(), 3.0f);
    ASSERT_EQ(rect.width(), 17.0f - std::numeric_limits<int>::min());
    ASSERT_EQ(rect.height(), static_cast<float>(std::numeric_limits<int>::max()) - 3.0f);
}

TEST(FloatQuad, BoundingBoxSaturateLarge)
{
    constexpr double large = std::numeric_limits<float>::max() * 4;
    FloatQuad quad(FloatPoint(-large, 3), FloatPoint(5, large), FloatPoint(11, 13), FloatPoint(17, 19));
    FloatRect rect = quad.boundingBox();
    ASSERT_EQ(rect.x(), std::numeric_limits<int>::min());
    ASSERT_EQ(rect.y(), 3.0f);
    ASSERT_EQ(rect.width(), 17.0f - std::numeric_limits<int>::min());
    ASSERT_EQ(rect.height(), static_cast<float>(std::numeric_limits<int>::max()) - 3.0f);
}

} // namespace TestWebKitAPI

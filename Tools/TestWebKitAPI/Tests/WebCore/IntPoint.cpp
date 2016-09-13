/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::IntPoint point)
{
    point.setX(1);
    EXPECT_EQ(1, point.x());
    point.setY(2);
    EXPECT_EQ(2, point.y());
}

TEST(IntPoint, DefaultConstruction)
{
    WebCore::IntPoint test;

    EXPECT_EQ(0, test.x());
    EXPECT_EQ(0, test.y());
    ASSERT_TRUE(test.isZero());

    testGetAndSet(test);
}

TEST(IntPoint, ValueConstruction)
{
    WebCore::IntPoint test(10, 9);

    EXPECT_EQ(10, test.x());
    EXPECT_EQ(9, test.y());

    testGetAndSet(test);
}

TEST(IntPoint, ZeroConstruction)
{
    WebCore::IntPoint test = WebCore::IntPoint::zero();

    EXPECT_EQ(0, test.x());
    EXPECT_EQ(0, test.y());
    ASSERT_TRUE(test.isZero());
}

TEST(IntPoint, IntSizeConstruction)
{
    WebCore::IntSize testInput(2003, 1997);
    WebCore::IntPoint test(testInput);

    EXPECT_EQ(2003, test.x());
    EXPECT_EQ(1997, test.y());
    ASSERT_FALSE(test.isZero());
}

TEST(IntPoint, FloatPointConstruction)
{
    WebCore::FloatPoint testInput(2003.2f, 1997.3f);
    WebCore::IntPoint test(testInput);

    EXPECT_EQ(2003, test.x());
    EXPECT_EQ(1997, test.y());
    ASSERT_FALSE(test.isZero());
}

TEST(IntPoint, Move)
{
    WebCore::IntPoint test(10, 20);
    WebCore::IntSize size(30, 50);

    test.move(size);

    EXPECT_EQ(40, test.x());
    EXPECT_EQ(70, test.y());

    test.move(-2, 8);

    EXPECT_EQ(38, test.x());
    EXPECT_EQ(78, test.y());

    WebCore::IntPoint offset(100, 100);

    test.moveBy(offset);

    EXPECT_EQ(138, test.x());
    EXPECT_EQ(178, test.y());
}

TEST(IntPoint, Scale)
{
    WebCore::IntPoint test(10, 20);

    test.scale(2.0);

    EXPECT_EQ(20, test.x());
    EXPECT_EQ(40, test.y());

    test.scale(3.0, 1.5);

    EXPECT_EQ(60, test.x());
    EXPECT_EQ(60, test.y());
}

TEST(IntPoint, Expand)
{
    WebCore::IntPoint a(10, 20);
    WebCore::IntPoint b(20, 40);

    auto c = a.expandedTo(b);

    EXPECT_EQ(20, c.x());
    EXPECT_EQ(40, c.y());
}

TEST(IntPoint, Shrink)
{
    WebCore::IntPoint a(10, 20);
    WebCore::IntPoint b(20, 40);

    auto c = b.shrunkTo(a);

    EXPECT_EQ(10, c.x());
    EXPECT_EQ(20, c.y());
}

TEST(IntPoint, Transpose)
{
    WebCore::IntPoint a(10, 20);

    auto b = a.transposedPoint();

    EXPECT_EQ(20, b.x());
    EXPECT_EQ(10, b.y());
}

TEST(IntPoint, Cast)
{
    WebCore::IntPoint a(10, 20);

    WebCore::IntSize as = WebCore::toIntSize(a);
    EXPECT_EQ(10, as.width());
    EXPECT_EQ(20, as.height());

#if USE(CG)
    CGPoint cgPoint = a;

    ASSERT_FLOAT_EQ(10.0f, cgPoint.x);
    ASSERT_FLOAT_EQ(20.0f, cgPoint.y);

    WebCore::IntPoint fromCGPoint(cgPoint);
    EXPECT_EQ(10, fromCGPoint.x());
    EXPECT_EQ(20, fromCGPoint.y());
    ASSERT_TRUE(a == fromCGPoint);
#endif

#if PLATFORM(WIN)
    POINT gdiPoint = a;

    ASSERT_FLOAT_EQ(10.0f, gdiPoint.x);
    ASSERT_FLOAT_EQ(20.0f, gdiPoint.y);

    WebCore::IntPoint fromGDIPoint(gdiPoint);
    EXPECT_EQ(10, fromGDIPoint.x());
    EXPECT_EQ(20, fromGDIPoint.y());
    ASSERT_TRUE(a == fromGDIPoint);

    D2D1_POINT_2F d2dPointF = a;

    ASSERT_FLOAT_EQ(10.0f, d2dPointF.x);
    ASSERT_FLOAT_EQ(20.0f, d2dPointF.y);

    WebCore::IntPoint fromD2DPointF(d2dPointF);
    EXPECT_EQ(10, fromD2DPointF.x());
    EXPECT_EQ(20, fromD2DPointF.y());
    ASSERT_TRUE(a == fromD2DPointF);

    D2D1_POINT_2U d2dPointU = a;

    ASSERT_FLOAT_EQ(10.0f, d2dPointU.x);
    ASSERT_FLOAT_EQ(20.0f, d2dPointU.y);

    WebCore::IntPoint fromD2DPointU(d2dPointU);
    EXPECT_EQ(10, fromD2DPointU.x());
    EXPECT_EQ(20, fromD2DPointU.y());
    ASSERT_TRUE(a == fromD2DPointU);
#endif
}

TEST(IntPoint, Addition)
{
    WebCore::IntPoint a(10, 20);
    WebCore::IntPoint b(50, 60);
    WebCore::IntSize bs(50, 60);

    auto c = a + b;

    EXPECT_EQ(60, c.x());
    EXPECT_EQ(80, c.y());

    a += bs;

    EXPECT_EQ(60, a.x());
    EXPECT_EQ(80, a.y());
}

TEST(IntPoint, Subtraction)
{
    WebCore::IntPoint a(100, 80);
    WebCore::IntPoint b(50, 60);
    WebCore::IntSize bs(50, 60);

    WebCore::IntSize c = a - b;

    EXPECT_EQ(50, c.width());
    EXPECT_EQ(20, c.height());

    WebCore::IntPoint d = a - bs;

    EXPECT_EQ(50, d.x());
    EXPECT_EQ(20, d.y());

    a -= bs;

    EXPECT_EQ(50, a.x());
    EXPECT_EQ(20, a.y());
}

TEST(IntPoint, Negation)
{
    WebCore::IntPoint a(100, 80);
    auto b = -a;

    EXPECT_EQ(-100, b.x());
    EXPECT_EQ(-80, b.y());
}

TEST(IntPoint, Equality)
{
    WebCore::IntPoint a(100, 80);
    WebCore::IntPoint b(70, 50);
    WebCore::IntPoint c(100, 80);

    ASSERT_TRUE(a == c);
    ASSERT_FALSE(a == b);
    ASSERT_FALSE(a != c);
    ASSERT_TRUE(a != b);
}

}

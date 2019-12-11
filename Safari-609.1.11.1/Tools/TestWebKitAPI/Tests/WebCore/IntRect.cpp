/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#include <WebCore/FloatRect.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::IntRect rect)
{
    rect.setX(1);
    EXPECT_EQ(1, rect.x());
    rect.setY(2);
    EXPECT_EQ(2, rect.y());
    rect.setWidth(203);
    EXPECT_EQ(203, rect.width());
    rect.setHeight(73);
    EXPECT_EQ(73, rect.height());
}

static void testEmptyRect(const WebCore::IntRect& rect)
{
    EXPECT_EQ(0, rect.x());
    EXPECT_EQ(0, rect.y());
    EXPECT_EQ(0, rect.width());
    EXPECT_EQ(0, rect.height());
    EXPECT_EQ(0, rect.maxX());
    EXPECT_EQ(0, rect.maxY());
    EXPECT_TRUE(rect.isEmpty());
}

TEST(IntRect, DefaultConstruction)
{
    WebCore::IntRect test;

    testEmptyRect(test);

    testGetAndSet(test);

    auto location = test.location();
    EXPECT_EQ(0, location.x());
    EXPECT_EQ(0, location.y());

    auto size = test.size();
    EXPECT_EQ(0, size.width());
    EXPECT_EQ(0, size.height());
}

TEST(IntRect, ValueConstruction)
{
    WebCore::IntRect test(10, 20, 100, 50);

    EXPECT_EQ(10, test.x());
    EXPECT_EQ(20, test.y());
    EXPECT_EQ(100, test.width());
    EXPECT_EQ(50, test.height());
    EXPECT_EQ(110, test.maxX());
    EXPECT_EQ(70, test.maxY());
    EXPECT_FALSE(test.isEmpty());

    auto location = test.location();
    EXPECT_EQ(10, location.x());
    EXPECT_EQ(20, location.y());

    auto size = test.size();
    EXPECT_EQ(100, size.width());
    EXPECT_EQ(50, size.height());
}

TEST(IntRect, PointSizeConstruction)
{
    WebCore::IntPoint location(20, 30);
    WebCore::IntSize size(100, 50);

    WebCore::IntRect test(location, size);

    EXPECT_EQ(20, test.x());
    EXPECT_EQ(30, test.y());
    EXPECT_EQ(100, test.width());
    EXPECT_EQ(50, test.height());
    EXPECT_EQ(120, test.maxX());
    EXPECT_EQ(80, test.maxY());
    EXPECT_FALSE(test.isEmpty());

    auto location2 = test.location();
    EXPECT_EQ(20, location2.x());
    EXPECT_EQ(30, location2.y());

    auto size2 = test.size();
    EXPECT_EQ(100, size2.width());
    EXPECT_EQ(50, size2.height());
}

TEST(IntRect, FloatRectConstruction)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 150.0f, 300.0f);

    WebCore::IntRect test(rect);

    EXPECT_EQ(20, test.x());
    EXPECT_EQ(30, test.y());
    EXPECT_EQ(150, test.width());
    EXPECT_EQ(300, test.height());
    EXPECT_EQ(170, test.maxX());
    EXPECT_EQ(330, test.maxY());
    EXPECT_FALSE(test.isEmpty());

    auto location = test.location();
    EXPECT_EQ(20, location.x());
    EXPECT_EQ(30, location.y());

    auto size = test.size();
    EXPECT_EQ(150, size.width());
    EXPECT_EQ(300, size.height());
}

TEST(IntRect, SetLocationAndSize)
{
    WebCore::IntRect rect;

    testEmptyRect(rect);

    WebCore::IntPoint location(10, 20);

    rect.setLocation(location);

    EXPECT_EQ(10, rect.x());
    EXPECT_EQ(20, rect.y());
    EXPECT_EQ(0, rect.width());
    EXPECT_EQ(0, rect.height());
    EXPECT_EQ(10, rect.maxX());
    EXPECT_EQ(20, rect.maxY());
    EXPECT_TRUE(rect.isEmpty());

    WebCore::IntSize size(100, 200);

    rect.setSize(size);

    EXPECT_EQ(10, rect.x());
    EXPECT_EQ(20, rect.y());
    EXPECT_EQ(100, rect.width());
    EXPECT_EQ(200, rect.height());
    EXPECT_EQ(110, rect.maxX());
    EXPECT_EQ(220, rect.maxY());
    EXPECT_FALSE(rect.isEmpty());
}

TEST(IntRect, Center)
{
    WebCore::IntRect rect(20, 40, 100, 200);

    auto center = rect.center();

    EXPECT_EQ(70, center.x());
    EXPECT_EQ(140, center.y());
}

TEST(IntRect, Move)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::IntSize delta(10, 20);

    rect.move(delta);

    EXPECT_EQ(30, rect.x());
    EXPECT_EQ(50, rect.y());

    WebCore::IntPoint deltaPoint(-20, -40);

    rect.moveBy(deltaPoint);

    EXPECT_EQ(10, rect.x());
    EXPECT_EQ(10, rect.y());

    rect.move(-10, 22);

    EXPECT_EQ(0, rect.x());
    EXPECT_EQ(32, rect.y());
}

TEST(IntRect, Expand)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::IntSize size(100, 100);

    rect.expand(size);

    EXPECT_EQ(200, rect.width());
    EXPECT_EQ(300, rect.height());

    rect.expand(55, 22);

    EXPECT_EQ(255, rect.width());
    EXPECT_EQ(322, rect.height());

    WebCore::IntSize size2(-10, -20);

    rect.expand(size2);

    EXPECT_EQ(245, rect.width());
    EXPECT_EQ(302, rect.height());

    rect.expand(-5, -2);

    EXPECT_EQ(240, rect.width());
    EXPECT_EQ(300, rect.height());
}

TEST(IntRect, Contract)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::IntSize size(50, 100);

    rect.contract(size);

    EXPECT_EQ(50, rect.width());
    EXPECT_EQ(100, rect.height());

    rect.contract(25, 22);

    EXPECT_EQ(25, rect.width());
    EXPECT_EQ(78, rect.height());

    WebCore::IntSize size2(-10, -20);

    rect.contract(size2);

    EXPECT_EQ(35, rect.width());
    EXPECT_EQ(98, rect.height());

    rect.contract(-5, -2);

    EXPECT_EQ(40, rect.width());
    EXPECT_EQ(100, rect.height());
}

TEST(IntRect, ShiftXEdge)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    rect.shiftXEdgeTo(77);

    EXPECT_EQ(77, rect.x());
    EXPECT_EQ(120, rect.maxX());
    EXPECT_EQ(30, rect.y());
    EXPECT_EQ(230, rect.maxY());
    EXPECT_EQ(43, rect.width());
    EXPECT_EQ(200, rect.height());

    rect.shiftMaxXEdgeTo(200);

    EXPECT_EQ(77, rect.x());
    EXPECT_EQ(200, rect.maxX());
    EXPECT_EQ(30, rect.y());
    EXPECT_EQ(230, rect.maxY());
    EXPECT_EQ(123, rect.width());
    EXPECT_EQ(200, rect.height());
}

TEST(IntRect, ShiftYEdge)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    rect.shiftYEdgeTo(59.0f);

    EXPECT_EQ(20, rect.x());
    EXPECT_EQ(120, rect.maxX());
    EXPECT_EQ(59, rect.y());
    EXPECT_EQ(230, rect.maxY());
    EXPECT_EQ(100, rect.width());
    EXPECT_EQ(171, rect.height());

    rect.shiftMaxYEdgeTo(270.0f);

    EXPECT_EQ(20, rect.x());
    EXPECT_EQ(120, rect.maxX());
    EXPECT_EQ(59, rect.y());
    EXPECT_EQ(270, rect.maxY());
    EXPECT_EQ(100, rect.width());
    EXPECT_EQ(211, rect.height());
}

TEST(IntRect, Inflate)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    rect.inflateX(5);

    EXPECT_EQ(15, rect.x());
    EXPECT_EQ(125, rect.maxX());

    rect.inflateY(4);

    EXPECT_EQ(26, rect.y());
    EXPECT_EQ(234, rect.maxY());

    rect.inflate(10);

    EXPECT_EQ(5, rect.x());
    EXPECT_EQ(135, rect.maxX());
    EXPECT_EQ(16, rect.y());
    EXPECT_EQ(244, rect.maxY());
}

TEST(IntRect, Corners)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::FloatPoint topLeft = rect.minXMinYCorner();
    EXPECT_EQ(20, topLeft.x());
    EXPECT_EQ(30, topLeft.y());

    WebCore::FloatPoint topRight = rect.maxXMinYCorner();
    EXPECT_EQ(120, topRight.x());
    EXPECT_EQ(30, topRight.y());

    WebCore::FloatPoint bottomLeft = rect.minXMaxYCorner();
    EXPECT_EQ(20, bottomLeft.x());
    EXPECT_EQ(230, bottomLeft.y());

    WebCore::FloatPoint bottomRight = rect.maxXMaxYCorner();
    EXPECT_EQ(120, bottomRight.x());
    EXPECT_EQ(230, bottomRight.y());
}

TEST(IntRect, Contains)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::IntRect contained(30, 40, 50, 100);

    ASSERT_TRUE(rect.contains(contained));

    WebCore::IntRect outside(120, 230, 50, 100);

    ASSERT_FALSE(rect.contains(outside));

    WebCore::IntRect intersects(10, 20, 90, 180);

    ASSERT_FALSE(rect.contains(intersects));

    WebCore::IntPoint pointInside(60, 70);

    ASSERT_TRUE(rect.contains(pointInside));

    WebCore::IntPoint pointOutside(160, 270);

    ASSERT_FALSE(rect.contains(pointOutside));

    WebCore::IntPoint pointOnLine(20, 30);

    ASSERT_TRUE(rect.contains(pointOnLine));

    ASSERT_TRUE(rect.contains(60, 70));
    ASSERT_FALSE(rect.contains(160, 270));
}

TEST(IntRect, Intersects)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    WebCore::IntRect contained(30, 40, 50, 100);

    ASSERT_TRUE(rect.intersects(contained));

    WebCore::IntRect outside(120, 230, 50, 100);

    ASSERT_FALSE(rect.intersects(outside));

    WebCore::IntRect intersects(10, 20, 90, 180);

    ASSERT_TRUE(rect.intersects(intersects));
}

static void testIntersectResult(const WebCore::IntRect& rect)
{
    EXPECT_EQ(70, rect.x());
    EXPECT_EQ(120, rect.maxX());
    EXPECT_EQ(80, rect.y());
    EXPECT_EQ(230, rect.maxY());
}

TEST(IntRect, Intersect)
{
    WebCore::IntRect rectA(20, 30, 100, 200);
    WebCore::IntRect rectB(70, 80, 100, 200);

    rectA.intersect(rectB);

    testIntersectResult(rectA);

    WebCore::IntRect rectC(20, 30, 100, 200);

    auto intersected = WebCore::intersection(rectC, rectB);

    testIntersectResult(intersected);
}

static void testUnitedRects(const WebCore::IntRect& united)
{
    EXPECT_EQ(20, united.x());
    EXPECT_EQ(170, united.maxX());
    EXPECT_EQ(30, united.y());
    EXPECT_EQ(280, united.maxY());
}

TEST(IntRect, Unite)
{
    WebCore::IntRect rectA(20, 30, 100, 200);
    WebCore::IntRect rectB(70, 80, 100, 200);

    rectA.unite(rectB);

    testUnitedRects(rectA);

    WebCore::IntRect rectC(20, 30, 100, 200);

    auto united = WebCore::unionRect(rectC, rectB);

    testUnitedRects(united);
}

TEST(IntRect, Scale)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    rect.scale(2.0f);

    EXPECT_EQ(40, rect.x());
    EXPECT_EQ(240, rect.maxX());
    EXPECT_EQ(60, rect.y());
    EXPECT_EQ(460, rect.maxY());
}

TEST(IntRect, Transpose)
{
    WebCore::IntRect rect(20, 30, 100, 200);

    auto transposed = rect.transposedRect();

    EXPECT_EQ(30, transposed.x());
    EXPECT_EQ(230, transposed.maxX());
    EXPECT_EQ(20, transposed.y());
    EXPECT_EQ(120, transposed.maxY());
}

#if USE(CG) || PLATFORM(WIN)
static void checkCastRect(const WebCore::IntRect& rect)
{
    EXPECT_EQ(10, rect.x());
    EXPECT_EQ(40, rect.maxX());
    EXPECT_EQ(20, rect.y());
    EXPECT_EQ(60, rect.maxY());
    EXPECT_EQ(30, rect.width());
    EXPECT_EQ(40, rect.height());
}
#endif

TEST(IntRect, Casting)
{
    WebCore::IntRect rect(10, 20, 30, 40);

#if USE(CG)
    CGRect cgRect = CGRectMake(10.0, 20.0, 30.0, 40.0);

    WebCore::IntRect rectFromCGRect(cgRect);

    checkCastRect(rectFromCGRect);
#endif

#if PLATFORM(WIN)
    RECT gdiRect = rect;

    EXPECT_EQ(10, gdiRect.left);
    EXPECT_EQ(20, gdiRect.top);
    EXPECT_EQ(40, gdiRect.right);
    EXPECT_EQ(60, gdiRect.bottom);

    WebCore::IntRect rectFromGDIRect(gdiRect);

    checkCastRect(rectFromGDIRect);

    D2D1_RECT_U d2dRectU = rect;

    EXPECT_EQ(10, d2dRectU.left);
    EXPECT_EQ(20, d2dRectU.top);
    EXPECT_EQ(40, d2dRectU.right);
    EXPECT_EQ(60, d2dRectU.bottom);

    WebCore::IntRect rectFromD2DRectU(d2dRectU);

    checkCastRect(rectFromD2DRectU);

    D2D1_RECT_F d2dRectF = rect;

    EXPECT_FLOAT_EQ(10.0f, d2dRectF.left);
    EXPECT_FLOAT_EQ(20.0f, d2dRectF.top);
    EXPECT_FLOAT_EQ(40.0f, d2dRectF.right);
    EXPECT_FLOAT_EQ(60.0f, d2dRectF.bottom);

    WebCore::IntRect rectFromD2DRectF(d2dRectF);

    checkCastRect(rectFromD2DRectF);
#endif
}

static void checkSubtractionResult1(const WebCore::IntRect& rect)
{
    EXPECT_EQ(-10, rect.x());
    EXPECT_EQ(90, rect.maxX());
    EXPECT_EQ(-10, rect.y());
    EXPECT_EQ(90, rect.maxY());
    EXPECT_EQ(100, rect.width());
    EXPECT_EQ(100, rect.height());
}

static void checkSubtractionResult2(const WebCore::IntRect& rect)
{
    EXPECT_EQ(-40, rect.x());
    EXPECT_EQ(60, rect.maxX());
    EXPECT_EQ(-50, rect.y());
    EXPECT_EQ(50, rect.maxY());
    EXPECT_EQ(100, rect.width());
    EXPECT_EQ(100, rect.height());
}

TEST(IntRect, Subtraction)
{
    WebCore::IntRect rect(10, 20, 100, 100);
    WebCore::IntPoint rightSide(20, 30);

    rect -= rightSide;

    checkSubtractionResult1(rect);

    auto rect2 = rect - WebCore::IntPoint(30, 40);
    checkSubtractionResult2(rect2);
}

TEST(IntRect, Equality)
{
    WebCore::IntRect rect(10, 20, 100, 100);
    WebCore::IntRect rect2(10, 20, 100, 100);
    WebCore::IntRect rightSide(110, 20, 20, 100);

    ASSERT_TRUE(rect == rect2);
    ASSERT_FALSE(rect != rect2);
    ASSERT_TRUE(rect != rightSide);
    ASSERT_FALSE(rect == rightSide);
}

#if USE(CG)
static void checkEnclosingIntRect(const WebCore::IntRect& rect)
{
    EXPECT_EQ(10, rect.x());
    EXPECT_EQ(41, rect.maxX());
    EXPECT_EQ(21, rect.y());
    EXPECT_EQ(62, rect.maxY());
    EXPECT_EQ(31, rect.width());
    EXPECT_EQ(41, rect.height());
}
#endif

TEST(IntRect, EnclosingIntRect)
{
#if USE(CG)
    CGRect cgRect = CGRectMake(10.5, 21.3, 30.1, 40.0);

    WebCore::IntRect enclosingCG = WebCore::enclosingIntRect(cgRect);

    checkEnclosingIntRect(enclosingCG);
#endif
}

TEST(IntRect, AreaAndDistances)
{
    WebCore::IntRect rect(10, 20, 100, 100);

    EXPECT_EQ(10000U, rect.area().unsafeGet());
}

}

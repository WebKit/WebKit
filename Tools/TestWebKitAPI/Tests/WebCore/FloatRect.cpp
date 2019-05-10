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
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntRect.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if PLATFORM(WIN)
#include <d2d1.h>
#endif

namespace TestWebKitAPI {

static void testGetAndSet(WebCore::FloatRect rect)
{
    rect.setX(1.1f);
    EXPECT_FLOAT_EQ(1.1f, rect.x());
    rect.setY(2.2f);
    EXPECT_FLOAT_EQ(2.2f, rect.y());
    rect.setWidth(203.2f);
    EXPECT_FLOAT_EQ(203.2f, rect.width());
    rect.setHeight(72.9f);
    EXPECT_FLOAT_EQ(72.9f, rect.height());
}

static void testEmptyRect(const WebCore::FloatRect& rect)
{
    EXPECT_FLOAT_EQ(0, rect.x());
    EXPECT_FLOAT_EQ(0, rect.y());
    EXPECT_FLOAT_EQ(0, rect.width());
    EXPECT_FLOAT_EQ(0, rect.height());
    EXPECT_FLOAT_EQ(0, rect.maxX());
    EXPECT_FLOAT_EQ(0, rect.maxY());
    EXPECT_TRUE(rect.isEmpty());
    EXPECT_TRUE(rect.isZero());
    EXPECT_FALSE(rect.isInfinite());
}

TEST(FloatRect, DefaultConstruction)
{
    WebCore::FloatRect test;

    testEmptyRect(test);

    testGetAndSet(test);

    auto location = test.location();
    EXPECT_FLOAT_EQ(0, location.x());
    EXPECT_FLOAT_EQ(0, location.y());

    auto size = test.size();
    EXPECT_FLOAT_EQ(0, size.width());
    EXPECT_FLOAT_EQ(0, size.height());
}

TEST(FloatRect, ValueConstruction)
{
    WebCore::FloatRect test(10.0f, 20.0f, 100.0f, 50.0f);

    EXPECT_FLOAT_EQ(10.0f, test.x());
    EXPECT_FLOAT_EQ(20.0f, test.y());
    EXPECT_FLOAT_EQ(100.0f, test.width());
    EXPECT_FLOAT_EQ(50.0f, test.height());
    EXPECT_FLOAT_EQ(110.0f, test.maxX());
    EXPECT_FLOAT_EQ(70.0f, test.maxY());
    EXPECT_FALSE(test.isEmpty());
    EXPECT_FALSE(test.isZero());
    EXPECT_FALSE(test.isInfinite());

    auto location = test.location();
    EXPECT_FLOAT_EQ(10.0f, location.x());
    EXPECT_FLOAT_EQ(20.0f, location.y());

    auto size = test.size();
    EXPECT_FLOAT_EQ(100.0f, size.width());
    EXPECT_FLOAT_EQ(50.0f, size.height());
}

TEST(FloatRect, PointSizeConstruction)
{
    WebCore::FloatPoint location(20.0f, 30.0f);
    WebCore::FloatSize size(100.0f, 50.0f);

    WebCore::FloatRect test(location, size);

    EXPECT_FLOAT_EQ(20.0f, test.x());
    EXPECT_FLOAT_EQ(30.0f, test.y());
    EXPECT_FLOAT_EQ(100.0f, test.width());
    EXPECT_FLOAT_EQ(50.0f, test.height());
    EXPECT_FLOAT_EQ(120.0f, test.maxX());
    EXPECT_FLOAT_EQ(80.0f, test.maxY());
    EXPECT_FALSE(test.isEmpty());
    EXPECT_FALSE(test.isZero());
    EXPECT_FALSE(test.isInfinite());

    auto location2 = test.location();
    EXPECT_FLOAT_EQ(20.0f, location2.x());
    EXPECT_FLOAT_EQ(30.0f, location2.y());

    auto size2 = test.size();
    EXPECT_FLOAT_EQ(100.0f, size2.width());
    EXPECT_FLOAT_EQ(50.0f, size2.height());
}

TEST(FloatRect, TwoPointConstruction)
{
    WebCore::FloatPoint point1(20.0f, 30.0f);
    WebCore::FloatPoint point2(150.0f, 250.0f);

    WebCore::FloatRect test(point1, point2);

    EXPECT_FLOAT_EQ(20.0f, test.x());
    EXPECT_FLOAT_EQ(30.0f, test.y());
    EXPECT_FLOAT_EQ(130.0f, test.width());
    EXPECT_FLOAT_EQ(220.0f, test.height());
    EXPECT_FLOAT_EQ(150.0f, test.maxX());
    EXPECT_FLOAT_EQ(250.0f, test.maxY());
    EXPECT_FALSE(test.isEmpty());
    EXPECT_FALSE(test.isZero());
    EXPECT_FALSE(test.isInfinite());

    auto location = test.location();
    EXPECT_FLOAT_EQ(20.0f, location.x());
    EXPECT_FLOAT_EQ(30.0f, location.y());

    auto size = test.size();
    EXPECT_FLOAT_EQ(130.0f, size.width());
    EXPECT_FLOAT_EQ(220.0f, size.height());
}

TEST(FloatRect, IntRectConstruction)
{
    WebCore::IntRect rect(20, 30, 150, 300);

    WebCore::FloatRect test(rect);

    EXPECT_FLOAT_EQ(20.0f, test.x());
    EXPECT_FLOAT_EQ(30.0f, test.y());
    EXPECT_FLOAT_EQ(150.0f, test.width());
    EXPECT_FLOAT_EQ(300.0f, test.height());
    EXPECT_FLOAT_EQ(170.0f, test.maxX());
    EXPECT_FLOAT_EQ(330.0f, test.maxY());
    EXPECT_FALSE(test.isEmpty());
    EXPECT_FALSE(test.isZero());
    EXPECT_FALSE(test.isInfinite());

    auto location = test.location();
    EXPECT_FLOAT_EQ(20.0f, location.x());
    EXPECT_FLOAT_EQ(30.0f, location.y());

    auto size = test.size();
    EXPECT_FLOAT_EQ(150.0f, size.width());
    EXPECT_FLOAT_EQ(300.0f, size.height());
}

TEST(FloatRect, SetLocationAndSize)
{
    WebCore::FloatRect rect;

    testEmptyRect(rect);

    WebCore::FloatPoint location(10.0f, 20.0f);

    rect.setLocation(location);

    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(0.0f, rect.width());
    EXPECT_FLOAT_EQ(0.0f, rect.height());
    EXPECT_FLOAT_EQ(10.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.maxY());
    EXPECT_TRUE(rect.isEmpty());
    EXPECT_TRUE(rect.isZero());
    EXPECT_FALSE(rect.isInfinite());

    WebCore::FloatSize size(100.0f, 200.0f);

    rect.setSize(size);

    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(100.0f, rect.width());
    EXPECT_FLOAT_EQ(200.0f, rect.height());
    EXPECT_FLOAT_EQ(110.0f, rect.maxX());
    EXPECT_FLOAT_EQ(220.0f, rect.maxY());
    EXPECT_FALSE(rect.isEmpty());
    EXPECT_FALSE(rect.isZero());
    EXPECT_FALSE(rect.isInfinite());
}

TEST(FloatRect, Center)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    auto center = rect.center();

    EXPECT_FLOAT_EQ(70.0f, center.x());
    EXPECT_FLOAT_EQ(130.0f, center.y());
}

TEST(FloatRect, Move)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatSize delta(10.0f, 20.0f);

    rect.move(delta);

    EXPECT_FLOAT_EQ(30.0f, rect.x());
    EXPECT_FLOAT_EQ(50.0f, rect.y());

    WebCore::FloatPoint deltaPoint(-20.0f, -40.0f);

    rect.moveBy(deltaPoint);

    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(10.0f, rect.y());

    rect.move(-10.0f, 22.0f);

    EXPECT_FLOAT_EQ(0.0f, rect.x());
    EXPECT_FLOAT_EQ(32.0f, rect.y());
}

TEST(FloatRect, Expand)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatSize size(100.0f, 100.0f);

    rect.expand(size);

    EXPECT_FLOAT_EQ(200.0f, rect.width());
    EXPECT_FLOAT_EQ(300.0f, rect.height());

    rect.expand(55.0f, 22.0f);

    EXPECT_FLOAT_EQ(255.0f, rect.width());
    EXPECT_FLOAT_EQ(322.0f, rect.height());

    WebCore::FloatSize size2(-10.0f, -20.0f);

    rect.expand(size2);

    EXPECT_FLOAT_EQ(245.0f, rect.width());
    EXPECT_FLOAT_EQ(302.0f, rect.height());

    rect.expand(-5.0f, -2.0f);

    EXPECT_FLOAT_EQ(240.0f, rect.width());
    EXPECT_FLOAT_EQ(300.0f, rect.height());

    EXPECT_FALSE(rect.isInfinite());
}

TEST(FloatRect, Contract)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatSize size(50.0f, 100.0f);

    rect.contract(size);

    EXPECT_FLOAT_EQ(50.0f, rect.width());
    EXPECT_FLOAT_EQ(100.0f, rect.height());

    rect.contract(25.0f, 22.0f);

    EXPECT_FLOAT_EQ(25.0f, rect.width());
    EXPECT_FLOAT_EQ(78.0f, rect.height());

    WebCore::FloatSize size2(-10.0f, -20.0f);

    rect.contract(size2);

    EXPECT_FLOAT_EQ(35.0f, rect.width());
    EXPECT_FLOAT_EQ(98.0f, rect.height());

    rect.contract(-5.0f, -2.0f);

    EXPECT_FLOAT_EQ(40.0f, rect.width());
    EXPECT_FLOAT_EQ(100.0f, rect.height());

    EXPECT_FALSE(rect.isInfinite());
}

TEST(FloatRect, ShiftXEdge)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    rect.shiftXEdgeTo(77.0f);

    EXPECT_FLOAT_EQ(77.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(30.0f, rect.y());
    EXPECT_FLOAT_EQ(230.0f, rect.maxY());
    EXPECT_FLOAT_EQ(43.0f, rect.width());
    EXPECT_FLOAT_EQ(200.0f, rect.height());

    rect.shiftMaxXEdgeTo(200.0f);

    EXPECT_FLOAT_EQ(77.0f, rect.x());
    EXPECT_FLOAT_EQ(200.0f, rect.maxX());
    EXPECT_FLOAT_EQ(30.0f, rect.y());
    EXPECT_FLOAT_EQ(230.0f, rect.maxY());
    EXPECT_FLOAT_EQ(123.0f, rect.width());
    EXPECT_FLOAT_EQ(200.0f, rect.height());
}

TEST(FloatRect, ShiftYEdge)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    rect.shiftYEdgeTo(59.0f);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(59.0f, rect.y());
    EXPECT_FLOAT_EQ(230.0f, rect.maxY());
    EXPECT_FLOAT_EQ(100.0f, rect.width());
    EXPECT_FLOAT_EQ(171.0f, rect.height());

    rect.shiftMaxYEdgeTo(270.0f);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(59.0f, rect.y());
    EXPECT_FLOAT_EQ(270.0f, rect.maxY());
    EXPECT_FLOAT_EQ(100.0f, rect.width());
    EXPECT_FLOAT_EQ(211.0f, rect.height());
}

TEST(FloatRect, Inflate)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    rect.inflateX(5.0f);

    EXPECT_FLOAT_EQ(15.0f, rect.x());
    EXPECT_FLOAT_EQ(125.0f, rect.maxX());

    rect.inflateY(4.0f);

    EXPECT_FLOAT_EQ(26.0f, rect.y());
    EXPECT_FLOAT_EQ(234.0f, rect.maxY());

    rect.inflate(10.0f);

    EXPECT_FLOAT_EQ(5.0f, rect.x());
    EXPECT_FLOAT_EQ(135.0f, rect.maxX());
    EXPECT_FLOAT_EQ(16.0f, rect.y());
    EXPECT_FLOAT_EQ(244.0f, rect.maxY());

    EXPECT_FALSE(rect.isInfinite());
}

TEST(FloatRect, Corners)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatPoint topLeft = rect.minXMinYCorner();
    EXPECT_FLOAT_EQ(20.0f, topLeft.x());
    EXPECT_FLOAT_EQ(30.0f, topLeft.y());

    WebCore::FloatPoint topRight = rect.maxXMinYCorner();
    EXPECT_FLOAT_EQ(120.0f, topRight.x());
    EXPECT_FLOAT_EQ(30.0f, topRight.y());

    WebCore::FloatPoint bottomLeft = rect.minXMaxYCorner();
    EXPECT_FLOAT_EQ(20.0f, bottomLeft.x());
    EXPECT_FLOAT_EQ(230.0f, bottomLeft.y());

    WebCore::FloatPoint bottomRight = rect.maxXMaxYCorner();
    EXPECT_FLOAT_EQ(120.0f, bottomRight.x());
    EXPECT_FLOAT_EQ(230.0f, bottomRight.y());
}

TEST(FloatRect, Contains)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatRect contained(30.0f, 40.0f, 50.0f, 100.0f);

    ASSERT_TRUE(rect.contains(contained));

    WebCore::FloatRect outside(120.0f, 230.0f, 50.0f, 100.0f);

    ASSERT_FALSE(rect.contains(outside));

    WebCore::FloatRect intersects(10.0f, 20.0f, 90.0f, 180.0f);

    ASSERT_FALSE(rect.contains(intersects));

    WebCore::FloatPoint pointInside(60.0f, 70.0f);

    ASSERT_TRUE(rect.contains(pointInside));
    ASSERT_TRUE(rect.contains(pointInside, WebCore::FloatRect::InsideButNotOnStroke));

    WebCore::FloatPoint pointOutside(160.0f, 270.0f);

    ASSERT_FALSE(rect.contains(pointOutside));
    ASSERT_FALSE(rect.contains(pointOutside, WebCore::FloatRect::InsideButNotOnStroke));

    WebCore::FloatPoint pointOnLine(20.0f, 30.0f);

    ASSERT_TRUE(rect.contains(pointOnLine));
    ASSERT_FALSE(rect.contains(pointOutside, WebCore::FloatRect::InsideButNotOnStroke));

    ASSERT_TRUE(rect.contains(60.0f, 70.0f));
    ASSERT_FALSE(rect.contains(160.0f, 270.0f));
}

TEST(FloatRect, Intersects)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    WebCore::FloatRect contained(30.0f, 40.0f, 50.0f, 100.0f);

    ASSERT_TRUE(rect.intersects(contained));

    WebCore::FloatRect outside(120.0f, 230.0f, 50.0f, 100.0f);

    ASSERT_FALSE(rect.intersects(outside));

    WebCore::FloatRect intersects(10.0f, 20.0f, 90.0f, 180.0f);

    ASSERT_TRUE(rect.intersects(intersects));
}

static void testIntersectResult(const WebCore::FloatRect& rect)
{
    EXPECT_FLOAT_EQ(70.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(80.0f, rect.y());
    EXPECT_FLOAT_EQ(230.0f, rect.maxY());
}

TEST(FloatRect, Intersect)
{
    WebCore::FloatRect rectA(20.0f, 30.0f, 100.0f, 200.0f);
    WebCore::FloatRect rectB(70.0f, 80.0f, 100.0f, 200.0f);

    rectA.intersect(rectB);

    testIntersectResult(rectA);

    WebCore::FloatRect rectC(20.0f, 30.0f, 100.0f, 200.0f);

    auto intersected = WebCore::intersection(rectC, rectB);

    testIntersectResult(intersected);
}

static void testUnitedRects(const WebCore::FloatRect& united)
{
    EXPECT_FLOAT_EQ(20.0f, united.x());
    EXPECT_FLOAT_EQ(170.0f, united.maxX());
    EXPECT_FLOAT_EQ(30.0f, united.y());
    EXPECT_FLOAT_EQ(280.0f, united.maxY());
}

TEST(FloatRect, Unite)
{
    WebCore::FloatRect rectA(20.0f, 30.0f, 100.0f, 200.0f);
    WebCore::FloatRect rectB(70.0f, 80.0f, 100.0f, 200.0f);

    rectA.unite(rectB);

    testUnitedRects(rectA);

    WebCore::FloatRect rectC(20.0f, 30.0f, 100.0f, 200.0f);

    auto united = WebCore::unionRect(rectC, rectB);

    testUnitedRects(united);
}

TEST(FloatRect, Extend)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);
    WebCore::FloatPoint point(170.0f, 280.0f);

    rect.extend(point);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(170.0f, rect.maxX());
    EXPECT_FLOAT_EQ(30.0f, rect.y());
    EXPECT_FLOAT_EQ(280.0f, rect.maxY());
}

TEST(FloatRect, Overlaps)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    ASSERT_FALSE(rect.overlapsXRange(0.0f, 10.0f));
    ASSERT_TRUE(rect.overlapsXRange(10.0f, 30.0f));
    ASSERT_TRUE(rect.overlapsXRange(40.0f, 70.0f));
    ASSERT_TRUE(rect.overlapsXRange(110.0f, 130.0f));
    ASSERT_FALSE(rect.overlapsXRange(140.0f, 600.0f));

    ASSERT_FALSE(rect.overlapsYRange(0.0f, 10.0f));
    ASSERT_TRUE(rect.overlapsYRange(10.0f, 40.0f));
    ASSERT_TRUE(rect.overlapsYRange(40.0f, 70.0f));
    ASSERT_TRUE(rect.overlapsYRange(220.0f, 250.0f));
    ASSERT_FALSE(rect.overlapsYRange(250.0f, 600.0f));
}

TEST(FloatRect, Scale)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    rect.scale(2.0f);

    EXPECT_FLOAT_EQ(40.0f, rect.x());
    EXPECT_FLOAT_EQ(240.0f, rect.maxX());
    EXPECT_FLOAT_EQ(60.0f, rect.y());
    EXPECT_FLOAT_EQ(460.0f, rect.maxY());

    rect.scale(0.5f, 2.0f);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(120.0f, rect.y());
    EXPECT_FLOAT_EQ(920.0f, rect.maxY());

    rect.scale(1.0f, 0.25f);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(120.0f, rect.maxX());
    EXPECT_FLOAT_EQ(30.0f, rect.y());
    EXPECT_FLOAT_EQ(230.0f, rect.maxY());
}

TEST(FloatRect, Transpose)
{
    WebCore::FloatRect rect(20.0f, 30.0f, 100.0f, 200.0f);

    auto transposed = rect.transposedRect();

    EXPECT_FLOAT_EQ(30.0f, transposed.x());
    EXPECT_FLOAT_EQ(230.0f, transposed.maxX());
    EXPECT_FLOAT_EQ(20.0f, transposed.y());
    EXPECT_FLOAT_EQ(120.0f, transposed.maxY());
}

TEST(FloatRect, FitToPoints)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 30.0f, 40.0f);

    WebCore::FloatPoint p0(20.0f, 30.0f);
    WebCore::FloatPoint p1(70.0f, 130.0f);
    WebCore::FloatPoint p2(50.0f, 20.0f);
    WebCore::FloatPoint p3(90.0f, 190.0f);

    rect.fitToPoints(p0, p1);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(70.0f, rect.maxX());
    EXPECT_FLOAT_EQ(30.0f, rect.y());
    EXPECT_FLOAT_EQ(130.0f, rect.maxY());

    rect.fitToPoints(p0, p1, p2);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(70.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(130.0f, rect.maxY());

    rect.fitToPoints(p0, p1, p2, p3);

    EXPECT_FLOAT_EQ(20.0f, rect.x());
    EXPECT_FLOAT_EQ(90.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(190.0f, rect.maxY());
}

#if USE(CG) || PLATFORM(WIN)
static void checkCastRect(const WebCore::FloatRect& rect)
{
    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(40.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(60.0f, rect.maxY());
    EXPECT_FLOAT_EQ(30.0f, rect.width());
    EXPECT_FLOAT_EQ(40.0f, rect.height());
}
#endif

TEST(FloatRect, Casting)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 30.0f, 40.0f);

#if USE(CG)
    CGRect cgRect = rect;

    EXPECT_FLOAT_EQ(10.0f, cgRect.origin.x);
    EXPECT_FLOAT_EQ(20.0f, cgRect.origin.y);
    EXPECT_FLOAT_EQ(30.0f, cgRect.size.width);
    EXPECT_FLOAT_EQ(40.0f, cgRect.size.height);

    WebCore::FloatRect rectFromCGRect(cgRect);

    checkCastRect(rectFromCGRect);
#endif

#if PLATFORM(WIN)
    D2D1_RECT_F d2dRect = rect;

    EXPECT_FLOAT_EQ(10.0f, d2dRect.left);
    EXPECT_FLOAT_EQ(20.0f, d2dRect.top);
    EXPECT_FLOAT_EQ(40.0f, d2dRect.right);
    EXPECT_FLOAT_EQ(60.0f, d2dRect.bottom);

    WebCore::FloatRect rectFromD2DRect(d2dRect);

    checkCastRect(rectFromD2DRect);
#endif
}

static void checkAdditionResult1(const WebCore::FloatRect& rect)
{
    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(130.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(220.0f, rect.maxY());
    EXPECT_FLOAT_EQ(120.0f, rect.width());
    EXPECT_FLOAT_EQ(200.0f, rect.height());
}

static void checkAdditionResult2(const WebCore::FloatRect& rect)
{
    EXPECT_FLOAT_EQ(10.0f, rect.x());
    EXPECT_FLOAT_EQ(250.0f, rect.maxX());
    EXPECT_FLOAT_EQ(20.0f, rect.y());
    EXPECT_FLOAT_EQ(240.0f, rect.maxY());
    EXPECT_FLOAT_EQ(240.0f, rect.width());
    EXPECT_FLOAT_EQ(220.0f, rect.height());
}

TEST(FloatRect, Addition)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 100.0f, 100.0f);
    WebCore::FloatRect rightSide(0.0f, 0.0f, 20.0f, 100.0f);
    WebCore::FloatRect bottom(0.0f, 0.0f, 120.0f, 20.0f);

    auto combined = rect + rightSide;

    checkAdditionResult1(combined);

    auto combined2 = combined + bottom;

    checkAdditionResult2(combined2);

    rect += rightSide;
    rect += bottom;

    checkAdditionResult2(rect);
}

TEST(FloatRect, Equality)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 100.0f, 100.0f);
    WebCore::FloatRect rect2(10.0f, 20.0f, 100.0f, 100.0f);
    WebCore::FloatRect rightSide(110.0f, 20.0f, 20.0f, 100.0f);

    ASSERT_TRUE(rect == rect2);
    ASSERT_FALSE(rect != rect2);
    ASSERT_TRUE(rect != rightSide);
    ASSERT_FALSE(rect == rightSide);
}

TEST(FloatRect, InfiniteRect)
{
    WebCore::FloatRect infinite = WebCore::FloatRect::infiniteRect();

    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max() / 2, infinite.x());
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max() / 2, infinite.y());
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), infinite.width());
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), infinite.height());

    // FIXME: We have an unusual representation for our infinite rect.
    // WebCore::Float::infiniteRect is (negative infinity)/2 for the upper left corner,
    // while CoreGraphics and D2D use (negative infinity).

#if USE(CG)
    CGRect cgInfiniteRect = CGRectInfinite;

#if PLATFORM(WIN)
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max() / 2, cgInfiniteRect.origin.x);
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max() / 2, cgInfiniteRect.origin.y);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), cgInfiniteRect.size.width);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), cgInfiniteRect.size.height);
#else
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max(), cgInfiniteRect.origin.x);
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max(), cgInfiniteRect.origin.y);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), cgInfiniteRect.origin.x + cgInfiniteRect.size.width);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), cgInfiniteRect.origin.y + cgInfiniteRect.size.height);
#endif
    // ASSERT_TRUE(infinite == cgInfiniteRect);
#endif

#if PLATFORM(WIN)
    D2D1_RECT_F d2dInfiniteRect = D2D1::InfiniteRect();

    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max(), d2dInfiniteRect.left);
    EXPECT_FLOAT_EQ(-std::numeric_limits<float>::max(), d2dInfiniteRect.top);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), d2dInfiniteRect.right);
    EXPECT_FLOAT_EQ(std::numeric_limits<float>::max(), d2dInfiniteRect.bottom);
    // ASSERT_TRUE(infinite == d2dInfiniteRect);
#endif
}

TEST(FloatRect, EnclosingAndRounding)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 1024.3f, 768.3f);

    auto enclosed = WebCore::encloseRectToDevicePixels(rect, 1.0f);

    EXPECT_FLOAT_EQ(10.0f, enclosed.x());
    EXPECT_FLOAT_EQ(20.0f, enclosed.y());
    EXPECT_FLOAT_EQ(1035.0f, enclosed.maxX());
    EXPECT_FLOAT_EQ(789.0f, enclosed.maxY());

    auto enclosed2 = WebCore::encloseRectToDevicePixels(rect, 2.0f);

    EXPECT_FLOAT_EQ(10.0f, enclosed2.x());
    EXPECT_FLOAT_EQ(20.0f, enclosed2.y());
    EXPECT_FLOAT_EQ(1034.5f, enclosed2.maxX());
    EXPECT_FLOAT_EQ(788.5f, enclosed2.maxY());
}

TEST(FloatRect, EnclosingIntRect)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 1024.3f, 768.6f);

    auto enclosed = WebCore::enclosingIntRect(rect);

    EXPECT_EQ(10, enclosed.x());
    EXPECT_EQ(20, enclosed.y());
    EXPECT_EQ(1035, enclosed.maxX());
    EXPECT_EQ(789, enclosed.maxY());

    WebCore::FloatRect maxIntRect(INT_MIN, INT_MIN, 0, 0);
    maxIntRect.shiftMaxXEdgeTo(INT_MAX);
    maxIntRect.shiftMaxYEdgeTo(INT_MAX);

    auto enclosed2 = WebCore::enclosingIntRect(maxIntRect);

    EXPECT_EQ(INT_MIN, enclosed2.x());
    EXPECT_EQ(INT_MIN, enclosed2.y());
    EXPECT_EQ(INT_MAX, enclosed2.width());
    EXPECT_EQ(INT_MAX, enclosed2.height());
}

TEST(FloatRect, RoundedIntRect)
{
    WebCore::FloatRect rect(10.0f, 20.0f, 1024.3f, 768.6f);

    auto enclosed = WebCore::roundedIntRect(rect);

    EXPECT_EQ(10, enclosed.x());
    EXPECT_EQ(20, enclosed.y());
    EXPECT_EQ(1034, enclosed.maxX());
    EXPECT_EQ(789, enclosed.maxY());
}

}

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

#include <WebCore/BezierUtilities.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>

namespace TestWebKitAPI {
using namespace WebCore;

// A cubic Bézier whose control points are evenly spaced, so it's exactly the straight line start->end (crossings are hand-computable).
static BezierSegment lineCurve(FloatPoint start, FloatPoint end)
{
    FloatSize delta = end - start;
    return { start, start + delta * (1.0f / 3.0f), start + delta * (2.0f / 3.0f), end };
}

static void expectPointNear(const FloatPoint& actual, float expectedX, float expectedY, float tolerance = 0.05f)
{
    EXPECT_NEAR(actual.x(), expectedX, tolerance);
    EXPECT_NEAR(actual.y(), expectedY, tolerance);
}

// A curve entirely inside the rect is returned unchanged (single piece, all four control points intact).
TEST(BezierUtilities, TrimReturnsStraightCurveInsideUnchanged)
{
    auto curve = lineCurve({ 5, 10 }, { 15, 10 });
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 5, 10);
    expectPointNear(result[0].controlPoint1, curve.controlPoint1.x(), curve.controlPoint1.y());
    expectPointNear(result[0].controlPoint2, curve.controlPoint2.x(), curve.controlPoint2.y());
    expectPointNear(result[0].end, 15, 10);
}

// A curved Bézier fully inside the rect is returned unchanged (exercises the real cubic solve finding no crossing in [0, 1]).
TEST(BezierUtilities, TrimReturnsCurvedBezierInsideUnchanged)
{
    BezierSegment curve { { 5, 10 }, { 8, 5 }, { 12, 5 }, { 15, 10 } };
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 5, 10);
    expectPointNear(result[0].controlPoint1, 8, 5);
    expectPointNear(result[0].controlPoint2, 12, 5);
    expectPointNear(result[0].end, 15, 10);
}

// A curve entirely outside the rect yields nothing.
TEST(BezierUtilities, TrimReturnsEmptyForCurveOutside)
{
    auto curve = lineCurve({ 5, 30 }, { 15, 30 }); // above the rect (y = 30 > 20)
    EXPECT_TRUE(trimBezierToRect(curve, FloatRect(0, 0, 20, 20)).isEmpty());
}

// A line spanning the rect horizontally is clipped to both the left and right edges.
TEST(BezierUtilities, TrimClipsSpanningLineToLeftAndRightEdges)
{
    auto curve = lineCurve({ -10, 10 }, { 30, 10 }); // enters left edge (x=0), exits right edge (x=20)
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 0, 10);
    expectPointNear(result[0].end, 20, 10);
}

// A line starting inside and exiting one edge keeps the original inside endpoint and clips the far end.
TEST(BezierUtilities, TrimKeepsInsideEndpointAndClipsCrossedEdge)
{
    auto curve = lineCurve({ 5, 10 }, { 30, 10 }); // starts inside, exits right edge at x = 20
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 5, 10); // unchanged inside endpoint
    expectPointNear(result[0].end, 20, 10); // clipped onto the right edge
}

// A diagonal line from (10,10) to (40,20) exits the right edge (x=20) at t=1/3, where y = 13.333.
TEST(BezierUtilities, TrimClipsDiagonalLineAtEdgeCrossing)
{
    auto curve = lineCurve({ 10, 10 }, { 40, 20 });
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 10, 10);
    expectPointNear(result[0].end, 20, 13.333f, 0.1f);
}

// A curved Bézier that dips through the top edge and back: clipped to the interior arc, both endpoints land on the edge (solver finds two real roots).
TEST(BezierUtilities, TrimClipsCurveDippingThroughTopEdge)
{
    // Endpoints above the rect (y = -5); control points pull the middle down to B(0.5) = (10, 10), inside.
    BezierSegment curve { { 5, -5 }, { 5, 15 }, { 15, 15 }, { 15, -5 } };
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    EXPECT_NEAR(result[0].start.y(), 0, 0.05f); // enters through the top edge (y = 0)
    EXPECT_NEAR(result[0].end.y(), 0, 0.05f); // exits through the top edge
    EXPECT_GT(result[0].start.x(), 0);
    EXPECT_LT(result[0].end.x(), 20);
}

// A vertical line spanning the rect is clipped to the top and bottom edges (the symmetric
// case of the horizontal span: its control bounds have zero width, which must not be
// treated as disjoint from the rect).
TEST(BezierUtilities, TrimClipsVerticalSpanningLineToTopAndBottomEdges)
{
    auto curve = lineCurve({ 10, -10 }, { 10, 30 }); // enters top edge (y=0), exits bottom edge (y=20)
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 10, 0);
    expectPointNear(result[0].end, 10, 20);
}

// A vertical line starting inside and exiting the bottom edge keeps the inside endpoint
// and clips the far end onto the edge.
TEST(BezierUtilities, TrimKeepsInsideEndpointAndClipsVerticalCrossedEdge)
{
    auto curve = lineCurve({ 10, 5 }, { 10, 30 }); // starts inside, exits bottom edge at y = 20
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 10, 5); // unchanged inside endpoint
    expectPointNear(result[0].end, 10, 20); // clipped onto the bottom edge
}

// A degenerate curve collapsed to a single point inside the rect is returned unchanged
// (zero-width and zero-height control bounds must still count as contained).
TEST(BezierUtilities, TrimReturnsSinglePointCurveInsideUnchanged)
{
    BezierSegment curve { { 10, 10 }, { 10, 10 }, { 10, 10 }, { 10, 10 } };
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    ASSERT_EQ(1u, result.size());
    expectPointNear(result[0].start, 10, 10);
    expectPointNear(result[0].end, 10, 10);
}

// A degenerate single-point curve outside the rect yields nothing.
TEST(BezierUtilities, TrimReturnsEmptyForSinglePointCurveOutside)
{
    BezierSegment curve { { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 } };
    EXPECT_TRUE(trimBezierToRect(curve, FloatRect(0, 0, 20, 20)).isEmpty());
}

// A horizontal line entirely to the left of the rect is disjoint and yields nothing, even
// though its control bounds are zero-height (guards the inclusive disjoint early-out).
TEST(BezierUtilities, TrimReturnsEmptyForHorizontalLineLeftOfRect)
{
    auto curve = lineCurve({ -30, 10 }, { -5, 10 });
    EXPECT_TRUE(trimBezierToRect(curve, FloatRect(0, 0, 20, 20)).isEmpty());
}

// A line lying exactly along the top edge is coincident with the rect boundary; its
// zero-height bounds sit on the edge, so it is retained rather than discarded as disjoint.
TEST(BezierUtilities, TrimKeepsLineLyingAlongTopEdge)
{
    auto curve = lineCurve({ 5, 0 }, { 15, 0 });
    auto result = trimBezierToRect(curve, FloatRect(0, 0, 20, 20));

    EXPECT_FALSE(result.isEmpty());
}

} // namespace TestWebKitAPI

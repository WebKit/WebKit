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

// Finds and applies the intersection in one step, as the corner contour builder does for
// corners that share an edge, where the ordering is known to be the right way round.
static bool trimAtIntersection(Vector<BezierSegment>& first, Vector<BezierSegment>& second)
{
    auto intersection = findMonotonicBezierCurvesIntersection(first, second);
    if (!intersection)
        return false;

    trimMonotonicBezierCurvesAtIntersection(first, second, *intersection);
    return true;
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

TEST(BezierUtilities, TrimMonotonicCurvesLeavesSeparatedRunsAlone)
{
    Vector<BezierSegment> first { lineCurve({ 0, 10 }, { 4, 0 }) }; // reaches the edge at x = 4
    Vector<BezierSegment> second { lineCurve({ 8, 0 }, { 12, 10 }) }; // leaves it at x = 8

    EXPECT_FALSE(trimAtIntersection(first, second));

    ASSERT_EQ(1u, first.size());
    ASSERT_EQ(1u, second.size());
    expectPointNear(first[0].end, 4, 0);
    expectPointNear(second[0].start, 8, 0);
}

TEST(BezierUtilities, TrimMonotonicCurvesMeetsStraightRunsAtTheirIntersection)
{
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 10, 10 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 10 }, { 10, 0 }) };

    EXPECT_TRUE(trimAtIntersection(first, second));

    ASSERT_EQ(1u, first.size());
    ASSERT_EQ(1u, second.size());
    expectPointNear(first[0].start, 0, 0); // the far end of each run is untouched
    expectPointNear(first[0].end, 5, 5);
    expectPointNear(second[0].start, 5, 5);
    expectPointNear(second[0].end, 10, 0);
}

TEST(BezierUtilities, TrimMonotonicCurvesFindsIntersectionAwayFromMidParameter)
{
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 12, 12 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 4 }, { 8, 4 }) };

    EXPECT_TRUE(trimAtIntersection(first, second));

    ASSERT_FALSE(first.isEmpty());
    ASSERT_FALSE(second.isEmpty());
    expectPointNear(first.last().end, 4, 4);
    expectPointNear(second.first().start, 4, 4);
}

TEST(BezierUtilities, TrimMonotonicCurvesLeavesParallelRunsAlone)
{
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 10, 10 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 3 }, { 10, 13 }) }; // parallel, three above

    EXPECT_FALSE(trimAtIntersection(first, second));

    ASSERT_EQ(1u, first.size());
    ASSERT_EQ(1u, second.size());
    expectPointNear(first[0].start, 0, 0);
    expectPointNear(first[0].end, 10, 10);
    expectPointNear(second[0].start, 0, 3);
    expectPointNear(second[0].end, 10, 13);
}

TEST(BezierUtilities, TrimMonotonicCurvesKeepsEarlierSegmentsWhole)
{
    // y = x, split at (5, 5), and y = 8, split at x = 5. They meet at (8, 8).
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 5, 5 }), lineCurve({ 5, 5 }, { 10, 10 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 8 }, { 5, 8 }), lineCurve({ 5, 8 }, { 10, 8 }) };

    EXPECT_TRUE(trimAtIntersection(first, second));

    ASSERT_EQ(2u, first.size());
    expectPointNear(first[0].start, 0, 0);
    expectPointNear(first[0].end, 5, 5); // untouched
    expectPointNear(first[1].end, 8, 8); // cut at the intersection

    ASSERT_EQ(1u, second.size()); // everything before the intersection is gone
    expectPointNear(second[0].start, 8, 8);
    expectPointNear(second[0].end, 10, 8);
}

TEST(BezierUtilities, TrimMonotonicCurvesLeavesRunsMeetingAtAPointAlone)
{
    Vector<BezierSegment> first { lineCurve({ 0, 10 }, { 5, 0 }) };
    Vector<BezierSegment> second { lineCurve({ 5, 0 }, { 10, 10 }) };

    trimAtIntersection(first, second);

    ASSERT_EQ(1u, first.size());
    ASSERT_EQ(1u, second.size());
    expectPointNear(first[0].start, 0, 10);
    expectPointNear(first[0].end, 5, 0);
    expectPointNear(second[0].start, 5, 0);
    expectPointNear(second[0].end, 10, 10);
}

TEST(BezierUtilities, TrimMonotonicCurvesMeetsCurvedRunsAtOnePoint)
{
    Vector<BezierSegment> first { { { 0, 0 }, { 6, 0 }, { 10, 4 }, { 10, 10 } } };
    Vector<BezierSegment> second { { { 0, 10 }, { 6, 10 }, { 10, 6 }, { 10, 0 } } };

    EXPECT_TRUE(trimAtIntersection(first, second));

    ASSERT_FALSE(first.isEmpty());
    ASSERT_FALSE(second.isEmpty());

    auto meeting = first.last().end;
    expectPointNear(second.first().start, meeting.x(), meeting.y());

    // Inside the box the two arcs span, and away from either run's far end.
    EXPECT_GT(meeting.x(), 0);
    EXPECT_LT(meeting.x(), 10);
    EXPECT_GT(meeting.y(), 0);
    EXPECT_LT(meeting.y(), 10);
}

TEST(BezierUtilities, TrimMonotonicCurvesHandlesEmptyRuns)
{
    Vector<BezierSegment> empty;
    Vector<BezierSegment> curves { lineCurve({ 0, 0 }, { 10, 10 }) };

    EXPECT_FALSE(trimAtIntersection(empty, curves));
    EXPECT_TRUE(empty.isEmpty());
    ASSERT_EQ(1u, curves.size());
    expectPointNear(curves[0].end, 10, 10);

    EXPECT_FALSE(trimAtIntersection(curves, empty));
    EXPECT_TRUE(empty.isEmpty());
    ASSERT_EQ(1u, curves.size());
    expectPointNear(curves[0].end, 10, 10);
}


// The intersection is reported with where it sits on each run, which is what lets a caller
// that tries both orderings of a pair keep only the meaningful one.
TEST(BezierUtilities, FindMonotonicCurvesReportsWhereTheIntersectionSits)
{
    // y = x split at (5, 5), and y = 8 split at x = 5. They meet at (8, 8), in the second
    // curve of each run, three fifths of the way along it.
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 5, 5 }), lineCurve({ 5, 5 }, { 10, 10 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 8 }, { 5, 8 }), lineCurve({ 5, 8 }, { 10, 8 }) };

    auto intersection = findMonotonicBezierCurvesIntersection(first, second);
    ASSERT_TRUE(intersection.has_value());

    EXPECT_EQ(1u, intersection->indexOnFirst);
    EXPECT_NEAR(intersection->parameterOnFirst, 0.6f, 0.01f);
    EXPECT_EQ(1u, intersection->indexOnSecond);
    EXPECT_NEAR(intersection->parameterOnSecond, 0.6f, 0.01f);

    // Both runs hold two curves, so the fractions are (1 + 0.6) / 2.
    EXPECT_NEAR(intersection->fractionAlongFirst, 0.8f, 0.01f);
    EXPECT_NEAR(intersection->fractionAlongSecond, 0.8f, 0.01f);
}

// Swapping the two runs describes the same point, but the sides to keep swap with it. Only
// one of the two orderings has the first run's tail meeting the second run's head.
TEST(BezierUtilities, FindMonotonicCurvesRejectsTheReversedOrdering)
{
    // The first run reaches the meeting point late, the second reaches it early.
    Vector<BezierSegment> tailFirst { lineCurve({ 0, 0 }, { 10, 10 }) };
    Vector<BezierSegment> headSecond { lineCurve({ 6, 6 }, { 16, 0 }) };

    auto forward = findMonotonicBezierCurvesIntersection(tailFirst, headSecond);
    ASSERT_TRUE(forward.has_value());
    EXPECT_GT(forward->fractionAlongFirst, forward->fractionAlongSecond);
    EXPECT_TRUE(forward->isTailToHead());

    auto reversed = findMonotonicBezierCurvesIntersection(headSecond, tailFirst);
    ASSERT_TRUE(reversed.has_value());
    EXPECT_FALSE(reversed->isTailToHead());
}

// Finding leaves the runs untouched, so a caller can look before deciding to cut.
TEST(BezierUtilities, FindMonotonicCurvesDoesNotModifyTheRuns)
{
    Vector<BezierSegment> first { lineCurve({ 0, 0 }, { 10, 10 }) };
    Vector<BezierSegment> second { lineCurve({ 0, 10 }, { 10, 0 }) };

    ASSERT_TRUE(findMonotonicBezierCurvesIntersection(first, second).has_value());

    ASSERT_EQ(1u, first.size());
    ASSERT_EQ(1u, second.size());
    expectPointNear(first[0].end, 10, 10);
    expectPointNear(second[0].start, 0, 10);
}

// Counting crossings tells a caller which side of a curve a point is on: the corner contour builder
// walks from the point to the corner's vertex and treats an even count as "the corner removed this point".
TEST(BezierUtilities, CountsCrossingsOfASegmentThatCutsTheCurve)
{
    auto curve = lineCurve({ 0, 10 }, { 20, 10 });

    EXPECT_EQ(1u, numberOfCrossingsWithSegment(curve, { 10, 0 }, { 10, 20 }));
    EXPECT_EQ(1u, numberOfCrossingsWithSegment(curve, { 10, 20 }, { 10, 0 }));
}

// Crossings past either end of the segment don't count, so a point can be tested against a nearby vertex.
TEST(BezierUtilities, CountsNoCrossingsForASegmentStoppingShortOfTheCurve)
{
    auto curve = lineCurve({ 0, 10 }, { 20, 10 });

    EXPECT_EQ(0u, numberOfCrossingsWithSegment(curve, { 10, 0 }, { 10, 9 }));
    EXPECT_EQ(0u, numberOfCrossingsWithSegment(curve, { 10, 11 }, { 10, 20 }));
    // Alongside the curve rather than across it.
    EXPECT_EQ(0u, numberOfCrossingsWithSegment(curve, { 5, 0 }, { 15, 0 }));
}

// A curve that doubles back crosses the same segment twice, which is what keeps the even/odd test honest.
TEST(BezierUtilities, CountsBothCrossingsOfACurveThatDoublesBack)
{
    BezierSegment archedCurve { { 0, 10 }, { 5, -10 }, { 15, -10 }, { 20, 10 } };

    EXPECT_EQ(2u, numberOfCrossingsWithSegment(archedCurve, { -5, 5 }, { 25, 5 }));
    EXPECT_EQ(0u, numberOfCrossingsWithSegment(archedCurve, { -5, 15 }, { 25, 15 }));
}

} // namespace TestWebKitAPI

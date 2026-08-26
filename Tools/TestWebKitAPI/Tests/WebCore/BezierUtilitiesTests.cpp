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

TEST(BezierUtilities, IntersectBeziersFindsSingleCrossing)
{
    auto rising = lineCurve({ 0, 0 }, { 10, 10 });
    auto falling = lineCurve({ 0, 10 }, { 10, 0 });

    auto result = intersectBeziers(rising, falling);

    ASSERT_EQ(1u, result.crossings.size());
    EXPECT_TRUE(result.coincidences.isEmpty());
    EXPECT_NEAR(0.5, result.crossings[0].parameterOnFirst, 1e-3);
    EXPECT_NEAR(0.5, result.crossings[0].parameterOnSecond, 1e-3);
    expectPointNear(result.crossings[0].point, 5, 5);
    EXPECT_FALSE(result.crossings[0].tangential);
}

TEST(BezierUtilities, IntersectBeziersFindsNothingWhenApart)
{
    auto result = intersectBeziers(lineCurve({ 0, 0 }, { 1, 1 }), lineCurve({ 10, 10 }, { 11, 11 }));

    EXPECT_TRUE(result.crossings.isEmpty());
    EXPECT_TRUE(result.coincidences.isEmpty());
}

// A crossing at the end of one curve and the start of the next still has to be reported.
TEST(BezierUtilities, IntersectBeziersFindsSharedEndpoint)
{
    auto along = lineCurve({ 0, 0 }, { 10, 0 });
    auto up = lineCurve({ 10, 0 }, { 10, 10 });

    auto result = intersectBeziers(along, up);

    ASSERT_EQ(1u, result.crossings.size());
    EXPECT_NEAR(1.0, result.crossings[0].parameterOnFirst, 1e-3);
    EXPECT_NEAR(0.0, result.crossings[0].parameterOnSecond, 1e-3);
    expectPointNear(result.crossings[0].point, 10, 0);
}

// The vertical controls 1, -1/3, -1/3, 1 are (2t - 1)^2, so this grazes y = 0 at t = 0.5 without
// crossing, and has to come back tangential.
TEST(BezierUtilities, IntersectBeziersFlagsTangentialTouch)
{
    BezierSegment grazing { { 0, 1 }, { 10.f / 3, -1.f / 3 }, { 20.f / 3, -1.f / 3 }, { 10, 1 } };
    auto flat = lineCurve({ 0, 0 }, { 10, 0 });

    auto result = intersectBeziers(grazing, flat);

    EXPECT_TRUE(result.coincidences.isEmpty());
    ASSERT_EQ(1u, result.crossings.size());
    EXPECT_NEAR(0.5, result.crossings[0].parameterOnFirst, 1e-2);
    expectPointNear(result.crossings[0].point, 5, 0);
    EXPECT_TRUE(result.crossings[0].tangential);
}

// Curves lying on one another overlap along a stretch, as two concave corners on a shared rect edge do,
// so it is reported as a coincidence rather than a crossing.
TEST(BezierUtilities, IntersectBeziersReportsCoincidentCurves)
{
    auto curve = lineCurve({ 0, 0 }, { 10, 0 });

    auto result = intersectBeziers(curve, curve);

    EXPECT_FALSE(result.coincidences.isEmpty());
}

// For a curve/line pair the line is the second edge, its parameter a fraction along the segment.
TEST(BezierUtilities, IntersectBezierAndLineFindsCrossing)
{
    auto result = intersectBezierAndLine(lineCurve({ 0, 0 }, { 10, 10 }), { 0, 10 }, { 10, 0 });

    ASSERT_EQ(1u, result.size());
    EXPECT_NEAR(0.5, result[0].parameterOnFirst, 1e-3);
    EXPECT_NEAR(0.5, result[0].parameterOnSecond, 1e-3);
    expectPointNear(result[0].point, 5, 5);
}

// The pieces have to tile the original: outer endpoints kept, each piece starting where the last ended.
TEST(BezierUtilities, SplitBezierAtParametersTilesTheCurve)
{
    auto curve = lineCurve({ 0, 0 }, { 12, 0 });

    auto pieces = splitBezierAtParameters(curve, { 0.25, 0.75 });

    ASSERT_EQ(3u, pieces.size());
    expectPointNear(pieces[0].start, 0, 0);
    expectPointNear(pieces[0].end, 3, 0);
    expectPointNear(pieces[1].start, 3, 0);
    expectPointNear(pieces[1].end, 9, 0);
    expectPointNear(pieces[2].start, 9, 0);
    expectPointNear(pieces[2].end, 12, 0);
}

// A cubic that is really a straight line keeps one direction end to end.
TEST(BezierUtilities, TangentOnBezierAtParameterFollowsTheCurve)
{
    auto curve = lineCurve({ 0, 0 }, { 10, 0 });

    for (double parameter : { 0.0, 0.25, 0.5, 1.0 }) {
        auto tangent = tangentOnBezierAtParameter(curve, parameter);
        EXPECT_GT(tangent.width(), 0);
        EXPECT_NEAR(0, tangent.height(), 1e-3);
    }
}

static BezierLoop rectangleLoop(const FloatRect& rect)
{
    return {
        lineCurve(rect.minXMinYCorner(), rect.maxXMinYCorner()),
        lineCurve(rect.maxXMinYCorner(), rect.maxXMaxYCorner()),
        lineCurve(rect.maxXMaxYCorner(), rect.minXMaxYCorner()),
        lineCurve(rect.minXMaxYCorner(), rect.minXMinYCorner()),
    };
}

// A corner sliver as the contour builder makes them: in along one edge, across, and back out.
static BezierLoop cornerSliver(const FloatPoint& vertex, const FloatPoint& start, const FloatPoint& end)
{
    return { lineCurve(vertex, start), lineCurve(start, end), lineCurve(end, vertex) };
}

static FloatRect loopBounds(const BezierLoop& loop)
{
    FloatRect bounds { loop.first().start, FloatSize { } };
    for (const auto& curve : loop) {
        bounds.extend(curve.start);
        bounds.extend(curve.end);
    }
    return bounds;
}

TEST(BezierUtilities, WindingNumberIsZeroOutsideAndNonZeroInside)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });

    EXPECT_EQ(0, windingNumberForLoop(square, { -1, 5 }));
    EXPECT_EQ(0, windingNumberForLoop(square, { 11, 5 }));
    EXPECT_EQ(0, windingNumberForLoop(square, { 5, -1 }));
    EXPECT_NE(0, windingNumberForLoop(square, { 5, 5 }));
    EXPECT_NE(0, windingNumberForLoop(square, { 0.5, 0.5 }));
}

TEST(BezierUtilities, SubtractingAMissingSliverLeavesTheRegionAlone)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    auto elsewhere = rectangleLoop({ 20, 20, 5, 5 });

    auto result = subtractLoopFromLoop(square, elsewhere);
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(square.size(), result[0].size());
}

TEST(BezierUtilities, SubtractingACoveringSliverLeavesNothing)
{
    auto square = rectangleLoop({ 2, 2, 4, 4 });
    auto cover = rectangleLoop({ 0, 0, 10, 10 });

    EXPECT_TRUE(subtractLoopFromLoop(square, cover).isEmpty());
}

// One corner cut off a square: the result keeps the square's other three corners and gains the cut.
TEST(BezierUtilities, SubtractingOneCornerSliverBevelsThatCorner)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    auto sliver = cornerSliver({ 0, 0 }, { 4, 0 }, { 0, 4 });

    auto result = subtractLoopFromLoop(square, sliver);
    ASSERT_EQ(1u, result.size());

    auto bounds = loopBounds(result[0]);
    EXPECT_NEAR(0, bounds.x(), 0.01);
    EXPECT_NEAR(0, bounds.y(), 0.01);
    EXPECT_NEAR(10, bounds.maxX(), 0.01);
    EXPECT_NEAR(10, bounds.maxY(), 0.01);

    // The cut corner is gone and the far corner is untouched.
    EXPECT_EQ(0, windingNumberForLoop(result[0], { 0.5, 0.5 }));
    EXPECT_NE(0, windingNumberForLoop(result[0], { 9.5, 9.5 }));
    EXPECT_NE(0, windingNumberForLoop(result[0], { 5, 5 }));
}

// Two slivers deep enough to overlap: subtracting in sequence has to remove their union, without the
// contour doubling back. This is the case the local crossing cut could not repair.
TEST(BezierUtilities, SubtractingOverlappingSliversRemovesTheirUnion)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    auto left = cornerSliver({ 0, 0 }, { 7, 0 }, { 0, 7 });
    auto right = cornerSliver({ 10, 0 }, { 10, 7 }, { 3, 0 });

    auto afterLeft = subtractLoopFromLoop(square, left);
    ASSERT_EQ(1u, afterLeft.size());
    auto result = subtractLoopFromLoop(afterLeft[0], right);
    ASSERT_EQ(1u, result.size());

    // Both cut corners are gone, and so is the wedge the two slivers share.
    EXPECT_EQ(0, windingNumberForLoop(result[0], { 1, 1 }));
    EXPECT_EQ(0, windingNumberForLoop(result[0], { 9, 1 }));
    EXPECT_EQ(0, windingNumberForLoop(result[0], { 5, 1 }));
    // The bottom of the square is untouched by either.
    EXPECT_NE(0, windingNumberForLoop(result[0], { 5, 9 }));
}

// Subtracting the same sliver twice must not change the result: overlap is idempotent, which is what lets
// callers subtract each corner without checking the others.
TEST(BezierUtilities, SubtractingTheSameSliverTwiceIsIdempotent)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    auto sliver = cornerSliver({ 0, 0 }, { 4, 0 }, { 0, 4 });

    auto once = subtractLoopFromLoop(square, sliver);
    ASSERT_EQ(1u, once.size());
    auto twice = subtractLoopFromLoop(once[0], sliver);
    ASSERT_EQ(1u, twice.size());

    EXPECT_EQ(once[0].size(), twice[0].size());
    EXPECT_EQ(0, windingNumberForLoop(twice[0], { 0.5, 0.5 }));
    EXPECT_NE(0, windingNumberForLoop(twice[0], { 5, 5 }));
}

// A curved sliver, so the arrangement is exercised on cubics rather than only on lines.
TEST(BezierUtilities, SubtractingACurvedSliverKeepsTheCurveInTheResult)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    BezierLoop sliver = {
        lineCurve({ 0, 0 }, { 5, 0 }),
        BezierSegment { { 5, 0 }, { 3, 1 }, { 1, 3 }, { 0, 5 } },
        lineCurve({ 0, 5 }, { 0, 0 }),
    };

    auto result = subtractLoopFromLoop(square, sliver);
    ASSERT_EQ(1u, result.size());

    EXPECT_EQ(0, windingNumberForLoop(result[0], { 0.5, 0.5 }));
    EXPECT_NE(0, windingNumberForLoop(result[0], { 5, 5 }));
    // The scooped side is inside the result, since the curve bows towards the corner.
    EXPECT_NE(0, windingNumberForLoop(result[0], { 3.5, 3.5 }));
}

// A wedge touching the region's edges only at its endpoints: the plain crossing case, unlike the slivers above.
TEST(BezierUtilities, SubtractingAWedgeTouchingTheRegionsEdgesAtPoints)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    BezierLoop wedge = {
        lineCurve({ 0, 4 }, { 4, 0 }),
        lineCurve({ 4, 0 }, { -40, -40 }),
        lineCurve({ -40, -40 }, { 0, 4 }),
    };

    auto result = subtractLoopFromLoop(square, wedge);
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(0, windingNumberForLoop(result[0], { 0.5, 0.5 }));
    EXPECT_NE(0, windingNumberForLoop(result[0], { 5, 5 }));
    EXPECT_NE(0, windingNumberForLoop(result[0], { 9.5, 0.5 }));
}

// A 54px double border on a 120px box with a 40px radius: every chord has left the 54..66 target rect and all
// four cross, leaving a diamond. Values from the corner-shape oracle (demos/corner-shape-oracle).
TEST(BezierUtilities, SubtractingFourBevelWedgesLeavesADiamond)
{
    // Each corner closes through a point far outside the box on its own diagonal, as the contour builder does.
    auto wedgeFor = [](FloatPoint start, FloatPoint end, FloatPoint apex) -> BezierLoop {
        return { lineCurve(start, end), lineCurve(end, apex), lineCurve(apex, start) };
    };

    BezierLoop region = rectangleLoop({ 54, 54, 12, 12 });
    const BezierLoop wedges[] = {
        wedgeFor({ 41.816f, 38.184f }, { 81.816f, 78.184f }, { 66 + 96, 54 - 96 }), // TopRight
        wedgeFor({ 81.816f, 41.816f }, { 41.816f, 81.816f }, { 66 + 96, 66 + 96 }), // BottomRight
        wedgeFor({ 78.184f, 81.816f }, { 38.184f, 41.816f }, { 54 - 96, 66 + 96 }), // BottomLeft
        wedgeFor({ 38.184f, 78.184f }, { 78.184f, 38.184f }, { 54 - 96, 54 - 96 }), // TopLeft
    };

    for (const auto& wedge : wedges) {
        auto loops = subtractLoopFromLoop(region, wedge);
        ASSERT_EQ(1u, loops.size());
        region = loops[0];
    }

    EXPECT_EQ(4u, region.size());
    EXPECT_NE(0, windingNumberForLoop(region, { 60, 60 }));
    // Just outside each diamond vertex, and well inside the target rect, so only the diamond can exclude it.
    EXPECT_EQ(0, windingNumberForLoop(region, { 56, 56 }));
    EXPECT_EQ(0, windingNumberForLoop(region, { 64, 64 }));

    auto bounds = loopBounds(region);
    EXPECT_NEAR(56.4, bounds.x(), 0.2);
    EXPECT_NEAR(63.6, bounds.maxX(), 0.2);
    EXPECT_NEAR(56.4, bounds.y(), 0.2);
    EXPECT_NEAR(63.6, bounds.maxY(), 0.2);
}

// A wedge whose apex touches the far edge exactly cuts the region in two, with four pieces meeting at that point.
// Proximity cannot pick between the two leaving it, so the branch goes by turning angle, as SkOpSegment::findNextOp.
TEST(BezierUtilities, SubtractingAWedgeTouchingTheOppositeEdgeSplitsTheRegion)
{
    auto square = rectangleLoop({ 0, 0, 10, 10 });
    BezierLoop wedge = {
        lineCurve({ 0, 0 }, { 10, 0 }),
        lineCurve({ 10, 0 }, { 5, 10 }),
        lineCurve({ 5, 10 }, { 0, 0 }),
    };

    auto result = subtractLoopFromLoop(square, wedge);
    ASSERT_EQ(2u, result.size());

    // One piece holds the bottom left, the other the bottom right, and neither holds the middle.
    bool leftHeld = false;
    bool rightHeld = false;
    for (const auto& loop : result) {
        bool holdsLeft = windingNumberForLoop(loop, { 1, 9 });
        bool holdsRight = windingNumberForLoop(loop, { 9, 9 });
        leftHeld |= holdsLeft;
        rightHeld |= holdsRight;
        EXPECT_EQ(0, windingNumberForLoop(loop, { 5, 5 }));
        // Taking the wrong branch at the touch point joins the two into one loop spanning both sides.
        EXPECT_FALSE(holdsLeft && holdsRight);
    }
    EXPECT_TRUE(leftHeld);
    EXPECT_TRUE(rightHeld);
}

} // namespace TestWebKitAPI

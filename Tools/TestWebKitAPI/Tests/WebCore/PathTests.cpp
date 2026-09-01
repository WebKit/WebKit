/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/AffineTransform.h>
#include <WebCore/BezierUtilities.h>
#include <WebCore/Path.h>
#include <WebCore/PathUtilities.h>
#include <limits>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(Path, DefinitelyEqual)
{
    Path path1;
    Path path2;
    const Path emptyPath;

    // Empty vs. empty.
    ASSERT_TRUE(path1.definitelyEqual(path2));

    constexpr auto testRect = FloatRect { 23, 12, 100, 200 };

    // Single segment vs. single segment.
    path1.addRect(testRect);
    path2.addRect(testRect);

    ASSERT_TRUE(path1.definitelyEqual(path1));
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Single segment vs. impl.
    path1.ensureImplForTesting();
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Impl vs. impl.
    path2.ensureImplForTesting();
    ASSERT_TRUE(path1.definitelyEqual(path2));
    ASSERT_TRUE(path2.definitelyEqual(path1));

    // Trigger impl. copying
    auto pathCopy = path1;
    ASSERT_TRUE(path1.definitelyEqual(pathCopy));
    ASSERT_TRUE(pathCopy.definitelyEqual(path1));

    // Empty vs. empty impl.
    Path emptyImplPath;
    emptyImplPath.ensureImplForTesting();
    ASSERT_TRUE(emptyPath.definitelyEqual(emptyImplPath));
    ASSERT_TRUE(emptyImplPath.definitelyEqual(emptyPath));
}

TEST(Path, DefinitelyNotEqual)
{
    Path path1;
    Path path2;
    const Path emptyPath;

    constexpr auto testRect1 = FloatRect { 23, 12, 100, 200 };
    constexpr auto testRect2 = FloatRect { 23, 13, 100, 200 };
    path1.addRect(testRect1);

    // Single segment vs. empty.
    ASSERT_FALSE(path1.definitelyEqual(emptyPath));
    ASSERT_FALSE(emptyPath.definitelyEqual(path1));

    // Single segment vs single segment.
    path2.addRect(testRect2);
    ASSERT_FALSE(path1.definitelyEqual(path2));
    ASSERT_FALSE(path2.definitelyEqual(path1));

    // Empty vs impl.
    path1.ensureImplForTesting();
    ASSERT_FALSE(emptyPath.definitelyEqual(path1));
    ASSERT_FALSE(path1.definitelyEqual(emptyPath));

    // Impl vs impl.
    path2.ensureImplForTesting();
    ASSERT_FALSE(path1.definitelyEqual(path2));
    ASSERT_FALSE(path2.definitelyEqual(path1));
}

TEST(Path, CurveBoundingRect)
{
    auto currentPoint = FloatPoint { 100, 0 };
    auto endPoint = FloatPoint { 300, 0 };
    auto controlPoint = FloatPoint { 200, 328 };

    Path path1;
    path1.moveTo(currentPoint);
    path1.addQuadCurveTo(controlPoint, endPoint);

    auto fastboundingRect1 = path1.fastBoundingRect();
    auto boundingRect1 = path1.boundingRect();

    ASSERT_TRUE(fastboundingRect1 != boundingRect1);
    ASSERT_TRUE(fastboundingRect1.contains(boundingRect1));

    // Build an equivalent cubic BezierCurve for the above QuadraticCurve.
    auto controlPoint1 = currentPoint + controlPoint.scaled(2);
    auto controlPoint2 = endPoint + controlPoint.scaled(2);

    static const float gOneOverThree = 1 / 3.f;

    controlPoint1.scale(gOneOverThree);
    controlPoint2.scale(gOneOverThree);

    Path path2;
    path2.moveTo(currentPoint);
    path2.addBezierCurveTo(controlPoint1, controlPoint2, endPoint);

    auto boundingRect2 = path2.boundingRect();

    ASSERT_TRUE(areEssentiallyEqual(boundingRect1, boundingRect2));
}

TEST(Path, IsEmpty)
{
    Path a;
    Path b;
    ASSERT_TRUE(a.isEmpty());
    ASSERT_TRUE(b.isEmpty());

    // platformPath() does not allocate new instances.
    ASSERT_EQ(a.platformPath(), a.platformPath());
    ASSERT_EQ(b.platformPath(), b.platformPath());

    // platformPath() does not change isEmpty().
    ASSERT_TRUE(a.isEmpty());
    ASSERT_TRUE(b.isEmpty());
    ASSERT_TRUE(a.definitelyEqual(b));
    ASSERT_TRUE(b.definitelyEqual(a));

    // ensureImplForTesting works.
    a.ensureImplForTesting();
    ASSERT_TRUE(a.definitelyEqual(b));
    ASSERT_TRUE(b.definitelyEqual(a));

    // addPath() with empty paths works.
    a.addPath(b, AffineTransform(1, 0, 0, 1, 0, 0));
    ASSERT_TRUE(a.isEmpty());
    ASSERT_TRUE(a.definitelyEqual(b));

    bool r = a.strokeContains({ 0, 0 }, [](GraphicsContext&) { });
    ASSERT_FALSE(r);
    ASSERT_TRUE(a.isEmpty());
    ASSERT_TRUE(a.definitelyEqual(b));
    r = a.contains({ 0, 0 }, WindRule::EvenOdd);
    ASSERT_FALSE(r);
    ASSERT_TRUE(a.isEmpty());
    ASSERT_TRUE(a.definitelyEqual(b));

}

TEST(PathFlattening, RectangleKeepsItsCornersAndCloses)
{
    Path path;
    path.addRect({ 10, 20, 100, 50 });

    auto polylines = flattenPathToPolylines(path, 1);
    ASSERT_EQ(1u, polylines.size());

    // Four corners, plus the first point repeated so the closing edge spans a consecutive pair.
    auto& contour = polylines[0];
    ASSERT_EQ(5u, contour.size());
    EXPECT_EQ(contour.first(), contour.last());
    for (auto corner : { FloatPoint { 10, 20 }, FloatPoint { 110, 20 }, FloatPoint { 110, 70 }, FloatPoint { 10, 70 } })
        EXPECT_TRUE(contour.contains(corner));
}

TEST(PathFlattening, CurveStaysWithinTolerance)
{
    // A circle of radius 100: every flattened point should land on it, to within the tolerance.
    constexpr float radius = 100;
    FloatPoint center { 200, 200 };
    Path path;
    path.addEllipseInRect({ center.x() - radius, center.y() - radius, 2 * radius, 2 * radius });

    for (float tolerance : { 4.f, 1.f, 0.25f }) {
        auto polylines = flattenPathToPolylines(path, 1, tolerance);
        ASSERT_EQ(1u, polylines.size());

        float worstRadialError = 0;
        float worstChord = 0;
        auto& contour = polylines[0];
        for (size_t i = 0; i < contour.size(); ++i) {
            worstRadialError = std::max(worstRadialError, std::abs((contour[i] - center).diagonalLength() - radius));
            if (i + 1 < contour.size())
                worstChord = std::max(worstChord, (contour[i + 1] - contour[i]).diagonalLength());
        }
        // The four kappa cubics are themselves ~0.00027*radius off a true circle, so allow for that on
        // top of the flattening tolerance rather than attributing it to the chords.
        EXPECT_LE(worstRadialError, tolerance + 0.0003f * radius) << "tolerance " << tolerance;
        // A finer tolerance has to actually subdivide more.
        EXPECT_GT(worstChord, 0.f);
    }

    // Tightening the tolerance must not reduce the number of samples.
    EXPECT_GE(flattenPathToPolylines(path, 1, 0.25f)[0].size(), flattenPathToPolylines(path, 1, 4.f)[0].size());
}

// A straight cubic needs no subdivision at all: its second differences are zero, so however long it is
// one chord reproduces it exactly. The chord length must not drive the count.
TEST(PathFlattening, StraightCubicNeedsOneChord)
{
    for (float length : { 50.f, 400.f, 4000.f }) {
        Path path;
        path.moveTo({ 0, 0 });
        path.addBezierCurveTo({ length / 3, 0 }, { 2 * length / 3, 0 }, { length, 0 });

        auto polylines = flattenPathToPolylines(path, 1);
        ASSERT_EQ(1u, polylines.size());
        // moveTo, the single chord's endpoint, and the repeated first point that closes the contour.
        EXPECT_EQ(3u, polylines[0].size()) << "length " << length;
    }
}

// A cusp turns hard while starting and ending in the same place, so a heuristic keyed off the distance
// from the start point underestimates it badly. Check the flattening actually tracks the curve.
TEST(PathFlattening, CuspStaysWithinTolerance)
{
    BezierSegment cusp { { 0, 0 }, { 100, 0 }, { -100, 0 }, { 0, 0 } };

    for (float tolerance : { 1.f, 0.25f }) {
        Vector<FloatPoint> chords;
        chords.append(cusp.start);
        appendFlattenedBezier(chords, cusp, 1, tolerance);

        float worst = 0;
        for (unsigned sample = 0; sample <= 2000; ++sample) {
            auto onCurve = pointOnBezierAtParameter(cusp, sample / 2000.0);
            float nearest = std::numeric_limits<float>::max();
            for (size_t index = 0; index + 1 < chords.size(); ++index) {
                auto span = chords[index + 1] - chords[index];
                float lengthSquared = span.width() * span.width() + span.height() * span.height();
                auto toPoint = onCurve - chords[index];
                float along = lengthSquared
                    ? std::clamp((toPoint.width() * span.width() + toPoint.height() * span.height()) / lengthSquared, 0.f, 1.f)
                    : 0.f;
                auto closest = chords[index] + span.scaled(along);
                nearest = std::min(nearest, (onCurve - closest).diagonalLength());
            }
            worst = std::max(worst, nearest);
        }
        EXPECT_LE(worst, tolerance) << "tolerance " << tolerance;
    }
}

// The device scale factor is what turns the tolerance into device pixels, so a 2x display has to subdivide
// more finely than a 1x one for the same geometry.
TEST(PathFlattening, HigherDeviceScaleSubdividesMore)
{
    Path path;
    path.addEllipseInRect({ 0, 0, 200, 200 });

    EXPECT_GT(flattenPathToPolylines(path, 2)[0].size(), flattenPathToPolylines(path, 1)[0].size());
}

TEST(PathFlattening, SeparateSubpathsStaySeparate)
{
    Path path;
    path.addRect({ 0, 0, 10, 10 });
    path.addRect({ 50, 50, 10, 10 });

    auto polylines = flattenPathToPolylines(path, 1);
    EXPECT_EQ(2u, polylines.size());
}

TEST(PathFlattening, EmptyPathHasNoPolylines)
{
    Path path;
    EXPECT_TRUE(flattenPathToPolylines(path, 1).isEmpty());
}

}

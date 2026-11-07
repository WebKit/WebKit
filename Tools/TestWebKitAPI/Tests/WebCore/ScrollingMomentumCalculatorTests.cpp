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

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ScrollExtents.h>
#include <WebCore/ScrollingMomentumCalculator.h>
#include <wtf/Seconds.h>

namespace TestWebKitAPI {

// These tests exercise BasicScrollingMomentumCalculator's cubic snap animation curve through the
// public scrollOffsetAfterElapsedTime() entry point. The interesting evaluation --
// cubicallyInterpolatedOffsetAtProgress() -- is private, so we assert the mathematical invariants
// the curve must satisfy: it starts at the initial offset, ends at the destination, and (for a
// destination collinear with the initial scroll delta) stays exactly on the straight line between
// them. These are the properties a change to the polynomial evaluation must preserve.

static constexpr WebCore::ScrollExtents largeScrollExtents { { 5000, 5000 }, { 500, 500 } };

// A destination is only reached via the cubic path when the initial delta is large enough, points
// toward the destination, and the start and end differ. largeScrollExtents keeps the projected
// destination well within bounds so it is not clamped.

TEST(ScrollingMomentumCalculator, CubicInterpolationStartsAtInitialOffset)
{
    // Initial delta collinear with (and pointing toward) the projected destination -> cubic path.
    WebCore::BasicScrollingMomentumCalculator calculator(largeScrollExtents, { 0, 0 }, { 100, 100 }, { 0, 0 });
    WebCore::ScrollingMomentumCalculator& momentumCalculator = calculator;

    auto offsetAtStart = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(0));

    // At zero elapsed time progress is 0, so the cubic must evaluate to its constant term, the
    // initial scroll offset, exactly.
    EXPECT_FLOAT_EQ(0, offsetAtStart.x());
    EXPECT_FLOAT_EQ(0, offsetAtStart.y());
}

TEST(ScrollingMomentumCalculator, CubicInterpolationReachesDestination)
{
    WebCore::BasicScrollingMomentumCalculator calculator(largeScrollExtents, { 0, 0 }, { 100, 100 }, { 0, 0 });
    WebCore::ScrollingMomentumCalculator& momentumCalculator = calculator;

    auto destination = momentumCalculator.destinationScrollOffset();
    EXPECT_GT(destination.x(), 0);
    EXPECT_FLOAT_EQ(destination.x(), destination.y());

    // Well past the 1s animation duration, progress clamps to 1 and the cubic must evaluate to the
    // destination (the sum of all curve coefficients).
    auto offsetAtEnd = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(5));
    EXPECT_NEAR(destination.x(), offsetAtEnd.x(), 0.5);
    EXPECT_NEAR(destination.y(), offsetAtEnd.y(), 0.5);
}

TEST(ScrollingMomentumCalculator, CubicInterpolationStaysOnLineForCollinearDelta)
{
    // The projected destination is initialOffset + k * initialDelta, so a diagonal delta yields a
    // destination on the line y = x. A cubic Bezier whose control points are collinear and evenly
    // spaced reduces to the straight-line parameterization, so every interpolated point must
    // satisfy x == y and advance monotonically from the origin to the destination.
    WebCore::BasicScrollingMomentumCalculator calculator(largeScrollExtents, { 0, 0 }, { 100, 100 }, { 0, 0 });
    WebCore::ScrollingMomentumCalculator& momentumCalculator = calculator;

    auto destination = momentumCalculator.destinationScrollOffset();

    float previousX = -1;
    for (int step = 0; step <= 10; ++step) {
        auto offset = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(step / 10.0));

        // On the line y = x throughout the animation.
        EXPECT_FLOAT_EQ(offset.x(), offset.y());

        // Monotonically non-decreasing and always within the [start, destination] bounding box.
        EXPECT_GE(offset.x(), previousX);
        EXPECT_GE(offset.x(), 0);
        EXPECT_LE(offset.x(), destination.x() + 0.5);
        previousX = offset.x();
    }
}

TEST(ScrollingMomentumCalculator, CubicInterpolationCurvesTowardInitialDeltaDirection)
{
    // A horizontal initial delta with a diagonal (retargeted) destination produces a genuinely
    // curved, non-collinear path. By construction the animation leaves the origin in the direction
    // of the initial delta (+x), so early in the animation x must lead y, while both endpoints are
    // still pinned to the initial offset and the destination.
    WebCore::BasicScrollingMomentumCalculator calculator(largeScrollExtents, { 0, 0 }, { 100, 0 }, { 0, 0 });
    WebCore::ScrollingMomentumCalculator& momentumCalculator = calculator;
    momentumCalculator.setRetargetedScrollOffset({ 1000, 1000 });

    auto destination = momentumCalculator.destinationScrollOffset();
    EXPECT_FLOAT_EQ(1000, destination.x());
    EXPECT_FLOAT_EQ(1000, destination.y());

    auto offsetAtStart = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(0));
    EXPECT_FLOAT_EQ(0, offsetAtStart.x());
    EXPECT_FLOAT_EQ(0, offsetAtStart.y());

    auto earlyOffset = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(0.05));
    EXPECT_GT(earlyOffset.x(), 0);
    EXPECT_GT(earlyOffset.x(), earlyOffset.y());

    auto offsetAtEnd = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(5));
    EXPECT_NEAR(destination.x(), offsetAtEnd.x(), 0.5);
    EXPECT_NEAR(destination.y(), offsetAtEnd.y(), 0.5);
}

TEST(ScrollingMomentumCalculator, LinearAnimationForSubUnitDelta)
{
    // A sub-unit initial delta forces the linear (non-cubic) animation curve. The endpoints should
    // still be the initial offset and the projected destination.
    WebCore::BasicScrollingMomentumCalculator calculator(largeScrollExtents, { 0, 0 }, { 0.5, 0.5 }, { 0, 0 });
    WebCore::ScrollingMomentumCalculator& momentumCalculator = calculator;

    auto destination = momentumCalculator.destinationScrollOffset();
    EXPECT_GT(destination.x(), 0);

    auto offsetAtStart = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(0));
    EXPECT_FLOAT_EQ(0, offsetAtStart.x());
    EXPECT_FLOAT_EQ(0, offsetAtStart.y());

    auto offsetAtEnd = momentumCalculator.scrollOffsetAfterElapsedTime(WTF::Seconds(5));
    EXPECT_NEAR(destination.x(), offsetAtEnd.x(), 0.5);
    EXPECT_NEAR(destination.y(), offsetAtEnd.y(), 0.5);
}

} // namespace TestWebKitAPI

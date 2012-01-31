/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "painting/PaintAggregator.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

TEST(PaintAggregator, InitialState)
{
    PaintAggregator greg;
    EXPECT_FALSE(greg.hasPendingUpdate());
}

TEST(PaintAggregator, SingleInvalidation)
{
    PaintAggregator greg;

    IntRect rect(2, 4, 10, 16);
    greg.invalidateRect(rect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    ASSERT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(rect, update.paintRects[0]);
}

TEST(PaintAggregator, DoubleDisjointInvalidation)
{
    PaintAggregator greg;

    IntRect r1(2, 4, 2, 40);
    IntRect r2(4, 2, 40, 2);

    greg.invalidateRect(r1);
    greg.invalidateRect(r2);

    IntRect expectedBounds = unionRect(r1, r2);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    EXPECT_EQ(2U, update.paintRects.size());

    EXPECT_EQ(expectedBounds, update.calculatePaintBounds());
}

TEST(PaintAggregator, DisjointInvalidationsCombined)
{
    PaintAggregator greg;

    // Make the rectangles such that they don't overlap but cover a very large
    // percentage of the area of covered by their union. This is so we're not
    // very sensitive to the combining heuristic in the paint aggregator.
    IntRect r1(2, 4, 2, 1000);
    IntRect r2(5, 2, 2, 1000);

    greg.invalidateRect(r1);
    greg.invalidateRect(r2);

    IntRect expectedBounds = unionRect(r1, r2);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    ASSERT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(expectedBounds, update.paintRects[0]);
}

TEST(PaintAggregator, SingleScroll)
{
    PaintAggregator greg;

    IntRect rect(1, 2, 3, 4);
    IntPoint delta(1, 0);
    greg.scrollRect(delta.x(), delta.y(), rect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.paintRects.isEmpty());
    EXPECT_FALSE(update.scrollRect.isEmpty());

    EXPECT_EQ(rect, update.scrollRect);

    EXPECT_EQ(delta.x(), update.scrollDelta.x());
    EXPECT_EQ(delta.y(), update.scrollDelta.y());

    IntRect resultingDamage = update.calculateScrollDamage();
    IntRect expectedDamage(1, 2, 1, 4);
    EXPECT_EQ(expectedDamage, resultingDamage);
}

TEST(PaintAggregator, DoubleOverlappingScroll)
{
    PaintAggregator greg;

    IntRect rect(1, 2, 3, 4);
    IntPoint delta1(1, 0);
    IntPoint delta2(1, 0);
    greg.scrollRect(delta1.x(), delta1.y(), rect);
    greg.scrollRect(delta2.x(), delta2.y(), rect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.paintRects.isEmpty());
    EXPECT_FALSE(update.scrollRect.isEmpty());

    EXPECT_EQ(rect, update.scrollRect);

    IntPoint expectedDelta(delta1.x() + delta2.x(),
                           delta1.y() + delta2.y());
    EXPECT_EQ(expectedDelta.x(), update.scrollDelta.x());
    EXPECT_EQ(expectedDelta.y(), update.scrollDelta.y());

    IntRect resultingDamage = update.calculateScrollDamage();
    IntRect expectedDamage(1, 2, 2, 4);
    EXPECT_EQ(expectedDamage, resultingDamage);
}

TEST(PaintAggregator, NegatingScroll)
{
    PaintAggregator greg;

    // Scroll twice in opposite directions by equal amounts. The result
    // should be no scrolling.

    IntRect rect(1, 2, 3, 4);
    IntPoint delta1(1, 0);
    IntPoint delta2(-1, 0);
    greg.scrollRect(delta1.x(), delta1.y(), rect);
    greg.scrollRect(delta2.x(), delta2.y(), rect);

    EXPECT_FALSE(greg.hasPendingUpdate());
}

TEST(PaintAggregator, DiagonalScroll)
{
    PaintAggregator greg;

    // We don't support optimized diagonal scrolling, so this should result in
    // repainting.

    IntRect rect(1, 2, 3, 4);
    IntPoint delta(1, 1);
    greg.scrollRect(delta.x(), delta.y(), rect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    ASSERT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(rect, update.paintRects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScroll)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    IntRect paintRect(4, 4, 2, 2);
    greg.invalidateRect(paintRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    // expecting a paint rect inside the scroll rect
    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(scrollRect, update.scrollRect);
    EXPECT_EQ(paintRect, update.paintRects[0]);
}

TEST(PaintAggregator, ContainedPaintBeforeScroll)
{
    PaintAggregator greg;

    IntRect paintRect(4, 4, 2, 2);
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    // Expecting a paint rect inside the scroll rect
    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    paintRect.move(2, 0);

    EXPECT_EQ(scrollRect, update.scrollRect);
    EXPECT_EQ(paintRect, update.paintRects[0]);
}

TEST(PaintAggregator, ContainedPaintsBeforeAndAfterScroll)
{
    PaintAggregator greg;

    IntRect paintRect1(4, 4, 2, 2);
    greg.invalidateRect(paintRect1);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    IntRect paintRect2(6, 4, 2, 2);
    greg.invalidateRect(paintRect2);

    IntRect expectedPaintRect = paintRect2;

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    // Expecting a paint rect inside the scroll rect
    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(scrollRect, update.scrollRect);
    EXPECT_EQ(expectedPaintRect, update.paintRects[0]);
}

TEST(PaintAggregator, LargeContainedPaintAfterScroll)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(0, 1, scrollRect);

    IntRect paintRect(0, 0, 10, 9); // Repaint 90%
    greg.invalidateRect(paintRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(scrollRect, update.paintRects[0]);
}

TEST(PaintAggregator, LargeContainedPaintBeforeScroll)
{
    PaintAggregator greg;

    IntRect paintRect(0, 0, 10, 9); // Repaint 90%
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(0, 1, scrollRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(scrollRect, update.paintRects[0]);
}

TEST(PaintAggregator, OverlappingPaintBeforeScroll)
{
    PaintAggregator greg;

    IntRect paintRect(4, 4, 10, 2);
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    IntRect expectedPaintRect = unionRect(scrollRect, paintRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(expectedPaintRect, update.paintRects[0]);
}

TEST(PaintAggregator, OverlappingPaintAfterScroll)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    IntRect paintRect(4, 4, 10, 2);
    greg.invalidateRect(paintRect);

    IntRect expectedPaintRect = unionRect(scrollRect, paintRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_TRUE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(expectedPaintRect, update.paintRects[0]);
}

TEST(PaintAggregator, DisjointPaintBeforeScroll)
{
    PaintAggregator greg;

    IntRect paintRect(4, 4, 10, 2);
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 2, 10);
    greg.scrollRect(2, 0, scrollRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(paintRect, update.paintRects[0]);
    EXPECT_EQ(scrollRect, update.scrollRect);
}

TEST(PaintAggregator, DisjointPaintAfterScroll)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 2, 10);
    greg.scrollRect(2, 0, scrollRect);

    IntRect paintRect(4, 4, 10, 2);
    greg.invalidateRect(paintRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(paintRect, update.paintRects[0]);
    EXPECT_EQ(scrollRect, update.scrollRect);
}

TEST(PaintAggregator, ContainedPaintTrimmedByScroll)
{
    PaintAggregator greg;

    IntRect paintRect(4, 4, 6, 6);
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(2, 0, scrollRect);

    // The paint rect should have become narrower.
    IntRect expectedPaintRect(6, 4, 4, 6);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(expectedPaintRect, update.paintRects[0]);
    EXPECT_EQ(scrollRect, update.scrollRect);
}

TEST(PaintAggregator, ContainedPaintEliminatedByScroll)
{
    PaintAggregator greg;

    IntRect paintRect(4, 4, 6, 6);
    greg.invalidateRect(paintRect);

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(6, 0, scrollRect);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_TRUE(update.paintRects.isEmpty());

    EXPECT_EQ(scrollRect, update.scrollRect);
}

TEST(PaintAggregator, ContainedPaintAfterScrollTrimmedByScrollDamage)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(4, 0, scrollRect);

    IntRect paintRect(2, 0, 4, 10);
    greg.invalidateRect(paintRect);

    IntRect expectedScrollDamage(0, 0, 4, 10);
    IntRect expectedPaintRect(4, 0, 2, 10);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_EQ(1U, update.paintRects.size());

    EXPECT_EQ(scrollRect, update.scrollRect);
    EXPECT_EQ(expectedScrollDamage, update.calculateScrollDamage());
    EXPECT_EQ(expectedPaintRect, update.paintRects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScrollEliminatedByScrollDamage)
{
    PaintAggregator greg;

    IntRect scrollRect(0, 0, 10, 10);
    greg.scrollRect(4, 0, scrollRect);

    IntRect paintRect(2, 0, 2, 10);
    greg.invalidateRect(paintRect);

    IntRect expectedScrollDamage(0, 0, 4, 10);

    EXPECT_TRUE(greg.hasPendingUpdate());
    PaintAggregator::PendingUpdate update;
    greg.popPendingUpdate(&update);

    EXPECT_FALSE(update.scrollRect.isEmpty());
    EXPECT_TRUE(update.paintRects.isEmpty());

    EXPECT_EQ(scrollRect, update.scrollRect);
    EXPECT_EQ(expectedScrollDamage, update.calculateScrollDamage());
}

} // namespace

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

#include <WebCore/FlexLineBreaker.h>
#include <WebCore/LayoutUnit.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using WebCore::LayoutUnit;

static Vector<LayoutUnit> itemSizes(std::initializer_list<int> values)
{
    Vector<LayoutUnit> sizes(ReserveCapacityTag { }, values.size());
    for (auto value : values)
        sizes.append(LayoutUnit(value));
    return sizes;
}

static void expectLineBreaks(const Vector<size_t>& actual, std::initializer_list<size_t> expected)
{
    Vector<size_t> expectedBreaks(ReserveCapacityTag { }, expected.size());
    for (auto value : expected)
        expectedBreaks.append(value);
    EXPECT_EQ(expectedBreaks, actual);
}

TEST(WebCoreFlexLineBreaker, GreedyFillsEachLineBeforeMovingOn)
{
    auto sizes = itemSizes({ 50, 50, 50, 50 });
    expectLineBreaks(WebCore::greedyLineBreaks(sizes.span(), LayoutUnit(120), LayoutUnit(0)), { 2, 4 });
}

TEST(WebCoreFlexLineBreaker, NoItemsProducesNoLines)
{
    expectLineBreaks(WebCore::balancedLineBreaks({ }, LayoutUnit(200), LayoutUnit(0), 1), { });
}

TEST(WebCoreFlexLineBreaker, EverythingOnOneLineWhenNoMinimumApplies)
{
    auto sizes = itemSizes({ 50, 50, 50, 50 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit(200), LayoutUnit(0), 1), { 4 });
}

TEST(WebCoreFlexLineBreaker, BalancingBeatsGreedyFilling)
{
    auto sizes = itemSizes({ 50, 50, 50, 50 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit(200), LayoutUnit(0), 2), { 2, 4 });
}

TEST(WebCoreFlexLineBreaker, MinimumLineCountSplitsALineThatWouldOtherwiseFit)
{
    auto sizes = itemSizes({ 60, 60, 60 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit(220), LayoutUnit(20), 2), { 2, 3 });
}

TEST(WebCoreFlexLineBreaker, MinimumLineCountClampsToTheItemCount)
{
    auto sizes = itemSizes({ 50, 50, 50 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit(500), LayoutUnit(0), 10), { 1, 2, 3 });
}

TEST(WebCoreFlexLineBreaker, AnItemWiderThanTheLineGetsALineToItself)
{
    auto sizes = itemSizes({ 50, 300, 50 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit(100), LayoutUnit(0), 1), { 1, 2, 3 });
}

TEST(WebCoreFlexLineBreaker, IndefiniteAvailableSpaceStillHonorsTheMinimumLineCount)
{
    auto sizes = itemSizes({ 64, 64, 64, 64, 64, 64, 64, 64 });
    expectLineBreaks(WebCore::balancedLineBreaks(sizes.span(), LayoutUnit::max(), LayoutUnit(0), 5), { 2, 4, 6, 7, 8 });
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeOfNoItemsIsZero)
{
    EXPECT_EQ(LayoutUnit(0), WebCore::smallestMainSizeForLineCount({ }, LayoutUnit(20), 2));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeFitsTheLineCount)
{
    auto sizes = itemSizes({ 40, 40, 40 });
    EXPECT_EQ(LayoutUnit(100), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(20), 2));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeForOneLineHoldsEveryItem)
{
    auto sizes = itemSizes({ 40, 40, 40 });
    EXPECT_EQ(LayoutUnit(160), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(20), 1));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeIsTheLongestItemWhenEachGetsALine)
{
    auto sizes = itemSizes({ 30, 100, 30 });
    EXPECT_EQ(LayoutUnit(100), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(20), 5));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeIsTheLongestLineOfTheTightestSplit)
{
    auto sizes = itemSizes({ 30, 100, 30 });
    EXPECT_EQ(LayoutUnit(150), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(20), 2));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeFloorsNegativeSizesAtZero)
{
    auto sizes = itemSizes({ -10, 40, 40 });
    EXPECT_EQ(LayoutUnit(60), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(20), 2));
}

TEST(WebCoreFlexLineBreaker, SmallestMainSizeKeepsFractionalSizes)
{
    auto sizes = Vector<LayoutUnit> { LayoutUnit(10.5f), LayoutUnit(10.25f), LayoutUnit(10.5f) };
    EXPECT_EQ(LayoutUnit(20.75f), WebCore::smallestMainSizeForLineCount(sizes.span(), LayoutUnit(0), 2));
}

TEST(WebCoreFlexLineBreaker, BalancedLineBreaksAtTheSmallestMainSizeHaveTheLineCount)
{
    auto sizes = itemSizes({ 30, 10, 60, 20, 50, 40, 10 });
    auto gap = LayoutUnit(5);
    for (size_t lineCount = 1; lineCount <= sizes.size(); ++lineCount) {
        auto mainSize = WebCore::smallestMainSizeForLineCount(sizes.span(), gap, lineCount);
        EXPECT_EQ(lineCount, WebCore::balancedLineBreaks(sizes.span(), mainSize, gap, lineCount).size());
        // Below the longest item, that item overflows a line of its own instead.
        if (mainSize > LayoutUnit(60))
            EXPECT_GT(WebCore::greedyLineBreaks(sizes.span(), mainSize - LayoutUnit::fromRawValue(1), gap).size(), lineCount);
    }
}

} // namespace TestWebKitAPI

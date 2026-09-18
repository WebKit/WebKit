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
    Vector<LayoutUnit> sizes;
    sizes.reserveInitialCapacity(values.size());
    for (auto value : values)
        sizes.append(LayoutUnit(value));
    return sizes;
}

static void expectLineBreaks(const Vector<size_t>& actual, std::initializer_list<size_t> expected)
{
    Vector<size_t> expectedBreaks;
    expectedBreaks.reserveInitialCapacity(expected.size());
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

} // namespace TestWebKitAPI

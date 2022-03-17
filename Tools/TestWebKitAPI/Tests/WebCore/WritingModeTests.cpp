/*
 * Copyright (C) 2022 Igalia S.L.
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

#include <WebCore/WritingMode.h>

using namespace WebCore;

namespace TestWebKitAPI {

constexpr std::array<TextFlow, 8> allTextFlows = {
    InlineEastBlockSouth,
    InlineWestBlockSouth,
    InlineEastBlockNorth,
    InlineWestBlockNorth,
    InlineSouthBlockEast,
    InlineSouthBlockWest,
    InlineNorthBlockEast,
    InlineNorthBlockWest,
};

TEST(WritingMode, LogicalBoxSide)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxSide logicalSide) {
        return mapPhysicalSideToLogicalSide(textFlow, mapLogicalSideToPhysicalSide(textFlow, logicalSide));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxSide::BlockStart), LogicalBoxSide::BlockStart) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxSide::BlockEnd), LogicalBoxSide::BlockEnd) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxSide::InlineStart), LogicalBoxSide::InlineStart) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxSide::InlineEnd), LogicalBoxSide::InlineEnd) << "with textFlow=" << textFlow;
    }
}

TEST(WritingMode, BoxSide)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxSide side) {
        return mapLogicalSideToPhysicalSide(textFlow, mapPhysicalSideToLogicalSide(textFlow, side));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, BoxSide::Top), BoxSide::Top) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxSide::Right), BoxSide::Right) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxSide::Bottom), BoxSide::Bottom) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxSide::Left), BoxSide::Left) << "with textFlow=" << textFlow;
    }
}

TEST(WritingMode, LogicalBoxCorner)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxCorner logicalCorner) {
        return mapPhysicalCornerToLogicalCorner(textFlow, mapLogicalCornerToPhysicalCorner(textFlow, logicalCorner));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxCorner::StartStart), LogicalBoxCorner::StartStart) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxCorner::StartEnd), LogicalBoxCorner::StartEnd) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxCorner::EndStart), LogicalBoxCorner::EndStart) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxCorner::EndEnd), LogicalBoxCorner::EndEnd) << "with textFlow=" << textFlow;
    }
}

TEST(WritingMode, BoxCorner)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxCorner corner) {
        return mapLogicalCornerToPhysicalCorner(textFlow, mapPhysicalCornerToLogicalCorner(textFlow, corner));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, BoxCorner::TopLeft), BoxCorner::TopLeft) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxCorner::TopRight), BoxCorner::TopRight) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxCorner::BottomRight), BoxCorner::BottomRight) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxCorner::BottomLeft), BoxCorner::BottomLeft) << "with textFlow=" << textFlow;
    }
}

TEST(WritingMode, LogicalBoxAxis)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxAxis logicalAxis) {
        return mapPhysicalAxisToLogicalAxis(textFlow, mapLogicalAxisToPhysicalAxis(textFlow, logicalAxis));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxAxis::Block), LogicalBoxAxis::Block) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, LogicalBoxAxis::Inline), LogicalBoxAxis::Inline) << "with textFlow=" << textFlow;
    }
}

TEST(WritingMode, BoxAxis)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxAxis axis) {
        return mapLogicalAxisToPhysicalAxis(textFlow, mapPhysicalAxisToLogicalAxis(textFlow, axis));
    };
    for (TextFlow textFlow : allTextFlows) {
        EXPECT_EQ(mapBackAndForth(textFlow, BoxAxis::Horizontal), BoxAxis::Horizontal) << "with textFlow=" << textFlow;
        EXPECT_EQ(mapBackAndForth(textFlow, BoxAxis::Vertical), BoxAxis::Vertical) << "with textFlow=" << textFlow;
    }
}

}

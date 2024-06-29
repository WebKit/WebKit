/*
 * Copyright (C) 2022 Igalia S.L.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

constexpr std::array<TextFlow, 8> flows = {
    TextFlow { BlockFlowDirection::TopToBottom, TextDirection::LTR },
    TextFlow { BlockFlowDirection::TopToBottom, TextDirection::RTL },
    TextFlow { BlockFlowDirection::BottomToTop, TextDirection::LTR },
    TextFlow { BlockFlowDirection::BottomToTop, TextDirection::RTL },
    TextFlow { BlockFlowDirection::LeftToRight, TextDirection::LTR },
    TextFlow { BlockFlowDirection::LeftToRight, TextDirection::RTL },
    TextFlow { BlockFlowDirection::RightToLeft, TextDirection::LTR },
    TextFlow { BlockFlowDirection::RightToLeft, TextDirection::RTL }
};

inline std::string textFlowString(TextFlow flow)
{
    TextStream stream;
    stream << flow;
    return stream.release().utf8().toStdString();
}

TEST(WritingMode, LogicalBoxSide)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxSide logicalSide) {
        return mapPhysicalSideToLogicalSide(textFlow, mapLogicalSideToPhysicalSide(textFlow, logicalSide));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::BlockStart), LogicalBoxSide::BlockStart) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::BlockEnd), LogicalBoxSide::BlockEnd) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::InlineStart), LogicalBoxSide::InlineStart) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::InlineEnd), LogicalBoxSide::InlineEnd) << "with flow=" << textFlowString(flow);
    }
}

TEST(WritingMode, BoxSide)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxSide side) {
        return mapLogicalSideToPhysicalSide(textFlow, mapPhysicalSideToLogicalSide(textFlow, side));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Top), BoxSide::Top) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Right), BoxSide::Right) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Bottom), BoxSide::Bottom) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Left), BoxSide::Left) << "with flow=" << textFlowString(flow);
    }
}

TEST(WritingMode, LogicalBoxCorner)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxCorner logicalCorner) {
        return mapPhysicalCornerToLogicalCorner(textFlow, mapLogicalCornerToPhysicalCorner(textFlow, logicalCorner));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::StartStart), LogicalBoxCorner::StartStart) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::StartEnd), LogicalBoxCorner::StartEnd) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::EndStart), LogicalBoxCorner::EndStart) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::EndEnd), LogicalBoxCorner::EndEnd) << "with flow=" << textFlowString(flow);
    }
}

TEST(WritingMode, BoxCorner)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxCorner corner) {
        return mapLogicalCornerToPhysicalCorner(textFlow, mapPhysicalCornerToLogicalCorner(textFlow, corner));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::TopLeft), BoxCorner::TopLeft) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::TopRight), BoxCorner::TopRight) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::BottomRight), BoxCorner::BottomRight) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::BottomLeft), BoxCorner::BottomLeft) << "with flow=" << textFlowString(flow);
    }
}

TEST(WritingMode, LogicalBoxAxis)
{
    auto mapBackAndForth = [](TextFlow textFlow, LogicalBoxAxis logicalAxis) {
        return mapPhysicalAxisToLogicalAxis(textFlow, mapLogicalAxisToPhysicalAxis(textFlow, logicalAxis));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxAxis::Block), LogicalBoxAxis::Block) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxAxis::Inline), LogicalBoxAxis::Inline) << "with flow=" << textFlowString(flow);
    }
}

TEST(WritingMode, BoxAxis)
{
    auto mapBackAndForth = [](TextFlow textFlow, BoxAxis axis) {
        return mapLogicalAxisToPhysicalAxis(textFlow, mapPhysicalAxisToLogicalAxis(textFlow, axis));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxAxis::Horizontal), BoxAxis::Horizontal) << "with flow=" << textFlowString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxAxis::Vertical), BoxAxis::Vertical) << "with flow=" << textFlowString(flow);
    }
}

}

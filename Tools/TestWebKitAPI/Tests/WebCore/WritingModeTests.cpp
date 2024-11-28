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

#include <WebCore/BoxSides.h>
#include <WebCore/WritingMode.h>

using namespace WebCore;

namespace TestWebKitAPI {

constexpr std::array<WritingMode, 28> flows = {
    WritingMode { StyleWritingMode::HorizontalTb, TextDirection::LTR, TextOrientation::Mixed  },
    WritingMode { StyleWritingMode::HorizontalTb, TextDirection::LTR, TextOrientation::Mixed  },
    WritingMode { StyleWritingMode::HorizontalBt, TextDirection::RTL, TextOrientation::Mixed  },
    WritingMode { StyleWritingMode::HorizontalBt, TextDirection::RTL, TextOrientation::Mixed  },

    WritingMode { StyleWritingMode::VerticalRl, TextDirection::LTR, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::VerticalRl, TextDirection::LTR, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::VerticalRl, TextDirection::LTR, TextOrientation::Sideways },
    WritingMode { StyleWritingMode::VerticalRl, TextDirection::RTL, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::VerticalRl, TextDirection::RTL, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::VerticalRl, TextDirection::RTL, TextOrientation::Sideways },

    WritingMode { StyleWritingMode::VerticalLr, TextDirection::LTR, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::VerticalLr, TextDirection::LTR, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::VerticalLr, TextDirection::LTR, TextOrientation::Sideways },
    WritingMode { StyleWritingMode::VerticalLr, TextDirection::RTL, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::VerticalLr, TextDirection::RTL, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::VerticalLr, TextDirection::RTL, TextOrientation::Sideways },

    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::LTR, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::LTR, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::LTR, TextOrientation::Sideways },
    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::RTL, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::RTL, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::SidewaysRl, TextDirection::RTL, TextOrientation::Sideways },

    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::LTR, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::LTR, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::LTR, TextOrientation::Sideways },
    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::RTL, TextOrientation::Mixed    },
    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::RTL, TextOrientation::Upright  },
    WritingMode { StyleWritingMode::SidewaysLr, TextDirection::RTL, TextOrientation::Sideways }
};

inline std::string writingModeString(WritingMode writingMode)
{
    TextStream stream;
    stream << writingMode;
    return stream.release().utf8().toStdString();
}

TEST(WritingMode, LogicalBoxSide)
{
    auto mapBackAndForth = [](WritingMode writingMode, LogicalBoxSide logicalSide) {
        return mapSidePhysicalToLogical(writingMode, mapSideLogicalToPhysical(writingMode, logicalSide));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::BlockStart), LogicalBoxSide::BlockStart) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::BlockEnd), LogicalBoxSide::BlockEnd) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::InlineStart), LogicalBoxSide::InlineStart) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxSide::InlineEnd), LogicalBoxSide::InlineEnd) << "with flow=" << writingModeString(flow);
    }
}

TEST(WritingMode, BoxSide)
{
    auto mapBackAndForth = [](WritingMode writingMode, BoxSide side) {
        return mapSideLogicalToPhysical(writingMode, mapSidePhysicalToLogical(writingMode, side));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Top), BoxSide::Top) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Right), BoxSide::Right) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Bottom), BoxSide::Bottom) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxSide::Left), BoxSide::Left) << "with flow=" << writingModeString(flow);
    }
}

TEST(WritingMode, LogicalBoxCorner)
{
    auto mapBackAndForth = [](WritingMode writingMode, LogicalBoxCorner logicalCorner) {
        return mapCornerPhysicalToLogical(writingMode, mapCornerLogicalToPhysical(writingMode, logicalCorner));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::StartStart), LogicalBoxCorner::StartStart) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::StartEnd), LogicalBoxCorner::StartEnd) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::EndStart), LogicalBoxCorner::EndStart) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxCorner::EndEnd), LogicalBoxCorner::EndEnd) << "with flow=" << writingModeString(flow);
    }
}

TEST(WritingMode, BoxCorner)
{
    auto mapBackAndForth = [](WritingMode writingMode, BoxCorner corner) {
        return mapCornerLogicalToPhysical(writingMode, mapCornerPhysicalToLogical(writingMode, corner));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::TopLeft), BoxCorner::TopLeft) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::TopRight), BoxCorner::TopRight) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::BottomRight), BoxCorner::BottomRight) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxCorner::BottomLeft), BoxCorner::BottomLeft) << "with flow=" << writingModeString(flow);
    }
}

TEST(WritingMode, LogicalBoxAxis)
{
    auto mapBackAndForth = [](WritingMode writingMode, LogicalBoxAxis logicalAxis) {
        return mapAxisPhysicalToLogical(writingMode, mapAxisLogicalToPhysical(writingMode, logicalAxis));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxAxis::Block), LogicalBoxAxis::Block) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, LogicalBoxAxis::Inline), LogicalBoxAxis::Inline) << "with flow=" << writingModeString(flow);
    }
}

TEST(WritingMode, BoxAxis)
{
    auto mapBackAndForth = [](WritingMode writingMode, BoxAxis axis) {
        return mapAxisLogicalToPhysical(writingMode, mapAxisPhysicalToLogical(writingMode, axis));
    };
    for (auto flow : flows) {
        EXPECT_EQ(mapBackAndForth(flow, BoxAxis::Horizontal), BoxAxis::Horizontal) << "with flow=" << writingModeString(flow);
        EXPECT_EQ(mapBackAndForth(flow, BoxAxis::Vertical), BoxAxis::Vertical) << "with flow=" << writingModeString(flow);
    }
}

}

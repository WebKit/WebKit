/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/FlowMode.h>

using namespace WebCore;

namespace TestWebKitAPI {

constexpr std::array<WritingMode, 28> writingModes = {
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

constexpr std::array<BoxAxis, 2> boxAxes = {
    BoxAxis::Horizontal,
    BoxAxis::Vertical
};

constexpr std::array<LogicalBoxAxis, 2> logicalBoxAxes = {
    LogicalBoxAxis::Inline,
    LogicalBoxAxis::Block
};

constexpr std::array<FlowReversal, 4> flowReversals = {
    FlowReversal { AxisDirection::Normal, AxisDirection::Normal },
    FlowReversal { AxisDirection::Normal, AxisDirection::Reverse },
    FlowReversal { AxisDirection::Reverse, AxisDirection::Normal },
    FlowReversal { AxisDirection::Reverse, AxisDirection::Reverse },
};

inline std::string writingModeString(WritingMode writingMode)
{
    TextStream stream;
    stream << writingMode;
    return stream.release().utf8().toStdString();
}

inline std::string flowModeString(FlowMode flowMode)
{
    TextStream stream;
    stream << flowMode;
    return stream.release().utf8().toStdString();
}

inline std::string flowReversalString(FlowReversal reversal)
{
    TextStream stream;
    stream << "(main=" << reversal.main << ", cross=" << reversal.cross << ")";
    return stream.release().utf8().toStdString();
}

// Tests all combinations of WritingMode, mainAxis, and FlowReversal
// using the FlowMode(WritingMode, BoxAxis, FlowReversal) constructor
// to ensure they map to an appropriate WritingMode.
TEST(FlowMode, BoxAxisConstructor)
{
    for (auto writingMode : writingModes) {
        for (auto mainAxis : boxAxes) {
            bool mainIsAligned = (mainAxis == BoxAxis::Horizontal) == writingMode.isHorizontal();
            for (auto reversal : flowReversals) {
                auto flowMode = FlowMode(writingMode, mainAxis, reversal);
                auto description = [&] {
                    return " with writingMode=" + writingModeString(writingMode) + ", mainAxis=" + (mainAxis == BoxAxis::Horizontal ? "Horizontal" : "Vertical") + ", reversal=" + flowReversalString(reversal);
                };

                bool isRTL = mainIsAligned
                    ? (writingMode.isInlineFlipped() != reversal.isMainReversed())
                    : (writingMode.isBlockFlipped() != reversal.isMainReversed());
                bool isFlippedBlock = mainIsAligned
                    ? (writingMode.isBlockFlipped() != reversal.isCrossReversed())
                    : (writingMode.isInlineFlipped() != reversal.isCrossReversed());

                auto expectedStyleWritingMode = mainAxis == BoxAxis::Horizontal
                    ? (isFlippedBlock ? StyleWritingMode::HorizontalBt : StyleWritingMode::HorizontalTb)
                    : (isFlippedBlock ? StyleWritingMode::VerticalRl : StyleWritingMode::VerticalLr);
                auto expectedWritingMode = WritingMode { expectedStyleWritingMode, isRTL ? TextDirection::RTL : TextDirection::LTR, TextOrientation::Mixed };

                EXPECT_EQ(flowMode.mainDirection(), FlowMode(expectedWritingMode).mainDirection())
                    << description()
                    << " for " << flowModeString(flowMode)
                    << ", " << writingModeString(expectedWritingMode);
                EXPECT_EQ(flowMode.crossDirection(), FlowMode(expectedWritingMode).crossDirection())
                    << description()
                    << " for " << flowModeString(flowMode)
                    << ", " << writingModeString(expectedWritingMode);
                EXPECT_EQ(static_cast<WritingMode>(flowMode).inlineDirection(), expectedWritingMode.inlineDirection())
                    << description()
                    << " for " << flowModeString(flowMode)
                    << ", " << writingModeString(expectedWritingMode);
                EXPECT_EQ(static_cast<WritingMode>(flowMode).blockDirection(), expectedWritingMode.blockDirection())
                    << description()
                    << " for " << flowModeString(flowMode)
                    << ", " << writingModeString(expectedWritingMode);
            }
        }
    }
}

// Verifies that the FlowMode(WritingMode, LogicalBoxAxis, FlowReversal) constructor
// produces the same FlowMode as the equivalent BoxAxis construction.
TEST(FlowMode, LogicalBoxAxisConstructor)
{
    for (auto writingMode : writingModes) {
        for (auto logicalAxis : logicalBoxAxes) {
            auto physicalAxis = mapAxisLogicalToPhysical(writingMode, logicalAxis);
            for (auto reversal : flowReversals) {
                auto flowModeFromLogical = FlowMode(writingMode, logicalAxis, reversal);
                auto flowModeFromPhysical = FlowMode(writingMode, physicalAxis, reversal);
                EXPECT_EQ(flowModeFromLogical, flowModeFromPhysical)
                    << " for " << writingModeString(writingMode)
                    << " " << (logicalAxis == LogicalBoxAxis::Inline ? "Inline" : "Block")
                    << " " << flowReversalString(reversal)
                    << " -> " << flowModeString(flowModeFromLogical)
                    << ", " << flowModeString(flowModeFromPhysical);
            }
        }
    }
}

// Verifies that the FlowMode(WritingMode, ConversionStyle) constructor produces
// effectively the same FlowMode as the equivalent LogicalBoxAxis construction.
TEST(FlowMode, ConversionStyleConstructor)
{
    for (auto writingMode : writingModes) {
        auto flowRelative = FlowMode(writingMode, FlowMode::ConversionStyle::FlowRelative);
        auto flowRelativeExpected = FlowMode(writingMode, LogicalBoxAxis::Inline, FlowReversal { AxisDirection::Normal, AxisDirection::Normal });
        EXPECT_EQ(static_cast<WritingMode>(flowRelative), static_cast<WritingMode>(flowRelativeExpected))
            << " for " << writingModeString(writingMode)
            << " flowRelative " << flowModeString(flowRelative)
            << " expected=" << flowModeString(flowRelativeExpected);

        auto lineRelative = FlowMode(writingMode, FlowMode::ConversionStyle::LineRelative);
        auto crossReversal = writingMode.isLineInverted() ? AxisDirection::Reverse : AxisDirection::Normal;
        auto lineRelativeExpected = FlowMode(writingMode, LogicalBoxAxis::Inline, FlowReversal { AxisDirection::Normal, crossReversal });
        EXPECT_EQ(static_cast<WritingMode>(flowRelative), static_cast<WritingMode>(flowRelativeExpected))
            << " for " << writingModeString(writingMode)
            << " lineRelative " << flowModeString(lineRelative)
            << " expected=" << flowModeString(lineRelativeExpected);
    }
}

}

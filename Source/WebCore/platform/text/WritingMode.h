/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 * Copyright (C) 2015, Apple Inc. All rights reserved.
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

#pragma once

#include "TextDirection.h"
#include <array>

namespace WebCore {

enum class TypographicMode : bool {
    Horizontal,
    Vertical,
};

enum class BlockFlowDirection : uint8_t {
    TopToBottom,
    BottomToTop,
    LeftToRight,
    RightToLeft,
};

enum class WritingMode : uint8_t {
    HorizontalTb,
    HorizontalBt, // Non-standard
    VerticalLr,
    VerticalRl,
    SidewaysLr,
    SidewaysRl,
};

constexpr inline BlockFlowDirection writingModeToBlockFlowDirection(WritingMode writingMode)
{
    switch (writingMode) {
    case WritingMode::HorizontalTb:
        return BlockFlowDirection::TopToBottom;
    case WritingMode::HorizontalBt:
        return BlockFlowDirection::BottomToTop;
    case WritingMode::SidewaysLr:
    case WritingMode::VerticalLr:
        return BlockFlowDirection::LeftToRight;
    case WritingMode::SidewaysRl:
    case WritingMode::VerticalRl:
        return BlockFlowDirection::RightToLeft;
    }
    ASSERT_NOT_REACHED();
    return BlockFlowDirection::TopToBottom;
}

constexpr unsigned makeTextFlowInitalizer(BlockFlowDirection blockFlow, TextDirection direction)
{
    return static_cast<unsigned>(blockFlow) << 1 | static_cast<unsigned>(direction);
}

// Define the text flow in terms of the writing mode and the text direction. The first
// part is the block flow direction and the second part is the inline base direction.
enum TextFlow {
    InlineEastBlockSouth = makeTextFlowInitalizer(BlockFlowDirection::TopToBottom, TextDirection::LTR),
    InlineWestBlockSouth = makeTextFlowInitalizer(BlockFlowDirection::TopToBottom, TextDirection::RTL),
    InlineEastBlockNorth = makeTextFlowInitalizer(BlockFlowDirection::BottomToTop, TextDirection::LTR),
    InlineWestBlockNorth = makeTextFlowInitalizer(BlockFlowDirection::BottomToTop, TextDirection::RTL),
    InlineSouthBlockEast = makeTextFlowInitalizer(BlockFlowDirection::LeftToRight, TextDirection::LTR),
    InlineSouthBlockWest = makeTextFlowInitalizer(BlockFlowDirection::LeftToRight, TextDirection::RTL),
    InlineNorthBlockEast = makeTextFlowInitalizer(BlockFlowDirection::RightToLeft, TextDirection::LTR),
    InlineNorthBlockWest = makeTextFlowInitalizer(BlockFlowDirection::RightToLeft, TextDirection::RTL)
};

constexpr inline TextFlow makeTextFlow(WritingMode writingMode, TextDirection direction)
{
    TextDirection inlineBaseDirection = direction;
    if (writingMode == WritingMode::SidewaysLr)
        inlineBaseDirection = direction == TextDirection::RTL ? TextDirection::LTR : TextDirection::RTL;
    return static_cast<TextFlow>(makeTextFlowInitalizer(writingModeToBlockFlowDirection(writingMode), inlineBaseDirection));
}

constexpr unsigned TextFlowReversedMask = 1;
constexpr unsigned TextFlowFlippedMask = 1 << 1;
constexpr unsigned TextFlowVerticalMask = 1 << 2;

constexpr inline bool isReversedTextFlow(TextFlow textflow)
{
    return textflow & TextFlowReversedMask;
}

constexpr inline bool isFlippedTextFlow(TextFlow textflow)
{
    return textflow & TextFlowFlippedMask;
}

constexpr inline bool isVerticalTextFlow(TextFlow textflow)
{
    return textflow & TextFlowVerticalMask;
}

constexpr inline bool isFlippedLinesTextFlow(TextFlow textflow)
{
    return isFlippedTextFlow(textflow) != isVerticalTextFlow(textflow);
}

// Lines have vertical orientation; modes vertical-lr or vertical-rl.
constexpr inline bool isVerticalWritingMode(WritingMode writingMode)
{
    return isVerticalTextFlow(makeTextFlow(writingMode, TextDirection::LTR));
}

// Block progression increases in the opposite direction to normal; modes vertical-rl or horizontal-bt.
constexpr inline bool isFlippedWritingMode(WritingMode writingMode)
{
    return isFlippedTextFlow(makeTextFlow(writingMode, TextDirection::LTR));
}

// Lines have horizontal orientation; modes horizontal-tb or horizontal-bt.
constexpr inline bool isHorizontalWritingMode(WritingMode writingMode)
{
    return !isVerticalWritingMode(writingMode);
}

// Bottom of the line occurs earlier in the block; modes vertical-lr or horizontal-bt.
constexpr inline bool isFlippedLinesWritingMode(WritingMode writingMode)
{
    return isFlippedLinesTextFlow(makeTextFlow(writingMode, TextDirection::LTR));
}

enum class LogicalBoxSide : uint8_t {
    BlockStart,
    InlineEnd,
    BlockEnd,
    InlineStart
};

enum class BoxSide : uint8_t {
    Top,
    Right,
    Bottom,
    Left
};

// The mapping is with the first start/end giving the block axis side,
// and the second the inline-axis side, e.g LogicalBoxCorner::StartEnd is
// the corner between LogicalBoxSide::BlockStart and LogicalBoxSide::InlineEnd.
enum class LogicalBoxCorner : uint8_t {
    StartStart,
    StartEnd,
    EndStart,
    EndEnd
};

enum class BoxCorner : uint8_t {
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft
};

enum class LogicalBoxAxis : uint8_t {
    Inline,
    Block
};

enum class BoxAxis : uint8_t {
    Horizontal,
    Vertical
};

constexpr std::array<BoxSide, 4> allBoxSides = { BoxSide::Top, BoxSide::Right, BoxSide::Bottom, BoxSide::Left };

constexpr BoxSide mapLogicalSideToPhysicalSide(TextFlow textflow, LogicalBoxSide logicalSide)
{
    bool isBlock = logicalSide == LogicalBoxSide::BlockStart || logicalSide == LogicalBoxSide::BlockEnd;
    bool isStart = logicalSide == LogicalBoxSide::BlockStart || logicalSide == LogicalBoxSide::InlineStart;
    bool isNormalStart = isStart != (isBlock ? isFlippedTextFlow(textflow) : isReversedTextFlow(textflow));
    bool isVertical = isBlock != isVerticalTextFlow(textflow);
    if (isVertical)
        return isNormalStart ? BoxSide::Top : BoxSide::Bottom;
    return isNormalStart ? BoxSide::Left : BoxSide::Right;
}

constexpr BoxSide mapLogicalSideToPhysicalSide(WritingMode writingMode, LogicalBoxSide logicalSide)
{
    // Set the direction such that side is mirrored if isFlippedWritingMode() is true
    TextDirection direction = isFlippedWritingMode(writingMode) ? TextDirection::RTL : TextDirection::LTR;
    return mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide);
}

constexpr LogicalBoxSide mapPhysicalSideToLogicalSide(TextFlow textflow, BoxSide side)
{
    bool isNormalStart = side == BoxSide::Top || side == BoxSide::Left;
    bool isVertical = side == BoxSide::Top || side == BoxSide::Bottom;
    bool isBlock = isVertical != isVerticalTextFlow(textflow);
    if (isBlock) {
        bool isBlockStart = isNormalStart != isFlippedTextFlow(textflow);
        return isBlockStart ? LogicalBoxSide::BlockStart : LogicalBoxSide::BlockEnd;
    }
    bool isInlineStart = isNormalStart != isReversedTextFlow(textflow);
    return isInlineStart ? LogicalBoxSide::InlineStart : LogicalBoxSide::InlineEnd;
}

constexpr BoxCorner mapLogicalCornerToPhysicalCorner(TextFlow textflow, LogicalBoxCorner logicalBoxCorner)
{
    bool isBlockStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::StartEnd;
    bool isInlineStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::EndStart;
    bool isNormalBlockStart = isBlockStart != isFlippedTextFlow(textflow);
    bool isNormalInlineStart = isInlineStart != isReversedTextFlow(textflow);
    bool usingVerticalTextFlow = isVerticalTextFlow(textflow);
    bool isTop = usingVerticalTextFlow ? isNormalInlineStart : isNormalBlockStart;
    bool isLeft = usingVerticalTextFlow ? isNormalBlockStart : isNormalInlineStart;
    if (isTop)
        return isLeft ? BoxCorner::TopLeft : BoxCorner::TopRight;
    return isLeft ? BoxCorner::BottomLeft : BoxCorner::BottomRight;
}

constexpr LogicalBoxCorner mapPhysicalCornerToLogicalCorner(TextFlow textflow, BoxCorner boxCorner)
{
    bool isTop = boxCorner == BoxCorner::TopLeft || boxCorner == BoxCorner::TopRight;
    bool isLeft = boxCorner == BoxCorner::TopLeft || boxCorner == BoxCorner::BottomLeft;
    bool usingVerticalTextFlow = isVerticalTextFlow(textflow);
    bool isNormalBlockStart = usingVerticalTextFlow ? isLeft : isTop;
    bool isNormalInlineStart = usingVerticalTextFlow ? isTop : isLeft;
    bool isBlockStart = isNormalBlockStart != isFlippedTextFlow(textflow);
    bool isInlineStart = isNormalInlineStart != isReversedTextFlow(textflow);
    if (isBlockStart)
        return isInlineStart ? LogicalBoxCorner::StartStart : LogicalBoxCorner::StartEnd;
    return isInlineStart ? LogicalBoxCorner::EndStart : LogicalBoxCorner::EndEnd;
}

constexpr BoxAxis mapLogicalAxisToPhysicalAxis(TextFlow textflow, LogicalBoxAxis logicalAxis)
{
    bool isBlock = logicalAxis == LogicalBoxAxis::Block;
    bool isVertical = isBlock != isVerticalTextFlow(textflow);
    return isVertical ? BoxAxis::Vertical : BoxAxis::Horizontal;
}

constexpr LogicalBoxAxis mapPhysicalAxisToLogicalAxis(TextFlow textflow, BoxAxis axis)
{
    bool isVertical = axis == BoxAxis::Vertical;
    bool isBlock = isVertical != isVerticalTextFlow(textflow);
    return isBlock ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline;
}

} // namespace WebCore

/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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

// Define the text flow in terms of the writing mode and the text direction. The first
// part is the block flow direction and the second part is the inline base direction.
struct TextFlow {
    BlockFlowDirection blockDirection;
    TextDirection textDirection;

    constexpr inline bool isReversed()
    {
        return textDirection == TextDirection::RTL;
    }

    constexpr inline bool isFlipped()
    {
        return blockDirection == BlockFlowDirection::BottomToTop
            || blockDirection == BlockFlowDirection::RightToLeft;
    }

    constexpr inline bool isVertical()
    {
        return blockDirection == BlockFlowDirection::LeftToRight
            || blockDirection == BlockFlowDirection::RightToLeft;
    }

    constexpr inline bool isFlippedLines()
    {
        return isFlipped() != isVertical();
    }
};

constexpr inline TextFlow makeTextFlow(WritingMode writingMode, TextDirection direction)
{
    auto textDirection = direction;

    // FIXME: Remove this erronous logic and remove `makeTextFlow` helper (webkit.org/b/276028).
    if (writingMode == WritingMode::SidewaysLr)
        textDirection = direction == TextDirection::RTL ? TextDirection::LTR : TextDirection::RTL;

    return { writingModeToBlockFlowDirection(writingMode), textDirection };
}

// Lines have vertical orientation; modes vertical-lr or vertical-rl.
constexpr inline bool isVerticalWritingMode(WritingMode writingMode)
{
    return makeTextFlow(writingMode, TextDirection::LTR).isVertical();
}

// Block progression increases in the opposite direction to normal; modes vertical-rl or horizontal-bt.
constexpr inline bool isFlippedWritingMode(WritingMode writingMode)
{
    return makeTextFlow(writingMode, TextDirection::LTR).isFlipped();
}

// Lines have horizontal orientation; modes horizontal-tb or horizontal-bt.
constexpr inline bool isHorizontalWritingMode(WritingMode writingMode)
{
    return !isVerticalWritingMode(writingMode);
}

// Bottom of the line occurs earlier in the block; modes vertical-lr or horizontal-bt.
constexpr inline bool isFlippedLinesWritingMode(WritingMode writingMode)
{
    return makeTextFlow(writingMode, TextDirection::LTR).isFlippedLines();
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

constexpr BoxSide mapLogicalSideToPhysicalSide(TextFlow flow, LogicalBoxSide logicalSide)
{
    bool isBlock = logicalSide == LogicalBoxSide::BlockStart || logicalSide == LogicalBoxSide::BlockEnd;
    bool isStart = logicalSide == LogicalBoxSide::BlockStart || logicalSide == LogicalBoxSide::InlineStart;
    bool isNormalStart = isStart != (isBlock ? flow.isFlipped() : flow.isReversed());
    bool isVertical = isBlock != flow.isVertical();
    if (isVertical)
        return isNormalStart ? BoxSide::Top : BoxSide::Bottom;
    return isNormalStart ? BoxSide::Left : BoxSide::Right;
}

constexpr BoxSide mapLogicalSideToPhysicalSide(WritingMode writingMode, LogicalBoxSide logicalSide)
{
    // Set the direction such that side is mirrored if isFlippedWritingMode() is true
    auto direction = isFlippedWritingMode(writingMode) ? TextDirection::RTL : TextDirection::LTR;
    return mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide);
}

constexpr LogicalBoxSide mapPhysicalSideToLogicalSide(TextFlow flow, BoxSide side)
{
    bool isNormalStart = side == BoxSide::Top || side == BoxSide::Left;
    bool isVertical = side == BoxSide::Top || side == BoxSide::Bottom;
    bool isBlock = isVertical != flow.isVertical();
    if (isBlock) {
        bool isBlockStart = isNormalStart != flow.isFlipped();
        return isBlockStart ? LogicalBoxSide::BlockStart : LogicalBoxSide::BlockEnd;
    }
    bool isInlineStart = isNormalStart != flow.isReversed();
    return isInlineStart ? LogicalBoxSide::InlineStart : LogicalBoxSide::InlineEnd;
}

constexpr BoxCorner mapLogicalCornerToPhysicalCorner(TextFlow flow, LogicalBoxCorner logicalBoxCorner)
{
    bool isBlockStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::StartEnd;
    bool isInlineStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::EndStart;
    bool isNormalBlockStart = isBlockStart != flow.isFlipped();
    bool isNormalInlineStart = isInlineStart != flow.isReversed();
    bool usingVerticalTextFlow = flow.isVertical();
    bool isTop = usingVerticalTextFlow ? isNormalInlineStart : isNormalBlockStart;
    bool isLeft = usingVerticalTextFlow ? isNormalBlockStart : isNormalInlineStart;
    if (isTop)
        return isLeft ? BoxCorner::TopLeft : BoxCorner::TopRight;
    return isLeft ? BoxCorner::BottomLeft : BoxCorner::BottomRight;
}

constexpr LogicalBoxCorner mapPhysicalCornerToLogicalCorner(TextFlow flow, BoxCorner boxCorner)
{
    bool isTop = boxCorner == BoxCorner::TopLeft || boxCorner == BoxCorner::TopRight;
    bool isLeft = boxCorner == BoxCorner::TopLeft || boxCorner == BoxCorner::BottomLeft;
    bool usingVerticalTextFlow = flow.isVertical();
    bool isNormalBlockStart = usingVerticalTextFlow ? isLeft : isTop;
    bool isNormalInlineStart = usingVerticalTextFlow ? isTop : isLeft;
    bool isBlockStart = isNormalBlockStart != flow.isFlipped();
    bool isInlineStart = isNormalInlineStart != flow.isReversed();
    if (isBlockStart)
        return isInlineStart ? LogicalBoxCorner::StartStart : LogicalBoxCorner::StartEnd;
    return isInlineStart ? LogicalBoxCorner::EndStart : LogicalBoxCorner::EndEnd;
}

constexpr BoxAxis mapLogicalAxisToPhysicalAxis(TextFlow flow, LogicalBoxAxis logicalAxis)
{
    bool isBlock = logicalAxis == LogicalBoxAxis::Block;
    bool isVertical = isBlock != flow.isVertical();
    return isVertical ? BoxAxis::Vertical : BoxAxis::Horizontal;
}

constexpr LogicalBoxAxis mapPhysicalAxisToLogicalAxis(TextFlow flow, BoxAxis axis)
{
    bool isVertical = axis == BoxAxis::Vertical;
    bool isBlock = isVertical != flow.isVertical();
    return isBlock ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline;
}

inline TextStream& operator<<(TextStream& stream, BlockFlowDirection blockFlow)
{
    switch (blockFlow) {
    case BlockFlowDirection::TopToBottom:
        stream << "top-to-bottom";
        break;
    case BlockFlowDirection::BottomToTop:
        stream << "bottom-to-top";
        break;
    case BlockFlowDirection::LeftToRight:
        stream << "left-to-right";
        break;
    case BlockFlowDirection::RightToLeft:
        stream << "right-to-left";
        break;
    }
    return stream;
}

inline TextStream& operator<<(TextStream& stream, TextFlow flow)
{
    stream << "(" << flow.blockDirection << ", " << flow.textDirection << ")";
    return stream;
}

} // namespace WebCore

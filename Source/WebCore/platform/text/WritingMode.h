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

enum class WritingMode : uint8_t {
    TopToBottom = 0, // horizontal-tb
    BottomToTop = 1, // horizontal-bt
    LeftToRight = 2, // vertical-lr
    RightToLeft = 3, // vertical-rl
};

constexpr unsigned makeTextFlowInitalizer(WritingMode writingMode, TextDirection direction)
{
    return static_cast<unsigned>(writingMode) << 1 | static_cast<unsigned>(direction);
}

// Define the text flow in terms of the writing mode and the text direction. The first
// part is the line growing direction and the second part is the block growing direction.
enum TextFlow {
    InlineEastBlockSouth = makeTextFlowInitalizer(WritingMode::TopToBottom, TextDirection::LTR),
    InlineWestBlockSouth = makeTextFlowInitalizer(WritingMode::TopToBottom, TextDirection::RTL),
    InlineEastBlockNorth = makeTextFlowInitalizer(WritingMode::BottomToTop, TextDirection::LTR),
    InlineWestBlockNorth = makeTextFlowInitalizer(WritingMode::BottomToTop, TextDirection::RTL),
    InlineSouthBlockEast = makeTextFlowInitalizer(WritingMode::LeftToRight, TextDirection::LTR),
    InlineSouthBlockWest = makeTextFlowInitalizer(WritingMode::LeftToRight, TextDirection::RTL),
    InlineNorthBlockEast = makeTextFlowInitalizer(WritingMode::RightToLeft, TextDirection::LTR),
    InlineNorthBlockWest = makeTextFlowInitalizer(WritingMode::RightToLeft, TextDirection::RTL)
};

constexpr inline TextFlow makeTextFlow(WritingMode writingMode, TextDirection direction)
{
    return static_cast<TextFlow>(makeTextFlowInitalizer(writingMode, direction));
}

constexpr unsigned TextFlowReversedMask = 1;
constexpr unsigned TextFlowFlippedMask = 2;
constexpr unsigned TextFlowVerticalMask = 4;

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
    Before,
    End,
    After,
    Start
};

enum class BoxSide : uint8_t {
    Top,
    Right,
    Bottom,
    Left
};

enum class LogicalBoxCorner : uint8_t {
    StartStart,
    StartEnd,
    EndStart,
    EndEnd
};

// The mapping is with the first start/end giving the block axis side,
// and the second the inline-axis side, i.e. patterned as 'border-block-inline-radius'.
enum class BoxCorner : uint8_t {
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft
};

constexpr std::array<BoxSide, 4> allBoxSides = { BoxSide::Top, BoxSide::Right, BoxSide::Bottom, BoxSide::Left };

constexpr inline bool isHorizontalPhysicalSide(BoxSide physicalSide)
{
    return physicalSide == BoxSide::Left || physicalSide == BoxSide::Right;
}

constexpr inline BoxSide mirrorPhysicalSide(BoxSide physicalSide)
{
    // top <-> bottom and left <-> right conversion
    return static_cast<BoxSide>((static_cast<int>(physicalSide) + 2) % 4);
}

constexpr inline BoxSide rotatePhysicalSide(BoxSide physicalSide)
{
    // top <-> left and right <-> bottom conversion
    bool horizontalSide = isHorizontalPhysicalSide(physicalSide);
    return static_cast<BoxSide>((static_cast<int>(physicalSide) + (horizontalSide ? 1 : 3)) % 4);
}

constexpr inline BoxSide mapLogicalSideToPhysicalSide(TextFlow textflow, LogicalBoxSide logicalSide)
{
    BoxSide physicalSide = static_cast<BoxSide>(logicalSide);
    bool horizontalSide = isHorizontalPhysicalSide(physicalSide);

    if (isVerticalTextFlow(textflow))
        physicalSide = rotatePhysicalSide(physicalSide);

    if ((horizontalSide && isReversedTextFlow(textflow)) || (!horizontalSide && isFlippedTextFlow(textflow)))
        physicalSide = mirrorPhysicalSide(physicalSide);

    return physicalSide;
}

constexpr inline BoxSide mapLogicalSideToPhysicalSide(WritingMode writingMode, LogicalBoxSide logicalSide)
{
    // Set the direction such that side is mirrored if isFlippedWritingMode() is true
    TextDirection direction = isFlippedWritingMode(writingMode) ? TextDirection::RTL : TextDirection::LTR;
    return mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide);
}

constexpr inline BoxCorner mapLogicalCornerToPhysicalCorner(TextFlow textflow, LogicalBoxCorner logicalBoxCorner)
{
    bool isBlockStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::StartEnd;
    bool isInlineStart = logicalBoxCorner == LogicalBoxCorner::StartStart || logicalBoxCorner == LogicalBoxCorner::EndStart;
    if (isBlockStart == isFlippedTextFlow(textflow)) {
        if (isInlineStart == isReversedTextFlow(textflow))
            return BoxCorner::BottomRight;
    } else if (isInlineStart != isReversedTextFlow(textflow))
        return BoxCorner::TopLeft;
    if (isBlockStart == isFlippedLinesTextFlow(textflow))
        return BoxCorner::BottomLeft;
    return BoxCorner::TopRight;
}

} // namespace WebCore

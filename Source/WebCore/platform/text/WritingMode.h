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

#include <wtf/EnumTraits.h>

namespace WebCore {

enum class TextDirection : bool { LTR, RTL };

inline bool isLeftToRightDirection(TextDirection direction)
{
    return direction == TextDirection::LTR;
}

enum WritingMode {
    TopToBottomWritingMode = 0, // horizontal-tb
    BottomToTopWritingMode = 1, // horizontal-bt
    LeftToRightWritingMode = 2, // vertical-lr
    RightToLeftWritingMode = 3, // vertical-rl
};

#define MAKE_TEXT_FLOW(writingMode, direction)  ((writingMode) << 1 | static_cast<unsigned>(direction))

// Define the text flow in terms of the writing mode and the text direction. The first
// part is the line growing direction and the second part is the block growing direction.
enum TextFlow {
    InlineEastBlockSouth = MAKE_TEXT_FLOW(TopToBottomWritingMode, TextDirection::LTR),
    InlineWestBlockSouth = MAKE_TEXT_FLOW(TopToBottomWritingMode, TextDirection::RTL),
    InlineEastBlockNorth = MAKE_TEXT_FLOW(BottomToTopWritingMode, TextDirection::LTR),
    InlineWestBlockNorth = MAKE_TEXT_FLOW(BottomToTopWritingMode, TextDirection::RTL),
    InlineSouthBlockEast = MAKE_TEXT_FLOW(LeftToRightWritingMode, TextDirection::LTR),
    InlineSouthBlockWest = MAKE_TEXT_FLOW(LeftToRightWritingMode, TextDirection::RTL),
    InlineNorthBlockEast = MAKE_TEXT_FLOW(RightToLeftWritingMode, TextDirection::LTR),
    InlineNorthBlockWest = MAKE_TEXT_FLOW(RightToLeftWritingMode, TextDirection::RTL)
};

inline TextFlow makeTextFlow(WritingMode writingMode, TextDirection direction)
{
    return static_cast<TextFlow>(MAKE_TEXT_FLOW(writingMode, direction));
}

#undef MAKE_TEXT_FLOW

const unsigned TextFlowReversedMask = 1;
const unsigned TextFlowFlippedMask = 2;
const unsigned TextFlowVerticalMask = 4;

inline bool isReversedTextFlow(TextFlow textflow)
{
    return textflow & TextFlowReversedMask;
}

inline bool isFlippedTextFlow(TextFlow textflow)
{
    return textflow & TextFlowFlippedMask;
}

inline bool isVerticalTextFlow(TextFlow textflow)
{
    return textflow & TextFlowVerticalMask;
}

// Lines have vertical orientation; modes vertical-lr or vertical-rl.
inline bool isVerticalWritingMode(WritingMode writingMode)
{
    return isVerticalTextFlow(makeTextFlow(writingMode, TextDirection::LTR));
}

// Block progression increases in the opposite direction to normal; modes vertical-rl or horizontal-bt.
inline bool isFlippedWritingMode(WritingMode writingMode)
{
    return isFlippedTextFlow(makeTextFlow(writingMode, TextDirection::LTR));
}

// Lines have horizontal orientation; modes horizontal-tb or horizontal-bt.
inline bool isHorizontalWritingMode(WritingMode writingMode)
{
    return !isVerticalWritingMode(writingMode);
}

// Bottom of the line occurs earlier in the block; modes vertical-lr or horizontal-bt.
inline bool isFlippedLinesWritingMode(WritingMode writingMode)
{
    return isVerticalWritingMode(writingMode) != isFlippedWritingMode(writingMode);
}

enum class LogicalBoxSide : uint8_t {
    Before,
    End,
    After,
    Start
};

enum class PhysicalBoxSide : uint8_t {
    Top,
    Right,
    Bottom,
    Left
};

inline bool isHorizontalPhysicalSide(PhysicalBoxSide physicalSide)
{
    return physicalSide == PhysicalBoxSide::Left || physicalSide == PhysicalBoxSide::Right;
}

inline PhysicalBoxSide mirrorPhysicalSide(PhysicalBoxSide physicalSide)
{
    // top <-> bottom and left <-> right conversion
    return static_cast<PhysicalBoxSide>((static_cast<int>(physicalSide) + 2) % 4);
}

inline PhysicalBoxSide rotatePhysicalSide(PhysicalBoxSide physicalSide)
{
    // top <-> left and right <-> bottom conversion
    bool horizontalSide = isHorizontalPhysicalSide(physicalSide);
    return static_cast<PhysicalBoxSide>((static_cast<int>(physicalSide) + (horizontalSide ? 1 : 3)) % 4);
}

inline PhysicalBoxSide mapLogicalSideToPhysicalSide(TextFlow textflow, LogicalBoxSide logicalSide)
{
    PhysicalBoxSide physicalSide = static_cast<PhysicalBoxSide>(logicalSide);
    bool horizontalSide = isHorizontalPhysicalSide(physicalSide);

    if (isVerticalTextFlow(textflow))
        physicalSide = rotatePhysicalSide(physicalSide);

    if ((horizontalSide && isReversedTextFlow(textflow)) || (!horizontalSide && isFlippedTextFlow(textflow)))
        physicalSide = mirrorPhysicalSide(physicalSide);

    return physicalSide;
}

inline PhysicalBoxSide mapLogicalSideToPhysicalSide(WritingMode writingMode, LogicalBoxSide logicalSide)
{
    // Set the direction such that side is mirrored if isFlippedWritingMode() is true
    TextDirection direction = isFlippedWritingMode(writingMode) ? TextDirection::RTL : TextDirection::LTR;
    return mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide);
}

} // namespace WebCore

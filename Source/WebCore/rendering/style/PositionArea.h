/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <WebCore/BoxSides.h>
#include <WebCore/StyleSelfAlignmentData.h>
#include <WebCore/WritingMode.h>

namespace WebCore {

// The axis that the span specifies.
// Represented as 3 bits: axis type, direction type, [Logical]BoxAxis value.
enum class PositionAreaAxis : uint8_t {
    // Physical axes × Physical directions.
    Horizontal = 0b000,
    Vertical   = 0b001,

    // Physical axes × Logical directions.
    X = 0b010,
    Y = 0b011,

    // Logical axes × Logical directions.
    Inline = 0b110,
    Block  = 0b111,
};

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaAxis);

// Get the opposite axis of a given axis.
static inline PositionAreaAxis oppositePositionAreaAxis(PositionAreaAxis axis)
{
    switch (axis) {
    case PositionAreaAxis::Horizontal:
        return PositionAreaAxis::Vertical;
    case PositionAreaAxis::Vertical:
        return PositionAreaAxis::Horizontal;

    case PositionAreaAxis::X:
        return PositionAreaAxis::Y;
    case PositionAreaAxis::Y:
        return PositionAreaAxis::X;

    case PositionAreaAxis::Block:
        return PositionAreaAxis::Inline;
    case PositionAreaAxis::Inline:
        return PositionAreaAxis::Block;
    }

    ASSERT_NOT_REACHED();
    return PositionAreaAxis::Horizontal;
}

static inline bool isPositionAreaAxisLogical(const PositionAreaAxis positionAxis)
{
    static const uint8_t axisBit = 0b100;
    return static_cast<uint8_t>(positionAxis) & axisBit;
}

static inline bool isPositionAreaDirectionLogical(const PositionAreaAxis positionAxis)
{
    static const uint8_t directionBit = 0b010;
    return static_cast<uint8_t>(positionAxis) & directionBit;
}

static inline BoxAxis mapPositionAreaAxisToPhysicalAxis(const PositionAreaAxis positionAxis, const WritingMode writingMode)
{
    static const uint8_t orientationBit = 0b001;
    auto physicalAxis = static_cast<uint8_t>(positionAxis) & orientationBit;
    if (isPositionAreaAxisLogical(positionAxis) && writingMode.isVertical())
        physicalAxis = !physicalAxis;
    return static_cast<BoxAxis>(physicalAxis);
}

static inline LogicalBoxAxis mapPositionAreaAxisToLogicalAxis(const PositionAreaAxis positionAxis, const WritingMode writingMode)
{
    static const uint8_t orientationBit = 0b001;
    auto logicalAxis = static_cast<uint8_t>(positionAxis) & orientationBit;
    if (!isPositionAreaAxisLogical(positionAxis) && writingMode.isVertical())
        logicalAxis = !logicalAxis;
    return static_cast<LogicalBoxAxis>(logicalAxis);
}

// Specifies which tracks(s) on the axis that the position-area span occupies.
// Represented as 3 bits: start track, center track, end track.
enum class PositionAreaTrack : uint8_t {
    Start     = 0b001, // First track.
    SpanStart = 0b011, // First and center tracks.
    End       = 0b100, // Last track.
    SpanEnd   = 0b110, // Center and last track.
    Center    = 0b010, // Center track.
    SpanAll   = 0b111, // All tracks along the axis.
};

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaTrack);

static inline PositionAreaTrack flipPositionAreaTrack(PositionAreaTrack track)
{
    // We need to cast values out of the enum type restrictions in order to do math.
    auto trackBits = static_cast<uint8_t>(track);
    static constexpr uint8_t startBit = static_cast<uint8_t>(PositionAreaTrack::Start);
    static constexpr uint8_t endBit = static_cast<uint8_t>(PositionAreaTrack::End);
    static constexpr uint8_t sideBits = startBit | endBit;

    bool isSymmetric = !(trackBits & startBit) == !(trackBits & endBit);
    auto invertedValue = isSymmetric ? trackBits
        // Flip side bits and merge with not-side bits.
        : ((trackBits & sideBits) ^ sideBits) | (trackBits & ~sideBits);

    return static_cast<PositionAreaTrack>(invertedValue);
}

// When the span refers to a logical axis that needs to be resolved to physical
// axis, this determines whether to use the writing mode of the element's
// containing block or the element itself.
enum class PositionAreaSelf : bool {
    // Use the writing mode of the element's containing block.
    No,

    // Use the writing mode of the element itself.
    Yes
};

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaSelf);

// A span in the position-area. position-area requires two spans of opposite
// axis to determine the containing block area.
//
// A span is uniquely determined by three properties:
// * the axis the span is on
// * which track(s) it occupies
// * "self" - whether to use the writing mode of the element itself or
//   its containing block to resolve logical axes.
//
// How a CSS position-area keyword fits into this model:
// * Every keyword (except start, center, end, span-all) selects a physical
//   or logical axis in PositionAreaAxis. For example, left/right/top/bottom
//   select the physical Horizontal/Vertical axis, x-*/y-*/block-*/inline-*
//   keywords select the logical X/Y/Block/Inline axis.
// * Every keyword also selects the "track", or the tiles on the axis it occupies,
//   in PositionAxisTrack. For example:
//     * left/top selects the Start track.
//     * *-start/*-end keywords selects the Start/End track.
//     * Span keywords select the SpanStart/SpanEnd track.
//     * center/span-all select the Center/SpanAll track.
// * start, center, end, span-all are "axis ambiguous" - its axis depends on the
//   axis of the other keyword in position-area. PositionAreaSpan does not support
//   this; Style::BuilderConverter is responsible for resolving to a concrete axis
//   before creating a PositionAreaSpan.
class PositionAreaSpan {
public:
    PositionAreaSpan(PositionAreaAxis, PositionAreaTrack, PositionAreaSelf);

    PositionAreaAxis axis() const { return static_cast<PositionAreaAxis>(m_axis); }
    PositionAreaTrack track() const { return static_cast<PositionAreaTrack>(m_track); }
    PositionAreaSelf self() const { return static_cast<PositionAreaSelf>(m_self); }

    friend bool operator==(PositionAreaSpan, PositionAreaSpan) = default;

private:
    uint8_t m_axis : 3;
    uint8_t m_track : 3;
    uint8_t m_self : 1;
};

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaSpan);

// A position-area is formed by two spans of opposite axes, that uniquely determine
// the area of the containing block.
class PositionArea {
public:
    PositionArea(PositionAreaSpan blockOrXAxis, PositionAreaSpan inlineOrYAxis);

    PositionAreaSpan blockOrXAxis() const { return m_blockOrXAxis; }
    PositionAreaSpan inlineOrYAxis() const { return m_inlineOrYAxis; }
    PositionAreaSpan spanForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;
    PositionAreaSpan spanForAxis(LogicalBoxAxis logicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    // Start/end based on container's coordinate-increasing direction (RenderBox coordinates)
    PositionAreaTrack coordMatchedTrackForAxis(BoxAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    ItemPosition defaultAlignmentForAxis(BoxAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    bool operator==(const PositionArea&) const = default;

private:
    PositionAreaSpan m_blockOrXAxis;
    PositionAreaSpan m_inlineOrYAxis;
};

WTF::TextStream& operator<<(WTF::TextStream&, PositionArea);

} // namespace WebCore

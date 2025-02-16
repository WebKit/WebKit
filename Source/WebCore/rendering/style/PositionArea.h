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

namespace WebCore {

// The axis that the span specifies.
enum class PositionAreaAxis : uint8_t {
    // Physical axes. Implies self is PositionAreaSelf::No, as physical axes
    // do not depend on writing mode of any elements.
    Horizontal,
    Vertical,

    // Logical axes.
    X,
    Y,
    Block,
    Inline,
};

// Specifies which tile(s) on the axis that the position-area span occupies.
enum class PositionAreaTrack : uint8_t {
    // First tile.
    Start,

    // First and center tiles.
    SpanStart,

    // Last tile.
    End,

    // Center and last tiles.
    SpanEnd,

    // Center tile.
    Center,

    // All tiles on the axis.
    SpanAll,
};

// When the span refers to a logical axis that needs to be resolved to physical
// axis, this determines whether to use the writing mode of the element's
// containing block or the element itself.
enum class PositionAreaSelf : bool {
    // Use the writing mode of the element's containing block.
    No,

    // Use the writing mode of the element itself.
    Yes
};

// A span in the position-area. position-area requires two spans of opposite
// axis to determine the containing block area.
//
// A span is uniquely determined by three properties:
// * the axis the span is on
// * which track it occupies
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

WTF::TextStream& operator<<(WTF::TextStream&, const PositionAreaSpan&);

// A position-area is formed by two spans of opposite axes, that uniquely determine
// the area of the containing block.
class PositionArea {
public:
    PositionArea(PositionAreaSpan blockOrXAxis, PositionAreaSpan inlineOrYAxis);

    PositionAreaSpan blockOrXAxis() const { return m_blockOrXAxis; }
    PositionAreaSpan inlineOrYAxis() const { return m_inlineOrYAxis; }

    bool operator==(const PositionArea&) const = default;

private:
    PositionAreaSpan m_blockOrXAxis;
    PositionAreaSpan m_inlineOrYAxis;
};

WTF::TextStream& operator<<(WTF::TextStream&, const PositionArea&);

} // namespace WebCore

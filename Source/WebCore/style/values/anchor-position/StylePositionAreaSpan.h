/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePositionAreaAxis.h>
#include <WebCore/StylePositionAreaSelf.h>
#include <WebCore/StylePositionAreaTrack.h>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace Style {

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
struct PositionAreaSpan {
    constexpr PositionAreaSpan(PositionAreaAxis axis, PositionAreaTrack track, PositionAreaSelf self)
        : m_axis(enumToUnderlyingType(axis))
        , m_track(enumToUnderlyingType(track))
        , m_self(enumToUnderlyingType(self))
    {
    }

    constexpr PositionAreaAxis axis() const { return static_cast<PositionAreaAxis>(m_axis); }
    constexpr PositionAreaTrack track() const { return static_cast<PositionAreaTrack>(m_track); }
    constexpr PositionAreaSelf self() const { return static_cast<PositionAreaSelf>(m_self); }

    constexpr bool operator==(const PositionAreaSpan&) const = default;

private:
    PREFERRED_TYPE(PositionAreaAxis) uint8_t m_axis : 3;
    PREFERRED_TYPE(PositionAreaTrack) uint8_t m_track : 3;
    PREFERRED_TYPE(PositionAreaSelf) uint8_t m_self : 1;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaSpan);

} // namespace Style
} // namespace WebCore

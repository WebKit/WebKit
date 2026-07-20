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

#pragma once

#include <WebCore/BoxSides.h>
#include <WebCore/WritingMode.h>

namespace WebCore {

struct FlowReversal {
    AxisDirection main { AxisDirection::Normal };
    AxisDirection cross { AxisDirection::Normal };

    constexpr FlowReversal() = default;
    constexpr FlowReversal(AxisDirection mainDirection, AxisDirection crossDirection)
        : main(mainDirection), cross(crossDirection)
        { }

    constexpr bool isMainReversed() const { return main == AxisDirection::Reverse; }
    constexpr bool isCrossReversed() const { return cross == AxisDirection::Reverse; }
};

/**
 * FlowMode represents the primary and secondary flow directions of a layout.
 * Main is the primary placement axis (like inline); cross is the secondary placement axis (like block).
 *
 * FlowMode allows efficient querying of information about the layout flow, and
 * can also be cast to a WritingMode object for use in APIs like marginStart().
 *
 * (It is not, however, an efficient representation for data-packing.)
 */

class FlowMode final {
public:

    FlowMode(WritingMode);
    enum class ConversionStyle { FlowRelative, LineRelative };
    FlowMode(WritingMode, ConversionStyle);
    FlowMode(WritingMode, BoxAxis mainAxis, FlowReversal = { });
    FlowMode(WritingMode, LogicalBoxAxis mainAxis, FlowReversal = { });

    // Axis identification.
    constexpr bool isMainHorizontal() const;
    constexpr bool isMainInline(WritingMode) const;

    // Compare to self writing mode.
    constexpr bool isMainInline() const;
    constexpr bool isMainReverse() const;
    constexpr bool isCrossReverse() const;

    // (Mis)Matching. Pass FlowMode if you want, it'll cast over.
    constexpr bool matches(WritingMode) const;
    constexpr bool isOrthogonal(WritingMode) const;
    constexpr bool isMainOpposing(WritingMode) const;
    constexpr bool isCrossOpposing(WritingMode) const;
    constexpr bool isMainMatchingAny(WritingMode) const;
    constexpr bool isCrossMatchingAny(WritingMode) const;

    // Physical flow directions.
    constexpr bool isMainTopToBottom() const;
    constexpr bool isMainLeftToRight() const;
    constexpr bool isCrossTopToBottom() const;
    constexpr bool isCrossLeftToRight() const;
    // Main OR cross direction is top-to-bottom.
    constexpr bool isAnyTopToBottom() const;
    // Main OR cross direction is left-to-right.
    constexpr bool isAnyLeftToRight() const;

    // Coordinate flow queries.
    constexpr bool isMainFlipped() const; // Main direction is RTL or BTT.
    constexpr bool isCrossFlipped() const; // Cross direction is RTL or BTT.
    constexpr bool isInlineFlipped() const; // Inline axis direction is RTL or BTT.
    constexpr bool isBlockFlipped() const; // Block axis direction is RTL or BTT.

    // Directions as enums. Prefer booleans if doing boolean checks.
    constexpr FlowDirection mainDirection() const;
    constexpr FlowDirection crossDirection() const;

    // Axes as enums. Prefer booleans if doing boolean checks.
    constexpr BoxAxis mainAxis() const;
    constexpr BoxAxis crossAxis() const;

    constexpr operator WritingMode() const { return static_cast<WritingMode::Data>(m_mode.m_bits & ~WritingMode::kFlowModeMask); }
    friend bool operator==(FlowMode, FlowMode) = default;

private:
    // Represents FlowMode as a horizontal- or vertical- WritingMode,
    // letting us re-use logic and cast efficiently.
    WritingMode m_mode;
};

/** Implementation Below **********************************************/

inline FlowMode::FlowMode(WritingMode writingMode)
    : m_mode(writingMode)
{
    // Standardize to sideways- representation of inline flow.
    if (writingMode.isVertical())
        m_mode.m_bits |= WritingMode::kIsVerticalType;
    if (writingMode.computedWritingMode() == StyleWritingMode::SidewaysLr) {
        if (writingMode.isBidiRTL())
            m_mode.m_bits &= ~WritingMode::kIsBidiRTL;
        else
            m_mode.m_bits |= WritingMode::kIsBidiRTL;
    }

    // Only keep the bits we need.
    m_mode.m_bits &= WritingMode::kIsVerticalText | WritingMode::kIsFlippedBlock | WritingMode::kIsVerticalType | WritingMode::kIsBidiRTL;
}

inline FlowMode::FlowMode(WritingMode writingMode, ConversionStyle conversion)
    : FlowMode(writingMode)
{
    if (conversion == ConversionStyle::LineRelative && writingMode.isLineInverted())
        m_mode.m_bits &= ~WritingMode::kIsFlippedBlock | (m_mode.m_bits ^ WritingMode::kIsFlippedBlock);
}

inline FlowMode::FlowMode(WritingMode writingMode, BoxAxis mainAxis, FlowReversal reversal)
    : FlowMode(writingMode, mapAxisPhysicalToLogical(writingMode, mainAxis), reversal)
{ }

inline FlowMode::FlowMode(WritingMode writingMode, LogicalBoxAxis mainAxis, FlowReversal reversal)
{
    if ((mainAxis == LogicalBoxAxis::Inline) == writingMode.isVertical())
        m_mode.m_bits |= WritingMode::kIsVerticalText | WritingMode::kIsVerticalType;

    if (mainAxis == LogicalBoxAxis::Inline) {
        if (writingMode.isInlineFlipped() != reversal.isMainReversed())
            m_mode.m_bits |= WritingMode::kIsBidiRTL;
        if (writingMode.isBlockFlipped() != reversal.isCrossReversed())
            m_mode.m_bits |= WritingMode::kIsFlippedBlock;
    } else {
        m_mode.m_bits |= WritingMode::kIsMainBlock;
        if (writingMode.isBlockFlipped() != reversal.isMainReversed())
            m_mode.m_bits |= WritingMode::kIsBidiRTL;
        if (writingMode.isInlineFlipped() != reversal.isCrossReversed())
            m_mode.m_bits |= WritingMode::kIsFlippedBlock;
    }

    if (reversal.isMainReversed())
        m_mode.m_bits |= WritingMode::kIsMainReverse;
    if (reversal.isCrossReversed())
        m_mode.m_bits |= WritingMode::kIsCrossReverse;
}

constexpr bool FlowMode::isMainHorizontal() const { return m_mode.isHorizontal(); }
constexpr bool FlowMode::isMainInline(WritingMode mode) const { return !m_mode.isOrthogonal(mode); }

constexpr bool FlowMode::isMainInline() const { return !(m_mode.m_bits & WritingMode::kIsMainBlock); }
constexpr bool FlowMode::isMainReverse() const { return m_mode.m_bits & WritingMode::kIsMainReverse; }
constexpr bool FlowMode::isCrossReverse() const { return m_mode.m_bits & WritingMode::kIsCrossReverse; }

constexpr bool FlowMode::matches(WritingMode mode) const { return m_mode.inlineDirection() == mode.inlineDirection() && m_mode.blockDirection() == mode.blockDirection(); }
constexpr bool FlowMode::isOrthogonal(WritingMode mode) const { return m_mode.isOrthogonal(mode); }
constexpr bool FlowMode::isMainOpposing(WritingMode mode) const { return m_mode.isInlineOpposing(mode); }
constexpr bool FlowMode::isCrossOpposing(WritingMode mode) const { return m_mode.isBlockOpposing(mode); }
constexpr bool FlowMode::isMainMatchingAny(WritingMode mode) const { return m_mode.isInlineMatchingAny(mode); }
constexpr bool FlowMode::isCrossMatchingAny(WritingMode mode) const { return m_mode.isBlockMatchingAny(mode); }

constexpr bool FlowMode::isMainTopToBottom() const { return m_mode.isInlineTopToBottom(); }
constexpr bool FlowMode::isMainLeftToRight() const { return m_mode.isInlineLeftToRight(); }
constexpr bool FlowMode::isCrossTopToBottom() const { return m_mode.isBlockTopToBottom(); }
constexpr bool FlowMode::isCrossLeftToRight() const { return m_mode.isBlockLeftToRight(); }
constexpr bool FlowMode::isAnyTopToBottom() const { return m_mode.isAnyTopToBottom(); }
constexpr bool FlowMode::isAnyLeftToRight() const { return m_mode.isAnyLeftToRight(); }

constexpr bool FlowMode::isMainFlipped() const { return m_mode.isInlineFlipped(); }
constexpr bool FlowMode::isCrossFlipped() const { return m_mode.isBlockFlipped(); }
constexpr bool FlowMode::isInlineFlipped() const { return isMainInline() ? m_mode.isInlineFlipped() : m_mode.isBlockFlipped(); }
constexpr bool FlowMode::isBlockFlipped() const { return isMainInline() ? m_mode.isBlockFlipped() : m_mode.isInlineFlipped(); }

constexpr FlowDirection FlowMode::mainDirection() const { return m_mode.inlineDirection(); }
constexpr FlowDirection FlowMode::crossDirection() const { return m_mode.blockDirection(); }

constexpr BoxAxis FlowMode::mainAxis() const { return m_mode.inlineAxis(); }
constexpr BoxAxis FlowMode::crossAxis() const { return m_mode.blockAxis(); }

/** Logging ***********************************************************/

inline TextStream& operator<<(TextStream& ts, AxisDirection direction)
{
    switch (direction) {
    case AxisDirection::Normal: ts << "normal"_s; break;
    case AxisDirection::Reverse: ts << "reverse"_s; break;
    }
    return ts;
}

inline TextStream& operator<<(TextStream& stream, FlowMode flowMode)
{
    stream << "(main=" << flowMode.mainDirection()
        << ", cross=" << flowMode.crossDirection()
        << ", mainReversal=" << (flowMode.isMainReverse() ? AxisDirection::Reverse : AxisDirection::Normal)
        << ", crossReversal=" << (flowMode.isCrossReverse() ? AxisDirection::Reverse : AxisDirection::Normal) << ")";
    return stream;
}

} // namespace WebCore

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

#include <WebCore/StylePositionAreaSpan.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class WritingMode;
enum class BoxAxis : uint8_t;
enum class ItemPosition : uint8_t;
enum class LogicalBoxAxis : uint8_t;

namespace Style {

// <'position-area'> = none | <position-area>
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-area

// A non-`none` position-area is formed by two spans of opposite axes, that uniquely determine
// the area of the containing block.
struct PositionAreaValue {
    PositionAreaValue(PositionAreaSpan blockOrXAxis, PositionAreaSpan inlineOrYAxis);

    constexpr PositionAreaSpan blockOrXAxis() const { return m_blockOrXAxis; }
    constexpr PositionAreaSpan inlineOrYAxis() const { return m_inlineOrYAxis; }

    PositionAreaSpan spanForAxis(BoxAxis physicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;
    PositionAreaSpan spanForAxis(LogicalBoxAxis logicalAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    // Start/end based on container's coordinate-increasing direction (RenderBox coordinates)
    PositionAreaTrack coordMatchedTrackForAxis(BoxAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    ItemPosition defaultAlignmentForAxis(BoxAxis, WritingMode containerWritingMode, WritingMode selfWritingMode) const;

    constexpr bool operator==(const PositionAreaValue&) const = default;

private:
    PositionAreaSpan m_blockOrXAxis;
    PositionAreaSpan m_inlineOrYAxis;
};

struct PositionArea {
    PositionArea(CSS::Keyword::None) : m_value { } { }
    PositionArea(PositionAreaValue value) : m_value { value } { }

    bool isNone() const { return !m_value; }
    bool isValue() const { return !!m_value; }
    std::optional<PositionAreaValue> tryValue() const { return m_value; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(*m_value);
    }

    bool operator==(const PositionArea&) const = default;

private:
    std::optional<PositionAreaValue> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<PositionArea> { auto operator()(BuilderState&, const CSSValue&) -> PositionArea; };
template<> struct CSSValueCreation<PositionAreaValue> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const PositionAreaValue&); };

// MARK: - Serialization

template<> struct Serialize<PositionAreaValue> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const PositionAreaValue&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaValue);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionArea)

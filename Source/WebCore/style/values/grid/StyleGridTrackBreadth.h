/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "StyleLengthWrapper.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// FIXME: Make LengthWrapperBase support additional numeric types in addition to the <length-percentage> one and then replace GridTrackBreadth with that.

struct GridTrackBreadthLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>, CSS::Keyword::MinContent, CSS::Keyword::MaxContent, CSS::Keyword::Auto> {
    using Base::Base;
};

// <track-breadth> = <length-percentage [0,inf]> | <flex [0,inf]> | min-content | max-content | auto
// https://www.w3.org/TR/css-grid-1/#typedef-track-breadth
class GridTrackBreadth {
public:
    using Fixed = GridTrackBreadthLength::Fixed;
    using Percentage = GridTrackBreadthLength::Percentage;
    using Calc = GridTrackBreadthLength::Calc;
    using Flex = Style::Flex<CSS::Nonnegative>;

    GridTrackBreadth(Fixed fixed)
        : m_length(fixed)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(Percentage percent)
        : m_length(percent)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(GridTrackBreadthLength&& length)
        : m_length(WTFMove(length))
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(Flex flex)
        : m_length(0_css_px)
        , m_flex(flex)
        , m_type(GridTrackBreadthType::Flex)
    {
    }

    GridTrackBreadth(CSS::Keyword::MinContent keyword)
        : m_length(keyword)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(CSS::Keyword::MaxContent keyword)
        : m_length(keyword)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(CSS::Keyword::Auto keyword)
        : m_length(keyword)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : m_length(literal)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal)
        : m_length(literal)
        , m_flex(0_css_fr)
        , m_type(GridTrackBreadthType::Length)
    {
    }

    GridTrackBreadth(CSS::ValueLiteral<CSS::FlexUnit::Fr> literal)
        : m_length(0_css_px)
        , m_flex(literal)
        , m_type(GridTrackBreadthType::Flex)
    {
    }

    bool isLength() const { return m_type == GridTrackBreadthType::Length; }
    bool isFlex() const { return m_type == GridTrackBreadthType::Flex; }

    const GridTrackBreadthLength& length() const { ASSERT(isLength()); return m_length; }
    Flex flex() const { ASSERT(isFlex()); return m_flex; }

    bool isPercentOrCalculated() const { return m_type == GridTrackBreadthType::Length && m_length.isPercentOrCalculated(); }
    bool isContentSized() const { return m_type == GridTrackBreadthType::Length && (m_length.isAuto() || m_length.isMinContent() || m_length.isMaxContent()); }
    bool isAuto() const { return m_type == GridTrackBreadthType::Length && m_length.isAuto(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isFlex())
            return visitor(m_flex);
        return WTF::switchOn(m_length, [&](const auto& value) { return visitor(value); });
    }

    bool operator==(const GridTrackBreadth&) const = default;

private:
    GridTrackBreadthLength m_length;
    Flex m_flex;

    enum class GridTrackBreadthType : bool { Length, Flex };
    GridTrackBreadthType m_type;
};

// MARK: - Conversion

template<> struct CSSValueConversion<GridTrackBreadth> { auto operator()(BuilderState&, const CSSPrimitiveValue&) -> GridTrackBreadth; };

// MARK: - Blending

template<> struct Blending<GridTrackBreadth> {
    auto blend(const GridTrackBreadth&, const GridTrackBreadth&, const BlendingContext&) -> GridTrackBreadth;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridTrackBreadthLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridTrackBreadth)

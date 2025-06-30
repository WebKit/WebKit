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

#include "LayoutUnit.h"
#include "StylePrimitiveNumeric.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// <'line-width'> = <length [0,∞]> | thin@(1px) | medium@(3px) | thick@(5px)
// https://drafts.csswg.org/css-backgrounds-3/#typedef-line-width
struct LineWidth {
    Length<CSS::Nonnegative> value;

    constexpr LineWidth(Length<CSS::Nonnegative> value) : value { value } { }
    constexpr LineWidth(CSS::Keyword::Thin) : value { 1_css_px } { }
    constexpr LineWidth(CSS::Keyword::Medium) : value { 3_css_px } { }
    constexpr LineWidth(CSS::Keyword::Thick) : value { 5_css_px } { }

    constexpr LineWidth(CSS::ValueLiteral<CSS::LengthUnit::Px> value) : value { value } { }

    constexpr bool operator==(const LineWidth&) const = default;
    constexpr auto operator<=>(const LineWidth&) const = default;
    constexpr auto operator<=>(const CSS::ValueLiteral<CSS::LengthUnit::Px>& other) { return value <=> other; }
    constexpr auto operator<=>(const LayoutUnit& other) { return value.value <=> other; }
};
DEFINE_TYPE_WRAPPER_GET(LineWidth, value);

// MARK: - Conversion

template<> struct CSSValueConversion<LineWidth> { auto operator()(BuilderState&, const CSSValue&) -> LineWidth; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::LineWidth)

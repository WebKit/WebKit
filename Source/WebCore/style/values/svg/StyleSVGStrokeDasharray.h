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

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

// <dasharray-value> = [ <length-percentage [0,∞]> | <number [0,∞]>@(converted-to-px) ]
struct SVGStrokeDasharrayValueLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>> {
    using Base::Base;
};
struct SVGStrokeDasharrayValue {
    using Fixed = SVGStrokeDasharrayValueLength::Fixed;
    using Percentage = SVGStrokeDasharrayValueLength::Percentage;
    using Calc = SVGStrokeDasharrayValueLength::Calc;

    SVGStrokeDasharrayValueLength value;

    SVGStrokeDasharrayValue(SVGStrokeDasharrayValueLength&& length) : value { WTFMove(length) } { }
    SVGStrokeDasharrayValue(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }
    SVGStrokeDasharrayValue(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : value { literal } { }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const SVGStrokeDasharrayValue&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(SVGStrokeDasharrayValue, value);

// <dasharray> = [ <dasharray-value>+ ]#
using SVGStrokeDasharrayList = CommaSeparatedFixedVector<SVGStrokeDasharrayValue>;

// <'stroke-dasharray'> = none | <dasharray>
// https://svgwg.org/svg2-draft/painting.html#StrokeDashing
struct SVGStrokeDasharray : ListOrNone<SVGStrokeDasharrayList> { using ListOrNone<SVGStrokeDasharrayList>::ListOrNone; };

// MARK: - Conversion

template<> struct CSSValueConversion<SVGStrokeDasharrayValue> { auto operator()(BuilderState&, const CSSValue&) -> SVGStrokeDasharrayValue; };

// MARK: - Blending

template<> struct Blending<SVGStrokeDasharray> {
    auto blend(const SVGStrokeDasharray&, const SVGStrokeDasharray&, const BlendingContext&) -> SVGStrokeDasharray;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::SVGStrokeDasharrayValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGStrokeDasharrayValueLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGStrokeDasharray)

/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSRatio.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// <ratio> = <number [0,∞]> [ / <number [0,∞]> ]?
// https://drafts.csswg.org/css-values-4/#ratio-value
struct Ratio {
    Number<CSS::Nonnegative> numerator;
    Number<CSS::Nonnegative> denominator;

    bool operator==(const Ratio&) const = default;
};

template<size_t I> const auto& get(const Ratio& value)
{
    if constexpr (!I)
        return value.numerator;
    else if constexpr (I == 1)
        return value.denominator;
}

DEFINE_TYPE_MAPPING(CSS::Ratio, Ratio)

// MARK: Conversion

// `Ratio` is special-cased to return a `CSSRatioValue`.
template<> struct CSSValueCreation<Ratio> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const Ratio&); };

} // namespace Style
} // namespace WebCore

DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Ratio, 2)

/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValueKeywords.h>
#include <WebCore/StyleCalculationTree.h>
#include <concepts>
#include <optional>

namespace WebCore {
namespace Style {

class CalcSizeValue;

// A percentage basis prepares to 100%, which is not a keyword.
struct HundredPercentBasis {
    constexpr bool operator==(const HundredPercentBasis&) const = default;
};

// A calc-size() basis after preparing for interpolation, which collapses every basis to one of these so
// that two can be compared.
// https://drafts.csswg.org/css-values-5/#calc-size-prepare-for-interpolation
using PreparedCalcSizeBasis = Variant<
    CSS::Keyword::Any,
    CSS::Keyword::Auto,
    CSS::Keyword::Content,
    CSS::Keyword::MinContent,
    CSS::Keyword::MaxContent,
    CSS::Keyword::FitContent,
    CSS::Keyword::Stretch,
    CSS::Keyword::WebkitFillAvailable,
    CSS::Keyword::Intrinsic,
    CSS::Keyword::MinIntrinsic,
    HundredPercentBasis
>;

struct PreparedCalcSize {
    PreparedCalcSizeBasis basis;
    Calculation::Tree calculation;
};

// Folds the basis into the calculation, leaving it a keyword, `any` or 100%, so that interpolating the
// calculations on their own stays linear. Fails if substitution grows the calculation past the limit.
std::optional<PreparedCalcSize> prepareForInterpolation(const CalcSizeValue&);

// The basis the interpolated value takes, or { } if the two cannot be interpolated at all.
std::optional<PreparedCalcSizeBasis> interpolatedBasis(const PreparedCalcSizeBasis&, const PreparedCalcSizeBasis&);

// The basis a keyword prepares to. Callers switch over a property's keywords, which include ones like
// `none` that calc-size() does not accept as a basis.
template<CSSValueID Id> constexpr std::optional<PreparedCalcSizeBasis> preparedBasisForKeyword(const Constant<Id>& keyword)
{
    if constexpr (std::constructible_from<PreparedCalcSizeBasis, Constant<Id>>)
        return keyword;
    else
        return { };
}

Ref<CalcSizeValue> makeCalcSizeValue(PreparedCalcSizeBasis, Calculation::Tree&&);

} // namespace Style
} // namespace WebCore

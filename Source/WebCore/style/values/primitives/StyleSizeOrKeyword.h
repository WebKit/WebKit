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

#include <WebCore/StyleCalcSizeValue+Evaluation.h>
#include <WebCore/StylePrimitiveNumericOrKeyword.h>

namespace WebCore {
namespace Style {

// Base for the sizing properties, which accept `calc-size()`, `stretch()` and `fit-content()` in addition to a
// <length-percentage> and their keywords. Being a distinct base keeps the handling of those off the
// other <length-percentage>-or-keyword types.
template<CSS::SpecificKeyword... Ks>
struct SizeOrKeyword : PrimitiveNumericOrKeywordOrOptionalCalcSize<LengthPercentage<CSS::NonnegativeLayoutUnitClamped>, CalcSizeSupport::Yes, Ks...> {
    using NumericOrKeyword = PrimitiveNumericOrKeywordOrOptionalCalcSize<LengthPercentage<CSS::NonnegativeLayoutUnitClamped>, CalcSizeSupport::Yes, Ks...>;
    using Base = SizeOrKeyword<Ks...>;

    using NumericOrKeyword::NumericOrKeyword;

    ALWAYS_INLINE bool isCalcSize() const { return this->template holdsAlternative<typename NumericOrKeyword::CalcSize>(); }
    ALWAYS_INLINE UnevaluatedCalcSize calcSize() const { return this->template get<typename NumericOrKeyword::CalcSize>(); }

    // A stretch() is its argument with the bit that it sizes the margin box rather than the box selected by box-sizing,
    // so it answers to its argument as well (isSpecified(), isFixed() and so on): only resolving the size tells them apart.
    ALWAYS_INLINE bool isStretchFunction() const { return this->template holdsAlternative<typename NumericOrKeyword::StretchFunction>(); }

    // A fit-content() answers to isFitContent() as well, since only resolving the size tells it apart.
    ALWAYS_INLINE bool isFitContentFunction() const { return this->template holdsAlternative<typename NumericOrKeyword::FitContentFunction>(); }
    ALWAYS_INLINE typename NumericOrKeyword::Numeric fitContentArgument() const { return NumericOrKeyword::fitContentArgument(); }

    // Specified because a calc-size() resolves to a length, unless its basis is a keyword, in which
    // case it behaves as that keyword.
    ALWAYS_INLINE bool isSpecified() const
    {
        if (isCalcSize())
            SUPPRESS_FORWARD_DECL_ARG return calcSizeBasisKeyword(this->calcSizeValue()) == CSSValueID::Invalid;
        return NumericOrKeyword::isSpecified();
    }

    // A calc-size() resolves against the containing block whenever a percentage appears anywhere in
    // it, so percentage bookkeeping has to see it.
    ALWAYS_INLINE bool isPercentOrCalculated() const
    {
        if (isCalcSize())
            SUPPRESS_FORWARD_DECL_ARG return calcSizeBasisKeyword(this->calcSizeValue()) == CSSValueID::Invalid && calcSizeHasPercentage(this->calcSizeValue());
        return NumericOrKeyword::isPercentOrCalculated();
    }
};

// Written as a refinement of LengthPercentageOrKeywordDerived so that specializations constrained on
// it are more specialized than ones constrained on that, rather than ambiguous with them.
template<typename T> concept SizeOrKeywordDerived = LengthPercentageOrKeywordDerived<T> && WTF::IsBaseOfTemplate<SizeOrKeyword, T>::value;

} // namespace Style
} // namespace WebCore

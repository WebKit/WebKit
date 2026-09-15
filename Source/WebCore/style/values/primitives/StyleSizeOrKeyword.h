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

// Base for the sizing properties, which accept `calc-size()` in addition to a <length-percentage>
// and their keywords. Being a distinct base keeps the calc-size() handling off the other
// <length-percentage>-or-keyword types.
template<CSS::SpecificKeyword... Ks>
struct SizeOrKeyword : PrimitiveNumericOrKeywordOrOptionalCalcSize<LengthPercentage<CSS::NonnegativeLayoutUnitClamped>, CalcSizeSupport::Yes, Ks...> {
    using NumericOrKeyword = PrimitiveNumericOrKeywordOrOptionalCalcSize<LengthPercentage<CSS::NonnegativeLayoutUnitClamped>, CalcSizeSupport::Yes, Ks...>;
    using Base = SizeOrKeyword<Ks...>;

    using NumericOrKeyword::NumericOrKeyword;

    ALWAYS_INLINE bool isCalcSize() const { return this->template holdsAlternative<typename NumericOrKeyword::CalcSize>(); }

    // Specified because a calc-size() resolves to a length, unless its basis is a keyword, in which
    // case it behaves as that keyword.
    ALWAYS_INLINE bool isSpecified() const
    {
        if (isCalcSize())
            SUPPRESS_FORWARD_DECL_ARG return calcSizeBasisKeyword(this->calcSizeValue()) == CSSValueInvalid;
        return NumericOrKeyword::isSpecified();
    }

    // A calc-size() resolves against the containing block whenever a percentage appears anywhere in
    // it, so percentage bookkeeping has to see it.
    ALWAYS_INLINE bool isPercentOrCalculated() const
    {
        if (isCalcSize())
            SUPPRESS_FORWARD_DECL_ARG return calcSizeBasisKeyword(this->calcSizeValue()) == CSSValueInvalid && calcSizeHasPercentage(this->calcSizeValue());
        return NumericOrKeyword::isPercentOrCalculated();
    }
};

template<typename T> concept SizeOrKeywordDerived = WTF::IsBaseOfTemplate<SizeOrKeyword, T>::value && VariantLike<T>;

} // namespace Style
} // namespace WebCore

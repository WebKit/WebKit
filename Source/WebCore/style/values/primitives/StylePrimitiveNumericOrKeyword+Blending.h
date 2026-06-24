/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "StylePrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

template<LengthPercentageOrKeywordDerived StyleType> struct Blending<StyleType> {
    using Numeric = typename StyleType::Numeric;
    using Calc = typename StyleType::Calc;

    auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return a.hasSameType(b) || (WTF::holdsAlternative<Numeric>(a) && WTF::holdsAlternative<Numeric>(b));
    }
    auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b) -> bool
    {
        return !a.hasSameType(b) || WTF::holdsAlternative<Calc>(a) || WTF::holdsAlternative<Calc>(b);
    }
    auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
    {
        if (!WTF::holdsAlternative<Numeric>(a) || !WTF::holdsAlternative<Numeric>(b))
            return context.progress < 0.5 ? a : b;

        return Style::blend(get<Numeric>(a), get<Numeric>(b), context);
    }
};

} // namespace Style
} // namespace WebCore

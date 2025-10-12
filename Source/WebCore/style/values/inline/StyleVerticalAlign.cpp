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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleVerticalAlign.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<VerticalAlign>::operator()(BuilderState& state, const CSSValue& value) -> VerticalAlign
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Baseline { };

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueBaseline:
            return CSS::Keyword::Baseline { };
        case CSSValueSub:
            return CSS::Keyword::Sub { };
        case CSSValueSuper:
            return CSS::Keyword::Super { };
        case CSSValueTop:
            return CSS::Keyword::Top { };
        case CSSValueTextTop:
            return CSS::Keyword::TextTop { };
        case CSSValueMiddle:
            return CSS::Keyword::Middle { };
        case CSSValueBottom:
            return CSS::Keyword::Bottom { };
        case CSSValueTextBottom:
            return CSS::Keyword::TextBottom { };
        case CSSValueWebkitBaselineMiddle:
            return CSS::Keyword::WebkitBaselineMiddle { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Baseline { };
    }

    return toStyleFromCSSValue<VerticalAlignLength>(state, *primitiveValue);
}

// MARK: - Blending

auto Blending<VerticalAlign>::canBlend(const VerticalAlign& a, const VerticalAlign& b) -> bool
{
    return a.m_value.index() == b.m_value.index();
}

auto Blending<VerticalAlign>::requiresInterpolationForAccumulativeIteration(const VerticalAlign& a, const VerticalAlign& b) -> bool
{
    if (a.m_value.index() != b.m_value.index())
        return true;
    if (!a.isLength())
        return false;
    return Style::requiresInterpolationForAccumulativeIteration(*a.tryLength(), *b.tryLength());
}

auto Blending<VerticalAlign>::blend(const VerticalAlign& a, const VerticalAlign& b, const BlendingContext& context) -> VerticalAlign
{
    if (!a.isLength() || !b.isLength())
        return context.progress < 0.5 ? a : b;

    ASSERT(canBlend(a, b));
    return Style::blend(*a.tryLength(), *b.tryLength(), context);
}

} // namespace Style
} // namespace WebCore

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

#include "config.h"
#include "StyleLetterSpacing.h"

#include "FrameDestructionObserverInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<LetterSpacing>::operator()(BuilderState& state, const CSSValue& value) -> LetterSpacing
{
    auto cssToLengthConversionDataWithTextZoomFactor = [](BuilderState& state) -> CSSToLengthConversionData {
        auto zoom = state.zoomWithTextZoomFactor();
        if (zoom == state.cssToLengthConversionData().zoom())
            return state.cssToLengthConversionData();
        return state.cssToLengthConversionData().copyWithAdjustedZoom(zoom, LetterSpacing::Fixed::range.zoomOptions);
    };

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    if (primitiveValue->valueID() == CSSValueNormal)
        return CSS::Keyword::Normal { };

    auto conversionData = state.useSVGZoomRulesForLength()
        ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : cssToLengthConversionDataWithTextZoomFactor(state);

    if (primitiveValue->isLength()) {
        return LetterSpacing {
            typename LetterSpacing::Fixed {
                CSS::clampToRange<LetterSpacing::Fixed::range, float>(primitiveValue->resolveAsLength(conversionData), minValueForCssLength, maxValueForCssLength),
            },
            primitiveValue->primitiveType() == CSSUnitType::CSS_QUIRKY_EM
        };
    }

    if (primitiveValue->isPercentage()) {
        return LetterSpacing {
            typename LetterSpacing::Percentage {
                CSS::clampToRange<LetterSpacing::Percentage::range, float>(primitiveValue->resolveAsPercentage(conversionData)),
            }
        };
    }

    if (primitiveValue->isCalculatedPercentageWithLength()) {
        return LetterSpacing {
            typename LetterSpacing::Calc {
                protect(primitiveValue->cssCalcValue())->createCalculationValue(conversionData, CSSCalcSymbolTable { })
            }
        };
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

} // namespace Style
} // namespace WebCore

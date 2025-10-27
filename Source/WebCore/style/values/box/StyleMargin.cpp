/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StyleMargin.h"

#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderConverter.h"
#include "StyleLengthWrapper+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<MarginEdge>::operator()(BuilderState& state, const CSSValue& value) -> MarginEdge
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    return operator()(state, *primitiveValue);
}

auto CSSValueConversion<MarginEdge>::operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue) -> MarginEdge
{
    if (primitiveValue.valueID() == CSSValueAuto)
        return CSS::Keyword::Auto { };

    auto cssToLengthConversionDataWithTextZoomFactorIfNeeded = [](BuilderState& state, bool isFontIndependentUnit) -> CSSToLengthConversionData {
        auto usedZoom = 1.0f;

        if (isFontIndependentUnit)
            usedZoom = evaluationTimeZoomEnabled(state) ? 1.0f : state.style().usedZoom();
        else
            usedZoom = zoomWithTextZoomFactor(state);

        return state.cssToLengthConversionData().copyWithAdjustedZoom(usedZoom, MarginEdge::Fixed::range.zoomOptions);
    };

    auto conversionData = state.useSVGZoomRulesForLength()
        ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
        : cssToLengthConversionDataWithTextZoomFactorIfNeeded(state, primitiveValue.isFontIndependentLength());

    if (primitiveValue.isLength()) {
        auto textZoom = (evaluationTimeZoomEnabled(state) && !primitiveValue.isFontIndependentLength()) ? conversionData.zoom() : 1.0f;
        return MarginEdge {
            typename MarginEdge::Fixed {
                CSS::clampToRange<MarginEdge::Fixed::range, float>(primitiveValue.resolveAsLength(conversionData) * textZoom, minValueForCssLength, maxValueForCssLength),
            },
            primitiveValue.primitiveType() == CSSUnitType::QuirkyEm
        };
    }

    if (primitiveValue.isPercentage()) {
        return MarginEdge {
            typename MarginEdge::Percentage {
                CSS::clampToRange<MarginEdge::Percentage::range, float>(primitiveValue.resolveAsPercentage(conversionData)),
            }
        };
    }

    if (primitiveValue.isCalculatedPercentageWithLength()) {
        return MarginEdge {
            typename MarginEdge::Calc {
                primitiveValue.protectedCssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })
            }
        };
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Auto { };
}

} // namespace Style
} // namespace WebCore

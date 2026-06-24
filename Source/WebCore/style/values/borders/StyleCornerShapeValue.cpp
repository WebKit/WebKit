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
#include "StyleCornerShapeValue.h"

#include "CSSFunctionValue.h"
#include "CSSKeywordValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<CornerShapeValue>::operator()(BuilderState& state, const CSSValue& value) -> CornerShapeValue
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueRound:
            return CSS::Keyword::Round { };
        case CSSValueScoop:
            return CSS::Keyword::Scoop { };
        case CSSValueBevel:
            return CSS::Keyword::Bevel { };
        case CSSValueNotch:
            return CSS::Keyword::Notch { };
        case CSSValueSquare:
            return CSS::Keyword::Square { };
        case CSSValueSquircle:
            return CSS::Keyword::Squircle { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Round { };
    }

    auto superellipseFunction = requiredFunctionDowncast<CSSValueSuperellipse, CSSValue>(state, value);
    if (!superellipseFunction)
        return CSS::Keyword::Round { };

    Ref superellipseDescriptor = superellipseFunction->item(0);

    // https://drafts.csswg.org/css-borders-4/#typedef-corner-shape-value
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(superellipseDescriptor)) {
        switch (keywordValue->valueID()) {
        case CSSValueInfinity:
            return { SuperellipseFunction { Number<>(std::numeric_limits<double>::infinity()) } };
        case CSSValueNegativeInfinity:
            return { SuperellipseFunction { Number<>(-std::numeric_limits<double>::infinity()) } };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { SuperellipseFunction { Number<>(std::numeric_limits<double>::infinity()) } };
        }
    }

    return { SuperellipseFunction { toStyleFromCSSValue<Number<>>(state, superellipseDescriptor) } };
}

// MARK: - Blending

// https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation

static Number<CSS::Nonnegative> convertCurvatureToInterpolationValue(const CornerShapeValue& cornerShape)
{
    auto curvature = cornerShape.superellipse->value;

    // 1. If curvature is -∞, return 0.
    if (curvature == -std::numeric_limits<double>::infinity())
        return 0.0;

    // 2. If curvature is ∞, return 1.
    if (curvature == std::numeric_limits<double>::infinity())
        return 1.0;

    // 3. Let k be 2^abs(curvature). Let convexHalfCorner be 0.5^(1/k).
    //    If curvature < 0, return 1 - convexHalfCorner. Otherwise return convexHalfCorner.
    auto k = std::exp2(std::abs(curvature));
    auto convexHalfCorner = std::pow(0.5, 1.0 / k);
    if (curvature < 0.0)
        return 1.0 - convexHalfCorner;
    return convexHalfCorner;
}

static CornerShapeValue convertInterpolationValueToCurvature(Number<CSS::Nonnegative> interpolationValue)
{
    auto interp = interpolationValue.value;

    // 1. If interpolationValue is 0, return -∞.
    if (interp <= 0.0)
        return { SuperellipseFunction { -std::numeric_limits<double>::infinity() } };

    // 2. If interpolationValue is 1, return ∞.
    if (interp >= 1.0)
        return { SuperellipseFunction { std::numeric_limits<double>::infinity() } };

    // 3. If interpolationValue is 0.5, return 0.
    if (interp == 0.5)
        return { SuperellipseFunction { 0.0 } };

    // 4. Let k be ln(0.5)/ln(max(interp, 1-interp)), s be log2(k). Return -s if interp < 0.5, else s.
    auto convexHalfCorner = interp >= 0.5 ? interp : 1.0 - interp;
    auto k = std::log(0.5) / std::log(convexHalfCorner);
    auto s = std::log2(k);
    return { SuperellipseFunction { interp < 0.5 ? -s : s } };
}

auto Blending<CornerShapeValue>::blend(const CornerShapeValue& a, const CornerShapeValue& b, const BlendingContext& context) -> CornerShapeValue
{
    auto aInterpolationValue = convertCurvatureToInterpolationValue(a);
    auto bInterpolationValue = convertCurvatureToInterpolationValue(b);

    auto interpolatedValue = Style::blend(aInterpolationValue, bInterpolationValue, context);

    return convertInterpolationValueToCurvature(interpolatedValue);
}

} // namespace Style
} // namespace WebCore

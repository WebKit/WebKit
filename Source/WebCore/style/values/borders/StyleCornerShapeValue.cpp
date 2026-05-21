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
        case CSSValueStraight:
            // Outdated WebKit spelling of `square`; accept and canonicalise.
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

    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(superellipseDescriptor)) {
        switch (keywordValue->valueID()) {
        case CSSValueInfinity:
            return { SuperellipseFunction { Number<CSS::All>(std::numeric_limits<double>::infinity()) } };
        case CSSValueNegativeInfinity:
            return { SuperellipseFunction { Number<CSS::All>(-std::numeric_limits<double>::infinity()) } };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { SuperellipseFunction { Number<CSS::All>(std::numeric_limits<double>::infinity()) } };
        }
    }

    // The function parameter IS the spec `superellipse parameter` (logarithmic);
    // we store it directly. The curve exponent is `pow(2, parameter)` (see
    // CornerShapeValue::exponent()).
    return { SuperellipseFunction { toStyleFromCSSValue<Number<CSS::All>>(state, superellipseDescriptor) } };
}

// MARK: - Blending

// https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation

// Maps a stored superellipse parameter (-∞..+∞) to a normalised interpolation
// value in [0,1], per https://drafts.csswg.org/css-borders-4/#superellipse-interpolation.
// Equivalent to interpValue = pow(0.5, pow(2, -parameter)).
static Number<CSS::Nonnegative> convertParameterToInterpolationValue(const CornerShapeValue& cornerShape)
{
    auto parameter = cornerShape.superellipse->value;
    if (parameter == -std::numeric_limits<double>::infinity())
        return 0.0;
    if (parameter == std::numeric_limits<double>::infinity())
        return 1.0;
    return std::pow(0.5, std::pow(2.0, -parameter));
}

static CornerShapeValue convertInterpolationValueToParameter(Number<CSS::Nonnegative> interpolationValue)
{
    if (interpolationValue.value == 0.0)
        return { SuperellipseFunction { -std::numeric_limits<double>::infinity() } };
    if (interpolationValue.value == 1.0)
        return { SuperellipseFunction { std::numeric_limits<double>::infinity() } };
    // Inverse of pow(0.5, pow(2, -s)): s = log2(log(0.5)/log(interpolationValue)).
    return { SuperellipseFunction { std::log2(std::log(0.5) / std::log(interpolationValue.value)) } };
}

auto Blending<CornerShapeValue>::blend(const CornerShapeValue& a, const CornerShapeValue& b, const BlendingContext& context) -> CornerShapeValue
{
    auto aInterpolationValue = convertParameterToInterpolationValue(a);
    auto bInterpolationValue = convertParameterToInterpolationValue(b);

    auto interpolatedValue = Style::blend(aInterpolationValue, bInterpolationValue, context);

    return convertInterpolationValueToParameter(interpolatedValue);
}

} // namespace Style
} // namespace WebCore

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

#include "config.h"
#include "StylePrimitiveNumericTypes.h"

#include "CSSCalcSymbolTable.h"
#include "CSSPrimitiveValue.h"

namespace WebCore {
namespace Style {

// MARK: Percentage

float evaluate(const Percentage& percentage, float percentResolutionLength)
{
    return percentage.value / 100.0f * percentResolutionLength;
}

// MARK: Angle

Angle canonicalizeNoConversionDataRequired(const CSS::AngleRaw& raw)
{
    return { narrowPrecisionToFloat(CSSPrimitiveValue::computeAngle<CSSPrimitiveValue::AngleUnit::Degrees>(raw.type, raw.value)) };
}

Angle canonicalize(const CSS::AngleRaw& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

// MARK: Length

Length canonicalizeNoConversionDataRequired(const CSS::LengthRaw& raw)
{
    ASSERT(!requiresConversionData(raw));
    return {
        .value = clampTo<float>(
            narrowPrecisionToFloat(CSSPrimitiveValue::computeNonCalcLengthDouble({ }, raw.type, raw.value)),
            minValueForCssLength,
            maxValueForCssLength
        ),
        .quirk = raw.type == CSSUnitType::CSS_QUIRKY_EM
    };
}

Length canonicalize(const CSS::LengthRaw& raw, const CSSToLengthConversionData& conversionData)
{
    return {
        .value = clampTo<float>(
            narrowPrecisionToFloat(CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, raw.type, raw.value)),
            minValueForCssLength,
            maxValueForCssLength
        ),
        .quirk = raw.type == CSSUnitType::CSS_QUIRKY_EM
    };
}

// MARK: Time

Time canonicalizeNoConversionDataRequired(const CSS::TimeRaw& raw)
{
    return { narrowPrecisionToFloat(CSSPrimitiveValue::computeTime<CSSPrimitiveValue::TimeUnit::Seconds>(raw.type, raw.value)) };
}

Time canonicalize(const CSS::TimeRaw& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

// MARK: Frequency

Frequency canonicalizeNoConversionDataRequired(const CSS::FrequencyRaw& raw)
{
    return { narrowPrecisionToFloat(CSSPrimitiveValue::computeFrequency<CSSPrimitiveValue::FrequencyUnit::Hz>(raw.type, raw.value)) };
}

Frequency canonicalize(const CSS::FrequencyRaw& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

// MARK: Resolution

Resolution canonicalizeNoConversionDataRequired(const CSS::ResolutionRaw& raw)
{
    return { narrowPrecisionToFloat(CSSPrimitiveValue::computeResolution<CSSPrimitiveValue::ResolutionUnit::Dppx>(raw.type, raw.value)) };
}

Resolution canonicalize(const CSS::ResolutionRaw& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

// MARK: AnglePercentage

AnglePercentage canonicalizeNoConversionDataRequired(const CSS::AnglePercentageRaw& raw)
{
    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return AnglePercentage { canonicalizeNoConversionDataRequired(CSS::PercentageRaw { raw.value }) };
    return AnglePercentage { canonicalizeNoConversionDataRequired(CSS::AngleRaw { raw.type, raw.value }) };
}

AnglePercentage canonicalize(const CSS::AnglePercentageRaw& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

float evaluate(const AnglePercentage& value, float percentResolutionLength)
{
    return value.value.switchOn(
        [&](Angle angle) -> float {
            return angle.value;
        },
        [&](Percentage percentage) -> float {
            return evaluate(percentage, percentResolutionLength);
        },
        [&](const CalculationValue& calculation) -> float {
            return calculation.evaluate(percentResolutionLength);
        }
    );
}

// MARK: LengthPercentage

LengthPercentage canonicalizeNoConversionDataRequired(const CSS::LengthPercentageRaw& raw)
{
    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return LengthPercentage { canonicalizeNoConversionDataRequired(CSS::PercentageRaw { raw.value }) };
    return LengthPercentage { canonicalizeNoConversionDataRequired(CSS::LengthRaw { raw.type, raw.value }) };
}

LengthPercentage canonicalize(const CSS::LengthPercentageRaw& raw, const CSSToLengthConversionData& conversionData)
{
    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return LengthPercentage { canonicalize(CSS::PercentageRaw { raw.value }, conversionData) };
    return LengthPercentage { canonicalize(CSS::LengthRaw { raw.type, raw.value }, conversionData) };
}

float evaluate(const LengthPercentage& value, float percentResolutionLength)
{
    return value.value.switchOn(
        [&](Length length) -> float {
            return length.value;
        },
        [&](Percentage percentage) -> float {
            return evaluate(percentage, percentResolutionLength);
        },
        [&](const CalculationValue& calculation) -> float {
            return calculation.evaluate(percentResolutionLength);
        }
    );
}

} // namespace Style
} // namespace WebCore

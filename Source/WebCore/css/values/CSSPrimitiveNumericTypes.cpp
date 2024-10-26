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
#include "CSSPrimitiveNumericTypes.h"

#include "CSSPrimitiveValue.h"

namespace WebCore {
namespace CSS {

// MARK: Angle

double canonicalizeAngle(double value, CSSUnitType type)
{
    return CSSPrimitiveValue::computeAngle<CSSPrimitiveValue::AngleUnit::Degrees>(type, value);
}

// MARK: Length

double canonicalizeLengthNoConversionDataRequired(double value, CSSUnitType type)
{
    return CSSPrimitiveValue::computeNonCalcLengthDouble({ }, type, value);
}

double canonicalizeLength(double value, CSSUnitType type, const CSSToLengthConversionData& conversionData)
{
    return CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, type, value);
}

static float clampLengthToAllowedLimits(double value)
{
    return clampTo<float>(narrowPrecisionToFloat(value), minValueForCssLength, maxValueForCssLength);
}

float canonicalizeAndClampLengthNoConversionDataRequired(double value, CSSUnitType type)
{
    return clampLengthToAllowedLimits(canonicalizeLengthNoConversionDataRequired(value, type));
}

float canonicalizeAndClampLength(double value, CSSUnitType type, const CSSToLengthConversionData& conversionData)
{
    return clampLengthToAllowedLimits(canonicalizeLength(value, type, conversionData));
}

// MARK: Time

double canonicalizeTime(double value, CSSUnitType type)
{
    return CSSPrimitiveValue::computeTime<CSSPrimitiveValue::TimeUnit::Seconds>(type, value);
}

// MARK: Frequency

double canonicalizeFrequency(double value, CSSUnitType type)
{
    return CSSPrimitiveValue::computeFrequency<CSSPrimitiveValue::FrequencyUnit::Hz>(type, value);
}

// MARK: Resolution

double canonicalizeResolution(double value, CSSUnitType type)
{
    return CSSPrimitiveValue::computeResolution<CSSPrimitiveValue::ResolutionUnit::Dppx>(type, value);
}

} // namespace CSS
} // namespace WebCore

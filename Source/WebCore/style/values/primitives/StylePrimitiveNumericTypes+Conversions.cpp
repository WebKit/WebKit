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
#include "StylePrimitiveNumericTypes+Conversions.h"

#include "RenderStyleInlines.h"
#include "StyleLengthResolution.h"

namespace WebCore {
namespace Style {

// MARK: Length Canonicalization

double canonicalizeLength(double value, CSS::LengthUnit unit, NoConversionDataRequiredToken)
{
    return computeNonCalcLengthDouble(value, unit, { });
}

double canonicalizeLength(double value, CSS::LengthUnit unit, const CSSToLengthConversionData& conversionData)
{
    return computeNonCalcLengthDouble(value, unit, conversionData);
}

float clampLengthToAllowedLimits(double value)
{
    return clampTo<float>(narrowPrecisionToFloat(value), minValueForCssLength, maxValueForCssLength);
}

float canonicalizeAndClampLength(double value, CSS::LengthUnit unit, NoConversionDataRequiredToken token)
{
    return clampLengthToAllowedLimits(canonicalizeLength(value, unit, token));
}

float canonicalizeAndClampLength(double value, CSS::LengthUnit unit, const CSSToLengthConversionData& conversionData)
{
    return clampLengthToAllowedLimits(canonicalizeLength(value, unit, conversionData));
}

// MARK: ToCSS utilities

float adjustForZoom(float value, const RenderStyle& style)
{
    return adjustFloatForAbsoluteZoom(value, style);
}

} // namespace Style
} // namespace WebCore

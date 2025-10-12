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
#include "StyleSVGGlyphOrientationHorizontal.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include <cmath>

namespace WebCore {
namespace Style {

auto CSSValueConversion<SVGGlyphOrientationHorizontal>::operator()(BuilderState& state, const CSSValue& value) -> SVGGlyphOrientationHorizontal
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return SVGGlyphOrientationHorizontal::Degrees0;

    auto angle = std::abs(std::fmod(toStyleFromCSSValue<Style::Angle<>>(state, *primitiveValue).value, 360.0f));
    if (angle <= 45.0f || angle > 315.0f)
        return SVGGlyphOrientationHorizontal::Degrees0;
    if (angle > 45.0f && angle <= 135.0f)
        return SVGGlyphOrientationHorizontal::Degrees90;
    if (angle > 135.0f && angle <= 225.0f)
        return SVGGlyphOrientationHorizontal::Degrees180;
    return SVGGlyphOrientationHorizontal::Degrees270;
}

} // namespace Style
} // namespace WebCore

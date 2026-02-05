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

#pragma once

#include "CSSPrimitiveNumericRange.h"
#include "StylePrimitiveNumericTypes+Rounding.h"
#include <concepts>

namespace WebCore {

class CSSToLengthConversionData;
class FontCascade;
class RenderStyle;
class RenderView;

enum CSSPropertyID : uint16_t;

namespace CSS {
enum class LengthUnit : uint8_t;
}

namespace Style {

// FIXME: These functions have odd names and invariants and could use improvements.

// NOTE: `computeUnzoomedNonCalcLengthDouble` has the following restrictions:
//
// It can never be called with the following LengthUnits:
//    Lh, Rlh, Cqw, Cqh, Cqi, Cqb, Cqmin, Cqmax (line height, and container-percentage units)
//
// If `fontCascadeForUnit` is nullptr, it additionally cannot be called with the following LengthUnits:
//    Em, QuirkyEm, Ex, Cap, Ch, Ic, Rca, Rc, Re, Re, Ri (font and root font dependent units)
//
// If `RenderView` is nullptr, the following LengthUnits will all cause a return value of zero:
//    Vw, Vh, Vmin, Vmax, Vb, Vi, Svw, Svh, Svmin, Svmax, Svb, Svi, Lvw, Lvh, Lvmin, Lvmax, Lvb, Lvi, Dvw, Dvh, Dvmin, Dvmax, Dvb, Dvi (viewport-percentage units)
double computeUnzoomedNonCalcLengthDouble(double value, CSS::LengthUnit, CSSPropertyID, const FontCascade* fontCascadeForUnit = nullptr, CSS::RangeZoomOptions = CSS::RangeZoomOptions::Default, const RenderView* = nullptr);
double computeNonCalcLengthDouble(double value, CSS::LengthUnit, const CSSToLengthConversionData&);
double computeCanonicalNonCalcLengthDouble(double value, CSS::LengthUnit, const CSSToLengthConversionData&);

// True if `computeNonCalcLengthDouble` would produce identical results when resolved against both these styles.
bool equalForLengthResolution(const RenderStyle&, const RenderStyle&);

// Utilities for common conversions.

double emToPxDouble(double value, const CSSToLengthConversionData&);
double emToPxDouble(double value, const RenderStyle&);

template<typename T> inline T emToPx(double value, const CSSToLengthConversionData& conversionData)
{
    if constexpr (std::floating_point<T>)
        return static_cast<T>(emToPxDouble(value, conversionData));
    else if constexpr (std::integral<T>)
        return roundForImpreciseConversion<T>(emToPxDouble(value, conversionData));
}

template<typename T> inline T emToPx(double value, const RenderStyle& style)
{
    // For `em`, we only need the element's style, so we can overload this to take just a `RenderStyle`.
    if constexpr (std::floating_point<T>)
        return static_cast<T>(emToPxDouble(value, style));
    else if constexpr (std::integral<T>)
        return roundForImpreciseConversion<T>(emToPxDouble(value, style));
}

} // namespace Style
} // namespace WebCore

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

#include "CSSNoConversionDataRequiredToken.h"
#include "CSSPrimitiveNumericRange.h"
#include "StylePrimitiveNumericTypes+Rounding.h"
#include <concepts>

namespace WebCore {

class CSSToLengthConversionData;
class FontCascade;
class RenderView;

enum CSSPropertyID : uint16_t;

namespace CSS {
enum class LengthUnit : uint8_t;
}

namespace Style {

class ComputedStyle;

// Resolves length `value` of the provided `CSS::LengthUnit` type to a length with `CSS::LengthUnit::Px` type.
double resolveLength(double value, CSS::LengthUnit, const CSSToLengthConversionData&);

// Only valid for absolute length units (Px, Cm, Mm, Q, In, Pt, Pc).
double resolveLength(double value, CSS::LengthUnit, NoConversionDataRequiredToken);

// If `RenderView` is nullptr, the following LengthUnits will all cause a return value of zero:
//    Vw, Vh, Vb, Vi, Svw, Svh, Svb, Svi, Lvw, Lvh, Lvb, Lvi, Dvw, Dvh, Dvb, Dvi (width/height/block/inline viewport-percentage units)
// and the following LengthUnits will all cause a return value of `value`:
//    Vmin, Vmax, Svmin, Svmax, Lvmin, Lvmax, Dvmin, Dvmax (min/max viewport-percentage units)
double resolveLength(double value, CSS::LengthUnit, CSSPropertyID, const FontCascade& fontCascadeForUnit, const RenderView*);

// True if `resolveLength` would produce identical results when resolved against both these styles.
bool equalForLengthResolution(const ComputedStyle&, const ComputedStyle&);

// Utilities for common conversions.

double emToPxDouble(double value, const ComputedStyle&);
double emToPxDoubleZoomed(double value, const ComputedStyle&);

template<typename T> inline T emToPx(double value, const ComputedStyle& style)
{
    if constexpr (std::floating_point<T>)
        return static_cast<T>(emToPxDouble(value, style));
    else if constexpr (std::integral<T>)
        return roundForImpreciseConversion<T>(emToPxDouble(value, style));
}

template<typename T> inline T emToPxZoomed(double value, const ComputedStyle& style)
{
    if constexpr (std::floating_point<T>)
        return static_cast<T>(emToPxDoubleZoomed(value, style));
    else if constexpr (std::integral<T>)
        return roundForImpreciseConversion<T>(emToPxDoubleZoomed(value, style));
}

} // namespace Style
} // namespace WebCore

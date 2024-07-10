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

#pragma once

#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "StyleCurrentColor.h"

namespace WebCore {

template<CSSValueID FilterFunction> struct CSSFilterFunctionDescriptor;

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-blur
template<> struct CSSFilterFunctionDescriptor<CSSValueBlur> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = false;

    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = CanonicalLengthRaw { 0 };
    static constexpr auto initialValueForInterpolation = CanonicalLengthRaw { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-brightness
template<> struct CSSFilterFunctionDescriptor<CSSValueBrightness> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-contrast
template<> struct CSSFilterFunctionDescriptor<CSSValueContrast> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-drop-shadow
template<> struct CSSFilterFunctionDescriptor<CSSValueDropShadow> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = false;

    static constexpr auto defaultColorValue = StyleCurrentColor { };
    static constexpr auto defaultStdDeviationValue = CanonicalLengthRaw { 0 };

    static constexpr auto initialColorValueForInterpolation = Color::transparentBlack;
    static constexpr auto initialLengthValueForInterpolation = CanonicalLengthRaw { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-grayscale
template<> struct CSSFilterFunctionDescriptor<CSSValueGrayscale> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-hue-rotate
template<> struct CSSFilterFunctionDescriptor<CSSValueHueRotate> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr auto defaultValue = CanonicalAngleRaw { 0 };
    static constexpr auto initialValueForInterpolation = CanonicalAngleRaw { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-invert
template<> struct CSSFilterFunctionDescriptor<CSSValueInvert> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-invert
template<> struct CSSFilterFunctionDescriptor<CSSValueOpacity> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-saturate
template<> struct CSSFilterFunctionDescriptor<CSSValueSaturate> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-sepia
template<> struct CSSFilterFunctionDescriptor<CSSValueSepia> {
    static constexpr bool isPixelFilterFunction = true;
    static constexpr bool isColorFilterFunction = true;

    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto defaultValue = NumberRaw { 1 };
    static constexpr auto initialValueForInterpolation = NumberRaw { 0 };
};

// Non-standard addition.
template<> struct CSSFilterFunctionDescriptor<CSSValueAppleInvertLightness> {
    static constexpr bool isPixelFilterFunction = false;
    static constexpr bool isColorFilterFunction = true;
};

template<CSSValueID filterFunction> static constexpr bool isPixelFilterFunction()
{
    return CSSFilterFunctionDescriptor<filterFunction>::isPixelFilterFunction;
}

template<CSSValueID filterFunction> static constexpr bool isColorFilterFunction()
{
    return CSSFilterFunctionDescriptor<filterFunction>::isColorFilterFunction;
}

template<CSSValueID filterFunction> static constexpr bool filterFunctionAllowsValuesGreaterThanOne()
{
    return CSSFilterFunctionDescriptor<filterFunction>::allowsValuesGreaterThanOne;
}

template<CSSValueID filterFunction> static constexpr bool filterFunctionAllowsValuesLessThanZero()
{
    return CSSFilterFunctionDescriptor<filterFunction>::allowsValuesLessThanZero;
}

template<CSSValueID filterFunction> static constexpr decltype(auto) filterFunctionDefaultValue()
{
    return CSSFilterFunctionDescriptor<filterFunction>::defaultValue;
}

template<CSSValueID filterFunction> static constexpr decltype(auto) filterFunctionInitialValueForInterpolation()
{
    return CSSFilterFunctionDescriptor<filterFunction>::initialValueForInterpolation;
}

} // namespace WebCore

/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSFontPaletteMix.h"
#include "StyleColorInterpolationMethod.h"
#include "StyleFontPalette.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

struct FontPaletteMixFunction;

namespace Style {

// <palette-mix()> = palette-mix( <color-interpolation-method>? , [ <'font-palette'> && <percentage [0,100]>? ]# )
// https://drafts.csswg.org/css-fonts-4/#funcdef-palette-mix
struct FontPaletteMixParameters {
    struct Component {
        using Percentage = Style::Percentage<CSS::ClosedPercentageRange>;

        FontPalette palette;
        std::optional<Percentage> percentage;

        bool operator==(const Component&) const = default;
    };
    using Components = CommaSeparatedVector<Component>;

    ColorInterpolationMethod colorInterpolationMethod;
    Components components;

    bool operator==(const FontPaletteMixParameters&) const = default;
};

template<size_t I> const auto& get(const FontPaletteMixParameters& value)
{
    if constexpr (!I)
        return value.colorInterpolationMethod;
    else if constexpr (I == 1)
        return value.components;
}

template<size_t I> const auto& get(const FontPaletteMixParameters::Component& value)
{
    if constexpr (!I)
        return value.palette;
    else if constexpr (I == 1)
        return value.percentage;
}

// NOTE: Type wrapper is used here to allow forward declaration of the mix function in `CSSFontPalette.h`.
using FontPaletteMixFunctionValue = FunctionNotation<CSSValuePaletteMix, FontPaletteMixParameters>;
DEFINE_TYPE_WRAPPER(FontPaletteMixFunction, FontPaletteMixFunctionValue);

// Overload of operator== for UniqueRef<FontPaletteFunction> to make FontPalette::Kind's operator== work.
inline bool operator==(const UniqueRef<FontPaletteMixFunction>& a, const UniqueRef<FontPaletteMixFunction>& b)
{
    return arePointingToEqualData(a, b);
}

// MARK: - Conversion

DEFINE_TYPE_MAPPING(CSS::FontPaletteMixParameters::Component, FontPaletteMixParameters::Component);
DEFINE_TYPE_MAPPING(CSS::FontPaletteMixParameters, FontPaletteMixParameters);
DEFINE_TYPE_MAPPING(CSS::FontPaletteMixFunction, FontPaletteMixFunction);

FontPaletteMixFunction fromPlatform(const WebCore::FontPaletteMixFunction&);

// MARK: Serialization

template<> struct Serialize<FontPaletteMixParameters> { void operator()(StringBuilder&, const CSS::SerializationContext&, const ComputedStyle&, const FontPaletteMixParameters&); };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::FontPaletteMixParameters::Component, 2)
DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::FontPaletteMixParameters, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::FontPaletteMixFunction)

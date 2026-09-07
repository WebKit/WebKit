/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleFontPalette.h"
#include "StyleFontPaletteInlines.h"

#include "AnimationUtilities.h"
#include "CSSCustomIdentValue.h"
#include "CSSFontPaletteValue.h"
#include "CSSKeywordValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "FontPaletteMix.h"
#include "StyleBuilderChecking.h"
#include "StyleFontPaletteMix.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

static WebCore::FontPaletteMixFunction toPlatformFontPaletteMixFunction(const CSS::FontPaletteMixParameters& parameters, const BuilderState& state)
{
    auto toPlatformPercentage = [](auto& percentage, auto& state) -> std::optional<double> {
        if (auto stylePercentage = toStyle(percentage, state))
            return stylePercentage->value;
        return std::nullopt;
    };

    return {
        .colorInterpolationMethod = parameters.colorInterpolationMethod.value,
        .components = WTF::map(parameters.components, [&](auto& component) {
            return WebCore::FontPaletteMixFunction::Component {
                .palette = toStyle(component.palette, state).takePlatform(),
                .percentage = toPlatformPercentage(component.percentage, state),
            };
        })
    };
}

auto ToStyle<CSS::FontPalette>::operator()(const CSS::FontPalette& value, const BuilderState& state) -> FontPalette
{
    return WTF::switchOn(value,
        [&](CSS::SpecificKeyword auto const& keyword) -> FontPalette {
            return toStyle(keyword, state);
        },
        [&](const CSS::CustomIdent& customIdent) -> FontPalette {
            return toStyle(customIdent, state);
        },
        [&](const CSS::FontPaletteMixFunction& mix) -> FontPalette {
            // We bypass Style::FontPaletteMixFunction and convert directly to the platform type
            // to avoid an unnecessary extra transient allocation.
            return WebCore::FontPalette { toPlatformFontPaletteMixFunction(mix.value.parameters, state) };
        }
    );
}

auto ToCSS<FontPalette>::operator()(const FontPalette& value, const ComputedStyle& style) -> CSS::FontPalette
{
    return WTF::switchOn(value,
        [&](CSS::SpecificKeyword auto const& keyword) -> CSS::FontPalette {
            return toCSS(keyword, style);
        },
        [&](const CustomIdent& customIdent) -> CSS::FontPalette {
            return toCSS(customIdent, style);
        },
        [&](const FontPaletteMixFunction& mix) -> CSS::FontPalette {
            return toCSS(mix, style);
        }
    );
}

auto CSSValueConversion<FontPalette>::operator()(BuilderState& state, const CSSValue& value) -> FontPalette
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (auto valueID = keywordValue->valueID(); valueID) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueLight:
            return CSS::Keyword::Light { };
        case CSSValueDark:
            return CSS::Keyword::Dark { };
        default:
            if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
                return CSS::Keyword::Normal { };

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    if (auto* customIdentValue = dynamicDowncast<CSSCustomIdentValue>(value))
        return toStyleFromCSSValue<CustomIdent>(state, *customIdentValue);

    RefPtr fontPaletteValue = requiredDowncast<CSSFontPaletteValue>(state, value);
    if (!fontPaletteValue)
        return CSS::Keyword::Normal { };

    return toStyle(fontPaletteValue->fontPalette(), state);
}

Ref<CSSValue> CSSValueCreation<FontPalette>::operator()(CSSValuePool&, const ComputedStyle& style, const FontPalette& value)
{
    return CSSFontPaletteValue::create(toCSS(value, style));
}

// MARK: - Blending

auto Blending<FontPalette>::blend(const FontPalette& a, const FontPalette& b, const BlendingContext& context) -> FontPalette
{
    if (context.progress <= 0)
        return a;
    if (context.progress >= 1)
        return b;

    auto aMixPercentage = (1.0 - context.progress) * 100.0;
    auto bMixPercentage = (context.progress) * 100.0;

    return FontPalette {
        WebCore::FontPalette {
            WebCore::FontPaletteMixFunction {
                .colorInterpolationMethod = CSS::defaultInterpolationMethodForPaletteMix.value,
                .components = {
                    WebCore::FontPaletteMixFunction::Component {
                        .palette = a.platform(),
                        .percentage = aMixPercentage,
                    },
                    WebCore::FontPaletteMixFunction::Component {
                        .palette = b.platform(),
                        .percentage = bMixPercentage,
                    }
                },
            }
        }
    };
}

} // namespace Style
} // namespace WebCore

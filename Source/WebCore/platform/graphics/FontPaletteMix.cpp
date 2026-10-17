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
#include "FontPaletteMix.h"

#include "CSSNormalizedMixPercentages.h"
#include "ColorInterpolation.h"
#include "FontPaletteValues.h"
#include <ranges>
#include <wtf/Hasher.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static Vector<Color> colorsForBasePalette(const OpenTypeCPAL& colorPalettes, std::optional<FontPaletteIndex> basePalette)
{
    if (basePalette) {
        switch (basePalette->type) {
        case FontPaletteIndex::Type::Light:
            return colorPalettes.colorsForLightBackground();
        case FontPaletteIndex::Type::Dark:
            return colorPalettes.colorsForDarkBackground();
        case FontPaletteIndex::Type::Integer:
            if (basePalette->integer < colorPalettes.numberOfPalettes())
                return colorPalettes.colors(basePalette->integer);
            break;
        }
    }
    return colorPalettes.defaultColors();
}

static Vector<Color> colorsForPalette(const OpenTypeCPAL& colorPalettes, const FontPalette& palette, const FontPaletteValues* fontPaletteValues)
{
    return palette.switchOn(
        [&](const FontPalette::Keyword& keyword) {
            switch (keyword) {
            case FontPalette::Keyword::Normal:
                break;
            case FontPalette::Keyword::Light:
                return colorsForBasePalette(colorPalettes, FontPaletteIndex { FontPaletteIndex::Type::Light });
            case FontPalette::Keyword::Dark:
                return colorsForBasePalette(colorPalettes, FontPaletteIndex { FontPaletteIndex::Type::Dark });
            }
            return colorsForBasePalette(colorPalettes, std::nullopt);
        },
        [&](const AtomString& ident) {
            auto* customPalette = fontPaletteValues ? fontPaletteValues->customPalette(ident) : nullptr;
            auto colors = colorsForBasePalette(colorPalettes, customPalette ? customPalette->basePalette : std::nullopt);
            if (customPalette) {
                for (auto& [index, color] : customPalette->overrideColors) {
                    if (index < colors.size())
                        colors[index] = color;
                }
            }
            return colors;
        },
        [&](const FontPaletteMixFunction&) -> Vector<Color> {
            ASSERT_NOT_REACHED();
            return { };
        }
    );
}

Vector<Color> mixedPaletteColors(const FontPaletteMixFunction& mix, const OpenTypeCPAL& colorPalettes, const FontPaletteValues* fontPaletteValues)
{
    if (mix.components.isEmpty())
        return { };

    // FIXME: Using CSS::normalizedMixPercentages is a layering violation. To solve this, we either need to make WebCore::FontPalette a virtual base class for a concrete type in the Style layer, or move Font/FontCascade from platform to Style.

    // https://drafts.csswg.org/css-color-5/#color-mix-result
    // The same percentage normalization and pairwise interpolation as color-mix() is used.

    struct InputMixPercentage {
        struct Percentage {
            double value;
        };
        std::optional<Percentage> percentage;
    };
    auto [percentages, leftover] = CSS::normalizedMixPercentages<CSS::ForceNormalization::Yes>(mix.components | std::views::transform([](const auto& component) {
        return InputMixPercentage { .percentage = component.percentage ? std::optional { InputMixPercentage::Percentage { *component.percentage } } : std::nullopt };
    }));

    auto colorsForComponent = [&](const FontPaletteMixFunction::Component& component) -> Vector<Color> {
        return component.palette.switchOn(
            [&](const FontPaletteMixFunction& nestedMix) {
                return mixedPaletteColors(nestedMix, colorPalettes, fontPaletteValues);
            },
            [&](const auto&) {
                return colorsForPalette(colorPalettes, component.palette, fontPaletteValues);
            }
        );
    };

    auto result = colorsForComponent(mix.components[0]);
    auto accumulatedPercentage = percentages[0];

    for (auto [component, percentage] : zippedRange(mix.components.subspan(1), percentages.subspan(1))) {
        auto colors = colorsForComponent(component);

        result.shrink(std::min(result.size(), colors.size()));

        auto combinedPercentage = accumulatedPercentage + percentage;
        auto progressPercentage = combinedPercentage > 0 ? percentage / combinedPercentage : 0.5;

        for (size_t entry = 0; entry < result.size(); ++entry)
            result[entry] = interpolateColors(mix.colorInterpolationMethod, result[entry], 1.0 - progressPercentage, colors[entry], progressPercentage);

        accumulatedPercentage = combinedPercentage;
    }

    if (leftover != 0) {
        for (auto& color : result)
            color = color.colorWithUnresolvedAlphaMultipliedBy(1.0 - (leftover / 100.0));
    }

    return result;
}

void add(Hasher& hasher, const FontPaletteMixFunction::Component& component)
{
    add(hasher, component.palette, component.percentage);
}

void add(Hasher& hasher, const FontPaletteMixFunction& mix)
{
    add(hasher, mix.colorInterpolationMethod, mix.components);
}

TextStream& operator<<(TextStream& ts, const FontPaletteMixFunction::Component& component)
{
    return ts << component.palette << " "_s << component.percentage;
}

TextStream& operator<<(TextStream& ts, const FontPaletteMixFunction& mix)
{
    return ts << "palette-mix("_s << mix.colorInterpolationMethod << ", "_s << mix.components << ")"_s;
}

} // namespace WebCore

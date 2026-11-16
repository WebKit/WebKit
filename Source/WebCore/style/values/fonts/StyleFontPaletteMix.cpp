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
#include "StyleFontPaletteMix.h"

#include "FontPaletteMix.h"
#include "StyleFontPaletteInlines.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

static FontPaletteMixParameters::Component fromPlatform(const WebCore::FontPaletteMixFunction::Component& component)
{
    return {
        .palette = FontPalette { component.palette },
        .percentage = component.percentage ? std::optional { FontPaletteMixParameters::Component::Percentage { *component.percentage } } : std::nullopt,
    };
}

FontPaletteMixFunction fromPlatform(const WebCore::FontPaletteMixFunction& mix)
{
    return FontPaletteMixFunction {
        FontPaletteMixFunctionValue {
            .parameters = FontPaletteMixParameters {
                .colorInterpolationMethod = { mix.colorInterpolationMethod },
                .components = FontPaletteMixParameters::Components::map(mix.components, [](auto& component) {
                    return fromPlatform(component);
                })
            }
        }
    };
}

// MARK: Serialization

void Serialize<FontPaletteMixParameters>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const ComputedStyle& style, const FontPaletteMixParameters& value)
{
    using namespace CSS::Literals;

    if (value.colorInterpolationMethod != CSS::defaultInterpolationMethodForPaletteMix) {
        serializationForCSS(builder, context, style, value.colorInterpolationMethod);
        builder.append(", "_s);
    }

    double specifiedSum = 0;
    size_t numberOfOmittedPercentages = 0;

    double valueToMatch = 100.0 / value.components.size();
    bool canOmitAllPercentages = true;

    for (auto& component : value.components) {
        if (component.percentage) {
            if (component.percentage->value != valueToMatch)
                canOmitAllPercentages = false;
            specifiedSum += component.percentage->value;
        } else
            ++numberOfOmittedPercentages;
    }

    double omittedPercentageValue = 0;
    if (numberOfOmittedPercentages > 0) {
        omittedPercentageValue = (100.0 - specifiedSum) / numberOfOmittedPercentages;
        if (omittedPercentageValue != valueToMatch)
            canOmitAllPercentages = false;
    }

    builder.append(interleave(value.components, [&](auto& builder, auto& component) {
        serializationForCSS(builder, context, style, component.palette);
        if (!canOmitAllPercentages) {
            if (component.percentage) {
                builder.append(' ');
                serializationForCSS(builder, context, style, *component.percentage);
            } else {
                builder.append(' ');
                serializationForCSS(builder, context, style, FontPaletteMixParameters::Component::Percentage { omittedPercentageValue });
            }
        }
    }, ", "_s));
}

} // namespace Style
} // namespace WebCore

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
#include "CSSRelativeAlphaColorResolver.h"

#include "CSSCalcSymbolTable.h"
#include "CSSColorConversion+ToColor.h"
#include "CSSColorConversion+ToTypedColor.h"
#include "CSSPrimitiveNumericTypes+SymbolReplacement.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace CSS {

static WebCore::Color convertToRelativeAlphaResultRepresentation(const WebCore::Color& color)
{
    // `UseColorFunctionSerialization` is set unconditionally due to `alpha()` serialization
    // always using the modern serialization formats.
    auto flags = OptionSet { WebCore::Color::Flags::UseColorFunctionSerialization };
    if (color.isSemantic())
        flags.add(WebCore::Color::Flags::Semantic);

    return color.callOnUnderlyingType([&]<typename ColorType>(const ColorType& underlyingColor) -> WebCore::Color {
        // 8-bit sRGB must be converted to a float based representation to allow alpha() to set values outside
        // that limited precision.
        if constexpr (std::is_same_v<ColorType, SRGBA<uint8_t>>)
            return { convertColor<ExtendedSRGBA<float>>(underlyingColor), flags };
        else
            return { underlyingColor, flags };
    });
}

// https://drafts.csswg.org/css-color-5/#relative-alpha
WebCore::Color resolve(const RelativeAlphaColorResolver& resolver, const CSSToLengthConversionData& conversionData)
{
    using Descriptor = RelativeAlphaColor::Descriptor;

    auto origin = convertToRelativeAlphaResultRepresentation(resolver.origin);
    auto originAlphaUnresolved = origin.unresolvedAlphaAsFloat();

    const CSSCalcSymbolTable constantSymbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::Number, originAlphaUnresolved * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Replace symbol value (e.g. CSSValueAlpha) to its corresponding value.
    auto componentWithUnevaluatedCalc = replaceSymbol(resolver.alpha, constantSymbolTable);

    auto originAlphaResolved = origin.alphaAsFloat();

    const CSSCalcSymbolTable calcSymbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::Number, originAlphaResolved * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Evaluated any calc value to their corresponding channel value.
    auto component = Style::toStyle(componentWithUnevaluatedCalc, conversionData, calcSymbolTable);

    // Normalize value into its numeric form.
    auto alpha = convertToTypeColorComponent<Descriptor, 0>(component);

    // Return origin color with alpha replaced.
    return origin.colorWithAlpha(alpha);
}

WebCore::Color resolveNoConversionDataRequired(const RelativeAlphaColorResolver& resolver)
{
    ASSERT(!requiresConversionData(resolver.alpha));

    using Descriptor = RelativeAlphaColor::Descriptor;

    auto origin = convertToRelativeAlphaResultRepresentation(resolver.origin);
    auto originAlphaUnresolved = origin.unresolvedAlphaAsFloat();

    const CSSCalcSymbolTable constantSymbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::Number, originAlphaUnresolved * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Replace any symbol value (e.g. CSSValueAlpha) with its corresponding value.
    auto componentWithUnevaluatedCalc = replaceSymbol(resolver.alpha, constantSymbolTable);

    auto originAlphaResolved = origin.alphaAsFloat();

    const CSSCalcSymbolTable calcSymbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::Number, originAlphaResolved * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Evaluate any calc value to its corresponding channel value.
    auto component = Style::toStyleNoConversionDataRequired(componentWithUnevaluatedCalc, calcSymbolTable);

    // Normalize value into its numeric form.
    auto alpha = convertToTypeColorComponent<Descriptor, 0>(component);

    // Return origin color with alpha replaced.
    return origin.colorWithAlpha(alpha);
}

} // namespace CSS
} // namespace WebCore

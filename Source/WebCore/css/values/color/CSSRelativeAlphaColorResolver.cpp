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

// https://drafts.csswg.org/css-color-5/#relative-alpha
WebCore::Color resolve(const RelativeAlphaColorResolver& resolver, const CSSToLengthConversionData& conversionData)
{
    using Descriptor = RelativeAlphaColor::Descriptor;

    if (!resolver.alpha)
        return resolver.origin;

    auto originAlpha = resolver.origin.alphaAsFloat();

    const CSSCalcSymbolTable symbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originAlpha * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Replace symbol value (e.g. CSSValueAlpha) to its corresponding value.
    auto componentWithUnevaluatedCalc = replaceSymbol(*resolver.alpha, symbolTable);

    // Evaluated any calc value to their corresponding channel value.
    auto component = Style::toStyle(componentWithUnevaluatedCalc, conversionData, symbolTable);

    // Normalize value into its numeric form.
    auto alpha = convertToTypeColorComponent<Descriptor, 0>(component);

    // Return origin color with alpha replaced.
    return resolver.origin.colorWithAlpha(alpha);
}

WebCore::Color resolveNoConversionDataRequired(const RelativeAlphaColorResolver& resolver)
{
    ASSERT(!requiresConversionData(resolver.alpha));

    using Descriptor = RelativeAlphaColor::Descriptor;

    if (!resolver.alpha)
        return resolver.origin;

    auto originAlpha = resolver.origin.alphaAsFloat();

    const CSSCalcSymbolTable symbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originAlpha * std::get<0>(Descriptor::components).symbolMultiplier },
    };

    // Replace any symbol value (e.g. CSSValueAlpha) with its corresponding value.
    auto componentWithUnevaluatedCalc = replaceSymbol(*resolver.alpha, symbolTable);

    // Evaluate any calc value to its corresponding channel value.
    auto component = Style::toStyleNoConversionDataRequired(componentWithUnevaluatedCalc, symbolTable);

    // Normalize value into its numeric form.
    auto alpha = convertToTypeColorComponent<Descriptor, 0>(component);

    // Return origin color with alpha replaced.
    return resolver.origin.colorWithAlpha(alpha);
}

} // namespace CSS
} // namespace WebCore

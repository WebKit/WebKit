/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSCalcSymbolTable.h"
#include "CSSColorConversion+ToColor.h"
#include "CSSColorConversion+ToTypedColor.h"
#include "CSSColorDescriptors.h"
#include "CSSPrimitiveNumericTypes+SymbolReplacement.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "Color.h"
#include <optional>

namespace WebCore {

template<typename D>
struct CSSRelativeColorResolver {
    using Descriptor = D;

    Color origin;
    CSSColorParseTypeWithCalcAndSymbols<Descriptor> components;
};

template<typename Descriptor>
Color resolve(const CSSRelativeColorResolver<Descriptor>& relative, const CSSToLengthConversionData& conversionData)
{
    auto originColor = relative.origin;
    auto originColorAsColorType = originColor.template toColorTypeLossy<GetColorType<Descriptor>>();
    auto originComponents = asColorComponents(originColorAsColorType.resolved());

    const CSSCalcSymbolTable symbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[0] * std::get<0>(Descriptor::components).symbolMultiplier },
        { std::get<1>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[1] * std::get<1>(Descriptor::components).symbolMultiplier },
        { std::get<2>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[2] * std::get<2>(Descriptor::components).symbolMultiplier },
        { std::get<3>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[3] * std::get<3>(Descriptor::components).symbolMultiplier }
    };

    // Replace any symbol value (e.g. CSSValueR) to their corresponding channel value.
    auto componentsWithUnevaluatedCalc = CSSColorParseTypeWithCalc<Descriptor> {
        CSS::replaceSymbol(std::get<0>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<1>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<2>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<3>(relative.components), symbolTable)
    };

    // Evaluated any calc values to their corresponding channel value.
    auto components = StyleColorParseType<Descriptor> {
        Style::toStyle(std::get<0>(componentsWithUnevaluatedCalc), conversionData, symbolTable),
        Style::toStyle(std::get<1>(componentsWithUnevaluatedCalc), conversionData, symbolTable),
        Style::toStyle(std::get<2>(componentsWithUnevaluatedCalc), conversionData, symbolTable),
        Style::toStyle(std::get<3>(componentsWithUnevaluatedCalc), conversionData, symbolTable)
    };

    // Normalize values into their numeric form, forming a validated typed color.
    auto typedColor = convertToTypedColor<Descriptor>(components, originColorAsColorType.unresolved().alpha);

    // Convert the validated typed color into a `Color`,
    return convertToColor<Descriptor, CSSColorFunctionForm::Relative>(typedColor);
}

// This resolve function should only be called if the components have been checked and don't require conversion data to be resolved.
template<typename Descriptor>
Color resolveNoConversionDataRequired(const CSSRelativeColorResolver<Descriptor>& relative)
{
    ASSERT(!requiresConversionData(relative.components));

    auto originColor = relative.origin;
    auto originColorAsColorType = originColor.template toColorTypeLossy<GetColorType<Descriptor>>();
    auto originComponents = asColorComponents(originColorAsColorType.resolved());

    const CSSCalcSymbolTable symbolTable {
        { std::get<0>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[0] * std::get<0>(Descriptor::components).symbolMultiplier },
        { std::get<1>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[1] * std::get<1>(Descriptor::components).symbolMultiplier },
        { std::get<2>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[2] * std::get<2>(Descriptor::components).symbolMultiplier },
        { std::get<3>(Descriptor::components).symbol, CSSUnitType::CSS_NUMBER, originComponents[3] * std::get<3>(Descriptor::components).symbolMultiplier }
    };

    // Replace any symbol value (e.g. CSSValueR) to their corresponding channel value.
    auto componentsWithUnevaluatedCalc = CSSColorParseTypeWithCalc<Descriptor> {
        CSS::replaceSymbol(std::get<0>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<1>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<2>(relative.components), symbolTable),
        CSS::replaceSymbol(std::get<3>(relative.components), symbolTable)
    };

    // Evaluated any calc values to their corresponding channel value.
    auto components = StyleColorParseType<Descriptor> {
        Style::toStyleNoConversionDataRequired(std::get<0>(componentsWithUnevaluatedCalc), symbolTable),
        Style::toStyleNoConversionDataRequired(std::get<1>(componentsWithUnevaluatedCalc), symbolTable),
        Style::toStyleNoConversionDataRequired(std::get<2>(componentsWithUnevaluatedCalc), symbolTable),
        Style::toStyleNoConversionDataRequired(std::get<3>(componentsWithUnevaluatedCalc), symbolTable)
    };

    // Normalize values into their numeric form, forming a validated typed color.
    auto typedColor = convertToTypedColor<Descriptor>(components, originColorAsColorType.unresolved().alpha);

    // Convert the validated typed color into a `Color`,
    return convertToColor<Descriptor, CSSColorFunctionForm::Relative>(typedColor);
}

} // namespace WebCore

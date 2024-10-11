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

#include "config.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSUnevaluatedCalc.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Style {

// MARK: Conversions to "CSS"

auto ToCSS<Number>::operator()(const Number& value, const RenderStyle&) -> CSS::Number
{
    return { CSS::NumberRaw { value.value } };
}

auto ToCSS<Percentage>::operator()(const Percentage& value, const RenderStyle&) -> CSS::Percentage
{
    return { CSS::PercentageRaw { value.value } };
}

auto ToCSS<AnglePercentage>::operator()(const AnglePercentage& value, const RenderStyle& style) -> CSS::AnglePercentage
{
    return value.value.switchOn(
        [&](Angle angle) -> CSS::AnglePercentage {
            return CSS::AnglePercentageRaw { angle.unit, angle.value };
        },
        [&](Percentage percentage) -> CSS::AnglePercentage {
            return CSS::AnglePercentageRaw { percentage.unit, percentage.value };
        },
        [&](const CalculationValue& calculation) -> CSS::AnglePercentage {
            return CSS::UnevaluatedCalc<CSS::AnglePercentageRaw> { CSSCalcValue::create(calculation, style) };
        }
    );
}

auto ToCSS<LengthPercentage>::operator()(const LengthPercentage& value, const RenderStyle& style) -> CSS::LengthPercentage
{
    return value.value.switchOn(
        [&](Length length) -> CSS::LengthPercentage {
            return CSS::LengthPercentageRaw { length.unit, adjustFloatForAbsoluteZoom(length.value, style) };
        },
        [&](Percentage percentage) -> CSS::LengthPercentage {
            return CSS::LengthPercentageRaw { percentage.unit, percentage.value };
        },
        [&](const CalculationValue& calculation) -> CSS::LengthPercentage {
            return CSS::UnevaluatedCalc<CSS::LengthPercentageRaw> { CSSCalcValue::create(calculation, style) };
        }
    );
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentageRaw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> AnglePercentage
{
    return AnglePercentage { canonicalize(value, conversionData) };
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return AnglePercentage { value.calc->anglePercentageValue(conversionData, symbolTable) };
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentage& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentageRaw& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return (*this)(value, conversionData<CSS::AnglePercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return (*this)(value, conversionData<CSS::AnglePercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentage& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return (*this)(value, conversionData<CSS::AnglePercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentageRaw& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> AnglePercentage
{
    return AnglePercentage { canonicalizeNoConversionDataRequired(value) };
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return AnglePercentage { value.calc->anglePercentageValueNoConversionDataRequired(symbolTable) };
}

auto ToStyle<CSS::AnglePercentage>::operator()(const CSS::AnglePercentage& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> AnglePercentage
{
    return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentageRaw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> LengthPercentage
{
    return LengthPercentage { canonicalize(value, conversionData) };
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return LengthPercentage { value.calc->lengthPercentageValue(conversionData, symbolTable) };
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentage& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentageRaw& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return (*this)(value, conversionData<CSS::LengthPercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return (*this)(value, conversionData<CSS::LengthPercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentage& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return (*this)(value, conversionData<CSS::LengthPercentageRaw>(state), symbolTable);
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentageRaw& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> LengthPercentage
{
    return LengthPercentage { canonicalizeNoConversionDataRequired(value) };
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>& calc, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return LengthPercentage { calc.calc->lengthPercentageValueNoConversionDataRequired(symbolTable) };
}

auto ToStyle<CSS::LengthPercentage>::operator()(const CSS::LengthPercentage& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> LengthPercentage
{
    return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
}

} // namespace CSS
} // namespace WebCore

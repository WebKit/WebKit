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

auto toCSS(const AnglePercentage& value, const RenderStyle& style) -> CSS::AnglePercentage
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

auto toCSS(const LengthPercentage& value, const RenderStyle& style) -> CSS::LengthPercentage
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

auto toCSS(const PercentageOrNumber& value, const RenderStyle&) -> CSS::PercentageOrNumber
{
    return WTF::switchOn(value,
        [](Percentage percentage) { return WebCore::CSS::PercentageOrNumber { CSS::PercentageRaw { percentage.value } }; },
        [](Number number) { return WebCore::CSS::PercentageOrNumber { CSS::NumberRaw { number.value } }; }
    );
}

} // namespace Style

namespace CSS {

Style::AnglePercentage toStyle(const UnevaluatedCalc<AnglePercentageRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->anglePercentageValue(conversionData, symbolTable);
}

Style::AnglePercentage toStyle(const UnevaluatedCalc<AnglePercentageRaw>& calc, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return toStyle(calc, Style::conversionData<AnglePercentageRaw>(state), symbolTable);
}

Style::AnglePercentage toStyleNoConversionDataRequired(const UnevaluatedCalc<AnglePercentageRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->anglePercentageValueNoConversionDataRequired(symbolTable);
}

Style::LengthPercentage toStyle(const UnevaluatedCalc<LengthPercentageRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->lengthPercentageValue(conversionData, symbolTable);
}

Style::LengthPercentage toStyle(const UnevaluatedCalc<LengthPercentageRaw>& calc, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return toStyle(calc, Style::conversionData<LengthPercentageRaw>(state), symbolTable);
}

Style::LengthPercentage toStyleNoConversionDataRequired(const UnevaluatedCalc<LengthPercentageRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->lengthPercentageValueNoConversionDataRequired(symbolTable);
}

} // namespace CSS
} // namespace WebCore

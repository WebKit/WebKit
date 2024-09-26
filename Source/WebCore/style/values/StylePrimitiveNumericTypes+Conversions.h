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

#include "CSSToLengthConversionData.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion Data specialization

template<typename T> struct ConversionDataSpecializer {
    static CSSToLengthConversionData conversionData(BuilderState& state)
    {
        return state.cssToLengthConversionData();
    }
};

template<> struct ConversionDataSpecializer<CSS::LengthRaw> {
    static CSSToLengthConversionData conversionData(BuilderState& state)
    {
        return state.useSVGZoomRulesForLength()
             ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
             : state.cssToLengthConversionData();
    }
};

template<typename T> CSSToLengthConversionData conversionData(BuilderState& state)
{
    return ConversionDataSpecializer<T>::conversionData(state);
}

// MARK: - Conversion from "Style to "CSS"

template<StyleNumeric StylePrimitive> auto toCSS(const StylePrimitive& style, const RenderStyle&) -> typename StylePrimitive::CSS
{
    return typename StylePrimitive::Raw { style.unit, style.value };
}

inline auto toCSS(const Number& style, const RenderStyle&) -> CSS::Number
{
    return CSS::NumberRaw { style.value };
}

inline auto toCSS(const Percentage& style, const RenderStyle&) -> CSS::Percentage
{
    return CSS::PercentageRaw { style.value };
}

auto toCSS(const LengthPercentage&, const RenderStyle&) -> CSS::LengthPercentage;
auto toCSS(const AnglePercentage&, const RenderStyle&) -> CSS::AnglePercentage;
auto toCSS(const PercentageOrNumber&, const RenderStyle&) -> CSS::PercentageOrNumber;

} // namespace Style

namespace CSS {

// MARK: - Conversion from CSS -> Style

// AnglePercentage / LengthPercentage require specialized implementations for calc().
Style::AnglePercentage toStyle(const UnevaluatedCalc<AnglePercentageRaw>&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&);
Style::AnglePercentage toStyle(const UnevaluatedCalc<AnglePercentageRaw>&, Style::BuilderState&, const CSSCalcSymbolTable&);
Style::AnglePercentage toStyleNoConversionDataRequired(const UnevaluatedCalc<AnglePercentageRaw>&, const CSSCalcSymbolTable&);

Style::LengthPercentage toStyle(const UnevaluatedCalc<LengthPercentageRaw>&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&);
Style::LengthPercentage toStyle(const UnevaluatedCalc<LengthPercentageRaw>&, Style::BuilderState&, const CSSCalcSymbolTable&);
Style::LengthPercentage toStyleNoConversionDataRequired(const UnevaluatedCalc<LengthPercentageRaw>&, const CSSCalcSymbolTable&);

// None just needs its trivial implementation.
constexpr Style::None toStyle(CSS::None, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) { return { }; }
constexpr Style::None toStyle(CSS::None, Style::BuilderState&, const CSSCalcSymbolTable&) { return { }; }
constexpr Style::None toStyleNoConversionDataRequired(CSS::None, const CSSCalcSymbolTable&) { return { }; }

template<RawNumeric RawType> decltype(auto) toStyle(const UnevaluatedCalc<RawType>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return TypeTransform::Type::RawToStyle<RawType> { unevaluatedCalcEvaluate(calc.calc, conversionData, symbolTable, RawType::category) };
}

template<RawNumeric RawType> decltype(auto) toStyle(const UnevaluatedCalc<RawType>& calc, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return toStyle(calc, Style::conversionData<RawType>(state), symbolTable);
}

template<RawNumeric RawType> decltype(auto) toStyle(const RawType& css, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&)
{
    return TypeTransform::Type::RawToStyle<RawType> { Style::canonicalize(css, conversionData) };
}

template<RawNumeric RawType> decltype(auto) toStyle(const RawType& css, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return toStyle(css, Style::conversionData<RawType>(state), symbolTable);
}

template<RawNumeric RawType> decltype(auto) toStyle(const PrimitiveNumeric<RawType>& css, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return WTF::switchOn(css.value, [&](const auto& alternative) { return toStyle(alternative, conversionData, symbolTable); });
}

template<RawNumeric RawType> decltype(auto) toStyle(const PrimitiveNumeric<RawType>& css, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return WTF::switchOn(css.value, [&](const auto& alternative) { return toStyle(alternative, state, symbolTable); });
}

template<RawNumeric Raw> decltype(auto) toStyleNoConversionDataRequired(const UnevaluatedCalc<Raw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return TypeTransform::Type::RawToStyle<Raw> { unevaluatedCalcEvaluateNoConversionDataRequired(calc.calc, symbolTable, Raw::category) };
}

template<RawNumeric Raw> decltype(auto) toStyleNoConversionDataRequired(const Raw& css, const CSSCalcSymbolTable&)
{
    return TypeTransform::Type::RawToStyle<Raw> { Style::canonicalizeNoConversionDataRequired(css) };
}

template<RawNumeric Raw> decltype(auto) toStyleNoConversionDataRequired(const PrimitiveNumeric<Raw>& css, const CSSCalcSymbolTable& symbolTable)
{
    return WTF::switchOn(css.value, [&](const auto& alternative) { return toStyleNoConversionDataRequired(alternative, symbolTable); });
}

} // namespace CSS
} // namespace WebCore

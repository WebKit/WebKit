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

// Number / Percentage require specialized implementations due to lack of `type` field.
template<> struct ToCSS<Number> { auto operator()(const Number&, const RenderStyle&) -> CSS::Number; };
template<> struct ToCSS<Percentage> { auto operator()(const Percentage&, const RenderStyle&) -> CSS::Percentage; };

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<> struct ToCSS<AnglePercentage> { auto operator()(const AnglePercentage&, const RenderStyle&) -> CSS::AnglePercentage; };
template<> struct ToCSS<LengthPercentage> { auto operator()(const LengthPercentage&, const RenderStyle&) -> CSS::LengthPercentage; };

// None just needs its trivial implementation.
template<> struct ToCSS<None> { constexpr auto operator()(const None&, const RenderStyle&) -> CSS::None { return { }; } };

// Partial specialization for remaining numeric types.
template<StyleNumeric StylePrimitive> struct ToCSS<StylePrimitive> {
    inline decltype(auto) operator()(const StylePrimitive& value, const RenderStyle&)
    {
        return typename StylePrimitive::CSS { typename StylePrimitive::Raw { value.unit, value.value } };
    }
};

// MARK: - Conversion from CSS -> Style

// Define the CSS (a.k.a. primitive) type the primary representation of `Raw` and `UnevaluatedCalc` types.
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<RawType> { using type = CSS::PrimitiveNumeric<RawType>; };
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<CSS::UnevaluatedCalc<RawType>> { using type = CSS::PrimitiveNumeric<RawType>; };
template<> struct ToPrimaryCSSTypeMapping<CSS::NoneRaw> { using type = CSS::None; };

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<> struct ToStyle<CSS::AnglePercentage> {
    auto operator()(const CSS::AnglePercentageRaw&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::AnglePercentage&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::AnglePercentageRaw&, BuilderState&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>&, BuilderState&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::AnglePercentage&, BuilderState&, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::AnglePercentageRaw&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> AnglePercentage;
    auto operator()(const CSS::AnglePercentage&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> AnglePercentage;
};
template<> struct ToStyle<CSS::LengthPercentage> {
    auto operator()(const CSS::LengthPercentageRaw&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::LengthPercentage&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::LengthPercentageRaw&, BuilderState&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>&, BuilderState&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::LengthPercentage&, BuilderState&, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::LengthPercentageRaw&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::UnevaluatedCalc<CSS::LengthPercentageRaw>&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> LengthPercentage;
    auto operator()(const CSS::LengthPercentage&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> LengthPercentage;
};

// None just needs its trivial implementation.
template<> struct ToStyle<CSS::None> {
    auto operator()(const CSS::None&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> None { return { }; }
    auto operator()(const CSS::None&, BuilderState&, const CSSCalcSymbolTable&) -> None { return { }; }
    auto operator()(const CSS::None&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> None { return { }; }
};

// Partial specialization for remaining numeric types.
template<CSS::RawNumeric RawType> struct ToStyle<CSS::PrimitiveNumeric<RawType>> {
    decltype(auto) operator()(const RawType& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&)
    {
        return CSS::TypeTransform::Type::RawToStyle<RawType> { canonicalize(value, conversionData) };
    }
    decltype(auto) operator()(const CSS::UnevaluatedCalc<RawType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return CSS::TypeTransform::Type::RawToStyle<RawType> { CSS::unevaluatedCalcEvaluate(value.calc, conversionData, symbolTable, RawType::category) };
    }
    decltype(auto) operator()(const CSS::PrimitiveNumeric<RawType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
    }

    decltype(auto) operator()(const RawType& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable)
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }
    decltype(auto) operator()(const CSS::UnevaluatedCalc<RawType>& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable)
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }
    decltype(auto) operator()(const CSS::PrimitiveNumeric<RawType>& value, BuilderState& state, const CSSCalcSymbolTable& symbolTable)
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }

    decltype(auto) operator()(const RawType& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&)
    {
        return CSS::TypeTransform::Type::RawToStyle<RawType> { canonicalizeNoConversionDataRequired(value) };
    }
    decltype(auto) operator()(const CSS::UnevaluatedCalc<RawType>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return CSS::TypeTransform::Type::RawToStyle<RawType> { CSS::unevaluatedCalcEvaluateNoConversionDataRequired(value.calc, symbolTable, RawType::category) };
    }
    decltype(auto) operator()(const CSS::PrimitiveNumeric<RawType>& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable)
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};

} // namespace CSS
} // namespace WebCore

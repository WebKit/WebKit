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
    static CSSToLengthConversionData conversionData(const BuilderState& state)
    {
        return state.cssToLengthConversionData();
    }
};

template<auto R> struct ConversionDataSpecializer<CSS::LengthRaw<R>> {
    static CSSToLengthConversionData conversionData(const BuilderState& state)
    {
        return state.useSVGZoomRulesForLength()
             ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
             : state.cssToLengthConversionData();
    }
};

template<typename T> CSSToLengthConversionData conversionData(const BuilderState& state)
{
    return ConversionDataSpecializer<T>::conversionData(state);
}

// MARK: - Conversion from "Style to "CSS"

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<auto R> struct ToCSS<AnglePercentage<R>> {
    auto operator()(const AnglePercentage<R>& value, const RenderStyle& style) -> CSS::AnglePercentage<R>
    {
        return value.value.switchOn(
            [&](Angle<R> angle) -> CSS::AnglePercentage<R> {
                return CSS::AnglePercentageRaw<R> { angle.unit, angle.value };
            },
            [&](Percentage<R> percentage) -> CSS::AnglePercentage<R> {
                return CSS::AnglePercentageRaw<R> { percentage.unit, percentage.value };
            },
            [&](const CalculationValue& calculation) -> CSS::AnglePercentage<R> {
                return CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R>> { CSSCalcValue::create(calculation, style) };
            }
        );
    }
};

template<auto R> struct ToCSS<LengthPercentage<R>> {
    auto operator()(const LengthPercentage<R>& value, const RenderStyle& style) -> CSS::LengthPercentage<R>
    {
        return value.value.switchOn(
            [&](Length<R> length) -> CSS::LengthPercentage<R> {
                return CSS::LengthPercentageRaw<R> { length.unit, adjustFloatForAbsoluteZoom(length.value, style) };
            },
            [&](Percentage<R> percentage) -> CSS::LengthPercentage<R> {
                return CSS::LengthPercentageRaw<R> { percentage.unit, percentage.value };
            },
            [&](const CalculationValue& calculation) -> CSS::LengthPercentage<R> {
                return CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R>> { CSSCalcValue::create(calculation, style) };
            }
        );
    }
};

// Partial specialization for remaining numeric types.
template<StyleNumeric StylePrimitive> struct ToCSS<StylePrimitive> {
    auto operator()(const StylePrimitive& value, const RenderStyle&) -> typename StylePrimitive::CSS
    {
        return { typename StylePrimitive::Raw { value.unit, value.value } };
    }
};

// None just needs its trivial implementation.
template<> struct ToCSS<None> { constexpr auto operator()(const None&, const RenderStyle&) -> CSS::None { return { }; } };

// MARK: - Conversion from CSS -> Style

// Define the CSS (a.k.a. primitive) type the primary representation of `Raw` and `UnevaluatedCalc` types.
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<RawType> { using type = CSS::PrimitiveNumeric<RawType>; };
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<CSS::UnevaluatedCalc<RawType>> { using type = CSS::PrimitiveNumeric<RawType>; };
template<> struct ToPrimaryCSSTypeMapping<CSS::NoneRaw> { using type = CSS::None; };

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<auto R> struct ToStyle<CSS::AnglePercentage<R>> {
    using From = CSS::AnglePercentage<R>;
    using To = AnglePercentage<R>;

    auto operator()(const typename From::Raw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalize(value, conversionData) };
    }
    auto operator()(const typename From::Calc& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == Calculation::Category::AnglePercentage);

        if (!calc->tree().type.percentHint)
            return { Style::Angle<R> { narrowPrecisionToFloat(calc->doubleValue(conversionData, symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { narrowPrecisionToFloat(calc->doubleValue(conversionData, symbolTable)) } };
        return { calc->createCalculationValue(conversionData, symbolTable) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
    }

    auto operator()(const typename From::Raw& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }
    auto operator()(const typename From::Calc& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }
    auto operator()(const From& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }

    auto operator()(const typename From::Raw& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalizeNoConversionDataRequired(value) };
    }
    auto operator()(const typename From::Calc& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> To
    {
        Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == Calculation::Category::AnglePercentage);

        if (!calc->tree().type.percentHint)
            return { Style::Angle<R> { narrowPrecisionToFloat(calc->doubleValueNoConversionDataRequired(symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { narrowPrecisionToFloat(calc->doubleValueNoConversionDataRequired(symbolTable)) } };
        return { calc->createCalculationValueNoConversionDataRequired(symbolTable) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};
template<auto R> struct ToStyle<CSS::LengthPercentage<R>> {
    using From = CSS::LengthPercentage<R>;
    using To = LengthPercentage<R>;

    auto operator()(const typename From::Raw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalize(value, conversionData) };
    }
    auto operator()(const typename From::Calc& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == Calculation::Category::LengthPercentage);

        if (!calc->tree().type.percentHint)
            return { Style::Length<R> { narrowPrecisionToFloat(calc->doubleValue(conversionData, symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { narrowPrecisionToFloat(calc->doubleValue(conversionData, symbolTable)) } };
        return { calc->createCalculationValue(conversionData, symbolTable) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
    }

    auto operator()(const typename From::Raw& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }
    auto operator()(const typename From::Calc& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }
    auto operator()(const From& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<typename From::Raw>(state), symbolTable);
    }

    auto operator()(const typename From::Raw& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalizeNoConversionDataRequired(value) };
    }
    auto operator()(const typename From::Calc& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> To
    {
            Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == Calculation::Category::LengthPercentage);

        if (!calc->tree().type.percentHint)
            return { Style::Length<R> { narrowPrecisionToFloat(calc->doubleValueNoConversionDataRequired(symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { narrowPrecisionToFloat(calc->doubleValueNoConversionDataRequired(symbolTable)) } };
        return { calc->createCalculationValueNoConversionDataRequired(symbolTable) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};

// Partial specialization for remaining numeric types.
template<CSS::RawNumeric RawType> struct ToStyle<CSS::PrimitiveNumeric<RawType>> {
    using From = CSS::PrimitiveNumeric<RawType>;
    using To = CSS::TypeTransform::Type::RawToStyle<RawType>;

    auto operator()(const typename From::Raw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalize(value, conversionData) };
    }
    auto operator()(const typename From::Calc& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return { CSS::unevaluatedCalcEvaluate(value.calc, conversionData, symbolTable, RawType::category) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
    }

    auto operator()(const typename From::Raw& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }
    auto operator()(const typename From::Calc& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }
    auto operator()(const CSS::PrimitiveNumeric<RawType>& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return (*this)(value, conversionData<RawType>(state), symbolTable);
    }

    auto operator()(const typename From::Raw& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalizeNoConversionDataRequired(value) };
    }
    auto operator()(const typename From::Calc& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return { CSS::unevaluatedCalcEvaluateNoConversionDataRequired(value.calc, symbolTable, RawType::category) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value.value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};

// None just needs its trivial implementation.
template<> struct ToStyle<CSS::None> {
    auto operator()(const CSS::None&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> None { return { }; }
    auto operator()(const CSS::None&, const BuilderState&, const CSSCalcSymbolTable&) -> None { return { }; }
    auto operator()(const CSS::None&, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> None { return { }; }
};

} // namespace Style
} // namespace WebCore

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

#include "CSSPrimitiveNumericTypes+Canonicalization.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSToLengthConversionData.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

template<auto R> constexpr Number<R> canonicalizeNoConversionDataRequired(const CSS::NumberRaw<R>& raw)
{
    return { raw.value };
}

template<auto R> constexpr Number<R> canonicalize(const CSS::NumberRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> constexpr Percentage<R> canonicalizeNoConversionDataRequired(const CSS::PercentageRaw<R>& raw)
{
    return { raw.value };
}

template<auto R> constexpr Percentage<R> canonicalize(const CSS::PercentageRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> Angle<R> canonicalizeNoConversionDataRequired(const CSS::AngleRaw<R>& raw)
{
    return { CSS::canonicalizeAngle(raw.value, raw.type) };
}

template<auto R> Angle<R> canonicalize(const CSS::AngleRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> Length<R> canonicalizeNoConversionDataRequired(const CSS::LengthRaw<R>& raw)
{
    ASSERT(!requiresConversionData(raw));

    return { CSS::canonicalizeAndClampLengthNoConversionDataRequired(raw.value, raw.type) };
}

template<auto R> Length<R> canonicalize(const CSS::LengthRaw<R>& raw, const CSSToLengthConversionData& conversionData)
{
    ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    return { CSS::canonicalizeAndClampLength(raw.value, raw.type, conversionData) };
}

template<auto R> Time<R> canonicalizeNoConversionDataRequired(const CSS::TimeRaw<R>& raw)
{
    return { CSS::canonicalizeTime(raw.value, raw.type) };
}

template<auto R> Time<R> canonicalize(const CSS::TimeRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> Frequency<R> canonicalizeNoConversionDataRequired(const CSS::FrequencyRaw<R>& raw)
{
    return { CSS::canonicalizeFrequency(raw.value, raw.type) };
}

template<auto R> Frequency<R> canonicalize(const CSS::FrequencyRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> Resolution<R> canonicalizeNoConversionDataRequired(const CSS::ResolutionRaw<R>& raw)
{
    return { CSS::canonicalizeResolution(raw.value, raw.type) };
}

template<auto R> Resolution<R> canonicalize(const CSS::ResolutionRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> constexpr Flex<R> canonicalizeNoConversionDataRequired(const CSS::FlexRaw<R>& raw)
{
    return { raw.value };
}

template<auto R> constexpr Flex<R> canonicalize(const CSS::FlexRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> AnglePercentage<R> canonicalizeNoConversionDataRequired(const CSS::AnglePercentageRaw<R>& raw)
{
    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return { canonicalizeNoConversionDataRequired(CSS::PercentageRaw<R> { raw.value }) };
    return { canonicalizeNoConversionDataRequired(CSS::AngleRaw<R> { raw.type, raw.value }) };
}

template<auto R> AnglePercentage<R> canonicalize(const CSS::AnglePercentageRaw<R>& raw, const CSSToLengthConversionData&)
{
    return canonicalizeNoConversionDataRequired(raw);
}

template<auto R> LengthPercentage<R> canonicalizeNoConversionDataRequired(const CSS::LengthPercentageRaw<R>& raw)
{
    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return canonicalizeNoConversionDataRequired(CSS::PercentageRaw<R> { raw.value });

    // NOTE: This uses the non-clamping version length canonicalization to match the behavior of CSSPrimitiveValue::convertToLength().
    return Length<R> { narrowPrecisionToFloat(CSS::canonicalizeLengthNoConversionDataRequired(raw.value, raw.type)) };
}

template<auto R> LengthPercentage<R> canonicalize(const CSS::LengthPercentageRaw<R>& raw, const CSSToLengthConversionData& conversionData)
{
    ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    if (raw.type == CSSUnitType::CSS_PERCENTAGE)
        return canonicalize(CSS::PercentageRaw<R> { raw.value }, conversionData);

    // NOTE: This uses the non-clamping version length canonicalization to match the behavior of CSSPrimitiveValue::convertToLength().
    return Length<R> { narrowPrecisionToFloat(CSS::canonicalizeLength(raw.value, raw.type, conversionData)) };
}

// MARK: - Conversion Data specialization

template<typename T> struct ConversionDataSpecializer {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        return state.cssToLengthConversionData();
    }
};

template<auto R> struct ConversionDataSpecializer<CSS::LengthRaw<R>> {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        return state.useSVGZoomRulesForLength()
             ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
             : state.cssToLengthConversionData();
    }
};

template<typename T> CSSToLengthConversionData conversionData(const BuilderState& state)
{
    return ConversionDataSpecializer<T>{}(state);
}

// MARK: - Conversion from "Style to "CSS"

// Out of line to avoid inclusion of CSSCalcValue.h
Ref<CSSCalcValue> makeCalc(const CalculationValue&, const RenderStyle&);
// Out of line to avoid inclusion of RenderStyleInlines.h
float adjustForZoom(float, const RenderStyle&);

// Length requires a specialized implementation due to zoom adjustment.
template<auto R> struct ToCSS<Length<R>> {
    auto operator()(const Length<R>& value, const RenderStyle& style) -> CSS::Length<R>
    {
        return CSS::LengthRaw<R> { value.unit, adjustForZoom(value.value, style) };
    }
};

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<auto R> struct ToCSS<AnglePercentage<R>> {
    auto operator()(const AnglePercentage<R>& value, const RenderStyle& style) -> CSS::AnglePercentage<R>
    {
        return WTF::switchOn(value,
            [&](Angle<R> angle) -> CSS::AnglePercentage<R> {
                return CSS::AnglePercentageRaw<R> { angle.unit, angle.value };
            },
            [&](Percentage<R> percentage) -> CSS::AnglePercentage<R> {
                return CSS::AnglePercentageRaw<R> { percentage.unit, percentage.value };
            },
            [&](const CalculationValue& calculation) -> CSS::AnglePercentage<R> {
                return CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R>> { makeCalc(calculation, style) };
            }
        );
    }
};

template<auto R> struct ToCSS<LengthPercentage<R>> {
    auto operator()(const LengthPercentage<R>& value, const RenderStyle& style) -> CSS::LengthPercentage<R>
    {
        return WTF::switchOn(value,
            [&](Length<R> length) -> CSS::LengthPercentage<R> {
                return CSS::LengthPercentageRaw<R> { length.unit, adjustForZoom(length.value, style) };
            },
            [&](Percentage<R> percentage) -> CSS::LengthPercentage<R> {
                return CSS::LengthPercentageRaw<R> { percentage.unit, percentage.value };
            },
            [&](const CalculationValue& calculation) -> CSS::LengthPercentage<R> {
                return CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R>> { makeCalc(calculation, style) };
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

// Specialization for NumberOrPercentageResolvedToNumber.
template<auto R> struct ToCSS<NumberOrPercentageResolvedToNumber<R>> {
    auto operator()(const NumberOrPercentageResolvedToNumber<R>& value, const RenderStyle& style) -> CSS::NumberOrPercentageResolvedToNumber<R>
    {
        return { toCSS(value.value, style) };
    }
};

// MARK: - Conversion from CSS -> Style

// Define the CSS (a.k.a. primitive) type the primary representation of `Raw` and `UnevaluatedCalc` types.
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<RawType> { using type = CSS::PrimitiveNumeric<RawType>; };
template<CSS::RawNumeric RawType> struct ToPrimaryCSSTypeMapping<CSS::UnevaluatedCalc<RawType>> { using type = CSS::PrimitiveNumeric<RawType>; };

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

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Angle<R> { calc->doubleValue(conversionData, symbolTable) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValue(conversionData, symbolTable) } };
        return { calc->createCalculationValue(conversionData, symbolTable) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
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

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Angle<R> { calc->doubleValueNoConversionDataRequired(symbolTable) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValueNoConversionDataRequired(symbolTable) } };
        return { calc->createCalculationValueNoConversionDataRequired(symbolTable) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
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

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Length<R> { CSS::clampLengthToAllowedLimits(calc->doubleValue(conversionData, symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValue(conversionData, symbolTable) } };
        return { calc->createCalculationValue(conversionData, symbolTable) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
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

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Length<R> { CSS::clampLengthToAllowedLimits(calc->doubleValueNoConversionDataRequired(symbolTable)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValueNoConversionDataRequired(symbolTable) } };
        return { calc->createCalculationValueNoConversionDataRequired(symbolTable) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};

template<auto R> struct ToStyle<CSS::Length<R>> {
    using From = CSS::Length<R>;
    using To = Length<R>;

    auto operator()(const typename From::Raw& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable&) -> To
    {
        return { canonicalize(value, conversionData) };
    }
    auto operator()(const typename From::Calc& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return { CSS::clampLengthToAllowedLimits(CSS::unevaluatedCalcEvaluate(value.protectedCalc(), conversionData, symbolTable, From::category)) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
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
        return { CSS::clampLengthToAllowedLimits(CSS::unevaluatedCalcEvaluateNoConversionDataRequired(value.protectedCalc(), symbolTable, From::category)) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
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
        return { CSS::unevaluatedCalcEvaluate(value.protectedCalc(), conversionData, symbolTable, From::category) };
    }
    auto operator()(const From& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, conversionData, symbolTable); });
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
        return { CSS::unevaluatedCalcEvaluateNoConversionDataRequired(value.protectedCalc(), symbolTable, From::category) };
    }
    auto operator()(const From& value, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return (*this)(value, token, symbolTable); });
    }
};

// NumberOrPercentageResolvedToNumber, as the name implies, resolves its percentage to a number.
template<auto R> struct ToStyle<CSS::NumberOrPercentageResolvedToNumber<R>> {
    auto operator()(const CSS::NumberOrPercentageResolvedToNumber<R>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> NumberOrPercentageResolvedToNumber<R>
    {
        return WTF::switchOn(value.value,
            [&](CSS::Number<R> number) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyle(number, conversionData, symbolTable) };
            },
            [&](CSS::Percentage<R> percentage) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyle(percentage, conversionData, symbolTable).value / 100.0 };
            }
        );
    }

    auto operator()(const CSS::NumberOrPercentageResolvedToNumber<R>& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> NumberOrPercentageResolvedToNumber<R>
    {
        return WTF::switchOn(value.value,
            [&](CSS::Number<R> number) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyle(number, state, symbolTable) };
            },
            [&](CSS::Percentage<R> percentage) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyle(percentage, state, symbolTable).value / 100.0 };
            }
        );
    }

    auto operator()(const CSS::NumberOrPercentageResolvedToNumber<R>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) -> NumberOrPercentageResolvedToNumber<R>
    {
        return WTF::switchOn(value.value,
            [&](CSS::Number<R> number) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyleNoConversionDataRequired(number, symbolTable) };
            },
            [&](CSS::Percentage<R> percentage) -> NumberOrPercentageResolvedToNumber<R> {
                return { toStyleNoConversionDataRequired(percentage, symbolTable).value / 100.0 };
            }
        );
    }
};

} // namespace Style
} // namespace WebCore

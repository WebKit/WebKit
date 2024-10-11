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

#include "CalculationCategory.h"
#include "CSSUnevaluatedCalc.h"
#include "CSSUnits.h"
#include "CSSValueTypes.h"
#include <wtf/Brigand.h>

namespace WebCore {
namespace CSS {

// Concept for use in generic contexts to filter on *Raw types.
template<typename T> concept RawNumeric = requires(T raw) {
    { raw.type } -> std::convertible_to<CSSUnitType>;
    { raw.value } -> std::convertible_to<double>;
};

// MARK: Number Primitives Raw

enum class IntegerValueRange : uint8_t { All, Positive, NonNegative };
template<typename IntType, IntegerValueRange Range> struct IntegerRaw {
    static constexpr auto category = Calculation::Category::Integer;
    static constexpr auto type = CSSUnitType::CSS_INTEGER;
    IntType value;

    constexpr bool operator==(const IntegerRaw<IntType, Range>&) const = default;
};

struct NumberRaw {
    static constexpr auto category = Calculation::Category::Number;
    static constexpr auto type = CSSUnitType::CSS_NUMBER;
    double value;

    constexpr bool operator==(const NumberRaw&) const = default;
};
template<typename T> struct IsNumberRaw : public std::integral_constant<bool, std::is_same_v<T, NumberRaw>> { };

// MARK: Percentage Primitive Raw

struct PercentageRaw {
    static constexpr auto category = Calculation::Category::Percentage;
    static constexpr auto type = CSSUnitType::CSS_PERCENTAGE;
    double value;

    constexpr bool operator==(const PercentageRaw&) const = default;
};
template<typename T> struct IsPercentageRaw : public std::integral_constant<bool, std::is_same_v<T, PercentageRaw>> { };

// MARK: Dimension Primitives Raw

struct AngleRaw {
    static constexpr auto category = Calculation::Category::Angle;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const AngleRaw&) const = default;
};
template<typename T> struct IsAngleRaw : public std::integral_constant<bool, std::is_same_v<T, AngleRaw>> { };

struct LengthRaw {
    static constexpr auto category = Calculation::Category::Length;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const LengthRaw&) const = default;
};
template<typename T> struct IsLengthRaw : public std::integral_constant<bool, std::is_same_v<T, LengthRaw>> { };

struct TimeRaw {
    static constexpr auto category = Calculation::Category::Time;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const TimeRaw&) const = default;
};
template<typename T> struct IsTimeRaw : public std::integral_constant<bool, std::is_same_v<T, TimeRaw>> { };

struct FrequencyRaw {
    static constexpr auto category = Calculation::Category::Frequency;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const FrequencyRaw&) const = default;
};
template<typename T> struct IsFrequencyRaw : public std::integral_constant<bool, std::is_same_v<T, FrequencyRaw>> { };

struct ResolutionRaw {
    static constexpr auto category = Calculation::Category::Resolution;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const ResolutionRaw&) const = default;
};
template<typename T> struct IsResolutionRaw : public std::integral_constant<bool, std::is_same_v<T, ResolutionRaw>> { };

struct FlexRaw {
    static constexpr auto category = Calculation::Category::Flex;
    static constexpr auto type = CSSUnitType::CSS_FR;
    double value;

    constexpr bool operator==(const FlexRaw&) const = default;
};
template<typename T> struct IsFlexRaw : public std::integral_constant<bool, std::is_same_v<T, FlexRaw>> { };

// MARK: Dimension + Percentage Primitives Raw

struct AnglePercentageRaw {
    static constexpr auto category = Calculation::Category::AnglePercentage;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const AnglePercentageRaw&) const = default;
};
template<typename T> struct IsAnglePercentageRaw : public std::integral_constant<bool, std::is_same_v<T, AnglePercentageRaw>> { };

struct LengthPercentageRaw {
    static constexpr auto category = Calculation::Category::LengthPercentage;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const LengthPercentageRaw&) const = default;
};
template<typename T> struct IsLengthPercentageRaw : public std::integral_constant<bool, std::is_same_v<T, LengthPercentageRaw>> { };

// MARK: Additional Numeric Adjacent Types Raw

struct NoneRaw {
    constexpr bool operator==(const NoneRaw&) const = default;
};
template<typename T> struct IsNoneRaw : public std::integral_constant<bool, std::is_same_v<T, NoneRaw>> { };

struct SymbolRaw {
    CSSValueID value;

    constexpr bool operator==(const SymbolRaw&) const = default;
};
template<typename T> struct IsSymbolRaw : public std::integral_constant<bool, std::is_same_v<T, SymbolRaw>> { };

// MARK: - Numeric Primitives (Raw + UnevaluatedCalc)

// Concept for use in generic contexts to filter on CSS types.
template<typename T> concept CSSNumeric = requires(T css) {
    typename T::Raw;
    requires RawNumeric<typename T::Raw>;
};

template<RawNumeric T> struct PrimitiveNumeric {
    using Raw = T;

    PrimitiveNumeric(std::variant<T, UnevaluatedCalc<T>>&& value)
        : value { WTFMove(value) }
    {
    }

    PrimitiveNumeric(const std::variant<T, UnevaluatedCalc<T>>& value)
        : value { value }
    {
    }

    PrimitiveNumeric(T&& value)
        : value { WTFMove(value) }
    {
    }

    PrimitiveNumeric(const T& value)
        : value { value }
    {
    }

    PrimitiveNumeric(UnevaluatedCalc<T>&& value)
        : value { WTFMove(value) }
    {
    }

    PrimitiveNumeric(const UnevaluatedCalc<T>& value)
        : value { value }
    {
    }

    bool operator==(const PrimitiveNumeric<T>&) const = default;

    std::variant<T, UnevaluatedCalc<T>> value;
};

// MARK: Number Primitive

template<typename IntType, IntegerValueRange Range> using Integer = PrimitiveNumeric<IntegerRaw<IntType, Range>>;

using Number = PrimitiveNumeric<NumberRaw>;

// MARK: Percentage Primitive

using Percentage = PrimitiveNumeric<PercentageRaw>;

// MARK: Dimension Primitives

using Angle = PrimitiveNumeric<AngleRaw>;
using Length = PrimitiveNumeric<LengthRaw>;
using Time = PrimitiveNumeric<TimeRaw>;
using Frequency = PrimitiveNumeric<FrequencyRaw>;
using Resolution = PrimitiveNumeric<ResolutionRaw>;
using Flex = PrimitiveNumeric<FlexRaw>;

// MARK: Dimension + Percentage Primitives

using AnglePercentage = PrimitiveNumeric<AnglePercentageRaw>;
using LengthPercentage = PrimitiveNumeric<LengthPercentageRaw>;

// MARK: Additional Numeric Adjacent Types

struct None {
    using Raw = NoneRaw;

    constexpr None() = default;
    constexpr None(NoneRaw&&) { }
    constexpr None(const NoneRaw&) { }

    constexpr bool operator==(const None&) const = default;
};
template<typename T> struct IsNone : public std::integral_constant<bool, std::is_same_v<T, None>> { };

struct Symbol {
    using Raw = SymbolRaw;

    constexpr Symbol(SymbolRaw&& value)
        : value { value.value }
    {
    }

    constexpr Symbol(const SymbolRaw& value)
        : value { value.value }
    {
    }

    constexpr bool operator==(const Symbol&) const = default;

    CSSValueID value;
};
template<typename T> struct IsSymbol : public std::integral_constant<bool, std::is_same_v<T, Symbol>> { };

// MARK: Additional Common Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
using PercentageOrNumber = std::variant<Percentage, Number>;

// MARK: - Requires Conversion Data

template<typename T> bool requiresConversionData(const PrimitiveNumeric<T>& primitive)
{
    return requiresConversionData(primitive.value);
}

// MARK: - Requires Conversion Data

template<typename T> bool isUnevaluatedCalc(const PrimitiveNumeric<T>& primitive)
{
    return isUnevaluatedCalc(primitive.value);
}

// MARK: Simplify

template<typename T> auto simplifyUnevaluatedCalc(const PrimitiveNumeric<T>& primitive, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> PrimitiveNumeric<T>
{
    return { simplifyUnevaluatedCalc(primitive.value, conversionData, symbolTable) };
}

// MARK: - Type List Modifiers

template<typename... Ts>
using VariantWrapper = typename std::variant<Ts...>;

template<typename TypeList>
using VariantOrSingle = std::conditional_t<
    brigand::size<TypeList>::value == 1,
    brigand::front<TypeList>,
    brigand::wrap<TypeList, VariantWrapper>
>;

namespace TypeTransform {

// MARK: - Type transforms

namespace Type {

// MARK: Raw type -> CSS type mapping (CSS type -> Raw type directly available via typename CSSType::Raw).

template<typename> struct RawToCSSMapping;
template<> struct RawToCSSMapping<NumberRaw> { using CSS = Number; };
template<> struct RawToCSSMapping<PercentageRaw> { using CSS = Percentage; };
template<> struct RawToCSSMapping<AngleRaw> { using CSS = Angle; };
template<> struct RawToCSSMapping<LengthRaw> { using CSS = Length; };
template<> struct RawToCSSMapping<TimeRaw> { using CSS = Time; };
template<> struct RawToCSSMapping<FrequencyRaw> { using CSS = Frequency; };
template<> struct RawToCSSMapping<ResolutionRaw> { using CSS = Resolution; };
template<> struct RawToCSSMapping<FlexRaw> { using CSS = Flex; };
template<> struct RawToCSSMapping<AnglePercentageRaw> { using CSS = AnglePercentage; };
template<> struct RawToCSSMapping<LengthPercentageRaw> { using CSS = LengthPercentage; };
template<> struct RawToCSSMapping<NoneRaw> { using CSS = None; };
template<> struct RawToCSSMapping<SymbolRaw> { using CSS = Symbol; };

// MARK: Transform Raw type -> CSS type

template<typename T> struct RawToCSSLazy {
    using type = typename RawToCSSMapping<T>::CSS;
};
template<typename T> using RawToCSS = typename RawToCSSLazy<T>::type;

} // namespace Type

// MARK: - List transforms

namespace List {

// MARK: Transform Raw type list -> CSS type list

// Transform `brigand::list<raw1, raw2, ...>`  -> `brigand::list<css1, css2, ...>`
template<typename TypeList> struct RawToCSSLazy {
    using type = brigand::transform<TypeList, Type::RawToCSSLazy<brigand::_1>>;
};
template<typename TypeList> using RawToCSS = typename RawToCSSLazy<TypeList>::type;

// MARK: Add/Remove Symbol

// MARK: Transform Raw type list -> CSS type list + Symbol

// Transform `brigand::list<raw1, raw2, ...>`  -> `brigand::list<css1, css2, ..., symbol>`
template<typename TypeList> struct RawToCSSPlusSymbolLazy {
    using type = brigand::append<brigand::transform<TypeList, Type::RawToCSSLazy<brigand::_1>>, brigand::list<Symbol>>;
};
template<typename TypeList> using RawToCSSPlusSymbol = typename RawToCSSPlusSymbolLazy<TypeList>::type;

// MARK: Transform CSS type list -> CSS type list + Symbol

// Transform `brigand::list<css1, css2, ...>`  -> `brigand::list<css1, css2, ..., symbol>`
template<typename TypeList> struct PlusSymbolLazy {
    using type = brigand::append<TypeList, brigand::list<Symbol>>;
};
template<typename TypeList> using PlusSymbol = typename PlusSymbolLazy<TypeList>::type;

// MARK: Transform CSS type list + Symbol -> CSS type list - Symbol

// Transform `brigand::list<css1, css2, ..., symbol>`  -> `brigand::list<css1, css2, ...>`
template<typename TypeList> struct MinusSymbolLazy {
    using type = brigand::remove_if<TypeList, IsSymbol<brigand::_1>>;
};
template<typename TypeList> using MinusSymbol = typename MinusSymbolLazy<TypeList>::type;

} // namespace List
} // namespace TypeTransform

} // namespace CSS
} // namespace WebCore

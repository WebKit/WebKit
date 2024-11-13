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

#include "CSSNone.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSUnevaluatedCalc.h"
#include "CSSUnits.h"
#include "CSSValueTypes.h"
#include "CalculationCategory.h"
#include <wtf/Brigand.h>

namespace WebCore {
namespace CSS {

// Concept for use in generic contexts to filter on *Raw types.
template<typename T> concept RawNumeric = requires(T raw) {
    { raw.type } -> std::convertible_to<CSSUnitType>;
    { raw.value } -> std::convertible_to<double>;
};

// MARK: Number Primitives Raw

template<typename T, Range R> struct IntegerRaw {
    using IntType = T;
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Integer;
    static constexpr auto type = CSSUnitType::CSS_INTEGER;
    IntType value;

    constexpr bool operator==(const IntegerRaw<T, R>&) const = default;
};

template<Range R = All> struct NumberRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Number;
    static constexpr auto type = CSSUnitType::CSS_NUMBER;
    double value;

    constexpr NumberRaw(double value)
        : value { value }
    {
    }

    // Constructor is required to allow generic code to uniformly initialize primitives with a CSSUnitType.
    constexpr NumberRaw(CSSUnitType, double value)
        : value { value }
    {
    }

    constexpr bool operator==(const NumberRaw<R>&) const = default;
};

// MARK: Percentage Primitive Raw

template<Range R = All> struct PercentageRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Percentage;
    static constexpr auto type = CSSUnitType::CSS_PERCENTAGE;
    double value;

    constexpr PercentageRaw(double value)
        : value { value }
    {
    }

    // Constructor is required to allow generic code to uniformly initialize primitives with a CSSUnitType.
    constexpr PercentageRaw(CSSUnitType, double value)
        : value { value }
    {
    }

    constexpr bool operator==(const PercentageRaw<R>&) const = default;
};

// MARK: Dimension Primitives Raw

template<Range R = All> struct AngleRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Angle;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const AngleRaw<R>&) const = default;
};

double canonicalizeAngle(double value, CSSUnitType);

template<Range R = All> struct LengthRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Length;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const LengthRaw<R>&) const = default;
};

double canonicalizeLengthNoConversionDataRequired(double, CSSUnitType);
double canonicalizeLength(double, CSSUnitType, const CSSToLengthConversionData&);
float canonicalizeAndClampLengthNoConversionDataRequired(double, CSSUnitType);
float canonicalizeAndClampLength(double, CSSUnitType, const CSSToLengthConversionData&);

template<Range R = All> struct TimeRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Time;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const TimeRaw<R>&) const = default;
};

double canonicalizeTime(double, CSSUnitType);

template<Range R = All> struct FrequencyRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Frequency;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const FrequencyRaw<R>&) const = default;
};

double canonicalizeFrequency(double, CSSUnitType);

template<Range R = Nonnegative> struct ResolutionRaw {
    static_assert(R.min >= 0, "<resolution> values must always have a minimum range of at least 0.");
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Resolution;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const ResolutionRaw<R>&) const = default;
};

double canonicalizeResolution(double, CSSUnitType);

template<Range R = All> struct FlexRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::Flex;
    static constexpr auto type = CSSUnitType::CSS_FR;
    double value;

    constexpr bool operator==(const FlexRaw<R>&) const = default;
};

// MARK: Dimension + Percentage Primitives Raw

template<Range R = All> struct AnglePercentageRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::AnglePercentage;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const AnglePercentageRaw<R>&) const = default;
};

template<Range R = All> struct LengthPercentageRaw {
    static constexpr auto range = R;
    static constexpr auto category = Calculation::Category::LengthPercentage;
    CSSUnitType type;
    double value;

    constexpr bool operator==(const LengthPercentageRaw<R>&) const = default;
};

// MARK: Additional Numeric Adjacent Types Raw

struct SymbolRaw {
    CSSValueID value;

    constexpr bool operator==(const SymbolRaw&) const = default;
};

// MARK: - Numeric Primitives (Raw + UnevaluatedCalc)

// Concept for use in generic contexts to filter on CSS types.
template<typename T> concept CSSNumeric = requires(T css) {
    typename T::Raw;
    requires RawNumeric<typename T::Raw>;
};

template<RawNumeric T> struct PrimitiveNumeric {
    static constexpr auto range = T::range;
    static constexpr auto category = T::category;
    using Raw = T;
    using Calc = UnevaluatedCalc<T>;

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

    const T* raw() const { return std::get_if<T>(&value); }
    const UnevaluatedCalc<T>* calc() const { return std::get_if<Calc>(&value); }

    std::variant<T, UnevaluatedCalc<T>> value;
};

// MARK: Number Primitive

template<typename IntType, Range R> using Integer = PrimitiveNumeric<IntegerRaw<IntType, R>>;

template<Range R = All> using Number = PrimitiveNumeric<NumberRaw<R>>;

// MARK: Percentage Primitive

template<Range R = All> using Percentage = PrimitiveNumeric<PercentageRaw<R>>;

// MARK: Dimension Primitives

template<Range R = All> using Angle = PrimitiveNumeric<AngleRaw<R>>;
template<Range R = All> using Length = PrimitiveNumeric<LengthRaw<R>>;
template<Range R = All> using Time = PrimitiveNumeric<TimeRaw<R>>;
template<Range R = All> using Frequency = PrimitiveNumeric<FrequencyRaw<R>>;
template<Range R = Nonnegative> using Resolution = PrimitiveNumeric<ResolutionRaw<R>>;
template<Range R = All> using Flex = PrimitiveNumeric<FlexRaw<R>>;

// MARK: Dimension + Percentage Primitives

template<Range R = All> using AnglePercentage = PrimitiveNumeric<AnglePercentageRaw<R>>;
template<Range R = All> using LengthPercentage = PrimitiveNumeric<LengthPercentageRaw<R>>;

// MARK: Additional Numeric Adjacent Types

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
using PercentageOrNumber = std::variant<Percentage<>, Number<>>;

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
template<auto R> struct RawToCSSMapping<NumberRaw<R>>           { using CSS = Number<R>; };
template<auto R> struct RawToCSSMapping<PercentageRaw<R>>       { using CSS = Percentage<R>; };
template<auto R> struct RawToCSSMapping<AngleRaw<R>>            { using CSS = Angle<R>; };
template<auto R> struct RawToCSSMapping<LengthRaw<R>>           { using CSS = Length<R>; };
template<auto R> struct RawToCSSMapping<TimeRaw<R>>             { using CSS = Time<R>; };
template<auto R> struct RawToCSSMapping<FrequencyRaw<R>>        { using CSS = Frequency<R>; };
template<auto R> struct RawToCSSMapping<ResolutionRaw<R>>       { using CSS = Resolution<R>; };
template<auto R> struct RawToCSSMapping<FlexRaw<R>>             { using CSS = Flex<R>; };
template<auto R> struct RawToCSSMapping<AnglePercentageRaw<R>>  { using CSS = AnglePercentage<R>; };
template<auto R> struct RawToCSSMapping<LengthPercentageRaw<R>> { using CSS = LengthPercentage<R>; };
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

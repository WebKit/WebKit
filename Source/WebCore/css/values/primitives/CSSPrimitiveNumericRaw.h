/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSPrimitiveNumericRange.h>
#include <WebCore/CSSPrimitiveNumericUnits.h>
#include <WebCore/CSSValueTypes.h>

namespace WebCore {
namespace CSS {

// MARK: Numeric Primitives Raw

// NOTE: `ResolvedValueType` only effects the type the CSS value gets resolved to. Unresolved CSS
// primitive numeric types always use a `double` as its internal representation.

// Default implementation of `PrimitiveNumericRaw` for numeric types with a unit specifier.
template<Range R, UnitEnum U, typename V> struct PrimitiveNumericRaw {
    using UnitType = U;
    using UnitTraits = CSS::UnitTraits<UnitType>;
    using ResolvedValueType = V;
    static constexpr auto range = R;
    static constexpr auto category = UnitTraits::category;

    static_assert(UnitTraits::isValidRangeForCategory(range));

    UnitType unit;
    double value;

    // Allows initialization from unit value of the same unit type and a value.
    // e.g.
    //    AngleRaw<R> foo { AngleUnit::Deg, 0 };
    constexpr PrimitiveNumericRaw(UnitType unit, double value)
        : unit { unit }
        , value { value }
    {
    }

    // Allows initialization from a literal with the same unit type.
    // e.g.
    //    AngleRaw<R> foo { 0_css_deg };
    template<UnitType unitValue>
    constexpr PrimitiveNumericRaw(ValueLiteral<unitValue> literal)
        : unit { literal.unit }
        , value { literal.value }
    {
    }

    // Allows initialization from nested unit types if the type is composite.
    // e.g.
    //    AnglePercentageRaw<R> foo { AngleUnit::Deg, 0 };
    //    AnglePercentageRaw<R> foo { PercentageUnit::Percentage, 0 };
    constexpr PrimitiveNumericRaw(NestedUnitEnumOf<UnitType> auto unit, double value)
        : unit { unitUpcast<UnitType>(unit) }
        , value { value }
    {
    }

    // Allows initialization from raw value of one of the nested unit types if the type is composite.
    // e.g.
    //    AnglePercentageRaw<R> foo { AngleRaw<R> { ... } };
    //    AnglePercentageRaw<R> foo { PercentageRaw<R> { ... } };
    template<typename T>
        requires NumericRaw<T> && NestedUnitEnumOf<typename T::UnitType, UnitType>
    constexpr PrimitiveNumericRaw(T other)
        : unit { unitUpcast<UnitType>(other.unit) }
        , value { other.value }
    {
    }

    // Allows initialization from literal value of one of the nested unit types if the type is composite.
    // e.g.
    //    AnglePercentageRaw<R> foo { 0_css_deg };
    //    AnglePercentageRaw<R> foo { 0_css_percentage };
    template<NestedUnitEnumOf<UnitType> E, E unitValue>
    constexpr PrimitiveNumericRaw(ValueLiteral<unitValue> literal)
        : unit { unitUpcast<UnitType>(literal.unit) }
        , value { literal.value }
    {
    }

    constexpr bool operator==(const PrimitiveNumericRaw&) const = default;

    template<typename T>
        requires NumericRaw<T> && NestedUnitEnumOf<typename T::UnitType, UnitType>
    constexpr bool operator==(const T& other) const
    {
        return unit == other.unit && value == other.value;
    }

    template<UnitType unitValue>
    constexpr bool operator==(const ValueLiteral<unitValue>& other) const
    {
        return unit == other.unit && value == other.value;
    }

    template<NestedUnitEnumOf<UnitType> E, E unitValue>
    constexpr bool operator==(const ValueLiteral<unitValue>& other) const
    {
        return unit == unitUpcast<UnitType>(other.unit) && value == other.value;
    }
};

// Specialization of `PrimitiveNumericRaw` for numeric types with only a single possible
// unit type (e.g. IntegerUnit, NumberUnit, PercentageUnit, FlexUnit).
template<Range R, SingleValueUnitEnum U, typename V> struct PrimitiveNumericRaw<R, U, V> {
    using ResolvedValueType = V;
    using UnitType = U;
    using UnitTraits = CSS::UnitTraits<UnitType>;
    static constexpr auto range = R;
    static constexpr auto category = UnitTraits::category;

    static_assert(UnitTraits::isValidRangeForCategory(range));

    static constexpr auto unit = UnitTraits::canonical;
    double value;

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    constexpr PrimitiveNumericRaw(T value)
        : value { static_cast<double>(value) }
    {
    }

    // Constructor is required to allow generic code to uniformly initialize primitives.
    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    constexpr PrimitiveNumericRaw(UnitType, T value)
        : value { static_cast<double>(value) }
    {
    }

    template<auto unitValue>
        requires std::same_as<decltype(unitValue), UnitType>
    constexpr PrimitiveNumericRaw(ValueLiteral<unitValue> literal)
        : value { literal.value }
    {
    }

    constexpr bool operator==(const PrimitiveNumericRaw&) const = default;

    template<auto unitValue>
        requires std::same_as<decltype(unitValue), UnitType>
    constexpr bool operator==(const ValueLiteral<unitValue>& literal) const
    {
        return value == literal.value;
    }
};

// MARK: Integer Primitive Raw

template<Range R = All, typename V = int> struct IntegerRaw : PrimitiveNumericRaw<R, IntegerUnit, V> {
    using Base = PrimitiveNumericRaw<R, IntegerUnit, V>;
    using Base::Base;
};

// MARK: Number Primitive Raw

template<Range R = All, typename V = double> struct NumberRaw : PrimitiveNumericRaw<R, NumberUnit, V> {
    using Base = PrimitiveNumericRaw<R, NumberUnit, V>;
    using Base::Base;
};

// MARK: Percentage Primitive Raw

template<Range R = All, typename V = double> struct PercentageRaw : PrimitiveNumericRaw<R, PercentageUnit, V> {
    using Base = PrimitiveNumericRaw<R, PercentageUnit, V>;
    using Base::Base;
};

// MARK: Dimension Primitives Raw

template<Range R = All, typename V = double> struct AngleRaw : PrimitiveNumericRaw<R, AngleUnit, V> {
    using Base = PrimitiveNumericRaw<R, AngleUnit, V>;
    using Base::Base;
};
template<Range R = All, typename V = float> struct LengthRaw : PrimitiveNumericRaw<R, LengthUnit, V> {
    using Base = PrimitiveNumericRaw<R, LengthUnit, V>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct TimeRaw : PrimitiveNumericRaw<R, TimeUnit, V> {
    using Base = PrimitiveNumericRaw<R, TimeUnit, V>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct FrequencyRaw : PrimitiveNumericRaw<R, FrequencyUnit, V> {
    using Base = PrimitiveNumericRaw<R, FrequencyUnit, V>;
    using Base::Base;
};
template<Range R = Nonnegative, typename V = double> struct ResolutionRaw : PrimitiveNumericRaw<R, ResolutionUnit, V> {
    using Base = PrimitiveNumericRaw<R, ResolutionUnit, V>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct FlexRaw : PrimitiveNumericRaw<R, FlexUnit, V> {
    using Base = PrimitiveNumericRaw<R, FlexUnit, V>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives Raw

template<Range R = All, typename V = float> struct AnglePercentageRaw : PrimitiveNumericRaw<R, AnglePercentageUnit, V> {
    using Base = PrimitiveNumericRaw<R, AnglePercentageUnit, V>;
    using Base::Base;
};
template<Range R = All, typename V = float> struct LengthPercentageRaw : PrimitiveNumericRaw<R, LengthPercentageUnit, V> {
    using Base = PrimitiveNumericRaw<R, LengthPercentageUnit, V>;
    using Base::Base;
};

} // namespace CSS
} // namespace WebCore

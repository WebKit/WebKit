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

#include "CSSPrimitiveNumeric.h"
#include "StylePrimitiveNumeric+Forward.h"
#include "StylePrimitiveNumericConcepts.h"
#include "StyleUnevaluatedCalculation.h"
#include "StyleValueTypes.h"
#include <algorithm>
#include <wtf/CompactVariant.h>
#include <wtf/Forward.h>

namespace WebCore {
namespace Style {

template<typename> struct DimensionPercentageMapping;

struct PrimitiveNumericEmptyToken { constexpr bool operator==(const PrimitiveNumericEmptyToken&) const = default; };

template<typename T> struct PrimitiveNumericMarkableTraits {
    static bool isEmptyValue(const T& value) { return value.isEmpty(); }
    static T emptyValue() { return T(PrimitiveNumericEmptyToken { }); }
};

// Default implementation of `PrimitiveNumeric` for non-composite numeric types.
template<CSS::Numeric CSSType> struct PrimitiveNumeric {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    static constexpr auto unit = UnitTraits::canonical;
    ResolvedValueType value { 0 };

    constexpr PrimitiveNumeric(ResolvedValueType value)
        : value { value }
    {
    }

    constexpr PrimitiveNumeric(WebCore::CSS::ValueLiteral<UnitTraits::canonical> value)
        : value { clampTo<ResolvedValueType>(value.value) }
    {
    }

    constexpr bool isZero() const { return !value; }

    constexpr bool operator==(const PrimitiveNumeric&) const = default;
    constexpr bool operator==(ResolvedValueType other) const { return value == other; };

private:
    template<typename> friend struct PrimitiveNumericMarkableTraits;

    PrimitiveNumeric(PrimitiveNumericEmptyToken) requires std::floating_point<ResolvedValueType>
        : value { std::numeric_limits<ResolvedValueType>::quiet_NaN() }
    {
    }

    bool isEmpty() const
        requires std::floating_point<ResolvedValueType>
    {
        return std::isnan(value);
    }
};

// Specialization of `PrimitiveNumeric` for composite dimension-percentage types.
template<CSS::DimensionPercentageNumeric CSSType> struct PrimitiveNumeric<CSSType> {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = typename CSS::ResolvedValueType;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

    // Composite types only currently support float as the `ResolvedValueType`, allowing unconditional use of `CompactVariant`.
    static_assert(std::same_as<ResolvedValueType, float>);

    using Dimension = typename DimensionPercentageMapping<CSS>::Dimension;
    using Percentage = typename DimensionPercentageMapping<CSS>::Percentage;
    using Calc = UnevaluatedCalculation<CSS>;
    using Representation = CompactVariant<Dimension, Percentage, Calc>;

    PrimitiveNumeric(Dimension dimension)
        : m_value { WTFMove(dimension) }
    {
    }

    PrimitiveNumeric(Percentage percentage)
        : m_value { WTFMove(percentage) }
    {
    }

    PrimitiveNumeric(Calc calc)
        : m_value { WTFMove(calc) }
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Dimension::UnitTraits::canonical> literal)
        : m_value { Dimension { literal } }
    {
    }

    PrimitiveNumeric(WebCore::CSS::ValueLiteral<Percentage::UnitTraits::canonical> literal)
        : m_value { Percentage { literal } }
    {
    }

    // NOTE: CalculatedValue is intentionally not part of IPCData.
    using IPCData = Variant<Dimension, Percentage>;
    PrimitiveNumeric(IPCData&& data)
        : m_value { WTF::switchOn(WTFMove(data), [&](auto&& data) -> Representation { return { WTFMove(data) }; }) }
    {
    }

    IPCData ipcData() const
    {
        return WTF::switchOn(m_value,
            [](const Dimension& dimension) -> IPCData { return dimension; },
            [](const Percentage& percentage) -> IPCData { return percentage; },
            [](const Calc&) -> IPCData { ASSERT_NOT_REACHED(); return Dimension { 0 }; }
        );
    }

    constexpr size_t index() const { return m_value.index(); }

    template<typename T> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<T>(m_value); }
    template<size_t I> constexpr bool holdsAlternative() const { return WTF::holdsAlternative<I>(m_value); }

    template<typename T> T get() const
    {
        return WTF::switchOn(m_value,
            []<std::same_as<T> U>(const U& alternative) -> T { return alternative; },
            [](const auto&) -> T { RELEASE_ASSERT_NOT_REACHED(); }
        );
    }

    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return WTF::switchOn(m_value, std::forward<F>(functors)...);
    }

    constexpr bool isZero() const
    {
        return WTF::switchOn(m_value,
            []<HasIsZero T>(const T& alternative) { return alternative.isZero(); },
            [](const auto&) { return false; }
        );
    }

    bool operator==(const PrimitiveNumeric&) const = default;

private:
    Representation m_value;
};

// MARK: Integer Primitive

template<CSS::Range R, typename V> struct Integer : PrimitiveNumeric<CSS::Integer<R, V>> {
    using Base = PrimitiveNumeric<CSS::Integer<R, V>>;
};

// MARK: Number Primitive

template<CSS::Range R, typename V> struct Number : PrimitiveNumeric<CSS::Number<R, V>> {
    using Base = PrimitiveNumeric<CSS::Number<R, V>>;
    using Base::Base;
};

// MARK: Percentage Primitive

template<CSS::Range R, typename V> struct Percentage : PrimitiveNumeric<CSS::Percentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::Percentage<R, V>>;
    using Base::Base;
};

// MARK: Dimension Primitives

template<CSS::Range R, typename V> struct Angle : PrimitiveNumeric<CSS::Angle<R, V>> {
    using Base = PrimitiveNumeric<CSS::Angle<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Length : PrimitiveNumeric<CSS::Length<R, V>> {
    using Base = PrimitiveNumeric<CSS::Length<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Time : PrimitiveNumeric<CSS::Time<R, V>> {
    using Base = PrimitiveNumeric<CSS::Time<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Frequency : PrimitiveNumeric<CSS::Frequency<R, V>> {
    using Base = PrimitiveNumeric<CSS::Frequency<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Resolution : PrimitiveNumeric<CSS::Resolution<R, V>> {
    using Base = PrimitiveNumeric<CSS::Resolution<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct Flex : PrimitiveNumeric<CSS::Flex<R, V>> {
    using Base = PrimitiveNumeric<CSS::Flex<R, V>>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives

template<CSS::Range R, typename V> struct AnglePercentage : PrimitiveNumeric<CSS::AnglePercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::AnglePercentage<R, V>>;
    using Base::Base;
};
template<CSS::Range R, typename V> struct LengthPercentage : PrimitiveNumeric<CSS::LengthPercentage<R, V>> {
    using Base = PrimitiveNumeric<CSS::LengthPercentage<R, V>>;
    using Base::Base;
};

template<auto R, typename V> struct DimensionPercentageMapping<CSS::AnglePercentage<R, V>> {
    using Dimension = Style::Angle<R, V>;
    using Percentage = Style::Percentage<R, V>;
};
template<auto R, typename V> struct DimensionPercentageMapping<CSS::LengthPercentage<R, V>> {
    using Dimension = Style::Length<R, V>;
    using Percentage = Style::Percentage<R, V>;
};

template<typename T> T get(DimensionPercentageNumeric auto const& dimensionPercentage)
{
    return dimensionPercentage.template get<T>();
}

// MARK: CSS -> Style

template<auto R, typename V> struct ToStyleMapping<CSS::Integer<R, V>>          { using type = Integer<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Number<R, V>>           { using type = Number<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Percentage<R, V>>       { using type = Percentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Angle<R, V>>            { using type = Angle<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Length<R, V>>           { using type = Length<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Time<R, V>>             { using type = Time<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Frequency<R, V>>        { using type = Frequency<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Resolution<R, V>>       { using type = Resolution<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::Flex<R, V>>             { using type = Flex<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::AnglePercentage<R, V>>  { using type = AnglePercentage<R, V>; };
template<auto R, typename V> struct ToStyleMapping<CSS::LengthPercentage<R, V>> { using type = LengthPercentage<R, V>; };

// MARK: Style -> CSS

template<Numeric T> struct ToCSSMapping<T> {
    using type = typename T::CSS;
};

// MARK: Utility Concepts

template<typename T> concept IsPercentageOrCalc =
       std::same_as<T, Percentage<T::range, typename T::ResolvedValueType>>
    || std::same_as<T, UnevaluatedCalculation<typename T::CSS>>;

} // namespace Style
} // namespace WebCore

template<WebCore::Style::DimensionPercentageNumeric T>
struct WTF::FlatteningVariantTraits<T> {
    using TypeList = typename FlatteningVariantTraits<typename T::Representation>::TypeList;
};

namespace WTF {

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Number<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Number<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Percentage<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Percentage<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Angle<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Angle<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Length<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Length<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Time<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Time<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Frequency<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Frequency<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Resolution<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Resolution<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::Style::Flex<R, V>> : WebCore::Style::PrimitiveNumericMarkableTraits<WebCore::Style::Flex<R, V>> { };

} // namespace WTF

template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::AnglePercentage<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentage<R, V>> = true;

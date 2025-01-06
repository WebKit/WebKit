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

#include "CSSPrimitiveNumericTypes.h"
#include "StylePrimitiveNumericConcepts.h"
#include "StyleUnevaluatedCalculation.h"
#include "StyleValueTypes.h"
#include <wtf/CompactVariant.h>

namespace WebCore {
namespace Style {

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

    constexpr bool isZero() const { return !value; }

    constexpr bool operator==(const PrimitiveNumeric&) const = default;
    constexpr bool operator==(ResolvedValueType other) const { return value == other; };
};

template<typename> struct DimensionPercentageMapping;

// Specialization of `PrimitiveNumeric` for composite dimension-percentage types.
template<CSS::DimensionPercentageNumeric CSSType> struct PrimitiveNumeric<CSSType> {
    using CSS = CSSType;
    using Raw = typename CSS::Raw;
    using UnitType = typename CSS::UnitType;
    using UnitTraits = typename CSS::UnitTraits;
    using ResolvedValueType = float;
    static constexpr auto range = CSS::range;
    static constexpr auto category = CSS::category;

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

    PrimitiveNumeric(Ref<CalculationValue> calculationValue)
        : m_value { Calc { WTFMove(calculationValue) } }
    {
    }

    PrimitiveNumeric(Calculation::Child calculationValue)
        : m_value { Calc { WTFMove(calculationValue) } }
    {
    }

    // NOTE: CalculatedValue is intentionally not part of IPCData.
    using IPCData = std::variant<Dimension, Percentage>;
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

template<CSS::Range R = CSS::All, typename V = int> struct Integer : PrimitiveNumeric<CSS::Integer<R, V>> {
    using Base = PrimitiveNumeric<CSS::Integer<R, V>>;
};

// MARK: Number Primitive

template<CSS::Range R = CSS::All> struct Number : PrimitiveNumeric<CSS::Number<R>> {
    using Base = PrimitiveNumeric<CSS::Number<R>>;
    using Base::Base;
};

// MARK: Percentage Primitive

template<CSS::Range R = CSS::All> struct Percentage : PrimitiveNumeric<CSS::Percentage<R>> {
    using Base = PrimitiveNumeric<CSS::Percentage<R>>;
    using Base::Base;
};

// MARK: Dimension Primitives

template<CSS::Range R = CSS::All> struct Angle : PrimitiveNumeric<CSS::Angle<R>> {
    using Base = PrimitiveNumeric<CSS::Angle<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All> struct Length : PrimitiveNumeric<CSS::Length<R>> {
    using Base = PrimitiveNumeric<CSS::Length<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All> struct Time : PrimitiveNumeric<CSS::Time<R>> {
    using Base = PrimitiveNumeric<CSS::Time<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All> struct Frequency : PrimitiveNumeric<CSS::Frequency<R>> {
    using Base = PrimitiveNumeric<CSS::Frequency<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::Nonnegative> struct Resolution : PrimitiveNumeric<CSS::Resolution<R>> {
    using Base = PrimitiveNumeric<CSS::Resolution<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All> struct Flex : PrimitiveNumeric<CSS::Flex<R>> {
    using Base = PrimitiveNumeric<CSS::Flex<R>>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives

template<CSS::Range R = CSS::All> struct AnglePercentage : PrimitiveNumeric<CSS::AnglePercentage<R>> {
    using Base = PrimitiveNumeric<CSS::AnglePercentage<R>>;
    using Base::Base;
};
template<CSS::Range R = CSS::All> struct LengthPercentage : PrimitiveNumeric<CSS::LengthPercentage<R>> {
    using Base = PrimitiveNumeric<CSS::LengthPercentage<R>>;
    using Base::Base;
};

template<auto R> struct DimensionPercentageMapping<CSS::AnglePercentage<R>> {
    using Dimension = Style::Angle<R>;
    using Percentage = Style::Percentage<R>;
};
template<auto R> struct DimensionPercentageMapping<CSS::LengthPercentage<R>> {
    using Dimension = Style::Length<R>;
    using Percentage = Style::Percentage<R>;
};

template<typename T> T get(DimensionPercentageNumeric auto const& dimensionPercentage)
{
    return dimensionPercentage.template get<T>();
}

// MARK: Additional Common Types and Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<CSS::Range nR = CSS::All, CSS::Range pR = nR> struct NumberOrPercentage {
    NumberOrPercentage(std::variant<Number<nR>, Percentage<pR>> value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentage(Number<nR> value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentage(Percentage<pR> value)
        : value { WTFMove(value) }
    {
    }

    bool operator==(const NumberOrPercentage&) const = default;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<Number<nR>>()));

        return WTF::switchOn(value,
            [](EmptyToken) -> ResultType {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const Number<nR>& number) -> ResultType {
                return visitor(number);
            },
            [&](const Percentage<pR>& percentage) -> ResultType {
                return visitor(percentage);
            }
        );
    }

    struct MarkableTraits {
        static bool isEmptyValue(const NumberOrPercentage& value) { return value.isEmpty(); }
        static NumberOrPercentage emptyValue() { return NumberOrPercentage(EmptyToken()); }
    };

private:
    struct EmptyToken { constexpr bool operator==(const EmptyToken&) const = default; };
    NumberOrPercentage(EmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<EmptyToken>(value); }

    std::variant<EmptyToken, Number<nR>, Percentage<pR>> value;
};

template<CSS::Range nR = CSS::All, CSS::Range pR = nR> struct NumberOrPercentageResolvedToNumber {
    Number<nR> value { 0 };

    constexpr NumberOrPercentageResolvedToNumber(typename Number<nR>::ResolvedValueType value)
        : value { value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Number<nR> number)
        : value { number }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Percentage<pR> percentage)
        : value { percentage.value / 100.0 }
    {
    }

    constexpr bool isZero() const { return value.isZero(); }

    constexpr bool operator==(const NumberOrPercentageResolvedToNumber<nR, pR>&) const = default;
    constexpr bool operator==(typename Number<nR>::ResolvedValueType other) const { return value.value == other; };
};

// Standard Numbers
using NumberAll = Number<CSS::All>;
using NumberNonnegative = Number<CSS::Nonnegative>;

// Standard Angles
using AngleAll = Length<CSS::All>;

// Standard Lengths
using LengthAll = Length<CSS::All>;
using LengthNonnegative = Length<CSS::Nonnegative>;

// Standard LengthPercentages
using LengthPercentageAll = LengthPercentage<CSS::All>;
using LengthPercentageNonnegative = LengthPercentage<CSS::Nonnegative>;

// Standard Percentages
using Percentage0To100 = LengthPercentage<CSS::Range{0,100}>;

// Standard Points
using LengthPercentageSpaceSeparatedPointAll = SpaceSeparatedPoint<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedPointNonnegative = SpaceSeparatedPoint<LengthPercentageNonnegative>;

// Standard Sizes
using LengthPercentageSpaceSeparatedSizeAll = SpaceSeparatedSize<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedSizeNonnegative = SpaceSeparatedSize<LengthPercentageNonnegative>;

// MARK: CSS -> Style

template<auto R, typename T> struct ToStyleMapping<CSS::Integer<R, T>> { using type = Integer<R, T>; };
template<auto R> struct ToStyleMapping<CSS::Number<R>>                 { using type = Number<R>; };
template<auto R> struct ToStyleMapping<CSS::Percentage<R>>             { using type = Percentage<R>; };
template<auto R> struct ToStyleMapping<CSS::Angle<R>>                  { using type = Angle<R>; };
template<auto R> struct ToStyleMapping<CSS::Length<R>>                 { using type = Length<R>; };
template<auto R> struct ToStyleMapping<CSS::Time<R>>                   { using type = Time<R>; };
template<auto R> struct ToStyleMapping<CSS::Frequency<R>>              { using type = Frequency<R>; };
template<auto R> struct ToStyleMapping<CSS::Resolution<R>>             { using type = Resolution<R>; };
template<auto R> struct ToStyleMapping<CSS::Flex<R>>                   { using type = Flex<R>; };
template<auto R> struct ToStyleMapping<CSS::AnglePercentage<R>>        { using type = AnglePercentage<R>; };
template<auto R> struct ToStyleMapping<CSS::LengthPercentage<R>>       { using type = LengthPercentage<R>; };

template<auto nR, auto pR> struct ToStyleMapping<CSS::NumberOrPercentage<nR, pR>> {
    using type = NumberOrPercentage<nR, pR>;
};

template<auto nR, auto pR> struct ToStyleMapping<CSS::NumberOrPercentageResolvedToNumber<nR, pR>> {
    using type = NumberOrPercentageResolvedToNumber<nR, pR>;
};

// MARK: Style -> CSS

template<Numeric T> struct ToCSSMapping<T> {
    using type = typename T::CSS;
};

template<auto nR, auto pR> struct ToCSSMapping<NumberOrPercentage<nR, pR>> {
    using type = CSS::NumberOrPercentage<nR, pR>;
};

template<auto nR, auto pR> struct ToCSSMapping<NumberOrPercentageResolvedToNumber<nR, pR>> {
    using type = CSS::NumberOrPercentageResolvedToNumber<nR, pR>;
};

} // namespace Style
} // namespace WebCore

namespace WTF {

// Allow primitives numeric types that usually store their value as a `double` to be
// used with CompactVariant by using a `float`, rather than `double` representation
// when used in a `CompactVariant`.

template<WebCore::Style::Numeric T>
    requires std::same_as<typename T::ResolvedValueType, double>
struct CompactVariantTraits<T> {
   static constexpr bool hasAlternativeRepresentation = true;

   static constexpr uint64_t encodeFromArguments(double value)
   {
       return static_cast<uint64_t>(std::bit_cast<uint32_t>(clampTo<float>(value)));
   }

   static constexpr uint64_t encode(const T& value)
   {
       return static_cast<uint64_t>(std::bit_cast<uint32_t>(clampTo<float>(value.value)));
   }

   static constexpr T decode(uint64_t value)
   {
       return { std::bit_cast<float>(static_cast<uint32_t>(value)) };
   }
};

} // namespace WTF

template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::AnglePercentage<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::LengthPercentage<R>> = true;
template<auto nR, auto pR> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::NumberOrPercentage<nR, pR>> = true;

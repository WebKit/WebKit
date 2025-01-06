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

#include "CSSPrimitiveNumericConcepts.h"
#include "CSSPrimitiveNumericRaw.h"
#include "CSSUnevaluatedCalc.h"
#include <limits>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace CSS {

// MARK: - Primitive Numeric (Raw + UnevaluatedCalc)

// NOTE: As is the case for `PrimitiveNumericRaw`, `ResolvedValueType` here only effects the type
// the CSS value gets resolved to. Unresolved CSS primitive numeric types always use a `double` as
// its internal representation.

template<typename T> struct PrimitiveNumericMarkableTraits {
    static bool isEmptyValue(const T& value) { return value.isEmpty(); }
    static T emptyValue() { return T(typename T::EmptyToken { }); }
};

template<NumericRaw RawType> struct PrimitiveNumeric {
    using Raw = RawType;
    using Calc = UnevaluatedCalc<Raw>;
    using UnitType = typename Raw::UnitType;
    using UnitTraits = typename Raw::UnitTraits;
    using ResolvedValueType = typename Raw::ResolvedValueType;
    using Index = std::underlying_type_t<UnitType>;
    static constexpr auto range = Raw::range;
    static constexpr auto category = Raw::category;

    // The potential values for the `index` are:
    //  - 0 ..< number of unit variations   -> Raw
    //  - number of unit variations         -> Calc
    //  - number of unit variations + 1     -> Empty (for Markable)
    //  - number of unit variations + 2     -> Moved from
    static constexpr Index indexForFirstRaw  = 0;
    static constexpr Index indexForCalc      = UnitTraits::count;
    static constexpr Index indexForEmpty     = UnitTraits::count + 1;
    static constexpr Index indexForMovedFrom = UnitTraits::count + 2;
    static_assert(UnitTraits::count < std::numeric_limits<Index>::max() - 2);

    PrimitiveNumeric(Raw raw)
    {
        m_index = enumToUnderlyingType(raw.unit);
        m_value.number = raw.value;
    }

    PrimitiveNumeric(Calc calc)
    {
        m_index = indexForCalc;
        m_value.calc = &calc.protectedCalc().leakRef();
    }

    PrimitiveNumeric(UnitEnum auto unit, double value) requires (requires { { Raw(unit, value) }; })
        : PrimitiveNumeric { Raw { unit, value } }
    {
    }

    PrimitiveNumeric(double value) requires (requires { { Raw(value) }; })
        : PrimitiveNumeric { Raw { value } }
    {
    }

    template<UnitEnum E, E unitValue>
    constexpr PrimitiveNumeric(ValueLiteral<unitValue> value) requires (requires { { Raw(value) }; })
        : PrimitiveNumeric { Raw { value } }
    {
    }

    PrimitiveNumeric(const PrimitiveNumeric& other)
    {
        if (other.isCalc()) {
            m_index = indexForCalc;
            m_value.calc = other.m_value.calc;
            m_value.calc->ref();
        } else {
            m_index = other.m_index;
            m_value.number = other.m_value.number;
        }
    }

    PrimitiveNumeric(PrimitiveNumeric&& other)
    {
        if (other.isCalc()) {
            m_index = indexForCalc;
            m_value.calc = other.m_value.calc;
        } else {
            m_index = other.m_index;
            m_value.number = other.m_value.number;
        }

        other.m_index = indexForMovedFrom;
        other.m_value.number = 0;
    }

    PrimitiveNumeric& operator=(const PrimitiveNumeric& other)
    {
        if (isCalc())
            m_value.calc->deref();

        if (other.isCalc()) {
            m_index = indexForCalc;
            m_value.calc = other.m_value.calc;
            m_value.calc->ref();
        } else {
            m_index = other.m_index;
            m_value.number = other.m_value.number;
        }

        return *this;
    }

    PrimitiveNumeric& operator=(PrimitiveNumeric&& other)
    {
        if (isCalc())
            m_value.calc->deref();

        if (other.isCalc()) {
            m_index = indexForCalc;
            m_value.calc = other.m_value.calc;
        } else {
            m_index = other.m_index;
            m_value.number = other.m_value.number;
        }

        other.m_index = indexForMovedFrom;
        other.m_value.number = 0;

        return *this;
    }

    ~PrimitiveNumeric()
    {
        if (isCalc())
            m_value.calc->deref();
    }

    bool operator==(const PrimitiveNumeric& other) const
    {
        if (m_index != other.m_index)
            return false;

        if (isCalc())
            return protectedCalc()->equals(other.protectedCalc());
        return m_value.number == other.m_value.number;
    }

    bool operator==(const Raw& otherRaw) const
    {
        if (m_index != enumToUnderlyingType(otherRaw.unit))
            return false;

        ASSERT(isRaw());
        return m_value.number == otherRaw.value;
    }

    template<typename T>
        requires NumericRaw<T> && NestedUnitEnumOf<typename T::UnitType, UnitType>
    constexpr bool operator==(const T& other) const
    {
        if (m_index != enumToUnderlyingType(unitUpcast<UnitType>(other.unit)))
            return false;

        ASSERT(isRaw());
        return m_value.number == other.value;
    }

    template<UnitType unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        if (m_index != enumToUnderlyingType(other.unit))
            return false;

        ASSERT(isRaw());
        return m_value.number == other.value;
    }

    template<NestedUnitEnumOf<UnitType> E, E unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        if (m_index != enumToUnderlyingType(unitUpcast<UnitType>(other.unit)))
            return false;

        ASSERT(isRaw());
        return m_value.number == other.value;
    }

    std::optional<Raw> raw() const
    {
        if (isRaw())
            return asRaw();
        return std::nullopt;
    }

    std::optional<Calc> calc() const
    {
        if (isCalc())
            return asCalc();
        return std::nullopt;
    }

    template<typename T> bool holdsAlternative() const
    {
        if constexpr (std::same_as<T, Calc>)
            return isCalc();
        else if constexpr (std::same_as<T, Raw>)
            return isRaw();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isCalc())
            return visitor(asCalc());
        return visitor(asRaw());
    }

    bool isKnownZero() const { return isRaw() && m_value.number == 0; }
    bool isKnownNotZero() const { return isRaw() && m_value.number != 0; }

    bool isCalc() const { return m_index == indexForCalc; }
    bool isRaw() const { return m_index < indexForCalc; }

private:
    template<typename> friend struct PrimitiveNumericMarkableTraits;

    struct EmptyToken { constexpr bool operator==(const EmptyToken&) const = default; };
    PrimitiveNumeric(EmptyToken)
    {
        m_index = indexForEmpty;
        m_value.number = 0;
    }

    bool isEmpty() const { return m_index == indexForEmpty; }

    Ref<CSSCalcValue> protectedCalc() const
    {
        ASSERT(isCalc());
        return Ref(*m_value.calc);
    }

    Raw asRaw() const
    {
        ASSERT(isRaw());
        return Raw { static_cast<UnitType>(m_index), m_value.number };
    }

    Calc asCalc() const
    {
        ASSERT(isCalc());
        return Calc { protectedCalc() };
    }

    // A std::variant is not used here to allow tighter packing.

    // FIXME: This could be even more packed types with only a single alternative (e.g. CSS::Number/CSS::Percentage/CSS::Flex),
    // by using NaN encoding scheme for the `calc` case.

    union {
        double number;
        CSSCalcValue* calc;
    } m_value;
    Index m_index;
};

// MARK: Integer Primitive

template<Range R = All, typename V = int> struct Integer : PrimitiveNumeric<IntegerRaw<R, V>> {
    using Base = PrimitiveNumeric<IntegerRaw<R, V>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Integer>;
};

// MARK: Number Primitive

template<Range R = All> struct Number : PrimitiveNumeric<NumberRaw<R>> {
    using Base = PrimitiveNumeric<NumberRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Number>;
};

// MARK: Percentage Primitive

template<Range R = All> struct Percentage : PrimitiveNumeric<PercentageRaw<R>> {
    using Base = PrimitiveNumeric<PercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Percentage>;
};

// MARK: Dimension Primitives

template<Range R = All> struct Angle : PrimitiveNumeric<AngleRaw<R>> {
    using Base = PrimitiveNumeric<AngleRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Angle>;
};
template<Range R = All> struct Length : PrimitiveNumeric<LengthRaw<R>> {
    using Base = PrimitiveNumeric<LengthRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Length>;
};
template<Range R = All> struct Time : PrimitiveNumeric<TimeRaw<R>> {
    using Base = PrimitiveNumeric<TimeRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Time>;
};
template<Range R = All> struct Frequency : PrimitiveNumeric<FrequencyRaw<R>> {
    using Base = PrimitiveNumeric<FrequencyRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Frequency>;
};
template<Range R = Nonnegative> struct Resolution : PrimitiveNumeric<ResolutionRaw<R>> {
    using Base = PrimitiveNumeric<ResolutionRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Resolution>;
};
template<Range R = All> struct Flex : PrimitiveNumeric<FlexRaw<R>> {
    using Base = PrimitiveNumeric<FlexRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<Flex>;
};

// MARK: Dimension + Percentage Primitives

template<Range R = All> struct AnglePercentage : PrimitiveNumeric<AnglePercentageRaw<R>> {
    using Base = PrimitiveNumeric<AnglePercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<AnglePercentage<R>>;
};
template<Range R = All> struct LengthPercentage : PrimitiveNumeric<LengthPercentageRaw<R>> {
    using Base = PrimitiveNumeric<LengthPercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveNumericMarkableTraits<LengthPercentage<R>>;
};

// MARK: Additional Common Groupings

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<Range nR = All, Range pR = nR> struct NumberOrPercentage {
    NumberOrPercentage(std::variant<Number<nR>, Percentage<pR>> value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentage(NumberRaw<nR> value)
        : value { Number<nR> { WTFMove(value) } }
    {
    }

    NumberOrPercentage(Number<nR> value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentage(PercentageRaw<pR> value)
        : value { Percentage<pR> { WTFMove(value) } }
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

template<Range nR = All, Range pR = nR> struct NumberOrPercentageResolvedToNumber {
    NumberOrPercentageResolvedToNumber(std::variant<Number<nR>, Percentage<pR>> value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentageResolvedToNumber(NumberRaw<nR> value)
        : value { Number<nR> { WTFMove(value) } }
    {
    }

    NumberOrPercentageResolvedToNumber(Number<nR> value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentageResolvedToNumber(PercentageRaw<pR> value)
        : value { Percentage<pR> { WTFMove(value) } }
    {
    }

    NumberOrPercentageResolvedToNumber(Percentage<pR> value)
        : value { WTFMove(value) }
    {
    }

    bool operator==(const NumberOrPercentageResolvedToNumber&) const = default;

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
        static bool isEmptyValue(const NumberOrPercentageResolvedToNumber& value) { return value.isEmpty(); }
        static NumberOrPercentageResolvedToNumber emptyValue() { return NumberOrPercentageResolvedToNumber(EmptyToken()); }
    };

private:
    struct EmptyToken { constexpr bool operator==(const EmptyToken&) const = default; };
    NumberOrPercentageResolvedToNumber(EmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<EmptyToken>(value); }

    std::variant<EmptyToken, Number<nR>, Percentage<pR>> value;
};

} // namespace CSS
} // namespace WebCore

template<auto R, typename T> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Integer<R, T>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Number<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Percentage<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Angle<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Length<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Time<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Frequency<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Resolution<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Flex<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::AnglePercentage<R>> = true;
template<auto R> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::LengthPercentage<R>> = true;

template<typename Raw> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::PrimitiveNumeric<Raw>> = true;
template<auto nR, auto pR> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentage<nR, pR>> = true;
template<auto nR, auto pR> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR>> = true;

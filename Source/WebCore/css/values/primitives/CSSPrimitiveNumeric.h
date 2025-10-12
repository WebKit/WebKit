/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSPrimitiveData.h>
#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSPrimitiveNumericRaw.h>
#include <WebCore/CSSUnevaluatedCalc.h>
#include <limits>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace CSS {

// MARK: - Forward Declarations

template<NumericRaw> struct PrimitiveNumeric;
template<Numeric, PrimitiveKeyword...> struct PrimitiveNumericOrKeyword;

// MARK: - Primitive Numeric (Raw + UnevaluatedCalc)

// NOTE: As is the case for `PrimitiveNumericRaw`, `ResolvedValueType` here only effects the type
// the CSS value gets resolved to. Unresolved CSS primitive numeric types always use a `double` as
// its internal representation.

template<NumericRaw RawType> struct PrimitiveNumeric {
    using Raw = RawType;
    using Calc = UnevaluatedCalc<Raw>;
    using UnitType = typename Raw::UnitType;
    using UnitTraits = typename Raw::UnitTraits;
    using ResolvedValueType = typename Raw::ResolvedValueType;
    using Data = PrimitiveData<PrimitiveNumeric<RawType>>;
    using Index = typename Data::Index;
    static constexpr auto range = Raw::range;
    static constexpr auto category = Raw::category;

    PrimitiveNumeric(Raw raw)
        : m_data { raw }
    {
    }

    PrimitiveNumeric(Calc calc)
        : m_data { WTFMove(calc) }
    {
    }

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    PrimitiveNumeric(T value) requires (requires { { Raw(value) }; })
        : m_data { Raw { value } }
    {
    }

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    PrimitiveNumeric(UnitEnum auto unit, T value) requires (requires { { Raw(unit, value) }; })
        : m_data { Raw { unit, value } }
    {
    }

    template<UnitEnum E, E unitValue>
    constexpr PrimitiveNumeric(ValueLiteral<unitValue> value) requires (requires { { Raw(value) }; })
        : m_data { Raw { value } }
    {
    }

    // MARK: Copy/Move Construction/Assignment

    PrimitiveNumeric(const PrimitiveNumeric& other)
        : m_data { other.m_data }
    {
    }

    PrimitiveNumeric(PrimitiveNumeric&& other)
        : m_data { WTFMove(other.m_data) }
    {
    }

    PrimitiveNumeric& operator=(const PrimitiveNumeric& other)
    {
        m_data = other.m_data;
        return *this;
    }

    PrimitiveNumeric& operator=(PrimitiveNumeric&& other)
    {
        m_data = WTFMove(other.m_data);
        return *this;
    }

    // MARK: Equality

    bool operator==(const PrimitiveNumeric& other) const
    {
        return m_data == other.m_data;
    }

    bool operator==(const Raw& other) const
    {
        return m_data == other;
    }

    template<typename T>
        requires NumericRaw<T> && NestedUnitEnumOf<typename T::UnitType, UnitType>
    constexpr bool operator==(const T& other) const
    {
        return m_data == other;
    }

    template<UnitType unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        return m_data == other;
    }

    template<NestedUnitEnumOf<UnitType> E, E unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        return m_data == other;
    }

    // MARK: Conditional Accessors

    std::optional<Raw> raw() const { return m_data.raw(); }
    std::optional<Calc> calc() const { return m_data.calc(); }

    // MARK: Variant-Like Conformance

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

    bool isKnownZero() const { return isRaw() && m_data.payload.number == 0; }
    bool isKnownNotZero() const { return isRaw() && m_data.payload.number != 0; }

    bool isRaw() const { return m_data.isRaw(); }
    bool isCalc() const { return m_data.isCalc(); }
    bool isEmpty() const { return m_data.isEmpty(); }

private:
    template<typename> friend struct PrimitiveDataMarkableTraits;
    template<Numeric, PrimitiveKeyword...> friend struct PrimitiveNumericOrKeyword;

    PrimitiveNumeric(PrimitiveDataEmptyToken token)
        : m_data { token }
    {
    }

    Raw asRaw() const { return m_data.asRaw(); }
    Calc asCalc() const { return m_data.asCalc(); }

    Data m_data;
};

// MARK: Integer Primitive

template<Range R = All, typename V = int> struct Integer : PrimitiveNumeric<IntegerRaw<R, V>> {
    using Base = PrimitiveNumeric<IntegerRaw<R, V>>;
    using Base::Base;
};

// MARK: Number Primitive

template<Range R = All, typename V = double> struct Number : PrimitiveNumeric<NumberRaw<R, V>> {
    using Base = PrimitiveNumeric<NumberRaw<R, V>>;
    using Base::Base;
};

// MARK: Percentage Primitive

template<Range R = All, typename V = double> struct Percentage : PrimitiveNumeric<PercentageRaw<R, V>> {
    using Base = PrimitiveNumeric<PercentageRaw<R, V>>;
    using Base::Base;
};

// MARK: Dimension Primitives

template<Range R = All, typename V = double> struct Angle : PrimitiveNumeric<AngleRaw<R, V>> {
    using Base = PrimitiveNumeric<AngleRaw<R, V>>;
    using Base::Base;
};
template<Range R = All, typename V = float> struct Length : PrimitiveNumeric<LengthRaw<R, V>> {
    using Base = PrimitiveNumeric<LengthRaw<R, V>>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct Time : PrimitiveNumeric<TimeRaw<R, V>> {
    using Base = PrimitiveNumeric<TimeRaw<R, V>>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct Frequency : PrimitiveNumeric<FrequencyRaw<R, V>> {
    using Base = PrimitiveNumeric<FrequencyRaw<R, V>>;
    using Base::Base;
};
template<Range R = Nonnegative, typename V = double> struct Resolution : PrimitiveNumeric<ResolutionRaw<R, V>> {
    using Base = PrimitiveNumeric<ResolutionRaw<R, V>>;
    using Base::Base;
};
template<Range R = All, typename V = double> struct Flex : PrimitiveNumeric<FlexRaw<R, V>> {
    using Base = PrimitiveNumeric<FlexRaw<R, V>>;
    using Base::Base;
};

// MARK: Dimension + Percentage Primitives

template<Range R = All, typename V = float> struct AnglePercentage : PrimitiveNumeric<AnglePercentageRaw<R, V>> {
    using Base = PrimitiveNumeric<AnglePercentageRaw<R, V>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<AnglePercentage<R, V>>;
};
template<Range R = All, typename V = float> struct LengthPercentage : PrimitiveNumeric<LengthPercentageRaw<R, V>> {
    using Base = PrimitiveNumeric<LengthPercentageRaw<R, V>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<LengthPercentage<R, V>>;
};

} // namespace CSS
} // namespace WebCore

namespace WTF {

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Integer<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Integer<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Number<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Number<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Percentage<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Percentage<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Angle<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Angle<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Length<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Length<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Time<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Time<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Frequency<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Frequency<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Resolution<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Resolution<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::Flex<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::Flex<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::AnglePercentage<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::AnglePercentage<R, V>> { };

template<auto R, typename V>
struct MarkableTraits<WebCore::CSS::LengthPercentage<R, V>> : WebCore::CSS::PrimitiveDataMarkableTraits<WebCore::CSS::LengthPercentage<R, V>> { };

} // namespace WTF

template<typename Raw> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::PrimitiveNumeric<Raw>> = true;

template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Integer<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Number<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Percentage<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Angle<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Length<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Time<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Frequency<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Resolution<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::Flex<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::AnglePercentage<R, V>> = true;
template<auto R, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::LengthPercentage<R, V>> = true;

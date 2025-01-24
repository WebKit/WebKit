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

#include "CSSPrimitiveData.h"
#include "CSSPrimitiveNumericConcepts.h"
#include "CSSPrimitiveNumericRaw.h"
#include "CSSUnevaluatedCalc.h"
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
    using MarkableTraits = PrimitiveDataMarkableTraits<Integer>;
};

// MARK: Number Primitive

template<Range R = All> struct Number : PrimitiveNumeric<NumberRaw<R>> {
    using Base = PrimitiveNumeric<NumberRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Number>;
};

// MARK: Percentage Primitive

template<Range R = All> struct Percentage : PrimitiveNumeric<PercentageRaw<R>> {
    using Base = PrimitiveNumeric<PercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Percentage>;
};

// MARK: Dimension Primitives

template<Range R = All> struct Angle : PrimitiveNumeric<AngleRaw<R>> {
    using Base = PrimitiveNumeric<AngleRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Angle>;
};
template<Range R = All> struct Length : PrimitiveNumeric<LengthRaw<R>> {
    using Base = PrimitiveNumeric<LengthRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Length>;
};
template<Range R = All> struct Time : PrimitiveNumeric<TimeRaw<R>> {
    using Base = PrimitiveNumeric<TimeRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Time>;
};
template<Range R = All> struct Frequency : PrimitiveNumeric<FrequencyRaw<R>> {
    using Base = PrimitiveNumeric<FrequencyRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Frequency>;
};
template<Range R = Nonnegative> struct Resolution : PrimitiveNumeric<ResolutionRaw<R>> {
    using Base = PrimitiveNumeric<ResolutionRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Resolution>;
};
template<Range R = All> struct Flex : PrimitiveNumeric<FlexRaw<R>> {
    using Base = PrimitiveNumeric<FlexRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<Flex>;
};

// MARK: Dimension + Percentage Primitives

template<Range R = All> struct AnglePercentage : PrimitiveNumeric<AnglePercentageRaw<R>> {
    using Base = PrimitiveNumeric<AnglePercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<AnglePercentage<R>>;
};
template<Range R = All> struct LengthPercentage : PrimitiveNumeric<LengthPercentageRaw<R>> {
    using Base = PrimitiveNumeric<LengthPercentageRaw<R>>;
    using Base::Base;
    using MarkableTraits = PrimitiveDataMarkableTraits<LengthPercentage<R>>;
};

} // namespace CSS
} // namespace WebCore

template<typename Raw> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::PrimitiveNumeric<Raw>> = true;

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

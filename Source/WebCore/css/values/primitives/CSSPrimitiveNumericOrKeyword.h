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

#include "CSSPrimitiveNumeric.h"

namespace WebCore {
namespace CSS {

// MARK: Primitive Numeric or Keyword

template<Numeric NumericType, PrimitiveKeyword... Ks> struct PrimitiveNumericOrKeyword {
    using Raw = typename NumericType::Raw;
    using Calc = typename NumericType::Calc;
    using UnitType = typename NumericType::UnitType;
    using UnitTraits = typename NumericType::UnitTraits;
    using ResolvedValueType = typename NumericType::ResolvedValueType;
    using Data = PrimitiveData<NumericType, Ks...>;
    using Index = typename Data::Index;
    using Keywords = typename Data::Keywords;
    static constexpr auto range = NumericType::range;
    static constexpr auto category = NumericType::category;

    // MARK: Constructors

    PrimitiveNumericOrKeyword(Raw raw)
        : m_data { raw }
    {
    }

    PrimitiveNumericOrKeyword(Calc calc)
        : m_data { WTFMove(calc) }
    {
    }

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    PrimitiveNumericOrKeyword(T value) requires (requires { { Raw(value) }; })
        : m_data { Raw { value } }
    {
    }

    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    PrimitiveNumericOrKeyword(UnitEnum auto unit, T value) requires (requires { { Raw(unit, value) }; })
        : m_data { Raw { unit, value } }
    {
    }

    template<UnitEnum E, E unitValue>
    constexpr PrimitiveNumericOrKeyword(ValueLiteral<unitValue> value) requires (requires { { Raw(value) }; })
        : m_data { Raw { value } }
    {
    }

    template<typename... U>
    constexpr PrimitiveNumericOrKeyword(Variant<U...>&& variant)
        : m_data {
            WTF::switchOn(WTFMove(variant),
                [](NumericType&& numeric) {
                    return Data { WTFMove(numeric.m_data) };
                },
                [](ValidKeywordForList<Keywords> auto keyword) {
                    return Data { keyword };
                }
            )
        }
    {
    }

    // MARK: Copy/Move Construction/Assignment

    PrimitiveNumericOrKeyword(const PrimitiveNumericOrKeyword& other)
        : m_data { other.m_data }
    {
    }

    PrimitiveNumericOrKeyword(PrimitiveNumericOrKeyword&& other)
        : m_data { WTFMove(other.m_data) }
    {
    }

    PrimitiveNumericOrKeyword& operator=(const PrimitiveNumericOrKeyword& other)
    {
        m_data = other.m_data;
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(PrimitiveNumericOrKeyword&& other)
    {
        m_data = WTFMove(other.m_data);
        return *this;
    }

    // MARK: Construction/Assignment from `NumericType`

    PrimitiveNumericOrKeyword(const NumericType& other)
        : m_data { other.m_data }
    {
    }

    PrimitiveNumericOrKeyword(NumericType&& other)
        : m_data { WTFMove(other.m_data) }
    {
    }

    PrimitiveNumericOrKeyword& operator=(const NumericType& other)
    {
        m_data = other;
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(NumericType&& other)
    {
        m_data = WTFMove(other);
        return *this;
    }

    // MARK: Construction/Assignment from `Keywords...`

    PrimitiveNumericOrKeyword(ValidKeywordForList<Keywords> auto keyword)
        : m_data { keyword }
    {
    }

    // MARK: Equality

    bool operator==(const PrimitiveNumericOrKeyword& other) const
    {
        return m_data == other.m_data;
    }

    bool operator==(ValidKeywordForList<Keywords> auto const& other) const
    {
        return m_data == other;
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
        if constexpr (std::same_as<T, NumericType>)
            return isCalc() || isRaw();
        else if constexpr (std::same_as<T, Calc>)
            return isCalc();
        else if constexpr (std::same_as<T, Raw>)
            return isRaw();
        else if constexpr (ValidKeywordForList<T, Keywords>)
            return isKeyword<T>();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return m_data.visit(WTF::makeVisitor(std::forward<F>(f)...));
    }

    bool isKnownZero() const { return isRaw() && m_data.payload.number == 0; }
    bool isKnownNotZero() const { return isRaw() && m_data.payload.number != 0; }

    bool isRaw() const { return m_data.isRaw(); }
    bool isCalc() const { return m_data.isCalc(); }
    template<ValidKeywordForList<Keywords> Keyword>
    bool isKeyword() const { return m_data.template isKeyword<Keyword>(); }
    bool isEmpty() const { return m_data.isEmpty(); }

private:
    friend struct MarkableTraits<PrimitiveNumericOrKeyword>;

    PrimitiveNumericOrKeyword(PrimitiveDataEmptyToken token)
        : m_data { token }
    {
    }

    Raw asRaw() const { return m_data.asRaw(); }
    Calc asCalc() const { return m_data.asCalc(); }

    Data m_data;
};

} // namespace CSS
} // namespace WebCore

namespace WTF {

template<typename N, typename... Ks>
struct MarkableTraits<WebCore::CSS::PrimitiveNumericOrKeyword<N, Ks...>> {
    static bool isEmptyValue(const WebCore::CSS::PrimitiveNumericOrKeyword<N, Ks...>& value) { return value.isEmpty(); }
    static WebCore::CSS::PrimitiveNumericOrKeyword<N, Ks...> emptyValue() { return { WebCore::CSS::PrimitiveDataEmptyToken { } }; }
};

} // namespace WTF

template<typename N, typename... Ks> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::PrimitiveNumericOrKeyword<N, Ks...>> = true;

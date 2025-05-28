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
#include "StylePrimitiveNumeric.h"
#include "StylePrimitiveNumericOrKeyword.h"

namespace WebCore {
namespace Style {

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<CSS::Range nR = CSS::All, CSS::Range pR = nR, typename V = double> struct NumberOrPercentage {
    using Number = Style::Number<nR, V>;
    using Percentage = Style::Percentage<pR, V>;

    NumberOrPercentage(Variant<Number, Percentage>&& value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentage(Number value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentage(Percentage value)
        : value { WTFMove(value) }
    {
    }

    bool operator==(const NumberOrPercentage&) const = default;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<Number>()));

        return WTF::switchOn(value,
            [](CSS::PrimitiveDataEmptyToken) -> ResultType {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const Number& number) -> ResultType {
                return visitor(number);
            },
            [&](const Percentage& percentage) -> ResultType {
                return visitor(percentage);
            }
        );
    }

    struct MarkableTraits {
        static bool isEmptyValue(const NumberOrPercentage& value) { return value.isEmpty(); }
        static NumberOrPercentage emptyValue() { return NumberOrPercentage(CSS::PrimitiveDataEmptyToken()); }
    };

private:
    NumberOrPercentage(CSS::PrimitiveDataEmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<CSS::PrimitiveDataEmptyToken>(value); }

    Variant<CSS::PrimitiveDataEmptyToken, Number, Percentage> value;
};

template<CSS::Range nR = CSS::All, CSS::Range pR = nR, typename V = double> struct NumberOrPercentageResolvedToNumber {
    using Number = Style::Number<nR, V>;
    using Percentage = Style::Percentage<pR, V>;

    Number value { 0 };

    constexpr NumberOrPercentageResolvedToNumber(typename Number::ResolvedValueType value)
        : value { value }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Number number)
        : value { number }
    {
    }

    constexpr NumberOrPercentageResolvedToNumber(Percentage percentage)
        : value { percentage.value / 100.0 }
    {
    }

    constexpr bool isZero() const { return value.isZero(); }

    constexpr bool operator==(const NumberOrPercentageResolvedToNumber&) const = default;
    constexpr bool operator==(typename Number::ResolvedValueType other) const { return value.value == other; };
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
using PercentageAll = Percentage<CSS::All>;
using PercentageNonnegative = Percentage<CSS::Nonnegative>;
using PercentageAllFloat = Percentage<CSS::All, float>;
using PercentageNonnegativeFloat = Percentage<CSS::Nonnegative, float>;
using Percentage0To100 = LengthPercentage<CSS::Range{0,100}>;

// Standard Points
using LengthPercentageSpaceSeparatedPointAll = SpaceSeparatedPoint<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedPointNonnegative = SpaceSeparatedPoint<LengthPercentageNonnegative>;

// Standard Sizes
using LengthPercentageSpaceSeparatedSizeAll = SpaceSeparatedSize<LengthPercentageAll>;
using LengthPercentageSpaceSeparatedSizeNonnegative = SpaceSeparatedSize<LengthPercentageNonnegative>;
using LengthPercentageMinimallySerializingSpaceSeparatedSizeAll = MinimallySerializingSpaceSeparatedSize<LengthPercentageAll>;
using LengthPercentageMinimallySerializingSpaceSeparatedSizeNonnegative = MinimallySerializingSpaceSeparatedSize<LengthPercentageNonnegative>;

// MARK: CSS -> Style

template<auto nR, auto pR, typename V> struct ToStyleMapping<CSS::NumberOrPercentage<nR, pR, V>> {
    using type = NumberOrPercentage<nR, pR, V>;
};

template<auto nR, auto pR, typename V> struct ToStyleMapping<CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using type = NumberOrPercentageResolvedToNumber<nR, pR, V>;
};

// MARK: Style -> CSS

template<auto nR, auto pR, typename V> struct ToCSSMapping<NumberOrPercentage<nR, pR, V>> {
    using type = CSS::NumberOrPercentage<nR, pR, V>;
};

template<auto nR, auto pR, typename V> struct ToCSSMapping<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using type = CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>;
};

} // namespace Style
} // namespace WebCore

template<auto nR, auto pR, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::NumberOrPercentage<nR, pR, V>> = true;

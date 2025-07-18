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

// Umbrella header for family of CSS numeric types.

#include "CSSPrimitiveNumeric.h"
#include "CSSPrimitiveNumericOrKeyword.h"

namespace WebCore {
namespace CSS {

// NOTE: This is spelled with an explicit "Or" to distinguish it from types like AnglePercentage/LengthPercentage that have behavior distinctions beyond just being a union of the two types (specifically, calc() has specific behaviors for those types).
template<Range nR = All, Range pR = nR, typename V = double> struct NumberOrPercentage {
    using Number = CSS::Number<nR, V>;
    using Percentage = CSS::Percentage<pR, V>;

    NumberOrPercentage(Variant<Number, Percentage>&& value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentage(typename Number::Raw value)
        : value { Number { WTFMove(value) } }
    {
    }

    NumberOrPercentage(Number value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentage(typename Percentage::Raw value)
        : value { Percentage { WTFMove(value) } }
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
            [](PrimitiveDataEmptyToken) -> ResultType {
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

private:
    friend struct MarkableTraits<NumberOrPercentage>;

    NumberOrPercentage(PrimitiveDataEmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<PrimitiveDataEmptyToken>(value); }

    Variant<PrimitiveDataEmptyToken, Number, Percentage> value;
};

template<Range nR = All, Range pR = nR, typename V = double> struct NumberOrPercentageResolvedToNumber {
    using Number = CSS::Number<nR, V>;
    using Percentage = CSS::Percentage<pR, V>;

    NumberOrPercentageResolvedToNumber(Variant<Number, Percentage>&& value)
    {
        WTF::switchOn(WTFMove(value), [this](auto&& alternative) { this->value = WTFMove(alternative); });
    }

    NumberOrPercentageResolvedToNumber(typename Number::Raw value)
        : value { Number { WTFMove(value) } }
    {
    }

    NumberOrPercentageResolvedToNumber(Number value)
        : value { WTFMove(value) }
    {
    }

    NumberOrPercentageResolvedToNumber(typename Percentage::Raw value)
        : value { Percentage { WTFMove(value) } }
    {
    }

    NumberOrPercentageResolvedToNumber(Percentage value)
        : value { WTFMove(value) }
    {
    }

    bool operator==(const NumberOrPercentageResolvedToNumber&) const = default;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        using ResultType = decltype(visitor(std::declval<Number>()));

        return WTF::switchOn(value,
            [](PrimitiveDataEmptyToken) -> ResultType {
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

private:
    NumberOrPercentageResolvedToNumber(PrimitiveDataEmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<PrimitiveDataEmptyToken>(value); }

    Variant<PrimitiveDataEmptyToken, Number, Percentage> value;
};

} // namespace CSS
} // namespace WebCore

namespace WTF {

template<auto nR, auto pR, typename V>
struct MarkableTraits<WebCore::CSS::NumberOrPercentage<nR, pR, V>> {
    static bool isEmptyValue(const WebCore::CSS::NumberOrPercentage<nR, pR, V>& value) { return value.isEmpty(); }
    static WebCore::CSS::NumberOrPercentage<nR, pR, V> emptyValue() { return WebCore::CSS::NumberOrPercentage<nR, pR, V>(WebCore::CSS::PrimitiveDataEmptyToken()); }
};

template<auto nR, auto pR, typename V>
struct MarkableTraits<WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    static bool isEmptyValue(const WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>& value) { return value.isEmpty(); }
    static WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR, V> emptyValue() { return WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>(WebCore::CSS::PrimitiveDataEmptyToken()); }
};

} // namespace WTF

template<auto nR, auto pR, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentage<nR, pR, V>> = true;
template<auto nR, auto pR, typename V> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> = true;

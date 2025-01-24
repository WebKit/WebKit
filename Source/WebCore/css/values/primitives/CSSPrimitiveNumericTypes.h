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
            [](PrimitiveDataEmptyToken) -> ResultType {
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
        static NumberOrPercentage emptyValue() { return NumberOrPercentage(PrimitiveDataEmptyToken()); }
    };

private:
    NumberOrPercentage(PrimitiveDataEmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<PrimitiveDataEmptyToken>(value); }

    std::variant<PrimitiveDataEmptyToken, Number<nR>, Percentage<pR>> value;
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
            [](PrimitiveDataEmptyToken) -> ResultType {
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
        static NumberOrPercentageResolvedToNumber emptyValue() { return NumberOrPercentageResolvedToNumber(PrimitiveDataEmptyToken()); }
    };

private:
    NumberOrPercentageResolvedToNumber(PrimitiveDataEmptyToken token)
        : value { WTFMove(token) }
    {
    }

    bool isEmpty() const { return std::holds_alternative<PrimitiveDataEmptyToken>(value); }

    std::variant<PrimitiveDataEmptyToken, Number<nR>, Percentage<pR>> value;
};

} // namespace CSS
} // namespace WebCore


template<auto nR, auto pR> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentage<nR, pR>> = true;
template<auto nR, auto pR> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::NumberOrPercentageResolvedToNumber<nR, pR>> = true;

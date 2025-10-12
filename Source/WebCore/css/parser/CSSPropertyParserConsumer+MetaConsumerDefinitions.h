/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - Generic Consumer Definition

template<typename> struct ConsumerDefinition;

// Used to check that a specialization of ConsumerDefinition exists.
struct HasConsumerDefinition {
private:
    template<typename T, typename U = decltype(ConsumerDefinition<T>{})>
    static constexpr bool exists(int) { return true; }

    template<typename T>
    static constexpr bool exists(char) { return false; }

public:
    template<typename T>
    static constexpr bool check() { return exists<T>(0); }
};

// FIXME: Bailing on infinity during validation does not seem to match the intent of the spec,
// though due to the use of "implementation-defined" it may still be conforming. The spec states:
//
//   "When a value cannot be explicitly supported due to range/precision limitations, it must
//    be converted to the closest value supported by the implementation, but how the implementation
//    defines "closest" is implementation-defined as well."
//
// Angles have the additional restriction that:
//
//   "If an <angle> must be converted due to exceeding the implementation-defined range of supported
//    values, it must be clamped to the nearest supported multiple of 360deg."
//
// (https://drafts.csswg.org/css-values-4/#numeric-types)
//
// The infinity here is produced by the parser when a parsed number is no representable in
// as a double. A potentially more appropriate behavior would be to have the parser use
// std::numeric_limits<double>::max() instead. For angles, this would require further integration
// with the fast_float library (or whatever is currently being used to parse the number) to
// extract the correct modulo 360deg value.

// Shared validator for types dimensional types that need to canonicalize to support range
// constraints other than 0 and +/-∞.
template<typename Raw, typename F> bool isValidDimensionValue(Raw raw, F&& functor)
{
    if (std::isinf(raw.value))
        return false;

    if constexpr (raw.range.min == -CSS::Range::infinity && raw.range.max == CSS::Range::infinity)
        return true;
    else if constexpr (raw.range.min == 0 && raw.range.max == CSS::Range::infinity)
        return raw.value >= 0;
    else if constexpr (raw.range.min == -CSS::Range::infinity && raw.range.max == 0)
        return raw.value <= 0;
    else
        return functor();
}

// Shared validator for types that only support 0 and +/-∞ as valid range constraints.
template<typename Raw> bool isValidNonCanonicalizableDimensionValue(Raw raw)
{
    if (std::isinf(raw.value))
        return false;

    if constexpr (raw.range.min == -CSS::Range::infinity && raw.range.max == CSS::Range::infinity)
        return true;
    else if constexpr (raw.range.min == 0 && raw.range.max == CSS::Range::infinity)
        return raw.value >= 0;
    else if constexpr (raw.range.min == -CSS::Range::infinity && raw.range.max == 0)
        return raw.value <= 0;
}

// Shared validator for types that always have their value in canonical units (number, percentage, flex).
template<typename Raw> bool isValidCanonicalValue(Raw raw)
{
    if (std::isinf(raw.value))
        return false;

    if constexpr (raw.range.min == -CSS::Range::infinity && raw.range.max == CSS::Range::infinity)
        return true;
    else if constexpr (raw.range.max == CSS::Range::infinity)
        return raw.value >= raw.range.min;
    else if constexpr (raw.range.min == -CSS::Range::infinity)
        return raw.value <= raw.range.max;
    else
        return raw.value >= raw.range.min && raw.value <= raw.range.max;
}

// Shared clamping utility.
template<typename Raw> Raw performParseTimeClamp(Raw raw)
{
    static_assert(raw.range.clampOptions != CSS::RangeClampOptions::Default);

    if constexpr (raw.range.clampOptions == CSS::RangeClampOptions::ClampLower)
        return { std::max<typename Raw::ResolvedValueType>(raw.value, raw.range.min) };
    else if constexpr (raw.range.clampOptions == CSS::RangeClampOptions::ClampUpper)
        return { std::min<typename Raw::ResolvedValueType>(raw.value, raw.range.max) };
    else if constexpr (raw.range.clampOptions == CSS::RangeClampOptions::ClampBoth)
        return { std::clamp<typename Raw::ResolvedValueType>(raw.value, raw.range.min, raw.range.max) };
}

// Shared consumer for `Dimension` tokens.
template<typename Primitive, typename Validator> struct DimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == DimensionToken);

        auto& token = range.peek();

        auto validatedUnit = Validator::validate(token.unitType(), state, options);
        if (!validatedUnit)
            return std::nullopt;

        auto rawValue = typename Primitive::Raw { *validatedUnit, token.numericValue() };

        if constexpr (rawValue.range.clampOptions != CSS::RangeClampOptions::Default)
            rawValue = performParseTimeClamp(rawValue);

        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Percentage` tokens.
template<typename Primitive, typename Validator> struct PercentageConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == PercentageToken);

        auto rawValue = typename Primitive::Raw { CSS::PercentageUnit::Percentage, range.peek().numericValue() };

        if constexpr (rawValue.range.clampOptions != CSS::RangeClampOptions::Default)
            rawValue = performParseTimeClamp(rawValue);

        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Number` tokens.
template<typename Primitive, typename Validator> struct NumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        auto rawValue = typename Primitive::Raw { CSS::NumberUnit::Number, range.peek().numericValue() };

        if constexpr (rawValue.range.clampOptions != CSS::RangeClampOptions::Default)
            rawValue = performParseTimeClamp(rawValue);

        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Number` tokens for use by dimensional primitives that support "unitless" values.
template<typename Primitive, typename Validator, auto unit> struct NumberConsumerForUnitlessValues {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        auto numericValue = range.peek().numericValue();
        if (!Validator::shouldAcceptUnitlessValue(numericValue, state, options))
            return std::nullopt;

        auto rawValue = typename Primitive::Raw { unit, numericValue };

        if constexpr (rawValue.range.clampOptions != CSS::RangeClampOptions::Default)
            rawValue = performParseTimeClamp(rawValue);

        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Function` tokens that processes `calc()` for the provided primitive.
template<typename Primitive> struct FunctionConsumerForCalcValues {
    static constexpr CSSParserTokenType tokenType = FunctionToken;

    static std::optional<typename Primitive::Calc> consume(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (RefPtr value = CSSCalcValue::parse(rangeCopy, state, Primitive::category, Primitive::range, WTFMove(symbolsAllowed), options)) {
            range = rangeCopy;
            return {{ value.releaseNonNull() }};
        }

        return std::nullopt;
    }
};

template<typename T> struct KeywordConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;

    static std::optional<T> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        ASSERT(range.peek().type() == IdentToken);

        if (range.peek().id() == T::value) {
            range.consumeIncludingWhitespace();
            return T { };
        }

        return std::nullopt;
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

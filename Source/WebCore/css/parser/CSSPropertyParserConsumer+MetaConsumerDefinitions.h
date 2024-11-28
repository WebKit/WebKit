/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserOptions.h"

namespace WebCore {

struct CSSParserContext;

namespace CSSPropertyParserHelpers {

// MARK: - Generic Consumer Definition

template<typename> struct ConsumerDefinition;

inline bool shouldAcceptUnitlessValue(double value, CSSPropertyParserOptions options)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths.

    if (!value && options.unitlessZero == UnitlessZeroQuirk::Allow)
        return true;

    if (isUnitlessValueParsingEnabledForMode(options.parserMode))
        return true;

    return options.parserMode == HTMLQuirksMode && options.unitless == UnitlessQuirk::Allow;
}

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

// Shared consumer for `Dimension` tokens.
template<typename Primitive, typename Validator> struct DimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == DimensionToken);

        auto& token = range.peek();

        auto unitType = token.unitType();
        if (!Validator::isValid(unitType, options))
            return std::nullopt;

        auto rawValue = typename Primitive::Raw { unitType, token.numericValue() };
        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Percentage` tokens.
template<typename Primitive, typename Validator> struct PercentageConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == PercentageToken);

        auto rawValue = typename Primitive::Raw { CSSUnitType::CSS_PERCENTAGE, range.peek().numericValue() };
        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Number` tokens.
template<typename Primitive, typename Validator> struct NumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        auto rawValue = typename Primitive::Raw { CSSUnitType::CSS_NUMBER, range.peek().numericValue() };
        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Number` tokens for use by dimensional primitives that support "unitless" values.
template<typename Primitive, typename Validator, CSSUnitType unitType> struct NumberConsumerForUnitlessValues {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        auto numericValue = range.peek().numericValue();
        if (!shouldAcceptUnitlessValue(numericValue, options))
            return std::nullopt;

        auto rawValue = typename Primitive::Raw { unitType, numericValue };
        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

// Shared consumer for `Function` tokens that processes `calc()` for the provided primitive.
template<typename Primitive> struct FunctionConsumerForCalcValues {
    static constexpr CSSParserTokenType tokenType = FunctionToken;

    static std::optional<typename Primitive::Calc> consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Primitive::category, Primitive::range, WTFMove(symbolsAllowed), options)) {
            range = rangeCopy;
            return {{ value.releaseNonNull() }};
        }

        return std::nullopt;
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

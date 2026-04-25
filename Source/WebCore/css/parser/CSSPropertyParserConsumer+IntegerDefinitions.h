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

#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

struct IntegerValidator {
    static constexpr std::optional<CSS::IntegerUnit> validate(CSSUnitType unitType, CSS::PropertyParserState&, CSSPropertyParserOptions)
    {
        return CSS::UnitTraits<CSS::IntegerUnit>::validate(unitType);
    }

    template<auto R, typename V> static bool isValid(CSS::IntegerRaw<R, V> raw, CSSPropertyParserOptions)
    {
        return isValidCanonicalValue(raw);
    }
};

template<typename Primitive, typename Validator> struct NumberConsumerForIntegerValues {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Primitive::Raw> consume(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (range.peek().numericValueType() != IntegerValueType)
            return std::nullopt;

        auto rawValue = typename Primitive::Raw { CSS::IntegerUnit::Integer, range.peek().numericValue() };

        if constexpr (rawValue.range.clampOptions != CSS::RangeClampOptions::Default)
            rawValue = performParseTimeClamp(rawValue);

        if (!Validator::isValid(rawValue, options))
            return std::nullopt;

        range.consumeIncludingWhitespace();
        return rawValue;
    }
};

template<CSS::Range R, typename IntType>
struct ConsumerDefinition<CSS::Integer<R, IntType>> {
    using FunctionToken = FunctionConsumerForCalcValues<CSS::Integer<R, IntType>>;
    using NumberToken = NumberConsumerForIntegerValues<CSS::Integer<R, IntType>, IntegerValidator>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

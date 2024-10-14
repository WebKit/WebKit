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
    template<auto R> static bool isValid(CSS::NumberRaw<R> raw, CSSPropertyParserOptions)
    {
        return isValidCanonicalValue(raw);
    }
};

template<typename Integer, typename Validator> struct NumberConsumerForIntegerValues {
    static constexpr CSSParserTokenType tokenType = NumberToken;

    static std::optional<typename Integer::Raw> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (range.peek().numericValueType() != IntegerValueType)
            return std::nullopt;

        // Validate the value using a CSS::NumberRaw since it has the same range semantics
        // but keeps the value a double.
        auto numberValue = CSS::NumberRaw<Integer::range> { range.peek().numericValue() };
        if (!Validator::isValid(numberValue, options))
            return std::nullopt;

        return typename Integer::Raw { clampTo<typename Integer::Raw::IntType>(range.consumeIncludingWhitespace().numericValue()) };
    }
};

template<typename IntType, CSS::Range R>
struct ConsumerDefinition<CSS::Integer<IntType, R>> {
    using FunctionToken = FunctionConsumerForCalcValues<CSS::Integer<IntType, R>>;
    using NumberToken = NumberConsumerForIntegerValues<CSS::Integer<IntType, R>, IntegerValidator>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

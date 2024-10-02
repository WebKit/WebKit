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
#include "CSSCalcValue.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserOptions.h"
#include "CalculationCategory.h"
#include "Length.h"
#include <limits>
#include <optional>
#include <wtf/Brigand.h>
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

constexpr double computeMinimumValue(CSS::IntegerValueRange range)
{
    switch (range) {
    case CSS::IntegerValueRange::All:
        return -std::numeric_limits<double>::infinity();
    case CSS::IntegerValueRange::NonNegative:
        return 0.0;
    case CSS::IntegerValueRange::Positive:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return 0.0;
}

template<typename IntType, CSS::IntegerValueRange integerRange>
std::optional<CSS::IntegerRaw<IntType, integerRange>> validatedRange(CSS::IntegerRaw<IntType, integerRange> value, CSSPropertyParserOptions)
{
    return value;
}

template<typename IntType, CSS::IntegerValueRange integerRange>
struct IntegerKnownTokenTypeFunctionConsumer {
    using RawType = CSS::IntegerRaw<IntType, integerRange>;

    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<CSS::UnevaluatedCalc<RawType>> consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Calculation::Category::Integer, WTFMove(symbolsAllowed), options)) {
            range = rangeCopy;
            return {{ value.releaseNonNull() }};
        }
        return std::nullopt;
    }
};

template<typename IntType, CSS::IntegerValueRange integerRange>
struct IntegerKnownTokenTypeNumberConsumer {
    using RawType = CSS::IntegerRaw<IntType, integerRange>;

    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<RawType> consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (range.peek().numericValueType() == NumberValueType || range.peek().numericValue() < computeMinimumValue(integerRange))
            return std::nullopt;
        return RawType { clampTo<IntType>(range.consumeIncludingWhitespace().numericValue()) };
    }
};

template<typename IntType, CSS::IntegerValueRange integerRange>
struct ConsumerDefinition<CSS::Integer<IntType, integerRange>> {
    using FunctionToken = IntegerKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

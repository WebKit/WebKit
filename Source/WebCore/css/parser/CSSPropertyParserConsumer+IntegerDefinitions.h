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

#include "CSSCalcParser.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "CalculationCategory.h"
#include "Length.h"
#include <limits>
#include <optional>
#include <wtf/Brigand.h>
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<typename IntType, IntegerValueRange integerRange>
std::optional<IntegerRaw<IntType, integerRange>> validatedRange(IntegerRaw<IntType, integerRange> value, CSSPropertyParserOptions)
{
    return value;
}

template<typename IntType, IntegerValueRange integerRange>
struct IntegerKnownTokenTypeFunctionConsumer {
    using RawType = IntegerRaw<IntType, integerRange>;

    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<UnevaluatedCalc<RawType>> consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Number, WTFMove(symbolsAllowed), options)) {
            range = rangeCopy;
            return {{ value.releaseNonNull() }};
        }
        return std::nullopt;
    }
};

template<typename IntType, IntegerValueRange integerRange>
struct IntegerKnownTokenTypeNumberConsumer {
    using RawType = IntegerRaw<IntType, integerRange>;

    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<RawType> consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed, CSSPropertyParserOptions)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (range.peek().numericValueType() == NumberValueType || range.peek().numericValue() < computeMinimumValue(integerRange))
            return std::nullopt;
        return RawType { clampTo<IntType>(range.consumeIncludingWhitespace().numericValue()) };
    }
};

template<typename IntType, IntegerValueRange integerRange>
struct ConsumerDefinition<IntegerRaw<IntType, integerRange>> {
    using RawType = IntegerRaw<IntType, integerRange>;
    using type = brigand::list<RawType, UnevaluatedCalc<RawType>>;

    using FunctionToken = IntegerKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

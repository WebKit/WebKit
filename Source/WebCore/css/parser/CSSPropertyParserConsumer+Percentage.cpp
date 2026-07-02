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

#include "config.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSParserContext.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumePercentageDividedBy100OrNumber(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    using NumberConsumer = ConsumerDefinition<CSS::Number<>>;
    using PercentageConsumer = ConsumerDefinition<CSS::Percentage<>>;

    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (auto value = NumberConsumer::FunctionToken::consume(range, state, { }, { }))
            return CSSPrimitiveValueResolver<CSS::Number<>>::resolve(*value);
        if (auto value = PercentageConsumer::FunctionToken::consume(range, state, { }, { }))
            return CSSPrimitiveValueResolver<CSS::Percentage<>>::resolve(*value);
        break;

    case NumberToken:
        if (auto value = NumberConsumer::NumberToken::consume(range, state, { }, { }))
            return CSSPrimitiveValueResolver<CSS::Number<>>::resolve(*value);
        break;

    case PercentageToken:
        if (auto value = PercentageConsumer::PercentageToken::consume(range, state, { }, { }))
            return CSSPrimitiveValue::create(value->value / 100.0);
        break;

    default:
        break;
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

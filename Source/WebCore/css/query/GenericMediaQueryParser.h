/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CSSParserContext.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "GenericMediaQueryTypes.h"

namespace WebCore {
namespace MQ {

class GenericMediaQueryParserBase {
public:
    GenericMediaQueryParserBase(const CSSParserContext& context)
        : m_context(context)
    { }

protected:
    std::optional<Feature> consumeFeature(CSSParserTokenRange&);
    std::optional<Feature> consumeBooleanOrPlainFeature(CSSParserTokenRange&);
    std::optional<Feature> consumeRangeFeature(CSSParserTokenRange&);
    RefPtr<CSSValue> consumeValue(CSSParserTokenRange&);

    const CSSParserContext& m_context;
};

template<typename ConcreteParser>
class GenericMediaQueryParser : public GenericMediaQueryParserBase {
public:
    GenericMediaQueryParser(const CSSParserContext& context)
        : GenericMediaQueryParserBase(context)
    { }

    template<typename ConditionType> std::optional<ConditionType> consumeCondition(CSSParserTokenRange&);

private:
    ConcreteParser& concreteParser() { return static_cast<ConcreteParser&>(*this); }
};

template<typename ConcreteParser>
template<typename ConditionType>
std::optional<ConditionType> GenericMediaQueryParser<ConcreteParser>::consumeCondition(CSSParserTokenRange& range)
{
    if (range.peek().type() == IdentToken) {
        if (range.peek().id() == CSSValueNot) {
            range.consumeIncludingWhitespace();
            if (auto query = concreteParser().consumeQueryInParens(range))
                return ConditionType { LogicalOperator::Not, { *query } };
            return { };
        }
    }

    ConditionType condition;

    auto query = concreteParser().consumeQueryInParens(range);
    if (!query)
        return { };

    condition.queries.append(*query);
    range.consumeWhitespace();

    auto consumeOperator = [&]() -> std::optional<LogicalOperator> {
        auto operatorToken = range.consumeIncludingWhitespace();
        if (operatorToken.type() != IdentToken)
            return { };
        if (operatorToken.id() == CSSValueAnd)
            return LogicalOperator::And;
        if (operatorToken.id() == CSSValueOr)
            return LogicalOperator::Or;
        return { };
    };

    while (!range.atEnd()) {
        auto op = consumeOperator();
        if (!op)
            return { };
        if (condition.queries.size() > 1 && condition.logicalOperator != *op)
            return { };

        condition.logicalOperator = *op;

        auto query = concreteParser().consumeQueryInParens(range);
        if (!query)
            return { };

        condition.queries.append(*query);
        range.consumeWhitespace();
    }

    return condition;
}

}
}

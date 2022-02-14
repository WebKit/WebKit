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

#include "config.h"
#include "ContainerQueryParser.h"

#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserHelpers.h"

namespace WebCore {

std::optional<ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ContainerQueryParser parser(context);
    return parser.consumeContainerQuery(range);
}

std::optional<CQ::ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        bool isSizeQuery = range.peek().functionId() == CSSValueSize;

        auto blockRange = range.consumeBlock();
        blockRange.consumeWhitespace();

        if (!isSizeQuery)
            return CQ::UnknownQuery { };

        auto sizeQuery = consumeSizeQuery(blockRange);
        if (!sizeQuery)
            return { };
        return { *sizeQuery };
    }

    if (range.peek().type() == LeftParenthesisToken) {
        auto blockRange = range.consumeBlock();
        range.consumeWhitespace();

        blockRange.consumeWhitespace();

        // Try to parse as a size query first.
        auto blockForSizeQuery = blockRange;
        if (auto sizeQuery = consumeSizeQuery(blockForSizeQuery))
            return { *sizeQuery };

        if (auto condition = consumeCondition<CQ::ContainerCondition>(blockRange))
            return { condition };
    }

    return { };
}

template<typename ConditionType>
std::optional<ConditionType> ContainerQueryParser::consumeCondition(CSSParserTokenRange& range)
{
    auto consumeQuery = [&](CSSParserTokenRange& range) {
        if constexpr (std::is_same_v<CQ::ContainerCondition, ConditionType>)
            return consumeContainerQuery(range);
        if constexpr (std::is_same_v<CQ::SizeCondition, ConditionType>) {
            if (range.peek().type() != LeftParenthesisToken)
                return std::optional<CQ::SizeQuery>();
            auto blockRange = range.consumeBlock();
            return consumeSizeQuery(blockRange);
        }
    };

    if (range.peek().type() == IdentToken) {
        if (range.peek().id() == CSSValueNot) {
            range.consumeIncludingWhitespace();
            if (auto query = consumeQuery(range))
                return ConditionType { CQ::LogicalOperator::Not, { *query } };
            return { };
        }
    }

    ConditionType condition;

    auto query = consumeQuery(range);
    if (!query)
        return { };

    condition.queries.append(*query);
    range.consumeWhitespace();

    auto consumeOperator = [&]() -> std::optional<CQ::LogicalOperator> {
        auto operatorToken = range.consumeIncludingWhitespace();
        if (operatorToken.type() != IdentToken)
            return { };
        if (operatorToken.id() == CSSValueAnd)
            return CQ::LogicalOperator::And;
        if (operatorToken.id() == CSSValueOr)
            return CQ::LogicalOperator::Or;
        return { };
    };

    while (!range.atEnd()) {
        auto op = consumeOperator();
        if (!op)
            return { };
        if (condition.queries.size() > 1 && condition.logicalOperator != *op)
            return { };

        condition.logicalOperator = *op;

        auto query = consumeQuery(range);
        if (!query)
            return { };

        condition.queries.append(*query);
        range.consumeWhitespace();
    }

    return condition;
}

std::optional<CQ::SizeQuery> ContainerQueryParser::consumeSizeQuery(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || range.peek().id() == CSSValueNot) {
        auto sizeCondition = consumeCondition<CQ::SizeCondition>(range);
        if (!sizeCondition)
            return { };
        return { *sizeCondition };
    }

    auto nameToken = range.consumeIncludingWhitespace();
    auto name = nameToken.value();

    if (range.atEnd())
        return CQ::SizeFeature { CQ::ComparisonOperator::True, name.toAtomString(), { } };

    auto opToken = range.consumeIncludingWhitespace();
    if (range.atEnd())
        return { };

    // FIXME: Support '<' style operators and ranges.
    auto op = CQ::ComparisonOperator::Equal;
    if (opToken.type() == ColonToken) {
        if (name.startsWith("min-"))
            op = CQ::ComparisonOperator::GreaterThanOrEqual;
        else if (name.startsWith("max-"))
            op = CQ::ComparisonOperator::LessThanOrEqual;

        if (op != CQ::ComparisonOperator::Equal)
            name = name.substring(4);
        if (name.isEmpty())
            return { };
    }

    auto length = CSSPropertyParserHelpers::consumeLength(range, m_context.mode, ValueRange::All);
    if (!length)
        return { };

    return CQ::SizeFeature { op, name.toAtomString(), length };
}

}

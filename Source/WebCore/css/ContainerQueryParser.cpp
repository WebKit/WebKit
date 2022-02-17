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
    if (range.peek().type() == LeftParenthesisToken || (range.peek().type() == IdentToken && range.peek().id() == CSSValueNot)) {
        auto sizeCondition = consumeCondition<CQ::SizeCondition>(range);
        if (!sizeCondition)
            return { };
        return { *sizeCondition };
    }

    auto sizeFeature = consumeSizeFeature(range);
    if (!sizeFeature)
        return { };

    return { *sizeFeature };
}

std::optional<CQ::SizeFeature> ContainerQueryParser::consumeSizeFeature(CSSParserTokenRange& range)
{
    auto rangeCopy = range;
    if (auto sizeFeature = consumePlainSizeFeature(range))
        return sizeFeature;

    range = rangeCopy;
    return consumeRangeSizeFeature(range);
}

std::optional<CQ::SizeFeature> ContainerQueryParser::consumePlainSizeFeature(CSSParserTokenRange& range)
{
    auto consumePlainFeatureName = [&]() -> std::pair<AtomString, CQ::ComparisonOperator> {
        if (range.peek().type() != IdentToken)
            return { };
        auto token = range.consumeIncludingWhitespace();
        auto name = token.value();
        if (name.startsWith("min-"))
            return { name.substring(4).toAtomString(), CQ::ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("max-"))
            return { name.substring(4).toAtomString(), CQ::ComparisonOperator::LessThanOrEqual };

        return { name.toAtomString(), CQ::ComparisonOperator::Equal };
    };

    auto [featureName, op] = consumePlainFeatureName();
    if (featureName.isEmpty())
        return { };

    range.consumeWhitespace();

    if (range.atEnd()) {
        if (op != CQ::ComparisonOperator::Equal)
            return { };
        return CQ::SizeFeature { featureName, { }, { } };
    }

    if (range.peek().type() != ColonToken)
        return { };

    range.consumeIncludingWhitespace();
    if (range.atEnd())
        return { };

    auto value = consumeValue(range);
    
    return CQ::SizeFeature { featureName, { }, CQ::Comparison { op, WTFMove(value) } };
}

std::optional<CQ::SizeFeature> ContainerQueryParser::consumeRangeSizeFeature(CSSParserTokenRange& range)
{
    auto consumeFeatureName = [&]() -> AtomString {
        if (range.peek().type() != IdentToken)
            return { };
        return range.consumeIncludingWhitespace().value().toAtomString();
    };

    auto consumeRangeOperator = [&]() -> std::optional<CQ::ComparisonOperator> {
        if (range.atEnd())
            return { };
        auto opToken = range.consume();
        if (range.atEnd() || opToken.type() != DelimiterToken)
            return { };

        switch (opToken.delimiter()) {
        case '=':
            range.consumeWhitespace();
            return CQ::ComparisonOperator::Equal;
        case '<':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return CQ::ComparisonOperator::LessThanOrEqual;
            }
            range.consumeWhitespace();
            return CQ::ComparisonOperator::LessThan;
        case '>':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return CQ::ComparisonOperator::GreaterThanOrEqual;
            }
            range.consumeWhitespace();
            return CQ::ComparisonOperator::GreaterThan;
        default:
            return { };
        }
    };

    auto consumeLeftComparison = [&]() -> std::optional<CQ::Comparison> {
        if (range.peek().type() == IdentToken)
            return { };
        auto value = consumeValue(range);
        auto op = consumeRangeOperator();
        if (!op)
            return { };

        return CQ::Comparison { *op, WTFMove(value) };
    };

    auto consumeRightComparison = [&]() -> std::optional<CQ::Comparison> {
        auto op = consumeRangeOperator();
        if (!op)
            return { };
        auto value = consumeValue(range);

        return CQ::Comparison { *op, WTFMove(value) };
    };

    auto leftComparison = consumeLeftComparison();

    auto featureName = consumeFeatureName();
    if (featureName.isEmpty())
        return { };

    auto rightComparison = consumeRightComparison();

    if (!leftComparison && !rightComparison)
        return { };

    return CQ::SizeFeature { WTFMove(featureName), WTFMove(leftComparison), WTFMove(rightComparison) };
}

RefPtr<CSSValue> ContainerQueryParser::consumeValue(CSSParserTokenRange& range)
{
    if (range.atEnd())
        return nullptr;
    if (auto value = CSSPropertyParserHelpers::consumeIdent(range))
        return value;
    if (auto value = CSSPropertyParserHelpers::consumeLength(range, m_context.mode, ValueRange::All))
        return value;
    if (auto value = CSSPropertyParserHelpers::consumeAspectRatioValue(range))
        return value;
    range.consumeIncludingWhitespace();
    return nullptr;
}

}

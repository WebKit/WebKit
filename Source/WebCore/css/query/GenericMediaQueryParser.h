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
#include "CSSParserTokenRange.h"
#include "GenericMediaQueryTypes.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

struct MediaQueryParserContext;

namespace MQ {

struct FeatureParser {
    static std::optional<Feature> consumeFeature(CSSParserTokenRange&, const MediaQueryParserContext&);
    static std::optional<Feature> consumeBooleanOrPlainFeature(CSSParserTokenRange&, const MediaQueryParserContext&);
    static std::optional<Feature> consumeRangeFeature(CSSParserTokenRange&, const MediaQueryParserContext&);
    static RefPtr<CSSValue> consumeValue(CSSParserTokenRange&, const MediaQueryParserContext&);

    static bool validateFeatureAgainstSchema(Feature&, const FeatureSchema&);
};

template<typename ConcreteParser>
struct GenericMediaQueryParser  {
    static std::optional<Condition> consumeCondition(CSSParserTokenRange&, const MediaQueryParserContext&);
    static std::optional<QueryInParens> consumeQueryInParens(CSSParserTokenRange&, const MediaQueryParserContext&);
    static std::optional<Feature> consumeAndValidateFeature(CSSParserTokenRange&, const MediaQueryParserContext&);

    static const FeatureSchema* schemaForFeatureName(const AtomString&, const MediaQueryParserContext&);
    static bool validateFeature(Feature&, const MediaQueryParserContext&);
};

template<typename ConcreteParser>
std::optional<Condition> GenericMediaQueryParser<ConcreteParser>::consumeCondition(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    if (range.peek().type() == IdentToken) {
        if (range.peek().id() == CSSValueNot) {
            range.consumeIncludingWhitespace();
            auto query = ConcreteParser::consumeQueryInParens(range, context);
            if (!query || !range.atEnd())
                return { };

            return Condition { LogicalOperator::Not, { *query } };
        }
    }

    Condition condition;

    auto query = ConcreteParser::consumeQueryInParens(range, context);
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

        auto query = ConcreteParser::consumeQueryInParens(range, context);
        if (!query)
            return { };

        condition.queries.append(*query);
        range.consumeWhitespace();
    }

    return condition;
}

template<typename ConcreteParser>
std::optional<QueryInParens> GenericMediaQueryParser<ConcreteParser>::consumeQueryInParens(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    if (range.peek().type() == FunctionToken) {
        auto name = range.peek().value();
        auto functionRange = range.consumeBlock();
        range.consumeWhitespace();
        return GeneralEnclosed { name.toString(), functionRange.serialize() };
    }

    if (range.peek().type() == LeftParenthesisToken) {
        auto blockRange = range.consumeBlock();
        range.consumeWhitespace();

        blockRange.consumeWhitespace();

        auto conditionRange = blockRange;
        if (auto condition = ConcreteParser::consumeCondition(conditionRange, context))
            return { condition };

        auto featureRange = blockRange;
        if (auto feature = ConcreteParser::consumeAndValidateFeature(featureRange, context))
            return { *feature };

        return GeneralEnclosed { { }, blockRange.serialize() };
    }

    return { };
}

template<typename ConcreteParser>
std::optional<Feature> GenericMediaQueryParser<ConcreteParser>::consumeAndValidateFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto feature = FeatureParser::consumeFeature(range, context);
    if (!feature)
        return { };

    if (!validateFeature(*feature, context))
        return { };

    return feature;
}

template<typename ConcreteParser>
bool GenericMediaQueryParser<ConcreteParser>::validateFeature(Feature& feature, const MediaQueryParserContext& context)
{
    auto* schema = ConcreteParser::schemaForFeatureName(feature.name, context);
    if (!schema)
        return false;
    return FeatureParser::validateFeatureAgainstSchema(feature, *schema);
}

template<typename ConcreteParser>
const FeatureSchema* GenericMediaQueryParser<ConcreteParser>::schemaForFeatureName(const AtomString& name, const MediaQueryParserContext&)
{
    using SchemaMap = MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, const FeatureSchema*>;

    static NeverDestroyed<SchemaMap> schemas = [&] {
        auto entries = ConcreteParser::featureSchemas();
        SchemaMap map;
        for (auto& entry : entries)
            map.add(entry->name, entry);
        return map;
    }();

    return schemas->get(name);
}

}
}

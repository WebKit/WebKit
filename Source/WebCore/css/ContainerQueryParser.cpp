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

std::optional<CQ::ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ContainerQueryParser parser(context);
    return parser.consumeContainerQuery(range);
}

std::optional<CQ::ContainerQuery> ContainerQueryParser::consumeContainerQuery(CSSParserTokenRange& range)
{
    auto consumeName = [&] {
        if (range.peek().type() == LeftParenthesisToken || range.peek().type() == FunctionToken)
            return nullAtom();
        auto nameValue = CSSPropertyParserHelpers::consumeSingleContainerName(range);
        if (!nameValue)
            return nullAtom();
        return AtomString { nameValue->stringValue() };
    };

    auto name = consumeName();

    m_requiredAxes = { };

    auto condition = consumeCondition<CQ::ContainerCondition>(range);
    if (!condition)
        return { };

    return CQ::ContainerQuery { name, m_requiredAxes, *condition };
}

std::optional<CQ::QueryInParens> ContainerQueryParser::consumeQueryInParens(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        auto name = range.peek().value();
        auto functionRange = range.consumeBlock();
        // This is where we would support style() queries.
        return MQ::GeneralEnclosed { name.toString(), functionRange.serialize() };
    }

    if (range.peek().type() == LeftParenthesisToken) {
        auto blockRange = range.consumeBlock();
        range.consumeWhitespace();

        blockRange.consumeWhitespace();

        // Try to parse as a condition first.
        auto conditionRange = blockRange;
        if (auto condition = consumeCondition<CQ::ContainerCondition>(conditionRange))
            return { condition };

        if (auto sizeFeature = consumeSizeFeature(blockRange))
            return { *sizeFeature };

        return MQ::GeneralEnclosed { { }, blockRange.serialize() };
    }

    return { };
}

std::optional<CQ::SizeFeature> ContainerQueryParser::consumeSizeFeature(CSSParserTokenRange& range)
{
    auto sizeFeature = consumeFeature(range);
    if (!sizeFeature)
        return { };

    m_requiredAxes.add(CQ::requiredAxesForFeature(sizeFeature->name));
    return sizeFeature;
}

}

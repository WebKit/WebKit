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
#include "MediaQueryParser.h"

#include "CSSValueKeywords.h"

namespace WebCore {
namespace MQ {

MediaQueryList MediaQueryParser::consumeMediaQueryList(CSSParserTokenRange& range)
{
    MediaQueryList list;

    while (!range.atEnd()) {
        auto begin = range.begin();
        while (!range.atEnd() && range.peek().type() != CommaToken)
            range.consumeComponentValue();

        auto subrange = range.makeSubRange(begin, &range.peek());

        if (!range.atEnd())
            range.consumeIncludingWhitespace();

        auto consumeMediaQueryOrNotAll = [&] {
            if (auto query = consumeMediaQuery(subrange))
                return *query;
            // "A media query that does not match the grammar in the previous section must be replaced by not all during parsing."
            return MediaQuery { Prefix::Not, "all"_s };
        };

        list.append(consumeMediaQueryOrNotAll());
    }

    return list;
}

std::optional<MediaQuery> MediaQueryParser::consumeMediaQuery(CSSParserTokenRange& range)
{
    // <media-condition>

    auto rangeCopy = range;
    if (auto condition = consumeCondition<MediaCondition>(range))
        return MediaQuery { { }, { }, condition };

    range = rangeCopy;

    // [ not | only ]? <media-type> [ and <media-condition-without-or> ]

    auto consumePrefix = [&]() -> std::optional<Prefix> {
        if (range.peek().type() != IdentToken)
            return { };

        if (range.peek().id() == CSSValueNot) {
            range.consumeIncludingWhitespace();
            return Prefix::Not;
        }
        if (range.peek().id() == CSSValueOnly) {
            // 'only' doesn't do anything. It exists to hide the rule from legacy agents.
            range.consumeIncludingWhitespace();
            return Prefix::Only;
        }
        return { };
    };

    auto consumeMediaType = [&]() -> AtomString {
        if (range.peek().type() != IdentToken)
            return { };

        auto identifier = range.peek().id();
        if (identifier == CSSValueOnly || identifier == CSSValueNot || identifier == CSSValueAnd || identifier == CSSValueOr)
            return { };

        return range.consumeIncludingWhitespace().value().convertToASCIILowercaseAtom();
    };

    auto prefix = consumePrefix();
    auto mediaType = consumeMediaType();

    if (mediaType.isNull())
        return { };

    if (range.atEnd())
        return MediaQuery { prefix, mediaType, { } };

    if (range.peek().type() != IdentToken || range.peek().id() != CSSValueAnd)
        return { };

    range.consumeIncludingWhitespace();

    auto condition = consumeCondition<MediaCondition>(range);
    if (!condition)
        return { };

    if (condition->logicalOperator == LogicalOperator::Or)
        return { };

    return MediaQuery { prefix, mediaType, condition };
}

std::optional<MediaInParens> MediaQueryParser::consumeQueryInParens(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        auto name = range.peek().value();
        auto functionRange = range.consumeBlock();
        return GeneralEnclosed { name.toString(), functionRange.serialize() };
    }

    if (range.peek().type() == LeftParenthesisToken) {
        auto blockRange = range.consumeBlock();
        range.consumeWhitespace();

        blockRange.consumeWhitespace();

        auto conditionRange = blockRange;
        if (auto condition = consumeCondition<MediaCondition>(conditionRange))
            return { condition };

        if (auto feature = consumeMediaFeature(blockRange))
            return { *feature };

        return GeneralEnclosed { { }, blockRange.serialize() };
    }

    return { };
}

std::optional<MediaFeature> MediaQueryParser::consumeMediaFeature(CSSParserTokenRange& range)
{
    return consumeFeature(range);
}

}
}

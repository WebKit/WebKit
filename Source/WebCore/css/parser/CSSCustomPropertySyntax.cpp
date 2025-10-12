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
#include "CSSCustomPropertySyntax.h"

#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSTokenizer.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

template<typename CharacterType>
auto CSSCustomPropertySyntax::parseComponent(std::span<const CharacterType> span) -> std::optional<Component>
{
    StringParsingBuffer buffer { span };

    auto consumeMultiplier = [&] {
        if (skipExactly(buffer, '+'))
            return Multiplier::SpaceList;
        if (skipExactly(buffer, '#'))
            return Multiplier::CommaList;
        return Multiplier::Single;
    };

    if (skipExactly(buffer, '<')) {
        auto begin = buffer.span();
        skipUntil(buffer, '>');
        if (buffer.position() == begin.data())
            return { };

        auto dataTypeName = StringView(begin.first(buffer.position() - begin.data()));
        if (!skipExactly(buffer, '>'))
            return { };

        auto multiplier = consumeMultiplier();

        skipWhile<isCSSSpace>(buffer);
        if (!buffer.atEnd())
            return { };

        auto type = typeForTypeName(dataTypeName);

        // <transform-list> is a pre-multiplied data type.
        // https://drafts.css-houdini.org/css-properties-values-api/#multipliers
        if (type == Type::TransformList && multiplier != Multiplier::Single)
            type = Type::Unknown;

        return Component { type, multiplier };
    }

    auto begin = buffer.span();
    while (buffer.hasCharactersRemaining() && (*buffer != '+' && *buffer != '#'))
        ++buffer;

    auto ident = [&] {
        auto tokenizer = CSSTokenizer::tryCreate(StringView(begin.first(buffer.position() - begin.data())).toStringWithoutCopying());
        if (!tokenizer)
            return nullAtom();

        auto range = tokenizer->tokenRange();
        range.consumeWhitespace();
        if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
            return nullAtom();

        auto value = range.consumeIncludingWhitespace().value();
        return range.atEnd() ? value.toAtomString() : nullAtom();
    }();

    if (ident.isNull())
        return { };

    auto multiplier = consumeMultiplier();

    return Component { Type::CustomIdent, multiplier, ident };
}

std::optional<CSSCustomPropertySyntax> CSSCustomPropertySyntax::parse(StringView syntax)
{
    // The format doesn't quite parse with CSSTokenizer.
    return readCharactersForParsing(syntax, [&](auto buffer) -> std::optional<CSSCustomPropertySyntax> {
        skipWhile<isCSSSpace>(buffer);

        if (skipExactly(buffer, '*')) {
            skipWhile<isCSSSpace>(buffer);
            if (buffer.hasCharactersRemaining())
                return { };

            return universal();
        }

        Definition definition;

        while (buffer.hasCharactersRemaining()) {
            auto begin = buffer.span();

            skipUntil(buffer, '|');

            auto component = parseComponent(begin.first(buffer.position() - begin.data()));
            if (!component)
                return { };

            definition.append(*component);

            skipExactly(buffer, '|');
            skipWhile<isCSSSpace>(buffer);
        }

        if (definition.isEmpty())
            return { };

        return CSSCustomPropertySyntax { definition };
    });
}

std::optional<CSSCustomPropertySyntax> CSSCustomPropertySyntax::consumeType(CSSParserTokenRange& range)
{
    // https://drafts.csswg.org/css-mixins/#typedef-css-type
    // <css-type> = <syntax-component> | <type()>

    if (range.peek().type() == FunctionToken) {
        // <type()> = type( <syntax> )
        if (!equalLettersIgnoringASCIICase(range.peek().value(), "type"_s))
            return { };

        auto typeRange = CSSPropertyParserHelpers::consumeFunction(range);
        auto type = CSSCustomPropertySyntax::parse(typeRange.serialize());
        if (!type || type->containsUnknownType())
            return { };
        return type;
    }

    auto typeRangeStart = range;
    while (!range.atEnd()) {
        auto peekType = range.peek().type();
        if (peekType == ColonToken || peekType == CommaToken || CSSTokenizer::isWhitespace(peekType))
            break;
        range.consume();
    }
    range.consumeWhitespace();

    auto typeRange = typeRangeStart.rangeUntil(range);
    auto type = CSSCustomPropertySyntax::parse(typeRange.serialize());
    if (!type || type->definition.size() != 1 || type->containsUnknownType())
        return { };

    return type;
}

bool CSSCustomPropertySyntax::containsUnknownType() const
{
    for (auto& component : definition) {
        if (component.type == Type::Unknown)
            return true;
    }
    return false;
}

auto CSSCustomPropertySyntax::typeForTypeName(StringView dataTypeName) -> Type
{
    static constexpr std::pair<ComparableASCIILiteral, Type> mappings[] = {
        { "angle"_s, Type::Angle },
        { "color"_s, Type::Color },
        { "custom-ident"_s, Type::CustomIdent },
        { "image"_s, Type::Image },
        { "integer"_s, Type::Integer },
        { "length"_s, Type::Length },
        { "length-percentage"_s, Type::LengthPercentage },
        { "number"_s, Type::Number },
        { "percentage"_s, Type::Percentage },
        { "resolution"_s, Type::Resolution },
        { "string"_s, Type::String },
        { "time"_s, Type::Time },
        { "transform-function"_s, Type::TransformFunction },
        { "transform-list"_s, Type::TransformList },
        { "url"_s, Type::URL },
    };

    static constexpr SortedArrayMap typeMap { mappings };
    return typeMap.get(dataTypeName, Type::Unknown);
}

}

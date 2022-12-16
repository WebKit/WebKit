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
#include "CSSTokenizer.h"
#include "ParsingUtilities.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {

template<typename CharacterType>
auto CSSCustomPropertySyntax::parseComponent(StringParsingBuffer<CharacterType> buffer) -> std::optional<Component>
{
    auto consumeMultiplier = [&] {
        if (skipExactly(buffer, '+'))
            return Multiplier::SpaceList;
        if (skipExactly(buffer, '#'))
            return Multiplier::CommaList;
        return Multiplier::Single;
    };

    if (skipExactly(buffer, '<')) {
        auto begin = buffer.position();
        skipUntil(buffer, '>');
        if (buffer.position() == begin)
            return { };

        auto dataTypeName = StringView(begin, buffer.position() - begin);
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

    auto begin = buffer.position();
    while (buffer.hasCharactersRemaining() && (*buffer != '+' && *buffer != '#'))
        ++buffer;

    auto ident = [&] {
        auto tokenizer = CSSTokenizer::tryCreate(StringView(begin, buffer.position() - begin).toStringWithoutCopying());
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
            auto begin = buffer.position();

            skipUntil(buffer, '|');

            auto component = parseComponent(StringParsingBuffer { begin, buffer.position() });
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

auto CSSCustomPropertySyntax::typeForTypeName(StringView dataTypeName) -> Type
{
    static constexpr std::pair<ComparableASCIILiteral, Type> mappings[] = {
        { "angle", Type::Angle },
        { "color", Type::Color },
        { "custom-ident", Type::CustomIdent },
        { "image", Type::Image },
        { "integer", Type::Integer },
        { "length", Type::Length },
        { "length-percentage", Type::LengthPercentage },
        { "number", Type::Number },
        { "percentage", Type::Percentage },
        { "resolution", Type::Resolution },
        { "time", Type::Time },
        { "transform-function", Type::TransformFunction },
        { "transform-list", Type::TransformList },
        { "url", Type::URL },
    };

    static constexpr SortedArrayMap typeMap { mappings };
    return typeMap.get(dataTypeName, Type::Unknown);
}

}

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
#include "CSSPropertySyntax.h"

#include "CSSParserIdioms.h"
#include "CSSTokenizer.h"
#include "ParsingUtilities.h"

namespace WebCore {

template<typename CharacterType>
auto CSSPropertySyntax::parseComponent(StringParsingBuffer<CharacterType> buffer) -> std::optional<Component>
{
    if (skipExactly(buffer, '<')) {
        auto begin = buffer.position();
        skipUntil(buffer, '>');
        if (buffer.position() == begin)
            return { };

        auto dataTypeName = StringView(begin, buffer.position() - begin);
        if (!skipExactly(buffer, '>'))
            return { };

        auto multiplier = [&] {
            if (skipExactly(buffer, '+'))
                return Multiplier::SpaceList;
            if (skipExactly(buffer, '#'))
                return Multiplier::CommaList;
            return Multiplier::Single;
        }();

        skipWhile<isCSSSpace>(buffer);
        if (!buffer.atEnd())
            return { };

        if (dataTypeName == "length"_s)
            return Component { Type::Length, multiplier };
        if (dataTypeName == "length-percentage"_s)
            return Component { Type::LengthPercentage, multiplier };
        if (dataTypeName == "custom-ident"_s)
            return Component { Type::CustomIdent, multiplier };
        if (dataTypeName == "percentage"_s)
            return Component { Type::Percentage, multiplier };
        if (dataTypeName == "integer"_s)
            return Component { Type::Integer, multiplier };
        if (dataTypeName == "number"_s)
            return Component { Type::Number, multiplier };
        if (dataTypeName == "angle"_s)
            return Component { Type::Angle, multiplier };
        if (dataTypeName == "color"_s)
            return Component { Type::Color, multiplier };
        if (dataTypeName == "image"_s)
            return Component { Type::Image, multiplier };
        if (dataTypeName == "url"_s)
            return Component { Type::URL, multiplier };

        return Component { Type::Unknown, multiplier };
    }

    auto tokenizer = CSSTokenizer::tryCreate(buffer.stringViewOfCharactersRemaining().toStringWithoutCopying());
    if (!tokenizer)
        return { };

    auto range = tokenizer->tokenRange();
    range.consumeWhitespace();
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return { };

    auto ident = range.consumeIncludingWhitespace().value();
    if (!range.atEnd())
        return { };

    return Component { Type::CustomIdent, Multiplier::Single, ident.toAtomString() };
}

auto CSSPropertySyntax::parse(StringView syntax) -> Definition
{
    return readCharactersForParsing(syntax, [&](auto buffer) -> Definition {
        skipWhile<isCSSSpace>(buffer);

        if (skipExactly(buffer, '*')) {
            skipWhile<isCSSSpace>(buffer);
            if (buffer.hasCharactersRemaining())
                return { };

            return { { Type::Universal } };
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

        return definition;
    });
}

}

/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RFC8941.h"

#include "RFC7230.h"
#include <wtf/text/Base64.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace RFC8941 {

template<typename CharacterType> constexpr bool isEndOfToken(CharacterType character)
{
    return !RFC7230::isTokenCharacter(character) && character != ':' && character != '/';
}

template<typename CharacterType> constexpr bool isEndOfKey(CharacterType character)
{
    return !isASCIILower(character) && !isASCIIDigit(character) && character != '_' && character != '-' && character != '.' && character != '*';
}

// Parsing a key (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.3.3).
template<typename CharType> static StringView parseKey(StringParsingBuffer<CharType>& buffer LIFETIME_BOUND)
{
    if (buffer.atEnd() || !isASCIILower(*buffer))
        return { };
    auto keyStart = buffer.span();
    ++buffer;
    skipUntil<isEndOfKey>(buffer);
    return keyStart.first(buffer.position() - keyStart.data());
}

// Parsing a String (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.5).
template<typename CharType> static std::optional<String> parseString(StringParsingBuffer<CharType>& buffer)
{
    if (!skipExactly(buffer, '"'))
        return std::nullopt;

    StringBuilder builder;
    while (buffer.hasCharactersRemaining()) {
        if (skipExactly(buffer, '\\')) {
            if (buffer.atEnd())
                return std::nullopt;
            CharType nextChar = buffer.consume();
            if (nextChar != '"' && nextChar != '\\')
                return std::nullopt;
            builder.append(nextChar);
            continue;
        }
        if (skipExactly(buffer, '"'))
            return builder.toString();
        CharType c = buffer.consume();
        if ((c >= 0x00 && c <= 0x1F) || (c >= 0x7F && c <= 0xFF))
            return std::nullopt;
        builder.append(c);
    }
    return std::nullopt;
}

// Parsing an Integer or Decimal (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.4).
template<typename CharType> static std::optional<BareItem> parseNumber(StringParsingBuffer<CharType>& buffer)
{
    bool isNegative = false;
    if (skipExactly(buffer, '-'))
        isNegative = true;

    int64_t integerValue = 0;
    size_t integerDigits = 0;
    constexpr size_t maxIntegerDigits = 15;
    while (buffer.hasCharactersRemaining() && isASCIIDigit(*buffer)) {
        if (++integerDigits > maxIntegerDigits)
            return std::nullopt;
        integerValue = (integerValue * 10) + (*buffer - '0');
        ++buffer;
    }
    if (!integerDigits)
        return std::nullopt;
    if (!skipExactly(buffer, '.'))
        return isNegative ? -integerValue : integerValue;

    constexpr size_t maxDecimalIntegerDigits = 12;
    if (integerDigits > maxDecimalIntegerDigits)
        return std::nullopt;
    double fractionalValue = 0.0;
    size_t fractionalDigits = 0;
    constexpr size_t maxFractionalDigits = 3;
    double divisor = 1.0;
    while (buffer.hasCharactersRemaining() && isASCIIDigit(*buffer)) {
        if (++fractionalDigits > maxFractionalDigits)
            return std::nullopt;
        divisor *= 10.0;
        fractionalValue = fractionalValue * 10.0 + (*buffer - '0');
        ++buffer;
    }
    if (!fractionalDigits)
        return std::nullopt;
    double decimalValue = integerValue + (fractionalValue / divisor);
    return isNegative ? -decimalValue : decimalValue;
}

// Parsing a Token (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.6).
template<typename CharType> static std::optional<Token> parseToken(StringParsingBuffer<CharType>& buffer)
{
    if (buffer.atEnd() || (!isASCIIAlpha(*buffer) && *buffer != '*'))
        return std::nullopt;
    auto tokenStart = buffer.span();
    skipUntil<isEndOfToken>(buffer);
    return Token { String(tokenStart.first(buffer.position() - tokenStart.data())) };
}

// Parsing a Byte Sequence (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.7).
template<typename CharType> static std::optional<Vector<uint8_t>> parseByteSequence(StringParsingBuffer<CharType>& buffer)
{
    if (!skipExactly(buffer, ':'))
        return std::nullopt;
    auto begin = buffer.span();
    skipUntil(buffer, ':');
    auto byteSequence = begin.first(buffer.position() - begin.data());
    if (!skipExactly(buffer, ':'))
        return std::nullopt;
    if (byteSequence.empty())
        return Vector<uint8_t>();

    auto decoded = base64Decode(StringView(byteSequence));
    if (!decoded)
        return std::nullopt;
    return WTF::move(*decoded);
}

// Parsing a Boolean (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.8).
template<typename CharType> static std::optional<bool> parseBoolean(StringParsingBuffer<CharType>& buffer)
{
    if (!skipExactly(buffer, '?'))
        return std::nullopt;
    if (skipExactly(buffer, '1'))
        return true;
    if (skipExactly(buffer, '0'))
        return false;
    return std::nullopt;
}

// Parsing a Bare Item (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.3.1).
template<typename CharType> static std::optional<BareItem> parseBareItem(StringParsingBuffer<CharType>& buffer)
{
    if (buffer.atEnd())
        return std::nullopt;
    CharType c = *buffer;
    if (c == ':')
        return parseByteSequence(buffer);
    if (c == '"')
        return parseString(buffer);
    if (isASCIIAlpha(c) || c == '*')
        return parseToken(buffer);
    if (c == '?')
        return parseBoolean(buffer);
    if (c == '-' || isASCIIDigit(c))
        return parseNumber(buffer);
    return std::nullopt;
}

// Parsing Parameters (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.3.2).
template<typename CharType> static std::optional<Parameters> parseParameters(StringParsingBuffer<CharType>& buffer)
{
    HashMap<String, BareItem> parameters;
    while (buffer.hasCharactersRemaining()) {
        if (!skipExactly(buffer, ';'))
            break;
        skipWhile(buffer, ' ');
        StringView key = parseKey(buffer);
        if (key.isNull())
            return std::nullopt;
        BareItem value = true;
        if (skipExactly(buffer, '=')) {
            auto parsedValue = parseBareItem(buffer);
            if (!parsedValue)
                return std::nullopt;
            value = WTF::move(*parsedValue);
        }
        parameters.set(key.toString(), WTF::move(value));
    }
    return Parameters { WTF::move(parameters) };
}

// Parsing an item (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.3).
template<typename CharType> static std::optional<std::pair<BareItem, Parameters>> parseItem(StringParsingBuffer<CharType>& buffer)
{
    auto bareItem = parseBareItem(buffer);
    if (!bareItem)
        return std::nullopt;

    auto parameters = parseParameters(buffer);
    if (!parameters)
        return std::nullopt;

    return std::pair { WTF::move(*bareItem), WTF::move(*parameters) };
}

// Parsing an Inner List (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.1.2).
template<typename CharType> static std::optional<std::pair<InnerList, Parameters>> parseInnerList(StringParsingBuffer<CharType>& buffer)
{
    if (buffer.atEnd())
        return std::nullopt;

    if (!skipExactly(buffer, '('))
        return std::nullopt;

    InnerList list;
    while (buffer.hasCharactersRemaining()) {
        skipWhile(buffer, ' ');
        if (skipExactly(buffer, ')')) {
            auto parameters = parseParameters(buffer);
            if (!parameters)
                return std::nullopt;
            return std::pair { WTF::move(list), WTF::move(*parameters) };
        }
        auto item = parseItem(buffer);
        if (!item)
            return std::nullopt;
        list.append(WTF::move(*item));
        if (buffer.atEnd() || (*buffer != ')' && *buffer != ' '))
            break;
    }
    return std::nullopt;
}

// Parsing an item or inner list (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.1.1).
template<typename CharType> static std::optional<std::pair<ItemOrInnerList, Parameters>> parseItemOrInnerList(StringParsingBuffer<CharType>& buffer)
{
    if (buffer.hasCharactersRemaining() && *buffer == '(')
        return parseInnerList(buffer);
    return parseItem(buffer);
}

// Parsing a dictionary (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.2).
template<typename CharType> static std::optional<HashMap<String, std::pair<ItemOrInnerList, Parameters>>> parseDictionary(StringParsingBuffer<CharType>& buffer)
{
    HashMap<String, std::pair<ItemOrInnerList, Parameters>> dictionary;
    while (buffer.hasCharactersRemaining()) {
        auto key = parseKey(buffer);
        if (key.isNull())
            return std::nullopt;
        std::pair<ItemOrInnerList, Parameters> member;
        if (skipExactly(buffer, '=')) {
            auto parsedMember = parseItemOrInnerList(buffer);
            if (!parsedMember)
                return std::nullopt;
            member = WTF::move(*parsedMember);
        } else {
            BareItem value = true;
            auto parameters = parseParameters(buffer);
            if (!parameters)
                return std::nullopt;
            member = std::pair { WTF::move(value), WTF::move(*parameters) };
        }
        dictionary.set(key.toString(), WTF::move(member));
        skipWhile<isTabOrSpace>(buffer);
        if (buffer.atEnd())
            return dictionary;
        if (!skipExactly(buffer, ','))
            return std::nullopt;
        skipWhile<isTabOrSpace>(buffer);
        if (buffer.atEnd())
            return std::nullopt;
    }
    ASSERT(dictionary.isEmpty());
    return dictionary;
}

// Parsing a list (https://datatracker.ietf.org/doc/html/rfc8941#section-4.2.1).
template<typename CharType> static std::optional<Vector<std::pair<ItemOrInnerList, Parameters>>> parseList(StringParsingBuffer<CharType>& buffer)
{
    Vector<std::pair<ItemOrInnerList, Parameters>> list;
    while (buffer.hasCharactersRemaining()) {
        auto member = parseItemOrInnerList(buffer);
        if (!member)
            return std::nullopt;
        list.append(WTF::move(*member));
        skipWhile<isTabOrSpace>(buffer);
        if (buffer.atEnd())
            return list;
        if (!skipExactly(buffer, ','))
            return std::nullopt;
        skipWhile<isTabOrSpace>(buffer);
        if (buffer.atEnd())
            return std::nullopt;
    }
    ASSERT(list.isEmpty());
    return list;
}

// https://datatracker.ietf.org/doc/html/rfc8941#section-4.2 with type "item".
std::optional<std::pair<BareItem, Parameters>> parseItemStructuredFieldValue(StringView header)
{
    if (header.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(WTF::move(header), [](auto buffer) -> std::optional<std::pair<BareItem, Parameters>> {
        skipWhile(buffer, ' ');

        auto item = parseItem(buffer);
        if (!item)
            return std::nullopt;

        skipWhile(buffer, ' ');

        if (buffer.hasCharactersRemaining())
            return std::nullopt;
        return WTF::move(*item);
    });
}

// https://datatracker.ietf.org/doc/html/rfc8941#section-4.2 with type "list".
std::optional<Vector<std::pair<ItemOrInnerList, Parameters>>> parseListStructuredFieldValue(StringView header)
{
    if (header.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(WTF::move(header), [](auto buffer) -> std::optional<Vector<std::pair<ItemOrInnerList, Parameters>>> {
        skipWhile(buffer, ' ');

        auto list = parseList(buffer);
        if (!list)
            return std::nullopt;

        skipWhile(buffer, ' ');

        if (buffer.hasCharactersRemaining())
            return std::nullopt;
        return WTF::move(*list);
    });
}

// https://datatracker.ietf.org/doc/html/rfc8941#section-4.2 with type "dictionary".
std::optional<HashMap<String, std::pair<ItemOrInnerList, Parameters>>> parseDictionaryStructuredFieldValue(StringView header)
{
    if (header.isEmpty())
        return std::nullopt;

    return readCharactersForParsing(WTF::move(header), [](auto buffer) -> std::optional<HashMap<String, std::pair<ItemOrInnerList, Parameters>>> {
        skipWhile(buffer, ' ');

        auto dictionary = parseDictionary(buffer);
        if (!dictionary)
            return std::nullopt;

        skipWhile(buffer, ' ');

        if (buffer.hasCharactersRemaining())
            return std::nullopt;
        return WTF::move(*dictionary);
    });
}

} // namespace RFC8941

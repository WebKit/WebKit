/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/StdLibExtras.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WTF {

template<typename CharacterType, typename DelimiterType> bool skipExactly(const CharacterType*& position, const CharacterType* end, DelimiterType delimiter)
{
    if (position < end && *position == delimiter) {
        ++position;
        return true;
    }
    return false;
}

template<typename CharacterType, typename DelimiterType> bool skipExactly(std::span<CharacterType>& data, DelimiterType delimiter)
{
    if (!data.empty() && data.front() == delimiter) {
        skip(data, 1);
        return true;
    }
    return false;
}

template<typename CharacterType, typename DelimiterType> bool skipExactly(StringParsingBuffer<CharacterType>& buffer, DelimiterType delimiter)
{
    if (buffer.hasCharactersRemaining() && *buffer == delimiter) {
        ++buffer;
        return true;
    }
    return false;
}

template<bool characterPredicate(Latin1Character)> bool skipExactly(StringParsingBuffer<Latin1Character>& buffer)
{
    if (buffer.hasCharactersRemaining() && characterPredicate(*buffer)) {
        ++buffer;
        return true;
    }
    return false;
}

template<bool characterPredicate(char16_t)> bool skipExactly(StringParsingBuffer<char16_t>& buffer)
{
    if (buffer.hasCharactersRemaining() && characterPredicate(*buffer)) {
        ++buffer;
        return true;
    }
    return false;
}

template<bool characterPredicate(Latin1Character), typename CharacterType> bool skipExactly(std::span<CharacterType>& buffer) requires(std::is_same_v<std::remove_const_t<CharacterType>, Latin1Character>)
{
    if (!buffer.empty() && characterPredicate(buffer[0])) {
        skip(buffer, 1);
        return true;
    }
    return false;
}

template<bool characterPredicate(char16_t), typename CharacterType> bool skipExactly(std::span<CharacterType>& buffer) requires(std::is_same_v<std::remove_const_t<CharacterType>, char16_t>)
{
    if (!buffer.empty() && characterPredicate(buffer[0])) {
        skip(buffer, 1);
        return true;
    }
    return false;
}

template<bool characterPredicate(char8_t), typename CharacterType> bool skipExactly(std::span<CharacterType>& buffer) requires(std::is_same_v<std::remove_const_t<CharacterType>, char8_t>)
{
    if (!buffer.empty() && characterPredicate(buffer[0])) {
        skip(buffer, 1);
        return true;
    }
    return false;
}

template<bool characterPredicate(char), typename CharacterType> bool skipExactly(std::span<CharacterType>& buffer) requires(std::is_same_v<std::remove_const_t<CharacterType>, char>)
{
    if (!buffer.empty() && characterPredicate(buffer[0])) {
        skip(buffer, 1);
        return true;
    }
    return false;
}

template<typename CharacterType, typename DelimiterType> void skipUntil(StringParsingBuffer<CharacterType>& buffer, DelimiterType delimiter)
{
    while (buffer.hasCharactersRemaining() && *buffer != delimiter)
        ++buffer;
}

template<typename CharacterType, typename DelimiterType> void skipUntil(std::span<CharacterType>& buffer, DelimiterType delimiter)
{
    size_t index = 0;
    while (index < buffer.size() && buffer[index] != delimiter)
        ++index;
    skip(buffer, index);
}

template<bool characterPredicate(Latin1Character), typename CharacterType> void skipUntil(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, Latin1Character>)
{
    size_t index = 0;
    while (index < data.size() && !characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char16_t), typename CharacterType> void skipUntil(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char16_t>)
{
    size_t index = 0;
    while (index < data.size() && !characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char8_t), typename CharacterType> void skipUntil(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char8_t>)
{
    size_t index = 0;
    while (index < data.size() && !characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char), typename CharacterType> void skipUntil(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char>)
{
    size_t index = 0;
    while (index < data.size() && !characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(Latin1Character)> void skipUntil(StringParsingBuffer<Latin1Character>& buffer)
{
    while (buffer.hasCharactersRemaining() && !characterPredicate(*buffer))
        ++buffer;
}

template<bool characterPredicate(char16_t)> void skipUntil(StringParsingBuffer<char16_t>& buffer)
{
    while (buffer.hasCharactersRemaining() && !characterPredicate(*buffer))
        ++buffer;
}

template<typename CharacterType, typename DelimiterType> void skipWhile(StringParsingBuffer<CharacterType>& buffer, DelimiterType delimiter)
{
    while (buffer.hasCharactersRemaining() && *buffer == delimiter)
        ++buffer;
}

template<typename CharacterType, typename DelimiterType> void skipWhile(std::span<CharacterType>& buffer, DelimiterType delimiter)
{
    size_t index = 0;
    while (index < buffer.size() && buffer[index] == delimiter)
        ++index;
    skip(buffer, index);
}

template<bool characterPredicate(Latin1Character), typename CharacterType> void skipWhile(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, Latin1Character>)
{
    size_t index = 0;
    while (index < data.size() && characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char16_t), typename CharacterType> void skipWhile(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char16_t>)
{
    size_t index = 0;
    while (index < data.size() && characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char8_t), typename CharacterType> void skipWhile(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char8_t>)
{
    size_t index = 0;
    while (index < data.size() && characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(char), typename CharacterType> void skipWhile(std::span<CharacterType>& data) requires(std::is_same_v<std::remove_const_t<CharacterType>, char>)
{
    size_t index = 0;
    while (index < data.size() && characterPredicate(data[index]))
        ++index;
    skip(data, index);
}

template<bool characterPredicate(Latin1Character)> void skipWhile(StringParsingBuffer<Latin1Character>& buffer)
{
    while (buffer.hasCharactersRemaining() && characterPredicate(*buffer))
        ++buffer;
}

template<bool characterPredicate(char16_t)> void skipWhile(StringParsingBuffer<char16_t>& buffer)
{
    while (buffer.hasCharactersRemaining() && characterPredicate(*buffer))
        ++buffer;
}

template<typename CharacterType> bool skipExactlyIgnoringASCIICase(StringParsingBuffer<CharacterType>& buffer, ASCIILiteral literal)
{
    auto literalLength = literal.length();

    if (buffer.lengthRemaining() < literalLength)
        return false;
    if (!equalLettersIgnoringASCIICaseWithLength(buffer.span(), literal.span8(), literalLength))
        return false;
    buffer += literalLength;
    return true;
}

template<typename CharacterType, std::size_t Extent> bool skipLettersExactlyIgnoringASCIICase(StringParsingBuffer<CharacterType>& buffer, std::span<const CharacterType, Extent> letters)
{
    if (buffer.lengthRemaining() < letters.size())
        return false;
    for (unsigned i = 0; i < letters.size(); ++i) {
        ASSERT(isASCIIAlpha(letters[i]));
        if (!isASCIIAlphaCaselessEqual(buffer[i], static_cast<char>(letters[i])))
            return false;
    }
    buffer += letters.size();
    return true;
}

template<typename CharacterType, std::size_t Extent> bool skipLettersExactlyIgnoringASCIICase(std::span<const CharacterType>& buffer, std::span<const CharacterType, Extent> letters)
{
    if (buffer.size() < letters.size())
        return false;
    if (!equalLettersIgnoringASCIICaseWithLength(buffer, letters, letters.size()))
        return false;
    skip(buffer, letters.size());
    return true;
}

template<typename CharacterType, std::size_t Extent> constexpr bool skipCharactersExactly(StringParsingBuffer<CharacterType>& buffer, std::span<const CharacterType, Extent> string)
{
    if (!spanHasPrefix(buffer.span(), string))
        return false;
    buffer += string.size();
    return true;
}

template<typename CharacterType, std::size_t Extent> constexpr bool skipCharactersExactly(std::span<const CharacterType>& buffer, std::span<const CharacterType, Extent> string)
{
    if (!spanHasPrefix(buffer, string))
        return false;
    skip(buffer, string.size());
    return true;
}

// Adapt a char16_t-predicate to a Latin1Character-predicate.
template<bool characterPredicate(char16_t)>
static inline bool Latin1CharacterPredicateAdapter(Latin1Character c) { return characterPredicate(c); }

} // namespace WTF

using WTF::Latin1CharacterPredicateAdapter;
using WTF::skipCharactersExactly;
using WTF::skipExactly;
using WTF::skipExactlyIgnoringASCIICase;
using WTF::skipLettersExactlyIgnoringASCIICase;
using WTF::skipUntil;
using WTF::skipWhile;

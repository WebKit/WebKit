/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include <wtf/text/StringCommon.h>

namespace WebCore {

template<typename CharacterType> inline bool isNotASCIISpace(CharacterType c)
{
    return !isASCIISpace(c);
}
    
template<typename CharacterType, typename DelimiterType> bool skipExactly(const CharacterType*& position, const CharacterType* end, DelimiterType delimiter)
{
    if (position < end && *position == delimiter) {
        ++position;
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

template<typename CharacterType, bool characterPredicate(CharacterType)> bool skipExactly(const CharacterType*& position, const CharacterType* end)
{
    if (position < end && characterPredicate(*position)) {
        ++position;
        return true;
    }
    return false;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> bool skipExactly(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.hasCharactersRemaining() && characterPredicate(*buffer)) {
        ++buffer;
        return true;
    }
    return false;
}

template<typename CharacterType, typename DelimiterType> void skipUntil(const CharacterType*& position, const CharacterType* end, DelimiterType delimiter)
{
    while (position < end && *position != delimiter)
        ++position;
}

template<typename CharacterType, typename DelimiterType> void skipUntil(StringParsingBuffer<CharacterType>& buffer, DelimiterType delimiter)
{
    while (buffer.hasCharactersRemaining() && *buffer != delimiter)
        ++buffer;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> void skipUntil(const CharacterType*& position, const CharacterType* end)
{
    while (position < end && !characterPredicate(*position))
        ++position;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> void skipUntil(StringParsingBuffer<CharacterType>& buffer)
{
    while (buffer.hasCharactersRemaining() && !characterPredicate(*buffer))
        ++buffer;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> void skipWhile(const CharacterType*& position, const CharacterType* end)
{
    while (position < end && characterPredicate(*position))
        ++position;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> void skipWhile(StringParsingBuffer<CharacterType>& buffer)
{
    while (buffer.hasCharactersRemaining() && characterPredicate(*buffer))
        ++buffer;
}

template<typename CharacterType, bool characterPredicate(CharacterType)> void reverseSkipWhile(const CharacterType*& position, const CharacterType* start)
{
    while (position >= start && characterPredicate(*position))
        --position;
}

template<typename CharacterType, unsigned lowercaseLettersArraySize> bool skipExactlyIgnoringASCIICase(const CharacterType*& position, const CharacterType* end, const char (&lowercaseLetters)[lowercaseLettersArraySize])
{
    constexpr auto lowercaseLettersLength = lowercaseLettersArraySize - 1;
    
    if (position + lowercaseLettersLength > end)
        return false;
    if (!WTF::equalLettersIgnoringASCIICase(position, lowercaseLettersLength, lowercaseLetters))
        return false;
    position += lowercaseLettersLength;
    return true;
}

template<typename CharacterType, unsigned lowercaseLettersArraySize> bool skipExactlyIgnoringASCIICase(StringParsingBuffer<CharacterType>& buffer, const char (&lowercaseLetters)[lowercaseLettersArraySize])
{
    constexpr auto lowercaseLettersLength = lowercaseLettersArraySize - 1;

    if (buffer.lengthRemaining() < lowercaseLettersLength)
        return false;
    if (!WTF::equalLettersIgnoringASCIICase(buffer.position(), lowercaseLettersLength, lowercaseLetters))
        return false;
    buffer += lowercaseLettersLength;
    return true;
}

template<typename CharacterType, unsigned characterCount> constexpr bool skipCharactersExactly(const CharacterType*& ptr, const CharacterType* end, const CharacterType(&str)[characterCount])
{
    if (end - ptr < characterCount)
        return false;
    if (memcmp(str, ptr, sizeof(CharacterType) * characterCount))
        return false;
    ptr += characterCount;
    return true;
}

template<typename CharacterType, unsigned characterCount> constexpr bool skipCharactersExactly(StringParsingBuffer<CharacterType>& buffer, const CharacterType(&str)[characterCount])
{
    if (buffer.lengthRemaining() < characterCount)
        return false;
    if (memcmp(str, buffer.position(), sizeof(CharacterType) * characterCount))
        return false;
    buffer += characterCount;
    return true;
}

} // namespace WebCore

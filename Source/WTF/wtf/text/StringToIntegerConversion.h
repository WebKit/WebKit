/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CheckedArithmetic.h>
#include <wtf/text/StringView.h>

namespace WTF {

// The parseInteger function template allows leading and trailing spaces as defined by isUnicodeCompatibleASCIIWhitespace, and, after the leading spaces, allows a single leading "+".
// The parseIntegerAllowingTrailingJunk function template is like parseInteger, but allows any characters after the integer.

// FIXME: Should we add a version that does not allow "+"?
// FIXME: Should we add a version that allows other definitions of spaces, like isASCIIWhitespace or isASCIIWhitespaceWithoutFF?
// FIXME: Should we add a version that does not allow leading and trailing spaces?

template<typename IntegralType> std::optional<IntegralType> parseInteger(StringView, uint8_t base = 10);
template<typename IntegralType> std::optional<IntegralType> parseIntegerAllowingTrailingJunk(StringView, uint8_t base = 10);

enum class TrailingJunkPolicy { Disallow, Allow };

template<typename IntegralType, typename CharacterType> std::optional<IntegralType> parseInteger(std::span<const CharacterType> data, uint8_t base, TrailingJunkPolicy policy)
{
    if (!data.data())
        return std::nullopt;

    while (!data.empty() && isUnicodeCompatibleASCIIWhitespace(data.front()))
        data = data.subspan(1);

    bool isNegative = false;
    if (std::is_signed_v<IntegralType> && !data.empty() && data.front() == '-') {
        data = data.subspan(1);
        isNegative = true;
    } else if (!data.empty() && data.front() == '+')
        data = data.subspan(1);

    auto isCharacterAllowedInBase = [] (auto character, auto base) {
        if (isASCIIDigit(character))
            return character - '0' < base;
        return toASCIILowerUnchecked(character) >= 'a' && toASCIILowerUnchecked(character) < 'a' + std::min(base - 10, 26);
    };

    if (!(!data.empty() && isCharacterAllowedInBase(data.front(), base)))
        return std::nullopt;

    Checked<IntegralType, RecordOverflow> value;
    do {
        IntegralType digitValue = isASCIIDigit(data.front()) ? data.front() - '0' : toASCIILowerUnchecked(data.front()) - 'a' + 10;
        data = data.subspan(1);
        value *= static_cast<IntegralType>(base);
        if (isNegative)
            value -= digitValue;
        else
            value += digitValue;
    } while (!data.empty() && isCharacterAllowedInBase(data.front(), base));

    if (UNLIKELY(value.hasOverflowed()))
        return std::nullopt;

    if (policy == TrailingJunkPolicy::Disallow) {
        while (!data.empty() && isUnicodeCompatibleASCIIWhitespace(data.front()))
            data = data.subspan(1);
        if (!data.empty())
            return std::nullopt;
    }

    return value.value();
}

template<typename IntegralType> std::optional<IntegralType> parseInteger(StringView string, uint8_t base)
{
    if (string.is8Bit())
        return parseInteger<IntegralType>(string.span8(), base, TrailingJunkPolicy::Disallow);
    return parseInteger<IntegralType>(string.span16(), base, TrailingJunkPolicy::Disallow);
}

template<typename IntegralType> std::optional<IntegralType> parseIntegerAllowingTrailingJunk(StringView string, uint8_t base)
{
    if (string.is8Bit())
        return parseInteger<IntegralType>(string.span8(), base, TrailingJunkPolicy::Allow);
    return parseInteger<IntegralType>(string.span16(), base, TrailingJunkPolicy::Allow);
}

}

using WTF::parseInteger;
using WTF::parseIntegerAllowingTrailingJunk;

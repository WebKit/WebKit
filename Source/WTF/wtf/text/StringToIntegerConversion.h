/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include <unicode/utypes.h>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>

namespace WTF {

inline bool isCharacterAllowedInBase(UChar c, int base)
{
    if (c > 0x7F)
        return false;
    if (isASCIIDigit(c))
        return c - '0' < base;
    if (isASCIIAlpha(c)) {
        if (base > 36)
            base = 36;
        return (c >= 'a' && c < 'a' + base - 10)
            || (c >= 'A' && c < 'A' + base - 10);
    }
    return false;
}

template<typename IntegralType, typename CharacterType>
inline IntegralType toIntegralType(const CharacterType* data, size_t length, bool* ok, int base = 10)
{
    static constexpr bool isSigned = std::numeric_limits<IntegralType>::is_signed;

    Checked<IntegralType, RecordOverflow> value;
    bool isOk = false;
    bool isNegative = false;

    if (!data)
        goto bye;

    // skip leading whitespace
    while (length && isSpaceOrNewline(*data)) {
        --length;
        ++data;
    }

    if (isSigned && length && *data == '-') {
        --length;
        ++data;
        isNegative = true;
    } else if (length && *data == '+') {
        --length;
        ++data;
    }

    if (!length || !isCharacterAllowedInBase(*data, base))
        goto bye;

    while (length && isCharacterAllowedInBase(*data, base)) {
        --length;
        IntegralType digitValue;
        auto c = *data;
        if (isASCIIDigit(c))
            digitValue = c - '0';
        else if (c >= 'a')
            digitValue = c - 'a' + 10;
        else
            digitValue = c - 'A' + 10;

        value *= static_cast<IntegralType>(base);
        if (isNegative)
            value -= digitValue;
        else
            value += digitValue;
        ++data;
    }

    if (UNLIKELY(value.hasOverflowed()))
        goto bye;

    // skip trailing space
    while (length && isSpaceOrNewline(*data)) {
        --length;
        ++data;
    }

    if (!length)
        isOk = true;
bye:
    if (ok)
        *ok = isOk;
    return isOk ? value.unsafeGet() : 0;
}

template<typename IntegralType, typename StringOrStringView>
inline IntegralType toIntegralType(const StringOrStringView& stringView, bool* ok, int base = 10)
{
    if (stringView.is8Bit())
        return toIntegralType<IntegralType, LChar>(stringView.characters8(), stringView.length(), ok, base);
    return toIntegralType<IntegralType, UChar>(stringView.characters16(), stringView.length(), ok, base);
}

template<typename IntegralType, typename CharacterType>
inline Optional<IntegralType> toIntegralType(const CharacterType* data, size_t length, int base = 10)
{
    bool ok = false;
    IntegralType value = toIntegralType<IntegralType>(data, length, &ok, base);
    if (!ok)
        return WTF::nullopt;
    return value;
}

template<typename IntegralType, typename StringOrStringView>
inline Optional<IntegralType> toIntegralType(const StringOrStringView& stringView, int base = 10)
{
    bool ok = false;
    IntegralType value = toIntegralType<IntegralType>(stringView, &ok, base);
    if (!ok)
        return WTF::nullopt;
    return value;
}

}

using WTF::toIntegralType;

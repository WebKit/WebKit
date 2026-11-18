/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <array>
#include <bit>
#include <optional>
#include <wtf/ASCIICType.h>
#include <wtf/SIMDHelpers.h>
#include <wtf/UnalignedAccess.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static constexpr size_t maxMantissaDigitsForDecimalNumber = 19;

// JSON requires a digit after '.'. JavaScript accepts "1." and "1.e5".
enum class DecimalFractionSyntax : bool { RequiresDigit, AllowsEmpty };

template<typename CharType>
struct ScannedDecimalNumber {
    const CharType* end;
    uint64_t mantissa;
    int64_t exponent;
    size_t digitCount;
};

// Finishes scanning a number without a sign whose integer part, start to integerEnd, has the value integer,
// rather than having the double parser scan it again from the start. The integer part may be empty, as in ".5",
// when a digit follows the '.'. Returns std::nullopt for more than maxMantissaDigitsForDecimalNumber significant
// digits, and for every syntax error, which are left to the double parser. Scanning stops at the first character
// that cannot continue the number, so the caller must check what follows it, such as a JavaScript numeric
// separator or BigInt suffix.
template<DecimalFractionSyntax fractionSyntax, typename CharType>
ALWAYS_INLINE std::optional<ScannedDecimalNumber<CharType>> scanDecimalNumber(const CharType* start, const CharType* integerEnd, const CharType* end, uint64_t integer)
{
    size_t integerDigits = integerEnd - start;
    if (integerDigits > maxMantissaDigitsForDecimalNumber) [[unlikely]]
        return std::nullopt;

    uint64_t mantissa = integer;
    const CharType* cursor = integerEnd;
    const CharType* fractionStart = integerEnd;
    if (cursor < end && *cursor == '.') [[likely]] {
        fractionStart = ++cursor;
        bool sawNonDigit = false;
        // Full precision doubles have long fractions, so take eight digits at a time, and the digits
        // left before the first non-digit in the last step.
        static constexpr std::array<uint64_t, 8> powersOfTen { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 };
        auto valueOfDigits = [](uint64_t digits) ALWAYS_INLINE_LAMBDA {
            digits = digits * 10 + (digits >> 8);
            return static_cast<uint32_t>((((digits & 0x000000FF000000FFULL) * 0x000F424000000064ULL) + (((digits >> 16) & 0x000000FF000000FFULL) * 0x0000271000000001ULL)) >> 32);
        };
        while (end - cursor >= 8) {
            uint64_t characters;
            if constexpr (sizeof(CharType) == 1)
                characters = WTF::unalignedLoad<uint64_t>(cursor);
            else {
                // Saturating each character to a byte turns every character above Latin-1 into a non-digit.
                characters = SIMD::loadAndSaturateNarrowToU64(std::bit_cast<const uint16_t*>(cursor));
            }
#if CPU(BIG_ENDIAN)
            // The arithmetic below needs the first character in the lowest byte.
            characters = std::byteswap(characters);
#endif
            uint64_t nonDigits = ((characters + 0x4646464646464646ULL) | (characters - 0x3030303030303030ULL)) & 0x8080808080808080ULL;
            uint64_t digits = characters - 0x3030303030303030ULL;
            if (nonDigits) {
                // Carries and borrows only reach later characters, so the first non-digit is flagged exactly.
                unsigned count = std::countr_zero(nonDigits) / 8;
                if (count) {
                    mantissa = mantissa * powersOfTen[count] + valueOfDigits(digits << (64 - 8 * count));
                    cursor += count;
                }
                sawNonDigit = true;
                break;
            }
            mantissa = mantissa * 100000000 + valueOfDigits(digits);
            cursor += 8;
        }
        if (!sawNonDigit) [[unlikely]] {
            while (cursor < end && isASCIIDigit(*cursor)) {
                mantissa = mantissa * 10 + (*cursor - '0');
                ++cursor;
            }
        }
        if (cursor == fractionStart) [[unlikely]] {
            if (fractionSyntax == DecimalFractionSyntax::RequiresDigit || !integerDigits)
                return std::nullopt;
        }
    }
    size_t fractionDigits = cursor - fractionStart;
    int64_t exponent = -static_cast<int64_t>(fractionDigits);
    if (cursor < end && (*cursor == 'e' || *cursor == 'E')) [[unlikely]] {
        ++cursor;
        bool negativeExponent = false;
        if (cursor < end && (*cursor == '+' || *cursor == '-'))
            negativeExponent = *cursor++ == '-';
        if (cursor >= end || !isASCIIDigit(*cursor)) [[unlikely]]
            return std::nullopt;
        // An exponent this large gives zero or infinity, unless as many leading zeros in the fraction offset it,
        // so it is left to the double parser.
        constexpr int64_t maxExplicitExponent = 100000;
        int64_t explicitExponent = 0;
        while (cursor < end && isASCIIDigit(*cursor)) {
            explicitExponent = explicitExponent * 10 + (*cursor - '0');
            if (explicitExponent >= maxExplicitExponent) [[unlikely]]
                return std::nullopt;
            ++cursor;
        }
        exponent += negativeExponent ? -explicitExponent : explicitExponent;
    }

    size_t digitCount = integerDigits + fractionDigits;
    if (digitCount > maxMantissaDigitsForDecimalNumber) [[unlikely]] {
        // An integer part of 0 and the zeros after it are not significant, and add nothing to the mantissa.
        if (integer)
            return std::nullopt;
        const CharType* firstSignificant = fractionStart;
        while (firstSignificant < cursor && *firstSignificant == '0')
            ++firstSignificant;
        digitCount = fractionStart + fractionDigits - firstSignificant;
        if (digitCount > maxMantissaDigitsForDecimalNumber)
            return std::nullopt;
    }
    return ScannedDecimalNumber<CharType> { cursor, mantissa, exponent, digitCount };
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

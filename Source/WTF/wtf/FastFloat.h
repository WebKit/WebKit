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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <cfloat>
#include <optional>
#include <span>
#include <unicode/utypes.h>
#include <wtf/ASCIICType.h>

namespace WTF {

WTF_EXPORT_PRIVATE double parseDouble(std::span<const Latin1Character> string, size_t& parsedLength);
WTF_EXPORT_PRIVATE double parseDouble(std::span<const char16_t> string, size_t& parsedLength);

WTF_EXPORT_PRIVATE double parseFixedDouble(std::span<const Latin1Character> string, size_t& parsedLength);
WTF_EXPORT_PRIVATE double parseFixedDouble(std::span<const char16_t> string, size_t& parsedLength);

WTF_EXPORT_PRIVATE std::optional<double> parseJSONDouble(std::span<const Latin1Character> string, size_t& parsedLength);
WTF_EXPORT_PRIVATE std::optional<double> parseJSONDouble(std::span<const char16_t> string, size_t& parsedLength);

WTF_EXPORT_PRIVATE double parseJSONDoubleFromDigitsSlow(uint64_t mantissa, int64_t exponent);

// Requires a mantissa of at most |digitCount| <= 19 significant digits. The result is mantissa * 10^exponent.
ALWAYS_INLINE double parseJSONDoubleFromDigits(uint64_t mantissa, int64_t exponent, size_t digitCount)
{
    // Fifteen digits are always below 2^53. Unlike the mantissa, the digit count is known before the
    // mantissa is computed, and it does not alternate between full precision doubles of 16 and 17 digits.
    constexpr size_t maxDigitsOfExactInteger = 15;
    // With extended precision intermediates, as on x87, the operation would be rounded twice.
    constexpr bool roundsOnceToDouble = FLT_EVAL_METHOD >= 0 && FLT_EVAL_METHOD <= 1;
    if (roundsOnceToDouble && digitCount <= maxDigitsOfExactInteger && exponent >= -22 && exponent <= 22) {
        // Both operands are exact, so one correctly rounded operation gives the correctly rounded result.
        static constexpr std::array<double, 23> exactPowersOfTen {
            1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
            1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
        };
        double value = static_cast<double>(mantissa);
        return exponent < 0 ? value / exactPowersOfTen[-exponent] : value * exactPowersOfTen[exponent];
    }
    return parseJSONDoubleFromDigitsSlow(mantissa, exponent);
}

} // namespace WTF

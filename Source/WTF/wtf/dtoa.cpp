/*
 *  Copyright (C) 2003-2024 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include <wtf/dtoa.h>

#include <bit>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ASCIILiteral.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
IGNORE_CLANG_WARNINGS_BEGIN("comma")
#include "zmij/zmij.h"
IGNORE_CLANG_WARNINGS_END
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace WTF {

static_assert(numberToShortestStringBufferSize >= zmij::double_buffer_size);
static_assert(sizeof(NumberToStringBuffer) >= numberToShortestStringBufferSize);

using ShortestStringBuffer = std::span<char, numberToShortestStringBufferSize>;

static constexpr uint64_t eightASCIIZeros = 0x3030303030303030;

// Splits value (< 10^8) into one decimal digit per byte, the most significant digit in the lowest byte.
static ALWAYS_INLINE uint64_t eightDecimalDigits(uint32_t value)
{
    uint64_t halves = (value / 10'000) | (static_cast<uint64_t>(value % 10'000) << 32);
    uint64_t hundreds = ((halves * 10'486) >> 20) & 0x0000007F0000007F;
    uint64_t pairs = hundreds | ((halves - hundreds * 100) << 16);
    uint64_t tens = ((pairs * 103) >> 10) & 0x000F000F000F000F;
    return tens | ((pairs - tens * 10) << 8);
}

static ALWAYS_INLINE void storeEightCharacters(std::span<char> destination, uint64_t characters)
{
    if constexpr (std::endian::native == std::endian::big)
        characters = std::byteswap(characters);
    memcpySpan(destination.first<8>(), asByteSpan(characters));
}

static ALWAYS_INLINE unsigned decimalDigitCount(uint64_t value)
{
    static constexpr std::array<uint64_t, 20> powersOfTen = [] {
        std::array<uint64_t, 20> powers { };
        uint64_t power = 1;
        for (auto& entry : powers) {
            entry = power;
            power *= 10;
        }
        return powers;
    }();
    // 1'233 / 4'096 approximates log10(2).
    unsigned estimate = ((64 - std::countl_zero(value)) * 1'233) >> 12;
    return estimate + (value >= powersOfTen[estimate]);
}

enum class ShortestNotation : bool { NumberToString, Exponential };

// Lays out significand * 10^exponent, where significand has at most 17 digits and may end in zeros.
template<ShortestNotation notation>
static NEVER_INLINE size_t formatShortest(uint64_t significand, int exponent, bool negative, ShortestStringBuffer output)
{
    ASSERT(significand && significand < 100'000'000'000'000'000);

    // The digits are right-aligned in field[0, 24), and field[24, 40) is '0', so fixed-width copies
    // that run past the last digit produce zeros.
    std::array<char, 40> field;
    uint64_t highDigits = eightDecimalDigits(significand / 10'000'000'000'000'000);
    uint64_t middleDigits = eightDecimalDigits((significand / 100'000'000) % 100'000'000);
    uint64_t lowDigits = eightDecimalDigits(significand % 100'000'000);
    storeEightCharacters(std::span { field }.subspan(0), highDigits | eightASCIIZeros);
    storeEightCharacters(std::span { field }.subspan(8), middleDigits | eightASCIIZeros);
    storeEightCharacters(std::span { field }.subspan(16), lowDigits | eightASCIIZeros);
    storeEightCharacters(std::span { field }.subspan(24), eightASCIIZeros);
    storeEightCharacters(std::span { field }.subspan(32), eightASCIIZeros);

    int length = decimalDigitCount(significand);
    int trailingZeros;
    if (lowDigits)
        trailingZeros = std::countl_zero(lowDigits) / 8;
    else if (middleDigits)
        trailingZeros = 8 + std::countl_zero(middleDigits) / 8;
    else
        trailingZeros = 16 + std::countl_zero(highDigits) / 8;
    int count = length - trailingZeros;
    auto digits = std::span<const char> { field }.subspan(24 - length);
    // The value is 0.<digits> * 10^decimalPoint.
    int decimalPoint = length + exponent;

    output[0] = '-';
    auto out = std::span<char> { output }.subspan(negative);

    if (notation == ShortestNotation::NumberToString && double_conversion::validShortestRepresentation(decimalPoint - 1, double_conversion::default_decimal_in_shortest_low, double_conversion::default_decimal_in_shortest_high)) {
        if (decimalPoint <= 0) {
            memcpySpan(out, "0.000000"_span);
            memcpySpan(out.subspan(2 - decimalPoint), digits.first<17>());
            return negative + 2 - decimalPoint + count;
        }
        if (decimalPoint >= count) {
            storeEightCharacters(out.subspan(0), eightASCIIZeros);
            storeEightCharacters(out.subspan(8), eightASCIIZeros);
            storeEightCharacters(out.subspan(16), eightASCIIZeros);
            memcpySpan(out, digits.first<17>());
            return negative + decimalPoint;
        }
        memcpySpan(out, digits.first<17>());
        memcpySpan(out.subspan(decimalPoint + 1), digits.subspan(decimalPoint).first<16>());
        out[decimalPoint] = '.';
        return negative + count + 1;
    }

    out[0] = digits[0];
    out[1] = '.';
    memcpySpan(out.subspan(2), digits.subspan(1).first<16>());
    size_t position = count > 1 ? count + 1 : 1;
    int scientificExponent = decimalPoint - 1;
    out[position++] = 'e';
    out[position++] = scientificExponent < 0 ? '-' : '+';
    unsigned absoluteExponent = std::abs(scientificExponent);
    if (absoluteExponent >= 100) {
        out[position++] = static_cast<char>('0' + absoluteExponent / 100);
        absoluteExponent %= 100;
        out[position++] = static_cast<char>('0' + absoluteExponent / 10);
    } else if (absoluteExponent >= 10)
        out[position++] = static_cast<char>('0' + absoluteExponent / 10);
    out[position++] = static_cast<char>('0' + absoluteExponent % 10);
    return negative + position;
}

template<ShortestNotation notation, typename Float>
static ALWAYS_INLINE size_t formatShortest(Float value, ShortestStringBuffer buffer)
{
    auto decimal = zmij::to_decimal(value);
    if (decimal.exp == zmij::nonfinite_exp) [[unlikely]] {
        if (decimal.sig) {
            memcpySpan(buffer, "NaN"_span);
            return 3;
        }
        buffer[0] = '-';
        memcpySpan(buffer.subspan(decimal.negative), "Infinity"_span);
        return decimal.negative + 8;
    }
    if (!decimal.sig) {
        if constexpr (notation == ShortestNotation::Exponential) {
            memcpySpan(buffer, "0e+0"_span);
            return 4;
        }
        buffer[0] = '0';
        return 1;
    }
    return formatShortest<notation>(decimal.sig, decimal.exp, decimal.negative, buffer);
}

size_t numberToShortestString(double number, ShortestStringBuffer buffer)
{
    // zmij's own output is already Number::toString output, except for zero, subnormals, non-finite
    // values, and the magnitudes [2^-30, 2^-13) and [2^53, 2^70). Those contain every value whose
    // leading decimal exponent is in [-9, -5] or [16, 20], where zmij picks a different notation or
    // pads the exponent to two digits.
    unsigned biasedExponent = (std::bit_cast<uint64_t>(number) >> 52) & 0x7ff;
    if ((biasedExponent - (1023 - 30)) < 17 || (biasedExponent - (1023 + 53)) < 17 || (biasedExponent - 1) >= 0x7fe) [[unlikely]]
        return formatShortest<ShortestNotation::NumberToString>(number, buffer);
    return zmij::write(buffer.data(), buffer.size(), number) - buffer.data();
}

NumberToStringSpan numberToShortestExponentialString(double number, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    return std::span { buffer }.first(formatShortest<ShortestNotation::Exponential>(number, std::span { buffer }.first<numberToShortestStringBufferSize>()));
}

NumberToStringSpan numberToStringAndSize(float number, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    return std::span { buffer }.first(formatShortest<ShortestNotation::NumberToString>(number, std::span { buffer }.first<numberToShortestStringBufferSize>()));
}

NumberToStringSpan numberToStringAndSize(double number, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    return std::span { buffer }.first(numberToShortestString(number, std::span { buffer }.first<numberToShortestStringBufferSize>()));
}

NumberToStringSpan numberToStringWithTrailingPoint(double d, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    double_conversion::StringBuilder builder(std::span<char> { buffer });
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverterWithTrailingPoint();
    converter.ToShortest(d, &builder);
    return builder.Finalize();
}

static inline void LEGACY_NODELETE truncateTrailingZeros(std::span<const char> buffer, double_conversion::StringBuilder& builder)
{
    size_t length = builder.position();
    size_t decimalPointPosition = 0;
    for (; decimalPointPosition < length; ++decimalPointPosition) {
        if (buffer[decimalPointPosition] == '.')
            break;
    }

    // No decimal separator found, early exit.
    if (decimalPointPosition == length)
        return;

    size_t pastMantissa = decimalPointPosition + 1;
    for (; pastMantissa < length; ++pastMantissa) {
        if (buffer[pastMantissa] == 'e')
            break;
    }

    size_t truncatedLength = pastMantissa;
    for (; truncatedLength > decimalPointPosition + 1; --truncatedLength) {
        if (buffer[truncatedLength - 1] != '0')
            break;
    }

    // No trailing zeros found to strip.
    if (truncatedLength == pastMantissa)
        return;

    // If we removed all trailing zeros, remove the decimal point as well.
    if (truncatedLength == decimalPointPosition + 1)
        truncatedLength = decimalPointPosition;

    // Truncate the mantissa, and return the final result.
    builder.RemoveCharacters(truncatedLength, pastMantissa);
}

NumberToStringSpan numberToFixedPrecisionString(float number, unsigned significantFigures, NumberToStringBuffer& buffer LIFETIME_BOUND, bool shouldTruncateTrailingZeros)
{
    // For now, just call the double precision version.
    // Do that here instead of at callers to pave the way to add a more efficient code path later.
    return numberToFixedPrecisionString(static_cast<double>(number), significantFigures, buffer, shouldTruncateTrailingZeros);
}

NumberToStringSpan numberToFixedPrecisionString(double d, unsigned significantFigures, NumberToStringBuffer& buffer LIFETIME_BOUND, bool shouldTruncateTrailingZeros)
{
    // Mimic sprintf("%.[precision]g", ...).
    // "g": Signed value printed in f or e format, whichever is more compact for the given value and precision.
    // The e format is used only when the exponent of the value is less than –4 or greater than or equal to the
    // precision argument. Trailing zeros are truncated, and the decimal point appears only if one or more digits follow it.
    // "precision": The precision specifies the maximum number of significant digits printed.
    double_conversion::StringBuilder builder(std::span<char> { buffer });
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    converter.ToPrecision(d, significantFigures, &builder);
    if (shouldTruncateTrailingZeros)
        truncateTrailingZeros(std::span<const char> { buffer }, builder);
    return builder.Finalize();
}

NumberToStringSpan numberToFixedWidthString(float number, unsigned decimalPlaces, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    // For now, just call the double precision version.
    // Do that here instead of at callers to pave the way to add a more efficient code path later.
    return numberToFixedWidthString(static_cast<double>(number), decimalPlaces, buffer);
}

NumberToStringSpan numberToFixedWidthString(double d, unsigned decimalPlaces, NumberToStringBuffer& buffer LIFETIME_BOUND)
{
    // Mimic sprintf("%.[precision]f", ...).
    // "f": Signed value having the form [ – ]dddd.dddd, where dddd is one or more decimal digits.
    // The number of digits before the decimal point depends on the magnitude of the number, and
    // the number of digits after the decimal point depends on the requested precision.
    // "precision": The precision value specifies the number of digits after the decimal point.
    // If a decimal point appears, at least one digit appears before it.
    // The value is rounded to the appropriate number of digits.
    double_conversion::StringBuilder builder(std::span<char> { buffer });
    auto& converter = double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    converter.ToFixed(d, decimalPlaces, &builder);
    return builder.Finalize();
}

NumberToStringSpan numberToCSSString(double d, NumberToCSSStringBuffer& buffer LIFETIME_BOUND)
{
    // Mimic sprintf("%.[precision]f", ...).
    // "f": Signed value having the form [ – ]dddd.dddd, where dddd is one or more decimal digits.
    // The number of digits before the decimal point depends on the magnitude of the number, and
    // the number of digits after the decimal point depends on the requested precision.
    // "precision": The precision value specifies the number of digits after the decimal point.
    // If a decimal point appears, at least one digit appears before it.
    // The value is rounded to the appropriate number of digits.
    double_conversion::StringBuilder builder(std::span<char> { buffer });
    auto& converter = double_conversion::DoubleToStringConverter::CSSConverter();
    converter.ToFixedUncapped(d, 6, &builder);
    truncateTrailingZeros(std::span<const char> { buffer }, builder);
    // If we've truncated the trailing zeros and a trailing decimal, we may have a -0. Remove the negative sign in this case.
    if (builder.position() == 2 && buffer[0] == '-' && buffer[1] == '0')
        builder.RemoveCharacters(0, 1);
    return builder.Finalize();
}

} // namespace WTF

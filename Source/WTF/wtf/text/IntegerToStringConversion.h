/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Latin1Character.h>

namespace WTF {

enum PositiveOrNegativeNumber { PositiveNumber, NegativeNumber };

template<typename> struct IntegerToStringConversionTrait;

// Not std::unsigned_integral, so that the class-based UInt128 on platforms without a native 128-bit type qualifies.
template<typename T>
concept UnsignedInteger = std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed && !std::same_as<std::remove_cv_t<T>, bool>;

inline constexpr auto powersOf10ForLengthOfIntegerAsString = [] {
    std::array<uint64_t, 20> powers { };
    uint64_t power = 1;
    for (auto& entry : powers) {
        entry = power;
        power *= 10;
    }
    return powers;
}();

template<typename UnsignedIntegerType, PositiveOrNegativeNumber NumberType>
constexpr unsigned lengthOfIntegerAsStringImpl(UnsignedIntegerType number)
{
    unsigned length = 0;

    if constexpr (sizeof(UnsignedIntegerType) <= sizeof(uint64_t)) {
        uint64_t magnitude = static_cast<uint64_t>(number) | 1;
        // 1233 / 4096 approximates log10(2), so this is the digit count or one less.
        unsigned digitsBelow = ((64 - std::countl_zero(magnitude)) * 1233) >> 12;
        length = digitsBelow + (magnitude >= powersOf10ForLengthOfIntegerAsString[digitsBelow]);
    } else {
        do {
            ++length;
            number /= 10;
        } while (number);
    }

    if (NumberType == NegativeNumber)
        ++length;

    return length;
}

template<typename IntegerType>
constexpr unsigned lengthOfIntegerAsString(IntegerType integer)
{
    static_assert(std::is_integral_v<IntegerType>);
    if constexpr (std::is_same_v<IntegerType, bool>) {
        UNUSED_PARAM(integer);
        return 1;
    }
    else if constexpr (std::is_signed_v<IntegerType>) {
        using UnsignedType = std::make_unsigned_t<IntegerType>;
        bool isNegative = integer < 0;
        UnsignedType magnitude = isNegative ? static_cast<UnsignedType>(0 - unsignedCast(integer)) : unsignedCast(integer);
        return lengthOfIntegerAsStringImpl<UnsignedType, PositiveNumber>(magnitude) + isNegative;
    } else
        return lengthOfIntegerAsStringImpl<IntegerType, PositiveNumber>(integer);
}

template<size_t characterSize>
using DecimalDigitPairUnit = std::conditional_t<characterSize == 1, uint8_t, char16_t>;

template<size_t characterSize>
inline constexpr auto decimalDigitPairs = [] {
    std::array<DecimalDigitPairUnit<characterSize>, 200> pairs { };
    for (unsigned i = 0; i < 100; ++i) {
        pairs[2 * i] = '0' + i / 10;
        pairs[2 * i + 1] = '0' + i % 10;
    }
    return pairs;
}();

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// Callers bounds-check the destination once per group of digits, so the per-pair stores stay unchecked.
template<typename CharacterType>
ALWAYS_INLINE void writeDecimalDigitPair(CharacterType* destination, uint32_t pair)
{
    static_assert(sizeof(CharacterType) == 1 || sizeof(CharacterType) == 2);
    ASSERT(pair < 100);
    std::span<const DecimalDigitPairUnit<sizeof(CharacterType)>, 2> source { decimalDigitPairs<sizeof(CharacterType)>.data() + 2 * pair, 2 };
    memcpySpan(std::span<CharacterType, 2> { destination, 2 }, source);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

template<typename CharacterType>
ALWAYS_INLINE void writeFourDecimalDigits(std::span<CharacterType, 4> destination, uint32_t value)
{
    ASSERT(value < 10000);
    writeDecimalDigitPair(destination.data(), value / 100);
    writeDecimalDigitPair(destination.template last<2>().data(), value % 100);
}

template<typename CharacterType>
ALWAYS_INLINE size_t writeUInt32Below10000ToBuffer(uint32_t value, std::span<CharacterType> destination)
{
    ASSERT(value < 10000);
    if (value < 100) {
        if (value < 10) {
            destination[0] = static_cast<CharacterType>('0' + value);
            return 1;
        }
        writeDecimalDigitPair(destination.first(2).data(), value);
        return 2;
    }
    uint32_t high = value / 100;
    uint32_t low = value % 100;
    if (value < 1000) {
        auto digits = destination.first(3);
        digits[0] = static_cast<CharacterType>('0' + high);
        writeDecimalDigitPair(digits.last(2).data(), low);
        return 3;
    }
    auto digits = destination.first(4);
    writeDecimalDigitPair(digits.data(), high);
    writeDecimalDigitPair(digits.last(2).data(), low);
    return 4;
}

// Splitting off groups of four digits keeps the divisions independent instead of one serial chain of divisions by 100.
template<typename CharacterType>
NEVER_INLINE size_t writeUInt32AtLeast10000ToBuffer(uint32_t value, std::span<CharacterType> destination)
{
    ASSERT(value >= 10000);
    if (value < 100000000) {
        size_t highLength = writeUInt32Below10000ToBuffer(value / 10000, destination);
        writeFourDecimalDigits(destination.subspan(highLength).template first<4>(), value % 10000);
        return highLength + 4;
    }
    uint32_t high = value / 100000000;
    size_t highLength = 1;
    if (high < 10)
        destination[0] = static_cast<CharacterType>('0' + high);
    else {
        writeDecimalDigitPair(destination.first(2).data(), high);
        highLength = 2;
    }
    uint32_t low = value % 100000000;
    auto digits = destination.subspan(highLength).template first<8>();
    writeFourDecimalDigits(digits.template first<4>(), low / 10000);
    writeFourDecimalDigits(digits.template last<4>(), low % 10000);
    return highLength + 8;
}

template<typename CharacterType>
ALWAYS_INLINE size_t writeUInt32ToBuffer(uint32_t value, std::span<CharacterType> destination)
{
    if (value < 10000) [[likely]]
        return writeUInt32Below10000ToBuffer(value, destination);
    return writeUInt32AtLeast10000ToBuffer(value, destination);
}

template<typename CharacterType>
NEVER_INLINE size_t writeUInt64AboveUInt32MaxToBuffer(uint64_t value, std::span<CharacterType> destination)
{
    ASSERT(value > std::numeric_limits<uint32_t>::max());
    uint64_t high = value / 100000000;
    uint32_t low = value % 100000000;
    size_t highLength;
    if (high <= std::numeric_limits<uint32_t>::max())
        highLength = writeUInt32ToBuffer(static_cast<uint32_t>(high), destination);
    else {
        highLength = writeUInt32Below10000ToBuffer(static_cast<uint32_t>(high / 100000000), destination);
        uint32_t middle = high % 100000000;
        auto digits = destination.subspan(highLength).template first<8>();
        writeFourDecimalDigits(digits.template first<4>(), middle / 10000);
        writeFourDecimalDigits(digits.template last<4>(), middle % 10000);
        highLength += 8;
    }
    auto digits = destination.subspan(highLength).template first<8>();
    writeFourDecimalDigits(digits.template first<4>(), low / 10000);
    writeFourDecimalDigits(digits.template last<4>(), low % 10000);
    return highLength + 8;
}

template<typename CharacterType, UnsignedInteger UnsignedIntegerType>
size_t writeWideUnsignedIntegerToBuffer(UnsignedIntegerType number, std::span<CharacterType> destination)
{
    size_t length = lengthOfIntegerAsStringImpl<UnsignedIntegerType, PositiveNumber>(number);
    auto digits = destination.first(length);
    size_t index = length;
    while (number >= 100) {
        UnsignedIntegerType quotient = number / 100;
        index -= 2;
        writeDecimalDigitPair(digits.subspan(index).data(), static_cast<uint32_t>(number - quotient * 100));
        number = quotient;
    }
    if (number >= 10)
        writeDecimalDigitPair(digits.data(), static_cast<uint32_t>(number));
    else
        digits[0] = static_cast<CharacterType>('0' + static_cast<uint32_t>(number));
    return length;
}

template<typename CharacterType, UnsignedInteger UnsignedIntegerType>
ALWAYS_INLINE size_t writeUnsignedIntegerToBuffer(UnsignedIntegerType number, std::span<CharacterType> destination)
{
    if constexpr (sizeof(UnsignedIntegerType) <= sizeof(uint32_t))
        return writeUInt32ToBuffer(static_cast<uint32_t>(number), destination);
    else if constexpr (sizeof(UnsignedIntegerType) <= sizeof(uint64_t)) {
        if (number <= std::numeric_limits<uint32_t>::max())
            return writeUInt32ToBuffer(static_cast<uint32_t>(number), destination);
        return writeUInt64AboveUInt32MaxToBuffer(static_cast<uint64_t>(number), destination);
    } else
        return writeWideUnsignedIntegerToBuffer(number, destination);
}

// Returns the number of characters written. The destination must have room for lengthOfIntegerAsString(integer) characters.
template<typename CharacterType, std::integral IntegerType>
ALWAYS_INLINE size_t writeIntegerToBuffer(IntegerType integer, std::span<CharacterType> destination)
{
    if constexpr (std::is_same_v<IntegerType, bool>) {
        destination[0] = static_cast<CharacterType>(integer ? '1' : '0');
        return 1;
    } else if constexpr (std::is_signed_v<IntegerType>) {
        if (integer < 0) {
            destination[0] = '-';
            return 1 + writeUnsignedIntegerToBuffer(unsignedCast(WTF::negate(integer)), destination.subspan(1));
        }
        return writeUnsignedIntegerToBuffer(unsignedCast(integer), destination);
    } else
        return writeUnsignedIntegerToBuffer(integer, destination);
}

template<typename T, UnsignedInteger UnsignedIntegerType, PositiveOrNegativeNumber NumberType, typename AdditionalArgumentType>
static inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringImpl(UnsignedIntegerType number, AdditionalArgumentType additionalArgument)
{
    std::array<Latin1Character, sizeof(UnsignedIntegerType) * 3 + 1> buffer;
    std::span<Latin1Character> destination { buffer };
    size_t length = 0;
    if constexpr (NumberType == NegativeNumber)
        destination[length++] = '-';
    length += writeUnsignedIntegerToBuffer(number, destination.subspan(length));
    return IntegerToStringConversionTrait<T>::flush(destination.first(length), additionalArgument);
}

template<typename T, std::signed_integral SignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringSigned(SignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    if (number < 0)
        return numberToStringImpl<T, typename std::make_unsigned_t<SignedIntegerType>, NegativeNumber>(-unsignedCast(number), additionalArgument);
    return numberToStringImpl<T, typename std::make_unsigned_t<SignedIntegerType>, PositiveNumber>(number, additionalArgument);
}

template<typename T, UnsignedInteger UnsignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringUnsigned(UnsignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    return numberToStringImpl<T, UnsignedIntegerType, PositiveNumber>(number, additionalArgument);
}

template<size_t N>
struct IntegerToStringConversionTrait<Vector<Latin1Character, N>> {
    using ReturnType = Vector<Latin1Character, N>;
    using AdditionalArgumentType = void;
    static ReturnType flush(std::span<const Latin1Character> characters, void*) { return characters; }
};

} // namespace WTF

using WTF::numberToStringSigned;
using WTF::numberToStringUnsigned;
using WTF::lengthOfIntegerAsString;
using WTF::writeIntegerToBuffer;

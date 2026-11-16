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
#include <cstdint>
#include <span>
#include <string>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Latin1Character.h>

namespace WTF {

enum PositiveOrNegativeNumber { PositiveNumber, NegativeNumber };

template<typename> struct IntegerToStringConversionTrait;

template<typename T, typename UnsignedIntegerType, PositiveOrNegativeNumber NumberType, typename AdditionalArgumentType>
static inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringImpl(UnsignedIntegerType number, AdditionalArgumentType additionalArgument)
{
    std::array<Latin1Character, sizeof(UnsignedIntegerType) * 3 + 1> buffer;
    auto index = buffer.size();
    do {
        buffer[--index] = static_cast<Latin1Character>(static_cast<unsigned>(number % 10) + '0');
        number /= 10;
    } while (number);

    if (NumberType == NegativeNumber)
        buffer[--index] = '-';

    return IntegerToStringConversionTrait<T>::flush(std::span { buffer }.subspan(index), additionalArgument);
}

template<typename T, typename SignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringSigned(SignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    if (number < 0)
        return numberToStringImpl<T, typename std::make_unsigned_t<SignedIntegerType>, NegativeNumber>(-unsignedCast(number), additionalArgument);
    return numberToStringImpl<T, typename std::make_unsigned_t<SignedIntegerType>, PositiveNumber>(number, additionalArgument);
}

template<typename T, typename UnsignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringUnsigned(UnsignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    return numberToStringImpl<T, UnsignedIntegerType, PositiveNumber>(number, additionalArgument);
}

template<typename CharacterType, typename UnsignedIntegerType, PositiveOrNegativeNumber NumberType>
static inline void writeIntegerToBufferImpl(UnsignedIntegerType number, std::span<CharacterType> destination)
{
    static_assert(!std::is_same_v<bool, std::remove_cv_t<UnsignedIntegerType>>, "'bool' not supported");
    std::array<Latin1Character, sizeof(UnsignedIntegerType) * 3 + 1> buffer;
    auto index = buffer.size();
    do {
        buffer[--index] = static_cast<Latin1Character>(static_cast<unsigned>(number % 10) + '0');
        number /= 10;
    } while (number);

    if (NumberType == NegativeNumber)
        buffer[--index] = '-';
    
    for (size_t i = 0; i < buffer.size() - index; ++i)
        destination[i] = static_cast<CharacterType>(buffer[index + i]);
}

template<typename CharacterType, typename IntegerType>
inline void writeIntegerToBuffer(IntegerType integer, std::span<CharacterType> destination)
{
    static_assert(std::is_integral_v<IntegerType>);
    if constexpr (std::is_same_v<IntegerType, bool>)
        return writeIntegerToBufferImpl<CharacterType, uint8_t, PositiveNumber>(integer ? 1 : 0, destination);
    else if constexpr (std::is_signed_v<IntegerType>) {
        if (integer < 0)
            return writeIntegerToBufferImpl<CharacterType, typename std::make_unsigned_t<IntegerType>, NegativeNumber>(WTF::negate(integer), destination);
        return writeIntegerToBufferImpl<CharacterType, typename std::make_unsigned_t<IntegerType>, PositiveNumber>(unsignedCast(integer), destination);
    } else
        return writeIntegerToBufferImpl<CharacterType, IntegerType, PositiveNumber>(integer, destination);
}

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

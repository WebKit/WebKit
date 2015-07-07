/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef IntegerToStringConversion_h
#define IntegerToStringConversion_h

#include "StringBuilder.h"

namespace WTF {

enum PositiveOrNegativeNumber {
    PositiveNumber,
    NegativeNumber
};

template<typename T> struct IntegerToStringConversionTrait;

template<> struct IntegerToStringConversionTrait<AtomicString> {
    typedef AtomicString ReturnType;
    typedef void AdditionalArgumentType;
    static ReturnType flush(LChar* characters, unsigned length, void*) { return AtomicString(characters, length); }
};
template<> struct IntegerToStringConversionTrait<String> {
    typedef String ReturnType;
    typedef void AdditionalArgumentType;
    static ReturnType flush(LChar* characters, unsigned length, void*) { return String(characters, length); }
};
template<> struct IntegerToStringConversionTrait<StringBuilder> {
    typedef void ReturnType;
    typedef StringBuilder AdditionalArgumentType;
    static ReturnType flush(LChar* characters, unsigned length, StringBuilder* stringBuilder) { stringBuilder->append(characters, length); }
};

template<typename T, typename UnsignedIntegerType, PositiveOrNegativeNumber NumberType, typename AdditionalArgumentType>
static typename IntegerToStringConversionTrait<T>::ReturnType numberToStringImpl(UnsignedIntegerType number, AdditionalArgumentType additionalArgument)
{
    LChar buf[sizeof(UnsignedIntegerType) * 3 + 1];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    do {
        *--p = static_cast<LChar>((number % 10) + '0');
        number /= 10;
    } while (number);

    if (NumberType == NegativeNumber)
        *--p = '-';

    return IntegerToStringConversionTrait<T>::flush(p, static_cast<unsigned>(end - p), additionalArgument);
}

template<typename T, typename SignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringSigned(SignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    if (number < 0)
        return numberToStringImpl<T, typename std::make_unsigned<SignedIntegerType>::type, NegativeNumber>(-number, additionalArgument);
    return numberToStringImpl<T, typename std::make_unsigned<SignedIntegerType>::type, PositiveNumber>(number, additionalArgument);
}

template<typename T, typename UnsignedIntegerType>
inline typename IntegerToStringConversionTrait<T>::ReturnType numberToStringUnsigned(UnsignedIntegerType number, typename IntegerToStringConversionTrait<T>::AdditionalArgumentType* additionalArgument = nullptr)
{
    return numberToStringImpl<T, UnsignedIntegerType, PositiveNumber>(number, additionalArgument);
}

} // namespace WTF

#endif // IntegerToStringConversion_h

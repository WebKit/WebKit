/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 */

#pragma once

#include <array>
#include <wtf/text/StringConcatenate.h>

namespace WTF {

enum HexConversionMode { Lowercase, Uppercase };

namespace Internal {

inline const LChar* hexDigitsForMode(HexConversionMode mode)
{
    static const LChar lowercaseHexDigits[17] = "0123456789abcdef";
    static const LChar uppercaseHexDigits[17] = "0123456789ABCDEF";
    return mode == Lowercase ? lowercaseHexDigits : uppercaseHexDigits;
}

} // namespace Internal

template<typename T>
inline void appendByteAsHex(unsigned char byte, T& destination, HexConversionMode mode = Uppercase)
{
    auto* hexDigits = Internal::hexDigitsForMode(mode);
    destination.append(hexDigits[byte >> 4]);
    destination.append(hexDigits[byte & 0xF]);
}

template<typename NumberType, typename DestinationType>
inline void appendUnsignedAsHex(NumberType number, DestinationType& destination, HexConversionMode mode = Uppercase)
{
    // Each byte can generate up to two digits.
    std::array<LChar, sizeof(NumberType) * 2> buffer;
    auto start = buffer.end();
    auto unsignedNumber = static_cast<typename std::make_unsigned<NumberType>::type>(number);
    auto hexDigits = Internal::hexDigitsForMode(mode);
    do {
        ASSERT(start > buffer.begin());
        *--start = hexDigits[unsignedNumber & 0xF];
        unsignedNumber >>= 4;
    } while (unsignedNumber);
    destination.append(&*start, buffer.end() - start);
}

// Same as appendUnsignedAsHex, but zero-padding to get at least the desired number of digits.
template<typename NumberType, typename DestinationType>
inline void appendUnsignedAsHexFixedSize(NumberType number, DestinationType& destination, unsigned desiredDigits, HexConversionMode mode = Uppercase)
{
    unsigned digits = 0;
    auto unsignedNumber = static_cast<typename std::make_unsigned<NumberType>::type>(number);
    do {
        ++digits;
        unsignedNumber >>= 4;
    } while (unsignedNumber);
    for (; digits < desiredDigits; ++digits)
        destination.append('0');
    appendUnsignedAsHex(number, destination, mode);
}

} // namespace WTF

using WTF::appendByteAsHex;
using WTF::appendUnsignedAsHex;
using WTF::appendUnsignedAsHexFixedSize;
using WTF::Lowercase;

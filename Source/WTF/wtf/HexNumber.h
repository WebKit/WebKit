/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

WTF_EXPORT std::pair<LChar*, unsigned> appendHex(LChar* buffer, unsigned bufferSize, std::uintmax_t number, unsigned minimumDigits, HexConversionMode);

template<size_t arraySize, typename NumberType>
inline std::pair<LChar*, unsigned> appendHex(std::array<LChar, arraySize>& buffer, NumberType number, unsigned minimumDigits, HexConversionMode mode)
{
    return appendHex(&buffer.front(), buffer.size(), static_cast<typename std::make_unsigned<NumberType>::type>(number), minimumDigits, mode);
}

} // namespace Internal

template<typename T>
inline void appendByteAsHex(unsigned char byte, T& destination, HexConversionMode mode = Uppercase)
{
    auto* hexDigits = Internal::hexDigitsForMode(mode);
    destination.append(hexDigits[byte >> 4]);
    destination.append(hexDigits[byte & 0xF]);
}

// FIXME: Consider renaming to appendHex.
template<typename NumberType, typename DestinationType>
inline void appendUnsignedAsHex(NumberType number, DestinationType& destination, HexConversionMode mode = Uppercase)
{
    appendUnsignedAsHexFixedSize(number, destination, 0, mode);
}

// FIXME: Consider renaming to appendHex.
// Same as appendUnsignedAsHex, but zero-padding to get at least the desired number of digits.
template<typename NumberType, typename DestinationType>
inline void appendUnsignedAsHexFixedSize(NumberType number, DestinationType& destination, unsigned minimumDigits, HexConversionMode mode = Uppercase)
{
    // Each byte can generate up to two digits.
    std::array<LChar, sizeof(NumberType) * 2> buffer;
    auto result = Internal::appendHex(buffer, number, minimumDigits, mode);
    destination.append(result.first, result.second);
}

struct HexNumberBuffer {
    std::array<LChar, 16> characters;
    unsigned length;
};

template<typename NumberType> HexNumberBuffer hex(NumberType number, unsigned minimumDigits = 0, HexConversionMode mode = Uppercase)
{
    // Each byte can generate up to two digits.
    HexNumberBuffer buffer;
    static_assert(sizeof(buffer.characters) >= sizeof(NumberType) * 2, "number too large for hexNumber");
    auto result = Internal::appendHex(buffer.characters, number, minimumDigits, mode);
    buffer.length = result.second;
    return buffer;
}

template<typename NumberType> HexNumberBuffer hex(NumberType number, HexConversionMode mode)
{
    return hex(number, 0, mode);
}

template<> class StringTypeAdapter<HexNumberBuffer> {
public:
    StringTypeAdapter(const HexNumberBuffer& buffer)
        : m_buffer { buffer }
    {
    }

    unsigned length() const { return m_buffer.length; }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { StringImpl::copyCharacters(destination, characters(), length()); }

private:
    const LChar* characters() const { return &*(m_buffer.characters.end() - length()); }

    const HexNumberBuffer& m_buffer;
};

} // namespace WTF

using WTF::appendByteAsHex;
using WTF::appendUnsignedAsHex;
using WTF::appendUnsignedAsHexFixedSize;
using WTF::hex;
using WTF::Lowercase;

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
#include <wtf/text/StringImpl.h>

namespace WTF {

enum HexConversionMode { Lowercase, Uppercase };

namespace Internal {

inline const std::array<LChar, 16>& hexDigitsForMode(HexConversionMode mode)
{
    static constexpr std::array<LChar, 16> lowercaseHexDigits { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    static constexpr std::array<LChar, 16> uppercaseHexDigits { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    return mode == Lowercase ? lowercaseHexDigits : uppercaseHexDigits;
}

WTF_EXPORT_PRIVATE std::span<LChar> appendHex(std::span<LChar> buffer, std::uintmax_t number, unsigned minimumDigits, HexConversionMode);

template<size_t arraySize, typename NumberType>
inline std::span<LChar> appendHex(std::array<LChar, arraySize>& buffer, NumberType number, unsigned minimumDigits, HexConversionMode mode)
{
    return appendHex(std::span<LChar> { buffer }, static_cast<typename std::make_unsigned<NumberType>::type>(number), minimumDigits, mode);
}

} // namespace Internal

struct HexNumberBuffer {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    std::array<LChar, 16> buffer;
    unsigned length;

    std::span<const LChar> span() const LIFETIME_BOUND { return std::span { buffer }.last(length); }
};

template<typename NumberType> HexNumberBuffer hex(NumberType number, unsigned minimumDigits = 0, HexConversionMode mode = Uppercase)
{
    // Each byte can generate up to two digits.
    HexNumberBuffer buffer;
    static_assert(sizeof(buffer.buffer) >= sizeof(NumberType) * 2, "number too large for hexNumber");
    auto result = Internal::appendHex(buffer.buffer, number, minimumDigits, mode);
    buffer.length = result.size();
    return buffer;
}

template<typename NumberType> HexNumberBuffer hex(NumberType number, HexConversionMode mode)
{
    return hex(number, 0, mode);
}

template<> class StringTypeAdapter<HexNumberBuffer> {
public:
    explicit StringTypeAdapter(const HexNumberBuffer& buffer)
        : m_buffer { buffer }
    {
    }

    unsigned length() const { return m_buffer.length; }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const { StringImpl::copyCharacters(destination.data(), m_buffer.span()); }

private:
    const HexNumberBuffer& m_buffer;
};

WTF_EXPORT_PRIVATE CString toHexCString(std::span<const uint8_t>);
WTF_EXPORT_PRIVATE String toHexString(std::span<const uint8_t>);

class PrintStream;
WTF_EXPORT_PRIVATE void printInternal(PrintStream&, HexNumberBuffer);

} // namespace WTF

using WTF::hex;
using WTF::toHexCString;
using WTF::toHexString;
using WTF::Lowercase;

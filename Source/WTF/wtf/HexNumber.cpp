/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HexNumber.h"

#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/IndexedRange.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WTF {

namespace Internal {

std::span<LChar> appendHex(std::span<LChar> buffer, std::uintmax_t number, unsigned minimumDigits, HexConversionMode mode)
{
    size_t startIndex = buffer.size();
    auto& hexDigits = hexDigitsForMode(mode);
    do {
        buffer[--startIndex] = hexDigits[number & 0xF];
        number >>= 4;
    } while (number);
    auto startIndexWithLeadingZeros = buffer.size() - std::min<size_t>(minimumDigits, buffer.size());
    if (startIndex > startIndexWithLeadingZeros) {
        memsetSpan(buffer.subspan(startIndexWithLeadingZeros, startIndex - startIndexWithLeadingZeros), '0');
        startIndex = startIndexWithLeadingZeros;
    }
    return buffer.subspan(startIndex);
}

}

void printInternal(PrintStream& out, HexNumberBuffer buffer)
{
    out.print(StringView(buffer.span()));
}

static void toHexInternal(std::span<const uint8_t> values, std::span<LChar> hexadecimalOutput)
{
    for (auto [i, digestValue] : indexedRange(values)) {
        hexadecimalOutput[i * 2] = upperNibbleToASCIIHexDigit(digestValue);
        hexadecimalOutput[i * 2 + 1] = lowerNibbleToASCIIHexDigit(digestValue);
    }
}

CString toHexCString(std::span<const uint8_t> values)
{
    std::span<char> buffer;
    auto result = CString::newUninitialized(CheckedSize(values.size()) * 2U, buffer);
    toHexInternal(values, byteCast<LChar>(buffer));
    return result;
}

String toHexString(std::span<const uint8_t> values)
{
    std::span<LChar> buffer;
    auto result = String::createUninitialized(CheckedSize(values.size()) * 2U, buffer);
    toHexInternal(values, buffer);
    return result;
}

} // namespace WTF

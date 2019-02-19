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

namespace WTF {

namespace Internal {

std::pair<LChar*, unsigned> appendHex(LChar* buffer, unsigned bufferSize, std::uintmax_t number, unsigned minimumDigits, HexConversionMode mode)
{
    auto end = buffer + bufferSize;
    auto start = end;
    auto hexDigits = hexDigitsForMode(mode);
    do {
        *--start = hexDigits[number & 0xF];
        number >>= 4;
    } while (number);
    auto startWithLeadingZeros = end - std::min(minimumDigits, bufferSize);
    if (start > startWithLeadingZeros) {
        std::memset(startWithLeadingZeros, '0', start - startWithLeadingZeros);
        start = startWithLeadingZeros;
    }
    return { start, end - start };
}

}

} // namespace WTF

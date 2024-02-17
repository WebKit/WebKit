/*
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <unicode/utypes.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/LChar.h>

namespace WTF {

class StringHasher;

namespace HasherHelpers {

static constexpr unsigned flagCount = 8; // Save 8 bits for StringImpl to use as flags.
static constexpr unsigned maskHash = (1U << (sizeof(unsigned) * 8 - flagCount)) - 1;

// This avoids ever returning a hash code of 0, since that is used to
// signal "hash not computed yet". Setting the high bit maintains
// reasonable fidelity to a hash code of 0 because it is likely to yield
// exactly 0 when hash lookup masks out the high bits.
ALWAYS_INLINE static constexpr unsigned avoidZero(unsigned hash)
{
    if (hash)
        return hash;
    return 0x80000000 >> flagCount;
}

ALWAYS_INLINE static constexpr unsigned finalize(unsigned hash)
{
    return avoidZero(hash);
}

ALWAYS_INLINE static constexpr unsigned finalizeAndMaskTop8Bits(unsigned hash)
{
    // Reserving space from the high bits for flags preserves most of the hash's
    // value, since hash lookup typically masks out the high bits anyway.
    return avoidZero(hash & maskHash);
}

struct DefaultConverter {
    template<typename CharType>
    static constexpr UChar convert(CharType character)
    {
        return static_cast<std::make_unsigned_t<CharType>>((character));
    }
};

} // namespace HasherHelpers
} // namespace WTF

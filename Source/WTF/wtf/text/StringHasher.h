/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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

#include <array>
#include <unicode/utypes.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/LChar.h>

namespace WTF {

// Golden ratio. Arbitrary start value to avoid mapping all zeros to a hash value of zero.
static constexpr unsigned stringHashingStartValue = 0x9E3779B9U;

class SuperFastHash;
class WYHash;

class StringHasher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr unsigned flagCount = 8; // Save 8 bits for StringImpl to use as flags.
    static constexpr unsigned maskHash = (1U << (sizeof(unsigned) * 8 - flagCount)) - 1;
    static constexpr unsigned numberOfCharactersInLargestBulkForWYHash = 24; // Don't change this value. It's fixed for WYhash algorithm.

    // Things need to do to update this threshold:
    // 1. This threshold must stay in sync with the threshold in the scripts create_hash_table, Hasher.pm, and hasher.py.
    // 2. Run script `run-bindings-tests --reset-results` to update all CompactHashIndex's under path `WebCore/bindings/scripts/test/JS/`.
    // 3. Manually update all CompactHashIndex's in JSDollarVM.cpp by using createHashTable in hasher.py.
    static constexpr unsigned smallStringThreshold = numberOfCharactersInLargestBulkForWYHash * 2;

    struct DefaultConverter {
        template<typename CharType>
        static constexpr UChar convert(CharType character)
        {
            return static_cast<std::make_unsigned_t<CharType>>((character));
        }
    };

    StringHasher() = default;

    template<typename T, typename Converter = DefaultConverter>
    static unsigned computeHashAndMaskTop8Bits(const T* data, unsigned characterCount);

    template<typename T, unsigned characterCount>
    static constexpr unsigned computeLiteralHashAndMaskTop8Bits(const T (&characters)[characterCount]);

    void addCharacter(UChar character);

    // hashWithTop8BitsMasked will reset to initial status.
    unsigned hashWithTop8BitsMasked();

private:
    friend class SuperFastHash;
    friend class WYHash;

    ALWAYS_INLINE static constexpr unsigned avalancheBits(unsigned hash)
    {
        unsigned result = hash;

        result ^= result << 3;
        result += result >> 5;
        result ^= result << 2;
        result += result >> 15;
        result ^= result << 10;

        return result;
    }

    ALWAYS_INLINE static constexpr unsigned finalize(unsigned hash)
    {
        return avoidZero(avalancheBits(hash));
    }

    ALWAYS_INLINE static constexpr unsigned finalizeAndMaskTop8Bits(unsigned hash)
    {
        // Reserving space from the high bits for flags preserves most of the hash's
        // value, since hash lookup typically masks out the high bits anyway.
        return avoidZero(avalancheBits(hash) & StringHasher::maskHash);
    }

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

#if PLATFORM(MAC)
    bool m_pendingHashValue { false };
    unsigned m_numberOfProcessedCharacters { 0 };
    uint64_t m_seed { 0 };
    uint64_t m_see1 { 0 };
    uint64_t m_see2 { 0 };

    unsigned m_bufferSize { 0 };
    std::array<UChar, smallStringThreshold> m_buffer;
#else
    unsigned m_hash { stringHashingStartValue };
    UChar m_pendingCharacter { 0 };
    bool m_hasPendingCharacter { false };
#endif
};

} // namespace WTF

using WTF::StringHasher;

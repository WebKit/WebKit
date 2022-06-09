/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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
#include <wtf/text/LChar.h>

namespace WTF {

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html

// LChar data is interpreted as Latin-1-encoded (zero-extended to 16 bits).

// NOTE: The hash computation here must stay in sync with the create_hash_table script in
// JavaScriptCore and the CodeGeneratorJS.pm script in WebCore.

// Golden ratio. Arbitrary start value to avoid mapping all zeros to a hash value of zero.
static constexpr const unsigned stringHashingStartValue = 0x9E3779B9U;

class StringHasher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr const unsigned flagCount = 8; // Save 8 bits for StringImpl to use as flags.
    static constexpr const unsigned maskHash = (1U << (sizeof(unsigned) * 8 - flagCount)) - 1;

    struct DefaultConverter {
        template<typename CharType>
        static constexpr UChar convert(CharType character)
        {
            return static_cast<std::make_unsigned_t<CharType>>((character));
        }
    };

    StringHasher() = default;

    // The hasher hashes two characters at a time, and thus an "aligned" hasher is one
    // where an even number of characters have been added. Callers that always add
    // characters two at a time can use the "assuming aligned" functions.
    void addCharactersAssumingAligned(UChar a, UChar b)
    {
        ASSERT(!m_hasPendingCharacter);
        m_hash = calculateWithTwoCharacters(m_hash, a, b);
    }

    void addCharacter(UChar character)
    {
        if (m_hasPendingCharacter) {
            m_hasPendingCharacter = false;
            addCharactersAssumingAligned(m_pendingCharacter, character);
            return;
        }

        m_pendingCharacter = character;
        m_hasPendingCharacter = true;
    }

    void addCharacters(UChar a, UChar b)
    {
        if (m_hasPendingCharacter) {
#if ASSERT_ENABLED
            m_hasPendingCharacter = false;
#endif
            addCharactersAssumingAligned(m_pendingCharacter, a);
            m_pendingCharacter = b;
#if ASSERT_ENABLED
            m_hasPendingCharacter = true;
#endif
            return;
        }

        addCharactersAssumingAligned(a, b);
    }

    template<typename T, typename Converter> void addCharactersAssumingAligned(const T* data, unsigned length)
    {
        ASSERT(!m_hasPendingCharacter);

        bool remainder = length & 1;
        length >>= 1;

        while (length--) {
            addCharactersAssumingAligned(Converter::convert(data[0]), Converter::convert(data[1]));
            data += 2;
        }

        if (remainder)
            addCharacter(Converter::convert(*data));
    }

    template<typename T> void addCharactersAssumingAligned(const T* data, unsigned length)
    {
        addCharactersAssumingAligned<T, DefaultConverter>(data, length);
    }

    template<typename T, typename Converter> void addCharactersAssumingAligned(const T* data)
    {
        ASSERT(!m_hasPendingCharacter);

        while (T a = *data++) {
            T b = *data++;
            if (!b) {
                addCharacter(Converter::convert(a));
                break;
            }
            addCharactersAssumingAligned(Converter::convert(a), Converter::convert(b));
        }
    }

    template<typename T> void addCharactersAssumingAligned(const T* data)
    {
        addCharactersAssumingAligned<T, DefaultConverter>(data);
    }

    template<typename T, typename Converter> void addCharacters(const T* data, unsigned length)
    {
        if (!length)
            return;
        if (m_hasPendingCharacter) {
            m_hasPendingCharacter = false;
            addCharactersAssumingAligned(m_pendingCharacter, Converter::convert(*data++));
            --length;
        }
        addCharactersAssumingAligned<T, Converter>(data, length);
    }

    template<typename T> void addCharacters(const T* data, unsigned length)
    {
        addCharacters<T, DefaultConverter>(data, length);
    }

    template<typename T, typename Converter> void addCharacters(const T* data)
    {
        if (m_hasPendingCharacter && *data) {
            m_hasPendingCharacter = false;
            addCharactersAssumingAligned(m_pendingCharacter, Converter::convert(*data++));
        }
        addCharactersAssumingAligned<T, Converter>(data);
    }

    template<typename T> void addCharacters(const T* data)
    {
        addCharacters<T, DefaultConverter>(data);
    }

    unsigned hashWithTop8BitsMasked() const
    {
        return finalizeAndMaskTop8Bits(processPendingCharacter());
    }

    unsigned hash() const
    {
        return finalize(processPendingCharacter());
    }

    template<typename T, typename Converter> static constexpr unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        return finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter> static constexpr unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        return finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data));
    }

    template<typename T> static constexpr unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        return computeHashAndMaskTop8Bits<T, DefaultConverter>(data, length);
    }

    template<typename T> static constexpr unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        return computeHashAndMaskTop8Bits<T, DefaultConverter>(data);
    }

    template<typename T, typename Converter> static constexpr unsigned computeHash(const T* data, unsigned length)
    {
        return finalize(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter> static constexpr unsigned computeHash(const T* data)
    {
        return finalize(computeHashImpl<T, Converter>(data));
    }

    template<typename T> static constexpr unsigned computeHash(const T* data, unsigned length)
    {
        return computeHash<T, DefaultConverter>(data, length);
    }

    template<typename T> static constexpr unsigned computeHash(const T* data)
    {
        return computeHash<T, DefaultConverter>(data);
    }


    template<typename T, unsigned charactersCount>
    static constexpr unsigned computeLiteralHash(const T (&characters)[charactersCount])
    {
        return computeHash<T, DefaultConverter>(characters, charactersCount - 1);
    }

    template<typename T, unsigned charactersCount>
    static constexpr unsigned computeLiteralHashAndMaskTop8Bits(const T (&characters)[charactersCount])
    {
        return computeHashAndMaskTop8Bits<T, DefaultConverter>(characters, charactersCount - 1);
    }

    static unsigned hashMemory(const void* data, unsigned length)
    {
        size_t lengthInUChar = length / sizeof(UChar);
        StringHasher hasher;
        hasher.addCharactersAssumingAligned(static_cast<const UChar*>(data), lengthInUChar);

        for (size_t i = 0; i < length % sizeof(UChar); ++i)
            hasher.addCharacter(static_cast<const char*>(data)[lengthInUChar * sizeof(UChar) + i]);

        return hasher.hash();
    }

    template<size_t length> static unsigned hashMemory(const void* data)
    {
        return hashMemory(data, length);
    }

private:
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

    static constexpr unsigned finalize(unsigned hash)
    {
        return avoidZero(avalancheBits(hash));
    }

    static constexpr unsigned finalizeAndMaskTop8Bits(unsigned hash)
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

    static constexpr unsigned calculateWithRemainingLastCharacter(unsigned hash, unsigned character)
    {
        unsigned result = hash;

        result += character;
        result ^= result << 11;
        result += result >> 17;

        return result;
    }

    ALWAYS_INLINE static constexpr unsigned calculateWithTwoCharacters(unsigned hash, unsigned firstCharacter, unsigned secondCharacter)
    {
        unsigned result = hash;

        result += firstCharacter;
        result = (result << 16) ^ ((secondCharacter << 11) ^ result);
        result += result >> 11;

        return result;
    }

    template<typename T, typename Converter>
    static constexpr unsigned computeHashImpl(const T* characters, unsigned length)
    {
        unsigned result = stringHashingStartValue;
        bool remainder = length & 1;
        length >>= 1;

        while (length--) {
            result = calculateWithTwoCharacters(result, Converter::convert(characters[0]), Converter::convert(characters[1]));
            characters += 2;
        }

        if (remainder)
            return calculateWithRemainingLastCharacter(result, Converter::convert(characters[0]));
        return result;
    }

    template<typename T, typename Converter>
    static constexpr unsigned computeHashImpl(const T* characters)
    {
        unsigned result = stringHashingStartValue;
        while (T a = *characters++) {
            T b = *characters++;
            if (!b)
                return calculateWithRemainingLastCharacter(result, Converter::convert(a));
            result = calculateWithTwoCharacters(result, Converter::convert(a), Converter::convert(b));
        }
        return result;
    }

    unsigned processPendingCharacter() const
    {
        unsigned result = m_hash;

        // Handle end case.
        if (m_hasPendingCharacter)
            return calculateWithRemainingLastCharacter(result, m_pendingCharacter);
        return result;
    }

    unsigned m_hash { stringHashingStartValue };
    UChar m_pendingCharacter { 0 };
    bool m_hasPendingCharacter { false };
};

} // namespace WTF

using WTF::StringHasher;

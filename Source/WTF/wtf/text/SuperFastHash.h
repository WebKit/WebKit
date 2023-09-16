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

#include <wtf/text/StringHasher.h>

namespace WTF {

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html

// LChar data is interpreted as Latin-1-encoded (zero-extended to 16 bits).

// NOTE: The hash computation here must stay in sync with the create_hash_table script in
// JavaScriptCore and the CodeGeneratorJS.pm script in WebCore.

class SuperFastHash {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr unsigned flagCount = StringHasher::flagCount;
    static constexpr unsigned maskHash = StringHasher::maskHash;
    typedef StringHasher::DefaultConverter DefaultConverter;

    SuperFastHash() = default;

    // The hasher hashes two characters at a time, and thus an "aligned" hasher is one
    // where an even number of characters have been added. Callers that always add
    // characters two at a time can use the "assuming aligned" functions.
    void addCharactersAssumingAligned(UChar a, UChar b)
    {
        ASSERT(!m_hasPendingCharacter);
        addCharactersAssumingAlignedImpl(a, b, m_hash);
    }

    void addCharacter(UChar character)
    {
        addCharacterImpl(character, m_hasPendingCharacter, m_pendingCharacter, m_hash);
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

    template<typename T, typename Converter = DefaultConverter>
    void addCharactersAssumingAligned(const T* data, unsigned length)
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

    template<typename T, typename Converter = DefaultConverter>
    void addCharactersAssumingAligned(const T* data)
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

    template<typename T, typename Converter = DefaultConverter>
    void addCharacters(const T* data, unsigned length)
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

    template<typename T, typename Converter = DefaultConverter>
    void addCharacters(const T* data)
    {
        if (m_hasPendingCharacter && *data) {
            m_hasPendingCharacter = false;
            addCharactersAssumingAligned(m_pendingCharacter, Converter::convert(*data++));
        }
        addCharactersAssumingAligned<T, Converter>(data);
    }

    unsigned hashWithTop8BitsMasked() const
    {
        return hashWithTop8BitsMaskedImpl(m_hasPendingCharacter, m_pendingCharacter, m_hash);
    }

    unsigned hash() const
    {
        return StringHasher::finalize(processPendingCharacter());
    }

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static constexpr unsigned computeHashAndMaskTop8Bits(const T* data, unsigned length)
    {
        return StringHasher::finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static constexpr unsigned computeHashAndMaskTop8Bits(const T* data)
    {
        return StringHasher::finalizeAndMaskTop8Bits(computeHashImpl<T, Converter>(data));
    }

    template<typename T, typename Converter = DefaultConverter>
    static constexpr unsigned computeHash(const T* data, unsigned length)
    {
        return StringHasher::finalize(computeHashImpl<T, Converter>(data, length));
    }

    template<typename T, typename Converter = DefaultConverter>
    static constexpr unsigned computeHash(const T* data)
    {
        return StringHasher::finalize(computeHashImpl<T, Converter>(data));
    }

    template<typename T, unsigned charactersCount>
    static constexpr unsigned computeLiteralHash(const T (&characters)[charactersCount])
    {
        return computeHash<T>(characters, charactersCount - 1);
    }

    template<typename T, unsigned charactersCount>
    static constexpr unsigned computeLiteralHashAndMaskTop8Bits(const T (&characters)[charactersCount])
    {
        return computeHashAndMaskTop8Bits<T>(characters, charactersCount - 1);
    }

private:
    friend class StringHasher;

    static void addCharactersAssumingAlignedImpl(UChar a, UChar b, unsigned& hash)
    {
        hash = calculateWithTwoCharacters(hash, a, b);
    }

    static void addCharacterImpl(UChar character, bool& hasPendingCharacter, UChar& pendingCharacter, unsigned& hash)
    {
        if (hasPendingCharacter) {
            hasPendingCharacter = false;
            addCharactersAssumingAlignedImpl(pendingCharacter, character, hash);
            return;
        }

        pendingCharacter = character;
        hasPendingCharacter = true;
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
        return processPendingCharacterImpl(m_hasPendingCharacter, m_pendingCharacter, m_hash);
    }

    static unsigned hashWithTop8BitsMaskedImpl(const bool hasPendingCharacter, const UChar pendingCharacter, const unsigned hash)
    {
        return StringHasher::finalizeAndMaskTop8Bits(processPendingCharacterImpl(hasPendingCharacter, pendingCharacter, hash));
    }

    static unsigned processPendingCharacterImpl(const bool hasPendingCharacter, const UChar pendingCharacter, const unsigned hash)
    {
        unsigned result = hash;

        // Handle end case.
        if (hasPendingCharacter)
            return calculateWithRemainingLastCharacter(result, pendingCharacter);
        return result;
    }

    unsigned m_hash { stringHashingStartValue };
    UChar m_pendingCharacter { 0 };
    bool m_hasPendingCharacter { false };
};

} // namespace WTF

using WTF::SuperFastHash;

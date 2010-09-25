/*
 * Copyright (C) 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#ifndef WTF_StringHashFunctions_h
#define WTF_StringHashFunctions_h

#include <wtf/unicode/Unicode.h>

namespace WTF {

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
static const unsigned stringHashingStartValue = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
// char* data is interpreted as latin-encoded (zero extended to 16 bits).
class StringHasher {
public:
    inline StringHasher()
        : m_hash(stringHashingStartValue)
        , m_cachedCharacter(invalidCharacterValue)
    {
    }

    inline void addCharacters(UChar a, UChar b)
    {
        ASSERT(m_cachedCharacter == invalidCharacterValue);
        addCharactersToHash(a, b);
    }

    inline void addCharacter(UChar ch)
    {
        ASSERT(ch != invalidCharacterValue);
        if (m_cachedCharacter != invalidCharacterValue) {
            addCharactersToHash(m_cachedCharacter, ch);
            m_cachedCharacter = invalidCharacterValue;
            return;
        }

        m_cachedCharacter = ch;
    }

    inline unsigned hash() const
    {
        unsigned result = m_hash;

        // Handle end case.
        if (m_cachedCharacter != invalidCharacterValue) {
            result += m_cachedCharacter;
            result ^= result << 11;
            result += result >> 17;
        }

        // Force "avalanching" of final 31 bits.
        result ^= result << 3;
        result += result >> 5;
        result ^= result << 2;
        result += result >> 15;
        result ^= result << 10;

        // First bit is used in UStringImpl for m_isIdentifier.
        result &= 0x7fffffff;

        // This avoids ever returning a hash code of 0, since that is used to
        // signal "hash not computed yet", using a value that is likely to be
        // effectively the same as 0 when the low bits are masked.
        if (!result)
            return 0x40000000;

        return result;
    }

    template<typename T, UChar Converter(T)> static inline unsigned createHash(const T* data, unsigned length)
    {
        StringHasher hasher;
        bool rem = length & 1;
        length >>= 1;

        while (length--) {
            hasher.addCharacters(Converter(data[0]), Converter(data[1]));
            data += 2;
        }

        if (rem)
            hasher.addCharacter(Converter(*data));

        return hasher.hash();
    }

    template<typename T, UChar Converter(T)> static inline unsigned createHash(const T* data)
    {
        StringHasher hasher;

        while (true) {
            UChar b0 = Converter(*data++);
            if (!b0)
                break;
            UChar b1 = Converter(*data++);
            if (!b1) {
                hasher.addCharacter(b0);
                break;
            }

            hasher.addCharacters(b0, b1);
        }

        return hasher.hash();
    }

    template<typename T> static inline unsigned createHash(const T* data, unsigned length)
    {
        return createHash<T, defaultCoverter>(data, length);
    }

    template<typename T> static inline unsigned createHash(const T* data)
    {
        return createHash<T, defaultCoverter>(data);
    }

private:
    static inline UChar defaultCoverter(UChar ch)
    {
        return ch;
    }

    static inline UChar defaultCoverter(char ch)
    {
        return static_cast<unsigned char>(ch);
    }

    inline void addCharactersToHash(UChar a, UChar b)
    {
        m_hash += a;
        unsigned tmp = (b << 11) ^ m_hash;
        m_hash = (m_hash << 16) ^ tmp;
        m_hash += m_hash >> 11;
    }

    unsigned m_hash;
    UChar m_cachedCharacter;

    static const UChar invalidCharacterValue = 0xfffe;
};



inline unsigned stringHash(const UChar* data, unsigned length)
{
    return StringHasher::createHash<UChar>(data, length);
}

inline unsigned stringHash(const char* data, unsigned length)
{
    return StringHasher::createHash<char>(data, length);
}

inline unsigned stringHash(const char* data)
{
    return StringHasher::createHash<char>(data);
}

} // namespace WTF

#endif // WTF_StringHashFunctions_h

/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WidthCache_h
#define WidthCache_h

#include "TextRun.h"
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashSet.h>
#include <wtf/Hasher.h>
#include <wtf/MemoryPressureHandler.h>

namespace WebCore {

struct GlyphOverflow;

class WidthCache {
private:
    // Used to optimize small strings as hash table keys. Avoids malloc'ing an out-of-line StringImpl.
    class SmallStringKey {
    public:
        static unsigned capacity() { return s_capacity; }

        SmallStringKey()
            : m_length(s_emptyValueLength)
        {
        }

        SmallStringKey(WTF::HashTableDeletedValueType)
            : m_length(s_deletedValueLength)
        {
        }

        template<typename CharacterType> SmallStringKey(CharacterType* characters, unsigned short length)
            : m_length(length)
        {
            ASSERT(length <= s_capacity);

            StringHasher hasher;

            bool remainder = length & 1;
            length >>= 1;

            unsigned i = 0;
            while (length--) {
                m_characters[i] = characters[i];
                m_characters[i + 1] = characters[i + 1];
                hasher.addCharactersAssumingAligned(characters[i], characters[i + 1]);
                i += 2;
            }

            if (remainder) {
                m_characters[i] = characters[i];
                hasher.addCharacter(characters[i]);
            }

            m_hash = hasher.hash();
        }

        const UChar* characters() const { return m_characters; }
        unsigned short length() const { return m_length; }
        unsigned hash() const { return m_hash; }

        bool isHashTableDeletedValue() const { return m_length == s_deletedValueLength; }
        bool isHashTableEmptyValue() const { return m_length == s_emptyValueLength; }

    private:
        static const unsigned s_capacity = 15;
        static const unsigned s_emptyValueLength = s_capacity + 1;
        static const unsigned s_deletedValueLength = s_capacity + 2;

        unsigned m_hash;
        unsigned short m_length;
        UChar m_characters[s_capacity];
    };

    struct SmallStringKeyHash {
        static unsigned hash(const SmallStringKey& key) { return key.hash(); }
        static bool equal(const SmallStringKey& a, const SmallStringKey& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = true; // Empty and deleted values have lengths that are not equal to any valid length.
    };

    struct SmallStringKeyHashTraits : WTF::SimpleClassHashTraits<SmallStringKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const SmallStringKey& key) { return key.isHashTableEmptyValue(); }
        static const int minimumTableSize = 16;
    };

    friend bool operator==(const SmallStringKey&, const SmallStringKey&);

public:
    WidthCache()
        : m_interval(s_maxInterval)
        , m_countdown(m_interval)
    {
    }

    float* add(StringView text, float entry)
    {
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
            return nullptr;

        if (text.length() > SmallStringKey::capacity())
            return nullptr;

        if (m_countdown > 0) {
            --m_countdown;
            return nullptr;
        }
        return addSlowCase(text, entry);
    }

    float* add(const TextRun& run, float entry, bool hasKerningOrLigatures, bool hasWordSpacingOrLetterSpacing, GlyphOverflow* glyphOverflow)
    {
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
            return nullptr;
        // The width cache is not really profitable unless we're doing expensive glyph transformations.
        if (!hasKerningOrLigatures)
            return nullptr;
        // Word spacing and letter spacing can change the width of a word.
        if (hasWordSpacingOrLetterSpacing)
            return nullptr;
        // Since this is just a width cache, we don't have enough information to satisfy glyph queries.
        if (glyphOverflow)
            return nullptr;
        // If we allow tabs and a tab occurs inside a word, the width of the word varies based on its position on the line.
        if (run.allowTabs())
            return nullptr;
        if (run.length() > SmallStringKey::capacity())
            return nullptr;

        if (m_countdown > 0) {
            --m_countdown;
            return nullptr;
        }

        return addSlowCase(run.text(), entry);
    }

    void clear()
    {
        m_singleCharMap.clear();
        m_map.clear();
    }

private:

    float* addSlowCase(StringView text, float entry)
    {
        int length = text.length();
        bool isNewEntry;
        float* value;
        if (length == 1) {
            SingleCharMap::AddResult addResult = m_singleCharMap.fastAdd(text[0], entry);
            isNewEntry = addResult.isNewEntry;
            value = &addResult.iterator->value;
        } else {
            SmallStringKey smallStringKey;
            if (text.is8Bit())
                smallStringKey = SmallStringKey(text.characters8(), length);
            else
                smallStringKey = SmallStringKey(text.characters16(), length);

            Map::AddResult addResult = m_map.fastAdd(smallStringKey, entry);
            isNewEntry = addResult.isNewEntry;
            value = &addResult.iterator->value;
        }

        // Cache hit: ramp up by sampling the next few words.
        if (!isNewEntry) {
            m_interval = s_minInterval;
            return value;
        }

        // Cache miss: ramp down by increasing our sampling interval.
        if (m_interval < s_maxInterval)
            ++m_interval;
        m_countdown = m_interval;

        if ((m_singleCharMap.size() + m_map.size()) < s_maxSize)
            return value;

        // No need to be fancy: we're just trying to avoid pathological growth.
        m_singleCharMap.clear();
        m_map.clear();
        return nullptr;
    }

    typedef HashMap<SmallStringKey, float, SmallStringKeyHash, SmallStringKeyHashTraits> Map;
    typedef HashMap<uint32_t, float, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> SingleCharMap;
    static const int s_minInterval = -3; // A cache hit pays for about 3 cache misses.
    static const int s_maxInterval = 20; // Sampling at this interval has almost no overhead.
    static const int s_maxSize = 500000; // Just enough to guard against pathological growth.

    int m_interval;
    int m_countdown;
    SingleCharMap m_singleCharMap;
    Map m_map;
};

inline bool operator==(const WidthCache::SmallStringKey& a, const WidthCache::SmallStringKey& b)
{
    if (a.length() != b.length())
        return false;
    return WTF::equal(a.characters(), b.characters(), a.length());
}

} // namespace WebCore

#endif // WidthCache_h

/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/TextRun.h>
#include <WebCore/TextSpacing.h>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashSet.h>
#include <wtf/Hasher.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WYHash.h>

namespace WebCore {

class FontCascade;

struct TextShapingContext {
    bool hasKerningOrLigatures { false };
    bool hasWordSpacingOrLetterSpacing { false };
    bool hasTextSpacing { false };

    TextShapingContext(const FontCascade&);
};

namespace TextMeasurementCacheDefaults {
static constexpr int minInterval = -3; // A cache hit pays for about 3 cache misses.
static constexpr int maxInterval = 20; // Sampling at this interval has almost no overhead.
static constexpr unsigned maxSize = 500000; // Just enough to guard against pathological growth.
static constexpr unsigned maxTextLength = 64; // Maximum text length for SmallStringKey.
}

template <typename CachedType,
    int InitialInterval = TextMeasurementCacheDefaults::maxInterval,
    int MinInterval = TextMeasurementCacheDefaults::minInterval,
    int MaxInterval = TextMeasurementCacheDefaults::maxInterval,
    unsigned MaxSize = TextMeasurementCacheDefaults::maxSize,
    unsigned MaxTextLength = TextMeasurementCacheDefaults::maxTextLength>
class TextMeasurementCache {
private:
    // Used to optimize small strings as hash table keys. Avoids malloc'ing an out-of-line StringImpl.
    class SmallStringKey {
    public:
        static constexpr unsigned capacity() { return s_capacity; }

        constexpr SmallStringKey() = default;

        constexpr SmallStringKey(WTF::HashTableDeletedValueType)
            : m_hashAndLength(s_deletedValueLength)
        {
        }

        ALWAYS_INLINE SmallStringKey(StringView string)
        {
            unsigned length = string.length();
            ASSERT(length <= s_capacity);
            if (string.is8Bit())
                copySmallCharacters(std::span { m_characters }, string.span8());
            else
                copySmallCharacters(std::span { m_characters }, string.span16());
            m_hashAndLength = WYHash::computeHashAndMaskTop8Bits(std::span<const char16_t> { m_characters }.first(s_capacity)) | (length << 24);
        }

        const char16_t* characters() const { return m_characters.data(); }
        unsigned length() const { return m_hashAndLength >> 24; }
        unsigned hash() const { return m_hashAndLength & 0x00ffffffU; }

        bool isHashTableDeletedValue() const { return m_hashAndLength == s_deletedValueLength; }
        bool isHashTableEmptyValue() const { return !m_hashAndLength; }
        // Empty and deleted values have lengths that are not equal to any valid length.
        static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

        friend bool operator==(const SmallStringKey&, const SmallStringKey&) = default;

    private:
        static constexpr unsigned s_capacity = MaxTextLength;
        static constexpr unsigned s_deletedValueLength = s_capacity + 1;

        template<typename CharacterType>
        ALWAYS_INLINE static void copySmallCharacters(std::span<char16_t, s_capacity> destination, std::span<const CharacterType> source)
        {
            if constexpr (std::is_same_v<CharacterType, char16_t>)
                memcpySpan(destination, source);
            else {
                for (auto [sourceCharacter, destinationCharacter] : zippedRange(source, destination))
                    destinationCharacter = sourceCharacter;
            }
        }

        std::array<char16_t, s_capacity> m_characters { };
        unsigned m_hashAndLength { 0 };
    };

    struct SmallStringKeyHashTraits : SimpleClassHashTraits<SmallStringKey> {
        static constexpr bool emptyValueIsZero = true;
        static constexpr bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const SmallStringKey& key) { return key.isHashTableEmptyValue(); }
        static constexpr int minimumTableSize = 16;
    };

public:
    TextMeasurementCache()
        : m_interval(InitialInterval)
        , m_countdown(InitialInterval)
    {
    }

    CachedType* add(StringView text, CachedType&& entry)
    {
        unsigned length = text.length();

        // Do not allow length = 0. This allows SmallStringKey empty-value-is-zero.
        if (!length) [[unlikely]]
            return nullptr;

        if (length > MaxTextLength)
            return nullptr;

        if (m_countdown > 0) {
            --m_countdown;
            return nullptr;
        }

        return addSlowCase(text, WTF::move(entry));
    }

    CachedType* add(const TextRun& run, CachedType&& entry, TextShapingContext shapingContext)
    {
        // The width cache is not really profitable unless we're doing expensive glyph transformations.
        if (!shapingContext.hasKerningOrLigatures)
            return nullptr;
        // Word spacing and letter spacing can change the width of a word.
        if (shapingContext.hasWordSpacingOrLetterSpacing)
            return nullptr;
        // If a tab occurs inside a word, the width of the word varies based on its position on the line.
        if (run.allowTabs() && run.text().contains(tabCharacter))
            return nullptr;
        // width calculation with text-spacing depends on context of adjacent characters.
        if (shapingContext.hasTextSpacing && invalidateCacheForTextSpacing(run))
            return nullptr;

        return add(run.text(), WTF::move(entry));
    }

    void clear()
    {
        m_singleCharMap.clear();
        m_map.clear();
    }

private:

    CachedType* addSlowCase(StringView text, CachedType&& entry)
    {
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure())
            return nullptr;

        unsigned length = text.length();
        bool isNewEntry;
        CachedType* value;
        if (length == 1) {
            // The map use 0 for empty key, thus we do +1 here to avoid conflicting against empty key.
            // This is fine since the key is uint32_t while character is char16_t. So +1 never causes overflow.
            uint32_t character = text[0];
            auto addResult = m_singleCharMap.fastAdd(character + 1, WTF::move(entry));
            isNewEntry = addResult.isNewEntry;
            value = &addResult.iterator->value;
        } else {
            auto addResult = m_map.fastAdd(text, WTF::move(entry));
            isNewEntry = addResult.isNewEntry;
            value = &addResult.iterator->value;
        }

        // Cache hit: ramp up by sampling the next few words.
        if (!isNewEntry) {
            m_interval = MinInterval;
            return value;
        }

        // Cache miss: ramp down by increasing our sampling interval.
        if (m_interval < MaxInterval)
            ++m_interval;
        m_countdown = m_interval;

        if ((m_singleCharMap.size() + m_map.size()) < MaxSize)
            return value;

        // No need to be fancy: we're just trying to avoid pathological growth.
        m_singleCharMap.clear();
        m_map.clear();
        return nullptr;
    }

    // returns true if cache is/was invalidated
    bool invalidateCacheForTextSpacing(const TextRun& textRun)
    {
        if (m_hasSeenIdeograph)
            return true;
        const auto& text = textRun.textAsString();
        for (unsigned index = 0; index < text.length(); ++index) {
            if (TextSpacing::isIdeograph(text.characterAt(index))) {
                m_hasSeenIdeograph = true;
                clear();
                return true;
            }
        }

        return false;
    }

    using Map = HashMap<SmallStringKey, CachedType, DefaultHash<SmallStringKey>, SmallStringKeyHashTraits>;
    using SingleCharMap = HashMap<uint32_t, CachedType, DefaultHash<uint32_t>, HashTraits<uint32_t>>;

    int m_interval;
    int m_countdown;
    SingleCharMap m_singleCharMap;
    Map m_map;
    bool m_hasSeenIdeograph;
};

} // namespace WebCore

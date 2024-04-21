/*
 * Copyright (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2007, 2008, 2011-2014 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "SpaceSplitString.h"

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

static_assert(!(sizeof(SpaceSplitStringData) % sizeof(uintptr_t)), "SpaceSplitStringDataTail is aligned to WordSize");

template <typename CharacterType, typename TokenProcessor>
static inline void tokenizeSpaceSplitString(TokenProcessor& tokenProcessor, std::span<const CharacterType> characters)
{
    for (unsigned start = 0; ; ) {
        while (start < characters.size() && isASCIIWhitespace(characters[start]))
            ++start;
        if (start >= characters.size())
            break;
        unsigned end = start + 1;
        while (end < characters.size() && !isASCIIWhitespace(characters[end]))
            ++end;

        if (!tokenProcessor.processToken(characters.subspan(start, end - start)))
            return;

        start = end + 1;
    }
}

template<typename TokenProcessor>
static inline void tokenizeSpaceSplitString(TokenProcessor& tokenProcessor, StringView string)
{
    ASSERT(!string.isNull());

    if (string.is8Bit())
        tokenizeSpaceSplitString(tokenProcessor, string.span8());
    else
        tokenizeSpaceSplitString(tokenProcessor, string.span16());
}

bool SpaceSplitStringData::containsAll(SpaceSplitStringData& other)
{
    if (this == &other)
        return true;

    unsigned otherSize = other.m_size;
    unsigned i = 0;
    do {
        if (!contains(other[i]))
            return false;
        ++i;
    } while (i < otherSize);
    return true;
}

struct SpaceSplitStringTableKeyTraits : public HashTraits<AtomString>
{
    // The number 200 for typicalNumberOfSpaceSplitString was based on the typical number of unique class names
    // on typical websites on August 2013.
    static const unsigned typicalNumberOfSpaceSplitString = 200;
    static const int minimumTableSize = WTF::HashTableCapacityForSize<typicalNumberOfSpaceSplitString>::value;
};

typedef HashMap<AtomString, SpaceSplitStringData*, AtomStringHash, SpaceSplitStringTableKeyTraits> SpaceSplitStringTable;

static SpaceSplitStringTable& spaceSplitStringTable()
{
    static NeverDestroyed<SpaceSplitStringTable> table;
    return table;
}

void SpaceSplitString::set(const AtomString& inputString, ShouldFoldCase shouldFoldCase)
{
    if (inputString.isNull()) {
        clear();
        return;
    }
    m_data = SpaceSplitStringData::create(shouldFoldCase == ShouldFoldCase::Yes ? inputString.convertToASCIILowercase() : inputString);
}

template<typename ReferenceCharacterType>
class TokenIsEqualToCharactersTokenProcessor {
public:
    explicit TokenIsEqualToCharactersTokenProcessor(const ReferenceCharacterType* referenceCharacters, unsigned length)
        : m_referenceCharacters(referenceCharacters)
        , m_referenceLength(length)
    {
    }

    template <typename TokenCharacterType>
    bool processToken(std::span<const TokenCharacterType> characters)
    {
        if (characters.size() == m_referenceLength && equal(m_referenceCharacters, characters)) {
            m_referenceStringWasFound = true;
            return false;
        }
        return true;
    }

    bool referenceStringWasFound() const { return m_referenceStringWasFound; }

private:
    const ReferenceCharacterType* m_referenceCharacters;
    unsigned m_referenceLength;
    bool m_referenceStringWasFound { false };
};

template <typename ValueCharacterType>
static bool spaceSplitStringContainsValueInternal(StringView spaceSplitString, StringView value)
{
    TokenIsEqualToCharactersTokenProcessor<ValueCharacterType> tokenProcessor(value.span<ValueCharacterType>().data(), value.length());
    tokenizeSpaceSplitString(tokenProcessor, spaceSplitString);
    return tokenProcessor.referenceStringWasFound();
}

bool SpaceSplitString::spaceSplitStringContainsValue(StringView spaceSplitString, StringView value, ShouldFoldCase shouldFoldCase)
{
    if (spaceSplitString.isNull())
        return false;

    if (value.is8Bit())
        return spaceSplitStringContainsValueInternal<LChar>(shouldFoldCase == ShouldFoldCase::Yes ? StringView { spaceSplitString.convertToASCIILowercase() } : spaceSplitString, value);
    return spaceSplitStringContainsValueInternal<UChar>(shouldFoldCase == ShouldFoldCase::Yes ? StringView { spaceSplitString.convertToASCIILowercase() } : spaceSplitString, value);
}

class TokenCounter {
    WTF_MAKE_NONCOPYABLE(TokenCounter);
public:
    TokenCounter() : m_tokenCount(0) { }

    template<typename CharacterType> bool processToken(std::span<const CharacterType>)
    {
        ++m_tokenCount;
        return true;
    }

    unsigned tokenCount() const { return m_tokenCount; }

private:
    unsigned m_tokenCount;
};

class TokenAtomStringInitializer {
    WTF_MAKE_NONCOPYABLE(TokenAtomStringInitializer);
public:
    TokenAtomStringInitializer(const AtomString& keyString, AtomString* memory)
        : m_keyString(keyString)
        , m_memoryBucket(memory)
    { }

    template<typename CharacterType> bool processToken(std::span<const CharacterType> characters)
    {
        if (characters.size() == m_keyString.length())
            new (NotNull, m_memoryBucket) AtomString(m_keyString);
        else
            new (NotNull, m_memoryBucket) AtomString(characters);
        ++m_memoryBucket;
        return true;
    }

    const AtomString* nextMemoryBucket() const { return m_memoryBucket; }

private:
    const AtomString& m_keyString;
    AtomString* m_memoryBucket;
};

inline Ref<SpaceSplitStringData> SpaceSplitStringData::create(const AtomString& keyString, unsigned tokenCount)
{
    ASSERT(tokenCount);

    RELEASE_ASSERT(tokenCount < (std::numeric_limits<unsigned>::max() - sizeof(SpaceSplitStringData)) / sizeof(AtomString));
    unsigned sizeToAllocate = sizeof(SpaceSplitStringData) + tokenCount * sizeof(AtomString);
    SpaceSplitStringData* spaceSplitStringData = static_cast<SpaceSplitStringData*>(fastMalloc(sizeToAllocate));

    new (NotNull, spaceSplitStringData) SpaceSplitStringData(keyString, tokenCount);
    AtomString* tokenArrayStart = spaceSplitStringData->tokenArrayStart();
    TokenAtomStringInitializer tokenInitializer(keyString, tokenArrayStart);
    tokenizeSpaceSplitString(tokenInitializer, keyString);
    ASSERT(static_cast<unsigned>(tokenInitializer.nextMemoryBucket() - tokenArrayStart) == tokenCount);
    ASSERT(reinterpret_cast<const char*>(tokenInitializer.nextMemoryBucket()) == reinterpret_cast<const char*>(spaceSplitStringData) + sizeToAllocate);

    return adoptRef(*spaceSplitStringData);
}

RefPtr<SpaceSplitStringData> SpaceSplitStringData::create(const AtomString& keyString)
{
    ASSERT(isMainThread());
    ASSERT(!keyString.isNull());

    auto addResult = spaceSplitStringTable().add(keyString, nullptr);
    if (!addResult.isNewEntry)
        return addResult.iterator->value;

    // Nothing in the cache? Let's create a new SpaceSplitStringData if the input has something useful.
    // 1) We find the number of strings in the input to know how much size we need to allocate.
    TokenCounter tokenCounter;
    tokenizeSpaceSplitString(tokenCounter, keyString);
    unsigned tokenCount = tokenCounter.tokenCount();

    if (!tokenCount)
        return nullptr;

    RefPtr<SpaceSplitStringData> spaceSplitStringData = create(keyString, tokenCount);
    addResult.iterator->value = spaceSplitStringData.get();
    return spaceSplitStringData;
}


void SpaceSplitStringData::destroy(SpaceSplitStringData* spaceSplitString)
{
    ASSERT(isMainThread());

    spaceSplitStringTable().remove(spaceSplitString->m_keyString);

    unsigned i = 0;
    unsigned size = spaceSplitString->size();
    const AtomString* data = spaceSplitString->tokenArrayStart();
    do {
        data[i].~AtomString();
        ++i;
    } while (i < size);

    spaceSplitString->~SpaceSplitStringData();

    fastFree(spaceSplitString);
}

} // namespace WebCore

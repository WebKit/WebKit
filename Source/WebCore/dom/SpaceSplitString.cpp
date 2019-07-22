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

#include "HTMLParserIdioms.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

COMPILE_ASSERT(!(sizeof(SpaceSplitStringData) % sizeof(uintptr_t)), SpaceSplitStringDataTailIsAlignedToWordSize);

template <typename CharacterType, typename TokenProcessor>
static inline void tokenizeSpaceSplitString(TokenProcessor& tokenProcessor, const CharacterType* characters, unsigned length)
{
    for (unsigned start = 0; ; ) {
        while (start < length && isHTMLSpace(characters[start]))
            ++start;
        if (start >= length)
            break;
        unsigned end = start + 1;
        while (end < length && isNotHTMLSpace(characters[end]))
            ++end;

        if (!tokenProcessor.processToken(characters + start, end - start))
            return;

        start = end + 1;
    }
}

template<typename TokenProcessor>
static inline void tokenizeSpaceSplitString(TokenProcessor& tokenProcessor, const String& string)
{
    ASSERT(!string.isNull());

    const StringImpl& stringImpl = *string.impl();
    if (stringImpl.is8Bit())
        tokenizeSpaceSplitString(tokenProcessor, stringImpl.characters8(), stringImpl.length());
    else
        tokenizeSpaceSplitString(tokenProcessor, stringImpl.characters16(), stringImpl.length());
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

void SpaceSplitString::set(const AtomString& inputString, bool shouldFoldCase)
{
    if (inputString.isNull()) {
        clear();
        return;
    }
    m_data = SpaceSplitStringData::create(shouldFoldCase ? inputString.convertToASCIILowercase() : inputString);
}

class TokenIsEqualToCStringTokenProcessor {
public:
    explicit TokenIsEqualToCStringTokenProcessor(const char* referenceString, unsigned referenceStringLength)
        : m_referenceString(referenceString)
        , m_referenceStringLength(referenceStringLength)
        , m_referenceStringWasFound(false)
    {
    }

    template <typename CharacterType>
    bool processToken(const CharacterType* characters, unsigned length)
    {
        if (length == m_referenceStringLength && equal(characters, reinterpret_cast<const LChar*>(m_referenceString), length)) {
            m_referenceStringWasFound = true;
            return false;
        }
        return true;
    }

    bool referenceStringWasFound() const { return m_referenceStringWasFound; }

private:
    const char* m_referenceString;
    unsigned m_referenceStringLength;
    bool m_referenceStringWasFound;
};

bool SpaceSplitString::spaceSplitStringContainsValue(const String& inputString, const char* value, unsigned valueLength, bool shouldFoldCase)
{
    if (inputString.isNull())
        return false;

    TokenIsEqualToCStringTokenProcessor tokenProcessor(value, valueLength);
    tokenizeSpaceSplitString(tokenProcessor, shouldFoldCase ? inputString.impl()->convertToASCIILowercase() : inputString);
    return tokenProcessor.referenceStringWasFound();
}

class TokenCounter {
    WTF_MAKE_NONCOPYABLE(TokenCounter);
public:
    TokenCounter() : m_tokenCount(0) { }

    template<typename CharacterType> bool processToken(const CharacterType*, unsigned)
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
    TokenAtomStringInitializer(AtomString* memory) : m_memoryBucket(memory) { }

    template<typename CharacterType> bool processToken(const CharacterType* characters, unsigned length)
    {
        new (NotNull, m_memoryBucket) AtomString(characters, length);
        ++m_memoryBucket;
        return true;
    }

    const AtomString* nextMemoryBucket() const { return m_memoryBucket; }

private:
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
    TokenAtomStringInitializer tokenInitializer(tokenArrayStart);
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

/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "SmallStrings.h"

#include "JSGlobalObject.h"
#include "JSString.h"

#include <wtf/Noncopyable.h>

namespace JSC {
static const unsigned numCharactersToStore = 0x100;

static inline bool isMarked(JSString* string)
{
    return string && Heap::isCellMarked(string);
}

class SmallStringsStorage : public Noncopyable {
public:
    SmallStringsStorage();

    UString::Rep* rep(unsigned char character) { return m_reps[character].get(); }

private:
    RefPtr<UString::Rep> m_reps[numCharactersToStore];
};

SmallStringsStorage::SmallStringsStorage()
{
    UChar* characterBuffer = 0;
    RefPtr<UStringImpl> baseString = UStringImpl::createUninitialized(numCharactersToStore, characterBuffer);
    for (unsigned i = 0; i < numCharactersToStore; ++i) {
        characterBuffer[i] = i;
        m_reps[i] = UStringImpl::create(baseString, i, 1);
    }
}

SmallStrings::SmallStrings()
{
    COMPILE_ASSERT(numCharactersToStore == sizeof(m_singleCharacterStrings) / sizeof(m_singleCharacterStrings[0]), IsNumCharactersConstInSyncWithClassUsage);
    clear();
}

SmallStrings::~SmallStrings()
{
}

void SmallStrings::markChildren(MarkStack& markStack)
{
    /*
       Our hypothesis is that small strings are very common. So, we cache them
       to avoid GC churn. However, in cases where this hypothesis turns out to
       be false -- including the degenerate case where all JavaScript execution
       has terminated -- we don't want to waste memory.

       To test our hypothesis, we check if any small string has been marked. If
       so, it's probably reasonable to mark the rest. If not, we clear the cache.
     */

    bool isAnyStringMarked = isMarked(m_emptyString);
    for (unsigned i = 0; i < numCharactersToStore && !isAnyStringMarked; ++i)
        isAnyStringMarked = isMarked(m_singleCharacterStrings[i]);
    
    if (!isAnyStringMarked) {
        clear();
        return;
    }
    
    if (m_emptyString)
        markStack.append(m_emptyString);
    for (unsigned i = 0; i < numCharactersToStore; ++i) {
        if (m_singleCharacterStrings[i])
            markStack.append(m_singleCharacterStrings[i]);
    }
}

void SmallStrings::clear()
{
    m_emptyString = 0;
    for (unsigned i = 0; i < numCharactersToStore; ++i)
        m_singleCharacterStrings[i] = 0;
}

unsigned SmallStrings::count() const
{
    unsigned count = 0;
    if (m_emptyString)
        ++count;
    for (unsigned i = 0; i < numCharactersToStore; ++i) {
        if (m_singleCharacterStrings[i])
            ++count;
    }
    return count;
}

void SmallStrings::createEmptyString(JSGlobalData* globalData)
{
    ASSERT(!m_emptyString);
    m_emptyString = new (globalData) JSString(globalData, "", JSString::HasOtherOwner);
}

void SmallStrings::createSingleCharacterString(JSGlobalData* globalData, unsigned char character)
{
    if (!m_storage)
        m_storage.set(new SmallStringsStorage);
    ASSERT(!m_singleCharacterStrings[character]);
    m_singleCharacterStrings[character] = new (globalData) JSString(globalData, m_storage->rep(character), JSString::HasOtherOwner);
}

UString::Rep* SmallStrings::singleCharacterStringRep(unsigned char character)
{
    if (!m_storage)
        m_storage.set(new SmallStringsStorage);
    return m_storage->rep(character);
}

} // namespace JSC

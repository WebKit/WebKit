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

namespace JSC {

class SmallStringsStorage {
public:
    SmallStringsStorage();
    ~SmallStringsStorage();

    UString::Rep* rep(unsigned char character) { return &reps[character]; }

private:
    UChar characters[0x100];
    UString::Rep* reps;
};

SmallStringsStorage::SmallStringsStorage()
    : reps(static_cast<UString::Rep*>(fastZeroedMalloc(sizeof(UString::Rep) * 0x100)))
{
    for (unsigned i = 0; i < 0x100; ++i) {
        characters[i] = i;
        reps[i].offset = i;
        reps[i].len = 1;
        reps[i].rc = 1;
        reps[i].baseString = &reps[0];
    }
    reps[0].rc = 0x101;
    reps[0].buf = characters;

    // make sure UString doesn't try to reuse the buffer by pretending we have one more character in it
    reps[0].usedCapacity = 0x101;
    reps[0].capacity = 0x101;
}

SmallStringsStorage::~SmallStringsStorage()
{
    fastFree(reps);
}

SmallStrings::SmallStrings()
    : m_emptyString(0)
    , m_storage(0)
{
    for (unsigned i = 0; i < 0x100; ++i)
        m_singleCharacterStrings[i] = 0;
}

SmallStrings::~SmallStrings()
{
}

void SmallStrings::mark()
{
    if (m_emptyString && !m_emptyString->marked())
        m_emptyString->mark();
    for (unsigned i = 0; i < 0x100; ++i) {
        if (m_singleCharacterStrings[i] && !m_singleCharacterStrings[i]->marked())
            m_singleCharacterStrings[i]->mark();
    }
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

}

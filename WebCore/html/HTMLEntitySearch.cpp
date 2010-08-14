/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "HTMLEntitySearch.h"

#include "HTMLEntityTable.h"

namespace WebCore {

namespace {
    
const HTMLEntityTableEntry* halfway(const HTMLEntityTableEntry* left, const HTMLEntityTableEntry* right)
{
    return &left[(right - left) / 2];
}

}
    
HTMLEntitySearch::HTMLEntitySearch()
    : m_currentLength(0)
    , m_currentValue(0)
    , m_lastMatch(0)
    , m_start(HTMLEntityTable::start())
    , m_end(HTMLEntityTable::end())
{
}

HTMLEntitySearch::CompareResult HTMLEntitySearch::compare(const HTMLEntityTableEntry* entry, UChar nextCharacter) const
{
    if (entry->length < m_currentLength + 1)
        return Before;
    UChar entryNextCharacter = entry->entity[m_currentLength];
    if (entryNextCharacter == nextCharacter)
        return Prefix;
    return entryNextCharacter < nextCharacter ? Before : After;
}

const HTMLEntityTableEntry* HTMLEntitySearch::findStart(UChar nextCharacter) const
{
    const HTMLEntityTableEntry* left = m_start;
    const HTMLEntityTableEntry* right = m_end;
    if (left == right)
        return left;
    CompareResult result = compare(left, nextCharacter);
    if (result == Prefix)
        return left;
    if (result == After)
        return right;
    while (left + 1 < right) {
        const HTMLEntityTableEntry* probe = halfway(left, right);
        result = compare(probe, nextCharacter);
        if (result == Before)
            left = probe;
        else {
            ASSERT(result == After || result == Prefix);
            right = probe;
        }
    }
    ASSERT(left + 1 == right);
    return right;
}

const HTMLEntityTableEntry* HTMLEntitySearch::findEnd(UChar nextCharacter) const
{
    const HTMLEntityTableEntry* left = m_start;
    const HTMLEntityTableEntry* right = m_end;
    if (left == right)
        return right;
    CompareResult result = compare(right, nextCharacter);
    if (result == Prefix)
        return right;
    if (result == Before)
        return left;
    while (left + 1 < right) {
        const HTMLEntityTableEntry* probe = halfway(left, right);
        result = compare(probe, nextCharacter);
        if (result == After)
            right = probe;
        else {
            ASSERT(result == Before || result == Prefix);
            left = probe;
        }
    }
    ASSERT(left + 1 == right);
    return left;
}

void HTMLEntitySearch::advance(UChar nextCharacter)
{
    ASSERT(isEntityPrefix());
    if (!m_currentLength) {
        m_start = HTMLEntityTable::start(nextCharacter);
        m_end = HTMLEntityTable::end(nextCharacter);
    } else {
        m_start = findStart(nextCharacter);
        m_end = findEnd(nextCharacter);
        if (m_start == m_end && compare(m_start, nextCharacter) != Prefix)
            return fail();
    }
    ++m_currentLength;
    if (m_start->length != m_currentLength) {
        m_currentValue = 0;
        return;
    }
    m_lastMatch = m_start;
    m_currentValue = m_lastMatch->value;
}

}

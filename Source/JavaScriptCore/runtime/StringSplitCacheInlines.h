/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012 the V8 project authors. All rights reserved.
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

#include "DeferGC.h"
#include "StringSplitCache.h"

namespace JSC {

inline JSCellButterfly* StringSplitCache::getForString(const String& subject, const String& separator)
{
    AssertNoGC assertNoGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return nullptr;
    if (!separator.impl() || !separator.impl()->isAtom())
        return nullptr;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    auto* separatorImpl = static_cast<AtomStringImpl*>(separator.impl());
    size_t index = subjectImpl->hash() & (m_stringEntries.size() - 1);
    {
        auto& entry = m_stringEntries[index];
        if (entry.m_subject == subjectImpl && entry.m_separator == separatorImpl)
            return entry.m_butterfly;
    }
    {
        auto& entry = m_stringEntries[(index + 1) & (m_stringEntries.size() - 1)];
        if (entry.m_subject == subjectImpl && entry.m_separator == separatorImpl)
            return entry.m_butterfly;
    }
    return nullptr;
}

inline void StringSplitCache::setForString(const String& subject, const String& separator, JSCellButterfly* butterfly)
{
    AssertNoGC assertNoGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return;
    if (!separator.impl() || !separator.impl()->isAtom())
        return;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    auto* separatorImpl = static_cast<AtomStringImpl*>(separator.impl());
    size_t index = subjectImpl->hash() & (m_stringEntries.size() - 1);
    {
        auto& entry1 = m_stringEntries[index];
        if (!entry1.m_subject) {
            entry1.m_subject = subjectImpl;
            entry1.m_separator = separatorImpl;
            entry1.m_butterfly = butterfly;
        } else {
            auto& entry2 = m_stringEntries[(index + 1) & (m_stringEntries.size() - 1)];
            if (!entry2.m_subject) {
                entry2.m_subject = subjectImpl;
                entry2.m_separator = separatorImpl;
                entry2.m_butterfly = butterfly;
            } else {
                entry2.m_subject = nullptr;
                entry2.m_separator = nullptr;
                entry1.m_subject = subjectImpl;
                entry1.m_separator = separatorImpl;
                entry1.m_butterfly = butterfly;
            }
        }
    }
}

inline JSCellButterfly* StringSplitCache::getForRegExp(const String& subject, RegExp* regExp, MatchResult& lastMatch)
{
    AssertNoGC assertNoGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return nullptr;
    if (!regExp)
        return nullptr;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    size_t index = regExpEntryIndex(subjectImpl, regExp);
    {
        auto& entry = m_regExpEntries[index];
        if (entry.m_subject == subjectImpl && entry.m_regExp == regExp) {
            lastMatch = entry.m_lastMatch;
            return entry.m_butterfly;
        }
    }
    {
        auto& entry = m_regExpEntries[(index + 1) & (m_regExpEntries.size() - 1)];
        if (entry.m_subject == subjectImpl && entry.m_regExp == regExp) {
            lastMatch = entry.m_lastMatch;
            return entry.m_butterfly;
        }
    }
    return nullptr;
}

inline void StringSplitCache::setForRegExp(const String& subject, RegExp* regExp, JSCellButterfly* butterfly, MatchResult lastMatch)
{
    AssertNoGC assertNoGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return;
    if (!regExp)
        return;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    size_t index = regExpEntryIndex(subjectImpl, regExp);
    auto store = [&](RegExpEntry& entry) {
        entry.m_subject = subjectImpl;
        entry.m_regExp = regExp;
        entry.m_butterfly = butterfly;
        entry.m_lastMatch = lastMatch;
    };
    auto& entry1 = m_regExpEntries[index];
    if (!entry1.m_subject) {
        store(entry1);
        return;
    }
    auto& entry2 = m_regExpEntries[(index + 1) & (m_regExpEntries.size() - 1)];
    if (!entry2.m_subject) {
        store(entry2);
        return;
    }
    entry2 = RegExpEntry { };
    store(entry1);
}

} // namespace JSC

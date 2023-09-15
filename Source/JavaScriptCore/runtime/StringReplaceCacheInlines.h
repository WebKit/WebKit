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

#include "StringReplaceCache.h"

namespace JSC {

inline StringReplaceCache::Entry* StringReplaceCache::get(const String& subject, RegExp* regExp)
{
    DisallowGC disallowGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return nullptr;
    ASSERT(regExp->global());
    ASSERT(subject.length() >= Options::thresholdForStringReplaceCache());

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    unsigned index = subjectImpl->hash() & (cacheSize - 1);
    {
        auto& entry = m_entries[index];
        if (entry.m_subject == subjectImpl && entry.m_regExp == regExp)
            return &entry;
    }
    {
        auto& entry = m_entries[(index + 1) & (cacheSize - 1)];
        if (entry.m_subject == subjectImpl && entry.m_regExp == regExp)
            return &entry;
    }
    return nullptr;
}

inline void StringReplaceCache::set(const String& subject, RegExp* regExp, JSImmutableButterfly* result, MatchResult matchResult, const Vector<int>& lastMatch)
{
    DisallowGC disallowGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    unsigned index = subjectImpl->hash() & (cacheSize - 1);
    {
        auto& entry1 = m_entries[index];
        if (!entry1.m_subject) {
            entry1.m_subject = subjectImpl;
            entry1.m_regExp = regExp;
            entry1.m_lastMatch = lastMatch;
            entry1.m_matchResult = matchResult;
            entry1.m_result = result;
        } else {
            auto& entry2 = m_entries[(index + 1) & (cacheSize - 1)];
            if (!entry2.m_subject) {
                entry2.m_subject = subjectImpl;
                entry2.m_regExp = regExp;
                entry2.m_lastMatch = lastMatch;
                entry2.m_matchResult = matchResult;
                entry2.m_result = result;
            } else {
                entry2 = { };
                entry1.m_subject = subjectImpl;
                entry1.m_regExp = regExp;
                entry1.m_lastMatch = lastMatch;
                entry1.m_matchResult = matchResult;
                entry1.m_result = result;
            }
        }
    }
}

} // namespace JSC

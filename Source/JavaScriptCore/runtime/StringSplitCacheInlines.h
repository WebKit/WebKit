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

#include "StringSplitCache.h"

namespace JSC {

inline JSImmutableButterfly* StringSplitCache::get(const String& subject, const String& separator)
{
    DisallowGC disallowGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return nullptr;
    if (!separator.impl() || !separator.impl()->isAtom())
        return nullptr;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    auto* separatorImpl = static_cast<AtomStringImpl*>(separator.impl());
    unsigned index = subjectImpl->hash() & (cacheSize - 1);
    {
        auto& entry = m_entries[index];
        if (entry.m_subject == subjectImpl && entry.m_separator == separatorImpl)
            return entry.m_butterfly;
    }
    {
        auto& entry = m_entries[(index + 1) & (cacheSize - 1)];
        if (entry.m_subject == subjectImpl && entry.m_separator == separatorImpl)
            return entry.m_butterfly;
    }
    return nullptr;
}

inline void StringSplitCache::set(const String& subject, const String& separator, JSImmutableButterfly* butterfly)
{
    DisallowGC disallowGC;
    if (!subject.impl() || !subject.impl()->isAtom())
        return;
    if (!separator.impl() || !separator.impl()->isAtom())
        return;

    auto* subjectImpl = static_cast<AtomStringImpl*>(subject.impl());
    auto* separatorImpl = static_cast<AtomStringImpl*>(separator.impl());
    unsigned index = subjectImpl->hash() & (cacheSize - 1);
    {
        auto& entry1 = m_entries[index];
        if (!entry1.m_subject) {
            entry1.m_subject = subjectImpl;
            entry1.m_separator = separatorImpl;
            entry1.m_butterfly = butterfly;
        } else {
            auto& entry2 = m_entries[(index + 1) & (cacheSize - 1)];
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

} // namespace JSC

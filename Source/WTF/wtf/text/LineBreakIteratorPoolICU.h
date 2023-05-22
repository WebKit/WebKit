/*
 * Copyright (C) 2011-2023 Apple Inc. All Rights Reserved.
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

#include <unicode/uloc.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace WTF {

class LineBreakIteratorPool {
    WTF_MAKE_NONCOPYABLE(LineBreakIteratorPool);
    WTF_MAKE_FAST_ALLOCATED;
public:
    LineBreakIteratorPool() = default;

    WTF_EXPORT_PRIVATE static LineBreakIteratorPool& sharedPool();

    UBreakIterator* take(const AtomString& locale, LineBreakIteratorMode mode)
    {
        auto localeWithOptionalBreakKeyword = TextBreakIteratorICU::makeLocaleWithBreakKeyword(locale, mode);

        UBreakIterator* iterator = nullptr;
        for (size_t i = 0; i < m_pool.size(); ++i) {
            if (m_pool[i].first == localeWithOptionalBreakKeyword) {
                iterator = m_pool[i].second;
                m_pool.remove(i);
                break;
            }
        }

        if (!iterator) {
            iterator = openLineBreakIterator(localeWithOptionalBreakKeyword);
            if (!iterator)
                return nullptr;
        }

        ASSERT(!m_vendedIterators.contains(iterator));
        m_vendedIterators.add(iterator, localeWithOptionalBreakKeyword);
        return iterator;
    }

    void put(UBreakIterator* iterator)
    {
        ASSERT(m_vendedIterators.contains(iterator));
        if (m_pool.size() == capacity) {
            closeLineBreakIterator(m_pool[0].second);
            m_pool.remove(0);
        }
        m_pool.uncheckedAppend({ m_vendedIterators.take(iterator), iterator });
    }

private:
    static constexpr size_t capacity = 4;

    Vector<std::pair<AtomString, UBreakIterator*>, capacity> m_pool;
    HashMap<UBreakIterator*, AtomString> m_vendedIterators;

    friend WTF::ThreadSpecific<LineBreakIteratorPool>::operator LineBreakIteratorPool*();
};

}

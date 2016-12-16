/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#include "TextBreakIterator.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/AtomicString.h>

namespace WTF {

class LineBreakIteratorPool {
    WTF_MAKE_NONCOPYABLE(LineBreakIteratorPool);
public:
    LineBreakIteratorPool() = default;

    static LineBreakIteratorPool& sharedPool()
    {
        static NeverDestroyed<WTF::ThreadSpecific<LineBreakIteratorPool>> pool;
        return *pool.get();
    }

    static AtomicString makeLocaleWithBreakKeyword(const AtomicString& locale, LineBreakIteratorMode mode)
    {
        switch (mode) {
        case LineBreakIteratorMode::Default:
            return locale;
        case LineBreakIteratorMode::Loose:
            return makeString(locale, "@break=loose");
        case LineBreakIteratorMode::Normal:
            return makeString(locale, "@break=normal");
        case LineBreakIteratorMode::Strict:
            return makeString(locale, "@break=strict");
        }
        ASSERT_NOT_REACHED();
        return locale;
    }

    UBreakIterator* take(const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
    {
        auto localeWithOptionalBreakKeyword = makeLocaleWithBreakKeyword(locale, mode);

        UBreakIterator* iterator = nullptr;
        for (size_t i = 0; i < m_pool.size(); ++i) {
            if (m_pool[i].first == localeWithOptionalBreakKeyword) {
                iterator = m_pool[i].second;
                m_pool.remove(i);
                break;
            }
        }

        if (!iterator) {
            iterator = openLineBreakIterator(localeWithOptionalBreakKeyword, mode, isCJK);
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

    Vector<std::pair<AtomicString, UBreakIterator*>, capacity> m_pool;
    HashMap<UBreakIterator*, AtomicString> m_vendedIterators;

    friend WTF::ThreadSpecific<LineBreakIteratorPool>::operator LineBreakIteratorPool*();
};

}

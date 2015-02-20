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

#ifndef LineBreakIteratorPoolICU_h
#define LineBreakIteratorPoolICU_h

#include "TextBreakIterator.h"
#include "TextBreakIteratorInternalICU.h"
#include <unicode/ubrk.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class LineBreakIteratorPool {
    WTF_MAKE_NONCOPYABLE(LineBreakIteratorPool);
public:
    static LineBreakIteratorPool& sharedPool()
    {
        static WTF::ThreadSpecific<LineBreakIteratorPool>* pool = new WTF::ThreadSpecific<LineBreakIteratorPool>;
        return **pool;
    }

    static PassOwnPtr<LineBreakIteratorPool> create() { return adoptPtr(new LineBreakIteratorPool); }

    static String makeLocaleWithBreakKeyword(const AtomicString& locale, LineBreakIteratorMode mode)
    {
        StringBuilder localeWithKeyword;
        localeWithKeyword.append(locale);
        localeWithKeyword.appendLiteral("@break=");
        switch (mode) {
        case LineBreakIteratorModeUAX14:
            ASSERT_NOT_REACHED();
            break;
        case LineBreakIteratorModeUAX14Loose:
            localeWithKeyword.appendLiteral("loose");
            break;
        case LineBreakIteratorModeUAX14Normal:
            localeWithKeyword.appendLiteral("normal");
            break;
        case LineBreakIteratorModeUAX14Strict:
            localeWithKeyword.appendLiteral("strict");
            break;
        }
        return localeWithKeyword.toString();
    }

    TextBreakIterator* take(const AtomicString& locale, LineBreakIteratorMode mode, bool isCJK)
    {
        AtomicString localeWithOptionalBreakKeyword;
        if (mode == LineBreakIteratorModeUAX14)
            localeWithOptionalBreakKeyword = locale;
        else
            localeWithOptionalBreakKeyword = makeLocaleWithBreakKeyword(locale, mode);

        TextBreakIterator* iterator = 0;
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
                return 0;
        }

        ASSERT(!m_vendedIterators.contains(iterator));
        m_vendedIterators.set(iterator, localeWithOptionalBreakKeyword);
        return iterator;
    }

    void put(TextBreakIterator* iterator)
    {
        ASSERT_ARG(iterator, m_vendedIterators.contains(iterator));

        if (m_pool.size() == capacity) {
            closeLineBreakIterator(m_pool[0].second);
            m_pool.remove(0);
        }

        m_pool.append(Entry(m_vendedIterators.take(iterator), iterator));
    }

private:
    LineBreakIteratorPool() { }

    static const size_t capacity = 4;

    typedef std::pair<AtomicString, TextBreakIterator*> Entry;
    typedef Vector<Entry, capacity> Pool;
    Pool m_pool;
    HashMap<TextBreakIterator*, AtomicString> m_vendedIterators;

    friend WTF::ThreadSpecific<LineBreakIteratorPool>::operator LineBreakIteratorPool*();
};

}

#endif

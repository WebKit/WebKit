/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/text/StringView.h>

namespace WTF {

// Note: The returned iterator is good only until you get another iterator, with the exception of acquireLineBreakIterator.

enum class LineBreakIteratorMode { Default, Loose, Normal, Strict };

// This is similar to character break iterator in most cases, but is subject to
// platform UI conventions. One notable example where this can be different
// from character break iterator is Thai prepend characters, see bug 24342.
// Use this for insertion point and selection manipulations.
WTF_EXPORT_PRIVATE UBreakIterator* cursorMovementIterator(StringView);

WTF_EXPORT_PRIVATE UBreakIterator* wordBreakIterator(StringView);
WTF_EXPORT_PRIVATE UBreakIterator* sentenceBreakIterator(StringView);

WTF_EXPORT_PRIVATE UBreakIterator* acquireLineBreakIterator(StringView, const AtomicString& locale, const UChar* priorContext, unsigned priorContextLength, LineBreakIteratorMode);
WTF_EXPORT_PRIVATE void releaseLineBreakIterator(UBreakIterator*);
UBreakIterator* openLineBreakIterator(const AtomicString& locale);
void closeLineBreakIterator(UBreakIterator*&);

WTF_EXPORT_PRIVATE bool isWordTextBreak(UBreakIterator*);

class LazyLineBreakIterator {
public:
    LazyLineBreakIterator()
    {
        resetPriorContext();
    }

    explicit LazyLineBreakIterator(StringView stringView, const AtomicString& locale = AtomicString(), LineBreakIteratorMode mode = LineBreakIteratorMode::Default)
        : m_stringView(stringView)
        , m_locale(locale)
        , m_mode(mode)
    {
        resetPriorContext();
    }

    ~LazyLineBreakIterator()
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
    }

    StringView stringView() const { return m_stringView; }
    LineBreakIteratorMode mode() const { return m_mode; }

    UChar lastCharacter() const
    {
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        return m_priorContext[1];
    }

    UChar secondToLastCharacter() const
    {
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        return m_priorContext[0];
    }

    void setPriorContext(UChar last, UChar secondToLast)
    {
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        m_priorContext[0] = secondToLast;
        m_priorContext[1] = last;
    }

    void updatePriorContext(UChar last)
    {
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        m_priorContext[0] = m_priorContext[1];
        m_priorContext[1] = last;
    }

    void resetPriorContext()
    {
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        m_priorContext[0] = 0;
        m_priorContext[1] = 0;
    }

    unsigned priorContextLength() const
    {
        unsigned priorContextLength = 0;
        static_assert(WTF_ARRAY_LENGTH(m_priorContext) == 2, "UBreakIterator unexpected prior context length");
        if (m_priorContext[1]) {
            ++priorContextLength;
            if (m_priorContext[0])
                ++priorContextLength;
        }
        return priorContextLength;
    }

    // Obtain text break iterator, possibly previously cached, where this iterator is (or has been)
    // initialized to use the previously stored string as the primary breaking context and using
    // previously stored prior context if non-empty.
    UBreakIterator* get(unsigned priorContextLength)
    {
        ASSERT(priorContextLength <= priorContextCapacity);
        const UChar* priorContext = priorContextLength ? &m_priorContext[priorContextCapacity - priorContextLength] : 0;
        if (!m_iterator) {
            m_iterator = acquireLineBreakIterator(m_stringView, m_locale, priorContext, priorContextLength, m_mode);
            m_cachedPriorContext = priorContext;
            m_cachedPriorContextLength = priorContextLength;
        } else if (priorContext != m_cachedPriorContext || priorContextLength != m_cachedPriorContextLength) {
            resetStringAndReleaseIterator(m_stringView, m_locale, m_mode);
            return this->get(priorContextLength);
        }
        return m_iterator;
    }

    void resetStringAndReleaseIterator(StringView stringView, const AtomicString& locale, LineBreakIteratorMode mode)
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
        m_stringView = stringView;
        m_locale = locale;
        m_iterator = nullptr;
        m_cachedPriorContext = nullptr;
        m_mode = mode;
        m_cachedPriorContextLength = 0;
    }

private:
    static constexpr unsigned priorContextCapacity = 2;
    StringView m_stringView;
    AtomicString m_locale;
    UBreakIterator* m_iterator { nullptr };
    const UChar* m_cachedPriorContext { nullptr };
    LineBreakIteratorMode m_mode { LineBreakIteratorMode::Default };
    unsigned m_cachedPriorContextLength { 0 };
    UChar m_priorContext[priorContextCapacity];
};

// Iterates over "extended grapheme clusters", as defined in UAX #29.
// Note that platform implementations may be less sophisticated - e.g. ICU prior to
// version 4.0 only supports "legacy grapheme clusters".
// Use this for general text processing, e.g. string truncation.

class NonSharedCharacterBreakIterator {
    WTF_MAKE_NONCOPYABLE(NonSharedCharacterBreakIterator);
public:
    WTF_EXPORT_PRIVATE NonSharedCharacterBreakIterator(StringView);
    WTF_EXPORT_PRIVATE ~NonSharedCharacterBreakIterator();

    NonSharedCharacterBreakIterator(NonSharedCharacterBreakIterator&&);

    operator UBreakIterator*() const { return m_iterator; }

private:
    UBreakIterator* m_iterator;
};

// Counts the number of grapheme clusters. A surrogate pair or a sequence
// of a non-combining character and following combining characters is
// counted as 1 grapheme cluster.
WTF_EXPORT_PRIVATE unsigned numGraphemeClusters(StringView);

// Returns the number of characters which will be less than or equal to
// the specified grapheme cluster length.
WTF_EXPORT_PRIVATE unsigned numCharactersInGraphemeClusters(StringView, unsigned);

}

using WTF::LazyLineBreakIterator;
using WTF::LineBreakIteratorMode;
using WTF::NonSharedCharacterBreakIterator;
using WTF::isWordTextBreak;

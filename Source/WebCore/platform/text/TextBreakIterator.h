/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef TextBreakIterator_h
#define TextBreakIterator_h

#include <wtf/text/AtomicString.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class TextBreakIterator;

// Note: The returned iterator is good only until you get another iterator, with the exception of acquireLineBreakIterator.

enum LineBreakIteratorMode {
    LineBreakIteratorModeUAX14,
    LineBreakIteratorModeUAX14Loose,
    LineBreakIteratorModeUAX14Normal,
    LineBreakIteratorModeUAX14Strict,
};

// This is similar to character break iterator in most cases, but is subject to
// platform UI conventions. One notable example where this can be different
// from character break iterator is Thai prepend characters, see bug 24342.
// Use this for insertion point and selection manipulations.
TextBreakIterator* cursorMovementIterator(StringView);

TextBreakIterator* wordBreakIterator(StringView);
TextBreakIterator* sentenceBreakIterator(StringView);

TextBreakIterator* acquireLineBreakIterator(StringView, const AtomicString& locale, const UChar* priorContext, unsigned priorContextLength, LineBreakIteratorMode, bool isCJK);
void releaseLineBreakIterator(TextBreakIterator*);
TextBreakIterator* openLineBreakIterator(const AtomicString& locale, LineBreakIteratorMode, bool isCJK);
void closeLineBreakIterator(TextBreakIterator*&);

int textBreakFirst(TextBreakIterator*);
int textBreakLast(TextBreakIterator*);
int textBreakNext(TextBreakIterator*);
int textBreakPrevious(TextBreakIterator*);
int textBreakCurrent(TextBreakIterator*);
int textBreakPreceding(TextBreakIterator*, int);
int textBreakFollowing(TextBreakIterator*, int);
bool isTextBreak(TextBreakIterator*, int);
bool isWordTextBreak(TextBreakIterator*);

const int TextBreakDone = -1;

bool isCJKLocale(const AtomicString&);

class LazyLineBreakIterator {
public:
    LazyLineBreakIterator()
        : m_iterator(nullptr)
        , m_cachedPriorContext(nullptr)
        , m_mode(LineBreakIteratorModeUAX14)
        , m_cachedPriorContextLength(0)
        , m_isCJK(false)
    {
        resetPriorContext();
    }

    LazyLineBreakIterator(String string, const AtomicString& locale = AtomicString(), LineBreakIteratorMode mode = LineBreakIteratorModeUAX14)
        : m_string(string)
        , m_locale(locale)
        , m_iterator(nullptr)
        , m_cachedPriorContext(nullptr)
        , m_mode(mode)
        , m_cachedPriorContextLength(0)
    {
        resetPriorContext();
        m_isCJK = isCJKLocale(locale);
    }

    ~LazyLineBreakIterator()
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
    }

    String string() const { return m_string; }
    bool isLooseCJKMode() const { return m_isCJK && m_mode == LineBreakIteratorModeUAX14Loose; }

    UChar lastCharacter() const
    {
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
        return m_priorContext[1];
    }

    UChar secondToLastCharacter() const
    {
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
        return m_priorContext[0];
    }

    void setPriorContext(UChar last, UChar secondToLast)
    {
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
        m_priorContext[0] = secondToLast;
        m_priorContext[1] = last;
    }

    void updatePriorContext(UChar last)
    {
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
        m_priorContext[0] = m_priorContext[1];
        m_priorContext[1] = last;
    }

    void resetPriorContext()
    {
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
        m_priorContext[0] = 0;
        m_priorContext[1] = 0;
    }

    unsigned priorContextLength() const
    {
        unsigned priorContextLength = 0;
        COMPILE_ASSERT(WTF_ARRAY_LENGTH(m_priorContext) == 2, TextBreakIterator_unexpected_prior_context_length);
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
    TextBreakIterator* get(unsigned priorContextLength)
    {
        ASSERT(priorContextLength <= priorContextCapacity);
        const UChar* priorContext = priorContextLength ? &m_priorContext[priorContextCapacity - priorContextLength] : 0;
        if (!m_iterator) {
            m_iterator = acquireLineBreakIterator(m_string, m_locale, priorContext, priorContextLength, m_mode, m_isCJK);
            m_cachedPriorContext = priorContext;
            m_cachedPriorContextLength = priorContextLength;
        } else if (priorContext != m_cachedPriorContext || priorContextLength != m_cachedPriorContextLength) {
            resetStringAndReleaseIterator(m_string, m_locale, m_mode);
            return this->get(priorContextLength);
        }
        return m_iterator;
    }

    void resetStringAndReleaseIterator(String string, const AtomicString& locale, LineBreakIteratorMode mode)
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
        m_string = string;
        m_locale = locale;
        m_iterator = nullptr;
        m_cachedPriorContext = nullptr;
        m_mode = mode;
        m_isCJK = isCJKLocale(locale);
        m_cachedPriorContextLength = 0;
    }

private:
    static const unsigned priorContextCapacity = 2;
    String m_string;
    AtomicString m_locale;
    TextBreakIterator* m_iterator;
    const UChar* m_cachedPriorContext;
    LineBreakIteratorMode m_mode;
    unsigned m_cachedPriorContextLength;
    UChar m_priorContext[priorContextCapacity];
    bool m_isCJK;
};

// Iterates over "extended grapheme clusters", as defined in UAX #29.
// Note that platform implementations may be less sophisticated - e.g. ICU prior to
// version 4.0 only supports "legacy grapheme clusters".
// Use this for general text processing, e.g. string truncation.

class NonSharedCharacterBreakIterator {
    WTF_MAKE_NONCOPYABLE(NonSharedCharacterBreakIterator);
public:
    NonSharedCharacterBreakIterator(StringView);
    ~NonSharedCharacterBreakIterator();

    operator TextBreakIterator*() const { return m_iterator; }

private:
    TextBreakIterator* m_iterator;
};

// Counts the number of grapheme clusters. A surrogate pair or a sequence
// of a non-combining character and following combining characters is
// counted as 1 grapheme cluster.
unsigned numGraphemeClusters(const String&);
// Returns the number of characters which will be less than or equal to
// the specified grapheme cluster length.
unsigned numCharactersInGraphemeClusters(const String&, unsigned);

}

#endif

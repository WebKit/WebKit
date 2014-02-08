/*
 * Copyright (C) 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#ifndef TextCheckingHelper_h
#define TextCheckingHelper_h

#include "EditorClient.h"
#include "ExceptionCode.h"
#include "TextChecking.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class Range;
class Position;
struct TextCheckingResult;

class TextCheckingParagraph {
public:
    explicit TextCheckingParagraph(PassRefPtr<Range> checkingRange);
    TextCheckingParagraph(PassRefPtr<Range> checkingRange, PassRefPtr<Range> paragraphRange);
    ~TextCheckingParagraph();

    int rangeLength() const;
    PassRefPtr<Range> subrange(int characterOffset, int characterCount) const;
    int offsetTo(const Position&, ExceptionCode&) const;
    void expandRangeToNextEnd();

    // FIXME: Consider changing this to return a StringView.
    const String& text() const;

    // FIXME: Consider removing these and just having the caller use text() directly.
    int textLength() const { return text().length(); }
    String textSubstring(unsigned pos, unsigned len = UINT_MAX) const { return text().substring(pos, len); }
    UChar textCharAt(int index) const { return text()[static_cast<unsigned>(index)]; }
    bool isTextEmpty() const { return text().isEmpty(); }

    bool isEmpty() const;
    bool isRangeEmpty() const { return checkingStart() >= checkingEnd(); }

    int checkingStart() const;
    int checkingEnd() const;
    int checkingLength() const;
    String checkingSubstring() const { return textSubstring(checkingStart(), checkingLength()); }

    bool checkingRangeMatches(int location, int length) const { return location == checkingStart() && length == checkingLength(); }
    bool isCheckingRangeCoveredBy(int location, int length) const { return location <= checkingStart() && location + length >= checkingStart() + checkingLength(); }
    bool checkingRangeCovers(int location, int length) const { return location < checkingEnd() && location + length > checkingStart(); }
    PassRefPtr<Range> paragraphRange() const;

private:
    void invalidateParagraphRangeValues();
    PassRefPtr<Range> checkingRange() const { return m_checkingRange; }
    PassRefPtr<Range> offsetAsRange() const;

    RefPtr<Range> m_checkingRange;
    mutable RefPtr<Range> m_paragraphRange;
    mutable RefPtr<Range> m_offsetAsRange;
    mutable String m_text;
    mutable int m_checkingStart;
    mutable int m_checkingEnd;
    mutable int m_checkingLength;
};

class TextCheckingHelper {
    WTF_MAKE_NONCOPYABLE(TextCheckingHelper);
public:
    TextCheckingHelper(EditorClient*, PassRefPtr<Range>);
    ~TextCheckingHelper();

    String findFirstMisspelling(int& firstMisspellingOffset, bool markAll, RefPtr<Range>& firstMisspellingRange);
    String findFirstMisspellingOrBadGrammar(bool checkGrammar, bool& outIsSpelling, int& outFirstFoundOffset, GrammarDetail& outGrammarDetail);
    void markAllMisspellings(RefPtr<Range>& firstMisspellingRange);
#if USE(GRAMMAR_CHECKING)
    String findFirstBadGrammar(GrammarDetail& outGrammarDetail, int& outGrammarPhraseOffset, bool markAll) const;
    void markAllBadGrammar();
    bool isUngrammatical() const;
#endif
    Vector<String> guessesForMisspelledOrUngrammaticalRange(bool checkGrammar, bool& misspelled, bool& ungrammatical) const;

private:
    EditorClient* m_client;
    RefPtr<Range> m_range;

    bool unifiedTextCheckerEnabled() const;
#if USE(GRAMMAR_CHECKING)
    int findFirstGrammarDetail(const Vector<GrammarDetail>&, int badGrammarPhraseLocation, int startOffset, int endOffset, bool markAll) const;
#endif
};

void checkTextOfParagraph(TextCheckerClient&, StringView, TextCheckingTypeMask, Vector<TextCheckingResult>&);

bool unifiedTextCheckerEnabled(const Frame*);

} // namespace WebCore

#endif // TextCheckingHelper_h

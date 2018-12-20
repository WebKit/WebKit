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

#pragma once

#include "EditorClient.h"
#include "ExceptionOr.h"
#include "TextChecking.h"

namespace WebCore {

class Position;
class Range;

struct TextCheckingResult;

class TextCheckingParagraph {
public:
    explicit TextCheckingParagraph(Ref<Range>&& checkingAndAutomaticReplacementRange);
    explicit TextCheckingParagraph(Ref<Range>&& checkingRange, Ref<Range>&& automaticReplacementRange, RefPtr<Range>&& paragraphRange);

    int rangeLength() const;
    Ref<Range> subrange(int characterOffset, int characterCount) const;
    ExceptionOr<int> offsetTo(const Position&) const;
    void expandRangeToNextEnd();

    // FIXME: Consider changing this to return a StringView.
    const String& text() const;

    // FIXME: Consider removing these and just having the caller use text() directly.
    int textLength() const { return text().length(); }
    String textSubstring(unsigned pos, unsigned len = UINT_MAX) const { return text().substring(pos, len); }
    UChar textCharAt(int index) const { return text()[static_cast<unsigned>(index)]; }

    bool isEmpty() const;

    int checkingStart() const;
    int checkingEnd() const;
    int checkingLength() const;
    String checkingSubstring() const { return textSubstring(checkingStart(), checkingLength()); }

    // Determines the range in which we allow automatic text replacement. If an automatic replacement range is not passed to the
    // text checking paragraph, this defaults to the spell checking range.
    int automaticReplacementStart() const;
    int automaticReplacementLength() const;

    bool checkingRangeMatches(int location, int length) const { return location == checkingStart() && length == checkingLength(); }
    bool isCheckingRangeCoveredBy(int location, int length) const { return location <= checkingStart() && location + length >= checkingStart() + checkingLength(); }
    bool checkingRangeCovers(int location, int length) const { return location < checkingEnd() && location + length > checkingStart(); }
    Range& paragraphRange() const;

private:
    void invalidateParagraphRangeValues();
    Range& offsetAsRange() const;

    Ref<Range> m_checkingRange;
    Ref<Range> m_automaticReplacementRange;
    mutable RefPtr<Range> m_paragraphRange;
    mutable RefPtr<Range> m_offsetAsRange;
    mutable String m_text;
    mutable Optional<int> m_checkingStart;
    mutable Optional<int> m_checkingEnd;
    mutable Optional<int> m_checkingLength;
    mutable Optional<int> m_automaticReplacementStart;
    mutable Optional<int> m_automaticReplacementLength;
};

class TextCheckingHelper {
    WTF_MAKE_NONCOPYABLE(TextCheckingHelper);
public:
    TextCheckingHelper(EditorClient&, Range&);
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
    EditorClient& m_client;
    Ref<Range> m_range;

    bool unifiedTextCheckerEnabled() const;
#if USE(GRAMMAR_CHECKING)
    int findFirstGrammarDetail(const Vector<GrammarDetail>&, int badGrammarPhraseLocation, int startOffset, int endOffset, bool markAll) const;
#endif
};

void checkTextOfParagraph(TextCheckerClient&, StringView, OptionSet<TextCheckingType>, Vector<TextCheckingResult>&, const VisibleSelection& currentSelection);

bool unifiedTextCheckerEnabled(const Frame*);

} // namespace WebCore

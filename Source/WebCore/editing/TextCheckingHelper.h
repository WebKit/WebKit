/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "SimpleRange.h"
#include "TextChecking.h"

namespace WebCore {

class Position;
struct TextCheckingResult;

class TextCheckingParagraph {
public:
    explicit TextCheckingParagraph(Ref<Range>&& checkingAndAutomaticReplacementRange);
    explicit TextCheckingParagraph(Ref<Range>&& checkingRange, Ref<Range>&& automaticReplacementRange, RefPtr<Range>&& paragraphRange);

    uint64_t rangeLength() const;
    Ref<Range> subrange(CharacterRange) const;
    ExceptionOr<uint64_t> offsetTo(const Position&) const;
    void expandRangeToNextEnd();

    StringView text() const;

    bool isEmpty() const;

    uint64_t checkingStart() const;
    uint64_t checkingEnd() const;
    uint64_t checkingLength() const;
    StringView checkingSubstring() const { return text().substring(checkingStart(), checkingLength()); }

    // Determines the range in which we allow automatic text replacement. If an automatic replacement range is not passed to the
    // text checking paragraph, this defaults to the spell checking range.
    uint64_t automaticReplacementStart() const;
    uint64_t automaticReplacementLength() const;

    bool checkingRangeMatches(CharacterRange range) const { return range.location == checkingStart() && range.length == checkingLength(); }
    bool isCheckingRangeCoveredBy(CharacterRange range) const { return range.location <= checkingStart() && range.location + range.length >= checkingStart() + checkingLength(); }
    bool checkingRangeCovers(CharacterRange range) const { return range.location < checkingEnd() && range.location + range.length > checkingStart(); }
    Range& paragraphRange() const;

private:
    void invalidateParagraphRangeValues();
    Range& offsetAsRange() const;

    Ref<Range> m_checkingRange;
    Ref<Range> m_automaticReplacementRange;
    mutable RefPtr<Range> m_paragraphRange;
    mutable RefPtr<Range> m_offsetAsRange;
    mutable String m_text;
    mutable Optional<uint64_t> m_checkingStart;
    mutable Optional<uint64_t> m_checkingLength;
    mutable Optional<uint64_t> m_automaticReplacementStart;
    mutable Optional<uint64_t> m_automaticReplacementLength;
};

class TextCheckingHelper {
    WTF_MAKE_NONCOPYABLE(TextCheckingHelper);
public:
    TextCheckingHelper(EditorClient&, const SimpleRange&);
    ~TextCheckingHelper();

    String findFirstMisspelling(uint64_t& firstMisspellingOffset, bool markAll, RefPtr<Range>& firstMisspellingRange);
    String findFirstMisspellingOrBadGrammar(bool checkGrammar, bool& outIsSpelling, uint64_t& outFirstFoundOffset, GrammarDetail& outGrammarDetail);
    void markAllMisspellings(RefPtr<Range>& firstMisspellingRange);
    String findFirstBadGrammar(GrammarDetail& outGrammarDetail, uint64_t& outGrammarPhraseOffset, bool markAll) const;
    void markAllBadGrammar();
    bool isUngrammatical() const;
    Vector<String> guessesForMisspelledOrUngrammaticalRange(bool checkGrammar, bool& misspelled, bool& ungrammatical) const;

private:
    EditorClient& m_client;
    SimpleRange m_range;

    bool unifiedTextCheckerEnabled() const;
    int findFirstGrammarDetail(const Vector<GrammarDetail>&, uint64_t badGrammarPhraseLocation, uint64_t startOffset, uint64_t endOffset, bool markAll) const;
};

void checkTextOfParagraph(TextCheckerClient&, StringView, OptionSet<TextCheckingType>, Vector<TextCheckingResult>&, const VisibleSelection& currentSelection);

bool unifiedTextCheckerEnabled(const Frame*);
bool platformDrivenTextCheckerEnabled();

} // namespace WebCore

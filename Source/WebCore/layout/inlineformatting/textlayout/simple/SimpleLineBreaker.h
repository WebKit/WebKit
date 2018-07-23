/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "RenderStyleConstants.h"
#include "Runs.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class FontCascade;
class RenderStyle;

namespace Layout {

class TextContentProvider;
struct TextRunSplitPair;

class SimpleLineBreaker {
    WTF_MAKE_ISO_ALLOCATED(SimpleLineBreaker);
public:
    struct LineConstraint {
        // verticalPosition sets the bottom edge for this constraint. Last entry in the list
        // should always be std::nullopt indicating that left/right pair is set for the rest of the lines
        // (Normally there's only one entry with the left/right block edges).
        std::optional<float> verticalPosition;
        float left { 0 };
        float right { 0 };
    };
    using LineConstraintList = Vector<LineConstraint>;
    SimpleLineBreaker(const Vector<TextRun>&, const TextContentProvider&, LineConstraintList&&, const RenderStyle&);

    void setLineHeight(float lineHeight) { m_lineHeight = lineHeight; }

    Vector<LayoutRun> runs();

private:
    struct Style {
        explicit Style(const RenderStyle&);

        const FontCascade& font;
        bool wrapLines { false };
        bool breakAnyWordOnOverflow { false };
        bool breakFirstWordOnOverflow { false };
        bool collapseWhitespace { false };
        bool preWrap { false };
        bool preserveNewline { false };
        TextAlignMode textAlign { TextAlignMode::Left };
        bool shouldHyphenate;
        float hyphenStringWidth;
        ItemPosition hyphenLimitBefore;
        ItemPosition hyphenLimitAfter;
        AtomicString locale;
        std::optional<unsigned> hyphenLimitLines;
    };

    class TextRunList {
    public:
        TextRunList(const Vector<TextRun>& textRuns);

        std::optional<TextRun> current() const;
        TextRunList& operator++();

        void overrideCurrent(TextRun textRun) { m_overrideTextRun = textRun; }
        bool isCurrentOverridden() const { return m_overrideTextRun.has_value(); }

    private:
        const Vector<TextRun>& m_textRuns;
        unsigned m_currentIndex { 0 };
        std::optional<TextRun> m_overrideTextRun;
    };

    class Line {
    public:
        Line(Vector<LayoutRun>& layoutRuns);

        void append(const TextRun&);
        float availableWidth() const { return m_availableWidth - m_runsWidth; }
        bool hasContent() const { return m_runsWidth; }

        void reset();
        bool hasTrailingWhitespace() const { return m_trailingWhitespaceWidth; }
        bool isWhitespaceOnly() const { return m_runsWidth == m_trailingWhitespaceWidth; }
        void collapseTrailingWhitespace();

        void setAvailableWidth(float availableWidth) { m_availableWidth = availableWidth; }
        void setLeft(float left) { m_left = left; }
        void setTextAlign(TextAlignMode);
        void setCollapseWhitespace(bool collapseWhitespace) { m_style.collapseWhitespace = collapseWhitespace; }

        void closeLastRun();
        void adjustRunsForTextAlign(bool lastLine);

    private:
        struct Style {
            bool collapseWhitespace { false };
            TextAlignMode textAlign { TextAlignMode::Left };
        };

        float adjustedLeftForTextAlign(TextAlignMode) const;
        void justifyRuns();
        void collectExpansionOpportunities(const TextRun&, bool textRunCreatesNewLayoutRun);

        Vector<LayoutRun>& m_layoutRuns;
        Style m_style;

        float m_runsWidth { 0 };
        float m_availableWidth { 0 };
        float m_left { 0 };
        float m_trailingWhitespaceWidth { 0 };
        unsigned m_firstRunIndex { 0 };
        std::optional<TextRun> m_lastTextRun;
        std::optional<TextRun> m_lastNonWhitespaceTextRun;
        bool m_collectExpansionOpportunities { false };
        struct ExpansionOpportunity {
            unsigned count { 0 };
            ExpansionBehavior behavior { DefaultExpansion };
        };
        Vector<ExpansionOpportunity> m_expansionOpportunityList;
        std::optional<ExpansionOpportunity> m_lastNonWhitespaceExpansionOppportunity;
    };

    void handleLineStart();
    void handleLineEnd();
    bool wrapContentOnOverflow() const { return m_style.wrapLines || m_style.breakFirstWordOnOverflow || m_style.breakAnyWordOnOverflow; }
    void collectRuns();
    void createRunsForLine();
    void handleOverflownRun();
    void collapseLeadingWhitespace();
    void collapseTrailingWhitespace();
    bool splitTextRun(const TextRun&);
    TextRunSplitPair split(const TextRun&, float leftSideMaximumWidth) const;
    std::optional<ContentPosition> hyphenPositionBefore(const TextRun&, ContentPosition before) const;
    std::optional<ContentPosition> adjustSplitPositionWithHyphenation(const TextRun&, ContentPosition splitPosition, float leftSideWidth) const;
    LineConstraint lineConstraint(float verticalPosition);
    float verticalPosition() const { return m_numberOfLines * m_lineHeight; }

    const TextContentProvider& m_contentProvider;
    const Style m_style;

    TextRunList m_textRunList;

    Vector<LayoutRun> m_layoutRuns;
    Line m_currentLine;

    LineConstraintList m_lineConstraintList;
    ConstVectorIterator<LineConstraint> m_lineConstraintIterator;

    unsigned m_numberOfLines { 0 };
    bool m_previousLineHasNonForcedContent { false };
    float m_lineHeight { 0 };
    bool m_hyphenationIsDisabled { false };
    unsigned m_numberOfPrecedingLinesWithHyphen { 0 };
};

inline std::optional<TextRun> SimpleLineBreaker::TextRunList::current() const
{
    if (m_overrideTextRun)
        return m_overrideTextRun;

    if (m_currentIndex < m_textRuns.size())
        return m_textRuns[m_currentIndex];

    return { };
}

inline SimpleLineBreaker::TextRunList& SimpleLineBreaker::TextRunList::operator++()
{
    m_overrideTextRun = { };
    ++m_currentIndex;
    return *this;
}

}
}
#endif

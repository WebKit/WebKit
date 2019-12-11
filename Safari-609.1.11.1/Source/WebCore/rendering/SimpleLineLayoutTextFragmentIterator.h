/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "BreakLines.h"
#include "RenderLineBreak.h"
#include "SimpleLineLayoutFlowContents.h"

namespace WebCore {

class RenderBlockFlow;
class RenderStyle;

namespace SimpleLineLayout {

class TextFragmentIterator  {
public:
    TextFragmentIterator(const RenderBlockFlow&);
    class TextFragment {
    public:
        enum Type { Invalid, ContentEnd, SoftLineBreak, HardLineBreak, Whitespace, NonWhitespace };
        TextFragment() = default;
        TextFragment(unsigned start, unsigned end, float width, Type type, bool isLastInRenderer = false, bool overlapsToNextRenderer = false, bool isCollapsed = false, bool isCollapsible = false)
            : m_start(start)
            , m_end(end)
            , m_width(width)
            , m_type(type)
            , m_isLastInRenderer(isLastInRenderer)
            , m_overlapsToNextRenderer(overlapsToNextRenderer)
            , m_isCollapsed(isCollapsed)
            , m_isCollapsible(isCollapsible)
        {
        }

        bool isValid() const { return m_type != Invalid; }
        unsigned start() const { return m_start; }
        unsigned end() const { return m_end; }
        unsigned length() const { ASSERT(m_end >= m_start); return m_end - m_start; }
        float width() const { return m_width; }
        Type type() const { return m_type; }
        bool overlapsToNextRenderer() const { return m_overlapsToNextRenderer; }
        bool isLastInRenderer() const { return m_isLastInRenderer; }
        bool isLineBreak() const { return m_type == SoftLineBreak || m_type == HardLineBreak; }
        bool isCollapsed() const { return m_isCollapsed; }
        bool isCollapsible() const { return m_isCollapsible; }
        bool hasHyphen() const { return m_hasHyphen; }

        bool isEmpty() const { return start() == end() && !isLineBreak(); }
        TextFragment split(unsigned splitPosition, float leftSideWidth, float rightSideWidth);
        TextFragment splitWithHyphen(unsigned hyphenPosition, float hyphenStringWidth, float leftSideWidth, float rightSideWidth);
        bool operator==(const TextFragment& other) const
        {
            return m_start == other.m_start
                && m_end == other.m_end
                && m_width == other.m_width
                && m_type == other.m_type
                && m_isLastInRenderer == other.m_isLastInRenderer
                && m_overlapsToNextRenderer == other.m_overlapsToNextRenderer
                && m_isCollapsed == other.m_isCollapsed
                && m_isCollapsible == other.m_isCollapsible
                && m_hasHyphen == other.m_hasHyphen;
        }

    private:
        unsigned m_start { 0 };
        unsigned m_end { 0 };
        float m_width { 0 };
        Type m_type { Invalid };
        bool m_isLastInRenderer { false };
        bool m_overlapsToNextRenderer { false };
        bool m_isCollapsed { false };
        bool m_isCollapsible { false };
        bool m_hasHyphen { false };
    };
    TextFragment nextTextFragment(float xPosition = 0);
    void revertToEndOfFragment(const TextFragment&);

    // FIXME: These functions below should be decoupled from the text iterator.
    float textWidth(unsigned startPosition, unsigned endPosition, float xPosition) const;
    Optional<unsigned> lastHyphenPosition(const TextFragmentIterator::TextFragment& run, unsigned beforeIndex) const;

    struct Style {
        explicit Style(const RenderStyle&);

        const FontCascade& font;
        TextAlignMode textAlign;
        bool hasKerningOrLigatures;
        bool collapseWhitespace;
        bool preserveNewline;
        bool wrapLines;
        bool breakSpaces;
        bool breakAnyWordOnOverflow;
        bool breakWordOnOverflow;
        bool breakFirstWordOnOverflow;
        bool breakNBSP;
        bool keepAllWordsForCJK;
        float wordSpacing;
        TabSize tabWidth;
        bool shouldHyphenate;
        float hyphenStringWidth;
        unsigned hyphenLimitBefore;
        unsigned hyphenLimitAfter;
        AtomString locale;
        Optional<unsigned> hyphenLimitLines;
    };
    const Style& style() const { return m_style; }

private:
    TextFragment findNextTextFragment(float xPosition);
    enum PositionType { Breakable, NonWhitespace };
    unsigned skipToNextPosition(PositionType, unsigned startPosition, float& width, float xPosition, bool& overlappingFragment);
    bool isSoftLineBreak(unsigned position) const;
    bool isHardLineBreak(const FlowContents::Iterator& segment) const;
    unsigned nextBreakablePosition(const FlowContents::Segment&, unsigned startPosition);
    unsigned nextNonWhitespacePosition(const FlowContents::Segment&, unsigned startPosition);

    FlowContents m_flowContents;
    FlowContents::Iterator m_currentSegment;
    LazyLineBreakIterator m_lineBreakIterator;
    const Style m_style;
    unsigned m_position { 0 };
    bool m_atEndOfSegment { false };
};

inline TextFragmentIterator::TextFragment TextFragmentIterator::TextFragment::split(unsigned splitPosition, float leftSideWidth, float rightSideWidth)
{
    auto updateFragmentProperties = [] (TextFragment& fragment, unsigned start, unsigned end, float width)
    {
        fragment.m_start = start;
        fragment.m_end = end;
        fragment.m_width = width;
        if (fragment.start() + 1 > fragment.end())
            return;
        fragment.m_isCollapsed = false;
    };

    TextFragment rightSide(*this);
    updateFragmentProperties(*this, start(), splitPosition, leftSideWidth);
    updateFragmentProperties(rightSide, splitPosition, rightSide.end(), rightSideWidth);
    return rightSide;
}

inline TextFragmentIterator::TextFragment TextFragmentIterator::TextFragment::splitWithHyphen(unsigned hyphenPosition, float hyphenStringWidth,
    float leftSideWidth, float rightSideWidth)
{
    auto rightSide = split(hyphenPosition, leftSideWidth, rightSideWidth);
    m_hasHyphen = true;
    m_width += hyphenStringWidth;
    return rightSide;
}

inline bool TextFragmentIterator::isSoftLineBreak(unsigned position) const
{
    const auto& segment = *m_currentSegment;
    ASSERT(segment.start <= position && position < segment.end);
    return m_style.preserveNewline && segment.text[position - segment.start] == '\n';
}

inline bool TextFragmentIterator::isHardLineBreak(const FlowContents::Iterator& segment) const
{
    ASSERT(segment->start != segment->end || (segment->start == segment->end && is<RenderLineBreak>(segment->renderer)));
    return segment->start == segment->end;
}

}
}

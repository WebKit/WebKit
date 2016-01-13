/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef SimpleLineLayoutTextFragmentIterator_h
#define SimpleLineLayoutTextFragmentIterator_h

#include "RenderLineBreak.h"
#include "RenderStyle.h"
#include "SimpleLineLayoutFlowContents.h"
#include "TextBreakIterator.h"
#include "break_lines.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class TextFragmentIterator  {
public:
    TextFragmentIterator(const RenderBlockFlow&);
    class TextFragment {
    public:
        enum Type { ContentEnd, SoftLineBreak, HardLineBreak, Whitespace, NonWhitespace };
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

        unsigned start() const { return m_start; }
        unsigned end() const { return m_end; }
        float width() const { return m_width; }
        Type type() const { return m_type; }
        bool overlapsToNextRenderer() const { return m_overlapsToNextRenderer; }
        bool isLastInRenderer() const { return m_isLastInRenderer; }
        bool isLineBreak() const { return m_type == SoftLineBreak || m_type == HardLineBreak; }
        bool isCollapsed() const { return m_isCollapsed; }
        bool isCollapsible() const { return m_isCollapsible; }

        bool isEmpty() const { return start() == end() && !isLineBreak(); }
        TextFragment split(unsigned splitPosition, const TextFragmentIterator&);
        bool operator==(const TextFragment& other) const
        {
            return m_start == other.m_start
                && m_end == other.m_end
                && m_width == other.m_width
                && m_type == other.m_type
                && m_isLastInRenderer == other.m_isLastInRenderer
                && m_overlapsToNextRenderer == other.m_overlapsToNextRenderer
                && m_isCollapsed == other.m_isCollapsed
                && m_isCollapsible == other.m_isCollapsible;
        }

    private:
        unsigned m_start { 0 };
        unsigned m_end { 0 };
        float m_width { 0 };
        Type m_type { NonWhitespace };
        bool m_isLastInRenderer { false };
        bool m_overlapsToNextRenderer { false };
        bool m_isCollapsed { false };
        bool m_isCollapsible { false };
    };
    TextFragment nextTextFragment(float xPosition = 0);
    void revertToEndOfFragment(const TextFragment&);
    float textWidth(unsigned startPosition, unsigned endPosition, float xPosition) const;

    struct Style {
        explicit Style(const RenderStyle&);

        const FontCascade& font;
        ETextAlign textAlign;
        bool collapseWhitespace;
        bool preserveNewline;
        bool wrapLines;
        bool breakAnyWordOnOverflow;
        bool breakFirstWordOnOverflow;
        float spaceWidth;
        float wordSpacing;
        unsigned tabWidth;
        AtomicString locale;
    };
    const Style& style() const { return m_style; }

private:
    TextFragment findNextTextFragment(float xPosition);
    enum PositionType { Breakable, NonWhitespace };
    unsigned skipToNextPosition(PositionType, unsigned startPosition, float& width, float xPosition, bool& overlappingFragment);
    bool isSoftLineBreak(unsigned position) const;
    bool isHardLineBreak(const FlowContents::Iterator& segment) const;
    template <typename CharacterType> unsigned nextBreakablePosition(const FlowContents::Segment&, unsigned startPosition);
    template <typename CharacterType> unsigned nextNonWhitespacePosition(const FlowContents::Segment&, unsigned startPosition);
    template <typename CharacterType> float runWidth(const FlowContents::Segment&, unsigned startPosition, unsigned endPosition, float xPosition) const;

    FlowContents m_flowContents;
    FlowContents::Iterator m_currentSegment;
    LazyLineBreakIterator m_lineBreakIterator;
    const Style m_style;
    unsigned m_position { 0 };
    bool m_atEndOfSegment { false };
};

inline TextFragmentIterator::TextFragment TextFragmentIterator::TextFragment::split(unsigned splitPosition, const TextFragmentIterator& textFragmentIterator)
{
    auto updateFragmentProperties = [&textFragmentIterator] (TextFragment& fragment)
    {
        fragment.m_width = 0;
        if (fragment.start() != fragment.end())
            fragment.m_width = textFragmentIterator.textWidth(fragment.start(), fragment.end(), 0);
        if (fragment.start() + 1 > fragment.end())
            return;
        fragment.m_isCollapsed = false;
    };

    TextFragment newFragment(*this);
    m_end = splitPosition;
    updateFragmentProperties(*this);

    newFragment.m_start = splitPosition;
    updateFragmentProperties(newFragment);
    return newFragment;
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

#endif

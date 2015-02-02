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

#ifndef SimpleLineLayoutFlowContentsIterator_h
#define SimpleLineLayoutFlowContentsIterator_h

#include "RenderStyle.h"
#include "SimpleLineLayoutFlowContents.h"
#include "TextBreakIterator.h"
#include "break_lines.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class FlowContentsIterator  {
public:
    FlowContentsIterator(const RenderBlockFlow&);
    class TextFragment {
    public:
        enum Type { ContentEnd, LineBreak, Whitespace, NonWhitespace };
        TextFragment() = default;
        TextFragment(unsigned start, unsigned end, float width, Type type, bool isCollapsed = false, bool isBreakable = false)
            : m_start(start)
            , m_end(end)
            , m_type(type)
            , m_width(width)
            , m_isCollapsed(isCollapsed)
            , m_isBreakable(isBreakable)
        {
        }

        unsigned start() const { return m_start; }
        unsigned end() const { return m_end; }
        float width() const { return m_width; }
        Type type() const { return m_type; }
        bool isCollapsed() const { return m_isCollapsed; }
        bool isBreakable() const { return m_isBreakable; }

        bool isEmpty() const { return start() == end(); }
        TextFragment split(unsigned splitPosition, const FlowContentsIterator&);

    private:
        unsigned m_start { 0 };
        unsigned m_end { 0 };
        Type m_type { NonWhitespace };
        float m_width { 0 };
        bool m_isCollapsed { false };
        bool m_isBreakable { false };
    };
    TextFragment nextTextFragment(float xPosition = 0);
    float textWidth(unsigned from, unsigned to, float xPosition) const;

    struct Style {
        explicit Style(const RenderStyle&);

        const FontCascade& font;
        ETextAlign textAlign;
        bool collapseWhitespace;
        bool preserveNewline;
        bool wrapLines;
        bool breakWordOnOverflow;
        float spaceWidth;
        unsigned tabWidth;
        AtomicString locale;
    };
    const Style& style() const { return m_style; }
    // FIXME: remove splitRunsAtRendererBoundary()
    const FlowContents::Segment& segmentForPosition(unsigned position) const { return m_flowContents.segmentForPosition(position); };

private:
    unsigned findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const;
    unsigned findNextBreakablePosition(unsigned position) const;
    UChar characterAt(unsigned position) const;
    bool isLineBreak(unsigned position) const;
    bool isEnd(unsigned position) const;
    template <typename CharacterType> float runWidth(const String&, unsigned from, unsigned to, float xPosition) const;

    FlowContents m_flowContents;
    mutable LazyLineBreakIterator m_lineBreakIterator;
    const Style m_style;
    unsigned m_position { 0 };
};

inline FlowContentsIterator::TextFragment FlowContentsIterator::TextFragment::split(unsigned splitPosition, const FlowContentsIterator& flowContentsIterator)
{
    auto updateFragmentProperties = [&flowContentsIterator] (TextFragment& fragment)
    {
        fragment.m_width = 0;
        if (fragment.start() != fragment.end())
            fragment.m_width = flowContentsIterator.textWidth(fragment.start(), fragment.end(), 0);
        if (fragment.start() + 1 > fragment.end())
            return;
        fragment.m_isCollapsed = false;
        fragment.m_isBreakable = false;
    };

    TextFragment newFragment(*this);
    m_end = splitPosition;
    updateFragmentProperties(*this);

    newFragment.m_start = splitPosition;
    updateFragmentProperties(newFragment);
    return newFragment;
}

inline UChar FlowContentsIterator::characterAt(unsigned position) const
{
    auto& segment = m_flowContents.segmentForPosition(position);
    return segment.text[position - segment.start];
}

inline bool FlowContentsIterator::isLineBreak(unsigned position) const
{
    if (isEnd(position))
        return false;
    return m_style.preserveNewline && characterAt(position) == '\n';
}

inline bool FlowContentsIterator::isEnd(unsigned position) const
{
    return position >= m_flowContents.length();
}

}
}

#endif

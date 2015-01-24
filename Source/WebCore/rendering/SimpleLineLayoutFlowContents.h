/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef SimpleLineLayoutFlowContents_h
#define SimpleLineLayoutFlowContents_h

#include "FontCascade.h"
#include "RenderStyle.h"
#include "TextBreakIterator.h"
#include "break_lines.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class FlowContents {
public:
    FlowContents(const RenderBlockFlow&);

    struct TextFragment {
        TextFragment() = default;
        TextFragment(unsigned textStart, unsigned textEnd, float textWidth, bool isWhitespaceOnly)
            : start(textStart)
            , end(textEnd)
            , type(isWhitespaceOnly ? Whitespace : NonWhitespace)
            , width(textWidth)
        {
        }

        bool isEmpty() const
        {
            return start == end;
        }

        enum Type { LineBreak, Whitespace, NonWhitespace };
        unsigned start = 0;
        unsigned end = 0;
        Type type = NonWhitespace;
        bool isCollapsed = false;
        bool isBreakable = false;
        float width = 0;
    };
    TextFragment nextTextFragment(unsigned position, float xPosition) const;
    float textWidth(unsigned from, unsigned to, float xPosition) const;
    bool isLineBreak(unsigned position) const;
    bool isEnd(unsigned position) const;

    struct Segment {
        unsigned start;
        unsigned end;
        String text;
        const RenderText& renderer;
    };
    const Segment& segmentForPosition(unsigned) const;
    const Segment& segmentForRenderer(const RenderText&) const;

    bool hasOneSegment() const { return m_segments.size() == 1; }
    unsigned length() const { return m_segments.last().end; };

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

private:
    unsigned findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const;
    unsigned findNextBreakablePosition(unsigned position) const;
    unsigned segmentIndexForPosition(unsigned position) const;
    unsigned segmentIndexForPositionSlow(unsigned position) const;

    UChar characterAt(unsigned position) const;
    template <typename CharacterType> float runWidth(const String&, unsigned from, unsigned to, float xPosition) const;

    const Style m_style;
    const Vector<Segment, 8> m_segments;

    mutable LazyLineBreakIterator m_lineBreakIterator;
    mutable unsigned m_lastSegmentIndex;
};

inline UChar FlowContents::characterAt(unsigned position) const
{
    auto& segment = segmentForPosition(position);
    return segment.text[position - segment.start];
}

inline bool FlowContents::isLineBreak(unsigned position) const
{
    if (isEnd(position))
        return false;
    return m_style.preserveNewline && characterAt(position) == '\n';
}

inline bool FlowContents::isEnd(unsigned position) const
{
    return position >= length();
}

inline unsigned FlowContents::segmentIndexForPosition(unsigned position) const
{
    ASSERT(!isEnd(position));
    auto& lastSegment = m_segments[m_lastSegmentIndex];
    if (lastSegment.start <= position && position < lastSegment.end)
        return m_lastSegmentIndex;
    return segmentIndexForPositionSlow(position);
}

inline const FlowContents::Segment& FlowContents::segmentForPosition(unsigned position) const
{
    return m_segments[segmentIndexForPosition(position)];
}

}
}

#endif

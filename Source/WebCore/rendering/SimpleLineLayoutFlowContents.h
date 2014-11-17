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

#include "Font.h"
#include "RenderStyle.h"
#include "TextBreakIterator.h"
#include "break_lines.h"

namespace WebCore {
class RenderBlockFlow;

namespace SimpleLineLayout {

class FlowContents {
public:
    FlowContents(const RenderBlockFlow&);

    unsigned findNextBreakablePosition(unsigned position);
    unsigned findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const;

    float textWidth(unsigned from, unsigned to, float xPosition) const;

    bool isNewlineCharacter(unsigned position) const;
    bool isEndOfContent(unsigned position) const;

    class Style {
    public:
        Style(const RenderStyle& style)
            : font(style.font())
            , textAlign(style.textAlign())
            , collapseWhitespace(style.collapseWhiteSpace())
            , preserveNewline(style.preserveNewline())
            , wrapLines(style.autoWrap())
            , breakWordOnOverflow(style.overflowWrap() == BreakOverflowWrap && (wrapLines || preserveNewline))
            , spaceWidth(font.width(TextRun(&space, 1)))
            , tabWidth(collapseWhitespace ? 0 : style.tabSize())
        {
        }

        const Font& font;
        ETextAlign textAlign;
        bool collapseWhitespace;
        bool preserveNewline;
        bool wrapLines;
        bool breakWordOnOverflow;
        float spaceWidth;
        unsigned tabWidth;
    };
    const Style& style() const { return m_style; }

private:
    unsigned nextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const;
    float runWidth(unsigned from, unsigned to, float xPosition) const;

    const RenderBlockFlow& m_flow;
    Style m_style;
    LazyLineBreakIterator m_lineBreakIterator;
};

inline bool FlowContents::isNewlineCharacter(unsigned position) const
{
    ASSERT(m_lineBreakIterator.string().length() > position);
    return m_lineBreakIterator.string().at(position) == '\n';
}

inline bool FlowContents::isEndOfContent(unsigned position) const
{
    return position >= m_lineBreakIterator.string().length();
}

}
}

#endif

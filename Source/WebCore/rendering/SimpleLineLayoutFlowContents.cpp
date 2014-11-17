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

#include "config.h"
#include "SimpleLineLayoutFlowContents.h"

#include "RenderBlockFlow.h"

namespace WebCore {
namespace SimpleLineLayout {

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_flow(flow)
    , m_style(flow.style())
    , m_lineBreakIterator(downcast<RenderText>(*flow.firstChild()).text(), flow.style().locale())
{
}

unsigned FlowContents::findNextBreakablePosition(unsigned position)
{
    String string = m_lineBreakIterator.string();
    return nextBreakablePosition<LChar, false>(m_lineBreakIterator, string.characters8(), string.length(), position);
}

unsigned FlowContents::findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    String string = m_lineBreakIterator.string();
    unsigned length = string.length();
    const LChar* text = string.characters8();
    spaceCount = 0;
    for (; position < length; ++position) {
        bool isSpace = text[position] == ' ';
        if (!(isSpace || text[position] == '\t' || (!m_style.preserveNewline && text[position] == '\n')))
            return position;
        if (isSpace)
            ++spaceCount;
    }
    return length;
}

float FlowContents::textWidth(unsigned from, unsigned to, float xPosition) const
{
    String string = m_lineBreakIterator.string();
    unsigned length = string.length();
    if (m_style.font.isFixedPitch() || (!from && to == length)) {
        const RenderText& renderer = downcast<RenderText>(*m_flow.firstChild());
        return renderer.width(from, to - from, m_style.font, xPosition, nullptr, nullptr);
    }

    TextRun run(string.characters8() + from, to - from);
    run.setXPos(xPosition);
    run.setCharactersLength(length - from);
    run.setTabSize(!!m_style.tabWidth, m_style.tabWidth);
    ASSERT(run.charactersLength() >= run.length());
    return m_style.font.width(run);
}

}
}

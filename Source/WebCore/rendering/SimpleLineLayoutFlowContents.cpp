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
#include "RenderChildIterator.h"
#include "RenderText.h"

namespace WebCore {
namespace SimpleLineLayout {

FlowContents::Style::Style(const RenderStyle& style)
    : font(style.font())
    , textAlign(style.textAlign())
    , collapseWhitespace(style.collapseWhiteSpace())
    , preserveNewline(style.preserveNewline())
    , wrapLines(style.autoWrap())
    , breakWordOnOverflow(style.overflowWrap() == BreakOverflowWrap && (wrapLines || preserveNewline))
    , spaceWidth(font.width(TextRun(&space, 1)))
    , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    , locale(style.locale())
{
}

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_style(flow.style())
    , m_lineBreakIterator(downcast<RenderText>(*flow.firstChild()).text(), flow.style().locale())
    , m_lastRendererIndex(0)
{
    unsigned startPosition = 0;
    for (auto& textChild : childrenOfType<RenderText>(flow)) {
        unsigned contentLength = textChild.text()->length();
        m_textRanges.append(std::make_pair(startPosition, &textChild));
        startPosition += contentLength;
    }
    // End item.
    const RenderText* closingNullItem = nullptr;
    m_textRanges.append(std::make_pair(startPosition, closingNullItem));
}

unsigned FlowContents::findNextBreakablePosition(unsigned position) const
{
    String string = m_lineBreakIterator.string();
    unsigned breakablePosition = nextBreakablePositionNonLoosely<LChar, NBSPBehavior::IgnoreNBSP>(m_lineBreakIterator, string.characters8(), string.length(), position);
    if (appendNextRendererContentIfNeeded(breakablePosition))
        return findNextBreakablePosition(position);
    ASSERT(breakablePosition >= position);
    return breakablePosition;
}

unsigned FlowContents::findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    unsigned nonWhitespacePosition = nextNonWhitespacePosition(position, spaceCount);
    if (appendNextRendererContentIfNeeded(nonWhitespacePosition))
        return findNextNonWhitespacePosition(position, spaceCount);
    ASSERT(nonWhitespacePosition >= position);
    return nonWhitespacePosition;
}

float FlowContents::textWidth(unsigned from, unsigned to, float xPosition) const
{
    unsigned rendererStart = 0;
    const RenderText* textRenderer = renderer(from, &rendererStart);
    ASSERT(textRenderer);
    // Resolved positions are relative to the renderers.
    unsigned absoluteStart = from - rendererStart;
    unsigned absoluteEnd = to - rendererStart;
    if ((m_style.font.isFixedPitch() && textRenderer == renderer(to)) || (!absoluteStart && absoluteEnd == textRenderer->text()->length()))
        return textRenderer->width(absoluteStart, to - from, m_style.font, xPosition, nullptr, nullptr);

    // We need to split up the text and measure renderers individually due to ligature.
    float textWidth = 0;
    unsigned fragmentEnd = 0;
    do {
        fragmentEnd = std::min(to, rendererStart + textRenderer->text()->length());
        unsigned absoluteFragmentEnd = fragmentEnd - rendererStart;
        absoluteStart = from - rendererStart;
        textWidth += runWidth(*textRenderer, absoluteStart, absoluteFragmentEnd, xPosition + textWidth);
        from = fragmentEnd;
        if (fragmentEnd < to)
            textRenderer = renderer(fragmentEnd, &rendererStart);
    } while (fragmentEnd < to && textRenderer);
    return textWidth;
}

const RenderText* FlowContents::renderer(unsigned position, unsigned* rendererStartPosition) const
{
    unsigned arraySize = m_textRanges.size();
    // Take advantage of the usage pattern.
    if (position >= m_textRanges.at(m_lastRendererIndex).first && m_lastRendererIndex + 1 < arraySize && position < m_textRanges.at(m_lastRendererIndex + 1).first) {
        if (rendererStartPosition)
            *rendererStartPosition = m_textRanges.at(m_lastRendererIndex).first;
        return m_textRanges.at(m_lastRendererIndex).second;
    }
    unsigned left = 0;
    unsigned right = arraySize - 1;
    ASSERT(arraySize);
    ASSERT(position >= 0);
    while (left < right) {
        unsigned middle = (left + right) / 2;
        unsigned endPosition = m_textRanges.at(middle + 1).first;
        if (position > endPosition)
            left = middle + 1;
        else if (position < endPosition)
            right = middle;
        else {
            right = middle + 1;
            break;
        }
    }
    if (rendererStartPosition)
        *rendererStartPosition = m_textRanges.at(right).first;
    return m_textRanges.at(right).second;
}

bool FlowContents::resolveRendererPositions(const RenderText& renderer, unsigned& startPosition, unsigned& endPosition) const
{
    unsigned arraySize = m_textRanges.size();
    if (!arraySize)
        return false;

    unsigned index = 0;
    do {
        auto range = m_textRanges.at(index);
        if (range.second == &renderer) {
            startPosition = range.first;
            ASSERT(index + 1 < arraySize);
            endPosition = m_textRanges.at(index + 1).first;
            return true;
        }
    } while (++index < arraySize);
    return false;
}

bool FlowContents::appendNextRendererContentIfNeeded(unsigned position) const
{
    String string = m_lineBreakIterator.string();
    if (position < string.length())
        return false;

    // Content needs to be requested sequentially.
    ASSERT(position == string.length());
    const RenderText* nextRenderer = renderer(position);
    if (!nextRenderer)
        return false;

    ++m_lastRendererIndex;
    m_lineBreakIterator.resetStringAndReleaseIterator(string + String(nextRenderer->text()), m_style.locale, LineBreakIteratorModeUAX14);
    return true;
}

unsigned FlowContents::nextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    String string = m_lineBreakIterator.string();
    unsigned length = string.length();
    const LChar* text = string.characters8();
    for (; position < length; ++position) {
        bool isSpace = text[position] == ' ';
        if (!(isSpace || text[position] == '\t' || (!m_style.preserveNewline && text[position] == '\n')))
            return position;
        if (isSpace)
            ++spaceCount;
    }
    return length;
}

float FlowContents::runWidth(const RenderText& renderer, unsigned from, unsigned to, float xPosition) const
{
    ASSERT(from < to);
    String string = renderer.text();
    bool measureWithEndSpace = m_style.collapseWhitespace && to < string.length() && string[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    TextRun run(string.characters8() + from, to - from);
    run.setXPos(xPosition);
    run.setTabSize(!!m_style.tabWidth, m_style.tabWidth);
    float width = m_style.font.width(run);
    if (measureWithEndSpace)
        width -= m_style.spaceWidth;
    return width;
}

}
}

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

#include "config.h"
#include "InlineTextItem.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BreakLines.h"

namespace WebCore {
namespace Layout {

static inline bool isWhitespaceCharacter(char character, bool preserveNewline)
{
    return character == ' ' || character == '\t' || (character == '\n' && !preserveNewline);
}

static inline bool isSoftLineBreak(char character, bool preserveNewline)
{
    return preserveNewline && character == '\n';
}

static unsigned moveToNextNonWhitespacePosition(String textContent, unsigned startPosition, bool preserveNewline)
{
    auto nextNonWhiteSpacePosition = startPosition;
    while (nextNonWhiteSpacePosition < textContent.length() && isWhitespaceCharacter(textContent[nextNonWhiteSpacePosition], preserveNewline))
        ++nextNonWhiteSpacePosition;
    return nextNonWhiteSpacePosition - startPosition;
}

static unsigned moveToNextBreakablePosition(unsigned startPosition, LazyLineBreakIterator lineBreakIterator, const RenderStyle& style)
{
    auto findNextBreakablePosition = [&](auto startPosition) {
        auto keepAllWordsForCJK = style.wordBreak() == WordBreak::KeepAll;
        auto breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;

        if (keepAllWordsForCJK) {
            if (breakNBSP)
                return nextBreakablePositionKeepingAllWords(lineBreakIterator, startPosition);
            return nextBreakablePositionKeepingAllWordsIgnoringNBSP(lineBreakIterator, startPosition);
        }

        if (lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
            if (breakNBSP)
                return WebCore::nextBreakablePosition(lineBreakIterator, startPosition);
            return nextBreakablePositionIgnoringNBSP(lineBreakIterator, startPosition);
        }

        if (breakNBSP)
            return nextBreakablePositionWithoutShortcut(lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSPWithoutShortcut(lineBreakIterator, startPosition);
    };

    auto textLength = lineBreakIterator.stringView().length();
    auto currentPosition = startPosition;
    while (currentPosition < textLength - 1) {
        auto nextBreakablePosition = findNextBreakablePosition(currentPosition);
        if (nextBreakablePosition != currentPosition)
            return nextBreakablePosition - currentPosition;
        ++currentPosition;
    }
    return textLength - startPosition;
}

void InlineTextItem::createAndAppendTextItems(InlineItems& inlineContent, const Box& inlineBox)
{
    auto text = inlineBox.textContent();
    if (!text.length())
        return inlineContent.append(makeUnique<InlineTextItem>(inlineBox, 0, 0, false, false));

    auto& style = inlineBox.style();
    auto preserveNewline = style.preserveNewline();
    auto collapseWhiteSpace = style.collapseWhiteSpace();
    LazyLineBreakIterator lineBreakIterator(text);
    unsigned currentPosition = 0;
    while (currentPosition < text.length()) {
        // Soft linebreak?
        if (isSoftLineBreak(text[currentPosition], preserveNewline)) {
            inlineContent.append(makeUnique<InlineTextItem>(inlineBox, currentPosition, 1, true, false));
            ++currentPosition;
            continue;
        }
        if (isWhitespaceCharacter(text[currentPosition], preserveNewline)) {
            auto length = moveToNextNonWhitespacePosition(text, currentPosition, preserveNewline);
            auto isCollapsed = collapseWhiteSpace && length > 1;
            inlineContent.append(makeUnique<InlineTextItem>(inlineBox, currentPosition, length, true, isCollapsed));
            currentPosition += length;
            continue;
        }

        auto length = moveToNextBreakablePosition(currentPosition, lineBreakIterator, style);
        inlineContent.append(makeUnique<InlineTextItem>(inlineBox, currentPosition, length, false, false));
        currentPosition += length;
    }
}

InlineTextItem::InlineTextItem(const Box& inlineBox, unsigned start, unsigned length, bool isWhitespace, bool isCollapsed)
    : InlineItem(inlineBox, Type::Text)
    , m_start(start)
    , m_length(length)
    , m_isWhitespace(isWhitespace)
    , m_isCollapsed(isCollapsed)
{
}

std::unique_ptr<InlineTextItem> InlineTextItem::split(unsigned splitPosition, unsigned length) const
{
    RELEASE_ASSERT(splitPosition >= this->start());
    RELEASE_ASSERT(splitPosition + length <= end());
    return makeUnique<InlineTextItem>(layoutBox(), splitPosition, length, isWhitespace(), isCollapsed());
}

}
}
#endif

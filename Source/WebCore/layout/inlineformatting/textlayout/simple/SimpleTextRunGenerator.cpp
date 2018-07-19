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
#include "SimpleTextRunGenerator.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BreakLines.h"
#include "RenderStyle.h"
#include "TextContentProvider.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(SimpleTextRunGenerator);

SimpleTextRunGenerator::SimpleTextRunGenerator(const TextContentProvider& contentProvider)
    : m_contentProvider(contentProvider)
    , m_textIterator(m_contentProvider.textContent())
    , m_hardLineBreakIterator(contentProvider.hardLineBreaks())
{
}

std::optional<TextRun> SimpleTextRunGenerator::current() const
{
    // A run can either be
    // 1. whitespace (collasped, non-collapsed multi or single)
    // 2. non-whitespace
    // 3. soft line break
    // 4. hard line break
    // 5. invalid
    return m_currentRun;
}

void SimpleTextRunGenerator::reset()
{
    m_xPosition = 0;
    m_currentRun = { };
    m_currentPosition = { };
}

void SimpleTextRunGenerator::findNextRun()
{
    // End of content -not even trailing linebreaks?
    if (m_currentPosition == m_contentProvider.length() && !m_hardLineBreakIterator.current()) {
        m_currentRun = { };
        return;
    }

    // Check if we are at a <br>.
    while (auto* hardLineBreak = m_hardLineBreakIterator.current()) {
        if (m_currentPosition < *hardLineBreak)
            break;
        if (m_currentPosition == *hardLineBreak) {
            ++m_hardLineBreakIterator;
            m_currentRun = TextRun::createHardLineBreakRun(m_currentPosition);
            return;
        }
        ASSERT_NOT_REACHED();
    }

    // Soft linebreak?
    if (isAtSoftLineBreak()) {
        m_currentRun = TextRun::createSoftLineBreakRun(m_currentPosition);
        ++m_currentPosition;
        return;
    }

    auto startPosition = m_currentPosition;
    auto isWhitespaceCollapsed = moveToNextNonWhitespacePosition();
    // Whitespace?
    if (m_currentPosition > startPosition) {
        auto width = m_contentProvider.width(startPosition, isWhitespaceCollapsed ? startPosition + 1 : m_currentPosition, m_xPosition);
        m_currentRun = TextRun::createWhitespaceRun(startPosition, m_currentPosition, width, isWhitespaceCollapsed);
        m_xPosition += width;
        return;
    }

    moveToNextBreakablePosition();
    // nextBreakablePosition returns the same position for certain characters such as hyphens. Call next again with modified position.
    auto skipCurrentPosition = m_currentPosition == startPosition;
    if (skipCurrentPosition) {
        ++m_currentPosition;
        moveToNextBreakablePosition();
    }
    auto width = m_contentProvider.width(startPosition, m_currentPosition, m_xPosition);
    m_currentRun = TextRun::createNonWhitespaceRun(startPosition, m_currentPosition, width);
    m_xPosition += width;
}

void SimpleTextRunGenerator::moveToNextBreakablePosition()
{
    auto findNextBreakablePosition = [&](const TextContentProvider::TextItem& textItem, ItemPosition startPosition) {
        // Swap iterator's content if we advanced to a new string.
        auto iteratorText = m_lineBreakIterator.stringView();
        if (iteratorText != textItem.text) {
            auto textLength = iteratorText.length();
            auto lastCharacter = textLength > 0 ? iteratorText[textLength - 1] : 0;
            auto secondToLastCharacter = textLength > 1 ? iteratorText[textLength - 2] : 0;
            m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
            m_lineBreakIterator.resetStringAndReleaseIterator(textItem.text, textItem.style.locale, LineBreakIteratorMode::Default);
        }

        if (textItem.style.keepAllWordsForCJK) {
            if (textItem.style.breakNBSP)
                return nextBreakablePositionKeepingAllWords(m_lineBreakIterator, startPosition);
            return nextBreakablePositionKeepingAllWordsIgnoringNBSP(m_lineBreakIterator, startPosition);
        }

        if (m_lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
            if (textItem.style.breakNBSP)
                return WebCore::nextBreakablePosition(m_lineBreakIterator, startPosition);
            return nextBreakablePositionIgnoringNBSP(m_lineBreakIterator, startPosition);
        }

        if (textItem.style.breakNBSP)
            return nextBreakablePositionWithoutShortcut(m_lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSPWithoutShortcut(m_lineBreakIterator, startPosition);
    };

    while (auto* textItem = m_textIterator.current()) {
        auto currentItemPosition = m_currentPosition.itemPosition();
        auto nextBreakablePosition = findNextBreakablePosition(*textItem, currentItemPosition);
        ASSERT(nextBreakablePosition >= currentItemPosition);
        m_currentPosition += (nextBreakablePosition - currentItemPosition);
        // Is the next breakable position still in this text?
        auto textItemLength = textItem->end - textItem->start;
        if (m_currentPosition.itemPosition() < textItemLength)
            break;

        // Hard line break inbetween?
        if (isAtLineBreak())
            break;

        // Move over to the next text item.
        m_currentPosition.resetItemPosition();
        ++m_textIterator;
    }
}

bool SimpleTextRunGenerator::moveToNextNonWhitespacePosition()
{
    auto startPosition = m_currentPosition;
    auto collapseWhitespace = false;
    while (auto* textItem = m_textIterator.current()) {
        auto string = textItem->text;
        auto preserveNewline = textItem->style.preserveNewline;
        collapseWhitespace = textItem->style.collapseWhitespace;
        for (; m_currentPosition.itemPosition() < string.length(); ++m_currentPosition) {
            auto character = string[m_currentPosition.itemPosition()];
            bool isWhitespace = character == ' ' || character == '\t' || (!preserveNewline && character == '\n');
            if (!isWhitespace)
                return collapseWhitespace && m_currentPosition - startPosition > 1;
        }
        // Check if there's a hard linebreak between text content.
        if (isAtLineBreak())
            break;

        // Move over to the next text item.
        m_currentPosition.resetItemPosition();
        ++m_textIterator;
    }

    return collapseWhitespace && m_currentPosition - startPosition > 1;
}

bool SimpleTextRunGenerator::isAtLineBreak() const
{
    if (auto* currentHardLineBreakPosition = m_hardLineBreakIterator.current())
        return *currentHardLineBreakPosition == m_currentPosition;
    return isAtSoftLineBreak();
}

bool SimpleTextRunGenerator::isAtSoftLineBreak() const
{
    auto* textItem = m_textIterator.current();
    if (!textItem)
        return false;

    if (!textItem->style.preserveNewline)
        return false;

    return textItem->text[m_currentPosition.itemPosition()] == '\n';
}

}
}
#endif

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
#include "InlineRunProvider.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BreakLines.h"
#include "LayoutInlineBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineRunProvider);

InlineRunProvider::InlineRunProvider(InlineFormattingState& inlineFormattingState)
    : m_inlineFormattingState(inlineFormattingState)
{
}

void InlineRunProvider::append(const Box& layoutBox)
{
    auto inlineItem = std::make_unique<InlineItem>(layoutBox);

    // Special case text item. Texts can overlap multiple items. <span>foo</span><span>bar</span>
    switch (inlineItem->type()) {
    case InlineItem::Type::Text:
        processInlineTextItem(*inlineItem);
        break;
    case InlineItem::Type::HardLineBreak:
        m_inlineRuns.append(InlineRunProvider::Run::createHardLineBreakRun(*inlineItem));
        break;
    case InlineItem::Type::InlineBox:
        m_inlineRuns.append(InlineRunProvider::Run::createBoxRun(*inlineItem));
        break;
    case InlineItem::Type::Float:
        m_inlineRuns.append(InlineRunProvider::Run::createFloatRun(*inlineItem));
        break;
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
    }

    m_inlineFormattingState.inlineContent().add(WTFMove(inlineItem));
}

void InlineRunProvider::insertBefore(const Box&, const Box&)
{
}

void InlineRunProvider::remove(const Box&)
{
}

static inline bool isWhitespace(char character, bool preserveNewline)
{
    return character == ' ' || character == '\t' || (character == '\n' && !preserveNewline);
}

static inline bool isSoftLineBreak(char character, bool preserveNewline)
{
    return preserveNewline && character == '\n';
}

bool InlineRunProvider::isContinousContent(InlineRunProvider::Run::Type newRunType, const InlineItem& newInlineItem)
{
    if (m_inlineRuns.isEmpty())
        return false;

    auto& lastRun = m_inlineRuns.last();
    // Same element, check type only.
    if (&newInlineItem == &lastRun.inlineItem())
        return newRunType == lastRun.type();

    // This new run is from a different inline box.
    // FIXME: check style.
    if (newRunType == InlineRunProvider::Run::Type::NonWhitespace && lastRun.isNonWhitespace())
        return true;

    if (newRunType == InlineRunProvider::Run::Type::Whitespace && lastRun.isWhitespace())
        return newInlineItem.style().collapseWhiteSpace() == lastRun.style().collapseWhiteSpace();

    return false;
}

void InlineRunProvider::processInlineTextItem(const InlineItem& inlineItem)
{
    // We need to reset the run iterator when the text content is not continuous.
    // <span>foo</span><img src=""><span>bar</span> (FIXME: floats?)
    if (!m_inlineRuns.isEmpty() && !m_inlineRuns.last().isText()) {
        m_lineBreakIterator.resetPriorContext();
        m_lineBreakIterator.resetStringAndReleaseIterator("", "", LineBreakIteratorMode::Default);
    }

    auto& style = inlineItem.style();
    auto text = inlineItem.textContent();
    ItemPosition currentItemPosition = 0;
    while (currentItemPosition < text.length()) {

        // Soft linebreak?
        if (isSoftLineBreak(text[currentItemPosition], style.preserveNewline())) {
            m_inlineRuns.append(InlineRunProvider::Run::createSoftLineBreakRun(inlineItem));
            ++currentItemPosition;
            continue;
        }

        auto isWhitespaceRun = isWhitespace(text[currentItemPosition], style.preserveNewline());
        auto length = isWhitespaceRun ? moveToNextNonWhitespacePosition(inlineItem, currentItemPosition) : moveToNextBreakablePosition(inlineItem, currentItemPosition);

        if (isContinousContent(isWhitespaceRun ? InlineRunProvider::Run::Type::Whitespace : InlineRunProvider::Run::Type::NonWhitespace, inlineItem)) {
            auto textContext = m_inlineRuns.last().textContext();
            textContext->setLength(textContext->length() + length);
        } else {
            m_inlineRuns.append(isWhitespaceRun ? InlineRunProvider::Run::createWhitespaceRun(inlineItem, currentItemPosition, length, style.collapseWhiteSpace())
                : InlineRunProvider::Run::createNonWhitespaceRun(inlineItem, currentItemPosition, length));
        }

        currentItemPosition += length;
    }
}

unsigned InlineRunProvider::moveToNextNonWhitespacePosition(const InlineItem& inlineItem, ItemPosition currentItemPosition)
{
    auto text = inlineItem.textContent();
    auto preserveNewline = inlineItem.style().preserveNewline();
    auto nextNonWhiteSpacePosition = currentItemPosition;

    while (nextNonWhiteSpacePosition < text.length() && isWhitespace(text[nextNonWhiteSpacePosition], preserveNewline))
        ++nextNonWhiteSpacePosition;
    return nextNonWhiteSpacePosition - currentItemPosition;
}

unsigned InlineRunProvider::moveToNextBreakablePosition(const InlineItem& inlineItem, ItemPosition currentItemPosition)
{
    auto findNextBreakablePosition = [&](auto inlineText, auto& style, ItemPosition startPosition) {
        // Swap iterator's content if we advanced to a new string.
        auto iteratorText = m_lineBreakIterator.stringView();

        if (iteratorText != inlineText) {
            auto textLength = iteratorText.length();
            auto lastCharacter = textLength > 0 ? iteratorText[textLength - 1] : 0;
            auto secondToLastCharacter = textLength > 1 ? iteratorText[textLength - 2] : 0;
            m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
            m_lineBreakIterator.resetStringAndReleaseIterator(inlineText, style.locale(), LineBreakIteratorMode::Default);
        }

        auto keepAllWordsForCJK = style.wordBreak() == WordBreak::KeepAll;
        auto breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;

        if (keepAllWordsForCJK) {
            if (breakNBSP)
                return nextBreakablePositionKeepingAllWords(m_lineBreakIterator, startPosition);
            return nextBreakablePositionKeepingAllWordsIgnoringNBSP(m_lineBreakIterator, startPosition);
        }

        if (m_lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
            if (breakNBSP)
                return WebCore::nextBreakablePosition(m_lineBreakIterator, startPosition);
            return nextBreakablePositionIgnoringNBSP(m_lineBreakIterator, startPosition);
        }

        if (breakNBSP)
            return nextBreakablePositionWithoutShortcut(m_lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSPWithoutShortcut(m_lineBreakIterator, startPosition);
    };

    auto& style = inlineItem.style();
    auto nextBreakablePosition = findNextBreakablePosition(inlineItem.textContent(), style, currentItemPosition);
    return nextBreakablePosition - currentItemPosition;
}

}
}
#endif

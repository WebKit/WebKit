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

#include "FontCascade.h"
#include "InlineSoftLineBreakItem.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

static_assert(sizeof(InlineItem) == sizeof(InlineTextItem), "");

struct WhitespaceContent {
    size_t length { 0 };
    bool isWordSeparator { true };
};
static Optional<WhitespaceContent> moveToNextNonWhitespacePosition(const StringView& textContent, size_t startPosition, bool preserveNewline, bool preserveTab)
{
    auto hasWordSeparatorCharacter = false;
    auto isWhitespaceCharacter = [&](auto character) {
        // white space processing in CSS affects only the document white space characters: spaces (U+0020), tabs (U+0009), and segment breaks.
        auto isTreatedAsSpaceCharacter = character == space || (character == newlineCharacter && !preserveNewline) || (character == tabCharacter && !preserveTab);
        hasWordSeparatorCharacter = hasWordSeparatorCharacter || isTreatedAsSpaceCharacter;
        return isTreatedAsSpaceCharacter || character == tabCharacter;
    };
    auto nextNonWhiteSpacePosition = startPosition;
    while (nextNonWhiteSpacePosition < textContent.length() && isWhitespaceCharacter(textContent[nextNonWhiteSpacePosition]))
        ++nextNonWhiteSpacePosition;
    return nextNonWhiteSpacePosition == startPosition ? WTF::nullopt : makeOptional(WhitespaceContent { nextNonWhiteSpacePosition - startPosition, hasWordSeparatorCharacter });
}

static unsigned moveToNextBreakablePosition(unsigned startPosition, LazyLineBreakIterator& lineBreakIterator, const RenderStyle& style)
{
    auto textLength = lineBreakIterator.stringView().length();
    auto startPositionForNextBreakablePosition = startPosition;
    while (startPositionForNextBreakablePosition < textLength) {
        auto nextBreakablePosition = TextUtil::findNextBreakablePosition(lineBreakIterator, startPositionForNextBreakablePosition, style);
        // Oftentimes the next breakable position comes back as the start position (most notably hyphens).
        if (nextBreakablePosition != startPosition)
            return nextBreakablePosition - startPosition;
        ++startPositionForNextBreakablePosition;
    }
    return textLength - startPosition;
}

void InlineTextItem::createAndAppendTextItems(InlineItems& inlineContent, const InlineTextBox& inlineTextBox)
{
    auto text = inlineTextBox.content();
    if (!text.length())
        return inlineContent.append(InlineTextItem::createEmptyItem(inlineTextBox));

    auto& style = inlineTextBox.style();
    auto& font = style.fontCascade();
    auto whitespaceContentIsTreatedAsSingleSpace = !TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto shouldPreseveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    LazyLineBreakIterator lineBreakIterator(text);
    unsigned currentPosition = 0;

    auto inlineItemWidth = [&](auto startPosition, auto length) -> Optional<InlineLayoutUnit> {
        if (!inlineTextBox.canUseSimplifiedContentMeasuring())
            return { };
        return TextUtil::width(inlineTextBox, startPosition, startPosition + length, { });
    };

    while (currentPosition < text.length()) {
        auto isSegmentBreakCandidate = [](auto character) {
            return character == '\n';
        };

        // Segment breaks with preserve new line style (white-space: pre, pre-wrap, break-spaces and pre-line) compute to forced line break.
        if (isSegmentBreakCandidate(text[currentPosition]) && shouldPreseveNewline) {
            inlineContent.append(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, currentPosition));
            ++currentPosition;
            continue;
        }

        if (auto whitespaceContent = moveToNextNonWhitespacePosition(text, currentPosition, shouldPreseveNewline, !whitespaceContentIsTreatedAsSingleSpace)) {
            ASSERT(whitespaceContent->length);
            auto appendWhitespaceItem = [&] (auto startPosition, auto itemLength) {
                auto simpleSingleWhitespaceContent = inlineTextBox.canUseSimplifiedContentMeasuring() && (itemLength == 1 || whitespaceContentIsTreatedAsSingleSpace);
                auto width = simpleSingleWhitespaceContent ? makeOptional(InlineLayoutUnit { font.spaceWidth() }) : inlineItemWidth(startPosition, itemLength);
                inlineContent.append(InlineTextItem::createWhitespaceItem(inlineTextBox, startPosition, itemLength, whitespaceContent->isWordSeparator, width));
            };
            if (style.whiteSpace() == WhiteSpace::BreakSpaces) {
                // https://www.w3.org/TR/css-text-3/#white-space-phase-1
                // For break-spaces, a soft wrap opportunity exists after every space and every tab.
                // FIXME: if this turns out to be a perf hit with too many individual whitespace inline items, we should transition this logic to line breaking.
                for (size_t i = 0; i < whitespaceContent->length; ++i)
                    appendWhitespaceItem(currentPosition + i, 1);
            } else
                appendWhitespaceItem(currentPosition, whitespaceContent->length);
            currentPosition += whitespaceContent->length;
            continue;
        }

        auto hasTrailingSoftHyphen = false;
        auto initialNonWhitespacePosition = currentPosition;
        auto isAtSoftHyphen = [&](auto position) {
            return text[position] == softHyphen;
        };
        if (style.hyphens() == Hyphens::None) {
            // Let's merge candidate InlineTextItems separated by soft hyphen when the style says so.
            while (currentPosition < text.length()) {
                auto nonWhiteSpaceLength = moveToNextBreakablePosition(currentPosition, lineBreakIterator, style);
                ASSERT(nonWhiteSpaceLength);
                currentPosition += nonWhiteSpaceLength;
                if (!isAtSoftHyphen(currentPosition - 1))
                    break;
            }
        } else {
            auto nonWhiteSpaceLength = moveToNextBreakablePosition(initialNonWhitespacePosition, lineBreakIterator, style);
            ASSERT(nonWhiteSpaceLength);
            currentPosition += nonWhiteSpaceLength;
            hasTrailingSoftHyphen = isAtSoftHyphen(currentPosition - 1);
        }
        ASSERT(initialNonWhitespacePosition < currentPosition);
        ASSERT_IMPLIES(style.hyphens() == Hyphens::None, !hasTrailingSoftHyphen);
        auto length = currentPosition - initialNonWhitespacePosition;
        inlineContent.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, initialNonWhitespacePosition, length, hasTrailingSoftHyphen, inlineItemWidth(initialNonWhitespacePosition, length)));
    }
}

bool InlineTextItem::isEmptyContent() const
{
    // FIXME: We should check for more zero width content and not just U+200B.
    return !m_length || (m_length == 1 && inlineTextBox().content()[start()] == zeroWidthSpace); 
}

bool InlineTextItem::shouldPreserveSpacesAndTabs(const InlineTextItem& inlineTextItem)
{
    ASSERT(inlineTextItem.isWhitespace());
    return TextUtil::shouldPreserveSpacesAndTabs(inlineTextItem.layoutBox());
}

}
}
#endif

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineItemsBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineSoftLineBreakItem.h"

namespace WebCore {
namespace Layout {

struct WhitespaceContent {
    size_t length { 0 };
    bool isWordSeparator { true };
};
static std::optional<WhitespaceContent> moveToNextNonWhitespacePosition(StringView textContent, size_t startPosition, bool preserveNewline, bool preserveTab, bool treatNonBreakingSpaceAsRegularSpace, bool stopAtWordSeparatorBoundary)
{
    auto hasWordSeparatorCharacter = false;
    auto isWordSeparatorCharacter = false;
    auto isWhitespaceCharacter = [&](auto character) {
        // white space processing in CSS affects only the document white space characters: spaces (U+0020), tabs (U+0009), and segment breaks.
        auto isTreatedAsSpaceCharacter = character == space || (character == newlineCharacter && !preserveNewline) || (character == tabCharacter && !preserveTab) || (character == noBreakSpace && treatNonBreakingSpaceAsRegularSpace);
        isWordSeparatorCharacter = isTreatedAsSpaceCharacter;
        hasWordSeparatorCharacter = hasWordSeparatorCharacter || isWordSeparatorCharacter;
        return isTreatedAsSpaceCharacter || character == tabCharacter;
    };
    auto nextNonWhiteSpacePosition = startPosition;
    while (nextNonWhiteSpacePosition < textContent.length() && isWhitespaceCharacter(textContent[nextNonWhiteSpacePosition])) {
        if (UNLIKELY(stopAtWordSeparatorBoundary && hasWordSeparatorCharacter && !isWordSeparatorCharacter))
            break;
        ++nextNonWhiteSpacePosition;
    }
    return nextNonWhiteSpacePosition == startPosition ? std::nullopt : std::make_optional(WhitespaceContent { nextNonWhiteSpacePosition - startPosition, hasWordSeparatorCharacter });
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

InlineItemsBuilder::InlineItemsBuilder(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : m_root(formattingContextRoot)
    , m_formattingState(formattingState)
{
}

void InlineItemsBuilder::build()
{
    // Traverse the tree and create inline items out of inline boxes and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
    ASSERT(root().hasInFlowOrFloatingChild());
    auto& formattingState = this->formattingState();

    Vector<const Box*> layoutQueue;
    layoutQueue.append(root().firstChild());
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            auto isInlineBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox() && !layoutBox.isOutOfFlowPositioned();
            if (!isInlineBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxStart });
            auto& inlineBoxWithInlineContent = downcast<ContainerBox>(layoutBox);
            if (!inlineBoxWithInlineContent.hasChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isOutOfFlowPositioned()) {
                // Let's not construct InlineItems for out-of-flow content as they don't participate in the inline layout.
                // However to be able to static positioning them, we need to compute their approximate positions.
                formattingState.addOutOfFlowBox(layoutBox);
            } else if (is<LineBreakBox>(layoutBox)) {
                auto& lineBreakBox = downcast<LineBreakBox>(layoutBox);
                formattingState.addInlineItem({ layoutBox, lineBreakBox.isOptional() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });
            } else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isInlineTextBox()) {
                createAndAppendTextItems(downcast<InlineTextBox>(layoutBox));
            } else if (layoutBox.isInlineBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxEnd });
            else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

void InlineItemsBuilder::createAndAppendTextItems(const InlineTextBox& inlineTextBox)
{
    auto& formattingState = this->formattingState();

    auto text = inlineTextBox.content();
    if (!text.length())
        return formattingState.addInlineItem(InlineTextItem::createEmptyItem(inlineTextBox));

    auto& style = inlineTextBox.style();
    auto& fontCascade = style.fontCascade();
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto whitespaceContentIsTreatedAsSingleSpace = !shouldPreserveSpacesAndTabs;
    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto shouldTreatNonBreakingSpaceAsRegularSpace = style.nbspMode() == NBSPMode::Space;
    auto lineBreakIterator = LazyLineBreakIterator { text, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()) };
    unsigned currentPosition = 0;

    auto inlineItemWidth = [&](auto startPosition, auto length) -> std::optional<InlineLayoutUnit> {
        if (!inlineTextBox.canUseSimplifiedContentMeasuring()
            || !TextUtil::canUseSimplifiedTextMeasuringForFirstLine(inlineTextBox.style(), inlineTextBox.firstLineStyle()))
            return { };
        return TextUtil::width(inlineTextBox, fontCascade, startPosition, startPosition + length, { });
    };

    while (currentPosition < text.length()) {
        auto isSegmentBreakCandidate = [](auto character) {
            return character == '\n';
        };

        // Segment breaks with preserve new line style (white-space: pre, pre-wrap, break-spaces and pre-line) compute to forced line break.
        if (isSegmentBreakCandidate(text[currentPosition]) && shouldPreserveNewline) {
            formattingState.addInlineItem(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, currentPosition));
            ++currentPosition;
            continue;
        }

        auto stopAtWordSeparatorBoundary = shouldPreserveSpacesAndTabs && fontCascade.wordSpacing();
        if (auto whitespaceContent = moveToNextNonWhitespacePosition(text, currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, shouldTreatNonBreakingSpaceAsRegularSpace, stopAtWordSeparatorBoundary)) {
            ASSERT(whitespaceContent->length);
            auto appendWhitespaceItem = [&] (auto startPosition, auto itemLength) {
                auto simpleSingleWhitespaceContent = inlineTextBox.canUseSimplifiedContentMeasuring() && (itemLength == 1 || whitespaceContentIsTreatedAsSingleSpace);
                auto width = simpleSingleWhitespaceContent ? std::make_optional(InlineLayoutUnit { fontCascade.spaceWidth() }) : inlineItemWidth(startPosition, itemLength);
                formattingState.addInlineItem(InlineTextItem::createWhitespaceItem(inlineTextBox, startPosition, itemLength, whitespaceContent->isWordSeparator, width));
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
        formattingState.addInlineItem(InlineTextItem::createNonWhitespaceItem(inlineTextBox, initialNonWhitespacePosition, length, hasTrailingSoftHyphen, inlineItemWidth(initialNonWhitespacePosition, length)));
    }
}
}
}

#endif

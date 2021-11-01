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
#include <wtf/Scope.h>

namespace WebCore {
namespace Layout {

#define ALLOW_BIDI_CONTENT 0

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

InlineItems InlineItemsBuilder::build()
{
    InlineItems inlineItems;
    collectInlineItems(inlineItems);
    breakAndComputeBidiLevels(inlineItems);
    return inlineItems;
}

void InlineItemsBuilder::collectInlineItems(InlineItems& inlineItems)
{
    // Traverse the tree and create inline items out of inline boxes and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
    ASSERT(root().hasInFlowOrFloatingChild());
    Vector<const Box*> layoutQueue;
    layoutQueue.append(root().firstChild());
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            auto isInlineBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox() && !layoutBox.isOutOfFlowPositioned();
            if (!isInlineBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            handleInlineBoxStart(layoutBox, inlineItems);
            auto& inlineBox = downcast<ContainerBox>(layoutBox);
            if (!inlineBox.hasChild())
                break;
            layoutQueue.append(inlineBox.firstChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isInlineTextBox())
                handleTextContent(downcast<InlineTextBox>(layoutBox), inlineItems);
            else if (layoutBox.isAtomicInlineLevelBox() || layoutBox.isLineBreakBox())
                handleInlineLevelBox(layoutBox, inlineItems);
            else if (layoutBox.isInlineBox())
                handleInlineBoxEnd(layoutBox, inlineItems);
            else if (layoutBox.isFloatingPositioned())
                inlineItems.append({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isOutOfFlowPositioned()) {
                // Let's not construct InlineItems for out-of-flow content as they don't participate in the inline layout.
                // However to be able to static positioning them, we need to compute their approximate positions.
                m_formattingState.addOutOfFlowBox(layoutBox);
            } else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

void InlineItemsBuilder::breakAndComputeBidiLevels(InlineItems& inlineItems)
{
    if (!hasSeenBidiContent())
        return;
    ASSERT(!inlineItems.isEmpty());

    // 1. Setup the bidi boundary loop by calling ubidi_setPara with the paragraph text.
    // 2. Call ubidi_getLogicalRun to advance to the next bidi boundary until we hit the end of the content.
    // 3. Set the computed bidi level on the associated inline items. Split them as needed.
    UBiDi* ubidi = ubidi_open();

    auto closeUBiDiOnExit = makeScopeExit([&] {
        ubidi_close(ubidi);
    });

    UBiDiLevel bidiLevel = UBIDI_DEFAULT_LTR;
    bool useHeuristicBaseDirection = root().style().unicodeBidi() == EUnicodeBidi::Plaintext;
    if (!useHeuristicBaseDirection)
        bidiLevel = root().style().isLeftToRightDirection() ? UBIDI_LTR : UBIDI_RTL;

    UErrorCode error = U_ZERO_ERROR;
    ubidi_setPara(ubidi, m_paragraphContentBuilder.characters16(), m_paragraphContentBuilder.length(), bidiLevel, nullptr, &error);
    if (U_FAILURE(error)) {
        ASSERT_NOT_REACHED();
        return;
    }

    size_t currentInlineItemIndex = 0;
    for (size_t currentPosition = 0; currentPosition < m_paragraphContentBuilder.length();) {
        UBiDiLevel level;
        int32_t endPosition = currentPosition;
        ubidi_getLogicalRun(ubidi, currentPosition, &endPosition, &level);

        auto setBidiLevelOnRange = [&] {
            // We should always have inline item(s) associated with a bidi range.
            ASSERT(currentInlineItemIndex < inlineItems.size());

            while (currentInlineItemIndex < inlineItems.size()) {
                auto& inlineItem = inlineItems[currentInlineItemIndex];
                if (!inlineItem.isText()) {
                    // FIXME: This fails with multiple inline boxes as they don't advance position.
                    inlineItem.setBidiLevel(level);
                    ++currentInlineItemIndex;
                    continue;
                }
                // FIXME: Find out if this is the most optimal place to measure and cache text widths. 
                auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
                inlineTextItem.setBidiLevel(level);

                auto inlineTextItemEnd = inlineTextItem.end();
                auto bidiEnd = endPosition - m_contentOffsetMap.get(&inlineTextItem.layoutBox());
                if (bidiEnd == inlineTextItemEnd) {
                    ++currentInlineItemIndex;
                    break;
                }
                if (bidiEnd < inlineTextItemEnd) {
                    if (currentInlineItemIndex == inlineItems.size() - 1)
                        inlineItems.append(inlineTextItem.splitAt(bidiEnd));
                    else
                        inlineItems.insert(currentInlineItemIndex + 1, inlineTextItem.splitAt(bidiEnd));
                    ++currentInlineItemIndex;
                    break;
                }
                ++currentInlineItemIndex;
            }
        };
        setBidiLevelOnRange();
        currentPosition = endPosition;
    }
}

void InlineItemsBuilder::handleTextContent(const InlineTextBox& inlineTextBox, InlineItems& inlineItems)
{
    auto text = inlineTextBox.content();
    auto contentLength = text.length();
    if (!contentLength)
        return inlineItems.append(InlineTextItem::createEmptyItem(inlineTextBox));

    auto& style = inlineTextBox.style();
    auto& fontCascade = style.fontCascade();
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto whitespaceContentIsTreatedAsSingleSpace = !shouldPreserveSpacesAndTabs;
    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto shouldTreatNonBreakingSpaceAsRegularSpace = style.nbspMode() == NBSPMode::Space;
    auto lineBreakIterator = LazyLineBreakIterator { text, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()) };
    unsigned currentPosition = 0;

    auto inlineItemWidth = [&](auto startPosition, auto length) -> std::optional<InlineLayoutUnit> {
        if (hasSeenBidiContent()) {
            // Delay content measuring until bidi split.
            return { };
        }
        if (!inlineTextBox.canUseSimplifiedContentMeasuring()
            || !TextUtil::canUseSimplifiedTextMeasuringForFirstLine(inlineTextBox.style(), inlineTextBox.firstLineStyle()))
            return { };
        return TextUtil::width(inlineTextBox, fontCascade, startPosition, startPosition + length, { });
    };

    if (hasSeenBidiContent()) {
        m_contentOffsetMap.set(&inlineTextBox, m_paragraphContentBuilder.length());
        m_paragraphContentBuilder.append(text);
    }

    while (currentPosition < contentLength) {
        auto handleSegmentBreak = [&] {
            // Segment breaks with preserve new line style (white-space: pre, pre-wrap, break-spaces and pre-line) compute to forced line break.
            auto isSegmentBreakCandidate = text[currentPosition] == newlineCharacter;
            if (!isSegmentBreakCandidate || !shouldPreserveNewline)
                return false;
            inlineItems.append(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, currentPosition++));
            return true;
        };
        if (handleSegmentBreak())
            continue;

        auto handleWhitespace = [&] {
            auto stopAtWordSeparatorBoundary = shouldPreserveSpacesAndTabs && fontCascade.wordSpacing();
            auto whitespaceContent = moveToNextNonWhitespacePosition(text, currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, shouldTreatNonBreakingSpaceAsRegularSpace, stopAtWordSeparatorBoundary);
            if (!whitespaceContent)
                return false;

            ASSERT(whitespaceContent->length);
            auto appendWhitespaceItem = [&] (auto startPosition, auto itemLength) {
                auto simpleSingleWhitespaceContent = inlineTextBox.canUseSimplifiedContentMeasuring() && (itemLength == 1 || whitespaceContentIsTreatedAsSingleSpace);
                auto width = simpleSingleWhitespaceContent ? std::make_optional(InlineLayoutUnit { fontCascade.spaceWidth() }) : inlineItemWidth(startPosition, itemLength);
                inlineItems.append(InlineTextItem::createWhitespaceItem(inlineTextBox, startPosition, itemLength, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, width));
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
            return true;
        };
        if (handleWhitespace())
            continue;

        auto handleNonWhitespace = [&] {
            auto startPosition = currentPosition;
            auto endPosition = startPosition;
            auto hasTrailingSoftHyphen = false;
            if (style.hyphens() == Hyphens::None) {
                // Let's merge candidate InlineTextItems separated by soft hyphen when the style says so.
                do {
                    endPosition += moveToNextBreakablePosition(endPosition, lineBreakIterator, style);
                    ASSERT(startPosition < endPosition);
                } while (endPosition < contentLength && text[endPosition - 1] == softHyphen);
            } else {
                endPosition += moveToNextBreakablePosition(startPosition, lineBreakIterator, style);
                ASSERT(startPosition < endPosition);
                hasTrailingSoftHyphen = text[endPosition - 1] == softHyphen;
            }
            ASSERT_IMPLIES(style.hyphens() == Hyphens::None, !hasTrailingSoftHyphen);
            auto inlineItemLength = endPosition - startPosition;
            inlineItems.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, inlineItemLength, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen, inlineItemWidth(startPosition, inlineItemLength)));
            currentPosition = endPosition;
#if ALLOW_BIDI_CONTENT
            // Check if the content has bidi dependency so that we have to start building the paragraph content for ubidi.
            if (text.is8Bit() || hasSeenBidiContent())
                return true;

            for (auto position = startPosition; position < endPosition;) {
                UChar32 character;
                U16_NEXT(text.characters16(), position, contentLength, character);

                auto bidiCategory = u_charDirection(character);
                auto needsBidi = bidiCategory == U_RIGHT_TO_LEFT
                    || bidiCategory == U_RIGHT_TO_LEFT_ARABIC
                    || bidiCategory == U_RIGHT_TO_LEFT_EMBEDDING
                    || bidiCategory == U_RIGHT_TO_LEFT_OVERRIDE
                    || bidiCategory == U_LEFT_TO_RIGHT_EMBEDDING
                    || bidiCategory == U_LEFT_TO_RIGHT_OVERRIDE
                    || bidiCategory == U_POP_DIRECTIONAL_FORMAT;
                if (needsBidi) {
                    buildPreviousTextContent(inlineItems);
                    // buildPreviousTextContent takes care of this content too as some inline items have already been constructed for this text.
                    break;
                }
            }
#endif
            return true;
        };
        if (handleNonWhitespace())
            continue;
        // Unsupported content?
        ASSERT_NOT_REACHED();
    }
}

void InlineItemsBuilder::handleInlineBoxStart(const Box& inlineBox, InlineItems& inlineItems)
{
    inlineItems.append({ inlineBox, InlineItem::Type::InlineBoxStart });
    // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
    auto& style = inlineBox.style();
    if (style.rtlOrdering() == Order::Visual)
        return;

    auto isLeftToRightDirection = style.isLeftToRightDirection();
    switch (style.unicodeBidi()) {
    case EUnicodeBidi::UBNormal:
        // The box does not open an additional level of embedding with respect to the bidirectional algorithm.
        // For inline boxes, implicit reordering works across box boundaries.
        break;
    case EUnicodeBidi::Embed:
        enterBidiContext(inlineBox, isLeftToRightDirection ? leftToRightEmbed : rightToLeftEmbed, inlineItems);
        break;
    case EUnicodeBidi::Override:
        enterBidiContext(inlineBox, isLeftToRightDirection ? leftToRightOverride : rightToLeftOverride, inlineItems);
        break;
    case EUnicodeBidi::Isolate:
        enterBidiContext(inlineBox, isLeftToRightDirection ? leftToRightIsolate : rightToLeftIsolate, inlineItems);
        break;
    case EUnicodeBidi::Plaintext:
        enterBidiContext(inlineBox, firstStrongIsolate, inlineItems);
        break;
    case EUnicodeBidi::IsolateOverride:
        enterBidiContext(inlineBox, firstStrongIsolate, inlineItems);
        enterBidiContext(inlineBox, isLeftToRightDirection ? leftToRightOverride : rightToLeftOverride, inlineItems);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void InlineItemsBuilder::handleInlineBoxEnd(const Box& inlineBox, InlineItems& inlineItems)
{
    inlineItems.append({ inlineBox, InlineItem::Type::InlineBoxEnd });
    // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
    auto& style = inlineBox.style();
    if (style.rtlOrdering() == Order::Visual)
        return;

    switch (style.unicodeBidi()) {
    case EUnicodeBidi::UBNormal:
        // The box does not open an additional level of embedding with respect to the bidirectional algorithm.
        // For inline boxes, implicit reordering works across box boundaries.
        break;
    case EUnicodeBidi::Embed:
        exitBidiContext(inlineBox, popDirectionalFormatting);
        break;
    case EUnicodeBidi::Override:
        exitBidiContext(inlineBox, popDirectionalFormatting);
        break;
    case EUnicodeBidi::Isolate:
        exitBidiContext(inlineBox, popDirectionalIsolate);
        break;
    case EUnicodeBidi::Plaintext:
        exitBidiContext(inlineBox, popDirectionalIsolate);
        break;
    case EUnicodeBidi::IsolateOverride:
        exitBidiContext(inlineBox, popDirectionalFormatting);
        exitBidiContext(inlineBox, popDirectionalIsolate);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void InlineItemsBuilder::handleInlineLevelBox(const Box& layoutBox, InlineItems& inlineItems)
{
    if (layoutBox.isAtomicInlineLevelBox())
        return inlineItems.append({ layoutBox, InlineItem::Type::Box });

    if (layoutBox.isLineBreakBox())
        return inlineItems.append({ layoutBox, downcast<LineBreakBox>(layoutBox).isOptional() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });

    ASSERT_NOT_REACHED();
}

void InlineItemsBuilder::enterBidiContext(const Box& box, UChar controlCharacter, const InlineItems& inlineItems)
{
    if (!hasSeenBidiContent())
        buildPreviousTextContent(inlineItems);
    // Let the first control character represent the  box.
    m_contentOffsetMap.add(&box, m_paragraphContentBuilder.length());
    m_paragraphContentBuilder.append(controlCharacter);
}

void InlineItemsBuilder::exitBidiContext(const Box&, UChar controlCharacter)
{
    ASSERT(hasSeenBidiContent());
    m_paragraphContentBuilder.append(controlCharacter);
}

void InlineItemsBuilder::buildPreviousTextContent(const InlineItems& inlineItems)
{
    ASSERT(!hasSeenBidiContent());
    ASSERT(m_contentOffsetMap.isEmpty());

    const Box* lastLayoutBox = nullptr;
    for (auto& inlineItem : inlineItems) {
        if (!inlineItem.isText())
            continue;
        auto& layoutBox = inlineItem.layoutBox();
        if (lastLayoutBox == &layoutBox) {
            // We've already appended this content.
            continue;
        }
        m_contentOffsetMap.set(&layoutBox, m_paragraphContentBuilder.length());
        m_paragraphContentBuilder.append(downcast<InlineTextBox>(layoutBox).content());
        lastLayoutBox = &layoutBox;
    }
}

}
}

#endif

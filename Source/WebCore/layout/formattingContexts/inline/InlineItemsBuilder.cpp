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
#include "LayoutLineBreakBox.h"
#include "StyleResolver.h"
#include "TextUtil.h"
#include <wtf/Scope.h>
#include <wtf/text/TextBreakIterator.h>

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
    if (hasSeenBidiContent())
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

using InlineItemOffsetList = Vector<std::optional<size_t>>;
static inline void buildBidiParagraph(const InlineItems& inlineItems,  StringBuilder& paragraphContentBuilder, InlineItemOffsetList& inlineItemOffsetList)
{
    const Box* lastInlineTextBox = nullptr;
    size_t inlineTextBoxOffset = 0;
    for (size_t index = 0; index < inlineItems.size(); ++index) {
        auto& inlineItem = inlineItems[index];
        auto& layoutBox = inlineItem.layoutBox();

        if (inlineItem.isText()) {
            if (lastInlineTextBox != &layoutBox) {
                inlineTextBoxOffset = paragraphContentBuilder.length();
                paragraphContentBuilder.append(downcast<InlineTextBox>(layoutBox).content());
                lastInlineTextBox = &layoutBox;
            }
            inlineItemOffsetList.uncheckedAppend({ inlineTextBoxOffset + downcast<InlineTextItem>(inlineItem).start() });
        } else if (inlineItem.isBox()) {
            inlineItemOffsetList.uncheckedAppend({ paragraphContentBuilder.length() });
            paragraphContentBuilder.append(objectReplacementCharacter);
        }
        else if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
            auto& style = inlineItem.style();
            auto initiatesControlCharacter = style.rtlOrdering() == Order::Logical && style.unicodeBidi() != EUnicodeBidi::UBNormal;
            if (!initiatesControlCharacter) {
                // Opaque items do not have position in the bidi paragraph. They inherit their bidi level from the next inline item.
                inlineItemOffsetList.uncheckedAppend({ });
                continue;
            }
            inlineItemOffsetList.uncheckedAppend({ paragraphContentBuilder.length() });
            auto isEnteringBidi = inlineItem.isInlineBoxStart();
            switch (style.unicodeBidi()) {
            case EUnicodeBidi::UBNormal:
                // The box does not open an additional level of embedding with respect to the bidirectional algorithm.
                // For inline boxes, implicit reordering works across box boundaries.
                ASSERT_NOT_REACHED();
                break;
            case EUnicodeBidi::Embed:
                paragraphContentBuilder.append(isEnteringBidi ? (style.isLeftToRightDirection() ? leftToRightEmbed : rightToLeftEmbed) : popDirectionalFormatting);
                break;
            case EUnicodeBidi::Override:
                paragraphContentBuilder.append(isEnteringBidi ? (style.isLeftToRightDirection() ? leftToRightOverride : rightToLeftOverride) : popDirectionalFormatting);
                break;
            case EUnicodeBidi::Isolate:
                paragraphContentBuilder.append(isEnteringBidi ? (style.isLeftToRightDirection() ? leftToRightIsolate : rightToLeftIsolate) : popDirectionalIsolate);
                break;
            case EUnicodeBidi::Plaintext:
                paragraphContentBuilder.append(isEnteringBidi ? firstStrongIsolate : popDirectionalIsolate);
                break;
            case EUnicodeBidi::IsolateOverride:
                if (isEnteringBidi) {
                    paragraphContentBuilder.append(firstStrongIsolate);
                    paragraphContentBuilder.append(style.isLeftToRightDirection() ? leftToRightOverride : rightToLeftOverride);
                } else {
                    paragraphContentBuilder.append(popDirectionalFormatting);
                    paragraphContentBuilder.append(popDirectionalIsolate);
                }
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        } else
            ASSERT_NOT_IMPLEMENTED_YET();
    }
}

void InlineItemsBuilder::breakAndComputeBidiLevels(InlineItems& inlineItems)
{
    ASSERT(hasSeenBidiContent());
    ASSERT(!inlineItems.isEmpty());

    StringBuilder paragraphContentBuilder;
    InlineItemOffsetList inlineItemOffsets;
    inlineItemOffsets.reserveInitialCapacity(inlineItems.size());
    buildBidiParagraph(inlineItems, paragraphContentBuilder, inlineItemOffsets);
    ASSERT(inlineItemOffsets.size() == inlineItems.size());

    // 1. Setup the bidi boundary loop by calling ubidi_setPara with the paragraph text.
    // 2. Call ubidi_getLogicalRun to advance to the next bidi boundary until we hit the end of the content.
    // 3. Set the computed bidi level on the associated inline items. Split them as needed.
    UBiDi* ubidi = ubidi_open();

    auto closeUBiDiOnExit = makeScopeExit([&] {
        ubidi_close(ubidi);
    });

    UBiDiLevel rootBidiLevel = UBIDI_DEFAULT_LTR;
    bool useHeuristicBaseDirection = root().style().unicodeBidi() == EUnicodeBidi::Plaintext;
    if (!useHeuristicBaseDirection)
        rootBidiLevel = root().style().isLeftToRightDirection() ? UBIDI_LTR : UBIDI_RTL;

    UErrorCode error = U_ZERO_ERROR;
    ASSERT(!paragraphContentBuilder.isEmpty());
    ubidi_setPara(ubidi, paragraphContentBuilder.characters16(), paragraphContentBuilder.length(), rootBidiLevel, nullptr, &error);
    if (U_FAILURE(error)) {
        ASSERT_NOT_REACHED();
        return;
    }

    size_t inlineItemIndex = 0;
    auto hasSeenOpaqueItem = false;
    for (size_t currentPosition = 0; currentPosition < paragraphContentBuilder.length();) {
        UBiDiLevel bidiLevel;
        int32_t endPosition = currentPosition;
        ubidi_getLogicalRun(ubidi, currentPosition, &endPosition, &bidiLevel);

        auto setBidiLevelOnRange = [&](size_t bidiEnd, auto bidiLevelForRange) {
            // We should always have inline item(s) associated with a bidi range.
            ASSERT(inlineItemIndex < inlineItemOffsets.size());
            // Start of the range is always where we left off (bidi ranges do not have gaps).
            for (; inlineItemIndex < inlineItemOffsets.size(); ++inlineItemIndex) {
                auto offset = inlineItemOffsets[inlineItemIndex];
                if (!offset) {
                    // This is an opaque item. Let's post-process it.
                    hasSeenOpaqueItem = true;
                    continue;
                }
                if (*offset >= bidiEnd) {
                    // This inline item is outside of the bidi range.
                    break;
                }
                auto& inlineItem = inlineItems[inlineItemIndex];
                inlineItem.setBidiLevel(bidiLevelForRange);
                if (!inlineItem.isText())
                    continue;
                // Check if this text item is on bidi boundary and needs splitting.
                auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
                auto endPosition = *offset + inlineTextItem.length();
                if (endPosition > bidiEnd) {
                    inlineItems.insert(inlineItemIndex + 1, inlineTextItem.split(bidiEnd - *offset));
                    // Right side is going to be processed at the next bidi range.
                    inlineItemOffsets.insert(inlineItemIndex + 1, bidiEnd);
                    ++inlineItemIndex;
                    break;
                }
            }
        };
        setBidiLevelOnRange(endPosition, bidiLevel);
        currentPosition = endPosition;
    }

    auto setBidiLevelForOpaqueInlineItems = [&] {
        if (!hasSeenOpaqueItem)
            return;
        // Opaque items (inline items with no paragraph content) get their bidi level values from their adjacent items.
        auto lastBidiLevel = rootBidiLevel;
        for (auto index = inlineItems.size(); index--;) {
            if (!inlineItemOffsets[index])
                inlineItems[index].setBidiLevel(lastBidiLevel);
            else
                lastBidiLevel = inlineItems[index].bidiLevel();
        }
    };
    setBidiLevelForOpaqueInlineItems();
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
                m_hasSeenBidiContent = bidiCategory == U_RIGHT_TO_LEFT
                    || bidiCategory == U_RIGHT_TO_LEFT_ARABIC
                    || bidiCategory == U_RIGHT_TO_LEFT_EMBEDDING
                    || bidiCategory == U_RIGHT_TO_LEFT_OVERRIDE
                    || bidiCategory == U_LEFT_TO_RIGHT_EMBEDDING
                    || bidiCategory == U_LEFT_TO_RIGHT_OVERRIDE
                    || bidiCategory == U_POP_DIRECTIONAL_FORMAT;
                if (m_hasSeenBidiContent)
                    break;
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
    auto& style = inlineBox.style();
    m_hasSeenBidiContent = m_hasSeenBidiContent || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != EUnicodeBidi::UBNormal); 
}

void InlineItemsBuilder::handleInlineBoxEnd(const Box& inlineBox, InlineItems& inlineItems)
{
    inlineItems.append({ inlineBox, InlineItem::Type::InlineBoxEnd });
    // Inline box end item itself can not trigger bidi content.
    ASSERT(hasSeenBidiContent() || inlineBox.style().rtlOrdering() == Order::Visual || inlineBox.style().unicodeBidi() == EUnicodeBidi::UBNormal);
}

void InlineItemsBuilder::handleInlineLevelBox(const Box& layoutBox, InlineItems& inlineItems)
{
    if (layoutBox.isAtomicInlineLevelBox())
        return inlineItems.append({ layoutBox, InlineItem::Type::Box });

    if (layoutBox.isLineBreakBox())
        return inlineItems.append({ layoutBox, downcast<LineBreakBox>(layoutBox).isOptional() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });

    ASSERT_NOT_REACHED();
}

}
}

#endif

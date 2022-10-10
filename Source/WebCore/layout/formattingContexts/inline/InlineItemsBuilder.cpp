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

#include "InlineSoftLineBreakItem.h"
#include "StyleResolver.h"
#include "TextUtil.h"
#include <wtf/Scope.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {
namespace Layout {

struct WhitespaceContent {
    size_t length { 0 };
    bool isWordSeparator { true };
};
static std::optional<WhitespaceContent> moveToNextNonWhitespacePosition(StringView textContent, size_t startPosition, bool preserveNewline, bool preserveTab, bool stopAtWordSeparatorBoundary)
{
    auto hasWordSeparatorCharacter = false;
    auto isWordSeparatorCharacter = false;
    auto isWhitespaceCharacter = [&](auto character) {
        // white space processing in CSS affects only the document white space characters: spaces (U+0020), tabs (U+0009), and segment breaks.
        auto isTreatedAsSpaceCharacter = character == space || (character == newlineCharacter && !preserveNewline) || (character == tabCharacter && !preserveTab);
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

InlineItemsBuilder::InlineItemsBuilder(const ElementBox& formattingContextRoot, InlineFormattingState& formattingState)
    : m_root(formattingContextRoot)
    , m_formattingState(formattingState)
    , m_needsVisualReordering(!formattingContextRoot.style().isLeftToRightDirection())
{
}

InlineItems InlineItemsBuilder::build()
{
    InlineItems inlineItems;
    collectInlineItems(inlineItems);
    if (needsVisualReordering())
        breakAndComputeBidiLevels(inlineItems);
    computeInlineTextItemWidths(inlineItems);
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
            auto& inlineBox = downcast<ElementBox>(layoutBox);
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

static void replaceNonPreservedNewLineCharactersAndAppend(const InlineTextBox& inlineTextBox, StringBuilder& paragraphContentBuilder)
{
    // ubidi prefers non-preserved new lines as whitespace characters.
    if (TextUtil::shouldPreserveNewline(inlineTextBox))
        return paragraphContentBuilder.append(inlineTextBox.content());

    auto textContent = inlineTextBox.content();
    auto contentLength = textContent.length();
    auto needsUnicodeHandling = !textContent.is8Bit();
    size_t nonReplacedContentStartPosition = 0;
    for (size_t position = 0; position < contentLength;) {
        auto startPosition = position;
        auto isNewLineCharacter = [&] {
            if (needsUnicodeHandling) {
                UChar32 character;
                U16_NEXT(textContent.characters16(), position, contentLength, character);
                return character == newlineCharacter;
            }
            return textContent[position++] == newlineCharacter;
        };
        if (!isNewLineCharacter())
            continue;

        if (nonReplacedContentStartPosition < startPosition)
            paragraphContentBuilder.append(StringView(textContent).substring(nonReplacedContentStartPosition, startPosition - nonReplacedContentStartPosition));
        paragraphContentBuilder.append(space);
        nonReplacedContentStartPosition = position;
    }
    if (nonReplacedContentStartPosition < contentLength)
        paragraphContentBuilder.append(StringView(textContent).right(contentLength - nonReplacedContentStartPosition));
}

struct BidiContext {
    UnicodeBidi unicodeBidi;
    bool isLeftToRightDirection { false };
    bool isBlockLevel { false };
};
using BidiContextStack = Vector<BidiContext>;

enum class EnterExitType : uint8_t {
    EnteringBlock,
    ExitingBlock,
    EnteringInlineBox,
    ExitingInlineBox
};
static inline void handleEnterExitBidiContext(StringBuilder& paragraphContentBuilder, UnicodeBidi unicodeBidi, bool isLTR, EnterExitType enterExitType, BidiContextStack& bidiContextStack)
{
    auto isEnteringBidi = enterExitType == EnterExitType::EnteringBlock || enterExitType == EnterExitType::EnteringInlineBox;
    switch (unicodeBidi) {
    case UnicodeBidi::Normal:
        // The box does not open an additional level of embedding with respect to the bidirectional algorithm.
        // For inline boxes, implicit reordering works across box boundaries.
        break;
    case UnicodeBidi::Embed:
        // Isolate and embed values are enforced by default and redundant on the block level boxes.
        if (enterExitType == EnterExitType::EnteringBlock)
            break;
        paragraphContentBuilder.append(isEnteringBidi ? (isLTR ? leftToRightEmbed : rightToLeftEmbed) : popDirectionalFormatting);
        break;
    case UnicodeBidi::Override:
        paragraphContentBuilder.append(isEnteringBidi ? (isLTR ? leftToRightOverride : rightToLeftOverride) : popDirectionalFormatting);
        break;
    case UnicodeBidi::Isolate:
        // Isolate and embed values are enforced by default and redundant on the block level boxes.
        if (enterExitType == EnterExitType::EnteringBlock)
            break;
        paragraphContentBuilder.append(isEnteringBidi ? (isLTR ? leftToRightIsolate : rightToLeftIsolate) : popDirectionalIsolate);
        break;
    case UnicodeBidi::Plaintext:
        paragraphContentBuilder.append(isEnteringBidi ? firstStrongIsolate : popDirectionalIsolate);
        break;
    case UnicodeBidi::IsolateOverride:
        if (isEnteringBidi) {
            paragraphContentBuilder.append(firstStrongIsolate);
            paragraphContentBuilder.append(isLTR ? leftToRightOverride : rightToLeftOverride);
        } else {
            paragraphContentBuilder.append(popDirectionalFormatting);
            paragraphContentBuilder.append(popDirectionalIsolate);
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    isEnteringBidi ? bidiContextStack.append({ unicodeBidi, isLTR, enterExitType == EnterExitType::EnteringBlock }) : bidiContextStack.removeLast();
}

using InlineItemOffsetList = Vector<std::optional<size_t>>;
static inline void buildBidiParagraph(const RenderStyle& rootStyle, const InlineItems& inlineItems,  StringBuilder& paragraphContentBuilder, InlineItemOffsetList& inlineItemOffsetList)
{
    auto bidiContextStack = BidiContextStack { };
    handleEnterExitBidiContext(paragraphContentBuilder, rootStyle.unicodeBidi(), rootStyle.isLeftToRightDirection(), EnterExitType::EnteringBlock, bidiContextStack);
    if (rootStyle.rtlOrdering() != Order::Logical)
        handleEnterExitBidiContext(paragraphContentBuilder, UnicodeBidi::Override, rootStyle.isLeftToRightDirection(), EnterExitType::EnteringBlock, bidiContextStack);

    const Box* lastInlineTextBox = nullptr;
    size_t inlineTextBoxOffset = 0;
    for (size_t index = 0; index < inlineItems.size(); ++index) {
        auto& inlineItem = inlineItems[index];
        auto& layoutBox = inlineItem.layoutBox();

        auto appendTextBasedContent = [&] {
            // Append the entire InlineTextBox content and keep track of individual inline item positions.
            if (lastInlineTextBox == &layoutBox)
                return;
            inlineTextBoxOffset = paragraphContentBuilder.length();
            replaceNonPreservedNewLineCharactersAndAppend(downcast<InlineTextBox>(layoutBox), paragraphContentBuilder);
            lastInlineTextBox = &layoutBox;
        };

        if (inlineItem.isText()) {
            appendTextBasedContent();
            inlineItemOffsetList.uncheckedAppend({ inlineTextBoxOffset + downcast<InlineTextItem>(inlineItem).start() });
        } else if (inlineItem.isBox()) {
            inlineItemOffsetList.uncheckedAppend({ paragraphContentBuilder.length() });
            paragraphContentBuilder.append(objectReplacementCharacter);
        } else if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
            auto& style = inlineItem.style();
            auto initiatesControlCharacter = style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal;
            if (!initiatesControlCharacter) {
                // Opaque items do not have position in the bidi paragraph. They inherit their bidi level from the next inline item.
                inlineItemOffsetList.uncheckedAppend({ });
                continue;
            }
            inlineItemOffsetList.uncheckedAppend({ paragraphContentBuilder.length() });
            auto isEnteringBidi = inlineItem.isInlineBoxStart();
            handleEnterExitBidiContext(paragraphContentBuilder
                , style.unicodeBidi()
                , style.isLeftToRightDirection()
                , isEnteringBidi ? EnterExitType::EnteringInlineBox : EnterExitType::ExitingInlineBox
                , bidiContextStack
            );
        } else if (inlineItem.isSoftLineBreak()) {
            // FIXME: Unwind the bidi stack for soft line break too.
            appendTextBasedContent();
            inlineItemOffsetList.uncheckedAppend({ inlineTextBoxOffset + downcast<InlineSoftLineBreakItem>(inlineItem).position() });
        } else if (inlineItem.isHardLineBreak()) {
            // Bidi handling requires us to close all the nested bidi contexts at the end of the line triggered by forced line breaks
            // and re-open it for the content on the next line (i.e. paragraph handling).
            auto copyOfBidiStack = bidiContextStack;

            size_t blockLevelBidiContextIndex = 0;
            auto unwindBidiContextStack = [&] {
                // Unwind all the way up to the block entry.
                ASSERT(!bidiContextStack.isEmpty());
                size_t unwindingIndex = copyOfBidiStack.size() - 1; 
                while (unwindingIndex && !copyOfBidiStack[unwindingIndex].isBlockLevel) {
                    handleEnterExitBidiContext(paragraphContentBuilder
                        , copyOfBidiStack[unwindingIndex].unicodeBidi
                        , copyOfBidiStack[unwindingIndex].isLeftToRightDirection
                        , EnterExitType::ExitingInlineBox
                        , bidiContextStack
                    );
                    --unwindingIndex;
                }
                blockLevelBidiContextIndex = unwindingIndex; 
                // and unwind the block entries as well.
                do {
                    ASSERT(copyOfBidiStack[unwindingIndex].isBlockLevel);
                    handleEnterExitBidiContext(paragraphContentBuilder
                        , copyOfBidiStack[unwindingIndex].unicodeBidi
                        , copyOfBidiStack[unwindingIndex].isLeftToRightDirection
                        , EnterExitType::ExitingBlock
                        , bidiContextStack
                    );
                } while (unwindingIndex--);
            };
            unwindBidiContextStack();

            inlineItemOffsetList.uncheckedAppend({ paragraphContentBuilder.length() });
            paragraphContentBuilder.append(newlineCharacter);

            auto rewindBidiContextStack = [&] {
                for (size_t blockLevelIndex = 0; blockLevelIndex <= blockLevelBidiContextIndex; ++blockLevelIndex) {
                    handleEnterExitBidiContext(paragraphContentBuilder
                        , copyOfBidiStack[blockLevelIndex].unicodeBidi
                        , copyOfBidiStack[blockLevelIndex].isLeftToRightDirection
                        , EnterExitType::EnteringBlock
                        , bidiContextStack
                    );
                }

                for (size_t inlineLevelIndex = blockLevelBidiContextIndex + 1; inlineLevelIndex < copyOfBidiStack.size(); ++inlineLevelIndex) {
                    handleEnterExitBidiContext(paragraphContentBuilder
                        , copyOfBidiStack[inlineLevelIndex].unicodeBidi
                        , copyOfBidiStack[inlineLevelIndex].isLeftToRightDirection
                        , EnterExitType::EnteringInlineBox
                        , bidiContextStack
                    );
                }
            };
            rewindBidiContextStack();
        } else if (inlineItem.isWordBreakOpportunity()) {
            // Soft wrap opportunity markers are opaque to bidi. 
            inlineItemOffsetList.uncheckedAppend({ });            
        } else if (inlineItem.isFloat()) {
            // Floats are not part of the inline content which make them opaque to bidi.
            inlineItemOffsetList.uncheckedAppend({ });
        } else
            ASSERT_NOT_IMPLEMENTED_YET();
    }
}

void InlineItemsBuilder::breakAndComputeBidiLevels(InlineItems& inlineItems)
{
    ASSERT(needsVisualReordering());
    ASSERT(!inlineItems.isEmpty());

    StringBuilder paragraphContentBuilder;
    InlineItemOffsetList inlineItemOffsets;
    inlineItemOffsets.reserveInitialCapacity(inlineItems.size());
    buildBidiParagraph(root().style(), inlineItems, paragraphContentBuilder, inlineItemOffsets);
    if (paragraphContentBuilder.isEmpty()) {
        // Style may trigger visual reordering even on a completely empty content.
        // e.g. <div><span style="direction:rtl"></span></div>
        // Let's not try to do bidi handling when there's no content to reorder.
        return;
    }
    ASSERT(inlineItemOffsets.size() == inlineItems.size());
    // 1. Setup the bidi boundary loop by calling ubidi_setPara with the paragraph text.
    // 2. Call ubidi_getLogicalRun to advance to the next bidi boundary until we hit the end of the content.
    // 3. Set the computed bidi level on the associated inline items. Split them as needed.
    UBiDi* ubidi = ubidi_open();

    auto closeUBiDiOnExit = makeScopeExit([&] {
        ubidi_close(ubidi);
    });

    UBiDiLevel rootBidiLevel = UBIDI_DEFAULT_LTR;
    bool useHeuristicBaseDirection = root().style().unicodeBidi() == UnicodeBidi::Plaintext;
    if (!useHeuristicBaseDirection)
        rootBidiLevel = root().style().isLeftToRightDirection() ? UBIDI_LTR : UBIDI_RTL;

    auto bidiContent = StringView { paragraphContentBuilder }.upconvertedCharacters();
    auto bidiContentLength = paragraphContentBuilder.length();
    UErrorCode error = U_ZERO_ERROR;
    ASSERT(bidiContentLength);
    ubidi_setPara(ubidi
        , bidiContent
        , bidiContentLength
        , rootBidiLevel
        , nullptr
        , &error);

    if (U_FAILURE(error)) {
        ASSERT_NOT_REACHED();
        return;
    }

    size_t inlineItemIndex = 0;
    auto hasSeenOpaqueItem = false;
    for (size_t currentPosition = 0; currentPosition < bidiContentLength;) {
        UBiDiLevel bidiLevel;
        int32_t endPosition = currentPosition;
        ubidi_getLogicalRun(ubidi, currentPosition, &endPosition, &bidiLevel);

        auto setBidiLevelOnRange = [&](size_t bidiEnd, auto bidiLevelForRange) {
            // We should always have inline item(s) associated with a bidi range.
            ASSERT(inlineItemIndex < inlineItemOffsets.size());
            // Start of the range is always where we left off (bidi ranges do not have gaps).
            for (; inlineItemIndex < inlineItemOffsets.size(); ++inlineItemIndex) {
                auto offset = inlineItemOffsets[inlineItemIndex];
                auto& inlineItem = inlineItems[inlineItemIndex];
                if (!offset) {
                    // This is an opaque item. Let's post-process it.
                    hasSeenOpaqueItem = true;
                    inlineItem.setBidiLevel(bidiLevelForRange);
                    continue;
                }
                if (*offset >= bidiEnd) {
                    // This inline item is outside of the bidi range.
                    break;
                }
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
        // Let's not confuse ubidi with non-content entries.
        // Opaque runs are excluded from the visual list (ie. only empty inline boxes should be kept around as bidi content -to figure out their visual order).
        enum class InlineBoxHasContent : bool { No, Yes };
        Vector<InlineBoxHasContent> inlineBoxContentFlagStack;
        inlineBoxContentFlagStack.reserveInitialCapacity(inlineItems.size());
        for (auto index = inlineItems.size(); index--;) {
            auto& inlineItem = inlineItems[index];
            if (inlineItem.isInlineBoxStart()) {
                ASSERT(!inlineBoxContentFlagStack.isEmpty());
                if (inlineBoxContentFlagStack.takeLast() == InlineBoxHasContent::Yes)
                    inlineItems[index].setBidiLevel(InlineItem::opaqueBidiLevel);
                continue;
            }
            if (inlineItem.isInlineBoxEnd()) {
                inlineBoxContentFlagStack.append(InlineBoxHasContent::No);
                inlineItem.setBidiLevel(InlineItem::opaqueBidiLevel);
                continue;
            }
            if (inlineItem.isWordBreakOpportunity()) {
                inlineItem.setBidiLevel(InlineItem::opaqueBidiLevel);
                continue;
            }
            // Mark the inline box stack with "content yes", when we come across a content type of inline item.
            if (!inlineItem.isText() || !downcast<InlineTextItem>(inlineItem).isWhitespace() || TextUtil::shouldPreserveSpacesAndTabs(inlineItem.layoutBox()))
                inlineBoxContentFlagStack.fill(InlineBoxHasContent::Yes);
        }
    };
    setBidiLevelForOpaqueInlineItems();
}

static inline bool canCacheMeasuredWidthOnInlineTextItem(const InlineTextBox& inlineTextBox, size_t start, size_t length, bool isWhitespace)
{
    // Do not cache when:
    // 1. first-line style's unique font properties may produce non-matching width values.
    // 2. position dependent content is present (preserved tab character atm).
    if (&inlineTextBox.style() != &inlineTextBox.firstLineStyle() && inlineTextBox.style().fontCascade() != inlineTextBox.firstLineStyle().fontCascade())
        return false;
    if (!isWhitespace || !TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox))
        return true;
    // FIXME: Currently we opt out of caching only when we see a preserved \t character (position dependent measured width).
    auto textContent = inlineTextBox.content();
    for (auto index = start; index < start + length; ++index) {
        if (textContent[index] == tabCharacter)
            return false;
    }
    return true;
}

void InlineItemsBuilder::computeInlineTextItemWidths(InlineItems& inlineItems)
{
    for (auto& inlineItem : inlineItems) {
        if (!inlineItem.isText())
            continue;

        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto& inlineTextBox = inlineTextItem.inlineTextBox();
        auto start = inlineTextItem.start();
        auto length = inlineTextItem.length();
        auto needsMeasuring = length && !inlineTextItem.isZeroWidthSpaceSeparator();
        if (!needsMeasuring || !canCacheMeasuredWidthOnInlineTextItem(inlineTextBox, start, length, inlineTextItem.isWhitespace()))
            continue;
        inlineTextItem.setWidth(TextUtil::width(inlineTextItem, inlineTextItem.style().fontCascade(), start, start + length, { }));
    }
}

void InlineItemsBuilder::handleTextContent(const InlineTextBox& inlineTextBox, InlineItems& inlineItems)
{
    auto text = inlineTextBox.content();
    auto contentLength = text.length();
    if (!contentLength)
        return inlineItems.append(InlineTextItem::createEmptyItem(inlineTextBox));

    m_needsVisualReordering = m_needsVisualReordering || TextUtil::containsStrongDirectionalityText(text);
    auto& style = inlineTextBox.style();
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto lineBreakIterator = LazyLineBreakIterator { text, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()) };
    unsigned currentPosition = 0;

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
            auto stopAtWordSeparatorBoundary = shouldPreserveSpacesAndTabs && style.fontCascade().wordSpacing();
            auto whitespaceContent = moveToNextNonWhitespacePosition(text, currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, stopAtWordSeparatorBoundary);
            if (!whitespaceContent)
                return false;

            ASSERT(whitespaceContent->length);
            if (style.whiteSpace() == WhiteSpace::BreakSpaces) {
                // https://www.w3.org/TR/css-text-3/#white-space-phase-1
                // For break-spaces, a soft wrap opportunity exists after every space and every tab.
                // FIXME: if this turns out to be a perf hit with too many individual whitespace inline items, we should transition this logic to line breaking.
                for (size_t i = 0; i < whitespaceContent->length; ++i)
                    inlineItems.append(InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition + i, 1, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, { }));
            } else
                inlineItems.append(InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition, whitespaceContent->length, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, { }));
            currentPosition += whitespaceContent->length;
            return true;
        };
        if (handleWhitespace())
            continue;

        auto handleNonBreakingSpace = [&] {
            if (style.nbspMode() != NBSPMode::Space) {
                // Let's just defer to regular non-whitespace inline items when non breaking space needs no special handling.
                return false;
            }
            auto startPosition = currentPosition;
            auto endPosition = startPosition;
            for (; endPosition < contentLength; ++endPosition) {
                if (text[endPosition] != noBreakSpace)
                    break;
            }
            if (startPosition == endPosition)
                return false;
            for (auto index = startPosition; index < endPosition; ++index)
                inlineItems.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, index, 1, UBIDI_DEFAULT_LTR, { }, { }));
            currentPosition = endPosition;
            return true;
        };
        if (handleNonBreakingSpace())
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
            inlineItems.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, endPosition - startPosition, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen, { }));
            currentPosition = endPosition;
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
    m_needsVisualReordering = m_needsVisualReordering || !style.isLeftToRightDirection() || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal);
}

void InlineItemsBuilder::handleInlineBoxEnd(const Box& inlineBox, InlineItems& inlineItems)
{
    inlineItems.append({ inlineBox, InlineItem::Type::InlineBoxEnd });
    // Inline box end item itself can not trigger bidi content.
    ASSERT(needsVisualReordering() || inlineBox.style().isLeftToRightDirection() || inlineBox.style().rtlOrdering() == Order::Visual || inlineBox.style().unicodeBidi() == UnicodeBidi::Normal);
}

void InlineItemsBuilder::handleInlineLevelBox(const Box& layoutBox, InlineItems& inlineItems)
{
    if (layoutBox.isAtomicInlineLevelBox())
        return inlineItems.append({ layoutBox, InlineItem::Type::Box });

    if (layoutBox.isLineBreakBox())
        return inlineItems.append({ layoutBox, layoutBox.isWordBreakOpportunity() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });

    ASSERT_NOT_REACHED();
}

}
}


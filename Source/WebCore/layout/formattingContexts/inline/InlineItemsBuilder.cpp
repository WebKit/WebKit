/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "FontCascade.h"
#include "InlineSoftLineBreakItem.h"
#include "RenderStyleInlines.h"
#include "StyleResolver.h"
#include "TextUtil.h"
#include "UnicodeBidi.h"
#include <wtf/Scope.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

struct WhitespaceContent {
    size_t length { 0 };
    bool isWordSeparator { true };
};

template<typename CharacterType>
static std::optional<WhitespaceContent> moveToNextNonWhitespacePosition(std::span<const CharacterType> characters, size_t startPosition, bool preserveNewline, bool preserveTab, bool stopAtWordSeparatorBoundary)
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
    auto* rawCharacters = characters.data(); // Not using characters[nextNonWhiteSpacePosition] to bypass the bounds check.
    while (nextNonWhiteSpacePosition < characters.size() && isWhitespaceCharacter(rawCharacters[nextNonWhiteSpacePosition])) {
        if (UNLIKELY(stopAtWordSeparatorBoundary && hasWordSeparatorCharacter && !isWordSeparatorCharacter))
            break;
        ++nextNonWhiteSpacePosition;
    }
    return nextNonWhiteSpacePosition == startPosition ? std::nullopt : std::make_optional(WhitespaceContent { nextNonWhiteSpacePosition - startPosition, hasWordSeparatorCharacter });
}

static unsigned moveToNextBreakablePosition(unsigned startPosition, CachedLineBreakIteratorFactory& lineBreakIteratorFactory, const RenderStyle& style)
{
    auto textLength = lineBreakIteratorFactory.stringView().length();
    auto startPositionForNextBreakablePosition = startPosition;
    while (startPositionForNextBreakablePosition < textLength) {
        auto nextBreakablePosition = TextUtil::findNextBreakablePosition(lineBreakIteratorFactory, startPositionForNextBreakablePosition, style);
        // Oftentimes the next breakable position comes back as the start position (most notably hyphens).
        if (nextBreakablePosition != startPosition)
            return nextBreakablePosition - startPosition;
        ++startPositionForNextBreakablePosition;
    }
    return textLength - startPosition;
}

InlineItemsBuilder::InlineItemsBuilder(InlineContentCache& inlineContentCache, const ElementBox& root)
    : m_inlineContentCache(inlineContentCache)
    , m_root(root)
{
}

void InlineItemsBuilder::build(InlineItemPosition startPosition)
{
    InlineItemList inlineItemList;
    collectInlineItems(inlineItemList, startPosition);

    if (!root().style().isLeftToRightDirection() || contentRequiresVisualReordering()) {
        // FIXME: Add support for partial, yet paragraph level bidi content handling.
        breakAndComputeBidiLevels(inlineItemList);
    }
    computeInlineTextItemWidths(inlineItemList);

    auto adjustInlineContentCacheWithNewInlineItems = [&] {
        auto contentAttributes = InlineContentCache::InlineItems::ContentAttributes { m_contentRequiresVisualReordering, m_isTextAndForcedLineBreakOnlyContent, m_inlineBoxCount };
        auto& inlineItemCache = inlineContentCache().inlineItems();
        if (!startPosition)
            return inlineItemCache.set(WTFMove(inlineItemList), contentAttributes);
        // Let's first remove the dirty inline items if there are any.
        if (startPosition.index >= inlineItemCache.content().size()) {
            ASSERT_NOT_REACHED();
            return inlineItemCache.set(WTFMove(inlineItemList), contentAttributes);
        }
        inlineItemCache.replace(startPosition.index, WTFMove(inlineItemList), contentAttributes);
    };
    adjustInlineContentCacheWithNewInlineItems();

#if ASSERT_ENABLED
    // Check if we've got matching inline box start/end pairs and unique inline level items (non-text, non-inline box items).
    size_t inlineBoxStart = 0;
    size_t inlineBoxEnd = 0;
    auto inlineLevelItems = HashSet<const Box*> { };
    for (auto& inlineItem : inlineContentCache().inlineItems().content()) {
        if (inlineItem.isInlineBoxStart())
            ++inlineBoxStart;
        else if (inlineItem.isInlineBoxEnd())
            ++inlineBoxEnd;
        else {
            auto hasToBeUniqueLayoutBox = inlineItem.isBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak();
            if (hasToBeUniqueLayoutBox)
                ASSERT(inlineLevelItems.add(&inlineItem.layoutBox()).isNewEntry);
        }
    }
    ASSERT(inlineBoxStart == inlineBoxEnd);
#endif
}

static inline bool isTextOrLineBreak(const Box& layoutBox)
{
    return layoutBox.isInFlow() && (layoutBox.isInlineTextBox() || (layoutBox.isLineBreakBox() && !layoutBox.isWordBreakOpportunity()));
}

static bool requiresVisualReordering(const Box& layoutBox)
{
    if (is<InlineTextBox>(layoutBox))
        return TextUtil::containsStrongDirectionalityText(downcast<InlineTextBox>(layoutBox).content());
    if (layoutBox.isInlineBox() && layoutBox.isInFlow()) {
        auto& style = layoutBox.style();
        return !style.isLeftToRightDirection() || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal);
    }
    return false;
}

bool InlineItemsBuilder::traverseUntilDamaged(LayoutQueue& layoutQueue, const Box& subtreeRoot, const Box& firstDamagedLayoutBox)
{
    if (&subtreeRoot == &firstDamagedLayoutBox)
        return true;

    m_contentRequiresVisualReordering = m_contentRequiresVisualReordering || requiresVisualReordering(subtreeRoot);
    if (!isTextOrLineBreak(subtreeRoot)) {
        m_isTextAndForcedLineBreakOnlyContent = false;
        if (subtreeRoot.isInlineBox())
            ++m_inlineBoxCount;
    }

    auto shouldSkipSubtree = subtreeRoot.establishesFormattingContext();
    if (!shouldSkipSubtree && is<ElementBox>(subtreeRoot) && downcast<ElementBox>(subtreeRoot).hasChild()) {
        auto& firstChild = *downcast<ElementBox>(subtreeRoot).firstChild();
        layoutQueue.append(firstChild);
        if (traverseUntilDamaged(layoutQueue, firstChild, firstDamagedLayoutBox))
            return true;
        layoutQueue.takeLast();
    }
    if (auto* nextSibling = subtreeRoot.nextSibling()) {
        layoutQueue.takeLast();
        layoutQueue.append(*nextSibling);
        if (traverseUntilDamaged(layoutQueue, *nextSibling, firstDamagedLayoutBox))
            return true;
    }
    return false;
}

InlineItemsBuilder::LayoutQueue InlineItemsBuilder::initializeLayoutQueue(InlineItemPosition startPosition)
{
    auto& root = this->root();
    if (!root.firstChild()) {
        // There should always be at least one inflow child in this inline formatting context.
        ASSERT_NOT_REACHED();
        return { };
    }

    if (!startPosition)
        return { *root.firstChild() };

    // For partial layout we need to build the layout queue up to the point where the new content is in order
    // to be able to produce non-content type of trailing inline items.
    // e.g <div><span<span>text</span></span> produces
    // [inline box start][inline box start][text][inline box end][inline box end]
    // and inserting new content after text
    // <div><span><span>text more_text</span></span> should produce
    // [inline box start][inline box start][text][ ][more_text][inline box end][inline box end]
    // where we start processing the content at the new layout box and continue with whatever we have on the stack (layout queue).
    auto& existingInlineItems = inlineContentCache().inlineItems().content();
    if (startPosition.index >= existingInlineItems.size()) {
        ASSERT_NOT_REACHED();
        return { *root.firstChild() };
    }

    auto& firstDamagedLayoutBox = existingInlineItems[startPosition.index].layoutBox();
    auto& firstChild = *root.firstChild();
    LayoutQueue layoutQueue;
    layoutQueue.append(firstChild);
    traverseUntilDamaged(layoutQueue, firstChild, firstDamagedLayoutBox);

    if (layoutQueue.isEmpty()) {
        ASSERT_NOT_REACHED();
        layoutQueue.append(*root.firstChild());
    }
    return layoutQueue;
}

void InlineItemsBuilder::collectInlineItems(InlineItemList& inlineItemList, InlineItemPosition startPosition)
{
    // Traverse the tree and create inline items out of inline boxes and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
    auto layoutQueue = initializeLayoutQueue(startPosition);
    auto& inlineItemCache = this->inlineContentCache().inlineItems();

    auto partialContentOffset = [&](auto& inlineTextBox) -> std::optional<size_t> {
        if (!startPosition)
            return { };
        auto& currentInlineItems = inlineItemCache.content();
        if (startPosition.index >= currentInlineItems.size()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        auto& damagedInlineItem = currentInlineItems[startPosition.index];
        if (&inlineTextBox != &damagedInlineItem.layoutBox())
            return { };
        if (is<InlineTextItem>(damagedInlineItem))
            return downcast<InlineTextItem>(damagedInlineItem).start();
        if (is<InlineSoftLineBreakItem>(damagedInlineItem))
            return downcast<InlineSoftLineBreakItem>(damagedInlineItem).position();
        ASSERT_NOT_REACHED();
        return { };
    };

    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto layoutBox = layoutQueue.last();
            auto isInlineBoxWithInlineContent = layoutBox->isInlineBox() && !layoutBox->isInlineTextBox() && !layoutBox->isLineBreakBox() && !layoutBox->isOutOfFlowPositioned();
            if (!isInlineBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            handleInlineBoxStart(layoutBox, inlineItemList);
            auto& inlineBox = downcast<ElementBox>(layoutBox);
            if (!inlineBox.hasChild())
                break;
            layoutQueue.append(*inlineBox.firstChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto layoutBox = layoutQueue.takeLast();
            if (layoutBox->isOutOfFlowPositioned()) {
                m_isTextAndForcedLineBreakOnlyContent = false;
                inlineItemList.append({ layoutBox, InlineItem::Type::Opaque });
            } else if (layoutBox->isInlineTextBox()) {
                auto& inlineTextBox = downcast<InlineTextBox>(layoutBox);
                handleTextContent(inlineTextBox, inlineItemList, partialContentOffset(inlineTextBox));
            } else if (layoutBox->isAtomicInlineLevelBox() || layoutBox->isLineBreakBox())
                handleInlineLevelBox(layoutBox, inlineItemList);
            else if (layoutBox->isInlineBox())
                handleInlineBoxEnd(layoutBox, inlineItemList);
            else if (layoutBox->isFloatingPositioned()) {
                inlineItemList.append({ layoutBox, InlineItem::Type::Float });
                m_isTextAndForcedLineBreakOnlyContent = false;
            } else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox->nextSibling()) {
                layoutQueue.append(*nextSibling);
                break;
            }
        }
    }
}

static void replaceNonPreservedNewLineAndTabCharactersAndAppend(const InlineTextBox& inlineTextBox, StringBuilder& paragraphContentBuilder)
{
    // ubidi prefers non-preserved new lines/tabs as space characters.
    ASSERT(!TextUtil::shouldPreserveNewline(inlineTextBox));
    auto textContent = inlineTextBox.content();
    auto contentLength = textContent.length();
    auto needsUnicodeHandling = !textContent.is8Bit();
    size_t nonReplacedContentStartPosition = 0;
    for (size_t position = 0; position < contentLength;) {
        // Note that because of proper code point boundary handling (see U16_NEXT), position is incremented in an unconventional way here.
        auto startPosition = position;
        auto isNewLineOrTabCharacter = [&] {
            if (needsUnicodeHandling) {
                char32_t character;
                U16_NEXT(textContent.characters16(), position, contentLength, character);
                return character == newlineCharacter || character == tabCharacter;
            }
            auto isNewLineOrTab = textContent[position] == newlineCharacter || textContent[position] == tabCharacter;
            ++position;
            return isNewLineOrTab;
        };
        if (!isNewLineOrTabCharacter())
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

static inline void unwindBidiContextStack(StringBuilder& paragraphContentBuilder, BidiContextStack& bidiContextStack, const BidiContextStack& copyOfBidiStack, size_t& blockLevelBidiContextIndex)
{
    if (bidiContextStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    // Unwind all the way up to the block entry.
    size_t unwindingIndex = bidiContextStack.size() - 1;
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
}

static inline void rewindBidiContextStack(StringBuilder& paragraphContentBuilder, BidiContextStack& bidiContextStack, const BidiContextStack& copyOfBidiStack, size_t blockLevelBidiContextIndex)
{
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
}

using InlineItemOffsetList = Vector<std::optional<size_t>>;
static inline void handleBidiParagraphStart(StringBuilder& paragraphContentBuilder, InlineItemOffsetList& inlineItemOffsetList, BidiContextStack& bidiContextStack)
{
    // Bidi handling requires us to close all the nested bidi contexts at the end of the line triggered by forced line breaks
    // and re-open it for the content on the next line (i.e. paragraph handling).
    auto copyOfBidiStack = bidiContextStack;

    size_t blockLevelBidiContextIndex = 0;
    unwindBidiContextStack(paragraphContentBuilder, bidiContextStack, copyOfBidiStack, blockLevelBidiContextIndex);

    inlineItemOffsetList.append({ paragraphContentBuilder.length() });
    paragraphContentBuilder.append(newlineCharacter);

    rewindBidiContextStack(paragraphContentBuilder, bidiContextStack, copyOfBidiStack, blockLevelBidiContextIndex);
}

static inline void buildBidiParagraph(const RenderStyle& rootStyle, const InlineItemList& inlineItemList,  StringBuilder& paragraphContentBuilder, InlineItemOffsetList& inlineItemOffsetList)
{
    auto bidiContextStack = BidiContextStack { };
    handleEnterExitBidiContext(paragraphContentBuilder, rootStyle.unicodeBidi(), rootStyle.isLeftToRightDirection(), EnterExitType::EnteringBlock, bidiContextStack);
    if (rootStyle.rtlOrdering() != Order::Logical)
        handleEnterExitBidiContext(paragraphContentBuilder, UnicodeBidi::Override, rootStyle.isLeftToRightDirection(), EnterExitType::EnteringBlock, bidiContextStack);

    const Box* lastInlineTextBox = nullptr;
    size_t inlineTextBoxOffset = 0;
    for (size_t index = 0; index < inlineItemList.size(); ++index) {
        auto& inlineItem = inlineItemList[index];
        auto& layoutBox = inlineItem.layoutBox();
        auto mayAppendTextContentAsOneEntry = is<InlineTextBox>(layoutBox) && !TextUtil::shouldPreserveNewline(downcast<InlineTextBox>(layoutBox));

        if (inlineItem.isText() || inlineItem.isSoftLineBreak()) {
            if (mayAppendTextContentAsOneEntry) {
                // Append the entire InlineTextBox content and keep track of individual inline item positions as we process them.
                if (lastInlineTextBox != &layoutBox) {
                    inlineTextBoxOffset = paragraphContentBuilder.length();
                    replaceNonPreservedNewLineAndTabCharactersAndAppend(downcast<InlineTextBox>(layoutBox), paragraphContentBuilder);
                    lastInlineTextBox = &layoutBox;
                }
                inlineItemOffsetList.append({ inlineTextBoxOffset + (is<InlineTextItem>(inlineItem) ? downcast<InlineTextItem>(inlineItem).start() : downcast<InlineSoftLineBreakItem>(inlineItem).position()) });
            } else if (is<InlineTextItem>(inlineItem)) {
                inlineItemOffsetList.append({ paragraphContentBuilder.length() });
                auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
                paragraphContentBuilder.append(StringView(inlineTextItem.inlineTextBox().content()).substring(inlineTextItem.start(), inlineTextItem.length()));
            } else if (is<InlineSoftLineBreakItem>(inlineItem))
                handleBidiParagraphStart(paragraphContentBuilder, inlineItemOffsetList, bidiContextStack);
            else
                ASSERT_NOT_REACHED();
        } else if (inlineItem.isBox()) {
            inlineItemOffsetList.append({ paragraphContentBuilder.length() });
            paragraphContentBuilder.append(objectReplacementCharacter);
        } else if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
            auto& style = inlineItem.style();
            auto initiatesControlCharacter = style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal;
            if (!initiatesControlCharacter) {
                // Opaque items do not have position in the bidi paragraph. They inherit their bidi level from the next inline item.
                inlineItemOffsetList.append({ });
                continue;
            }
            inlineItemOffsetList.append({ paragraphContentBuilder.length() });
            auto isEnteringBidi = inlineItem.isInlineBoxStart();
            handleEnterExitBidiContext(paragraphContentBuilder
                , style.unicodeBidi()
                , style.isLeftToRightDirection()
                , isEnteringBidi ? EnterExitType::EnteringInlineBox : EnterExitType::ExitingInlineBox
                , bidiContextStack
            );
        } else if (inlineItem.isHardLineBreak())
            handleBidiParagraphStart(paragraphContentBuilder, inlineItemOffsetList, bidiContextStack);
        else if (inlineItem.isWordBreakOpportunity()) {
            // Soft wrap opportunity markers are opaque to bidi. 
            inlineItemOffsetList.append({ });
        } else if (inlineItem.isFloat()) {
            // Floats are not part of the inline content which make them opaque to bidi.
            inlineItemOffsetList.append({ });
        } else if (inlineItem.isOpaque()) {
            if (inlineItem.layoutBox().isOutOfFlowPositioned()) {
                // Out of flow boxes participate in inflow layout as if they were static positioned.
                inlineItemOffsetList.append({ paragraphContentBuilder.length() });
                paragraphContentBuilder.append(objectReplacementCharacter);
            } else {
                // truly opaque items are also opaque to bidi.
                inlineItemOffsetList.append({ });
            }
        } else
            ASSERT_NOT_IMPLEMENTED_YET();

    }
}

void InlineItemsBuilder::breakAndComputeBidiLevels(InlineItemList& inlineItemList)
{
    ASSERT(!inlineItemList.isEmpty());

    StringBuilder paragraphContentBuilder;
    InlineItemOffsetList inlineItemOffsets;
    inlineItemOffsets.reserveInitialCapacity(inlineItemList.size());
    buildBidiParagraph(root().style(), inlineItemList, paragraphContentBuilder, inlineItemOffsets);
    if (paragraphContentBuilder.isEmpty()) {
        // Style may trigger visual reordering even on a completely empty content.
        // e.g. <div><span style="direction:rtl"></span></div>
        // Let's not try to do bidi handling when there's no content to reorder.
        return;
    }
    auto mayNotUseBlockDirection = root().style().unicodeBidi() == UnicodeBidi::Plaintext;
    if (!contentRequiresVisualReordering() && mayNotUseBlockDirection && TextUtil::directionForTextContent(paragraphContentBuilder) == TextDirection::LTR) {
        // UnicodeBidi::Plaintext makes directionality calculated without taking parent direction property into account.
        return;
    }
    ASSERT(inlineItemOffsets.size() == inlineItemList.size());
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
                auto& inlineItem = inlineItemList[inlineItemIndex];
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
                    inlineItemList.insert(inlineItemIndex + 1, inlineTextItem.split(bidiEnd - *offset));
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
        inlineBoxContentFlagStack.reserveInitialCapacity(inlineItemList.size());
        for (auto index = inlineItemList.size(); index--;) {
            auto& inlineItem = inlineItemList[index];
            auto& style = inlineItem.style();
            auto initiatesControlCharacter = style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal;

            if (inlineItem.isInlineBoxStart()) {
                ASSERT(!inlineBoxContentFlagStack.isEmpty());
                if (inlineBoxContentFlagStack.takeLast() == InlineBoxHasContent::Yes) {
                    if (!initiatesControlCharacter)
                        inlineItemList[index].setBidiLevel(InlineItem::opaqueBidiLevel);
                }
                continue;
            }
            if (inlineItem.isInlineBoxEnd()) {
                inlineBoxContentFlagStack.append(InlineBoxHasContent::No);
                if (!initiatesControlCharacter)
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

static inline bool canCacheMeasuredWidthOnInlineTextItem(const InlineTextBox& inlineTextBox, bool isWhitespace)
{
    // Do not cache when:
    // 1. first-line style's unique font properties may produce non-matching width values.
    // 2. position dependent content is present (preserved tab character atm).
    if (&inlineTextBox.style() != &inlineTextBox.firstLineStyle() && inlineTextBox.style().fontCascade() != inlineTextBox.firstLineStyle().fontCascade())
        return false;
    if (!isWhitespace || !TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox))
        return true;
    return !inlineTextBox.hasPositionDependentContentWidth();
}

void InlineItemsBuilder::computeInlineTextItemWidths(InlineItemList& inlineItemList)
{
    for (auto& inlineItem : inlineItemList) {
        if (!inlineItem.isText())
            continue;

        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto& inlineTextBox = inlineTextItem.inlineTextBox();
        auto start = inlineTextItem.start();
        auto length = inlineTextItem.length();
        auto needsMeasuring = length && !inlineTextItem.isZeroWidthSpaceSeparator();
        if (!needsMeasuring || !canCacheMeasuredWidthOnInlineTextItem(inlineTextBox, inlineTextItem.isWhitespace()))
            continue;
        inlineTextItem.setWidth(TextUtil::width(inlineTextItem, inlineTextItem.style().fontCascade(), start, start + length, { }));
    }
}

void InlineItemsBuilder::handleTextContent(const InlineTextBox& inlineTextBox, InlineItemList& inlineItemList, std::optional<size_t> partialContentOffset)
{
    auto text = inlineTextBox.content();
    auto contentLength = text.length();
    if (!contentLength)
        return inlineItemList.append(InlineTextItem::createEmptyItem(inlineTextBox));

    if (inlineTextBox.isCombined())
        return inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, { }, contentLength, UBIDI_DEFAULT_LTR, false, { }));

    m_contentRequiresVisualReordering = m_contentRequiresVisualReordering || requiresVisualReordering(inlineTextBox);
    auto& style = inlineTextBox.style();
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { text, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()), TextUtil::contentAnalysis(style.wordBreak()) };
    auto currentPosition = partialContentOffset.value_or(0lu);
    ASSERT(currentPosition <= contentLength);

    while (currentPosition < contentLength) {
        auto handleSegmentBreak = [&] {
            // Segment breaks with preserve new line style (white-space: pre, pre-wrap, break-spaces and pre-line) compute to forced line break.
            auto isSegmentBreakCandidate = text[currentPosition] == newlineCharacter;
            if (!isSegmentBreakCandidate || !shouldPreserveNewline)
                return false;
            inlineItemList.append(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, currentPosition++));
            return true;
        };
        if (handleSegmentBreak())
            continue;

        auto handleWhitespace = [&] {
            auto stopAtWordSeparatorBoundary = shouldPreserveSpacesAndTabs && style.fontCascade().wordSpacing();
            auto whitespaceContent = text.is8Bit() ?
                moveToNextNonWhitespacePosition(text.span8(), currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, stopAtWordSeparatorBoundary) :
                moveToNextNonWhitespacePosition(text.span16(), currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, stopAtWordSeparatorBoundary);
            if (!whitespaceContent)
                return false;

            ASSERT(whitespaceContent->length);
            if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces) {
                // https://www.w3.org/TR/css-text-3/#white-space-phase-1
                // For break-spaces, a soft wrap opportunity exists after every space and every tab.
                // FIXME: if this turns out to be a perf hit with too many individual whitespace inline items, we should transition this logic to line breaking.
                for (size_t i = 0; i < whitespaceContent->length; ++i)
                    inlineItemList.append(InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition + i, 1, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, { }));
            } else
                inlineItemList.append(InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition, whitespaceContent->length, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, { }));
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
                inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, index, 1, UBIDI_DEFAULT_LTR, { }, { }));
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
                    endPosition += moveToNextBreakablePosition(endPosition, lineBreakIteratorFactory, style);
                    ASSERT(startPosition < endPosition);
                } while (endPosition < contentLength && text[endPosition - 1] == softHyphen);
            } else {
                endPosition += moveToNextBreakablePosition(startPosition, lineBreakIteratorFactory, style);
                ASSERT(startPosition < endPosition);
                hasTrailingSoftHyphen = text[endPosition - 1] == softHyphen;
            }
            ASSERT_IMPLIES(style.hyphens() == Hyphens::None, !hasTrailingSoftHyphen);
            inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, endPosition - startPosition, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen, { }));
            currentPosition = endPosition;
            return true;
        };
        if (handleNonWhitespace())
            continue;
        // Unsupported content?
        ASSERT_NOT_REACHED();
    }
}

void InlineItemsBuilder::handleInlineBoxStart(const Box& inlineBox, InlineItemList& inlineItemList)
{
    inlineItemList.append({ inlineBox, InlineItem::Type::InlineBoxStart });
    m_contentRequiresVisualReordering = m_contentRequiresVisualReordering || requiresVisualReordering(inlineBox);
    ++m_inlineBoxCount;
}

void InlineItemsBuilder::handleInlineBoxEnd(const Box& inlineBox, InlineItemList& inlineItemList)
{
    inlineItemList.append({ inlineBox, InlineItem::Type::InlineBoxEnd });
    // Inline box end item itself can not trigger bidi content.
    ASSERT(m_inlineBoxCount);
    ASSERT(contentRequiresVisualReordering() || inlineBox.style().isLeftToRightDirection() || inlineBox.style().rtlOrdering() == Order::Visual || inlineBox.style().unicodeBidi() == UnicodeBidi::Normal);
}

void InlineItemsBuilder::handleInlineLevelBox(const Box& layoutBox, InlineItemList& inlineItemList)
{
    if (layoutBox.isRubyAnnotationBox())
        return;

    if (layoutBox.isAtomicInlineLevelBox()) {
        m_isTextAndForcedLineBreakOnlyContent = false;
        return inlineItemList.append({ layoutBox, InlineItem::Type::Box });
    }

    if (layoutBox.isLineBreakBox()) {
        m_isTextAndForcedLineBreakOnlyContent = m_isTextAndForcedLineBreakOnlyContent && isTextOrLineBreak(layoutBox);
        return inlineItemList.append({ layoutBox, layoutBox.isWordBreakOpportunity() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });
    }

    ASSERT_NOT_REACHED();
}

}
}


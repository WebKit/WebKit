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
#include "FontCascadeCache.h"
#include "FontCascadeFonts.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingUtils.h"
#include "FontCascadeInlines.h"
#include "InlineSoftLineBreakItem.h"
#include "LayoutBoxInlines.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleResolver.h"
#include "TextBreakingPositionCache.h"
#include "TextUtil.h"
#include "UnicodeBidi.h"
#include "platform/text/TextSpacing.h"
#include <wtf/BitVector.h>
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
    while (nextNonWhiteSpacePosition < characters.size() && isWhitespaceCharacter(characters[nextNonWhiteSpacePosition])) {
        if (stopAtWordSeparatorBoundary && hasWordSeparatorCharacter && !isWordSeparatorCharacter) [[unlikely]]
            break;
        ++nextNonWhiteSpacePosition;
    }
    return nextNonWhiteSpacePosition == startPosition ? std::nullopt : std::make_optional(WhitespaceContent { nextNonWhiteSpacePosition - startPosition, hasWordSeparatorCharacter });
}

static unsigned moveToNextBreakablePosition(unsigned startPosition, CachedLineBreakIteratorFactory& lineBreakIteratorFactory, const Style::ComputedStyle& style)
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

NODELETE static inline bool isTextOrLineBreak(const Box& layoutBox)
{
    return layoutBox.isInFlow() && (layoutBox.isInlineTextBox() || (layoutBox.isLineBreakBox() && !layoutBox.isWordBreakOpportunity()));
}

NODELETE static inline bool isTextWithStyleOfItsOwn(const Box& layoutBox)
{
    auto* inlineTextBox = dynamicDowncast<InlineTextBox>(layoutBox);
    return inlineTextBox && inlineTextBox->inheritsStyleFromDisplayContentsParent();
}

struct WidthAndGlyphOverflow {
    InlineLayoutUnit width { 0.f };
    std::optional<std::pair<LayoutUnit, LayoutUnit>> topBottomOverflow { };
};
static inline WidthAndGlyphOverflow nonWhitespaceContentWidth(const auto& inlineTextBox, auto startPosition, auto endPosition, auto mayHaveGlyphOverflow)
{
    CheckedRef fontCascade = inlineTextBox.style().fontCascade();
    if (!mayHaveGlyphOverflow)
        return { TextUtil::width(inlineTextBox, fontCascade, startPosition, endPosition, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::Yes), { } };

    // We don't need glyph overflow until after display boxes are created, but walking the content again is a major performance hit. See InlineDisplayContentBuilder::appendTextDisplayBox's addGlyphOverflow.
    GlyphOverflow glyphOverflow;
    glyphOverflow.computeBounds = true;
    auto width = TextUtil::width(inlineTextBox, fontCascade, startPosition, endPosition, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::Yes, { }, &glyphOverflow);

    auto& fontMetrics = fontCascade->metricsOfPrimaryFont();
    glyphOverflow.top = std::max(0.f, glyphOverflow.top - fontMetrics.ascent(FontBaseline::Alphabetic));
    glyphOverflow.bottom = std::max(0.f, glyphOverflow.bottom - fontMetrics.descent(FontBaseline::Alphabetic));
    return { width, std::pair<LayoutUnit, LayoutUnit> { std::clamp(glyphOverflow.top, 0_lu, 31_lu), std::clamp(glyphOverflow.bottom, 0_lu, 7_lu) } };
}

InlineItemsBuilder::InlineItemsBuilder(InlineContentCache& inlineContentCache, const ElementBox& root, const SecurityOrigin& securityOrigin)
    : m_inlineContentCache(inlineContentCache)
    , m_root(root)
    , m_securityOrigin(securityOrigin)
{
}

void InlineItemsBuilder::build(InlineItemPosition startPosition)
{
    InlineItemList inlineItemList;
    collectInlineItems(inlineItemList, startPosition);

    if (m_hasWhiteSpaceTrim)
        adjustInlineItemsForWhiteSpaceTrim(inlineItemList);

    if (root().writingMode().isBidiRTL() || contentRequiresVisualReordering()) {
        // FIXME: Add support for partial, yet paragraph level bidi content handling.
        breakAndComputeBidiLevels(inlineItemList);
    }

    if (contentRequiresVisualReordering() || m_hasTextAutospace)
        computeInlineTextItemWidthsAndTextSpacing(inlineItemList);

    auto adjustInlineContentCacheWithNewInlineItems = [&] {
        ASSERT(!startPosition || startPosition.index < inlineContentCache().inlineItems().content().size());

        auto contentAttributes = InlineContentCache::InlineItems::ContentAttributes { m_contentRequiresVisualReordering, m_hasTextAndLineBreakOnlyContent, m_hasTextWithStyleOfItsOwn, m_hasTextAutospace, m_hasWhiteSpaceTrim, m_inlineBoxCount };
        auto isPopulatedFromCache = m_textContentPopulatedFromCache && *m_textContentPopulatedFromCache ? InlineContentCache::InlineItems::IsPopulatedFromCache::Yes : InlineContentCache::InlineItems::IsPopulatedFromCache::No;
        auto& inlineItemCache = inlineContentCache().inlineItems();
        if (!startPosition || startPosition.index >= inlineItemCache.content().size())
            return inlineItemCache.set(WTF::move(inlineItemList), contentAttributes, isPopulatedFromCache);
        inlineItemCache.replace(startPosition.index, WTF::move(inlineItemList), contentAttributes, isPopulatedFromCache);
    };
    adjustInlineContentCacheWithNewInlineItems();

#if ASSERT_ENABLED
    // Check if we've got matching inline box start/end pairs and unique inline level items (non-text, non-inline box items).
    size_t inlineBoxStart = 0;
    size_t inlineBoxEnd = 0;
    auto hasTextAndLineBreakOnlyContent = true;
    auto hasTextWithStyleOfItsOwn = false;
    auto inlineLevelItems = HashSet<const Box*> { };
    for (auto& inlineItem : inlineContentCache().inlineItems().content()) {
        if (inlineItem.isInlineBoxStart()) {
            ++inlineBoxStart;
            continue;
        }
        if (inlineItem.isInlineBoxEnd()) {
            ++inlineBoxEnd;
            continue;
        }
        auto hasToBeUniqueLayoutBox = inlineItem.isAtomicInlineBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak();
        if (hasToBeUniqueLayoutBox)
            ASSERT(inlineLevelItems.add(&inlineItem.layoutBox()).isNewEntry);
        hasTextAndLineBreakOnlyContent &= isTextOrLineBreak(inlineItem.layoutBox());
        hasTextWithStyleOfItsOwn |= isTextWithStyleOfItsOwn(inlineItem.layoutBox());
    }
    ASSERT(inlineBoxStart == inlineBoxEnd);
    ASSERT(m_inlineBoxCount == inlineBoxStart);
    ASSERT(m_hasTextAndLineBreakOnlyContent == hasTextAndLineBreakOnlyContent);
    // 'white-space-trim' may discard every item of a text box, which leaves the flag computed from the boxes set.
    ASSERT(m_hasTextWithStyleOfItsOwn == hasTextWithStyleOfItsOwn || (m_hasWhiteSpaceTrim && m_hasTextWithStyleOfItsOwn));
#endif
}

NODELETE static bool isNonReplacedInlineBlock(const Box& layoutBox)
{
    // inline-block (InlineFlowRoot) is the inline-level block container 'white-space-trim' applies to; it already
    // implies isAtomicInlineBox(). A replaced box or iframe may carry InlineFlowRoot display but is not a block container.
    return layoutBox.isInlineBlockBox() && !layoutBox.isReplacedBox() && !layoutBox.isIFrame();
}

NODELETE static bool isDocumentWhiteSpace(const InlineItem& item)
{
    // https://drafts.csswg.org/css-text-4/#document-white-space-characters
    // Spaces, tabs and segment breaks, preserved or not.
    if (auto* textItem = dynamicDowncast<InlineTextItem>(item))
        return textItem->isWhitespace();
    auto* softLineBreakItem = dynamicDowncast<InlineSoftLineBreakItem>(item);
    return softLineBreakItem && softLineBreakItem->isSegmentBreak();
}

NODELETE static bool isPreservedWhiteSpace(const InlineItem& documentWhiteSpaceItem)
{
    ASSERT(isDocumentWhiteSpace(documentWhiteSpaceItem));
    // A preserved segment break is a soft line break item.
    auto* textItem = dynamicDowncast<InlineTextItem>(documentWhiteSpaceItem);
    return !textItem || InlineTextItem::shouldPreserveSpacesAndTabs(*textItem);
}

enum class TrimDirection : bool { Forward, Backward };

static void collectWhiteSpaceToDiscardAfterEdges(const InlineItemList& inlineItemList, TrimDirection direction, BitVector& itemsToDiscard)
{
    // Collects the white space that follows a box edge in the direction of travel:
    //  - Walking forward: the white space after a discard-after box (an inline box end, or an inline-block)
    //    and the white space at the start of a discard-inner inline box's content.
    //  - Walking backward: the white space before a discard-before box (an inline box start, or an
    //    inline-block) and the white space at the end of a discard-inner inline box's content.
    // 'discard-inner' only reaches the element's own content: it stops at the element's far edge, even when
    // the element contains nothing but white space.
    //
    // When walking forward we enter an inline box through its start and leave it through its end; walking
    // backward it is the other way around. The value that trims past the edge we leave a box through is
    // discard-after (forward) or discard-before (backward).
    auto enteringEdge = direction == TrimDirection::Forward ? InlineItem::Type::InlineBoxStart : InlineItem::Type::InlineBoxEnd;
    auto discardOutside = direction == TrimDirection::Forward ? Style::WhiteSpaceTrimValue::DiscardAfter : Style::WhiteSpaceTrimValue::DiscardBefore;
    // Set by a discard-outside edge we just left; trims up to the next piece of content.
    bool isTrimmingOutsideWhiteSpace = false;
    // The number of open discard-inner inline boxes with no content so far. Content resets it, so these are
    // always the innermost open discard-inner boxes: when it is non-zero, the next one to close is among them.
    size_t discardInnerBoxesWithoutContent = 0;

    auto itemCount = inlineItemList.size();
    for (size_t step = 0; step < itemCount; ++step) {
        auto index = direction == TrimDirection::Forward ? step : itemCount - 1 - step;
        auto& item = inlineItemList[index];
        if (isDocumentWhiteSpace(item)) {
            auto isPreserved = isPreservedWhiteSpace(item);
            if (discardInnerBoxesWithoutContent || (isTrimmingOutsideWhiteSpace && !isPreserved)) {
                itemsToDiscard.quickSet(index);
                continue;
            }
            // discard-before / discard-after don't reach across preserved white space.
            isTrimmingOutsideWhiteSpace &= !isPreserved;
            continue;
        }
        if (item.isInlineBoxStart() || item.isInlineBoxEnd()) {
            // Inline box boundaries are transparent: carry the state across them.
            auto whiteSpaceTrim = item.layoutBox().style().whiteSpaceTrim();
            auto isDiscardInner = whiteSpaceTrim.contains(Style::WhiteSpaceTrimValue::DiscardInner);
            if (item.type() == enteringEdge) {
                discardInnerBoxesWithoutContent += isDiscardInner;
                continue;
            }
            if (isDiscardInner && discardInnerBoxesWithoutContent)
                --discardInnerBoxesWithoutContent;
            isTrimmingOutsideWhiteSpace |= whiteSpaceTrim.contains(discardOutside);
            continue;
        }
        if (item.isFloat() || item.isOutOfFlow()) {
            // Floats and out-of-flow boxes are not in-flow content; like white space collapsing they are
            // transparent to this adjacency and neither start nor terminate a discardable white space run.
            continue;
        }
        // Any other item is content and ends both adjacencies. An inline-block is a block container
        // 'white-space-trim' applies to: it is content too, but it can still trim the non-preserved white
        // space past its own edge.
        discardInnerBoxesWithoutContent = 0;
        isTrimmingOutsideWhiteSpace = isNonReplacedInlineBlock(item.layoutBox()) && item.layoutBox().style().whiteSpaceTrim().contains(discardOutside);
    }
}

void InlineItemsBuilder::adjustInlineItemsForWhiteSpaceTrim(InlineItemList& inlineItemList)
{
    // https://drafts.csswg.org/css-text-4/#white-space-trim
    // 'discard-before' / 'discard-after' remove the non-preserved white space immediately before the start /
    // after the end of an element; 'discard-inner' removes all white space at the start and end of the
    // element's own content (i.e. immediately inside its edges), preserved or not. Discarding the
    // corresponding inline items here -before line building, bidi resolution and intrinsic sizing consume
    // them- keeps the removed white space out of every downstream computation (rendering, soft wrap
    // opportunities and min/max-content) at once. Inline box boundaries are transparent to this adjacency,
    // matching white space collapsing.
    ASSERT(m_hasWhiteSpaceTrim);

    BitVector itemsToDiscard(inlineItemList.size());
    collectWhiteSpaceToDiscardAfterEdges(inlineItemList, TrimDirection::Forward, itemsToDiscard);
    collectWhiteSpaceToDiscardAfterEdges(inlineItemList, TrimDirection::Backward, itemsToDiscard);
    if (itemsToDiscard.isEmpty())
        return;

    InlineItemList remainingItems(ReserveCapacityTag { }, inlineItemList.size());
    for (size_t index = 0; index < inlineItemList.size(); ++index) {
        if (!itemsToDiscard.quickGet(index))
            remainingItems.append(WTF::move(inlineItemList[index]));
    }
    inlineItemList = WTF::move(remainingItems);
}

void InlineItemsBuilder::computeInlineBoxBoundaryTextSpacings(const InlineItemList& inlineItemList)
{
    ASSERT(m_hasTextAutospace);
    char32_t lastCharacterFromPreviousRun = 0;
    size_t lastCharacterDepth = 0;
    size_t currentCharacterDepth = 0;
    InlineBoxBoundaryTextSpacings spacings;
    Vector<unsigned> inlineBoxStartIndexesOnInlineItemsList;
    bool processInlineBoxBoundary = false;
    for (unsigned inlineItemIndex = 0; inlineItemIndex < inlineItemList.size(); ++inlineItemIndex) {
        auto& inlineItem = inlineItemList[inlineItemIndex];
        if (inlineItem.isInlineBoxStart()) {
            inlineBoxStartIndexesOnInlineItemsList.append(inlineItemIndex);
            ++currentCharacterDepth;
            continue;
        }
        if (inlineItem.isInlineBoxEnd()) {
            if (!currentCharacterDepth--) {
                ASSERT_NOT_REACHED();
                // Skip unbalanced inline box start/end pairs.
                processInlineBoxBoundary = false;
                currentCharacterDepth = 0;
                continue;
            }
            processInlineBoxBoundary = true;
            continue;
        }
        auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem);
        if (!inlineTextItem)
            continue;

        auto start = inlineTextItem->start();
        auto length = inlineTextItem->length();
        CheckedRef inlineTextBox = inlineTextItem->inlineTextBox();
        auto content = StringView { inlineTextBox->content() }.substring(start, length);
        if (!processInlineBoxBoundary || !lastCharacterFromPreviousRun) {
            lastCharacterFromPreviousRun = TextUtil::lastBaseCharacterFromText(content);
            lastCharacterDepth = currentCharacterDepth;
            processInlineBoxBoundary = false;
            continue;
        }

        size_t boundaryDepth = std::min(currentCharacterDepth, lastCharacterDepth);
        size_t inlineBoxStartOnBoundaryIndex = inlineBoxStartIndexesOnInlineItemsList.size() - 1 - (currentCharacterDepth - boundaryDepth);
        size_t boundaryIndex = inlineBoxStartIndexesOnInlineItemsList[inlineBoxStartOnBoundaryIndex];
        CheckedRef boundaryOwnerStyle = inlineItemList[boundaryIndex].layoutBox().parent().style();
        auto boundaryTextAutospace = boundaryOwnerStyle->textAutospace();
        if (!boundaryTextAutospace.isNoAutospace() && boundaryTextAutospace.shouldApplySpacing(inlineTextBox->content().codePointAt(start), lastCharacterFromPreviousRun))
            spacings.add(boundaryIndex, TextAutospace::textAutospaceSize(protect(boundaryOwnerStyle->fontCascade())->primaryFont()));

        lastCharacterFromPreviousRun = TextUtil::lastBaseCharacterFromText(content);
        lastCharacterDepth = currentCharacterDepth;
        processInlineBoxBoundary = false;
    }
    if (!spacings.isEmpty())
        inlineContentCache().setInlineBoxBoundaryTextSpacings(WTF::move(spacings));
}

NODELETE static bool requiresVisualReordering(const Box& layoutBox)
{
    if (auto* inlineTextBox = dynamicDowncast<InlineTextBox>(layoutBox))
        return inlineTextBox->hasStrongDirectionalityContent();
    if ((layoutBox.isInlineBox() || layoutBox.isLineBreakBox()) && layoutBox.isInFlow()) {
        auto& style = layoutBox.style();
        return style.writingMode().isBidiRTL() || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal);
    }
    return false;
}

NODELETE static inline bool isInlineBoxWithInlineContent(const Box& layoutBox)
{
    return layoutBox.isInlineBox() && !layoutBox.isLineBreakBox() && !layoutBox.isOutOfFlowPositioned();
}

InlineItemsBuilder::LayoutQueue InlineItemsBuilder::traverseUntilDamaged(const Box& firstDamagedLayoutBox)
{
    LayoutQueue queue;

    auto appendAndCheckForDamage = [&] (auto& layoutBox) {
        queue.append(layoutBox);
        m_contentRequiresVisualReordering = m_contentRequiresVisualReordering || requiresVisualReordering(layoutBox);
        m_hasTextAndLineBreakOnlyContent &= (isInlineBoxWithInlineContent(layoutBox) || isTextOrLineBreak(layoutBox));
        m_hasTextWithStyleOfItsOwn |= isTextWithStyleOfItsOwn(layoutBox);
        return &layoutBox == &firstDamagedLayoutBox;
    };

    if (appendAndCheckForDamage(*protect(root().firstChild())))
        return queue;

    while (!queue.isEmpty()) {
        while (true) {
            auto layoutBox = queue.last();
            if (!isInlineBoxWithInlineContent(layoutBox))
                break;
            ++m_inlineBoxCount;
            CheckedPtr firstChild = downcast<ElementBox>(layoutBox).firstChild();
            if (!firstChild)
                break;
            if (appendAndCheckForDamage(*firstChild))
                return queue;
        }

        while (!queue.isEmpty()) {
            if (CheckedPtr nextSibling = queue.takeLast()->nextSibling()) {
                if (appendAndCheckForDamage(*nextSibling))
                    return queue;
                break;
            }
        }
    }
    // How did we miss the damaged box?
    ASSERT_NOT_REACHED();
    queue.append(*protect(root().firstChild()));
    return queue;
}

// Text length of the given text box and all content after it.
static size_t textLengthFromTextBoxToEnd(const InlineTextBox& inlineTextBox, std::span<const CheckedRef<const Box>> enclosingInlineBoxes)
{
    size_t textLength = inlineTextBox.content().length();
    Vector<CheckedRef<const Box>, 16> boxesToVisit;
    auto appendNextSibling = [&](const Box& box) {
        if (CheckedPtr nextSibling = box.nextSibling())
            boxesToVisit.append(*nextSibling);
    };
    appendNextSibling(inlineTextBox);
    for (auto& enclosingInlineBox : enclosingInlineBoxes)
        appendNextSibling(enclosingInlineBox);

    while (!boxesToVisit.isEmpty()) {
        auto box = boxesToVisit.takeLast();
        appendNextSibling(box);
        if (CheckedPtr textBox = dynamicDowncast<InlineTextBox>(box))
            textLength += textBox->content().length();
        else if (isInlineBoxWithInlineContent(box)) {
            if (CheckedPtr firstChild = downcast<ElementBox>(box).firstChild())
                boxesToVisit.append(*firstChild);
        }
    }
    return textLength;
}

InlineItemsBuilder::LayoutQueue InlineItemsBuilder::initializeLayoutQueue(InlineItemPosition startPosition)
{
    CheckedRef root = this->root();
    if (!root->firstChild()) {
        // A list item's outside marker is not in the box tree and takes no part in the line, so a list item with
        // nothing else in it has no inflow child at all while still needing the line the marker sits on
        // (see InlineFormattingContext::layout). Nothing else establishes a childless inline formatting context.
        ASSERT(root->isListItem());
        return { };
    }

    if (!startPosition)
        return { *root->firstChild() };

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
        return { *root->firstChild() };
    }

    CheckedRef firstDamagedLayoutBox = existingInlineItems[startPosition.index].layoutBox();
    return traverseUntilDamaged(firstDamagedLayoutBox.get());
}

void InlineItemsBuilder::collectInlineItems(InlineItemList& inlineItemList, InlineItemPosition startPosition)
{
    // Traverse the tree and create inline items out of inline boxes and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
    auto layoutQueue = initializeLayoutQueue(startPosition);
    auto& inlineItemCache = this->inlineContentCache().inlineItems();

    // Large content would otherwise grow the list in many steps that each copy it. Reserve once at the rate measured so far, with room for the cached items the list is merged into.
    constexpr size_t minimumTextLengthForPresizing = 64 * 1024;
    auto initialItemCount = inlineItemList.size();
    size_t processedTextLength = 0;
    bool hasPresized = false;
    auto presizeForRemainingContentIfNeeded = [&](const InlineTextBox& inlineTextBox) {
        if (hasPresized || processedTextLength < minimumTextLengthForPresizing)
            return;
        hasPresized = true;
        auto itemsPerCharacter = static_cast<double>(inlineItemList.size() - initialItemCount) / processedTextLength;
        auto remainingItemCount = static_cast<size_t>(textLengthFromTextBoxToEnd(inlineTextBox, layoutQueue.span()) * itemsPerCharacter * 1.1);
        auto cachedItemCount = startPosition ? startPosition.index + 1 : 0;
        inlineItemList.reserveCapacity(inlineItemList.size() + remainingItemCount + cachedItemCount);
    };

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
        if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(damagedInlineItem))
            return inlineTextItem->start();
        if (auto* inlineSoftLineBreakItem = dynamicDowncast<InlineSoftLineBreakItem>(damagedInlineItem))
            return inlineSoftLineBreakItem->position();
        ASSERT_NOT_REACHED();
        return { };
    };

    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto layoutBox = layoutQueue.last();
            if (!isInlineBoxWithInlineContent(layoutBox))
                break;
            // This is the start of an inline box (e.g. <span>).
            handleInlineBoxStart(layoutBox, inlineItemList);
            CheckedRef inlineBox = downcast<ElementBox>(layoutBox);
            if (!inlineBox->hasChild())
                break;
            layoutQueue.append(*protect(inlineBox->firstChild()));
        }

        while (!layoutQueue.isEmpty()) {
            auto layoutBox = layoutQueue.takeLast();
            m_hasTextAndLineBreakOnlyContent &= (isInlineBoxWithInlineContent(layoutBox) || isTextOrLineBreak(layoutBox));
            m_hasTextWithStyleOfItsOwn |= isTextWithStyleOfItsOwn(layoutBox);
            if (layoutBox->isOutOfFlowPositioned())
                inlineItemList.append({ layoutBox, InlineItem::Type::OutOfFlow });
            else if (CheckedPtr inlineTextBox = dynamicDowncast<InlineTextBox>(layoutBox)) {
                presizeForRemainingContentIfNeeded(*inlineTextBox);
                auto contentOffset = partialContentOffset(*inlineTextBox);
                handleTextContent(*inlineTextBox, inlineItemList, contentOffset);
                processedTextLength += inlineTextBox->content().length() - contentOffset.value_or(0);
            }
            else if (layoutBox->isAtomicInlineBox() || layoutBox->isLineBreakBox())
                handleInlineLevelBox(layoutBox, inlineItemList);
            else if (layoutBox->isInlineBox())
                handleInlineBoxEnd(layoutBox, inlineItemList);
            else if (layoutBox->isFloatingPositioned())
                inlineItemList.append({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox->isBlockLevelBox())
                inlineItemList.append({ layoutBox, InlineItem::Type::Block });
            else
                ASSERT_NOT_REACHED();

            if (CheckedPtr nextSibling = layoutBox->nextSibling()) {
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
                auto characters = textContent.span16();
                U16_NEXT(characters, position, contentLength, character);
                return character == newlineCharacter || character == tabCharacter || character == paragraphSeparator;
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
    if (enterExitType == EnterExitType::ExitingInlineBox && bidiContextStack.size() == 1) {
        // Refuse to pop the initial block entry off of the stack. It indicates unbalanced InlineBoxStart/End pairs.
        ASSERT_NOT_REACHED();
        return;
    }

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
    if (copyOfBidiStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

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

static inline void buildBidiParagraph(const Style::ComputedStyle& rootStyle, const InlineItemList& inlineItemList,  StringBuilder& paragraphContentBuilder, InlineItemOffsetList& inlineItemOffsetList)
{
    auto bidiContextStack = BidiContextStack { };
    handleEnterExitBidiContext(paragraphContentBuilder, rootStyle.unicodeBidi(), rootStyle.writingMode().isBidiLTR(), EnterExitType::EnteringBlock, bidiContextStack);
    if (rootStyle.rtlOrdering() != Order::Logical)
        handleEnterExitBidiContext(paragraphContentBuilder, UnicodeBidi::Override, rootStyle.writingMode().isBidiLTR(), EnterExitType::EnteringBlock, bidiContextStack);

    CheckedPtr<const Box> lastInlineTextBox = nullptr;
    size_t inlineTextBoxOffset = 0;
    for (size_t index = 0; index < inlineItemList.size(); ++index) {
        auto& inlineItem = inlineItemList[index];
        CheckedRef layoutBox = inlineItem.layoutBox();

        auto isBidiParagraphSeparator = [&] {
            // Bidi paragraph separators (hard line breaks, block-level items, and soft line breaks
            // at characters with bidi class B like U+000A and U+2029) need handleBidiParagraphStart
            // to unwind/rewind the bidi context stack. U+2028 LINE SEPARATOR has bidi class WS
            // and does not reset the paragraph context.
            if (inlineItem.isHardLineBreak() || inlineItem.isBlock())
                return true;
            auto* softLineBreak = dynamicDowncast<InlineSoftLineBreakItem>(inlineItem);
            return softLineBreak && softLineBreak->isBidiParagraphStart();
        };
        if (isBidiParagraphSeparator())
            handleBidiParagraphStart(paragraphContentBuilder, inlineItemOffsetList, bidiContextStack);
        else if (inlineItem.isText() || inlineItem.isSoftLineBreak()) {
            CheckedPtr inlineTextBox = dynamicDowncast<InlineTextBox>(layoutBox);
            auto mayAppendTextContentAsOneEntry = inlineTextBox && !TextUtil::shouldPreserveNewline(*inlineTextBox);
            if (mayAppendTextContentAsOneEntry) {
                // Append the entire InlineTextBox content and keep track of individual inline item positions as we process them.
                if (lastInlineTextBox != layoutBox.ptr()) {
                    inlineTextBoxOffset = paragraphContentBuilder.length();
                    replaceNonPreservedNewLineAndTabCharactersAndAppend(*inlineTextBox, paragraphContentBuilder);
                    lastInlineTextBox = layoutBox.ptr();
                }
                auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem);
                inlineItemOffsetList.append({ inlineTextBoxOffset + (inlineTextItem ? inlineTextItem->start() : downcast<InlineSoftLineBreakItem>(inlineItem).position()) });
            } else if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
                inlineItemOffsetList.append({ paragraphContentBuilder.length() });
                paragraphContentBuilder.append(inlineTextItem->content());
            } else if (auto* softLineBreak = dynamicDowncast<InlineSoftLineBreakItem>(inlineItem)) {
                // Non-paragraph-separator soft breaks (e.g., U+2028) in per-item mode just record their offset.
                inlineItemOffsetList.append({ paragraphContentBuilder.length() });
                paragraphContentBuilder.append(softLineBreak->character());
            } else
                ASSERT_NOT_REACHED();
        } else if (inlineItem.isAtomicInlineBox()) {
            inlineItemOffsetList.append({ paragraphContentBuilder.length() });
            paragraphContentBuilder.append(objectReplacementCharacter);
        } else if (inlineItem.isInlineBoxStartOrEnd()) {
            // https://drafts.csswg.org/css-writing-modes/#unicode-bidi
            CheckedRef style = inlineItem.style();
            auto initiatesControlCharacter = style->rtlOrdering() == Order::Logical && style->unicodeBidi() != UnicodeBidi::Normal;
            if (!initiatesControlCharacter) {
                // Opaque items do not have position in the bidi paragraph. They inherit their bidi level from the next inline item.
                inlineItemOffsetList.append({ });
                continue;
            }
            inlineItemOffsetList.append({ paragraphContentBuilder.length() });
            auto isEnteringBidi = inlineItem.isInlineBoxStart();
            handleEnterExitBidiContext(paragraphContentBuilder
                , style->unicodeBidi()
                , style->writingMode().isBidiLTR()
                , isEnteringBidi ? EnterExitType::EnteringInlineBox : EnterExitType::ExitingInlineBox
                , bidiContextStack
            );
        } else if (inlineItem.isWordBreakOpportunity()) {
            // Soft wrap opportunity markers are opaque to bidi. 
            inlineItemOffsetList.append({ });
        } else if (inlineItem.isFloat()) {
            // Floats are not part of the inline content which make them opaque to bidi.
            inlineItemOffsetList.append({ });
        } else if (inlineItem.isOutOfFlow()) {
            if (inlineItem.layoutBox().isOutOfFlowPositioned()) {
                // Out of flow boxes participate in inflow layout as if they were static positioned.
                inlineItemOffsetList.append({ paragraphContentBuilder.length() });
                paragraphContentBuilder.append(objectReplacementCharacter);
            } else {
                // Out-of-flow items are also opaque to bidi.
                inlineItemOffsetList.append({ });
            }
        } else
            ASSERT_NOT_IMPLEMENTED_YET();

    }
}

void InlineItemsBuilder::breakAndComputeBidiLevels(InlineItemList& inlineItemList)
{
    // A list item with nothing but its outside marker has no inline items at all, the marker not being in the box
    // tree, and still needs the line the marker sits on (see InlineFormattingContext::layout).
    ASSERT(!inlineItemList.isEmpty() || root().isListItem());

    StringBuilder paragraphContentBuilder;
    InlineItemOffsetList inlineItemOffsets(ReserveCapacityTag { }, inlineItemList.size());
    buildBidiParagraph(protect(root().style()), inlineItemList, paragraphContentBuilder, inlineItemOffsets);
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
        rootBidiLevel = root().writingMode().isBidiLTR() ? UBIDI_LTR : UBIDI_RTL;

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
            // We should always have inline item(s) associated with a bidi range, unless 'white-space-trim' discarded
            // them (the paragraph has the entire content of the text box).
            ASSERT(inlineItemIndex < inlineItemOffsets.size() || m_hasWhiteSpaceTrim);
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
                auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem);
                if (!inlineTextItem)
                    continue;
                // Check if this text item is on bidi boundary and needs splitting.
                auto endPosition = *offset + inlineTextItem->length();
                if (endPosition > bidiEnd) {
                    inlineItemList.insert(inlineItemIndex + 1, inlineTextItem->split(bidiEnd - *offset));
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
        Vector<InlineBoxHasContent> inlineBoxContentFlagStack(ReserveCapacityTag { }, inlineItemList.size());
        for (auto index = inlineItemList.size(); index--;) {
            auto& inlineItem = inlineItemList[index];
            CheckedRef style = inlineItem.style();
            auto initiatesControlCharacter = style->rtlOrdering() == Order::Logical && style->unicodeBidi() != UnicodeBidi::Normal;

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

            auto isContentfulInlineItem = [&] {
                if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem))
                    return !inlineTextItem->isWhitespace() || TextUtil::shouldPreserveSpacesAndTabs(inlineTextItem->layoutBox());
                return inlineItem.isAtomicInlineBox() || inlineItem.isLineBreak() || (inlineItem.isOutOfFlow() && inlineItem.layoutBox().isOutOfFlowPositioned());
            };
            if (isContentfulInlineItem()) {
                // Mark the inline box stack with "content yes", when we come across a content type of inline item
                // so that we can mark the inline box as opaque and let the content drive visual ordering.
                inlineBoxContentFlagStack.fill(InlineBoxHasContent::Yes);
            }
        }
    };
    setBidiLevelForOpaqueInlineItems();
}

static inline bool canCacheWidthOnInlineTextItem(const InlineTextBox& inlineTextBox, bool isWhitespace)
{
    // Do not cache when:
    // 1. first-line style's unique font properties may produce non-matching width values.
    // 2. position dependent content is present (preserved tab character atm).
    if (&inlineTextBox.style() != &inlineTextBox.firstLineStyle() && !protect(inlineTextBox.style())->fontCascadeEqual(protect(inlineTextBox.firstLineStyle())))
        return false;
    if (!isWhitespace || !TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox))
        return true;
    return !inlineTextBox.hasPositionDependentContentWidth();
}

static bool textBoxMayHaveGlyphOverflow(const InlineTextBox& inlineTextBox)
{
    CheckedRef fontCascade = inlineTextBox.style().fontCascade();
    return !inlineTextBox.canUseSimpleFontCodePath() || fontCascade->primaryFont().origin() == FontOrigin::Remote;
}

static bool inlineTextBoxWidthsAreCacheable(const InlineTextBox& inlineTextBox, bool contentMayAdjustWidths, bool mayHaveGlyphOverflow)
{
    if (contentMayAdjustWidths || mayHaveGlyphOverflow || !canCacheWidthOnInlineTextItem(inlineTextBox, false))
        return false;
    return inlineTextBox.canUseSimplifiedContentMeasuring();
}

static FontCascadeCacheKey fontCascadeIdentifier(const FontCascade& fontCascade)
{
    return makeFontCascadeCacheKey(fontCascade.fontDescription(), protect(fontCascade.fontSelector()));
}

static void handleTextSpacing(TextSpacing::SpacingState& spacingState, TrimmableTextSpacings& trimmableTextSpacings, const InlineTextItem& inlineTextItem, size_t inlineItemIndex)
{
    auto autospace = inlineTextItem.style().textAutospace();
    if (!autospace.isNoAutospace()) {
        // We need to store information about spacing added between inline text items since it needs to be trimmed during line breaking if the consecutive items are placed on different lines
        auto characterClass = TextSpacing::characterClass(inlineTextItem.content().codePointAt(0));
        if (autospace.shouldApplySpacing(spacingState.lastCharacterClassFromPreviousRun, characterClass))
            trimmableTextSpacings.add(inlineItemIndex, TextAutospace::textAutospaceSize(protect(inlineTextItem.style().fontCascade())->primaryFont()));

        auto lastCharacterFromPreviousRun = TextUtil::lastBaseCharacterFromText(inlineTextItem.content());
        spacingState.lastCharacterClassFromPreviousRun = TextSpacing::characterClass(lastCharacterFromPreviousRun);
    } else
        spacingState.lastCharacterClassFromPreviousRun = TextSpacing::CharacterClass::Undefined;
}

void InlineItemsBuilder::computeInlineTextItemWidthsAndTextSpacing(InlineItemList& inlineItemList)
{
    if (inlineItemList.isEmpty())
        return;

    if (m_hasTextAutospace)
        computeInlineBoxBoundaryTextSpacings(inlineItemList);

    TextSpacing::SpacingState spacingState;
    TrimmableTextSpacings trimmableTextSpacings;
    auto& inlineBoxBoundaryTextSpacings = inlineContentCache().inlineBoxBoundaryTextSpacings();
    CheckedPtr<const InlineTextBox> currentInlineTextBox = nullptr;
    auto currentInlineTextBoxMayHaveGlyphOverflow = false;
    for (size_t inlineItemIndex = 0; inlineItemIndex < inlineItemList.size(); ++inlineItemIndex) {
        auto extraInlineTextSpacing = 0.f;
        auto& inlineItem = inlineItemList[inlineItemIndex];

        if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem)) {
            auto needsMeasuring = inlineTextItem->length() && !inlineTextItem->isZeroWidthSpaceSeparator() && canCacheWidthOnInlineTextItem(protect(inlineTextItem->inlineTextBox()), inlineTextItem->isWhitespace());
            if (needsMeasuring) {
                auto start = inlineTextItem->start();
                if (inlineItemIndex) {
                    // Box boudary text spacing is potentially registered for inline box start items which appear logically before an inline text item
                    auto potentialInlineBoxStartIndex = inlineItemIndex - 1;
                    if (auto inlineBoxBoundaryTextSpacing = inlineBoxBoundaryTextSpacings.find(potentialInlineBoxStartIndex); inlineBoxBoundaryTextSpacing != inlineBoxBoundaryTextSpacings.end())
                        extraInlineTextSpacing = inlineBoxBoundaryTextSpacing->value;
                }
                CheckedRef fontCascade = inlineTextItem->style().fontCascade();
                auto width = InlineLayoutUnit { };
                auto mayHaveGlyphOverflow = [&] {
                    if (currentInlineTextBox != &inlineTextItem->inlineTextBox()) {
                        currentInlineTextBoxMayHaveGlyphOverflow = textBoxMayHaveGlyphOverflow(protect(inlineTextItem->inlineTextBox()));
                        currentInlineTextBox = &inlineTextItem->inlineTextBox();
                    }
                    return currentInlineTextBoxMayHaveGlyphOverflow;
                };
                if (mayHaveGlyphOverflow()) {
                    // We don't need glyph overflow until after display boxes are created, but walking the content again is a major performance hit. See InlineDisplayContentBuilder::appendTextDisplayBox's addGlyphOverflow.
                    GlyphOverflow glyphOverflow;
                    glyphOverflow.computeBounds = true;
                    width = TextUtil::width(*inlineTextItem, fontCascade.get(), start, start + inlineTextItem->length(), { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::Yes, spacingState, &glyphOverflow);
                    inlineTextItem->setGlyphOverflow(std::clamp(glyphOverflow.top, 0_lu, 31_lu), std::clamp(glyphOverflow.bottom, 0_lu, 7_lu));
                } else
                    width = TextUtil::width(*inlineTextItem, fontCascade.get(), start, start + inlineTextItem->length(), { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::Yes, spacingState);
                inlineTextItem->setWidth(width + extraInlineTextSpacing);
                handleTextSpacing(spacingState, trimmableTextSpacings, *inlineTextItem, inlineItemIndex);
            }
            continue;
        }
        spacingState.lastCharacterClassFromPreviousRun = TextSpacing::CharacterClass::Undefined;
    }
    inlineContentCache().setTrimmableTextSpacings(WTF::move(trimmableTextSpacings));
}

bool InlineItemsBuilder::buildInlineItemListForTextFromBreakingPositionsCache(const InlineTextBox& inlineTextBox, InlineItemList& inlineItemList)
{
    auto& text = inlineTextBox.content();
    TextBreakingPositionCache::Key cacheKey { text, { inlineTextBox.style() }, m_securityOrigin->data() };
    auto* cacheEntry = TextBreakingPositionCache::singleton().get(cacheKey);
    if (!cacheEntry)
        return false;
    auto& breakingPositions = cacheEntry->breakingPositions;

    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    CheckedRef style = inlineTextBox.style();

    auto intialSize = inlineItemList.size();
    auto contentLength = text.length();
    ASSERT(contentLength);

    CheckedRef fontCascade = style->fontCascade();
    auto [ deferNonWhitespaceMeasurement, deferWhitespaceMeasurement ] = shouldDeferTextMeasurement(inlineTextBox);
    auto singleSpaceWidth = !deferWhitespaceMeasurement ? std::optional(std::max(0.f, TextUtil::singleSpaceWidth(fontCascade.get(), inlineTextBox.canUseSimplifiedContentMeasuring()))) : std::nullopt;
    auto mayHaveGlyphOverflow = textBoxMayHaveGlyphOverflow(inlineTextBox);

    auto widthCacheEligible = inlineTextBoxWidthsAreCacheable(inlineTextBox, contentRequiresVisualReordering() || m_hasTextAutospace, mayHaveGlyphOverflow);
    std::optional<FontCascadeCacheKey> widthCacheFontCascadeIdentifier;
    const TextBreakingPositionCache::WidthList* cachedWidths = nullptr;
    TextBreakingPositionCache::WidthList measuredWidths;
    if (widthCacheEligible) {
        widthCacheFontCascadeIdentifier = fontCascadeIdentifier(fontCascade);
        cachedWidths = TextBreakingPositionCache::singleton().widths(*cacheEntry, *widthCacheFontCascadeIdentifier);
    }
    size_t nonWhitespaceIndex = 0;

    auto nonWhitespaceItemWidth = [&](unsigned startPosition, unsigned endPosition) -> WidthAndGlyphOverflow {
        if (cachedWidths && nonWhitespaceIndex < cachedWidths->size()) {
            auto cachedWidth = (*cachedWidths)[nonWhitespaceIndex];
            ASSERT(cachedWidth == nonWhitespaceContentWidth(inlineTextBox, startPosition, endPosition, mayHaveGlyphOverflow).width);
            ASSERT(!mayHaveGlyphOverflow);
            return { cachedWidth, { } };
        }
        auto measured = nonWhitespaceContentWidth(inlineTextBox, startPosition, endPosition, mayHaveGlyphOverflow);
        if (widthCacheEligible && !cachedWidths)
            measuredWidths.append(measured.width);
        return measured;
    };

    inlineItemList.reserveCapacity(inlineItemList.size() + breakingPositions.size());
    unsigned previousPosition = 0;
    for (auto endPosition : breakingPositions) {
        auto startPosition = std::exchange(previousPosition, endPosition);
        if (endPosition > contentLength || startPosition >= endPosition) {
            ASSERT_NOT_REACHED();
            if (inlineItemList.size() > intialSize) {
                // Revert.
                !intialSize ? inlineItemList.clear() : inlineItemList.removeAt(intialSize, inlineItemList.size() - intialSize);
            }
            return false;
        }

        auto character = text[startPosition];
        if (character == lineSeparator || character == paragraphSeparator || (character == newlineCharacter && shouldPreserveNewline)) {
            inlineItemList.append(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, startPosition));
            continue;
        }

        auto isWhitespaceCharacter = character == space || character == newlineCharacter || character == tabCharacter;
        if (isWhitespaceCharacter) {
            auto isWordSeparator = character != tabCharacter || !shouldPreserveSpacesAndTabs;
            auto width = [&]() -> std::optional<InlineLayoutUnit> {
                if (deferWhitespaceMeasurement)
                    return { };
                if (!shouldPreserveSpacesAndTabs || endPosition - startPosition == 1)
                    return *singleSpaceWidth;
                return TextUtil::width(inlineTextBox, fontCascade, startPosition, endPosition, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No);
            };
            inlineItemList.append(InlineTextItem::createWhitespaceItem(inlineTextBox, startPosition, endPosition - startPosition, UBIDI_DEFAULT_LTR, isWordSeparator, width()));
            continue;
        }

        ASSERT(endPosition);
        auto hasTrailingSoftHyphen = text[endPosition - 1] == softHyphen;
        auto length = endPosition - startPosition;
        if (deferNonWhitespaceMeasurement || !length) {
            inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, length, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen));
            continue;
        }
        auto [width, topBottomOverflow] = nonWhitespaceItemWidth(startPosition, endPosition);
        inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, length, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen, width, topBottomOverflow));
        ++nonWhitespaceIndex;
    }

    if (widthCacheEligible && !cachedWidths && !measuredWidths.isEmpty())
        TextBreakingPositionCache::singleton().addWidths(cacheKey, *widthCacheFontCascadeIdentifier, WTF::move(measuredWidths));

    return true;
}

void InlineItemsBuilder::handleTextContent(const InlineTextBox& inlineTextBox, InlineItemList& inlineItemList, std::optional<size_t> partialContentOffset)
{
    auto text = inlineTextBox.content();
    auto contentLength = text.length();
    if (!contentLength)
        return inlineItemList.append(InlineTextItem::createEmptyItem(inlineTextBox));

    m_contentRequiresVisualReordering = m_contentRequiresVisualReordering || requiresVisualReordering(inlineTextBox);

    if (inlineTextBox.isCombined())
        return inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, { }, contentLength, UBIDI_DEFAULT_LTR, { }));

    if (!partialContentOffset && buildInlineItemListForTextFromBreakingPositionsCache(inlineTextBox, inlineItemList)) {
        if (!m_textContentPopulatedFromCache)
            m_textContentPopulatedFromCache = true;
        return;
    }

    m_textContentPopulatedFromCache = false;
    CheckedRef style = inlineTextBox.style();
    CheckedRef fontCascade = style->fontCascade();
    auto [ deferNonWhitespaceMeasurement, deferWhitespaceMeasurement ] = shouldDeferTextMeasurement(inlineTextBox);
    auto mayHaveGlyphOverflow = textBoxMayHaveGlyphOverflow(inlineTextBox);
    auto singleSpaceWidth = !deferWhitespaceMeasurement ? std::optional(std::max(0.f, TextUtil::singleSpaceWidth(fontCascade.get(), inlineTextBox.canUseSimplifiedContentMeasuring()))) : std::nullopt;
    auto shouldPreserveSpacesAndTabs = TextUtil::shouldPreserveSpacesAndTabs(inlineTextBox);
    auto shouldPreserveNewline = TextUtil::shouldPreserveNewline(inlineTextBox);
    auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { text, Style::toPlatform(style->usedLocale()), TextUtil::lineBreakIteratorMode(style->lineBreak()), TextUtil::contentAnalysis(style->wordBreak()) };
    auto currentPosition = partialContentOffset.value_or(0lu);
    ASSERT(currentPosition <= contentLength);

    auto handleSegmentBreak = [&] {
        // U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR are always forced line breaks (UAX#14 class BK, CSS Text §5.1).
        // U+000A is a forced line break only when white-space preserves newlines.
        auto character = text[currentPosition];
        if (character != lineSeparator && character != paragraphSeparator && (character != newlineCharacter || !shouldPreserveNewline))
            return false;
        inlineItemList.append(InlineSoftLineBreakItem::createSoftLineBreakItem(inlineTextBox, currentPosition++));
        return true;
    };
    auto handleWhitespace = [&] {
        auto stopAtWordSeparatorBoundary = shouldPreserveSpacesAndTabs && fontCascade->wordSpacing();
        auto whitespaceContent = text.is8Bit()
            ? moveToNextNonWhitespacePosition(text.span8(), currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, stopAtWordSeparatorBoundary)
            : moveToNextNonWhitespacePosition(text.span16(), currentPosition, shouldPreserveNewline, shouldPreserveSpacesAndTabs, stopAtWordSeparatorBoundary);
        if (!whitespaceContent)
            return false;

        ASSERT(whitespaceContent->length);
        if (style->whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces) {
            // https://www.w3.org/TR/css-text-3/#white-space-phase-1
            // For break-spaces, a soft wrap opportunity exists after every space and every tab.
            // FIXME: if this turns out to be a perf hit with too many individual whitespace inline items, we should transition this logic to line breaking.
            inlineItemList.appendUsingFunctor(whitespaceContent->length, [&](size_t offset) {
                return InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition + offset, 1, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, singleSpaceWidth);
            });
        } else {
            auto width = [&]() -> std::optional<InlineLayoutUnit> {
                if (deferWhitespaceMeasurement)
                    return { };
                if (!shouldPreserveSpacesAndTabs || whitespaceContent->length == 1)
                    return *singleSpaceWidth;
                return TextUtil::width(inlineTextBox, fontCascade, currentPosition, currentPosition + whitespaceContent->length, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No);
            };
            inlineItemList.append(InlineTextItem::createWhitespaceItem(inlineTextBox, currentPosition, whitespaceContent->length, UBIDI_DEFAULT_LTR, whitespaceContent->isWordSeparator, width()));
        }
        currentPosition += whitespaceContent->length;
        return true;

    };
    auto handleNonBreakingSpace = [&] {
        if (style->nbspMode() != NBSPMode::Space) {
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
        inlineItemList.appendUsingFunctor(endPosition - startPosition, [&](size_t offset) {
            return InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition + offset, 1, UBIDI_DEFAULT_LTR, false, nonWhitespaceContentWidth(inlineTextBox, startPosition + offset, startPosition + offset + 1, mayHaveGlyphOverflow).width, { });
        });
        currentPosition = endPosition;
        return true;
    };
    auto handleNonWhitespace = [&] {
        auto startPosition = currentPosition;
        auto endPosition = startPosition;
        auto hasTrailingSoftHyphen = false;
        if (style->hyphens() == Hyphens::None) {
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
        ASSERT_IMPLIES(style->hyphens() == Hyphens::None, !hasTrailingSoftHyphen);

        auto length = endPosition - startPosition;
        if (deferNonWhitespaceMeasurement || !length)
            inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, length, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen));
        else {
            auto widthAndGlyphOverflow = nonWhitespaceContentWidth(inlineTextBox, startPosition, endPosition, mayHaveGlyphOverflow);
            inlineItemList.append(InlineTextItem::createNonWhitespaceItem(inlineTextBox, startPosition, length, UBIDI_DEFAULT_LTR, hasTrailingSoftHyphen, widthAndGlyphOverflow.width, widthAndGlyphOverflow.topBottomOverflow));
        }
        currentPosition = endPosition;
        return true;
    };
    while (currentPosition < contentLength) {
        if (handleSegmentBreak())
            continue;
        if (handleWhitespace())
            continue;
        if (handleNonBreakingSpace())
            continue;
        if (handleNonWhitespace())
            continue;
        // Unsupported content?
        ASSERT_NOT_REACHED();
    }
}

void InlineItemsBuilder::handleInlineBoxStart(const Box& inlineBox, InlineItemList& inlineItemList)
{
    inlineItemList.append({ inlineBox, InlineItem::Type::InlineBoxStart });
    m_contentRequiresVisualReordering |= requiresVisualReordering(inlineBox);
    m_hasTextAutospace |= !inlineBox.style().textAutospace().isNoAutospace();
    m_hasWhiteSpaceTrim |= !inlineBox.style().whiteSpaceTrim().isNone();
    ++m_inlineBoxCount;
}

void InlineItemsBuilder::handleInlineBoxEnd(const Box& inlineBox, InlineItemList& inlineItemList)
{
    inlineItemList.append({ inlineBox, InlineItem::Type::InlineBoxEnd });
    // Inline box end item itself can not trigger bidi content.
    ASSERT(contentRequiresVisualReordering() || inlineBox.writingMode().isBidiLTR() || inlineBox.style().rtlOrdering() == Order::Visual || inlineBox.style().unicodeBidi() == UnicodeBidi::Normal);
}

void InlineItemsBuilder::handleInlineLevelBox(const Box& layoutBox, InlineItemList& inlineItemList)
{
    if (layoutBox.isLineBreakBox())
        return inlineItemList.append({ layoutBox, layoutBox.isWordBreakOpportunity() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });

    if (layoutBox.isRubyAnnotationBox())
        return inlineItemList.append({ layoutBox, InlineItem::Type::OutOfFlow });

    if (layoutBox.isAtomicInlineBox()) {
        // An inline-block is a block container 'white-space-trim' applies to; track it as a carrier so the
        // discard pass runs. Replaced content and iframes are not block containers and never carry it.
        if (isNonReplacedInlineBlock(layoutBox))
            m_hasWhiteSpaceTrim |= !layoutBox.style().whiteSpaceTrim().isNone();
        return inlineItemList.append({ layoutBox, InlineItem::Type::AtomicInlineBox });
    }

    ASSERT_NOT_REACHED();
}

static TextBreakingPositionCache::WidthList collectWidthsFromBuiltItems(const InlineTextBox& inlineTextBox, std::span<const InlineItem> span, bool contentMayAdjustWidths)
{
    if (!inlineTextBoxWidthsAreCacheable(inlineTextBox, contentMayAdjustWidths, textBoxMayHaveGlyphOverflow(inlineTextBox)))
        return { };

    TextBreakingPositionCache::WidthList widths;
    for (auto& inlineItem : span) {
        auto* textItem = dynamicDowncast<InlineTextItem>(inlineItem);
        if (!textItem || textItem->isWhitespace())
            continue;
        auto width = textItem->width();
        if (!width)
            return { };
        widths.append(*width);
    }
    return widths;
}

void InlineItemsBuilder::populateBreakingPositionCache(const InlineItemList& inlineItemList, const Document& document, bool contentMayAdjustWidths)
{
    if (inlineItemList.size() < TextBreakingPositionCache::minimumRequiredContentBreaks)
        return;

    auto inlineTextBoxContentSpan = [](const InlineItemList& inlineItemList, size_t index, const InlineTextBox* inlineTextBox) ALWAYS_INLINE_LAMBDA {
        size_t length = 0;
        for (auto& item : inlineItemList.subspan(index)) {
            if (&item.layoutBox() != inlineTextBox)
                break;
            ++length;
        }
        return inlineItemList.subspan(index, length);
    };

    // Preserve breaking positions across content mutation.
    Ref securityOrigin = document.securityOrigin();
    auto& breakingPositionCache = TextBreakingPositionCache::singleton();
    size_t index = 0;
    while (index < inlineItemList.size()) {
        CheckedPtr inlineTextBox = dynamicDowncast<InlineTextBox>(inlineItemList[index].layoutBox());
        if (!inlineTextBox) {
            ++index;
            continue;
        }

        auto span = inlineTextBoxContentSpan(inlineItemList, index, inlineTextBox);
        if (span.size() < TextBreakingPositionCache::minimumRequiredContentBreaks) {
            // Inline text box content's span is too short.
            index += span.size();
            continue;
        }

        auto isInlineTextBoxEligibleForBreakingPositionCache = inlineTextBox->content().length() >= TextBreakingPositionCache::minimumRequiredTextLengthForContentBreakCache;
        if (!isInlineTextBoxEligibleForBreakingPositionCache) {
            // Text is too short.
            index += span.size();
            continue;
        }

        TextBreakingPositionContext context { inlineTextBox->style() };
        if (breakingPositionCache.get({ inlineTextBox->content(), context, securityOrigin->data() })) {
            // Cache is already populated.
            index += span.size();
            continue;
        }

        TextBreakingPositionCache::List breakingPositionList(ReserveCapacityTag { }, span.size());
        for (auto& inlineItem : span) {
            if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem))
                breakingPositionList.append(inlineTextItem->end());
            else if (auto* softLineBreakItem = dynamicDowncast<InlineSoftLineBreakItem>(inlineItem))
                breakingPositionList.append(softLineBreakItem->position() + 1);
            else {
                ASSERT_NOT_REACHED();
                breakingPositionList.clear();
                break;
            }
        }

        ASSERT(!breakingPositionList.isEmpty());
        if (breakingPositionList.size() >= TextBreakingPositionCache::minimumRequiredContentBreaks) {
            auto widths = collectWidthsFromBuiltItems(*inlineTextBox, span, contentMayAdjustWidths);
            TextBreakingPositionCache::Key key { inlineTextBox->content(), context, securityOrigin->data() };
            breakingPositionCache.set(key, WTF::move(breakingPositionList));
            if (!widths.isEmpty()) {
                CheckedRef fontCascade = inlineTextBox->style().fontCascade();
                breakingPositionCache.addWidths(key, fontCascadeIdentifier(fontCascade), WTF::move(widths));
            }
        }
        index += span.size();
    }
}

std::pair<bool, bool> InlineItemsBuilder::shouldDeferTextMeasurement(const InlineTextBox& inlineTextBox) const
{
    auto shouldDefer = contentRequiresVisualReordering() || m_hasTextAutospace;
    return { shouldDefer || !canCacheWidthOnInlineTextItem(inlineTextBox, false), shouldDefer || !canCacheWidthOnInlineTextItem(inlineTextBox, true) };
}

}
}

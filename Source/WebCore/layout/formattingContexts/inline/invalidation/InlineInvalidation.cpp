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
#include "InlineInvalidation.h"

#include "InlineDamage.h"
#include "InlineSoftLineBreakItem.h"
#include "LayoutElementBox.h"
#include "LayoutUnit.h"
#include "TextBreakingPositionContext.h"
#include "WritingMode.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

InlineInvalidation::InlineInvalidation(InlineDamage& inlineDamage, const InlineItemList& inlineItemList, const InlineDisplay::Content& displayContent)
    : m_inlineDamage(inlineDamage)
    , m_inlineItemList(inlineItemList)
    , m_displayContent(displayContent)
{
}

bool InlineInvalidation::rootStyleWillChange(const ElementBox& formattingContextRoot, const RenderStyle& newStyle)
{
    ASSERT(formattingContextRoot.establishesInlineFormattingContext());

    if (m_inlineDamage.isInlineItemListDirty())
        return true;

    auto inlineItemListNeedsUpdate = [&] {
        auto& oldStyle = formattingContextRoot.style();

        if (TextBreakingPositionContext { oldStyle } != TextBreakingPositionContext { newStyle })
            return true;

        if (oldStyle.fontCascade() != newStyle.fontCascade())
            return true;

        auto* newFirstLineStyle = newStyle.getCachedPseudoStyle({ PseudoId::FirstLine });
        auto* oldFirstLineStyle = oldStyle.getCachedPseudoStyle({ PseudoId::FirstLine });
        if (newFirstLineStyle && oldFirstLineStyle && oldFirstLineStyle->fontCascade() != newFirstLineStyle->fontCascade())
            return true;

        if ((newFirstLineStyle && newFirstLineStyle->fontCascade() != oldStyle.fontCascade()) || (oldFirstLineStyle && oldFirstLineStyle->fontCascade() != newStyle.fontCascade()))
            return true;

        if (oldStyle.writingMode().bidiDirection() != newStyle.writingMode().bidiDirection() || oldStyle.unicodeBidi() != newStyle.unicodeBidi() || oldStyle.tabSize() != newStyle.tabSize() || oldStyle.textSecurity() != newStyle.textSecurity())
            return true;

        return false;
    };

    if (inlineItemListNeedsUpdate())
        m_inlineDamage.setInlineItemListDirty();

    return true;
}

bool InlineInvalidation::styleWillChange(const Box& layoutBox, const RenderStyle& newStyle, StyleDifference diff)
{
    if (diff == StyleDifference::Layout) {
        m_inlineDamage.resetLayoutPosition();
        m_inlineDamage.setDamageReason(InlineDamage::Reason::StyleChange);
    }

    if (m_inlineDamage.isInlineItemListDirty())
        return true;

    if (layoutBox.isInlineTextBox()) {
        // Either the root or parent inline box takes care of this style change.
        return true;
    }

    auto inlineItemListNeedsUpdate = [&] {
        auto& oldStyle = layoutBox.style();

        auto hasInlineItemTypeChanged = oldStyle.hasOutOfFlowPosition() != newStyle.hasOutOfFlowPosition() || oldStyle.isFloating() != newStyle.isFloating() || oldStyle.display() != newStyle.display();
        if (hasInlineItemTypeChanged)
            return true;

        if (!layoutBox.isInlineBox())
            return false;

        auto contentMayNeedNewBreakingPositionsAndMeasuring = TextBreakingPositionContext { oldStyle } != TextBreakingPositionContext { newStyle } || oldStyle.fontCascade() != newStyle.fontCascade();
        if (contentMayNeedNewBreakingPositionsAndMeasuring)
            return true;

        auto bidiContextChanged = oldStyle.unicodeBidi() != newStyle.unicodeBidi() || oldStyle.writingMode().bidiDirection() != newStyle.writingMode().bidiDirection();
        if (bidiContextChanged)
            return true;

        return false;
    };

    if (inlineItemListNeedsUpdate())
        m_inlineDamage.setInlineItemListDirty();

    return true;
}

bool InlineInvalidation::inlineLevelBoxContentWillChange(const Box&)
{
    // FIXME: Add support for partial layout when inline box content change may trigger size change.
    m_inlineDamage.resetLayoutPosition();
    return true;
}

struct DamagedContent {
    const Box& layoutBox;
    // Only text type of boxes may have offset. No offset also simply points to the end of the layout box.
    std::optional<size_t> offset { };
    enum class Type : uint8_t {
        Insertion,
        Removal
    };
    Type type { Type::Insertion };
};
static std::optional<size_t> damagedLineIndex(const DamagedContent& damagedContent, const InlineDisplay::Boxes& displayBoxes)
{
    ASSERT(!displayBoxes.isEmpty());

    if (displayBoxes.size() == 1) {
        // Let's return the first line on empty content (when only root inline display box is present). 
        return displayBoxes.last().lineIndex();
    }

    auto lastDisplayBoxIndexForLayoutBox = [&]() -> std::optional<size_t> {
        for (auto index = displayBoxes.size(); index--;) {
            auto& displayBox = displayBoxes[index];
            if (&damagedContent.layoutBox == &displayBox.layoutBox())
                return index;
        }
        // FIXME: When the damaged layout box does not generate even one display box (all collapsed), we should
        // look at previous/next content to damage lines.
        return { };
    };
    auto lastDisplayBoxIndex = lastDisplayBoxIndexForLayoutBox();
    if (!lastDisplayBoxIndex) {
        // FIXME: Completely collapsed content should not trigger full invalidation.
        return { };
    }

    auto candidateLineIndex = [&](auto damagedDisplayBoxIndex) -> std::optional<size_t> {
        if (!damagedDisplayBoxIndex || damagedDisplayBoxIndex >= displayBoxes.size()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        auto& damagedDisplayBox = displayBoxes[damagedDisplayBoxIndex];
        ASSERT(&damagedDisplayBox.layoutBox() == &damagedContent.layoutBox);
        // In case of content deletion, we may need to damage the "previous" line.
        if (damagedContent.type == DamagedContent::Type::Insertion || !damagedDisplayBox.lineIndex())
            return { damagedDisplayBox.lineIndex() };
        if (!displayBoxes[damagedDisplayBoxIndex - 1].isRootInlineBox()) {
            // There's more content in front, it's safe to stay on the current line.
            ASSERT(displayBoxes[damagedDisplayBoxIndex - 1].lineIndex() == damagedDisplayBox.lineIndex());
            return { damagedDisplayBox.lineIndex() };
        }
        auto damagedContentIsFirstContentOnLine = !damagedDisplayBox.isText() || (damagedContent.offset && (!*damagedContent.offset || damagedDisplayBox.text().start() == *damagedContent.offset));
        return { damagedContentIsFirstContentOnLine ? damagedDisplayBox.lineIndex() - 1 : damagedDisplayBox.lineIndex() };
    };

    auto leadingIndexForDisplayBox = *lastDisplayBoxIndex;
    if (damagedContent.layoutBox.isLineBreakBox() || damagedContent.layoutBox.isReplacedBox())
        return candidateLineIndex(leadingIndexForDisplayBox);

    if (is<InlineTextBox>(damagedContent.layoutBox)) {
        if (!damagedContent.offset)
            return candidateLineIndex(leadingIndexForDisplayBox);

        if (damagedContent.offset > displayBoxes[leadingIndexForDisplayBox].text().end()) {
            // Protect against text updater providing bogus damage offset (offset is _after_ the last display box here).
            // Let's just fallback to full invalidation.
            return { };
        }

        for (auto index = leadingIndexForDisplayBox; index > 0; --index) {
            auto& displayBox = displayBoxes[index];
            if (displayBox.isRootInlineBox() || (displayBox.isInlineBox() && !displayBox.isFirstForLayoutBox())) {
                // Multi line damage includes line leading root inline display boxes (and maybe line spanning inline boxes).
                continue;
            }
            if (&displayBoxes[index].layoutBox() != &damagedContent.layoutBox) {
                // We should always be able to find the damaged offset within this layout box.
                break;
            }
            ASSERT(displayBox.isTextOrSoftLineBreak());
            // Find out which display box has the damaged offset position.
            auto startOffset = displayBox.text().start();
            if (startOffset <= *damagedContent.offset) {
                // FIXME: Add support for leading inline box deletion too.
                return candidateLineIndex(index);
            }
            leadingIndexForDisplayBox = index;
        }
        // In some cases leading content does not produce a display box (collapsed whitespace), let's just use the first display box.
        ASSERT(leadingIndexForDisplayBox);
        return candidateLineIndex(leadingIndexForDisplayBox);
    }
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

static const InlineDisplay::Box* leadingContentDisplayForLineIndex(size_t lineIndex, const InlineDisplay::Boxes& displayBoxes)
{
    auto rootInlineBoxIndexOnLine = [&]() -> size_t {
        for (auto index = displayBoxes.size(); index--;) {
            auto& displayBox = displayBoxes[index];
            if (displayBox.lineIndex() == lineIndex && displayBox.isRootInlineBox())
                return index;
        }
        ASSERT_NOT_REACHED();
        return 0;
    };

    for (auto firstContentDisplayBoxIndex = rootInlineBoxIndexOnLine() + 1; firstContentDisplayBoxIndex < displayBoxes.size(); ++firstContentDisplayBoxIndex) {
        auto& displayBox = displayBoxes[firstContentDisplayBoxIndex];
        if (displayBox.isRootInlineBox())
            return nullptr;
        if (!displayBox.isNonRootInlineBox() || displayBox.isFirstForLayoutBox())
            return &displayBox;
    }
    return nullptr;
}

static std::optional<InlineItemPosition> inlineItemPositionForDisplayBox(const InlineDisplay::Box& displayBox, const InlineItemList& inlineItemList)
{
    ASSERT(!inlineItemList.isEmpty());

    auto lastInlineItemIndexForDisplayBox = [&]() -> std::optional<size_t> {
        for (size_t index = inlineItemList.size(); index--;) {
            if (&inlineItemList[index].layoutBox() == &displayBox.layoutBox() && !inlineItemList[index].isInlineBoxEnd())
                return index;
        }
        return { };
    }();

    if (!lastInlineItemIndexForDisplayBox)
        return { };

    // This is our last InlineItem associated with this layout box. Let's find out
    // which previous InlineItem is at the beginning of this display box.
    auto inlineItemIndex = *lastInlineItemIndexForDisplayBox;
    if (!displayBox.isTextOrSoftLineBreak())
        return InlineItemPosition { inlineItemIndex, 0 };
    // 1. A display box could consist of one or more InlineItems.
    // 2. A display box could start at any offset inside an InlineItem.
    auto startOffset = displayBox.text().start();
    while (true) {
        auto inlineTextItemRange = [&]() -> WTF::Range<unsigned> {
            if (auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItemList[inlineItemIndex]))
                return { inlineTextItem->start(), inlineTextItem->end() };
            if (auto* inlineSoftBreakItem = dynamicDowncast<InlineSoftLineBreakItem>(inlineItemList[inlineItemIndex])) {
                auto startPosition = inlineSoftBreakItem->position();
                return { startPosition, startPosition + 1 };
            }
            ASSERT_NOT_REACHED();
            return { };
        };
        auto textRange = inlineTextItemRange();
        if (textRange.begin() <= startOffset && textRange.end() > startOffset)
            return InlineItemPosition { inlineItemIndex, startOffset - textRange.begin() };
        if (!inlineItemIndex--)
            break;
    }
    return { };
}

static std::optional<InlineItemPosition> inlineItemPositionForDamagedContentPosition(const DamagedContent& damagedContent, InlineItemPosition candidatePosition, const InlineItemList& inlineItemList)
{
    ASSERT(candidatePosition.index < inlineItemList.size());
    if (!candidatePosition)
        return candidatePosition;
    if (!is<InlineTextBox>(damagedContent.layoutBox)) {
        ASSERT(!damagedContent.offset);
        return candidatePosition;
    }
    auto candidateInlineItem = inlineItemList[candidatePosition.index];
    if (&candidateInlineItem.layoutBox() != &damagedContent.layoutBox || (!is<InlineTextItem>(candidateInlineItem) && !is<InlineSoftLineBreakItem>(candidateInlineItem)))
        return candidatePosition;
    if (!damagedContent.offset) {
        // When damage points to "after" the layout box, whatever InlineItem we found is surely before the damage.
        return candidatePosition;
    }

    auto startPosition = [&](auto& inlineItem) {
        auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItem);
        return inlineTextItem ? inlineTextItem->start() : downcast<InlineSoftLineBreakItem>(inlineItem).position();
    };

    auto contentOffset = startPosition(candidateInlineItem) + candidatePosition.offset;
    if (contentOffset < *damagedContent.offset) {
        // The damaged content is after the start of this inline item.
        return candidatePosition;
    }
    // When the inline item's entire content is being removed, we need to find the previous inline item that belongs to this damaged layout box.
    if (contentOffset == *damagedContent.offset && damagedContent.type != DamagedContent::Type::Removal)
        return candidatePosition;

    // The damage offset is before the first display box we managed to find for this layout box.
    // Let's adjust the candidate position by moving it over to the damaged offset.
    for (auto index = candidatePosition.index; index--;) {
        auto& previousInlineItem = inlineItemList[index];
        if (&candidateInlineItem.layoutBox() != &previousInlineItem.layoutBox()) {
            // We should stay on the same layout box.
            ASSERT_NOT_REACHED();
            return { };
        }
        if (startPosition(previousInlineItem) <= *damagedContent.offset)
            return InlineItemPosition { index, { } };
    }
    return { };
}

static bool isValidInlineItemPositionForLine(const InlineItemPosition& inlineItemPosition, size_t lineIndex)
{
    // It's clearly not correct when starting position is 0 while the damaged line is not the first one.
    if (!inlineItemPosition && lineIndex)
        return false;
    // FIXME: Consider checking for the case when we are on the first line with a non-zero inlineItemPosition.
    return true;
}

static std::optional<InlineItemPosition> leadingInlineItemPositionOnLastLine(const InlineItemList& inlineItemList, const InlineDisplay::Boxes& displayBoxes)
{
    ASSERT(!displayBoxes.isEmpty());
    auto lastLineIndex = displayBoxes.last().lineIndex();
    auto leadingContentDisplayBoxOnInvalidatedLine = leadingContentDisplayForLineIndex(lastLineIndex, displayBoxes);
    if (!leadingContentDisplayBoxOnInvalidatedLine)
        return { };

    auto inlineItemPositionForLeadingDisplayBox = inlineItemPositionForDisplayBox(*leadingContentDisplayBoxOnInvalidatedLine, inlineItemList);
    if (!inlineItemPositionForLeadingDisplayBox)
        return { };

    if (!isValidInlineItemPositionForLine(*inlineItemPositionForLeadingDisplayBox, lastLineIndex)) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return InlineItemPosition { *inlineItemPositionForLeadingDisplayBox };
}

struct InvalidatedLine {
    size_t index { 0 };
    InlineItemPosition leadingInlineItemPosition { };
    size_t damagedLineIndex { 0 };
};
static std::optional<InvalidatedLine> invalidatedLineByDamagedBox(const DamagedContent& damagedContent, const InlineItemList& inlineItemList, const InlineDisplay::Boxes& displayBoxes)
{
    ASSERT(!displayBoxes.isEmpty());
    // 1. Find the root inline box based on the damaged layout box (this is our damaged line)
    // 2. Find the first content display box on this damaged line
    // 3. Find the associated InlineItem with partial text offset if applicable
    auto damagedLine = damagedLineIndex(damagedContent, displayBoxes);
    if (!damagedLine)
        return { };

    // We can't be sure whether a newly inserted content introduces a new breaking opportunity, so let's just initiate layout from the previous line.
    auto lineToInvalidate = damagedContent.type == DamagedContent::Type::Removal ? *damagedLine : !*damagedLine ? 0 : *damagedLine - 1;
    auto leadingContentDisplayBoxOnInvalidatedLine = leadingContentDisplayForLineIndex(lineToInvalidate, displayBoxes);
    if (!leadingContentDisplayBoxOnInvalidatedLine) {
        // This is a completely empty line (e.g. <div><span> </span></div> where the whitespace is collapsed).
        return { };
    }
    if (auto inlineItemPositionForLeadingDisplayBox = inlineItemPositionForDisplayBox(*leadingContentDisplayBoxOnInvalidatedLine, inlineItemList)) {
        if (!isValidInlineItemPositionForLine(*inlineItemPositionForLeadingDisplayBox, lineToInvalidate)) {
            ASSERT_NOT_REACHED();
            return { };
        }
        // In some rare cases, the leading display box's inline item position is after the actual damage (e.g. when collapsed leading content is removed).
        if (auto adjustedPosition = inlineItemPositionForDamagedContentPosition(damagedContent, *inlineItemPositionForLeadingDisplayBox, inlineItemList))
            return InvalidatedLine { lineToInvalidate, *adjustedPosition, *damagedLine };
    }
    return { };
}

static InlineDamage::TrailingDisplayBoxList trailingDisplayBoxesAfterDamagedLine(size_t damagedLineIndex, const InlineDisplay::Content& displayContent)
{
    auto trailingDisplayBoxes = InlineDamage::TrailingDisplayBoxList { };
    auto& lines = displayContent.lines;
    auto& boxes = displayContent.boxes;
    for (size_t lineIndex = damagedLineIndex + 1; lineIndex < lines.size(); ++lineIndex) {
        auto lastDisplayBoxIndexForLine = lines[lineIndex].firstBoxIndex() + lines[lineIndex].boxCount() - 1;
        if (lastDisplayBoxIndexForLine >= boxes.size()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        trailingDisplayBoxes.append(boxes[lastDisplayBoxIndexForLine]);
    }
    return trailingDisplayBoxes;
}

bool InlineInvalidation::updateInlineDamage(const InvalidatedLine& invalidatedLine, InlineDamage::Reason reason, ShouldApplyRangeLayout shouldApplyRangeLayout, LayoutUnit pageTopAdjustment)
{
    auto isValidDamage = [&] {
        // Check for consistency.
        if (!invalidatedLine.leadingInlineItemPosition) {
            // We have to start at the first line if damage points to the leading inline item.
            return !invalidatedLine.index;
        }
        return true;
    };
    if (!isValidDamage()) {
        ASSERT_NOT_REACHED();
        m_inlineDamage.resetLayoutPosition();
        return false;
    }

    auto partialContentTop = LayoutUnit { invalidatedLine.index ? m_displayContent.lines[invalidatedLine.index - 1].lineBoxLogicalRect().maxY() : 0 } + pageTopAdjustment;

    auto layoutStartPosition = InlineDamage::LayoutPosition {
        invalidatedLine.index,
        invalidatedLine.leadingInlineItemPosition,
        partialContentTop
    };

    m_inlineDamage.setDamageReason(reason);
    m_inlineDamage.setLayoutStartPosition(WTFMove(layoutStartPosition));

    if (shouldApplyRangeLayout == ShouldApplyRangeLayout::Yes)
        m_inlineDamage.setTrailingDisplayBoxes(trailingDisplayBoxesAfterDamagedLine(invalidatedLine.index, m_displayContent));
    return true;
}

static bool isSupportedContent(const Box& layoutBox)
{
    return is<InlineTextBox>(layoutBox) || layoutBox.isLineBreakBox() || layoutBox.isReplacedBox() || layoutBox.isInlineBox();
}

bool InlineInvalidation::setFullLayoutIfNeeded(const Box& layoutBox)
{
    if (!isSupportedContent(layoutBox)) {
        ASSERT_NOT_REACHED();
        m_inlineDamage.resetLayoutPosition();
        return true;
    }

    if (displayBoxes().isEmpty()) {
        ASSERT_NOT_REACHED();
        m_inlineDamage.resetLayoutPosition();
        return true;
    }

    if (m_inlineItemList.isEmpty()) {
        // We must be under memory pressure.
        m_inlineDamage.resetLayoutPosition();
        return true;
    }

    if (m_inlineDamage.reasons().contains(InlineDamage::Reason::StyleChange)) {
        m_inlineDamage.resetLayoutPosition();
        return true;
    }

    return false;
}

bool InlineInvalidation::textInserted(const InlineTextBox& newOrDamagedInlineTextBox, std::optional<size_t> offset)
{
    m_inlineDamage.setInlineItemListDirty();

    if (setFullLayoutIfNeeded(newOrDamagedInlineTextBox))
        return false;

    auto& displayBoxes = this->displayBoxes();
    auto invalidatedLine = std::optional<InvalidatedLine> { };
    auto damageReason = offset ? InlineDamage::Reason::ContentChange : newOrDamagedInlineTextBox.nextInFlowSibling() ? InlineDamage::Reason::Insert : InlineDamage::Reason::Append;
    switch (damageReason) {
    case InlineDamage::Reason::ContentChange:
        // Existing text box got modified. Dirty all the way up to the damaged position's line.
        invalidatedLine = invalidatedLineByDamagedBox({ newOrDamagedInlineTextBox, *offset }, m_inlineItemList, displayBoxes);
        break;
    case InlineDamage::Reason::Append:
        // New text box got appended. Let's dirty the last line.
        if (auto leadingInlineItemPosition = leadingInlineItemPositionOnLastLine(m_inlineItemList, displayBoxes)) {
            auto lastLineIndex = displayBoxes.last().lineIndex();
            invalidatedLine = InvalidatedLine { lastLineIndex, *leadingInlineItemPosition, lastLineIndex };
        }
        break;
    case InlineDamage::Reason::Insert: {
        invalidatedLine = InvalidatedLine { };
        // New text box got inserted. Let's damage existing content starting from the previous sibling.
        if (auto* previousSibling = newOrDamagedInlineTextBox.previousInFlowSibling())
            invalidatedLine = invalidatedLineByDamagedBox({ *previousSibling }, m_inlineItemList, displayBoxes);
        break;
        }
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (invalidatedLine)
        return updateInlineDamage(*invalidatedLine, damageReason, offset ? ShouldApplyRangeLayout::No : ShouldApplyRangeLayout::Yes);

    m_inlineDamage.resetLayoutPosition();
    return false;
}

bool InlineInvalidation::textWillBeRemoved(const InlineTextBox& damagedInlineTextBox, std::optional<size_t> offset)
{
    m_inlineDamage.setInlineItemListDirty();

    if (setFullLayoutIfNeeded(damagedInlineTextBox))
        return false;

    if (auto invalidatedLine = invalidatedLineByDamagedBox({ damagedInlineTextBox, offset.value_or(0), DamagedContent::Type::Removal }, m_inlineItemList, displayBoxes()))
        return updateInlineDamage(*invalidatedLine, InlineDamage::Reason::Remove, ShouldApplyRangeLayout::No);

    m_inlineDamage.resetLayoutPosition();
    return false;
}

bool InlineInvalidation::inlineLevelBoxInserted(const Box& layoutBox)
{
    m_inlineDamage.setInlineItemListDirty();

    if (setFullLayoutIfNeeded(layoutBox))
        return false;

    auto& displayBoxes = this->displayBoxes();
    auto invalidatedLine = std::optional<InvalidatedLine> { };
    if (!layoutBox.nextInFlowSibling()) {
        // New box got appended. Let's dirty the last line.
        if (m_inlineDamage.reasons() == InlineDamage::Reason::Append) {
            // Series of append operations always produces the same damage position.
            return m_inlineDamage.layoutStartPosition().has_value();
        }
        ASSERT(!m_inlineDamage.reasons());
        if (auto leadingInlineItemPosition = leadingInlineItemPositionOnLastLine(m_inlineItemList, displayBoxes)) {
            auto lastLineIndex = displayBoxes.last().lineIndex();
            invalidatedLine = InvalidatedLine { lastLineIndex, *leadingInlineItemPosition, lastLineIndex };
        }
    } else {
        invalidatedLine = InvalidatedLine { };
        // New box got inserted. Let's damage existing content starting from the previous sibling.
        if (auto* previousSibling = layoutBox.previousInFlowSibling())
            invalidatedLine = invalidatedLineByDamagedBox({ *previousSibling }, m_inlineItemList, displayBoxes);
    }
    if (invalidatedLine)
        return updateInlineDamage(*invalidatedLine, !layoutBox.nextInFlowSibling() ? InlineDamage::Reason::Append : InlineDamage::Reason::Insert, ShouldApplyRangeLayout::Yes);

    m_inlineDamage.resetLayoutPosition();
    return false;
}

bool InlineInvalidation::inlineLevelBoxWillBeRemoved(const Box& layoutBox)
{
    m_inlineDamage.setInlineItemListDirty();

    if (setFullLayoutIfNeeded(layoutBox))
        return false;

    if (auto invalidatedLine = invalidatedLineByDamagedBox({ layoutBox, { }, DamagedContent::Type::Removal }, m_inlineItemList, displayBoxes()))
        return updateInlineDamage(*invalidatedLine, InlineDamage::Reason::Remove, ShouldApplyRangeLayout::Yes);

    m_inlineDamage.resetLayoutPosition();
    return false;
}

bool InlineInvalidation::restartForPagination(size_t lineIndex, LayoutUnit pageTopAdjustment)
{
    auto leadingContentDisplayBoxOnDamagedLine = leadingContentDisplayForLineIndex(lineIndex, displayBoxes());
    if (!leadingContentDisplayBoxOnDamagedLine)
        return false;
    auto inlineItemPositionForLeadingDisplayBox = inlineItemPositionForDisplayBox(*leadingContentDisplayBoxOnDamagedLine, m_inlineItemList);
    if (!inlineItemPositionForLeadingDisplayBox || !*inlineItemPositionForLeadingDisplayBox)
        return false;

    return updateInlineDamage({ lineIndex, *inlineItemPositionForLeadingDisplayBox }, InlineDamage::Reason::Pagination, ShouldApplyRangeLayout::Yes, pageTopAdjustment);
}

void InlineInvalidation::resetInlineDamage(InlineDamage& inlineDamage)
{
    inlineDamage.setInlineItemListDirty();
    inlineDamage.resetLayoutPosition();
}

}
}

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
#include "InlineFormattingContext.h"

#include "AvailableLineWidthOverride.h"
#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineContentBalancer.h"
#include "InlineContentCache.h"
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineItemsBuilder.h"
#include "InlineLayoutState.h"
#include "InlineLineBox.h"
#include "InlineLineBoxBuilder.h"
#include "InlineLineTypes.h"
#include "InlineTextItem.h"
#include "IntrinsicWidthHandler.h"
#include "LayoutBox.h"
#include "LayoutContext.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RenderStyleInlines.h"
#include "TextOnlySimpleLineBuilder.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

static std::optional<InlineItemRange> partialRangeForDamage(const InlineItemList& inlineItemList, const InlineDamage& lineDamage)
{
    auto damageStartPosition = lineDamage.start()->inlineItemPosition;
    if (damageStartPosition.index >= inlineItemList.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& damagedInlineItem = inlineItemList[damageStartPosition.index];
    if (damageStartPosition.offset && (!is<InlineTextItem>(damagedInlineItem) || damageStartPosition.offset >= downcast<InlineTextItem>(damagedInlineItem).length())) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return InlineItemRange { damageStartPosition, { inlineItemList.size(), 0 } };
}

InlineFormattingContext::InlineFormattingContext(const ElementBox& rootBlockContainer, LayoutState& layoutState, BlockLayoutState& parentBlockLayoutState)
    : m_rootBlockContainer(rootBlockContainer)
    , m_layoutState(layoutState)
    , m_floatingContext(rootBlockContainer, layoutState, parentBlockLayoutState.placedFloats())
    , m_inlineFormattingUtils(*this)
    , m_inlineQuirks(*this)
    , m_inlineContentCache(layoutState.inlineContentCache(rootBlockContainer))
    , m_inlineLayoutState(parentBlockLayoutState)
{
    initializeInlineLayoutState(layoutState);
}

InlineLayoutResult InlineFormattingContext::layout(const ConstraintsForInlineContent& constraints, const InlineDamage* lineDamage)
{
    auto& placedFloats = layoutState().placedFloats();
    if (!root().hasInFlowChild() && !root().hasOutOfFlowChild()) {
        // Float only content does not support partial layout.
        ASSERT(!lineDamage);
        layoutFloatContentOnly(constraints);
        return { { }, InlineLayoutResult::Range::Full };
    }

    auto& inlineContentCache = this->inlineContentCache();
    auto needsInlineItemListUpdate = inlineContentCache.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemListUpdate) {
        // FIXME: This should go to invalidation.
        inlineContentCache.clearMaximumIntrinsicWidthLayoutResult();
        auto startPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
        InlineItemsBuilder { inlineContentCache, root() }.build(startPosition);
    }

    auto& inlineItemList = inlineContentCache.inlineItems().content();
    auto needsLayoutRange = InlineItemRange { { }, { inlineItemList.size(), 0 } };
    if (lineDamage) {
        if (auto partialRange = partialRangeForDamage(inlineItemList, *lineDamage))
            needsLayoutRange = *partialRange;
        else {
            // Demote this layout to full range.
            lineDamage = nullptr;
        }
    }
    if (needsLayoutRange.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutRange.start)
            return { };
        if (!lineDamage || !lineDamage->start()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        auto lastLineIndex = lineDamage->start()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    };

    if (root().style().textWrapMode() == TextWrapMode::Wrap && root().style().textWrapStyle() == TextWrapStyle::Balance) {
        auto balancer = InlineContentBalancer { *this, inlineItemList, constraints.horizontal() };
        auto balancedLineWidths = balancer.computeBalanceConstraints();
        if (balancedLineWidths)
            layoutState().setAvailableLineWidthOverride({ *balancedLineWidths });
    }

    if (TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), this->inlineContentCache(), &placedFloats)) {
        auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { *this, constraints.horizontal(), inlineItemList };
        return lineLayout(simplifiedLineBuilder, inlineItemList, needsLayoutRange, previousLine(), constraints, lineDamage);
    }
    auto lineBuilder = LineBuilder { *this, constraints.horizontal(), inlineItemList };
    return lineLayout(lineBuilder, inlineItemList, needsLayoutRange, previousLine(), constraints, lineDamage);
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicSizes(const InlineDamage* lineDamage)
{
    auto& inlineContentCache = this->inlineContentCache();
    if (lineDamage)
        inlineContentCache.resetIntrinsicWidthConstraints();

    if (auto intrinsicSizes = inlineContentCache.intrinsicWidthConstraints())
        return *intrinsicSizes;

    auto needsLayoutStartPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
    auto needsInlineItemListUpdate = inlineContentCache.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemListUpdate)
        InlineItemsBuilder { inlineContentCache, root() }.build(needsLayoutStartPosition);

    auto mayUseSimplifiedTextOnlyInlineLayout = TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), inlineContentCache);
    auto intrinsicWidthHandler = IntrinsicWidthHandler { *this, inlineContentCache.inlineItems().content(), mayUseSimplifiedTextOnlyInlineLayout };
    auto intrinsicSizes = intrinsicWidthHandler.computedIntrinsicSizes();
    inlineContentCache.setIntrinsicWidthConstraints(intrinsicSizes);
    if (intrinsicWidthHandler.maximumIntrinsicWidthResult())
        inlineContentCache.setMaximumIntrinsicWidthLayoutResult(WTFMove(*intrinsicWidthHandler.maximumIntrinsicWidthResult()));
    return intrinsicSizes;
}

LayoutUnit InlineFormattingContext::maximumContentSize()
{
    auto& inlineContentCache = this->inlineContentCache();
    if (auto intrinsicWidthConstraints = inlineContentCache.intrinsicWidthConstraints())
        return intrinsicWidthConstraints->maximum;

    auto mayUseSimplifiedTextOnlyInlineLayout = TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), inlineContentCache);
    return IntrinsicWidthHandler { *this, inlineContentCache.inlineItems().content(), mayUseSimplifiedTextOnlyInlineLayout }.maximumContentSize();
}

static bool mayExitFromPartialLayout(const InlineDamage& lineDamage, size_t lineIndex, const InlineDisplay::Boxes& newContent)
{
    if (lineDamage.start()->lineIndex == lineIndex) {
        // Never stop at the damaged line. Adding trailing overflowing content could easily produce the
        // same set of display boxes for the first damaged line.
        return false;
    }
    auto trailingContentFromPreviousLayout = lineDamage.trailingContentForLine(lineIndex);
    return trailingContentFromPreviousLayout ? (!newContent.isEmpty() && *trailingContentFromPreviousLayout == newContent.last()) : false;
}

InlineLayoutResult InlineFormattingContext::lineLayout(AbstractLineBuilder& lineBuilder, const InlineItemList& inlineItemList, InlineItemRange needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, const InlineDamage* lineDamage)
{
    ASSERT(!needsLayoutRange.isEmpty());

    auto layoutResult = InlineLayoutResult { };
    if (!needsLayoutRange.start)
        layoutResult.displayContent.boxes.reserveInitialCapacity(inlineItemList.size());

    auto isPartialLayout = lineDamage && lineDamage->start();
    if (!isPartialLayout && createDisplayContentForLineFromCachedContent(constraints, layoutResult)) {
        ASSERT(!previousLine);
        layoutResult.range = InlineLayoutResult::Range::Full;
        return layoutResult;
    }

    auto floatingContext = this->floatingContext();
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingUtils().initialLineHeight(!previousLine.has_value()) };
        auto lineInput = LineInput { { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect };
        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;

        auto lineLayoutResult = lineBuilder.layoutInlineContent(lineInput, previousLine);
        auto lineBox = LineBoxBuilder { *this, lineLayoutResult }.build(lineIndex);
        auto lineLogicalRect = createDisplayContentForInlineContent(lineBox, lineLayoutResult, constraints, layoutResult.displayContent);
        updateBoxGeometryForPlacedFloats(lineLayoutResult.floatContent.placedFloats);
        updateInlineLayoutStateWithLineLayoutResult(lineLayoutResult, lineLogicalRect, floatingContext);

        auto lineContentEnd = lineLayoutResult.inlineItemRange.end;
        leadingInlineItemPosition = InlineFormattingUtils::leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, needsLayoutRange.end);
        auto isLastLine = leadingInlineItemPosition == needsLayoutRange.end && lineLayoutResult.floatContent.suspendedFloats.isEmpty();
        if (isLastLine) {
            layoutResult.range = !isPartialLayout ? InlineLayoutResult::Range::Full : InlineLayoutResult::Range::FullFromDamage;
            break;
        }
        if (isPartialLayout && mayExitFromPartialLayout(*lineDamage, lineIndex, layoutResult.displayContent.boxes)) {
            layoutResult.range = InlineLayoutResult::Range::PartialFromDamage;
            break;
        }

        previousLine = PreviousLine { lineIndex, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak(), lineLayoutResult.directionality.inlineBaseDirection, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
        previousLineEnd = lineContentEnd;
        lineLogicalTop = formattingUtils().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext);
    }
    InlineDisplayLineBuilder::addLineClampTrailingLinkBoxIfApplicable(*this, layoutState(), layoutResult.displayContent);
    return layoutResult;
}

void InlineFormattingContext::layoutFloatContentOnly(const ConstraintsForInlineContent& constraints)
{
    ASSERT(!root().hasInFlowChild());

    auto& inlineContentCache = this->inlineContentCache();
    auto floatingContext = this->floatingContext();
    auto& placedFloats = layoutState().placedFloats();

    InlineItemsBuilder { inlineContentCache, root() }.build({ });

    for (auto& inlineItem : inlineContentCache.inlineItems().content()) {
        if (inlineItem.isFloat()) {
            auto& floatBox = inlineItem.layoutBox();
            auto& floatBoxGeometry = geometryForBox(floatBox);
            auto staticPosition = LayoutPoint { constraints.horizontal().logicalLeft, constraints.logicalTop() };
            staticPosition.move(floatBoxGeometry.marginStart(), floatBoxGeometry.marginBefore());
            floatBoxGeometry.setTopLeft(staticPosition);

            auto floatBoxTopLeft = floatingContext.positionForFloat(floatBox, floatBoxGeometry, constraints.horizontal());
            floatBoxGeometry.setTopLeft(floatBoxTopLeft);
            placedFloats.append(floatingContext.makeFloatItem(floatBox, floatBoxGeometry));
            continue;
        }
        ASSERT_NOT_REACHED();
    }
}

static LineEndingEllipsisPolicy lineEndingEllipsisPolicy(const RenderStyle& rootStyle, size_t numberOfLines, std::optional<size_t> numberOfVisibleLinesAllowed)
{
    // We may have passed the line-clamp line with overflow visible.
    if (numberOfVisibleLinesAllowed && numberOfLines < *numberOfVisibleLinesAllowed) {
        // If the next call to layoutInlineContent() won't produce a line with content (e.g. only floats), we'll end up here again.
        auto shouldApplyClampWhenApplicable = *numberOfVisibleLinesAllowed - numberOfLines == 1;
        if (shouldApplyClampWhenApplicable)
            return LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection;
    }
    // Truncation is in effect when the block container has overflow other than visible.
    if (rootStyle.overflowX() != Overflow::Visible && rootStyle.textOverflow() == TextOverflow::Ellipsis)
        return LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection;
    return LineEndingEllipsisPolicy::No;
}

void InlineFormattingContext::updateInlineLayoutStateWithLineLayoutResult(const LineLayoutResult& lineLayoutResult, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext)
{
    if (auto firstLineGap = lineLayoutResult.lineGeometry.initialLetterClearGap) {
        ASSERT(!layoutState().clearGapBeforeFirstLine());
        layoutState().setClearGapBeforeFirstLine(*firstLineGap);
    }

    if (lineLayoutResult.isFirstLast.isLastLineWithInlineContent)
        layoutState().setClearGapAfterLastLine(formattingUtils().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

    lineLayoutResult.endsWithHyphen ? layoutState().incrementSuccessiveHyphenatedLineCount() : layoutState().resetSuccessiveHyphenatedLineCount();
}

void InlineFormattingContext::updateBoxGeometryForPlacedFloats(const LineLayoutResult::PlacedFloatList& placedFloats)
{
    for (auto& floatItem : placedFloats) {
        if (!floatItem.layoutBox()) {
            ASSERT_NOT_REACHED();
            // We should not be placing intrusive floats coming from parent BFC.
            continue;
        }
        auto& boxGeometry = geometryForBox(*floatItem.layoutBox());
        auto usedGeometry = floatItem.boxGeometry();
        boxGeometry.setTopLeft(BoxGeometry::borderBoxTopLeft(usedGeometry));
        // Adopt trimmed inline direction margin.
        boxGeometry.setHorizontalMargin(usedGeometry.horizontalMargin());
    }
}

InlineRect InlineFormattingContext::createDisplayContentForInlineContent(const LineBox& lineBox, const LineLayoutResult& lineLayoutResult, const ConstraintsForInlineContent& constraints, InlineDisplay::Content& displayContent)
{
    auto lineIndex = lineBox.lineIndex();
    auto numberOfVisibleLinesAllowed = [&] () -> std::optional<size_t> {
        if (auto lineClamp = layoutState().parentBlockLayoutState().lineClamp())
            return lineClamp->maximumLineCount > lineClamp->currentLineCount ? lineClamp->maximumLineCount - lineClamp->currentLineCount : 0;
        return { };
    }();

    auto lineIsFullyTruncatedInBlockDirection = numberOfVisibleLinesAllowed && lineIndex + 1 > *numberOfVisibleLinesAllowed;
    auto displayLine = InlineDisplayLineBuilder { *this, constraints }.build(lineLayoutResult, lineBox, lineIsFullyTruncatedInBlockDirection);
    auto boxes = InlineDisplayContentBuilder { *this, constraints, displayLine, lineIndex }.build(lineLayoutResult, lineBox);

    auto ellipsisPolicy = lineEndingEllipsisPolicy(root().style(), lineIndex, numberOfVisibleLinesAllowed);
    if (auto ellipsisRect = InlineDisplayLineBuilder::trailingEllipsisVisualRectAfterTruncation(ellipsisPolicy, displayLine, boxes, lineLayoutResult.isFirstLast.isLastLineWithInlineContent)) {
        displayLine.setEllipsisVisualRect(*ellipsisRect);
        if (ellipsisPolicy == LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection)
            layoutState().setClampedLineIndex(lineIndex);
    }

    displayContent.boxes.appendVector(WTFMove(boxes));
    displayContent.lines.append(displayLine);
    return InlineFormattingUtils::flipVisualRectToLogicalForWritingMode(displayContent.lines.last().lineBoxRect(), root().style().writingMode());
}

void InlineFormattingContext::resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineLayoutResult::SuspendedFloatList& suspendedFloats, LayoutPoint topleft)
{
    if (needsDisplayContentRange.isEmpty() && suspendedFloats.isEmpty())
        return;

    auto& inlineItemList = inlineContentCache().inlineItems().content();
    for (size_t index = needsDisplayContentRange.startIndex(); index < needsDisplayContentRange.endIndex(); ++index) {
        auto& inlineItem = inlineItemList[index];
        auto hasBoxGeometry = inlineItem.isBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak() || inlineItem.isInlineBoxStart();
        if (!hasBoxGeometry)
            continue;
        auto& boxGeometry = geometryForBox(inlineItem.layoutBox());
        boxGeometry.setTopLeft(topleft);
        boxGeometry.setContentBoxHeight({ });
        boxGeometry.setContentBoxWidth({ });
    }
}

bool InlineFormattingContext::createDisplayContentForLineFromCachedContent(const ConstraintsForInlineContent& constraints, InlineLayoutResult& layoutResult)
{
    auto& inlineContentCache = this->inlineContentCache();

    if (!inlineContentCache.maximumIntrinsicWidthLayoutResult())
        return false;

    auto& maximumIntrinsicWidthResultForSingleLine = inlineContentCache.maximumIntrinsicWidthLayoutResult();
    auto horizontalAvailableSpace = constraints.horizontal().logicalWidth;
    if (maximumIntrinsicWidthResultForSingleLine->constraint > horizontalAvailableSpace) {
        inlineContentCache.clearMaximumIntrinsicWidthLayoutResult();
        return false;
    }
    if (!layoutState().placedFloats().isEmpty()) {
        inlineContentCache.clearMaximumIntrinsicWidthLayoutResult();
        return false;
    }

    auto& lineBreakingResult = maximumIntrinsicWidthResultForSingleLine->result;
    auto restoreTrimmedTrailingWhitespaceIfApplicable = [&] {
        if (root().style().lineBreak() != LineBreak::AfterWhiteSpace || !lineBreakingResult.trimmedTrailingWhitespaceWidth)
            return;
        if (ceiledLayoutUnit(lineBreakingResult.contentGeometry.logicalWidth) + LayoutUnit::epsilon() <= horizontalAvailableSpace)
            return;
        if (!Line::restoreTrimmedTrailingWhitespace(lineBreakingResult.trimmedTrailingWhitespaceWidth, lineBreakingResult.inlineContent)) {
            ASSERT_NOT_REACHED();
            inlineContentCache.clearMaximumIntrinsicWidthLayoutResult();
            return;
        }
        lineBreakingResult.contentGeometry.logicalWidth += lineBreakingResult.trimmedTrailingWhitespaceWidth;
        lineBreakingResult.contentGeometry.logicalRightIncludingNegativeMargin += lineBreakingResult.trimmedTrailingWhitespaceWidth;
        lineBreakingResult.trimmedTrailingWhitespaceWidth = { };
    };
    restoreTrimmedTrailingWhitespaceIfApplicable();

    lineBreakingResult.lineGeometry.logicalTopLeft = { constraints.horizontal().logicalLeft, constraints.logicalTop() };
    lineBreakingResult.lineGeometry.logicalWidth = constraints.horizontal().logicalWidth;
    lineBreakingResult.contentGeometry.logicalLeft = InlineFormattingUtils::horizontalAlignmentOffset(root().style(), lineBreakingResult.contentGeometry.logicalWidth, lineBreakingResult.lineGeometry.logicalWidth, lineBreakingResult.hangingContent.logicalWidth, lineBreakingResult.inlineContent, true);
    auto lineBox = LineBoxBuilder { *this, lineBreakingResult }.build({ });
    createDisplayContentForInlineContent(lineBox, lineBreakingResult, constraints, layoutResult.displayContent);
    return true;
}

void InlineFormattingContext::initializeInlineLayoutState(const LayoutState& layoutState)
{
    auto& inlineLayoutState = this->layoutState();

    if (auto limitLinesValue = root().style().hyphenationLimitLines(); limitLinesValue != RenderStyle::initialHyphenationLimitLines())
        inlineLayoutState.setHyphenationLimitLines(limitLinesValue);
    // FIXME: Remove when IFC takes care of running layout on inline-blocks.
    inlineLayoutState.setShouldNotSynthesizeInlineBlockBaseline();
    if (layoutState.inStandardsMode())
        inlineLayoutState.setInStandardsMode();
}

#if ASSERT_ENABLED
static inline bool isOkToAccessBoxGeometry(const Box& layoutBox, const ElementBox& rootBlockContainer, std::optional<InlineFormattingContext::EscapeReason> escapeReason)
{
    if (escapeReason == InlineFormattingContext::EscapeReason::InkOverflowNeedsInitialContiningBlockForStrokeWidth && is<InitialContainingBlock>(layoutBox))
        return true;
    // This is the non-escape case of accessing a box's geometry information within the same formatting context when computing static position for out-of-flow boxes.
    if (layoutBox.isOutOfFlowPositioned())
        return true;
    auto containingBlock = [&]() -> const Box* {
        for (auto* ancestor = &layoutBox.parent(); !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) {
            if (ancestor->isContainingBlockForInFlow())
                return ancestor;
        }
        return nullptr;
    };
    // This is the non-escape case of accessing a box's geometry information within the same formatting context.
    return containingBlock() == &rootBlockContainer;
};
#endif

const BoxGeometry& InlineFormattingContext::geometryForBox(const Box& layoutBox, std::optional<EscapeReason> escapeReason) const
{
    ASSERT_UNUSED(escapeReason, isOkToAccessBoxGeometry(layoutBox, root(), escapeReason));
    return m_layoutState.geometryForBox(layoutBox);
}

BoxGeometry& InlineFormattingContext::geometryForBox(const Box& layoutBox, std::optional<EscapeReason> escapeReason)
{
    ASSERT_UNUSED(escapeReason, isOkToAccessBoxGeometry(layoutBox, root(), escapeReason));
    return m_layoutState.ensureGeometryForBox(layoutBox);
}

}
}


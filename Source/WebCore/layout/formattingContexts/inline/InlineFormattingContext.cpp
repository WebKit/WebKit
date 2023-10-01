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
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineFormattingState.h"
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

InlineFormattingContext::InlineFormattingContext(const ElementBox& formattingContextRoot, InlineFormattingState& formattingState, BlockLayoutState& parentBlockLayoutState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_inlineFormattingGeometry(*this)
    , m_inlineFormattingQuirks(*this)
    , m_inlineLayoutState(parentBlockLayoutState)
{
    initializeLayoutState();
}

InlineLayoutResult InlineFormattingContext::layout(const ConstraintsForInlineContent& constraints, const InlineDamage* lineDamage)
{
    auto& floatingState = this->floatingState();
    if (!root().hasInFlowChild() && !root().hasOutOfFlowChild()) {
        // Float only content does not support partial layout.
        ASSERT(!lineDamage);
        layoutFloatContentOnly(constraints);
        return { { }, InlineLayoutResult::Range::Full };
    }

    auto& inlineFormattingState = formattingState();
    auto needsLayoutStartPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemsUpdate) {
        // FIXME: This should go to invalidation.
        inlineFormattingState.clearMaximumIntrinsicWidthLayoutResult();
        InlineItemsBuilder { *this }.build(needsLayoutStartPosition);
    }

    auto& inlineItems = inlineFormattingState.inlineItems();
    auto needsLayoutRange = InlineItemRange { needsLayoutStartPosition, { inlineItems.size(), 0 } };

    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutStartPosition)
            return { };
        if (!lineDamage || !lineDamage->start())
            return { };
        auto lastLineIndex = lineDamage->start()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    };

    if (root().style().textWrap() == TextWrap::Balance) {
        auto balancer = InlineContentBalancer { *this, inlineItems, constraints.horizontal() };
        auto balancedLineWidths = balancer.computeBalanceConstraints();
        if (balancedLineWidths)
            inlineLayoutState().setAvailableLineWidthOverride({ *balancedLineWidths });
    }

    if (TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), formattingState(), &floatingState)) {
        auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { *this, constraints.horizontal(), inlineItems };
        return lineLayout(simplifiedLineBuilder, inlineItems, needsLayoutRange, previousLine(), constraints, lineDamage);
    }
    auto lineBuilder = LineBuilder { *this, constraints.horizontal(), inlineItems };
    return lineLayout(lineBuilder, inlineItems, needsLayoutRange, previousLine(), constraints, lineDamage);
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicSizes(const InlineDamage* lineDamage)
{
    auto& inlineFormattingState = formattingState();
    if (lineDamage)
        inlineFormattingState.resetIntrinsicWidthConstraints();

    if (auto intrinsicSizes = inlineFormattingState.intrinsicWidthConstraints())
        return *intrinsicSizes;

    auto needsLayoutStartPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { *this }.build(needsLayoutStartPosition);

    auto intrinsicWidthHandler = IntrinsicWidthHandler { *this };
    auto intrinsicSizes = intrinsicWidthHandler.computedIntrinsicSizes();
    inlineFormattingState.setIntrinsicWidthConstraints(intrinsicSizes);
    if (intrinsicWidthHandler.maximumIntrinsicWidthResult())
        inlineFormattingState.setMaximumIntrinsicWidthLayoutResult(WTFMove(*intrinsicWidthHandler.maximumIntrinsicWidthResult()));
    return intrinsicSizes;
}

LayoutUnit InlineFormattingContext::maximumContentSize()
{
    auto& inlineFormattingState = formattingState();
    if (auto intrinsicWidthConstraints = inlineFormattingState.intrinsicWidthConstraints())
        return intrinsicWidthConstraints->maximum;

    return IntrinsicWidthHandler { *this }.maximumContentSize();
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

InlineLayoutResult InlineFormattingContext::lineLayout(AbstractLineBuilder& lineBuilder, const InlineItems& inlineItems, InlineItemRange needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, const InlineDamage* lineDamage)
{
    ASSERT(!needsLayoutRange.isEmpty());

    auto layoutResult = InlineLayoutResult { };
    if (!needsLayoutRange.start)
        layoutResult.displayContent.boxes.reserveInitialCapacity(inlineItems.size());

    auto isPartialLayout = lineDamage && lineDamage->start();
    if (!isPartialLayout && createDisplayContentForLineFromCachedContent(constraints, layoutResult)) {
        ASSERT(!previousLine);
        layoutResult.range = InlineLayoutResult::Range::Full;
        return layoutResult;
    }

    auto floatingContext = FloatingContext { *this, floatingState() };
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingGeometry().initialLineHeight(!previousLine.has_value()) };
        auto lineInput = LineInput { { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect };
        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;

        auto lineLayoutResult = lineBuilder.layoutInlineContent(lineInput, previousLine);
        auto lineBox = LineBoxBuilder { *this, lineLayoutResult }.build(lineIndex);
        auto lineLogicalRect = createDisplayContentForInlineContent(lineBox, lineLayoutResult, constraints, layoutResult.displayContent);
        updateBoxGeometryForPlacedFloats(lineLayoutResult.floatContent.placedFloats);
        updateInlineLayoutStateWithLineLayoutResult(lineLayoutResult, lineLogicalRect, floatingContext);

        auto lineContentEnd = lineLayoutResult.inlineItemRange.end;
        leadingInlineItemPosition = InlineFormattingGeometry::leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, needsLayoutRange.end);
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
        lineLogicalTop = formattingGeometry().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext);
    }
    InlineDisplayLineBuilder::addLineClampTrailingLinkBoxIfApplicable(*this, inlineLayoutState(), layoutResult.displayContent);
    return layoutResult;
}

void InlineFormattingContext::layoutFloatContentOnly(const ConstraintsForInlineContent& constraints)
{
    ASSERT(!root().hasInFlowChild());

    auto& inlineFormattingState = formattingState();
    auto& floatingState = this->floatingState();
    auto floatingContext = FloatingContext { *this, floatingState };

    InlineItemsBuilder { *this }.build({ });

    for (auto& inlineItem : inlineFormattingState.inlineItems()) {
        if (inlineItem.isFloat()) {
            auto& floatBox = inlineItem.layoutBox();
            auto& floatBoxGeometry = inlineFormattingState.boxGeometry(floatBox);
            auto staticPosition = LayoutPoint { constraints.horizontal().logicalLeft, constraints.logicalTop() };
            staticPosition.move(floatBoxGeometry.marginStart(), floatBoxGeometry.marginBefore());
            floatBoxGeometry.setTopLeft(staticPosition);

            auto floatBoxTopLeft = floatingContext.positionForFloat(floatBox, floatBoxGeometry, constraints.horizontal());
            floatBoxGeometry.setTopLeft(floatBoxTopLeft);
            floatingState.append(floatingContext.makeFloatItem(floatBox, floatBoxGeometry));
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
        ASSERT(!inlineLayoutState().clearGapBeforeFirstLine());
        inlineLayoutState().setClearGapBeforeFirstLine(*firstLineGap);
    }

    if (lineLayoutResult.isFirstLast.isLastLineWithInlineContent)
        inlineLayoutState().setClearGapAfterLastLine(formattingGeometry().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

    lineLayoutResult.endsWithHyphen ? inlineLayoutState().incrementSuccessiveHyphenatedLineCount() : inlineLayoutState().resetSuccessiveHyphenatedLineCount();
}

void InlineFormattingContext::updateBoxGeometryForPlacedFloats(const LineLayoutResult::PlacedFloatList& placedFloats)
{
    for (auto& floatItem : placedFloats) {
        if (!floatItem.layoutBox()) {
            ASSERT_NOT_REACHED();
            // We should not be placing intrusive floats coming from parent BFC.
            continue;
        }
        auto& boxGeometry = formattingState().boxGeometry(*floatItem.layoutBox());
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
        if (auto lineClamp = inlineLayoutState().parentBlockLayoutState().lineClamp())
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
            inlineLayoutState().setClampedLineIndex(lineIndex);
    }

    displayContent.boxes.appendVector(WTFMove(boxes));
    displayContent.lines.append(displayLine);
    return InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(displayContent.lines.last().lineBoxRect(), root().style().writingMode());
}

void InlineFormattingContext::resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineLayoutResult::SuspendedFloatList& suspendedFloats, LayoutPoint topleft)
{
    if (needsDisplayContentRange.isEmpty() && suspendedFloats.isEmpty())
        return;

    auto& inlineItems = formattingState().inlineItems();
    for (size_t index = needsDisplayContentRange.startIndex(); index < needsDisplayContentRange.endIndex(); ++index) {
        auto& inlineItem = inlineItems[index];
        auto hasBoxGeometry = inlineItem.isBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak() || inlineItem.isInlineBoxStart();
        if (!hasBoxGeometry)
            continue;
        auto& boxGeometry = formattingState().boxGeometry(inlineItem.layoutBox());
        boxGeometry.setTopLeft(topleft);
        boxGeometry.setContentBoxHeight({ });
        boxGeometry.setContentBoxWidth({ });
    }
}

bool InlineFormattingContext::createDisplayContentForLineFromCachedContent(const ConstraintsForInlineContent& constraints, InlineLayoutResult& layoutResult)
{
    auto& inlineFormattingState = formattingState();

    if (!inlineFormattingState.maximumIntrinsicWidthLayoutResult())
        return false;

    auto& maximumIntrinsicWidthResultForSingleLine = inlineFormattingState.maximumIntrinsicWidthLayoutResult();
    auto horizontalAvailableSpace = constraints.horizontal().logicalWidth;
    if (maximumIntrinsicWidthResultForSingleLine->constraint > horizontalAvailableSpace) {
        inlineFormattingState.clearMaximumIntrinsicWidthLayoutResult();
        return false;
    }
    if (!floatingState().isEmpty()) {
        inlineFormattingState.clearMaximumIntrinsicWidthLayoutResult();
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
            inlineFormattingState.clearMaximumIntrinsicWidthLayoutResult();
            return;
        }
        lineBreakingResult.contentGeometry.logicalWidth += lineBreakingResult.trimmedTrailingWhitespaceWidth;
        lineBreakingResult.contentGeometry.logicalRightIncludingNegativeMargin += lineBreakingResult.trimmedTrailingWhitespaceWidth;
        lineBreakingResult.trimmedTrailingWhitespaceWidth = { };
    };
    restoreTrimmedTrailingWhitespaceIfApplicable();

    lineBreakingResult.lineGeometry.logicalTopLeft = { constraints.horizontal().logicalLeft, constraints.logicalTop() };
    lineBreakingResult.lineGeometry.logicalWidth = constraints.horizontal().logicalWidth;
    lineBreakingResult.contentGeometry.logicalLeft = InlineFormattingGeometry::horizontalAlignmentOffset(root().style(), lineBreakingResult.contentGeometry.logicalWidth, lineBreakingResult.lineGeometry.logicalWidth, lineBreakingResult.hangingContent.logicalWidth, lineBreakingResult.inlineContent, true);
    auto lineBox = LineBoxBuilder { *this, lineBreakingResult }.build({ });
    createDisplayContentForInlineContent(lineBox, lineBreakingResult, constraints, layoutResult.displayContent);
    return true;
}

void InlineFormattingContext::initializeLayoutState()
{
    if (auto limitLinesValue = root().style().hyphenationLimitLines(); limitLinesValue != RenderStyle::initialHyphenationLimitLines())
        inlineLayoutState().setHyphenationLimitLines(limitLinesValue);
}

}
}


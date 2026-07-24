/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FlexFormattingUtils.h"

#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderFlexibleBox.h"
#include "RenderReplaced.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {

static bool isSVGRootWithIntrinsicAspectRatio(const RenderBox& flexItem)
{
    if (!flexItem.isRenderOrLegacyRenderSVGRoot())
        return false;
    // It's common for some replaced elements, such as SVGs, to have intrinsic aspect ratios but no intrinsic sizes.
    // That's why it isn't enough just to check for intrinsic sizes in those cases.
    return flexItem.preferredAspectRatioAsSize().aspectRatioDouble() > 0;
}

FlexFormattingUtils::FlexFormattingUtils(const RenderFlexibleBox& flexBox)
    : m_flexBox(flexBox)
{
}

LayoutUnit FlexFormattingUtils::flowAwareBorderStart() const
{
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexBox().borderLeft() : flexBox().borderRight();
    return isLeftToRightFlow() ? flexBox().borderTop() : flexBox().borderBottom();
}

LayoutUnit FlexFormattingUtils::flowAwareBorderEnd() const
{
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexBox().borderRight() : flexBox().borderLeft();
    return isLeftToRightFlow() ? flexBox().borderBottom() : flexBox().borderTop();
}

LayoutUnit FlexFormattingUtils::flowAwareBorderBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexBox().borderTop();
    case FlowDirection::BottomToTop:
        return flexBox().borderBottom();
    case FlowDirection::LeftToRight:
        return flexBox().borderLeft();
    case FlowDirection::RightToLeft:
        return flexBox().borderRight();
    }
    ASSERT_NOT_REACHED();
    return flexBox().borderTop();
}

LayoutUnit FlexFormattingUtils::flowAwareBorderAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexBox().borderBottom();
    case FlowDirection::BottomToTop:
        return flexBox().borderTop();
    case FlowDirection::LeftToRight:
        return flexBox().borderRight();
    case FlowDirection::RightToLeft:
        return flexBox().borderLeft();
    }
    ASSERT_NOT_REACHED();
    return flexBox().borderTop();
}

LayoutUnit FlexFormattingUtils::flowAwarePaddingStart() const
{
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexBox().paddingLeft() : flexBox().paddingRight();
    return isLeftToRightFlow() ? flexBox().paddingTop() : flexBox().paddingBottom();
}

LayoutUnit FlexFormattingUtils::flowAwarePaddingEnd() const
{
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexBox().paddingRight() : flexBox().paddingLeft();
    return isLeftToRightFlow() ? flexBox().paddingBottom() : flexBox().paddingTop();
}

LayoutUnit FlexFormattingUtils::flowAwarePaddingBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexBox().paddingTop();
    case FlowDirection::BottomToTop:
        return flexBox().paddingBottom();
    case FlowDirection::LeftToRight:
        return flexBox().paddingLeft();
    case FlowDirection::RightToLeft:
        return flexBox().paddingRight();
    }
    ASSERT_NOT_REACHED();
    return flexBox().paddingTop();
}

LayoutUnit FlexFormattingUtils::flowAwarePaddingAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexBox().paddingBottom();
    case FlowDirection::BottomToTop:
        return flexBox().paddingTop();
    case FlowDirection::LeftToRight:
        return flexBox().paddingRight();
    case FlowDirection::RightToLeft:
        return flexBox().paddingLeft();
    }
    ASSERT_NOT_REACHED();
    return flexBox().paddingTop();
}

LayoutUnit FlexFormattingUtils::flowAwareMarginStartForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexItem->marginLeft() : flexItem->marginRight();
    return isLeftToRightFlow() ? flexItem->marginTop() : flexItem->marginBottom();
}

LayoutUnit FlexFormattingUtils::flowAwareMarginEndForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    if (isHorizontalFlow(flexBox()))
        return isLeftToRightFlow() ? flexItem->marginRight() : flexItem->marginLeft();
    return isLeftToRightFlow() ? flexItem->marginBottom() : flexItem->marginTop();
}

LayoutUnit FlexFormattingUtils::flowAwareMarginBeforeForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexItem->marginTop();
    case FlowDirection::BottomToTop:
        return flexItem->marginBottom();
    case FlowDirection::LeftToRight:
        return flexItem->marginLeft();
    case FlowDirection::RightToLeft:
        return flexItem->marginRight();
    }
    ASSERT_NOT_REACHED();
    return flexBox().marginTop();
}

LayoutUnit FlexFormattingUtils::crossAxisExtentForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow(flexBox()) ? flexItem.borderBoxHeight() : flexItem.borderBoxWidth();
}

LayoutUnit FlexFormattingUtils::mainAxisExtentForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow(flexBox()) ? flexItem.borderBoxSize().width() : flexItem.borderBoxSize().height();
}

LayoutUnit FlexFormattingUtils::mainAxisExtent() const
{
    return isHorizontalFlow(flexBox()) ? flexBox().borderBoxSize().width() : flexBox().borderBoxSize().height();
}

LayoutUnit FlexFormattingUtils::crossAxisContentExtent(const RenderFlexibleBox& flexBox)
{
    return isHorizontalFlow(flexBox) ? flexBox.contentBoxHeight() : flexBox.contentBoxWidth();
}

LayoutUnit FlexFormattingUtils::computeGap(const RenderFlexibleBox& flexBox, GapType gapType)
{
    // row-gap is used for gaps between flex items in column flows or for gaps between lines in row flows.
    bool usesRowGap = (gapType == GapType::BetweenItems) == isColumnFlow(flexBox);
    auto& gap = usesRowGap ? flexBox.style().rowGap() : flexBox.style().columnGap();
    if (gap.isNormal()) [[likely]]
        return { };

    auto availableSize = usesRowGap ? flexBox.availableLogicalHeightForPercentageComputation().value_or(0_lu) : flexBox.contentBoxLogicalWidth();
    return Style::evaluateMinimum<LayoutUnit>(gap, availableSize, flexBox.style().usedZoomForLength());
}

LayoutUnit FlexFormattingUtils::mainAxisMarginExtentForFlexItem(const RenderBox& flexItem) const
{
    if (!flexItem.needsLayout())
        return isHorizontalFlow(flexBox()) ? flexItem.horizontalMarginExtent() : flexItem.verticalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow(flexBox()))
        flexItem.computeInlineDirectionMargins(flexBox(), flexItem.containingBlockLogicalWidthForContent(), flexItem.logicalWidth(), { }, marginStart, marginEnd);
    else
        flexItem.computeBlockDirectionMargins(flexBox(), marginStart, marginEnd);
    return marginStart + marginEnd;
}

LayoutUnit FlexFormattingUtils::crossAxisMarginExtentForFlexItem(const RenderBox& flexItem)
{
    CheckedRef flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    if (!flexItem.needsLayout())
        return isHorizontalFlow(flexBox) ? flexItem.verticalMarginExtent() : flexItem.horizontalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow(flexBox))
        flexItem.computeBlockDirectionMargins(flexBox, marginStart, marginEnd);
    else
        flexItem.computeInlineDirectionMargins(flexBox, flexItem.containingBlockLogicalWidthForContent(), flexItem.logicalWidth(), { }, marginStart, marginEnd);
    return marginStart + marginEnd;
}

LayoutUnit FlexFormattingUtils::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow(flexBox()) ? flexBox().horizontalScrollbarHeight() : flexBox().verticalScrollbarWidth();
}

LayoutUnit FlexFormattingUtils::mainAxisScrollbarExtent() const
{
    return isHorizontalFlow(flexBox()) ? flexBox().verticalScrollbarWidth() : flexBox().horizontalScrollbarHeight();
}

const Style::PreferredSize& FlexFormattingUtils::preferredMainSizeLengthForFlexItem(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    return isHorizontalFlow(flexBox) ? flexItem.style().width() : flexItem.style().height();
}

const Style::MinimumSize& FlexFormattingUtils::minMainSizeLengthForFlexItem(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    return isHorizontalFlow(flexBox) ? flexItem.style().minWidth() : flexItem.style().minHeight();
}

const Style::MaximumSize& FlexFormattingUtils::maxMainSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    auto& style = flexLayoutItem.style();
    return isHorizontalFlow(flexBox()) ? style.maxWidth() : style.maxHeight();
}

const Style::PreferredSize& FlexFormattingUtils::preferredCrossSizeLengthForFlexItem(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    return isHorizontalFlow(flexBox) ? flexItem.style().height() : flexItem.style().width();
}

const Style::MinimumSize& FlexFormattingUtils::minCrossSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    auto& style = flexLayoutItem.style();
    return isHorizontalFlow(flexBox()) ? style.minHeight() : style.minWidth();
}

const Style::MaximumSize& FlexFormattingUtils::maxCrossSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    auto& style = flexLayoutItem.style();
    return isHorizontalFlow(flexBox()) ? style.maxHeight() : style.maxWidth();
}

Overflow FlexFormattingUtils::mainAxisOverflowForFlexItem(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    if (isHorizontalFlow(flexBox))
        return flexItem.style().overflowX();
    return flexItem.style().overflowY();
}

OverflowAlignment FlexFormattingUtils::overflowAlignmentForFlexItem(const RenderBox& flexItem) const
{
    CheckedRef containerStyle = flexBox().style();
    return flexItem.style().alignSelf().resolve(containerStyle.ptr()).overflow();
}

bool FlexFormattingUtils::hasAutoMarginsInCrossAxis(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    if (isHorizontalFlow(flexBox))
        return flexItem.style().marginTop().isAuto() || flexItem.style().marginBottom().isAuto();
    return flexItem.style().marginLeft().isAuto() || flexItem.style().marginRight().isAuto();
}

// https://drafts.csswg.org/css-flexbox/#min-size-auto
bool FlexFormattingUtils::useContentBasedMinimumSize(const RenderBox& flexItem)
{
    auto minSize = minMainSizeLengthForFlexItem(flexItem);
    // min, max and fit-content are equivalent to the automatic size for block sizes https://drafts.csswg.org/css-sizing-3/#valdef-width-min-content.
    // Unlike auto, these are explicit author values so the overflow gate does not apply.
    bool flexItemBlockSizeIsEquivalentToAutomaticSize = !mainAxisIsFlexItemInlineAxis(flexItem) && (minSize.isMinContent() || minSize.isMaxContent() || minSize.isFitContent());
    if (flexItemBlockSizeIsEquivalentToAutomaticSize)
        return true;

    auto computedOverflowIsNotScrollable = [&flexItem]() {
        auto overflow = mainAxisOverflowForFlexItem(flexItem);
        return overflow == Overflow::Visible || overflow == Overflow::Clip;
    };

    return minSize.isAuto() && computedOverflowIsNotScrollable();
}

double FlexFormattingUtils::preferredAspectRatioForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    auto flexItemAspectRatio = [&] {
        auto flexItemIntrinsicSize = LayoutSize { flexItem->intrinsicLogicalWidth(), flexItem->intrinsicLogicalHeight() };
        if (flexItem->isRenderOrLegacyRenderSVGRoot())
            return flexItem->preferredAspectRatioAsSize().aspectRatioDouble();
        if (flexItem->style().aspectRatio().isRatio() || (flexItem->style().aspectRatio().isAutoAndRatio() && flexItemIntrinsicSize.isEmpty()))
            return flexItem->style().logicalAspectRatio();
        if (is<RenderReplaced>(flexItem))
            return flexItem->preferredAspectRatioAsSize().aspectRatioDouble();

        ASSERT(flexItem->intrinsicLogicalHeight());
        return flexItem->intrinsicLogicalWidth().toDouble() / flexItem->intrinsicLogicalHeight().toDouble();
    };

    if (mainAxisIsFlexItemInlineAxis(flexItem))
        return flexItemAspectRatio();
    return 1 / flexItemAspectRatio();
}

bool FlexFormattingUtils::flexItemHasAspectRatio(const RenderBox& flexItem)
{
    return flexItem.hasIntrinsicAspectRatio()
        || flexItem.style().aspectRatio().hasRatio()
        || isSVGRootWithIntrinsicAspectRatio(flexItem);
}

bool FlexFormattingUtils::canResolveFullyConstrainedLogicalHeight(const RenderFlexibleBox& flexBox)
{
    // The height is fully constrained by insets (CSS Sizing 3 section 3.2.1), but we can
    // only resolve it if the containing block's height is itself definite right now.
    // In nested flex layouts, the containing block may be a flex item that hasn't been
    // stretched yet - its height is 0 at that point and computeLogicalHeight would give
    // a wrong answer. This happens when the nested out-of-flow flex is laid out as part of the
    // anector flex's main-axis sizing, before the stretch phase sets the final cross size.
    CheckedPtr containingBlock = flexBox.containingBlock();
    return flexBox.hasFullyConstrainedLogicalHeight() && containingBlock->hasDefiniteLogicalHeight();
}

bool FlexFormattingUtils::flexItemHasComputableAspectRatio(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    if (!flexItemHasAspectRatio(flexItem))
        return false;
    return flexItem->preferredAspectRatioAsSize().aspectRatioDouble() > 0;
}

bool FlexFormattingUtils::needToStretchFlexItemLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // This function is a little bit magical. It relies on the fact that blocks
    // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
    // an implicit width: 100%. So the child will automatically stretch if our
    // cross axis is the child's inline axis. That's the case if:
    // - We are horizontal and the child is in vertical writing mode
    // - We are vertical and the child is in horizontal writing mode
    // Otherwise, we need to stretch if the cross axis size is auto.
    if (isHorizontalFlow(flexBox()) != flexItem->isHorizontalWritingMode())
        return false;

    // Aspect ratio is properly handled by RenderReplaced during layout.
    if (flexItem->isRenderReplaced() && flexItemHasAspectRatio(flexItem))
        return false;

    if (flexItem->style().logicalHeight().isStretch())
        return true;

    return alignmentForFlexItem(flexItem) == ItemPosition::Stretch && flexItem->style().logicalHeight().isAuto();
}

LayoutUnit FlexFormattingUtils::innerCrossSizeForFlexItem(const RenderBox& flexItem)
{
    CheckedRef flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    if (isColumnFlow(flexBox))
        return flexBox->contentBoxLogicalWidth();

    // Keep this sync'ed with hasDefiniteCrossSizeForFlexItem().
    auto flexContainerInnerCrossSize = [&] {
        auto isHorizontal = isHorizontalFlow(flexBox);
        auto size = isHorizontal ? flexBox->style().height() : flexBox->style().width();
        auto innerCrossSize = LayoutUnit { };
        if (auto fixedSize = size.tryFixed())
            innerCrossSize = flexBox->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedSize->resolveZoom(flexBox->style().usedZoomForLength()) });
        else if (size.isPercent())
            innerCrossSize = flexBox->availableLogicalHeightForPercentageComputation().value_or(0_lu);
        else if (canResolveFullyConstrainedLogicalHeight(flexBox) || hasDefiniteLogicalWidthForAspectRatioCrossSize(flexBox))
            innerCrossSize = std::max(0_lu, flexBox->computeLogicalHeight(flexBox->logicalHeight(), 0_lu).extent - flexBox->borderAndPaddingLogicalHeight() - flexBox->scrollbarLogicalHeight());
        else {
            ASSERT_NOT_REACHED();
            return 0_lu;
        }

        auto maximumSize = isHorizontal ? flexBox->style().maxHeight() : flexBox->style().maxWidth();
        if (auto fixedMaximumSize = maximumSize.tryFixed())
            innerCrossSize = std::min(innerCrossSize, flexBox->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedMaximumSize->resolveZoom(flexBox->style().usedZoomForLength()) }));

        auto minimumSize = isHorizontal ? flexBox->style().minHeight() : flexBox->style().minWidth();
        if (auto fixedMinimumSize = minimumSize.tryFixed())
            innerCrossSize = std::max(innerCrossSize, flexBox->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedMinimumSize->resolveZoom(flexBox->style().usedZoomForLength()) }));

        return innerCrossSize;
    };
    return std::max(0_lu, flexContainerInnerCrossSize() - crossAxisMarginExtentForFlexItem(flexItem));
}

LayoutUnit FlexFormattingUtils::columnInnerMainSize(LayoutUnit hypotheticalMainSize) const
{
    ASSERT(isColumnFlow(flexBox()));
    auto borderPaddingAndScrollbar = flexBox().borderAndPaddingLogicalHeight() + flexBox().scrollbarLogicalHeight();
    auto logicalHeight = flexBox().computeLogicalHeight(hypotheticalMainSize + borderPaddingAndScrollbar, flexBox().logicalTop()).extent;
    return logicalHeight == LayoutUnit::max() ? logicalHeight : std::max(0_lu, logicalHeight - borderPaddingAndScrollbar);
}

FlowDirection FlexFormattingUtils::crossAxisDirection() const
{
    auto crossAxisDirection = flexBox().style().isRowFlexDirection() ? flexBox().writingMode().blockDirection() : flexBox().writingMode().inlineDirection();
    switch (crossAxisDirection) {
    case FlowDirection::TopToBottom:
        return isWrapReverse(flexBox()) ? FlowDirection::BottomToTop : FlowDirection::TopToBottom;
    case FlowDirection::BottomToTop:
        return isWrapReverse(flexBox()) ? FlowDirection::TopToBottom : FlowDirection::BottomToTop;
    case FlowDirection::LeftToRight:
        return isWrapReverse(flexBox()) ? FlowDirection::RightToLeft : FlowDirection::LeftToRight;
    case FlowDirection::RightToLeft:
        return isWrapReverse(flexBox()) ? FlowDirection::LeftToRight : FlowDirection::RightToLeft;
    default:
        ASSERT_NOT_REACHED();
        return FlowDirection::TopToBottom;
    }
}

bool FlexFormattingUtils::isColumnOrRowReverse() const
{
    return flexBox().style().flexDirection() == FlexDirection::ColumnReverse || flexBox().style().flexDirection() == FlexDirection::RowReverse;
}

bool FlexFormattingUtils::isWrapReverse(const RenderFlexibleBox& flexBox)
{
    return flexBox.style().flexWrap() == FlexWrap::Reverse;
}

bool FlexFormattingUtils::hasDefiniteLogicalWidthForAspectRatioCrossSize(const RenderFlexibleBox& flexBox)
{
    // A style-fixed logical width combined with aspect-ratio yields a definite
    // cross size derivable from style alone, with no layout-state dependency.
    // CSS Sizing 4 section 5.1.4: an aspect-ratio transferred size is definite
    // when its input axis is definite, and CSS Sizing 3 section 5.1 makes a
    // style-fixed width definite. The cross axis must be auto so the ratio is
    // actually consulted; min-content/max-content/fit-content ignore the ratio
    // per CSS Sizing 4 section 5.4.
    auto& crossSize = isHorizontalFlow(flexBox) ? flexBox.style().height() : flexBox.style().width();
    return crossSize.isAuto() && flexBox.style().logicalWidth().isFixed() && flexBox.style().aspectRatio().hasRatio();
}

std::optional<TextDirection> FlexFormattingUtils::leftRightAxisDirectionFromStyle(const Style::ComputedStyle& style)
{
    if (!style.isColumnFlexDirection()) // Prioritize text direction.
        return style.writingMode().bidiDirection();

    // Fall back to block direction if possible.
    if (style.writingMode().isVertical()) {
        return style.writingMode().isBlockLeftToRight()
            ? TextDirection::LTR
            : TextDirection::RTL;
    }

    return std::nullopt;
}

bool FlexFormattingUtils::shouldTrimMainAxisMarginStart() const
{
    if (isHorizontalFlow(flexBox()))
        return flexBox().style().marginTrim().contains(Style::MarginTrimSide::InlineStart);
    return flexBox().style().marginTrim().contains(Style::MarginTrimSide::BlockStart);
}

bool FlexFormattingUtils::shouldTrimMainAxisMarginEnd() const
{
    if (isHorizontalFlow(flexBox()))
        return flexBox().style().marginTrim().contains(Style::MarginTrimSide::InlineEnd);
    return flexBox().style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
}

bool FlexFormattingUtils::shouldTrimCrossAxisMarginStart() const
{
    if (isHorizontalFlow(flexBox()))
        return flexBox().style().marginTrim().contains(Style::MarginTrimSide::BlockStart);
    return flexBox().style().marginTrim().contains(Style::MarginTrimSide::InlineStart);
}

bool FlexFormattingUtils::shouldTrimCrossAxisMarginEnd() const
{
    if (isHorizontalFlow(flexBox()))
        return flexBox().style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
    return flexBox().style().marginTrim().contains(Style::MarginTrimSide::InlineEnd);
}

FlowDirection FlexFormattingUtils::transformedBlockFlowDirection() const
{
    if (!isColumnFlow(flexBox()))
        return flexBox().writingMode().blockDirection();
    return flexBox().writingMode().inlineDirection();
}

bool FlexFormattingUtils::isLeftToRightFlow() const
{
    if (isColumnFlow(flexBox()))
        return flexBox().writingMode().blockDirection() == FlowDirection::TopToBottom || flexBox().writingMode().blockDirection() == FlowDirection::LeftToRight;
    return flexBox().writingMode().isLogicalLeftInlineStart() ^ (flexBox().style().flexDirection() == FlexDirection::RowReverse);
}

bool FlexFormattingUtils::isColumnFlow(const RenderFlexibleBox& flexBox)
{
    return flexBox.style().isColumnFlexDirection();
}

LayoutUnit FlexFormattingUtils::availableAlignmentSpaceForFlexItem(LayoutUnit lineCrossAxisExtent, const RenderBox& flexItem, LayoutUnit crossSize) const
{
    LayoutUnit flexItemCrossExtent = crossAxisMarginExtentForFlexItem(flexItem) + crossSize;
    return lineCrossAxisExtent - flexItemCrossExtent;
}

LayoutUnit FlexFormattingUtils::marginBoxAscentForFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit crossSize) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    auto isHorizontalFlow = this->isHorizontalFlow(flexBox());
    auto direction = isHorizontalFlow ? LineDirection::Horizontal : LineDirection::Vertical;
    auto flexboxWritingMode = flexBox().style().writingMode();

    if (!mainAxisIsFlexItemInlineAxis(flexItem)) {
        auto alignmentContextAxis = flexBox().style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        auto writingModeForSynthesis = BaselineAlignment::usedWritingModeForBaselineAlignment(alignmentContextAxis, flexboxWritingMode, flexItem->writingMode());
        return BaselineAlignment::synthesizedBaseline(flexItem, BaselineAlignment::dominantBaseline(flexboxWritingMode),
            writingModeForSynthesis, direction, BaselineSynthesisEdge::BorderBox) + flowAwareMarginBeforeForFlexItem(flexLayoutItem);
    }
    auto ascent = alignmentForFlexItem(flexItem) == ItemPosition::LastBaseline ? flexItem->lastLineBaseline() : flexItem->firstLineBaseline();
    if (!ascent)
        return BaselineAlignment::synthesizedBaseline(flexItem, BaselineAlignment::dominantBaseline(flexboxWritingMode), flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox) + flowAwareMarginBeforeForFlexItem(flexLayoutItem);

    if (!flexItem->writingMode().isBlockMatchingAny(flexBox().writingMode())) {
        // Baseline from flex item with opposite block direction needs to be resolved as if flex item had the same block direction.
        //  _____________________________ <- flex box top/left (e.g. writing-mode: vertical-rl)
        // |        __________________   |
        // |       |  20px |    80px  |<-- flex item with vertical-lr (top is at visual left)
        // |       |<----->|<-------->|  |
        // |       top     baseline   |  |
        // where computed baseline is 20px and resolved (as if flex item shares the block direction with flex box) is 80px.
        ascent = flexItem->logicalHeight() - *ascent;
    }

    if (isHorizontalFlow ? flexItem->isScrollContainerY() : flexItem->isScrollContainerX())
        return std::max(0_lu, std::min(*ascent, crossSize)) + flowAwareMarginBeforeForFlexItem(flexLayoutItem);
    return *ascent + flowAwareMarginBeforeForFlexItem(flexLayoutItem);;
}

bool FlexFormattingUtils::isHorizontalFlow(const RenderFlexibleBox& flexBox)
{
    return flexBox.isHorizontalWritingMode() ? !isColumnFlow(flexBox) : isColumnFlow(flexBox);
}

bool FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem)
{
    auto& flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    return isHorizontalFlow(flexBox) == flexItem.isHorizontalWritingMode();
}

bool FlexFormattingUtils::isMultiline(const RenderFlexibleBox& flexBox)
{
    return flexBox.style().flexWrap() != FlexWrap::NoWrap;
}

Style::FlexBasis FlexFormattingUtils::flexBasisForFlexItem(const RenderBox& flexItem)
{
    auto flexBasis = flexItem.style().flexBasis();
    if (flexBasis.isAuto())
        flexBasis = preferredMainSizeLengthForFlexItem(flexItem).asFlexBasis();
    return flexBasis;
}

ItemPosition FlexFormattingUtils::alignmentForFlexItem(const RenderBox& flexItem)
{
    CheckedRef flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    CheckedRef containerStyle = flexBox->style();
    auto align = flexItem.style().alignSelf().resolve(containerStyle.ptr()).position();
    if (align == ItemPosition::Normal)
        align = ItemPosition::Stretch;

    ASSERT(align != ItemPosition::Auto && align != ItemPosition::Normal);
    // Left and Right are only for justify-*.
    ASSERT(align != ItemPosition::Left && align != ItemPosition::Right);

    // We can safely return here because start/end are not affected by a reversed flex-wrap because the
    // alignment container is the flex line, and in a wrap reversed flex container the start and end within
    // a flex line are still the same. Contrary to this flex-start/flex-end depend on the flex container
    // start/end edges which are flipped in the case of wrap-reverse.
    if (align == ItemPosition::Start)
        return ItemPosition::FlexStart;
    if (align == ItemPosition::End)
        return ItemPosition::FlexEnd;

    if (align == ItemPosition::SelfStart || align == ItemPosition::SelfEnd) {
        bool hasSameDirection = isHorizontalFlow(flexBox)
            ? flexBox->writingMode().isAnyTopToBottom() == flexItem.writingMode().isAnyTopToBottom()
            : flexBox->writingMode().isAnyLeftToRight() == flexItem.writingMode().isAnyLeftToRight();
        return hasSameDirection == (align == ItemPosition::SelfStart)
            ? ItemPosition::FlexStart : ItemPosition::FlexEnd;
    }

    if (isWrapReverse(flexBox)) {
        if (align == ItemPosition::FlexStart)
            align = ItemPosition::FlexEnd;
        else if (align == ItemPosition::FlexEnd)
            align = ItemPosition::FlexStart;
    }

    return align;
}

bool FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem)
{
    CheckedRef flexBox = downcast<RenderFlexibleBox>(*flexItem.parent());
    // 9.8 https://drafts.csswg.org/css-flexbox/#definite-sizes
    // 1. If a single-line flex container has a definite cross size, the automatic preferred outer cross size of any
    // stretched flex items is the flex container's inner cross size (clamped to the flex item's min and max cross size)
    // and is considered definite.
    if (!isMultiline(flexBox) && alignmentForFlexItem(flexItem) == ItemPosition::Stretch && !hasAutoMarginsInCrossAxis(flexItem) && preferredCrossSizeLengthForFlexItem(flexItem).isAuto()) {
        if (isColumnFlow(flexBox))
            return true;
        // This must be kept in sync with computeMainSizeFromAspectRatioUsing().
        auto& crossSize = isHorizontalFlow(flexBox) ? flexBox->style().height() : flexBox->style().width();
        if (crossSize.isFixed())
            return true;
        if (crossSize.isPercent() && flexBox->availableLogicalHeightForPercentageComputation())
            return true;
        if (canResolveFullyConstrainedLogicalHeight(flexBox))
            return true;
        if (hasDefiniteLogicalWidthForAspectRatioCrossSize(flexBox))
            return true;
    }
    return false;
}

const StyleContentAlignmentData& FlexFormattingUtils::contentAlignmentNormalBehavior()
{
    // The justify-content property applies along the main axis, but since
    // flexing in the main axis is controlled by flex, stretch behaves as
    // flex-start (ignoring the specified fallback alignment, if any).
    // https://drafts.csswg.org/css-align/#distribution-flex
    static const StyleContentAlignmentData normalBehavior = { ContentPosition::Normal, ContentDistribution::Stretch };
    return normalBehavior;
}

ContentPosition FlexFormattingUtils::resolveLeftRightAlignment(ContentPosition position, const StyleContentAlignmentData& justifyContent, const Style::ComputedStyle& style, bool isReversed)
{
    if (position == ContentPosition::Left || position == ContentPosition::Right) {
        auto leftRightAxisDirection = FlexFormattingUtils::leftRightAxisDirectionFromStyle(style);
        position = (justifyContent.isEndward(leftRightAxisDirection, isReversed))
            ? ContentPosition::End : ContentPosition::Start;
    }
    return position;
}

LayoutUnit FlexFormattingUtils::initialJustifyContentOffset(const Style::ComputedStyle& style, LayoutUnit availableFreeSpace, unsigned numberOfFlexItems, bool isReversed)
{
    auto resolvedJustifyContent = style.justifyContent().resolve(contentAlignmentNormalBehavior());
    auto justifyContentPosition = resolvedJustifyContent.position();
    auto justifyContentDistribution = resolvedJustifyContent.distribution();

    if (availableFreeSpace < 0 && resolvedJustifyContent.overflow() == OverflowAlignment::Safe) {
        ASSERT(justifyContentPosition != ContentPosition::Normal);
        justifyContentPosition = ContentPosition::Start;
    } else {
        // First of all resolve Left and Right so we could convert it to their equivalent properties handled bellow.
        // If the property's axis is not parallel with either left<->right axis, this value behaves as start. Currently,
        // the only case where the property's axis is not parallel with either left<->right axis is in a column flexbox.
        // https: //www.w3.org/TR/css-align-3/#valdef-justify-content-left
        justifyContentPosition = resolveLeftRightAlignment(justifyContentPosition, resolvedJustifyContent, style, isReversed);
    }

    ASSERT(justifyContentPosition != ContentPosition::Left);
    ASSERT(justifyContentPosition != ContentPosition::Right);

    if (justifyContentPosition == ContentPosition::FlexEnd
        || (justifyContentPosition == ContentPosition::End && !isReversed)
        || (justifyContentPosition == ContentPosition::Start && isReversed))
        return availableFreeSpace;
    if (justifyContentPosition == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (justifyContentDistribution == ContentDistribution::SpaceAround) {
        if (!numberOfFlexItems)
            return availableFreeSpace / 2;
        if (availableFreeSpace > 0)
            return availableFreeSpace / (2 * numberOfFlexItems);
        return { };
    }
    if (justifyContentDistribution == ContentDistribution::SpaceEvenly) {
        if (!numberOfFlexItems)
            return availableFreeSpace / 2;
        if (availableFreeSpace > 0)
            return availableFreeSpace / (numberOfFlexItems + 1);
        return { };
    }
    return { };
}

LayoutUnit FlexFormattingUtils::justifyContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution justifyContentDistribution, unsigned numberOfFlexItems)
{
    if (availableFreeSpace > 0 && numberOfFlexItems > 1) {
        if (justifyContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfFlexItems - 1);
        if (justifyContentDistribution == ContentDistribution::SpaceAround)
            return availableFreeSpace / numberOfFlexItems;
        if (justifyContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfFlexItems + 1);
    }
    return 0;
}

LayoutUnit FlexFormattingUtils::alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition position, std::optional<LayoutUnit> ascent, std::optional<LayoutUnit> maxAscent, bool isWrapReverse)
{
    switch (position) {
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        ASSERT_NOT_REACHED();
        break;
    case ItemPosition::Start:
    case ItemPosition::End:
    case ItemPosition::SelfStart:
    case ItemPosition::SelfEnd:
    case ItemPosition::Left:
    case ItemPosition::Right:
        ASSERT_NOT_REACHED("%u alignmentForFlexItem should have transformed this position value to something we handle below.", static_cast<uint8_t>(position));
        break;
    case ItemPosition::Stretch:
        // Actual stretching must be handled by the caller. Since wrap-reverse
        // flips cross start and cross end, stretch children should be aligned
        // with the cross end. This matters because applyStretchAlignment
        // doesn't always stretch or stretch fully (explicit cross size given, or
        // stretching constrained by max-height/max-width). For flex-start and
        // flex-end this is handled by alignmentForFlexItem().
        if (isWrapReverse)
            return availableFreeSpace;
        break;
    case ItemPosition::FlexStart:
        break;
    case ItemPosition::FlexEnd:
        return availableFreeSpace;
    case ItemPosition::Center:
    case ItemPosition::AnchorCenter:
        return availableFreeSpace / 2;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
        return maxAscent.value_or(0_lu) - ascent.value_or(0_lu);
    }
    return 0;
}

LayoutUnit FlexFormattingUtils::contentAlignmentStartOverflow(LayoutUnit availableFreeSpace, ContentPosition position, ContentDistribution distribution, OverflowAlignment safety, bool isReverse)
{
    if (availableFreeSpace >= 0 || safety == OverflowAlignment::Safe)
        return 0_lu;

    if (distribution == ContentDistribution::SpaceAround
        || distribution == ContentDistribution::SpaceEvenly)
        return -availableFreeSpace / 2;

    switch (position) {
    case ContentPosition::Start:
    case ContentPosition::Baseline:
    case ContentPosition::LastBaseline:
        return 0_lu;
    case ContentPosition::FlexStart:
        return isReverse ? -availableFreeSpace : 0_lu;
    case ContentPosition::Center:
        return -availableFreeSpace / 2;
    case ContentPosition::End:
        return -availableFreeSpace;
    case ContentPosition::FlexEnd:
        return isReverse ? 0_lu : -availableFreeSpace;
    default:
        ASSERT((distribution == ContentDistribution::Default && position == ContentPosition::Normal) // Normal alignment.
            || distribution == ContentDistribution::Stretch
            || distribution == ContentDistribution::SpaceBetween);
        return isReverse ? -availableFreeSpace : 0_lu;
    }
}

LayoutUnit FlexFormattingUtils::initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistribution alignContentDistribution, OverflowAlignment safety, unsigned numberOfLines, bool isReversed)
{
    if (availableFreeSpace < 0 && safety == OverflowAlignment::Safe) {
        ASSERT(alignContent != ContentPosition::Normal);
        alignContent = ContentPosition::Start;
    }

    if (alignContent == ContentPosition::FlexEnd
        || (alignContent == ContentPosition::End && !isReversed)
        || (alignContent == ContentPosition::Start && isReversed))
        return availableFreeSpace;
    if (alignContent == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (alignContentDistribution == ContentDistribution::SpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return std::max(0_lu, availableFreeSpace / 2);
    }
    if (alignContentDistribution == ContentDistribution::SpaceEvenly) {
        if (availableFreeSpace > 0)
            return availableFreeSpace / (numberOfLines + 1);
        // Fallback to 'safe center'
        return std::max(0_lu, availableFreeSpace / 2);
    }
    return 0_lu;
}

LayoutUnit FlexFormattingUtils::alignContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution alignContentDistribution, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContentDistribution == ContentDistribution::SpaceAround || alignContentDistribution == ContentDistribution::Stretch)
            return availableFreeSpace / numberOfLines;
        if (alignContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfLines + 1);
    }
    return 0_lu;
}

// Instance forwarders for the formatting context, which holds a FlexFormattingUtils and so need not re-pass the container.
LayoutUnit FlexFormattingUtils::computeGap(GapType gapType) const
{
    return computeGap(flexBox(), gapType);
}

LayoutUnit FlexFormattingUtils::crossAxisMarginExtentForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return crossAxisMarginExtentForFlexItem(flexLayoutItem.renderer.get());
}

const Style::PreferredSize& FlexFormattingUtils::preferredMainSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return preferredMainSizeLengthForFlexItem(flexLayoutItem.renderer.get());
}

const Style::MinimumSize& FlexFormattingUtils::minMainSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return minMainSizeLengthForFlexItem(flexLayoutItem.renderer.get());
}

const Style::PreferredSize& FlexFormattingUtils::preferredCrossSizeLengthForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return preferredCrossSizeLengthForFlexItem(flexLayoutItem.renderer.get());
}

bool FlexFormattingUtils::hasAutoMarginsInCrossAxis(const FlexLayoutItem& flexLayoutItem) const
{
    return hasAutoMarginsInCrossAxis(flexLayoutItem.renderer.get());
}

bool FlexFormattingUtils::useContentBasedMinimumSize(const FlexLayoutItem& flexLayoutItem) const
{
    return useContentBasedMinimumSize(flexLayoutItem.renderer.get());
}

LayoutUnit FlexFormattingUtils::innerCrossSizeForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return innerCrossSizeForFlexItem(flexLayoutItem.renderer.get());
}

bool FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(const FlexLayoutItem& flexLayoutItem) const
{
    return mainAxisIsFlexItemInlineAxis(flexLayoutItem.renderer.get());
}

Style::FlexBasis FlexFormattingUtils::flexBasisForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return flexBasisForFlexItem(flexLayoutItem.renderer.get());
}

ItemPosition FlexFormattingUtils::alignmentForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return alignmentForFlexItem(flexLayoutItem.renderer.get());
}

bool FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(const FlexLayoutItem& flexLayoutItem) const
{
    return hasDefiniteCrossSizeForFlexItem(flexLayoutItem.renderer.get());
}

} // namespace WebCore

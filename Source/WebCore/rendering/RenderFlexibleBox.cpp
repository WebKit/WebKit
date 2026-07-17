/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderFlexibleBox.h"

#include "BaselineAlignment.h"
#include "FontBaseline.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationFlexLayout.h"
#include "LayoutRepainter.h"
#include "LayoutUnit.h"
#include "LineClampUpdater.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexLayout.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectEnums.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderSVGRoot.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderFlexibleBox);

static bool canSetFlexItemContentLogicalHeight(const RenderBox& flexItem)
{
    return !flexItem.isFloatingOrOutOfFlowPositioned() && !flexItem.shouldComputeLogicalHeightFromAspectRatio() && !is<RenderReplaced>(flexItem);
}

static void updateFlexItemDirtyBitsBeforeLayout(bool relayoutFlexItem, RenderBox& flexItem)
{
    if (flexItem.isOutOfFlowPositioned())
        return;

    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutFlexItem || flexItem.hasRelativeLogicalHeight())
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
}

#define SET_OR_CLEAR_OVERRIDING_SIZE(box, SizeType, size)       \
    {                                                           \
        if (size)                                               \
            box.setOverridingBorderBoxLogical##SizeType(*size); \
        else                                                    \
            box.clearOverridingBorderBoxLogical##SizeType();    \
    }

// RAII class which defines a scope in which overriding sizes of a box are either:
//   1) replaced by other size in one axis if size is specified
//   2) cleared in both axis if size == std::nullopt
//
// In any case the previous overriding sizes are restored on destruction (in case of
// not having a previous value it's simply cleared).
RenderFlexibleBox::OverridingSizesScope::OverridingSizesScope(RenderBox& box, Axis axis, std::optional<LayoutUnit> size)
    : m_box(box)
    , m_axis(axis)
{
    ASSERT(!size || (axis != Axis::Both));
    if (axis == Axis::Both || axis == Axis::Inline) {
        m_previousOverridingBorderBoxLogicalWidth = box.overridingBorderBoxLogicalWidth();
        SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Width, size);
    }
    if (axis == Axis::Both || axis == Axis::Block) {
        m_previousOverridingBorderBoxLogicalHeight = box.overridingBorderBoxLogicalHeight();
        SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Height, size);
    }
}

RenderFlexibleBox::OverridingSizesScope::~OverridingSizesScope()
{
    if (m_axis == Axis::Inline || m_axis == Axis::Both)
        SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Width, m_previousOverridingBorderBoxLogicalWidth);

    if (m_axis == Axis::Block || m_axis == Axis::Both)
        SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Height, m_previousOverridingBorderBoxLogicalHeight);
}

// Sets m_inFlexItemIntrinsicWidthComputation and applies the container's definite
// cross size as the flex item's cross-axis override when applicable. Clears all
// overrides otherwise.
//
// When invalidateContentLogicalWidths is true, the flex item's preferred widths are
// invalidated so that min/maxContentLogicalWidthContribution() will recompute them with the
// cross-axis override in place. The destructor ASSERTs the dirty flag was consumed.
RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::ScopedCrossAxisOverrideForFlexItem(const RenderFlexibleBox& flexBox, RenderBox& flexItem, InvalidateContentWidths invalidateContentWidths)
    : m_intrinsicWidthComputation(flexBox.m_inFlexItemIntrinsicWidthComputation, true)
#if ASSERT_ENABLED
    , m_flexItem(flexItem)
#endif
{
    if (flexBox.flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem)) {
        auto axis = flexBox.flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem) ? OverridingSizesScope::Axis::Block : OverridingSizesScope::Axis::Inline;
        m_overridingScope.emplace(flexItem, axis, flexBox.flexLayoutUtils().innerCrossSizeForFlexItem(flexItem));
        if (invalidateContentWidths == InvalidateContentWidths::Yes) {
            flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
#if ASSERT_ENABLED
            m_didInvalidateContentLogicalWidths = true;
#endif
        }
    } else
        m_overridingScope.emplace(flexItem, OverridingSizesScope::Axis::Both);
}

RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::~ScopedCrossAxisOverrideForFlexItem()
{
#if ASSERT_ENABLED
    if (m_didInvalidateContentLogicalWidths)
        ASSERT(!m_flexItem.hasInvalidContentLogicalWidths());
#endif
}

RenderFlexibleBox::RenderFlexibleBox(Type type, Element& element, Style::ComputedStyle&& style)
    : RenderBlock(type, element, WTF::move(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::RenderFlexibleBox(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderBlock(type, document, WTF::move(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::~RenderFlexibleBox() = default;

ASCIILiteral RenderFlexibleBox::renderName() const
{
    return "RenderFlexibleBox"_s;
}

void RenderFlexibleBox::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (relayoutChildren == RelayoutChildren::No) {
        auto simplifiedLayoutScope = SetForScope(m_inSimplifiedLayout, true);
        if (simplifiedLayout())
            return;
    }

    LayoutRepainter repainter(*this);

    resetLogicalHeightBeforeLayoutIfNeeded();
    m_flexItemsWithCompletedLayout.clear();

    bool oldInLayout = m_inLayout;
    m_inLayout = true;

    if (!style().marginTrim().isNone())
        initializeMarginTrimState();

    clearFlexItemOverridingSizes();

    if (recomputeLogicalWidth())
        relayoutChildren = RelayoutChildren::Yes;

    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
    {
        auto lineClampUpdater = LineClampUpdater { *this };
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());

        preparePaginationBeforeBlockLayout(relayoutChildren);

        m_numberOfFlexItemsOnFirstLine = { };
        m_numberOfFlexItemsOnLastLine = { };
        m_justifyContentStartOverflow = 0;

        beginUpdateScrollInfoAfterLayoutTransaction();

        prepareOrderIteratorAndMargins();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        FlexItemBorderBoxRects oldFlexItemRects;
        appendFlexItemBorderBoxRects(oldFlexItemRects);

        if (!layoutUsingFlexFormattingContext()) {
            auto flexItems = collectFlexItems(relayoutChildren);
            if (flexItems.isEmpty()) {
                adjustLogicalHeightForLineIfEmpty();
                updateLogicalHeight();
            } else {
                auto flexLayoutResult = FlexLayout(*this, flexLayoutConstraints()).performFlexLayout(flexItems, relayoutChildren);
                if (flexLayoutResult.alignContentStartOverflow)
                    m_alignContentStartOverflow = *flexLayoutResult.alignContentStartOverflow;
                m_justifyContentStartOverflow = flexLayoutResult.justifyContentStartOverflow;
                m_numberOfFlexItemsOnFirstLine = flexLayoutResult.numberOfFlexItemsOnFirstLine;
                m_numberOfFlexItemsOnLastLine = flexLayoutResult.numberOfFlexItemsOnLastLine;
            }
        }

        {
            auto scrollbarLayout = SetForScope(m_inPostFlexUpdateScrollbarLayout, true);
            endAndCommitUpdateScrollInfoAfterLayoutTransaction();
        }

        repaintFlexItemsDuringLayoutIfMoved(oldFlexItemRects);
        // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
        updateInFlowDescendantTransformsAfterLayout();
        computeInFlowOverflow(flippedContentBoxRect(),  { ComputeOverflowOptions::MarginsExtendContentAreaX, ComputeOverflowOptions::MarginsExtendContentAreaY });
        // FIXME: Only the items at the edges should contribute to the content area. But this distinction only matters in some weird cases with extreme negative margins.

        if (isDocumentElementRenderer() || logicalHeight() != previousHeight)
            layoutOutOfFlowBoxes(RelayoutChildren::Yes);
        else
            layoutOutOfFlowBoxes(relayoutChildren);
        updateOutOfFlowDescendantTransformsAfterLayout();
        addOverflowFromOutOfFlowBoxes();
    }

    updateLayerTransform();

    // We have to reset this, because changes to our ancestors' style can affect
    // this value. Also, this needs to be before we call updateAfterLayout, as
    // that function may re-enter this one.
    resetHasDefiniteHeight();

    repainter.repaintAfterLayout();

    m_inLayout = oldInLayout;
}

std::optional<LayoutUnit> RenderFlexibleBox::firstLineBaseline() const
{
    if ((isWritingModeRoot() && !isFlexItem()) || !m_numberOfFlexItemsOnFirstLine || shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForFirstBaseline();
    if (!baselineFlexItem)
        return { };

    auto baseline = std::optional<LayoutUnit> { };
    if (!flexLayoutUtils().isColumnFlow() && !flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = flexLayoutUtils().crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (flexLayoutUtils().isColumnFlow() && flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = flexLayoutUtils().mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto firstLineBaseline = baselineFlexItem->firstLineBaseline())
        baseline = firstLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignment::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignment::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = baselineFlexItem->logicalTop() + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (flexLayoutUtils().isHorizontalFlow() ? isScrollContainerY() : isScrollContainerX())
        return std::max(0_lu, std::min(result, logicalHeight()));
    return result;
}

std::optional <LayoutUnit> RenderFlexibleBox::lastLineBaseline() const
{
    if (isWritingModeRoot() || !m_numberOfFlexItemsOnLastLine || shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForLastBaseline();
    if (!baselineFlexItem)
        return { };

    auto baseline = std::optional<LayoutUnit> { };
    if (!flexLayoutUtils().isColumnFlow() && !flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = flexLayoutUtils().crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (flexLayoutUtils().isColumnFlow() && flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = flexLayoutUtils().mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto lastLineBaseline = baselineFlexItem->lastLineBaseline())
        baseline = lastLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignment::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignment::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = baselineFlexItem->logicalTop() + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (flexLayoutUtils().isHorizontalFlow() ? isScrollContainerY() : isScrollContainerX())
        return std::max(0_lu, std::min(result, logicalHeight()));
    return result;
}

void RenderFlexibleBox::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != Style::DifferenceResult::Layout)
        return;

    auto oldAlignItems = oldStyle->alignItems().resolve().position();
    auto newAlignItems = style().alignItems().resolve().position();
    auto alignItemsStretchChanged = (oldAlignItems == ItemPosition::Normal || oldAlignItems == ItemPosition::Stretch) != (newAlignItems == ItemPosition::Normal || newAlignItems == ItemPosition::Stretch);
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        // Flex items that were previously stretching need to be relayed out so we
        // can compute new available cross axis space. This is only necessary for
        // stretching since other alignment values don't change the size of the
        // box.
        if (alignItemsStretchChanged && flexItem.style().alignSelf().isAuto())
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

bool RenderFlexibleBox::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestAction::Foreground)
        return false;

    LayoutPoint scrolledOffset = hasNonVisibleOverflow() ? adjustedLocation - toLayoutSize(scrollPosition()) : adjustedLocation;

    // If collecting the children in reverse order is bad for performance, this Vector could be determined at layout time.
    Vector<RenderBox*> reversedOrderIteratorForHitTesting;
    for (auto* flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (flexItem->isOutOfFlowPositioned())
            continue;
        reversedOrderIteratorForHitTesting.append(flexItem);
    }
    reversedOrderIteratorForHitTesting.reverse();

    for (auto* flexItem : reversedOrderIteratorForHitTesting) {
        if (flexItem->hasSelfPaintingLayer())
            continue;
        auto location = flipForWritingModeForChild(*flexItem, scrolledOffset);
        if (flexItem->hitTest(request, result, locationInContainer, location)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - adjustedLocation)));
            return true;
        }
    }

    return false;
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForFlexItem, bool usePrintRect)
{
    for (RenderBox* flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (!paintChild(*flexItem, paintInfo, paintOffset, paintInfoForFlexItem, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

bool RenderFlexibleBox::willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode mode) const
{
    auto physicalAxis = mapAxisLogicalToPhysical(writingMode(), containingAxis);
    if (flexLayoutUtils().isHorizontalFlow() == (BoxAxis::Horizontal == physicalAxis))
        return false;

    auto& itemStyle = item.style();
    bool isVerticalCrossAxis = physicalAxis == BoxAxis::Vertical;
    auto& crossSize = isVerticalCrossAxis ? itemStyle.height() : itemStyle.width();

    if (!crossSize.isStretch()) {
        if (!itemStyle.alignSelf().resolve(&style()).isStretchy(mode == StretchingMode::Explicit ? ItemPosition::Normal : ItemPosition::Stretch))
            return false;
        if (!crossSize.isAuto())
            return false;
    }

    return isVerticalCrossAxis
        ? !itemStyle.marginTop().isAuto() && !itemStyle.marginBottom().isAuto()
        : !itemStyle.marginLeft().isAuto() && !itemStyle.marginRight().isAuto();
}

LayoutOptionalOutsets RenderFlexibleBox::allowedLayoutOverflow() const
{
    LayoutOptionalOutsets allowance = RenderBox::allowedLayoutOverflow();

    bool isColumnar = style().isColumnFlexDirection();
    if (isHorizontalWritingMode()) {
        allowance.top() = isColumnar ? m_justifyContentStartOverflow : m_alignContentStartOverflow;
        if (writingMode().isInlineLeftToRight())
            allowance.left() = isColumnar ? m_alignContentStartOverflow : m_justifyContentStartOverflow;
        else
            allowance.right() = isColumnar ? m_alignContentStartOverflow : m_justifyContentStartOverflow;
    } else {
        allowance.left() = isColumnar ? m_justifyContentStartOverflow : m_alignContentStartOverflow;
        if (writingMode().isInlineTopToBottom())
            allowance.top() = isColumnar ? m_alignContentStartOverflow : m_justifyContentStartOverflow;
        else
            allowance.bottom() = isColumnar ? m_alignContentStartOverflow : m_justifyContentStartOverflow;
    }

    return allowance;
}

std::pair<LayoutUnit, LayoutUnit> RenderFlexibleBox::computeIntrinsicLogicalWidths() const
{
    auto scrollbarWidth = scrollbarLogicalWidth();

    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth())
            return { width.value() + scrollbarWidth, width.value() + scrollbarWidth };
        return { scrollbarWidth, scrollbarWidth };
    }

    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
    // honoring it though until the flex shorthand stops setting it to 0. See
    // https://bugs.webkit.org/show_bug.cgi?id=116117 and
    // https://crbug.com/240765.
    auto [legendMinWidth, legendMaxWidth] = computeIntrinsicLogicalWidthsForFieldsetLegend();

    auto minLogicalWidth = LayoutUnit { };
    auto maxLogicalWidth = LayoutUnit { };

    size_t numItemsWithNormalLayout = 0;
    for (RenderBox* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (flexItem->isOutOfFlowPositioned() || flexItem->isExcludedFromNormalLayout())
            continue;
        ++numItemsWithNormalLayout;

        // Pre-layout orthogonal children in order to get a valid value for the preferred width.
        if (writingMode().isOrthogonal(flexItem->writingMode()))
            flexItem->layoutIfNeeded();

        LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*flexItem);

        auto [minContentLogicalWidth, maxContentLogicalWidth] = computeChildIntrinsicLogicalWidths(*flexItem);

        minContentLogicalWidth += margin;
        maxContentLogicalWidth += margin;

        if (!flexLayoutUtils().isColumnFlow()) {
            maxLogicalWidth += maxContentLogicalWidth;
            if (flexLayoutUtils().isMultiline()) {
                // For multiline, the min preferred width is if you put a break between
                // each item.
                minLogicalWidth = std::max(minLogicalWidth, minContentLogicalWidth);
            } else
                minLogicalWidth += minContentLogicalWidth;
        } else {
            minLogicalWidth = std::max(minContentLogicalWidth, minLogicalWidth);
            maxLogicalWidth = std::max(maxContentLogicalWidth, maxLogicalWidth);
        }
    }

    if (!flexLayoutUtils().isColumnFlow() && numItemsWithNormalLayout > 1) {
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * flexLayoutUtils().computeGap(FlexLayoutUtils::GapType::BetweenItems);
        maxLogicalWidth += inlineGapSize;
        if (!flexLayoutUtils().isMultiline())
            minLogicalWidth += inlineGapSize;
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    // Due to negative margins, it is possible that we calculated a negative
    // intrinsic width. Make sure that we never return a negative width.
    minLogicalWidth = std::max(0_lu, minLogicalWidth);
    maxLogicalWidth = std::max(0_lu, maxLogicalWidth);

    minLogicalWidth = std::max(minLogicalWidth, legendMinWidth);
    maxLogicalWidth = std::max(maxLogicalWidth, legendMaxWidth);

    return { minLogicalWidth + scrollbarWidth, maxLogicalWidth + scrollbarWidth };
}

// This method is only called whenever a descendant of a flex item wants to resolve a percentage in its
// block axis (logical height). The key here is that percentages should be generally resolved before the
// flex item is flexed, meaning that they shouldn't be recomputed once the flex item has been flexed. There
// are some exceptions though that are implemented here, like the case of fully inflexible items with
// definite flex-basis, or whenever the flex container has a definite main size. See
// https://drafts.csswg.org/css-flexbox/#definite-sizes for additional details.
std::optional<LayoutUnit> RenderFlexibleBox::usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox& flexItem)
{
    return canUseFlexItemForPercentageResolution(flexItem) ? flexItem.overridingBorderBoxLogicalHeight() : std::nullopt;
}

bool RenderFlexibleBox::canUseFlexItemForPercentageResolution(const RenderBox& flexItem)
{
    ASSERT(flexItem.isFlexItem());

    auto canUseByLayoutPhase = [&] {
        if (m_inFlexItemIntrinsicWidthComputation)
            return flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem);

        if (m_afterMainAxisItemSizing) {
            // Final sizes for flex items are available only along the main axis.
            // Percentages can be resolved only against those items when they are orthogonal to the flex container (i.e., their logical height is computed and final)
            return !flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem);
        }

        if (m_afterCrossAxisItemSizing) {
            // Final sizes for flex items are known in both the main and cross directions, so it's fine to resolve percentage heights using those final values.
            return true;
        }

        if (m_inPostFlexUpdateScrollbarLayout) {
            // We run layout on flex content _after_ performing flex layout (see endAndCommitUpdateScrollInfoAfterLayoutTransaction/updateScrollInfoAfterLayout).
            // Final sizes for flex items are known in both the main and cross directions.
            return true;
        }

        if (m_inSimplifiedLayout) {
            // While in simplified layout, we should only re-compute overflow and/or re-position out-of-flow boxes, some renderers (e.g. RenderReplaced and subclasses)
            // currently ignore this optimization and run regular layout.
            // Final sizes for flex items are known in both the main and cross directions, computed during previous layout(s).
            return true;
        }

        if (&flexItem == view().frameView().layoutContext().subtreeLayoutRoot())
            return !flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem);

        // Outside of layout (i.e. when using relative percentage positioning), base the decision on style.
        return !m_inLayout;
    };
    if (!canUseByLayoutPhase())
        return false;

    auto canUseByStyle = [&] {
        if (flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem))
            return flexLayoutUtils().alignmentForFlexItem(flexItem) == ItemPosition::Stretch;

        // Flexbox 9.8 rule 2: definite flex-basis makes post-flexing main size definite.
        if (flexItemMainSizeIsDefinite(flexItem, flexLayoutUtils().flexBasisForFlexItem(flexItem)))
            return true;

        // Flexbox 9.8 rule 1: definite container main size makes post-flexing sizes definite.
        return canComputePercentageFlexBasis(flexItem, Style::PreferredSize { 0_css_percentage }, UpdatePercentageHeightDescendants::Yes);
    };
    return canUseByStyle();
}

void RenderFlexibleBox::invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem)
{
    m_blockAxisSize.remove(flexItem);
}

void RenderFlexibleBox::flexItemWillBeRemoved(const RenderBox& flexItem)
{
    m_contentLogicalHeights.remove(flexItem);
    m_blockAxisSize.remove(flexItem);
}

// https://drafts.csswg.org/css-flexbox/#min-size-auto

LayoutUnit RenderFlexibleBox::flexItemContentLogicalHeight(const RenderBox& flexItem) const
{
    if (CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(flexItem))
        return renderReplaced->intrinsicLogicalHeight();

    if (auto logicalHeight = m_contentLogicalHeights.getOptional(flexItem))
        return *logicalHeight;

    return flexItem.contentBoxLogicalHeight();
}

void RenderFlexibleBox::setFlexItemContentLogicalHeightIfNeeded(const RenderBox& flexItem, LayoutUnit height)
{
    // Captures a flex item's content logical height mid-layout, before computeLogicalHeight
    // applies fixed/min/max or any overridingBorderBoxLogicalHeight set by the flex
    // container for stretch alignment.
    // Reading logicalHeight() at the end of the flex item's layout would give the constrained/overridden value,
    // not the content height the flex algorithm needs.
    if (!canSetFlexItemContentLogicalHeight(flexItem))
        return;
    if (flexItem.overridingBorderBoxLogicalHeight())
        return;
    m_contentLogicalHeights.set(flexItem, height);
}

void RenderFlexibleBox::cacheFlexItemContentLogicalHeightIfAllowed(const RenderBox& flexItem, LayoutUnit height)
{
    if (canSetFlexItemContentLogicalHeight(flexItem))
        m_contentLogicalHeights.set(flexItem, height);
}

LayoutUnit RenderFlexibleBox::computeBlockAxisContentSizeForFlexItem(RenderBox& flexItem)
{
    // Reuse the size cached in a previous layout while the item stays clean.
    if (!flexItem.needsLayout()) {
        if (auto cachedBlockAxisContentSize = blockAxisSizeForFlexItem(flexItem))
            return *cachedBlockAxisContentSize;
    }

    // Don't resolve percentages in children. This is especially important for the min-height calculation,
    // where we want percentages to be treated as auto. For flex-basis itself, this is not a problem because
    // by definition we have an indefinite flex basis here and thus percentages should not resolve.
    auto percentResolveDisableScope = FlexPercentResolveDisabler { view().frameView().layoutContext(), flexItem };
    flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    flexItem.layoutIfNeeded();

    auto blockAxisContentSize = [&] {
        auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
        if (flexBasis.isPercentOrCalculated() && !flexItemMainSizeIsDefinite(flexItem, flexBasis))
            return flexItemContentLogicalHeight(flexItem) + flexItem.scrollbarLogicalHeight();
        return flexItem.logicalHeight() - flexItem.borderAndPaddingLogicalHeight();
    }();

    // Cache it so a later layout can skip re-laying-out this item while it stays clean, and record that we laid it out this iteration.
    setBlockAxisSizeForFlexItem(flexItem, blockAxisContentSize);
    markFlexItemLayoutComplete(flexItem);
    return blockAxisContentSize;
}

void RenderFlexibleBox::stretchFlexItemLogicalHeight(RenderBox& flexItem, LayoutUnit desiredLogicalHeight, bool needsRelayout)
{
    if (needsRelayout || !flexItem.overridingBorderBoxLogicalHeight())
        flexItem.setOverridingBorderBoxLogicalHeight(desiredLogicalHeight);
    if (!needsRelayout)
        return;

    auto resetFlexItemLogicalHeight = scopedResetFlexItemLogicalHeightBeforeLayout();
    // We cache the child's content logical height to avoid it being reset to the stretched height.
    // FIXME: This is fragile. RenderBoxes should be smart enough to determine their content logical height
    // correctly even when there's an overrideHeight.
    LayoutUnit contentLogicalHeight = flexItemContentLogicalHeight(flexItem);
    flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    dirtyPercentHeightDescendantsWithinFlexItem(flexItem);

    // Don't use layoutChildIfNeeded to avoid setting cross axis cached size twice.
    flexItem.layoutIfNeeded();

    cacheFlexItemContentLogicalHeightIfAllowed(flexItem, contentLogicalHeight);
}

void RenderFlexibleBox::relayoutFlexItemForStretchedCrossSize(RenderBox& flexItem, LayoutUnit crossSize, LogicalBoxAxis crossAxis)
{
    if (crossAxis == LogicalBoxAxis::Block)
        flexItem.setOverridingBorderBoxLogicalHeight(crossSize);
    else
        flexItem.setOverridingBorderBoxLogicalWidth(crossSize);
    flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    flexItem.layoutIfNeeded();
}

void RenderFlexibleBox::dirtyPercentHeightDescendantsWithinFlexItem(RenderBox& flexItem)
{
    // In quirks mode, the percentage height walk may register descendants on the
    // flex container instead of the flex item. This method uses
    // dirtyForLayoutFromPercentageHeightDescendant to propagate layout through
    // intermediate auto-height ancestors down to those descendants.
    if (!hasPercentHeightDescendants())
        return;
    CheckedPtr flexItemBlockFlow = dynamicDowncast<RenderBlockFlow>(flexItem);
    if (!flexItemBlockFlow)
        return;
    for (auto& descendant : *percentHeightDescendants()) {
        if (descendant.parent() == this)
            continue;
        if (flexItemBlockFlow->isContainingBlockAncestorFor(descendant))
            flexItemBlockFlow->dirtyForLayoutFromPercentageHeightDescendant(descendant);
    }
}

void RenderFlexibleBox::layoutFlexItemAfterMainSizing(FlexLayoutItem& flexLayoutItem, LayoutUnit mainSize, RelayoutChildren relayoutChildren)
{
    auto& flexItem = flexLayoutItem.renderer.get();

    setOverridingMainSizeForFlexItem(flexItem, mainSize);
    // The flexed content size and the override size include the scrollbar
    // width, so we need to compare to the size including the scrollbar.
    // FIXME: Should it include the scrollbar?
    if (mainSize != flexLayoutUtils().mainAxisContentExtentForFlexItemIncludingScrollbar(flexItem))
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    else {
        // To avoid double applying margin changes in
        // updateAutoMarginsInCrossAxis, we reset the margins here.
        resetAutoMarginsAndLogicalTopInCrossAxis(flexItem);
    }
    // We may have already forced relayout for orthogonal flowing children in
    // computeInnerFlexBaseSizeForFlexItem.
    bool forceFlexItemRelayout = relayoutChildren == RelayoutChildren::Yes && !hasFlexItemCompletedLayout(flexItem);
    if (!forceFlexItemRelayout && flexItemHasPercentHeightDescendants(flexItem)) {
        // Have to force another relayout even though the child is sized
        // correctly, because its descendants are not sized correctly yet. Our
        // previous layout of the child was done without an override height set.
        // So, redo it here.
        forceFlexItemRelayout = true;
    }
    updateFlexItemDirtyBitsBeforeLayout(forceFlexItemRelayout, flexItem);
    if (!flexItem.needsLayout())
        flexItem.markForPaginationRelayoutIfNeeded();
    if (flexItem.needsLayout())
        markFlexItemLayoutComplete(flexItem);

    {
        auto flexLayoutScope = scopedAfterMainAxisItemSizing();
        flexItem.layoutIfNeeded();
    }

    if (!flexLayoutItem.everHadLayout && flexItem.checkForRepaintDuringLayout()) {
        flexItem.repaint();
        flexItem.repaintOverhangingFloats(true);
    }
}

void RenderFlexibleBox::setOverridingMainSizeForFlexItem(RenderBox& flexItem, LayoutUnit preferredSize)
{
    if (flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem))
        flexItem.setOverridingBorderBoxLogicalWidth(preferredSize + flexItem.borderAndPaddingLogicalWidth());
    else
        flexItem.setOverridingBorderBoxLogicalHeight(preferredSize + flexItem.borderAndPaddingLogicalHeight());
}

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem)
{
    if (flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem)) {
        flexItem.updateLogicalHeight();
        if (flexLayoutUtils().isHorizontalFlow()) {
            if (flexItem.style().marginTop().isAuto())
                flexItem.setMarginTop(0_lu);
            if (flexItem.style().marginBottom().isAuto())
                flexItem.setMarginBottom(0_lu);
        } else {
            if (flexItem.style().marginLeft().isAuto())
                flexItem.setMarginLeft(0_lu);
            if (flexItem.style().marginRight().isAuto())
                flexItem.setMarginRight(0_lu);
        }
    }
}

bool RenderFlexibleBox::flexItemHasPercentHeightDescendants(const RenderBox& renderer) const
{
    // FIXME: This function can be removed soon after webkit.org/b/204318 is fixed. Evaluate whether the
    // skipContainingBlockForPercentHeightCalculation() check below should be moved to the caller in that case.
    CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(renderer);
    if (!renderBlock)
        return false;

    // FlexibleBoxImpl's like RenderButton might wrap their children in anonymous blocks. Those anonymous blocks are
    // skipped for percentage height calculations in RenderBox::computePercentageLogicalHeight() and thus
    // addPercentHeightDescendant() is never called for them. This means that this method would always wrongly
    // return false for a child of a <button> with a percentage height.
    if (hasPercentHeightDescendants()) {
        for (auto& descendant : *percentHeightDescendants()) {
            if (renderBlock->isContainingBlockAncestorFor(descendant))
                return true;
        }
    }

    if (!renderBlock->hasPercentHeightDescendants())
        return false;

    auto* percentHeightDescendants = renderBlock->percentHeightDescendants();
    if (!percentHeightDescendants)
        return false;

    for (auto& descendant : *percentHeightDescendants) {
        bool hasOutOfFlowAncestor = false;
        for (auto* ancestor = descendant.containingBlock(); ancestor && ancestor != renderBlock.get(); ancestor = ancestor->containingBlock()) {
            if (ancestor->isOutOfFlowPositioned()) {
                hasOutOfFlowAncestor = true;
                break;
            }
        }
        if (!hasOutOfFlowAncestor)
            return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::staticMainAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    auto flexItemMainExtent = flexLayoutUtils().mainAxisMarginExtentForFlexItem(flexItem) + flexLayoutUtils().mainAxisExtentForFlexItem(flexItem);
    auto mainAxisContentSize = flexLayoutUtils().isColumnFlow() ? contentBoxLogicalHeight() : contentBoxLogicalWidth();
    auto availableSpace = mainAxisContentSize - flexItemMainExtent;
    auto isReverse = flexLayoutUtils().isColumnOrRowReverse();
    LayoutUnit offset = FlexLayoutUtils::initialJustifyContentOffset(style(), availableSpace, { }, isReverse);
    if (isReverse)
        offset = availableSpace - offset;
    return offset;
}

LayoutUnit RenderFlexibleBox::staticCrossAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    auto availableSpace = flexLayoutUtils().availableAlignmentSpaceForFlexItem(flexLayoutUtils().crossAxisContentExtent(), flexItem, flexLayoutUtils().crossAxisExtentForFlexItem(flexItem));
    auto safety = flexLayoutUtils().overflowAlignmentForFlexItem(flexItem);
    auto align = flexLayoutUtils().alignmentForFlexItem(flexItem);
    if (availableSpace < 0 && safety == OverflowAlignment::Safe)
        align = ItemPosition::FlexStart;
    return FlexLayoutUtils::alignmentOffset(availableSpace, align, { }, { }, flexLayoutUtils().isWrapReverse());
}

LayoutUnit RenderFlexibleBox::staticInlinePositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return startOffsetForContent() + (flexLayoutUtils().isColumnFlow() ? staticCrossAxisPositionForPositionedFlexItem(flexItem) : staticMainAxisPositionForPositionedFlexItem(flexItem));
}

LayoutUnit RenderFlexibleBox::staticBlockPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return borderAndPaddingBefore() + (flexLayoutUtils().isColumnFlow() ? staticMainAxisPositionForPositionedFlexItem(flexItem) : staticCrossAxisPositionForPositionedFlexItem(flexItem));
}

bool RenderFlexibleBox::setStaticPositionForPositionedLayout(const RenderBox& flexItem)
{
    bool positionChanged = false;
    CheckedPtr layer = flexItem.layer();
    if (flexItem.style().hasStaticInlinePosition(writingMode().isHorizontal())) {
        LayoutUnit inlinePosition = staticInlinePositionForPositionedFlexItem(flexItem);
        if (layer->staticInlinePosition() != inlinePosition) {
            layer->setStaticInlinePosition(inlinePosition);
            positionChanged = true;
        }
    }
    if (flexItem.style().hasStaticBlockPosition(writingMode().isHorizontal())) {
        LayoutUnit blockPosition = staticBlockPositionForPositionedFlexItem(flexItem);
        if (layer->staticBlockPosition() != blockPosition) {
            layer->setStaticBlockPosition(blockPosition);
            positionChanged = true;
        }
    }
    return positionChanged;
}

void RenderFlexibleBox::appendFlexItemBorderBoxRects(FlexItemBorderBoxRects& flexItemBorderBoxRects)
{
    for (RenderBox* flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (!flexItem->isOutOfFlowPositioned())
            flexItemBorderBoxRects.append(flexItem->borderBoxRectInContainer());
    }
}

void RenderFlexibleBox::repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects& oldFlexItemRects)
{
    size_t index = 0;
    for (RenderBox* flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (flexItem->isOutOfFlowPositioned())
            continue;

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && flexItem->checkForRepaintDuringLayout())
            flexItem->repaintDuringLayoutIfMoved(oldFlexItemRects[index]);
        ++index;
    }
    ASSERT(index == oldFlexItemRects.size());
}

// FIXME: consider adding this check to RenderBox::hasIntrinsicAspectRatio(). We could even make it
// virtual returning false by default. RenderReplaced will overwrite it with the current implementation
// plus this extra check. See wkb.ug/231955.

template<typename SizeType> bool RenderFlexibleBox::canComputePercentageFlexBasis(const RenderBox& flexItem, const SizeType& flexBasis, UpdatePercentageHeightDescendants updateDescendants)
{
    if (!flexLayoutUtils().isColumnFlow() || m_hasDefiniteHeight == SizeDefiniteness::Definite)
        return true;
    if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
        return false;

    auto isPercentResolveSuspended = view().frameView().layoutContext().isPercentHeightResolveDisabledFor(flexItem);
    ASSERT(!isPercentResolveSuspended || is<RenderBlock>(flexItem));

    bool definite = !isPercentResolveSuspended && flexItem.computePercentageLogicalHeight(flexBasis, updateDescendants).has_value();
    if (m_inLayout && (isHorizontalWritingMode() == flexItem.isHorizontalWritingMode())) {
        // We can reach this code even while we're not laying ourselves out, such
        // as from mainSizeForPercentageResolution.
        m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
    }
    return definite;
}

template<typename SizeType> bool RenderFlexibleBox::flexItemMainSizeIsDefinite(const RenderBox& flexItem, const SizeType& size)
{
    if constexpr (!std::same_as<SizeType, Style::MaximumSize>) {
        if (size.isAuto())
            return false;
    }
    if constexpr (std::same_as<SizeType, Style::FlexBasis>) {
        if (size.isContent())
            return false;
    }
    if (!flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem) && (size.isIntrinsic() || size.isIntrinsicKeyword()))
        return false;
    // Stretch is definite in the same cases as percentages, i.e., when the
    // container's cross size is definite. We use a dummy percentage since
    // canComputePercentageFlexBasis evaluates the value as a percentage.
    if (size.isStretch())
        return canComputePercentageFlexBasis(flexItem, Style::PreferredSize { 0_css_percentage }, UpdatePercentageHeightDescendants::No);
    if (size.isPercentOrCalculated())
        return canComputePercentageFlexBasis(flexItem, size, UpdatePercentageHeightDescendants::No);
    return true;
}

// Explicit instantiations for the SizeTypes FlexLayout resolves through RenderFlexibleBox from a separate translation unit.
template bool RenderFlexibleBox::flexItemMainSizeIsDefinite<Style::FlexBasis>(const RenderBox&, const Style::FlexBasis&);
template bool RenderFlexibleBox::flexItemMainSizeIsDefinite<Style::MinimumSize>(const RenderBox&, const Style::MinimumSize&);
template bool RenderFlexibleBox::flexItemMainSizeIsDefinite<Style::MaximumSize>(const RenderBox&, const Style::MaximumSize&);
template bool RenderFlexibleBox::flexItemMainSizeIsDefinite<Style::PreferredSize>(const RenderBox&, const Style::PreferredSize&);

void RenderFlexibleBox::initializeMarginTrimState()
{
    // When computeIntrinsicLogicalWidth goes through each of the children, it
    // will include the margins when computing the flexbox's min and max widths.
    // We need to trim the margins of the first and last child early so that
    // these margins do not incorrectly constribute to the box's min/max width
    auto marginTrim = style().marginTrim();
    auto isRowsFlexbox = flexLayoutUtils().isHorizontalFlow();
    if (auto flexItem = firstInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineStart))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineStart.add(*flexItem) : m_marginTrimItems.m_itemsOnFirstFlexLine.add(*flexItem);
    if (auto flexItem = lastInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineEnd))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineEnd.add(*flexItem) : m_marginTrimItems.m_itemsOnLastFlexLine.add(*flexItem);
}

bool RenderFlexibleBox::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& flexItem) const
{
    ASSERT(style().marginTrim().contains(marginTrimSide));
    auto isMarginParallelWithMainAxis = [this](Style::MarginTrimSide marginTrimSide) {
        if (flexLayoutUtils().isHorizontalFlow())
            return marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::BlockEnd;
        return marginTrimSide == Style::MarginTrimSide::InlineStart || marginTrimSide == Style::MarginTrimSide::InlineEnd;
    };
    if (isMarginParallelWithMainAxis(marginTrimSide))
        return (marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::InlineStart) ? m_marginTrimItems.m_itemsOnFirstFlexLine.contains(flexItem) : m_marginTrimItems.m_itemsOnLastFlexLine.contains(flexItem);
    return (marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::InlineStart) ? m_marginTrimItems.m_itemsAtFlexLineStart.contains(flexItem) : m_marginTrimItems.m_itemsAtFlexLineEnd.contains(flexItem);
}

void RenderFlexibleBox::clearFlexItemOverridingSizes()
{
    for (auto* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (!flexItem->isOutOfFlowPositioned())
            flexItem->clearOverridingSize();
    }
}

const RenderBox* RenderFlexibleBox::flexItemForFirstBaseline() const
{
    // Looking for baseline flex candidate on visually first line.
    auto useLastLine = flexLayoutUtils().isWrapReverse();
    auto useLastItem = style().flexDirection() == FlexDirection::RowReverse || style().flexDirection() == FlexDirection::ColumnReverse;

    if (!useLastLine) {
        if (!useLastItem) {
            // Logically (and visually) first item on logically (and visually) first line.
            return firstBaselineCandidateOnLine(m_orderIterator, m_numberOfFlexItemsOnFirstLine);
        }
        // Logically last (but visually first) item on logically (and visually) first line.
        return lastBaselineCandidateOnLine(m_orderIterator, m_numberOfFlexItemsOnFirstLine);
    }

    if (!useLastItem) {
        // Logically (and visually) first item on logically last (but visually first) line.
        return lastBaselineCandidateOnLine(m_orderIterator.reverse(), m_numberOfFlexItemsOnLastLine);
    }
    // Logically last (but visually first) item on logically last (but visually first) line.
    return firstBaselineCandidateOnLine(m_orderIterator.reverse(), m_numberOfFlexItemsOnLastLine);
}

const RenderBox* RenderFlexibleBox::flexItemForLastBaseline() const
{
    // Looking for baseline flex candidate on visually last line.
    auto useLastLine = flexLayoutUtils().isWrapReverse();
    auto useLastItem = style().flexDirection() == FlexDirection::RowReverse || style().flexDirection() == FlexDirection::ColumnReverse;

    if (!useLastLine) {
        if (!useLastItem) {
            // Logically (and visually) last item on logically (and visually) last line.
            return firstBaselineCandidateOnLine(m_orderIterator.reverse(), m_numberOfFlexItemsOnLastLine);
        }
        // Logically first (but visually last) item  on logically (and visually) last line.
        return lastBaselineCandidateOnLine(m_orderIterator.reverse(), m_numberOfFlexItemsOnLastLine);
    }

    if (!useLastItem) {
        // Logically (and visually) last item on logically first (but visually last) line.
        return lastBaselineCandidateOnLine(m_orderIterator, m_numberOfFlexItemsOnFirstLine);
    }
    // Logically first (but visually last) item on logically last (but visually first) line.
    return firstBaselineCandidateOnLine(m_orderIterator, m_numberOfFlexItemsOnFirstLine);
}

const RenderBox* RenderFlexibleBox::firstBaselineCandidateOnLine(OrderIterator flexItemIterator, size_t numberOfItemsOnLine) const
{
    // Note that "first" here means in iterator order and not logical flex order (caller can pass in reversed order).
    size_t index = 0;
    const RenderBox* baselineFlexItem = nullptr;
    for (auto* flexItem = flexItemIterator.first(); flexItem; flexItem = flexItemIterator.next()) {
        if (flexItemIterator.shouldSkipChild(*flexItem))
            continue;
        auto flexItemPosition = flexLayoutUtils().alignmentForFlexItem(*flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*flexItem) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(*flexItem))
            return flexItem;
        if (!baselineFlexItem)
            baselineFlexItem = flexItem;
        if (++index == numberOfItemsOnLine)
            return baselineFlexItem;
    }
    return nullptr;
}

const RenderBox* RenderFlexibleBox::lastBaselineCandidateOnLine(OrderIterator flexItemIterator, size_t numberOfItemsOnLine) const
{
    // Note that "last" here means in iterator order and not logical flex order (caller can pass in reversed order).
    size_t index = 0;
    RenderBox* baselineFlexItem = nullptr;
    for (auto* flexItem = flexItemIterator.first(); flexItem; flexItem = flexItemIterator.next()) {
        if (flexItemIterator.shouldSkipChild(*flexItem))
            continue;
        auto flexItemPosition = flexLayoutUtils().alignmentForFlexItem(*flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && flexLayoutUtils().mainAxisIsFlexItemInlineAxis(*flexItem) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(*flexItem))
            baselineFlexItem = flexItem;
        if (++index == numberOfItemsOnLine)
            return baselineFlexItem ? baselineFlexItem : flexItem;
    }
    return nullptr;
}

void RenderFlexibleBox::prepareOrderIteratorAndMargins()
{
    OrderIteratorPopulator populator(m_orderIterator);

    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (!populator.collectChild(flexItem))
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (flexLayoutUtils().isHorizontalFlow()) {
            flexItem.setMarginLeft(flexLayoutUtils().computeFlexItemMarginValue(flexItem.style().marginLeft()));
            flexItem.setMarginRight(flexLayoutUtils().computeFlexItemMarginValue(flexItem.style().marginRight()));
        } else {
            flexItem.setMarginTop(flexLayoutUtils().computeFlexItemMarginValue(flexItem.style().marginTop()));
            flexItem.setMarginBottom(flexLayoutUtils().computeFlexItemMarginValue(flexItem.style().marginBottom()));
        }
    }
}

std::optional<LayoutUnit> RenderFlexibleBox::minimumHeightForLineIfEmpty() const
{
    // Even if we collected a flex line, the flexbox might not have a line because all our children
    // might be out of flow positioned. Make sure the flexbox has at least a line's worth of height.
    if (!hasLineIfEmpty())
        return { };
    return borderAndPaddingLogicalHeight() + lineHeight() + scrollbarLogicalHeight();
}

void RenderFlexibleBox::adjustLogicalHeightForLineIfEmpty()
{
    auto minHeight = minimumHeightForLineIfEmpty();
    if (minHeight && borderBoxHeight() < *minHeight)
        setLogicalHeight(*minHeight);
}

FlexLayoutConstraints RenderFlexibleBox::flexLayoutConstraints()
{
    auto& utils = flexLayoutUtils();
    return {
        .style = style(),
        .isHorizontalFlow = utils.isHorizontalFlow(),
        .isColumnFlow = utils.isColumnFlow(),
        .isMultiline = utils.isMultiline(),
        .isWrapReverse = utils.isWrapReverse(),
        .isColumnOrRowReverse = utils.isColumnOrRowReverse(),
        .isLeftToRightFlow = utils.isLeftToRightFlow(),
        .crossAxisDirection = utils.crossAxisDirection(),
        .flowAwareBorderInline = { utils.flowAwareBorderStart(), utils.flowAwareBorderEnd() },
        .flowAwareBorderBlock = { utils.flowAwareBorderBefore(), utils.flowAwareBorderAfter() },
        .flowAwarePaddingInline = { utils.flowAwarePaddingStart(), utils.flowAwarePaddingEnd() },
        .flowAwarePaddingBlock = { utils.flowAwarePaddingBefore(), utils.flowAwarePaddingAfter() },
        .mainAxisAvailableSpace = mainAxisAvailableSpace(),
        .mainAxisSizeForLengthResolution = utils.isColumnFlow() ? availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) : contentBoxLogicalWidth(),
        .minimumHeightForLineIfEmpty = minimumHeightForLineIfEmpty(),
    };
}

LayoutUnit RenderFlexibleBox::mainAxisAvailableSpace()
{
    if (!flexLayoutUtils().isColumnFlow())
        return contentBoxLogicalWidth();

    auto logicalHeightIgnoringFlexBasisOverride = [&] {
        // The flex-basis override is for the parent flex's sizing of this item,
        // not for this container's own wrapping decisions. Temporarily clear it
        // so computeLogicalHeight sees the specified height.
        auto override = overridingLogicalHeightForFlexBasisComputation();
        if (!override)
            return computeLogicalHeight(LayoutUnit::max(), logicalTop()).extent;

        clearOverridingLogicalHeightForFlexBasisComputation();
        auto computedValues = computeLogicalHeight(LayoutUnit::max(), logicalTop());
        setOverridingBorderBoxLogicalHeightForFlexBasisComputation(*override);
        return computedValues.extent;
    };
    auto logicalHeight = logicalHeightIgnoringFlexBasisOverride();
    return logicalHeight == LayoutUnit::max() ? logicalHeight : std::max(0_lu, logicalHeight - (borderAndPaddingLogicalHeight() + scrollbarLogicalHeight()));
}

void RenderFlexibleBox::setLogicalHeightForRowFlexContent(LayoutUnit contentLogicalHeight)
{
    // Row flow's cross size is the content extent FlexLayout accumulated from the lines, including the gaps
    // between them. (Column flow's logical height is its main size, already set while placing the items.)
    setLogicalHeight(contentLogicalHeight);
}

void RenderFlexibleBox::finalizeFlexContainerLogicalHeight(std::optional<LayoutUnit> minimumHeightForLineIfEmpty)
{
    // Reserve a line's worth of height if the container has a line even while empty, then resolve the final
    // logical height against the container's own specified/min/max height and box-sizing.
    if (minimumHeightForLineIfEmpty && borderBoxHeight() < *minimumHeightForLineIfEmpty)
        setLogicalHeight(*minimumHeightForLineIfEmpty);
    updateLogicalHeight();
}

FlexLayoutItems RenderFlexibleBox::collectFlexItems(RelayoutChildren relayoutChildren)
{
    // Build this container's flex items in order-modified document order, skipping out-of-flow children (which are not flex items).
    FlexLayoutItems flexItems;
    for (CheckedPtr flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*flexItem)) {
            if (flexItem->isOutOfFlowPositioned())
                prepareFlexItemForPositionedLayout(*flexItem);
            continue;
        }
        auto everHadLayout = flexItem->everHadLayout();
        if (CheckedPtr flexibleBox = dynamicDowncast<RenderFlexibleBox>(flexItem.get()))
            flexibleBox->resetHasDefiniteHeight();
        if (everHadLayout && flexItem->hasTrimmedMargin(std::optional<Style::MarginTrimSide> { }))
            flexItem->clearTrimmedMarginsMarkings();
        if (flexItem->shouldInvalidateContentWidths())
            flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *flexItem);
        flexItems.append({ *flexItem, everHadLayout });
    }
    return flexItems;
}

void RenderFlexibleBox::prepareFlexItemForPositionedLayout(RenderBox& flexItem)
{
    ASSERT(flexItem.isOutOfFlowPositioned());
    flexItem.containingBlock()->addOutOfFlowBox(flexItem);
    CheckedPtr layer = flexItem.layer();
    LayoutUnit staticInlinePosition = flexLayoutUtils().flowAwareBorderStart() + flexLayoutUtils().flowAwarePaddingStart();
    if (layer->staticInlinePosition() != staticInlinePosition) {
        layer->setStaticInlinePosition(staticInlinePosition);
        if (flexItem.style().hasStaticInlinePosition(writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = flexLayoutUtils().flowAwareBorderBefore() + flexLayoutUtils().flowAwarePaddingBefore();
    if (layer->staticBlockPosition() != staticBlockPosition) {
        layer->setStaticBlockPosition(staticBlockPosition);
        if (flexItem.style().hasStaticBlockPosition(writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

bool RenderFlexibleBox::layoutUsingFlexFormattingContext()
{
    if (m_hasFlexFormattingContextLayout && !*m_hasFlexFormattingContextLayout) {
        // FIXME: Avoid continous content checking on (potentially) unsupported content. This ensures no pref impact on cases like resize etc.
        // Remove when canUseForFlexLayout becomes less expensive.
        return false;
    }

    m_hasFlexFormattingContextLayout = LayoutIntegration::canUseForFlexLayout(*this);
    if (!*m_hasFlexFormattingContextLayout)
        return false;

    auto flexLayout = LayoutIntegration::FlexLayout { *this };
    flexLayout.updateFormattingContexGeometries();

    flexLayout.layout();
    setLogicalHeight(std::max(logicalHeight(), borderAndPaddingLogicalHeight() + flexLayout.contentBoxLogicalHeight()));
    updateLogicalHeight();
    return true;
}

}

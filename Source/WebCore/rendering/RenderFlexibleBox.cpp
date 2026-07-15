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
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
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

static bool flexContainerIsHorizontalFlow(const RenderBox& flexItem)
{
    return downcast<RenderFlexibleBox>(*flexItem.parent()).flexLayoutUtils().isHorizontalFlow();
}

RenderFlexibleBox::FlexLayoutItem::FlexLayoutItem(RenderBox& flexItem, bool everHadLayout)
    : renderer(flexItem)
    , mainAxisBorderAndPadding(flexContainerIsHorizontalFlow(flexItem) ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent())
    , crossAxisBorderAndPadding(flexContainerIsHorizontalFlow(flexItem) ? flexItem.verticalBorderAndPaddingExtent() : flexItem.horizontalBorderAndPaddingExtent())
    , mainAxisIsInlineAxis(flexContainerIsHorizontalFlow(flexItem) == flexItem.isHorizontalWritingMode())
    , everHadLayout(everHadLayout)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::hypotheticalMainAxisMarginBoxSize(LayoutUnit hypotheticalMainContentSize) const
{
    return hypotheticalMainContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::flexBaseMarginBoxSize(LayoutUnit flexBaseContentSize) const
{
    return flexBaseContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::flexedMarginBoxSize(LayoutUnit mainSize) const
{
    return mainSize + mainAxisBorderAndPadding + mainAxisMargin;
}

const Style::ComputedStyle& RenderFlexibleBox::FlexLayoutItem::style() const
{
    return renderer->style();
}

static LayoutUnit constrainSizeByMinMax(LayoutUnit size, std::pair<LayoutUnit, LayoutUnit> minMaxSizes)
{
    return std::max(minMaxSizes.first, std::min(size, minMaxSizes.second));
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

static bool canSetFlexItemContentLogicalHeight(const RenderBox& flexItem)
{
    return !flexItem.isFloatingOrOutOfFlowPositioned() && !flexItem.shouldComputeLogicalHeightFromAspectRatio() && !is<RenderReplaced>(flexItem);
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

        performFlexLayout(relayoutChildren);

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
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * computeGap(GapType::BetweenItems);
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

LayoutUnit RenderFlexibleBox::computeGap(RenderFlexibleBox::GapType gapType) const
{
    // row-gap is used for gaps between flex items in column flows or for gaps between lines in row flows.
    bool usesRowGap = (gapType == GapType::BetweenItems) == flexLayoutUtils().isColumnFlow();
    auto& gap = usesRowGap ? style().rowGap() : style().columnGap();
    if (gap.isNormal()) [[likely]]
        return { };

    auto availableSize = usesRowGap ? availableLogicalHeightForPercentageComputation().value_or(0_lu) : contentBoxLogicalWidth();
    return Style::evaluateMinimum<LayoutUnit>(gap, availableSize, style().usedZoomForLength());
}

void RenderFlexibleBox::performFlexLayout(RelayoutChildren relayoutChildren)
{
    if (layoutUsingFlexFormattingContext())
        return;

    FlexBaseAndHypotheticalMainSizeList flexBaseAndHypotheticalMainSizeList;
    auto flexItems = collectFlexItems(relayoutChildren, flexBaseAndHypotheticalMainSizeList);
    if (flexItems.isEmpty()) {
        adjustLogicalHeightForLineIfEmpty();
        updateLogicalHeight();
        return;
    }

    LayoutUnit gapBetweenItems = computeGap(GapType::BetweenItems);
    LayoutUnit gapBetweenLines = computeGap(GapType::BetweenLines);

    FlexLines flexLines;
    Vector<LayoutUnit> mainSizeList;
    Vector<LayoutUnit> lineCrossSizeList;
    Vector<LayoutUnit> lineCrossOffsetList;
    LayoutUnit crossAxisStartEdge;

    auto performContentSizing = [&] {
        InspectorInstrumentation::flexibleBoxRendererBeganLayout(*this);
        // 9.3. (#5) Collect the flex items into flex lines.
        flexLines = computeFlexLines(flexItems, flexBaseAndHypotheticalMainSizeList.span());
        // 9.3. (#6) Resolve the flexible lengths to find the used main size of each item.
        mainSizeList = computeMainSizeForFlexItems(flexItems, flexLines, flexBaseAndHypotheticalMainSizeList.span(), gapBetweenItems);
        trimCrossAxisMarginsForFlexItems(flexItems, flexLines);
        layoutFlexItems(flexItems.mutableSpan(), mainSizeList.span(), relayoutChildren);
        // 9.4. (#7) Determine the hypothetical cross size of each item.
        auto hypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems);
        // 9.4. (#8) Calculate the cross size of each flex line.
        lineCrossSizeList = crossSizeForFlexLines(flexLines, flexItems, hypotheticalCrossSizeList);

        // Record each line's cross-axis offset, growing the container's cross size to fit the lines (row flow).
        lineCrossOffsetList = Vector<LayoutUnit>(flexLines.ranges.size());
        LayoutUnit crossAxisOffset = flexLayoutUtils().flowAwareBorderBefore() + flexLayoutUtils().flowAwarePaddingBefore();
        for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
            InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(*this, flexLines.ranges[lineIndex].end());
            if (!flexLayoutUtils().isColumnFlow())
                setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flexLayoutUtils().flowAwareBorderAfter() + flexLayoutUtils().flowAwarePaddingAfter() + lineCrossSizeList[lineIndex] + flexLayoutUtils().crossAxisScrollbarExtent()));
            lineCrossOffsetList[lineIndex] = crossAxisOffset;
            crossAxisOffset += lineCrossSizeList[lineIndex];
        }
    };
    performContentSizing();

    Vector<LayoutPoint> positionList;
    Vector<LayoutUnit> crossSizeList;
    auto performContentAlignment = [&] {
        // 9.5. (#12) Main-Axis Alignment.
        positionList = handleMainAxisAlignment(flexLines, flexItems, mainSizeList, lineCrossOffsetList, gapBetweenItems);
        setFlexItemCountsForFirstAndLastLine(flexLines);

        // 9.6. (#15) Determine the flex container's used cross size.
        adjustLogicalHeightForLineIfEmpty();
        if (!flexLayoutUtils().isColumnFlow() && flexLines.ranges.size() > 1)
            setLogicalHeight(logicalHeight() + gapBetweenLines * (flexLines.ranges.size() - 1));
        updateLogicalHeight();

        // Multi-line column flex only knows its main size now, so re-resolve the flexible lengths of any lines that were left short.
        distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(flexLines, flexItems, flexBaseAndHypotheticalMainSizeList.span(), mainSizeList, positionList, lineCrossOffsetList, gapBetweenItems);

        // 9.6. (#13 - #16) Cross-Axis Alignment.
        crossAxisStartEdge = lineCrossOffsetList.isEmpty() ? 0_lu : lineCrossOffsetList[0];
        // If we have a single line flexbox, the line height is all the available space. For flex-direction: row,
        // this means we need to use the height, so we do this after calling updateLogicalHeight.
        if (!flexLayoutUtils().isMultiline() && !lineCrossSizeList.isEmpty())
            lineCrossSizeList[0] = flexLayoutUtils().crossAxisContentExtent();

        // 9.4. (#9) Handle 'align-content: stretch' and 9.6. (#16) align all flex lines per align-content.
        handleCrossAxisAlignmentForFlexLines(flexLines, positionList, lineCrossOffsetList, lineCrossSizeList, gapBetweenLines);

        // 9.4. (#11) Determine the used cross size of each flex item.
        crossSizeList = computeCrossSizeForFlexItems(flexLines, flexItems, lineCrossSizeList);

        // 9.6. (#13 - #14) Resolve cross-axis auto margins and align each item per align-self.
        handleCrossAxisAlignmentForFlexItems(flexLines, flexItems, crossSizeList, lineCrossSizeList, positionList);
    };
    performContentAlignment();

    // 9.6. Place each flex item at its final flow-aware location, applying the wrap-reverse and rtl-column
    // cross-axis flips, and write it to the renderer (cf. FlexLayout::computeFlexItemRects).
    auto computeFlexItemRects = [&] {
        auto crossContentExtent = flexLayoutUtils().crossAxisContentExtent();
        auto crossExtent = flexLayoutUtils().crossAxisExtent();
        bool isRightToLeftColumn = !writingMode().isLogicalLeftInlineStart() && flexLayoutUtils().isColumnFlow();
        for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
            auto lineRange = flexLines.ranges[lineIndex];
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto location = positionList[flexItemIndex];
                if (flexLayoutUtils().isWrapReverse()) {
                    auto originalOffset = lineCrossOffsetList[lineIndex] - crossAxisStartEdge;
                    location.move(0_lu, (crossContentExtent - originalOffset - lineCrossSizeList[lineIndex]) - originalOffset);
                }
                if (isRightToLeftColumn) {
                    // For vertical flows, setFlowAwareLocationForFlexItem will transpose x and
                    // y, so using the y axis for a column cross axis extent is correct.
                    location.setY(crossExtent - crossSizeList[flexItemIndex] - location.y());
                    if (!isHorizontalWritingMode())
                        location.move(LayoutSize(0, -horizontalScrollbarHeight()));
                }
                setFlexItemGeometry(flexItems[flexItemIndex], location);
            }
        }
    };
    computeFlexItemRects();
}

RenderFlexibleBox::FlexLayoutItems RenderFlexibleBox::collectFlexItems(RelayoutChildren relayoutChildren, FlexBaseAndHypotheticalMainSizeList& sizingList)
{
    // Set up our master list of flex items. All of the rest of the algorithm
    // should work off this list of a subset.
    // FIXME: That second part is not yet true.
    FlexLayoutItems flexItems;
    for (CheckedPtr flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*flexItem)) {
            // Out-of-flow children are not flex items, so we skip them here.
            if (flexItem->isOutOfFlowPositioned())
                prepareFlexItemForPositionedLayout(*flexItem);
            continue;
        }
        auto prepareFlexItem = [&] {
            auto everHadLayout = flexItem->everHadLayout();
            if (CheckedPtr flexibleBox = dynamicDowncast<RenderFlexibleBox>(flexItem.get()))
                flexibleBox->resetHasDefiniteHeight();
            if (everHadLayout && flexItem->hasTrimmedMargin(std::optional<Style::MarginTrimSide> { }))
                flexItem->clearTrimmedMarginsMarkings();
            if (flexItem->shouldInvalidateContentWidths())
                flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
            updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *flexItem);
            return everHadLayout;
        };
        auto everHadLayout = prepareFlexItem();
        flexItems.append({ *flexItem, everHadLayout });
        auto& flexLayoutItem = flexItems.last();
        sizingList.append(flexBaseAndHypotheticalMainSize(flexLayoutItem));
        // Snapshot the main-axis margin now that flexBaseAndHypotheticalMainSize has laid the item out: an orthogonal
        // flex item only resolves its physical margins during that layout, so reading them at FlexLayoutItem
        // construction time (before layout) would capture stale values and throw off margin-trim.
        flexLayoutItem.mainAxisMargin = flexLayoutUtils().isHorizontalFlow() ? flexLayoutItem.renderer->horizontalMarginExtent() : flexLayoutItem.renderer->verticalMarginExtent();
        // flexBaseAndHypotheticalMainSize might set the override containing block height so any value cached for definiteness might be incorrect.
        resetHasDefiniteHeight();
    }
    return flexItems;
}

RenderFlexibleBox::FlexLines RenderFlexibleBox::computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList)
{
    // 9.3. (#5) Collect flex items into flex lines: a single-line container collects all items into one line; a
    // multi-line container collects consecutive items until the next item's outer hypothetical main size would not
    // fit in the inner main size. The first uncollected item is always collected, even if it does not fit.
    auto mainAxisAvailableSpace = this->mainAxisAvailableSpace();
    auto gapBetweenItems = computeGap(GapType::BetweenItems);
    FlexLines flexLines;
    size_t nextIndex = 0;
    while (nextIndex < flexItems.size()) {
        auto lineStartIndex = nextIndex;
        LayoutUnit sumFlexBaseSize;
        LayoutUnit sumHypotheticalMainSize;
        // Trim the main-axis margin of the item at the start of the flex line.
        if (flexLayoutUtils().shouldTrimMainAxisMarginStart())
            trimMainAxisMarginStart(flexItems[nextIndex]);
        for (; nextIndex < flexItems.size(); ++nextIndex) {
            const auto& flexLayoutItem = flexItems[nextIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            if (flexLayoutUtils().isMultiline() && (sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(sizingList[nextIndex].hypotheticalMainContentSize) > mainAxisAvailableSpace && !canFitItemWithTrimmedMarginEnd(flexLayoutItem, sizingList[nextIndex].hypotheticalMainContentSize, sumHypotheticalMainSize, mainAxisAvailableSpace)) && nextIndex > lineStartIndex)
                break;
            sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize(sizingList[nextIndex].flexBaseContentSize) + gapBetweenItems;
            sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize(sizingList[nextIndex].hypotheticalMainContentSize) + gapBetweenItems;
        }

        // We added a gap after every item but there shouldn't be one after the last item, so subtract it here. Note
        // that sums might be negative here due to negative margins in flex items.
        sumHypotheticalMainSize -= gapBetweenItems;
        sumFlexBaseSize -= gapBetweenItems;

        // Trim the main-axis margin of the item at the end of the flex line.
        if (flexLayoutUtils().shouldTrimMainAxisMarginEnd()) {
            auto& lastItem = flexItems[nextIndex - 1];
            removeMarginEndFromFlexSizes(lastItem, sumFlexBaseSize, sumHypotheticalMainSize);
            trimMainAxisMarginEnd(lastItem);
        }

        flexLines.ranges.append({ lineStartIndex, nextIndex });
        flexLines.hypotheticalMainSizes.append(sumHypotheticalMainSize);
    }
    return flexLines;
}

Vector<LayoutUnit> RenderFlexibleBox::computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, LayoutUnit gapBetweenItems)
{
    Vector<LayoutUnit> mainSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineSizing = sizingList.subspan(lineRange.begin(), lineRange.distance());
        auto lineMainSizes = mainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto containerMainInnerSize = flexLayoutUtils().isColumnFlow() ? flexLayoutUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : contentBoxLogicalWidth();
        resolveFlexibleLengthsForLineItems(lineItems, lineSizing, lineMainSizes, containerMainInnerSize, gapBetweenItems);
    }
    return mainSizeList;
}

LayoutUnit RenderFlexibleBox::resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem> lineItems, std::span<const FlexBaseAndHypotheticalMainSize> lineSizing, std::span<LayoutUnit> mainSizes, LayoutUnit containerMainInnerSize, LayoutUnit gapBetweenItems)
{
    Vector<bool> frozen(FillWith { }, lineItems.size(), false);
    double totalFlexGrow = 0;
    double totalFlexShrink = 0;
    double totalWeightedFlexShrink = 0;
    LayoutUnit sumFlexBaseSize;
    LayoutUnit sumHypotheticalMainSize;
    for (size_t index = 0; index < lineItems.size(); ++index) {
        auto& flexLayoutItem = lineItems[index];
        mainSizes[index] = lineSizing[index].flexBaseContentSize;
        totalFlexGrow += flexLayoutItem.style().flexGrow().value;
        totalFlexShrink += flexLayoutItem.style().flexShrink().value;
        totalWeightedFlexShrink += flexLayoutItem.style().flexShrink().value * lineSizing[index].flexBaseContentSize;
        sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize(lineSizing[index].flexBaseContentSize);
        sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize(lineSizing[index].hypotheticalMainContentSize);
    }
    if (lineItems.size() > 1) {
        auto totalGap = (lineItems.size() - 1) * gapBetweenItems;
        sumFlexBaseSize += totalGap;
        sumHypotheticalMainSize += totalGap;
    }

    auto remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
    // Step 1 (determine the used flex factor): if the summed hypothetical main sizes are less than the container's
    // inner main size, use the flex grow factor for the rest of the algorithm, otherwise the flex shrink factor.
    auto flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? FlexSign::PositiveFlexibility : FlexSign::NegativeFlexibility;

    auto freezeViolations = [&](Vector<size_t, 4>& violations) {
        for (auto index : violations) {
            auto& flexLayoutItem = lineItems[index];
            ASSERT(!frozen[index]);
            auto& flexItemStyle = flexLayoutItem.style();
            LayoutUnit flexItemSize = mainSizes[index];
            remainingFreeSpace -= flexItemSize - lineSizing[index].flexBaseContentSize;
            totalFlexGrow -= flexItemStyle.flexGrow().value;
            totalFlexShrink -= flexItemStyle.flexShrink().value;
            totalWeightedFlexShrink -= flexItemStyle.flexShrink().value * lineSizing[index].flexBaseContentSize;
            // totalWeightedFlexShrink can be negative when we exceed the precision of
            // a double when we initially calcuate totalWeightedFlexShrink. We then
            // subtract each child's weighted flex shrink with full precision, now
            // leading to a negative result. See
            // css3/flexbox/large-flex-shrink-assert.html
            totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
            frozen[index] = true;
        }
    };

    // Step 2 (size inflexible items), https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths: freeze, at
    // its hypothetical main size, every item with a flex factor of zero, and -- when growing -- any item whose flex
    // base size is greater than its hypothetical main size, or -- when shrinking -- smaller than it.
    Vector<size_t, 4> newInflexibleItems;
    for (size_t index = 0; index < lineItems.size(); ++index) {
        auto& flexLayoutItem = lineItems[index];
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        ASSERT(!frozen[index]);
        float flexFactor = (flexSign == FlexSign::PositiveFlexibility) ? flexLayoutItem.style().flexGrow().value : flexLayoutItem.style().flexShrink().value;
        if (!flexFactor || (flexSign == FlexSign::PositiveFlexibility && lineSizing[index].flexBaseContentSize > lineSizing[index].hypotheticalMainContentSize) || (flexSign == FlexSign::NegativeFlexibility && lineSizing[index].flexBaseContentSize < lineSizing[index].hypotheticalMainContentSize)) {
            mainSizes[index] = lineSizing[index].hypotheticalMainContentSize;
            newInflexibleItems.append(index);
        }
    }
    freezeViolations(newInflexibleItems);

    // Step 3: record the initial free space (the remaining free space after freezing the inflexible items).
    auto initialFreeSpace = remainingFreeSpace;

    // Step 4 (loop): while unfrozen items remain, distribute the remaining free space over them by their flex
    // factors, clamp each to min/max, and freeze the items whose clamp introduced a violation.
    while (true) {
        LayoutUnit totalViolation;
        LayoutUnit usedFreeSpace;
        Vector<size_t, 4> minViolations;
        Vector<size_t, 4> maxViolations;

        // If the unfrozen items' flex factors sum to less than one, multiply the initial free space by that sum
        // and use it as the remaining free space when its magnitude is smaller.
        double sumFlexFactors = (flexSign == FlexSign::PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
        if (sumFlexFactors > 0 && sumFlexFactors < 1) {
            LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
            if (fractional.abs() < remainingFreeSpace.abs())
                remainingFreeSpace = fractional;
        }

        for (size_t index = 0; index < lineItems.size(); ++index) {
            auto& flexLayoutItem = lineItems[index];
            // This check also covers out-of-flow children.
            if (frozen[index])
                continue;

            auto& flexItemStyle = flexLayoutItem.style();
            LayoutUnit flexItemSize = lineSizing[index].flexBaseContentSize;
            double extraSpace = 0;
            if (remainingFreeSpace > 0 && totalFlexGrow > 0 && flexSign == FlexSign::PositiveFlexibility && std::isfinite(totalFlexGrow))
                extraSpace = remainingFreeSpace * (flexItemStyle.flexGrow().value / totalFlexGrow);
            else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == FlexSign::NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && !flexItemStyle.flexShrink().isZero())
                extraSpace = remainingFreeSpace * flexItemStyle.flexShrink().value * lineSizing[index].flexBaseContentSize / totalWeightedFlexShrink;
            if (std::isfinite(extraSpace))
                flexItemSize += LayoutUnit::fromFloatRound(extraSpace);

            LayoutUnit adjustedFlexItemSize = constrainSizeByMinMax(flexItemSize, lineSizing[index].minMaxMainSizes);
            ASSERT(adjustedFlexItemSize >= 0);
            mainSizes[index] = adjustedFlexItemSize;
            usedFreeSpace += adjustedFlexItemSize - lineSizing[index].flexBaseContentSize;

            LayoutUnit violation = adjustedFlexItemSize - flexItemSize;
            if (violation > 0)
                minViolations.append(index);
            else if (violation < 0)
                maxViolations.append(index);
            totalViolation += violation;
        }

        if (!totalViolation) {
            remainingFreeSpace -= usedFreeSpace;
            break;
        }
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations);
    }

    return remainingFreeSpace;
}

void RenderFlexibleBox::distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines& flexLines, FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, Vector<LayoutUnit>& mainSizeList, Vector<LayoutPoint>& positionList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems)
{
    // In multi-line column flex, the container's main size (height) is only known
    // after all lines are laid out. Lines whose items had flex-grow may not have
    // received enough space because the container height wasn't final during the
    // per-line pass. Re-resolve and relayout those lines now.
    if (!flexLayoutUtils().isMultiline() || !flexLayoutUtils().isColumnFlow())
        return;

    auto containerMainInnerSize = contentBoxLogicalHeight();
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineSizing = sizingList.subspan(lineRange.begin(), lineRange.distance());
        auto lineMainSizes = mainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto linePositions = positionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());

        auto lineMainSize = LayoutUnit { };
        for (size_t index = 0; index < lineItems.size(); ++index)
            lineMainSize += lineItems[index].flexedMarginBoxSize(lineMainSizes[index]);
        lineMainSize += (lineItems.size() - 1) * gapBetweenItems;
        if (lineMainSize >= containerMainInnerSize)
            continue;

        resolveFlexibleLengthsForLineItems(lineItems, lineSizing, lineMainSizes, containerMainInnerSize, gapBetweenItems);

        auto remainingFreeSpace = containerMainInnerSize;
        for (size_t index = 0; index < lineItems.size(); ++index)
            remainingFreeSpace -= lineItems[index].flexedMarginBoxSize(lineMainSizes[index]);
        remainingFreeSpace -= (lineItems.size() - 1) * gapBetweenItems;

        layoutFlexItems(lineItems, lineMainSizes, RelayoutChildren::No);
        placeFlexItems(lineCrossOffsetList[lineIndex], lineItems, linePositions, remainingFreeSpace, gapBetweenItems);
    }
}

void RenderFlexibleBox::trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines)
{
    // Cross axis margins are only trimmed on the first and last flex line.
    auto shouldTrimStart = flexLayoutUtils().shouldTrimCrossAxisMarginStart();
    auto shouldTrimEnd = flexLayoutUtils().shouldTrimCrossAxisMarginEnd();
    if (!shouldTrimStart && !shouldTrimEnd)
        return;

    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto shouldTrimCrossAxisStart = shouldTrimStart && !lineIndex;
        auto shouldTrimCrossAxisEnd = shouldTrimEnd && lineIndex == flexLines.ranges.size() - 1;
        if (!shouldTrimCrossAxisStart && !shouldTrimCrossAxisEnd)
            continue;

        auto lineRange = flexLines.ranges[lineIndex];
        for (auto& flexLayoutItem : flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance())) {
            if (shouldTrimCrossAxisStart)
                trimCrossAxisMarginStart(flexLayoutItem);
            if (shouldTrimCrossAxisEnd)
                trimCrossAxisMarginEnd(flexLayoutItem);
        }
    }
}

void RenderFlexibleBox::layoutFlexItems(std::span<FlexLayoutItem> flexLayoutItems, std::span<const LayoutUnit> mainSizes, RelayoutChildren relayoutChildren)
{
    for (size_t index = 0; index < flexLayoutItems.size(); ++index)
        layoutFlexItemAfterMainSizing(flexLayoutItems[index], mainSizes[index], relayoutChildren);
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
    bool forceFlexItemRelayout = relayoutChildren == RelayoutChildren::Yes && !m_flexItemsWithCompletedLayout.contains(flexItem);
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
        m_flexItemsWithCompletedLayout.add(flexItem);

    {
        auto flexLayoutScope = SetForScope(m_afterMainAxisItemSizing, true);
        flexItem.layoutIfNeeded();
    }

    if (!flexLayoutItem.everHadLayout && flexItem.checkForRepaintDuringLayout()) {
        flexItem.repaint();
        flexItem.repaintOverhangingFloats(true);
    }
}

Vector<LayoutUnit> RenderFlexibleBox::hypotheticalCrossSizeForFlexItems(const FlexLayoutItems& flexItems)
{
    // 9.4. (#7) The hypothetical cross size of each item is the cross size it would have at its used main size.
    Vector<LayoutUnit> hypotheticalCrossSizeList(flexItems.size());
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        hypotheticalCrossSizeList[flexItemIndex] = crossAxisIntrinsicExtentForFlexItem(flexItems[flexItemIndex]);
    return hypotheticalCrossSizeList;
}

Vector<LayoutUnit> RenderFlexibleBox::crossSizeForFlexLines(const FlexLines& flexLines, const FlexLayoutItems& flexItems, const Vector<LayoutUnit>& hypotheticalCrossSizeList)
{
    // 9.4. (#8) The used cross size of each flex line is the largest of: the summed baseline ascent and descent of
    // its baseline-aligned items, the largest outer hypothetical cross size of the remaining items, and zero. (The
    // single-line container with a definite cross size uses the inner cross size; the caller applies that.)
    Vector<LayoutUnit> flexLinesCrossSizeList(flexLines.ranges.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];

        LayoutUnit maxFlexItemCrossAxisExtent;
        LayoutUnit maxAscent;
        LayoutUnit maxDescent = LayoutUnit::min();
        LayoutUnit lastBaselineMaxAscent;
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexItem = flexItems[flexItemIndex].renderer.get();
            ASSERT(!flexItem.isOutOfFlowPositioned());

            LayoutUnit flexItemCrossAxisMarginBoxExtent;
            auto alignment = flexLayoutUtils().alignmentForFlexItem(flexItem);
            if ((alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem)) {
                LayoutUnit ascent = flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, flexLayoutUtils().crossAxisExtentForFlexItem(flexItem));
                LayoutUnit descent = (flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem) + flexLayoutUtils().crossAxisExtentForFlexItem(flexItem)) - ascent;
                maxDescent = std::max(maxDescent, descent);
                if (alignment == ItemPosition::Baseline) {
                    maxAscent = std::max(maxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = maxAscent + maxDescent;
                } else {
                    lastBaselineMaxAscent = std::max(lastBaselineMaxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = lastBaselineMaxAscent + maxDescent;
                }
            } else
                flexItemCrossAxisMarginBoxExtent = hypotheticalCrossSizeList[flexItemIndex] + flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem);

            maxFlexItemCrossAxisExtent = std::max(maxFlexItemCrossAxisExtent, flexItemCrossAxisMarginBoxExtent);
        }
        flexLinesCrossSizeList[lineIndex] = maxFlexItemCrossAxisExtent;
    }
    return flexLinesCrossSizeList;
}

Vector<LayoutPoint> RenderFlexibleBox::handleMainAxisAlignment(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& mainSizeList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems)
{
    Vector<LayoutPoint> positionList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto containerMainInnerSize = flexLayoutUtils().isColumnFlow() ? flexLayoutUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : contentBoxLogicalWidth();

        // The remaining free space is the line's inner main size minus the used outer main sizes of its items.
        // (The 0..1 flex-factor adjustment means we recompute it here rather than trust the resolve step's leftover.)
        auto remainingFreeSpace = containerMainInnerSize;
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            ASSERT(!flexItems[flexItemIndex].renderer->isOutOfFlowPositioned());
            remainingFreeSpace -= mainSizeList[flexItemIndex] + flexItems[flexItemIndex].mainAxisBorderAndPadding + flexItems[flexItemIndex].mainAxisMargin;
        }
        remainingFreeSpace -= (lineRange.distance() - 1) * gapBetweenItems;

        placeFlexItems(lineCrossOffsetList[lineIndex], flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance()), positionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance()), remainingFreeSpace, gapBetweenItems);
    }
    return positionList;
}

Vector<LayoutUnit> RenderFlexibleBox::computeCrossSizeForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& lineCrossSizeList)
{
    Vector<LayoutUnit> crossSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexItem = flexItems[flexItemIndex].renderer.get();
            ASSERT(!flexItem.isOutOfFlowPositioned());
            // If a flex item has align-self: stretch, its computed cross size property is auto, and neither of its cross-axis margins are auto, the used outer cross size is the used cross size
            // of its flex line, clamped according to the item's used min and max cross sizes. Otherwise, the used cross size is the item's hypothetical cross size.
            if (flexLayoutUtils().alignmentForFlexItem(flexItem) == ItemPosition::Stretch && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
                crossSizeList[flexItemIndex] = applyStretchAlignmentToFlexItem(flexItems[flexItemIndex], lineCrossSizeList[lineIndex]);
            else
                crossSizeList[flexItemIndex] = flexLayoutUtils().crossAxisExtentForFlexItem(flexItem);
        }
    }
    return crossSizeList;
}

void RenderFlexibleBox::handleCrossAxisAlignmentForFlexLines(const FlexLines& flexLines, Vector<LayoutPoint>& positionList, Vector<LayoutUnit>& lineCrossOffsetList, Vector<LayoutUnit>& lineCrossSizeList, LayoutUnit gapBetweenLines)
{
    // 9.6. (#16) Align the flex lines within the flex container per align-content, and (#9) grow the lines to fill
    // the container for align-content: stretch. A single-line container has nothing to align.
    if (flexLines.ranges.isEmpty() || !flexLayoutUtils().isMultiline())
        return;

    auto alignedContent = style().alignContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
    auto position = alignedContent.position();
    auto distribution = alignedContent.distribution();
    auto safety = alignedContent.overflow();

    bool isWrapReverse = flexLayoutUtils().isWrapReverse();

    if (position == ContentPosition::FlexStart && !gapBetweenLines && safety != OverflowAlignment::Safe && !isWrapReverse)
        return;

    size_t numLines = flexLines.ranges.size();
    LayoutUnit availableCrossAxisSpace = flexLayoutUtils().crossAxisContentExtent() - (numLines - 1) * gapBetweenLines;
    for (size_t i = 0; i < numLines; ++i)
        availableCrossAxisSpace -= lineCrossSizeList[i];

    m_alignContentStartOverflow = FlexLayoutUtils::contentAlignmentStartOverflow(availableCrossAxisSpace, position, distribution, safety, isWrapReverse);
    LayoutUnit lineOffset = FlexLayoutUtils::initialAlignContentOffset(availableCrossAxisSpace, position, distribution, safety, numLines, isWrapReverse);
    for (unsigned lineNumber = 0; lineNumber < numLines; ++lineNumber) {
        lineCrossOffsetList[lineNumber] += lineOffset;
        // Fold this line's align-content offset into each of its items' cross-axis position.
        auto lineRange = flexLines.ranges[lineNumber];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex)
            positionList[flexItemIndex].move(0_lu, lineOffset);

        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            lineCrossSizeList[lineNumber] += availableCrossAxisSpace / static_cast<unsigned>(numLines);

        lineOffset += FlexLayoutUtils::alignContentSpaceBetweenFlexItems(availableCrossAxisSpace, distribution, numLines) + gapBetweenLines;
    }
}

void RenderFlexibleBox::handleCrossAxisAlignmentForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& crossSizeList, const Vector<LayoutUnit>& lineCrossSizeList, Vector<LayoutPoint>& positionList)
{
    // 9.6. (#13, #14) For each item resolve its cross-axis auto margins, then -- when neither cross-axis margin is
    // auto -- align it within its line per align-self (baseline-aligned items go through performBaselineAlignment).
    Vector<LayoutUnit> crossItemOffsetList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        LayoutUnit lineCrossAxisExtent = lineCrossSizeList[lineIndex];

        performBaselineAlignment(lineRange, flexItems, crossItemOffsetList, crossSizeList, lineCrossAxisExtent);

        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexLayoutItem = flexItems[flexItemIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());

            auto safety = flexLayoutUtils().overflowAlignmentForFlexItem(flexLayoutItem.renderer);
            auto position = flexLayoutUtils().alignmentForFlexItem(flexLayoutItem.renderer);
            if (updateAutoMarginsInCrossAxis(flexLayoutItem, crossItemOffsetList[flexItemIndex], std::max(0_lu, flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer, crossSizeList[flexItemIndex]))) || position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
                continue;

            LayoutUnit availableSpace = flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer, crossSizeList[flexItemIndex]);
            if (availableSpace < 0 && safety == OverflowAlignment::Safe)
                position = ItemPosition::FlexStart; // See Start == FlexStart assumption in flexLayoutUtils().alignmentForFlexItem().
            LayoutUnit offset = FlexLayoutUtils::alignmentOffset(availableSpace, position, { }, { }, flexLayoutUtils().isWrapReverse());
            crossItemOffsetList[flexItemIndex] += offset;
        }
    }
    // Fold each item's cross-axis alignment offset (align-self / auto-margin / baseline) into its position.
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        positionList[flexItemIndex].move(0_lu, crossItemOffsetList[flexItemIndex]);
}

void RenderFlexibleBox::performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems& flexItems, Vector<LayoutUnit>& crossItemOffsetList, const Vector<LayoutUnit>& crossSizeList, LayoutUnit lineCrossAxisExtent)
{
    // 9.6. (#14) Align each baseline-aligned item (align-self: baseline / last baseline) so its baseline sits on
    // its baseline-sharing group's shared baseline within the flex line.
    bool containerHasWrapReverse = flexLayoutUtils().isWrapReverse();

    auto flexItemWritingModeForBaselineAlignment = [&](const RenderBox& flexItem) {
        if (flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem))
            return flexItem.style().writingMode();

        auto alignmentContextAxis = style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        return BaselineAlignment::usedWritingModeForBaselineAlignment(alignmentContextAxis, writingMode(), flexItem.writingMode());
    };

    auto shouldAdjustItemTowardsCrossAxisEnd = [&](const FlowDirection& flexItemBlockFlowDirection, ItemPosition alignment) {
        ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);

        // The direction in which we are aligning (i.e. direction of the cross axis) must be parallel with the direction of the flex item's used writing mode
        ASSERT_IMPLIES(flexLayoutUtils().crossAxisDirection() == RenderFlexibleBox::Direction::TopToBottom || flexLayoutUtils().crossAxisDirection() == RenderFlexibleBox::Direction::BottomToTop, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::TopToBottom || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::BottomToTop);
        ASSERT_IMPLIES(flexLayoutUtils().crossAxisDirection() == RenderFlexibleBox::Direction::LeftToRight || flexLayoutUtils().crossAxisDirection() == RenderFlexibleBox::Direction::RightToLeft, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::LeftToRight || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::RightToLeft);

        // For first baseline aligned items, if its block direction is the opposite of
        // the cross axis direction, then that means its fallback alignment (safe self-start)
        // is in the direction of the end of the cross axis
        //
        // For last baseline aligned items, if its block direction is in the same direction as
        // the cross axis direction, then that means its fallback alignment (safe self-end) is
        // in the direction of the end of the cross axis
        if (alignment == ItemPosition::Baseline)
            return flexLayoutUtils().crossAxisDirection() != flexItemBlockFlowDirection;
        return flexLayoutUtils().crossAxisDirection() == flexItemBlockFlowDirection;
    };

    // Build the baseline sharing groups for this line: first- and last-baseline items whose cross-axis margins are both non-auto.
    std::optional<BaselineAlignmentState> baselineAlignmentState;
    BaselineSharingGroups baselineSharingGroups;
    for (auto itemIndex = lineRange.begin(); itemIndex < lineRange.end(); ++itemIndex) {
        auto& flexItem = flexItems[itemIndex].renderer.get();
        auto alignment = flexLayoutUtils().alignmentForFlexItem(flexItem);
        if ((alignment != ItemPosition::Baseline && alignment != ItemPosition::LastBaseline) || flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
            continue;
        if (!baselineAlignmentState) {
            auto alignmentContextAxis = style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
            baselineAlignmentState = BaselineAlignmentState { alignmentContextAxis, style().writingMode() };
        }
        auto baselineSharingGroupIndex = baselineAlignmentState->sharedGroupIndex(flexItem.writingMode(), alignment);
        if (baselineSharingGroupIndex == baselineSharingGroups.size())
            baselineSharingGroups.append({ });
        auto& group = baselineSharingGroups[baselineSharingGroupIndex];
        group.maxAscent = std::max(group.maxAscent, flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, crossSizeList[itemIndex]));
        group.items.append(itemIndex);
    }

    for (auto& baselineSharingGroup : baselineSharingGroups) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        for (auto itemIndex : baselineSharingGroup.items) {
            auto& flexItem = flexItems[itemIndex].renderer.get();
            auto position = flexLayoutUtils().alignmentForFlexItem(flexItem);
            ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
            auto offset = FlexLayoutUtils::alignmentOffset(flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem, crossSizeList[itemIndex]), position, flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, crossSizeList[itemIndex]), baselineSharingGroup.maxAscent, containerHasWrapReverse);
            crossItemOffsetList[itemIndex] += offset;

            if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), position))
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem, crossSizeList[itemIndex]) - offset);
        }
        // css-align-3 9.3 part 3:
        // Position the aligned baseline-sharing group within the alignment container according to its
        // fallback alignment. The fallback alignment of a baseline-sharing group is the fallback alignment
        // of its items as resolved to physical directions.
        if (minMarginAfterBaseline) {
            for (auto itemIndex : baselineSharingGroup.items) {
                auto& flexItem = flexItems[itemIndex].renderer.get();
                if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), flexLayoutUtils().alignmentForFlexItem(flexItem)) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
                    crossItemOffsetList[itemIndex] += minMarginAfterBaseline;
            }
        }
    }
}

void RenderFlexibleBox::placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // 9.5. (#12) Position the items along the main axis: first give any positive free space to main-axis auto
    // margins, then place each item per justify-content (the initial offset plus the spacing between items).
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(flexLayoutItems, availableFreeSpace);
    LayoutUnit mainAxisOffset = flexLayoutUtils().flowAwareBorderStart() + flexLayoutUtils().flowAwarePaddingStart();
    mainAxisOffset += FlexLayoutUtils::initialJustifyContentOffset(style(), availableFreeSpace, flexLayoutItems.size(), flexLayoutUtils().isColumnOrRowReverse());
    if (style().flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += flexLayoutUtils().isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    if (availableFreeSpace < 0) {
        auto resolvedJustifyContent = style().justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
        auto distribution = resolvedJustifyContent.distribution();
        auto safety = resolvedJustifyContent.overflow();
        auto position = FlexLayoutUtils::resolveLeftRightAlignment(resolvedJustifyContent.position(), resolvedJustifyContent, style(), flexLayoutUtils().isColumnOrRowReverse());
        LayoutUnit overflow = FlexLayoutUtils::contentAlignmentStartOverflow(availableFreeSpace, position, distribution, safety, flexLayoutUtils().isColumnOrRowReverse());
        m_justifyContentStartOverflow = std::max(m_justifyContentStartOverflow, overflow);
    }

    LayoutUnit totalMainExtent = flexLayoutUtils().mainAxisExtent();

    auto resolvedJustifyContent = style().justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
    auto distribution = resolvedJustifyContent.distribution();
    bool shouldFlipMainAxis = !flexLayoutUtils().isColumnFlow() && !flexLayoutUtils().isLeftToRightFlow();
    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer.get();

        ASSERT(!flexItem.isOutOfFlowPositioned());

        updateAutoMarginsInMainAxis(flexItem, autoMarginOffset);

        mainAxisOffset += flexLayoutUtils().flowAwareMarginStartForFlexItem(flexItem);

        LayoutUnit flexItemMainExtent = flexLayoutUtils().mainAxisExtentForFlexItem(flexItem);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later by the rtl-column flip in computeFlexItemRects.
        auto leadingScrollbarSize = writingMode().isInlineFlipped() && writingMode().isVertical() ? flexLayoutUtils().mainAxisScrollbarExtent() : LayoutUnit();
        LayoutPoint location(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - flexItemMainExtent - leadingScrollbarSize : mainAxisOffset, crossAxisOffset + flexLayoutUtils().flowAwareMarginBeforeForFlexItem(flexItem));
        positions[i] = location;
        setFlexItemGeometry(flexLayoutItems[i], positions[i]);
        mainAxisOffset += flexItemMainExtent + flexLayoutUtils().flowAwareMarginEndForFlexItem(flexItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += FlexLayoutUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }

        // FIXME: Deal with pagination.
    }

    if (flexLayoutUtils().isColumnFlow())
        setLogicalHeight(std::max(logicalHeight(), mainAxisOffset + flexLayoutUtils().flowAwareBorderEnd() + flexLayoutUtils().flowAwarePaddingEnd() + scrollbarLogicalHeight()));

    if (style().flexDirection() == FlexDirection::ColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex
        // items since the start depends on the height of the flexbox, which we
        // only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(flexLayoutItems, positions, crossAxisOffset, availableFreeSpace, gapBetweenItems);
    }
}

void RenderFlexibleBox::layoutColumnReverse(std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // This is similar to the logic in placeFlexItems, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flexLayoutUtils().flowAwareBorderEnd() - flexLayoutUtils().flowAwarePaddingEnd();
    mainAxisOffset -= FlexLayoutUtils::initialJustifyContentOffset(style(), availableFreeSpace, flexLayoutItems.size(), flexLayoutUtils().isColumnOrRowReverse());
    mainAxisOffset -= flexLayoutUtils().isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    auto distribution = style().justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior()).distribution();

    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer;
        ASSERT(!flexItem->isOutOfFlowPositioned());
        mainAxisOffset -= flexLayoutUtils().mainAxisExtentForFlexItem(flexItem) + flexLayoutUtils().flowAwareMarginEndForFlexItem(flexItem);
        positions[i] = LayoutPoint(mainAxisOffset, crossAxisOffset + flexLayoutUtils().flowAwareMarginBeforeForFlexItem(flexItem));
        setFlexItemGeometry(flexLayoutItems[i], positions[i]);
        mainAxisOffset -= flexLayoutUtils().flowAwareMarginStartForFlexItem(flexItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset -= FlexLayoutUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }
    }
}

void RenderFlexibleBox::setFlexItemCountsForFirstAndLastLine(const FlexLines& flexLines)
{
    if (flexLines.ranges.isEmpty())
        return;

    auto isWrapReverse = flexLayoutUtils().isWrapReverse();
    auto firstLineItemsCountInOriginalOrder = flexLines.ranges.first().distance();
    auto lastLineItemsCountInOriginalOrder = flexLines.ranges.last().distance();

    m_numberOfFlexItemsOnFirstLine = !isWrapReverse ? firstLineItemsCountInOriginalOrder : lastLineItemsCountInOriginalOrder;
    m_numberOfFlexItemsOnLastLine = !isWrapReverse ? lastLineItemsCountInOriginalOrder : firstLineItemsCountInOriginalOrder;
}

void RenderFlexibleBox::adjustLogicalHeightForLineIfEmpty()
{
    if (!hasLineIfEmpty())
        return;

    // Even if we collected a flex line, the flexbox might not have a line because all our
    // children might be out of flow positioned.
    // Instead of just checking if we have a line, make sure the flexbox
    // has at least a line's worth of height to cover this case.
    LayoutUnit minHeight = borderAndPaddingLogicalHeight() + lineHeight() + scrollbarLogicalHeight();
    if (borderBoxHeight() < minHeight)
        setLogicalHeight(minHeight);
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

RenderFlexibleBox::FlexBaseAndHypotheticalMainSize RenderFlexibleBox::flexBaseAndHypotheticalMainSize(const FlexLayoutItem& flexLayoutItem)
{
    auto flexBaseContentSize = flexBaseSizeForFlexItem(flexLayoutItem);
    auto minMaxMainSizes = computeFlexItemMinMaxMainSizes(flexLayoutItem);
    // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes.
    return { flexBaseContentSize, std::max(minMaxMainSizes.first, std::min(flexBaseContentSize, minMaxMainSizes.second)), minMaxMainSizes };
}

// This is a RAII class that is used to temporarily set the flex basis as the child size in the main axis.
class ScopedFlexBasisAsFlexItemMainSize {
public:
    ScopedFlexBasisAsFlexItemMainSize(RenderBox& flexItem, Style::PreferredSize&& flexBasis, bool mainAxisIsInlineAxis)
        : m_flexItem(flexItem)
        , m_mainAxisIsInlineAxis(mainAxisIsInlineAxis)
    {
        if (flexBasis.isAuto())
            return;

        if (m_mainAxisIsInlineAxis)
            m_flexItem.setOverridingBorderBoxLogicalWidthForFlexBasisComputation(WTF::move(flexBasis));
        else
            m_flexItem.setOverridingBorderBoxLogicalHeightForFlexBasisComputation(WTF::move(flexBasis));
        m_didOverride = true;
    }

    ~ScopedFlexBasisAsFlexItemMainSize()
    {
        if (!m_didOverride)
            return;

        if (m_mainAxisIsInlineAxis)
            m_flexItem.clearOverridingLogicalWidthForFlexBasisComputation();
        else
            m_flexItem.clearOverridingLogicalHeightForFlexBasisComputation();
    }

private:
    RenderBox& m_flexItem;
    bool m_mainAxisIsInlineAxis { true };
    bool m_didOverride { false };
};

// https://drafts.csswg.org/css-flexbox/#algo-main-item
LayoutUnit RenderFlexibleBox::flexBaseSizeForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
    ScopedFlexBasisAsFlexItemMainSize scoped(flexItem, flexBasis.tryPreferredSize().value_or(Style::PreferredSize { CSS::Keyword::MaxContent { } }), flexLayoutItem.mainAxisIsInlineAxis);
    // FIXME: While we are supposed to ignore min/max here, the cached
    // m_blockAxisSize entry may hold a min/max-constrained size.
    SetForScope<bool> computingBaseSizesScope(m_isComputingFlexBaseSizes, true);
    ensureBlockAxisContentSizeForFlexItemIfNeeded(flexLayoutItem);

    // A. If the item has a definite used flex basis, that's the flex base size.
    if (flexItemMainSizeIsDefinite(flexItem, flexBasis))
        return std::max(0_lu, computeMainAxisExtentForFlexItem(flexLayoutItem, flexBasis).value());

    // B. If the flex item has a preferred aspect ratio, a used flex basis of content, and a definite cross size,
    // the flex base size is calculated from its used cross size and the flex item's aspect ratio.
    if (flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto& crossSizeLength = flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem);
        return adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossSizeLength));
    }

    // FIXME: C. If the used flex basis is content or depends on its available space, and the flex container is being
    // sized under a min-content or max-content constraint, size the item under that constraint.
    // FIXME: D. If the used flex basis is content or depends on its available space, the available main size is
    // infinite, and the flex item's inline axis is parallel to the main axis, lay the item out using the rules for a
    // box in an orthogonal flow. The flex base size is the item's max-content main size.

    // E. Otherwise, size the item into the available space using its used flex basis in place of its main size,
    // treating a value of content as max-content. The flex base size is the item's resulting main size.
    if (!flexLayoutItem.mainAxisIsInlineAxis) {
        ASSERT(!flexItem.needsLayout());
        ASSERT(m_blockAxisSize.contains(flexItem));
        return m_blockAxisSize.getOptional(flexItem).value_or(0_lu);
    }

    // We don't need to add scrollbarLogicalWidth here because the preferred
    // width includes the scrollbar, even for overflow: auto.
    ScopedCrossAxisOverrideForFlexItem crossSizeScope(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
    auto mainAxisExtent = flexItem.maxContentLogicalWidthContribution();
    auto mainAxisBorderAndPadding = flexLayoutUtils().isHorizontalFlow() ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return mainAxisExtent - mainAxisBorderAndPadding;
}

bool RenderFlexibleBox::flexBaseSizeNeedsBlockAxisContentSize(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if (flexLayoutItem.mainAxisIsInlineAxis)
        return false;

    auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
    auto minSize = flexLayoutUtils().minMainSizeLengthForFlexItem(flexItem);
    auto maxSize = flexLayoutUtils().maxMainSizeLengthForFlexItem(flexItem);
    // FIXME: we must run flexItemMainSizeIsDefinite() because it might end up calling computePercentageLogicalHeight()
    // which has some side effects like calling addPercentHeightDescendant() for example so it is not possible to skip
    // the call for example by moving it to the end of the conditional expression. This is error-prone and we should
    // refactor computePercentageLogicalHeight() at some point so that it only computes stuff without those side effects.
    if (!flexItemMainSizeIsDefinite(flexItem, flexBasis) || minSize.isIntrinsic() || maxSize.isIntrinsic())
        return true;

    if (flexLayoutUtils().useContentBasedMinimumSize(flexItem))
        return true;

    return false;
}

void RenderFlexibleBox::ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if (!flexBaseSizeNeedsBlockAxisContentSize(flexLayoutItem))
        return;

    if (!flexItem.needsLayout() && m_blockAxisSize.contains(flexItem))
        return;

    // Don't resolve percentages in children. This is especially important for the min-height calculation,
    // where we want percentages to be treated as auto. For flex-basis itself, this is not a problem because
    // by definition we have an indefinite flex basis here and thus percentages should not resolve.
    auto percentResolveDisableScope = FlexPercentResolveDisabler { view().frameView().layoutContext(), flexItem };
    flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    flexItem.layoutIfNeeded();

    auto innerSize = [&] {
        auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
        if (flexBasis.isPercentOrCalculated() && !flexItemMainSizeIsDefinite(flexItem, flexBasis))
            return flexItemContentLogicalHeight(flexItem) + flexItem.scrollbarLogicalHeight();
        return flexItem.logicalHeight() - flexItem.borderAndPaddingLogicalHeight();
    }();

    m_blockAxisSize.set(flexItem, innerSize);
    m_flexItemsWithCompletedLayout.add(flexItem);
}

std::pair<LayoutUnit, LayoutUnit> RenderFlexibleBox::computeFlexItemMinMaxMainSizes(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto maxExtent = computeUsedMaxMainSize(flexLayoutItem);
    auto resolvedMax = maxExtent.value_or(LayoutUnit::max());

    // useContentBasedMinimumSize covers both auto-equivalent cases: min:auto with
    // non-scrollable overflow (§ 4.5) and block-axis intrinsic keywords (CSS Sizing
    // 3 § 5.2 makes those behave like auto, regardless of overflow).
    if (flexLayoutUtils().useContentBasedMinimumSize(flexItem))
        return { computeContentBasedMinMainSize(flexLayoutItem, maxExtent), resolvedMax };

    auto min = flexLayoutUtils().minMainSizeLengthForFlexItem(flexItem);
    if (!min.isAuto())
        return { computeUsedNonAutoMinMainSize(flexLayoutItem, min), resolvedMax };

    // min:auto on a scroll container — spec says the automatic minimum size is zero.
    return { 0_lu, resolvedMax };
}

std::optional<LayoutUnit> RenderFlexibleBox::computeUsedMaxMainSize(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto max = flexLayoutUtils().maxMainSizeLengthForFlexItem(flexItem);
    if (max.isSpecified())
        return computeMainAxisExtentForFlexItem(flexLayoutItem, max);
    if (max.isIntrinsicOrStretch()) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        return computeMainAxisExtentForFlexItem(flexLayoutItem, max);
    }
    return { };
}

LayoutUnit RenderFlexibleBox::computeUsedNonAutoMinMainSize(const FlexLayoutItem& flexLayoutItem, const Style::MinimumSize& min)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // https://drafts.csswg.org/css-flexbox/#main-size-property
    // Resolves the used min main size for every case except min:auto. Three values
    // route here: a specified length/percentage, the stretch keyword, and intrinsic
    // keywords (min-content/max-content/fit-content) on the inline axis. Per CSS
    // Sizing 3 § 5.2, intrinsic keywords on the block axis behave like auto, so
    // those go through computeContentBasedMinMainSize instead.
    auto minExtent = [&] {
        if (min.isIntrinsicOrStretch()) {
            ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
            return computeMainAxisExtentForFlexItem(flexLayoutItem, min).value_or(0_lu);
        }
        return computeMainAxisExtentForFlexItem(flexLayoutItem, min).value_or(0_lu);
    }();

    // We must never return a min size smaller than the min preferred size for tables.
    if (flexItem.isRenderTable() && flexLayoutItem.mainAxisIsInlineAxis) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
        minExtent = std::max(minExtent, flexItem.minContentLogicalWidthContribution());
    }
    return minExtent;
}

LayoutUnit RenderFlexibleBox::computeContentBasedMinMainSize(const FlexLayoutItem& flexLayoutItem, std::optional<LayoutUnit> maxExtent)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // The content-based minimum size (https://drafts.csswg.org/css-flexbox/#min-size-auto): for a non-replaced item
    // the larger, and for a replaced item the smaller, of the content size suggestion and the transferred size
    // suggestion, capped by the specified size suggestion, and clamped by the definite maximum main size.
    // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForFlexItem and
    // ensure it's valid through the virtual calls of computeSizingKeywordLogicalContentHeightUsing.
    LayoutUnit contentSize;
    auto& flexItemCrossSizeLength = flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem);

    // Content size suggestion: the min-content size in the main axis, clamped (when the item has a preferred
    // aspect ratio) through the aspect ratio.
    bool canComputeSizeThroughAspectRatio = flexLayoutUtils().flexItemHasComputableAspectRatio(flexItem) && flexItemCrossSizeIsDefinite(flexLayoutItem, flexItemCrossSizeLength);
    if (canComputeSizeThroughAspectRatio)
        contentSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);

    if (!canComputeSizeThroughAspectRatio || !flexItem.isRenderReplaced()) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        auto minContentSize = computeMainAxisExtentForFlexItem(flexLayoutItem, Style::MinimumSize { CSS::Keyword::MinContent { } }).value_or(0_lu);
        contentSize = std::max(contentSize, minContentSize);
    }

    if (flexLayoutUtils().flexItemHasAspectRatio(flexItem))
        contentSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, contentSize);

    contentSize = std::max(0_lu, contentSize);
    ASSERT(contentSize >= 0);
    contentSize = std::min(contentSize, maxExtent.value_or(contentSize));

    // Specified size suggestion: if the item's preferred main size is definite, cap the result by that size.
    auto mainSize = flexLayoutUtils().preferredMainSizeLengthForFlexItem(flexItem);
    if (flexItemMainSizeIsDefinite(flexItem, mainSize)) {
        auto resolvedMainSize = computeMainAxisExtentForFlexItem(flexLayoutItem, mainSize).value_or(0);
        ASSERT(resolvedMainSize >= 0);
        auto specifiedSize = std::min(resolvedMainSize, maxExtent.value_or(resolvedMainSize));
        return std::min(specifiedSize, contentSize);
    }

    // Transferred size suggestion: a replaced item's definite cross size converted through its aspect ratio.
    if (flexItem.isRenderReplaced() && flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto transferredSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);
        transferredSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, transferredSize);
        return std::min(transferredSize, contentSize);
    }

    return contentSize;
}

// FIXME: consider adding this check to RenderBox::hasIntrinsicAspectRatio(). We could even make it
// virtual returning false by default. RenderReplaced will overwrite it with the current implementation
// plus this extra check. See wkb.ug/231955.

template<typename SizeType> std::optional<LayoutUnit> RenderFlexibleBox::computeMainAxisExtentForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    if (!flexLayoutItem.mainAxisIsInlineAxis) {
        // No "auto" check needed: computeContentLogicalHeight returns nullopt for
        // auto and we propagate that below.
        auto height = flexItem.computeContentLogicalHeight(size, flexItemContentLogicalHeight(flexItem));
        if (!height)
            return height;

        // Tables interpret overriding sizes as the size of captions + rows. However the specified height of a table
        // only includes the size of the rows. That's why we need to add the size of the captions here so that the table
        // layout algorithm behaves appropriately.
        LayoutUnit captionsHeight;
        if (CheckedPtr table = dynamicDowncast<RenderTable>(flexItem); table && flexItemMainSizeIsDefinite(flexItem, size))
            captionsHeight = table->sumCaptionsLogicalHeight();

        // scrollbarLogicalHeight depends on layout having run. flexBaseSizeForFlexItem
        // calls ensureBlockAxisContentSizeForFlexItemIfNeeded before reaching here,
        // which forces layout when flexBaseSizeNeedsBlockAxisContentSize is true. On
        // the false path (definite flex-basis + non-auto min-size + non-visible/clip
        // overflow) layout has not run and this returns 0; that coincides with the
        // spec, which does not attribute a scrollbar contribution to the flex base
        // size on that path.
        return *height + flexItem.scrollbarLogicalHeight() + captionsHeight;
    }

    // computeLogicalWidth always re-computes the intrinsic widths. However, when
    // our logical width is auto, we can just use our cached value. So let's do
    // that here. (Compare code in RenderBlock::computeIntrinsicLogicalWidthContributions)
    if (flexItem.style().logicalWidth().isAuto() && !flexLayoutUtils().flexItemHasAspectRatio(flexItem)) {
        if (size.isMinContent()) {
            if (flexItem.shouldInvalidateContentWidths())
                flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
            return flexItem.minContentLogicalWidthContribution() - flexItem.borderAndPaddingLogicalWidth();
        }
        if (size.isMaxContent()) {
            if (flexItem.shouldInvalidateContentWidths())
                flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
            return flexItem.maxContentLogicalWidthContribution() - flexItem.borderAndPaddingLogicalWidth();
        }
    }

    auto mainAxisWidth = flexLayoutUtils().isColumnFlow() ? availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) : contentBoxLogicalWidth();
    return flexItem.computeLogicalWidthUsing(size, mainAxisWidth, *this) - flexItem.borderAndPaddingLogicalWidth();
}

// FIXME: computeMainSizeFromAspectRatioUsing may need to return an std::optional<LayoutUnit> in the future
// rather than returning indefinite sizes as 0/-1.
template<typename SizeType> LayoutUnit RenderFlexibleBox::computeMainSizeFromAspectRatioUsing(const FlexLayoutItem& flexLayoutItem, const SizeType& crossSizeLength) const
{
    auto& flexItem = flexLayoutItem.renderer.get();
    ASSERT(flexLayoutUtils().flexItemHasAspectRatio(flexItem));
    auto flexItemCrossAxisBorderAndPadding = flexLayoutItem.crossAxisBorderAndPadding;

    // All paths return border-box cross size.
    auto crossSizeOptional = WTF::switchOn(crossSizeLength,
        [&](const SizeType::Fixed& fixedCrossSizeLength) -> std::optional<LayoutUnit> {
            auto value = LayoutUnit { fixedCrossSizeLength.resolveZoom(flexItem.style().usedZoomForLength()) };
            if (flexItem.style().boxSizing() == BoxSizing::ContentBox)
                value += flexItemCrossAxisBorderAndPadding;
            return value;
        },
        [&](const SizeType::Percentage& percentageCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem.computePercentageLogicalHeight(SizeType { percentageCrossSizeLength })
                : adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(percentageCrossSizeLength, contentBoxWidth()));
        },
        [&](const SizeType::Calc& calcCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem.computePercentageLogicalHeight(calcCrossSizeLength)
                : adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(calcCrossSizeLength, contentBoxWidth(), flexItem.style().usedZoomForLength()));
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            ASSERT(flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem));
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> {
            // Resolve stretch against the flex container's cross-axis definite size.
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_REACHED();
            return { };
        }
    );
    if (!crossSizeOptional)
        return 0_lu;

    auto crossSize = *crossSizeOptional;
    auto preferredAspectRatio = flexLayoutUtils().preferredAspectRatioForFlexItem(flexItem);

    auto useCSSAspectRatio = flexItem.style().aspectRatio().isRatio() || (flexItem.style().aspectRatio().isAutoAndRatio() && flexItem.intrinsicSize().isEmpty());
    if (!useCSSAspectRatio) {
        // Intrinsic aspect ratio (e.g. from <img>). The sizing calculations that floor
        // the content box size at zero when applying box-sizing are also ignored.
        // https://drafts.csswg.org/css-flexbox/#algo-main-item.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    auto boxSizingForAspectRatio = flexItem.style().boxSizingForAspectRatio();
    if (boxSizingForAspectRatio == BoxSizing::ContentBox) {
        // Ratio applies to content dimensions. Convert border-box cross size to content-box.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    // Ratio applies to border-box dimensions. Compute border-box main size,
    // then subtract main-axis border+padding to return content-box.
    ASSERT(flexItem.style().boxSizing() == BoxSizing::BorderBox);
    auto flexItemMainAxisBorderAndPadding = flexLayoutUtils().isHorizontalFlow() ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio } - flexItemMainAxisBorderAndPadding);
}

LayoutUnit RenderFlexibleBox::adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const FlexLayoutItem& flexLayoutItem, LayoutUnit flexItemSize)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto& crossMin = flexLayoutUtils().minCrossSizeLengthForFlexItem(flexItem);
    auto& crossMax = flexLayoutUtils().maxCrossSizeLengthForFlexItem(flexItem);

    if (flexItemCrossSizeIsDefinite(flexLayoutItem, crossMax)) {
        LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossMax);
        flexItemSize = std::min(maxValue, flexItemSize);
    }

    if (flexItemCrossSizeIsDefinite(flexLayoutItem, crossMin)) {
        LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossMin);
        flexItemSize = std::max(minValue, flexItemSize);
    }

    return flexItemSize;
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

LayoutUnit RenderFlexibleBox::crossAxisIntrinsicExtentForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    return flexLayoutItem.mainAxisIsInlineAxis ? flexItemIntrinsicLogicalHeight(flexLayoutItem) : flexItemIntrinsicLogicalWidth(flexLayoutItem);
}

LayoutUnit RenderFlexibleBox::flexItemIntrinsicLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // This should only be called if the logical height is the cross size
    ASSERT(flexLayoutItem.mainAxisIsInlineAxis);
    if (flexLayoutUtils().needToStretchFlexItemLogicalHeight(flexItem)) {
        LayoutUnit flexItemContentHeight = flexItemContentLogicalHeight(flexItem);
        LayoutUnit flexItemLogicalHeight = flexItemContentHeight + flexItem.scrollbarLogicalHeight() + flexItem.borderAndPaddingLogicalHeight();
        return flexItem.constrainLogicalHeightByMinMax(flexItemLogicalHeight, flexItemContentHeight);
    }
    return flexItem.logicalHeight();
}

LayoutUnit RenderFlexibleBox::flexItemIntrinsicLogicalWidth(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // This should only be called if the logical width is the cross size
    ASSERT(!flexLayoutItem.mainAxisIsInlineAxis);
    if (flexItemCrossSizeIsDefinite(flexLayoutItem, flexItem.style().logicalWidth()))
        return flexItem.logicalWidth();

    LogicalExtentComputedValues values;
    {
        OverridingSizesScope cleanOverridingWidthScope(flexItem, OverridingSizesScope::Axis::Inline);
        flexItem.computeLogicalWidth(values);
    }
    return values.extent;
}

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

template<typename SizeType> bool RenderFlexibleBox::flexItemCrossSizeIsDefinite(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if constexpr (!std::same_as<SizeType, Style::MaximumSize>) {
        if (size.isAuto())
            return false;
    }

    // Stretch is definite in the same cases as percentages, i.e. when the
    // container's cross size is definite. We use a dummy percentage for stretch
    // since computePercentageLogicalHeight evaluates the value as a percentage.
    auto crossSizeIsDefinite = [&](const auto& sizeForPercentageComputation) {
        if (!flexLayoutItem.mainAxisIsInlineAxis || m_hasDefiniteHeight == SizeDefiniteness::Definite)
            return true;
        if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
            return false;
        bool definite = bool(flexItem.computePercentageLogicalHeight(sizeForPercentageComputation));
        m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
        return definite;
    };

    if (size.isPercentOrCalculated())
        return crossSizeIsDefinite(size);

    if (size.isStretch())
        return crossSizeIsDefinite(Style::PreferredSize { 0_css_percentage });

    // FIXME: Support other intrinsic sizes (min-content, max-content, fit-content) here.
    // Requires updating computeMainSizeFromAspectRatioUsing.
    return size.isFixed();
}

bool RenderFlexibleBox::flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    return flexLayoutUtils().flexItemHasComputableAspectRatio(flexItem)
        && (flexItemCrossSizeIsDefinite(flexLayoutItem, flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem)) || flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem));
}

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

void RenderFlexibleBox::trimMainAxisMarginStart(FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = flexLayoutUtils().isHorizontalFlow();
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? flexLayoutItem.renderer->marginStart(writingMode())
        : flexLayoutItem.renderer->marginBefore(writingMode());
    if (horizontalFlow)
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    m_marginTrimItems.m_itemsAtFlexLineStart.add(flexLayoutItem.renderer);
}

void RenderFlexibleBox::trimMainAxisMarginEnd(FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = flexLayoutUtils().isHorizontalFlow();
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? flexLayoutItem.renderer->marginEnd(writingMode())
        : flexLayoutItem.renderer->marginAfter(writingMode());
    if (horizontalFlow)
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineEnd);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockEnd);
    m_marginTrimItems.m_itemsAtFlexLineEnd.add(flexLayoutItem.renderer);
}

void RenderFlexibleBox::trimCrossAxisMarginStart(const FlexLayoutItem& flexLayoutItem)
{
    if (flexLayoutUtils().isHorizontalFlow())
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    m_marginTrimItems.m_itemsOnFirstFlexLine.add(flexLayoutItem.renderer);
}

void RenderFlexibleBox::trimCrossAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    if (flexLayoutUtils().isHorizontalFlow())
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockEnd);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineEnd);
    m_marginTrimItems.m_itemsOnLastFlexLine.add(flexLayoutItem.renderer);
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

bool RenderFlexibleBox::canFitItemWithTrimmedMarginEnd(const FlexLayoutItem& flexLayoutItem, LayoutUnit hypotheticalMainContentSize, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const
{
    auto marginTrim = style().marginTrim();
    if ((flexLayoutUtils().isHorizontalFlow() && marginTrim.contains(Style::MarginTrimSide::InlineEnd)) || (flexLayoutUtils().isColumnFlow() && marginTrim.contains(Style::MarginTrimSide::BlockEnd)))
        return sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(hypotheticalMainContentSize) - flexLayoutUtils().flowAwareMarginEndForFlexItem(flexLayoutItem.renderer) <= mainAxisAvailableSpace;
    return false;
}

void RenderFlexibleBox::removeMarginEndFromFlexSizes(FlexLayoutItem& flexLayoutItem, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const
{
    LayoutUnit margin;
    if (flexLayoutUtils().isHorizontalFlow())
        margin = flexLayoutItem.renderer->marginEnd(writingMode());
    else
        margin = flexLayoutItem.renderer->marginAfter(writingMode());
    sumFlexBaseSize -= margin;
    sumHypotheticalMainSize -= margin;
}

LayoutUnit RenderFlexibleBox::autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem> flexLayoutItems, LayoutUnit& availableFreeSpace)
{
    // 9.5. (#12) If the remaining free space is positive and at least one main-axis margin on the line is auto,
    // distribute the free space equally among those auto margins; otherwise they resolve to zero.
    if (availableFreeSpace <= 0_lu)
        return 0_lu;

    int numberOfAutoMargins = 0;
    bool isHorizontal = flexLayoutUtils().isHorizontalFlow();
    for (auto& flexLayoutItem : flexLayoutItems) {
        auto& flexItemStyle = flexLayoutItem.style();
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        if (isHorizontal) {
            if (flexItemStyle.marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (flexItemStyle.marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (flexItemStyle.marginTop().isAuto())
                ++numberOfAutoMargins;
            if (flexItemStyle.marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0_lu;

    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0_lu;
    return sizeOfAutoMargin;
}

void RenderFlexibleBox::updateAutoMarginsInMainAxis(RenderBox& flexItem, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0_lu);

    if (flexLayoutUtils().isHorizontalFlow()) {
        if (flexItem.style().marginLeft().isAuto())
            flexItem.setMarginLeft(autoMarginOffset);
        if (flexItem.style().marginRight().isAuto())
            flexItem.setMarginRight(autoMarginOffset);
    } else {
        if (flexItem.style().marginTop().isAuto())
            flexItem.setMarginTop(autoMarginOffset);
        if (flexItem.style().marginBottom().isAuto())
            flexItem.setMarginBottom(autoMarginOffset);
    }
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(FlexLayoutItem& flexLayoutItem, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace)
{
    // 9.6. (#13) Resolve cross-axis auto margins: if both cross-axis margins are auto, split the free space
    // between them; if only one is auto, give it all the free space so the item's outer cross size fills the line.
    auto& flexItem = flexLayoutItem.renderer.get();
    ASSERT(!flexItem.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);

    bool isHorizontal = flexLayoutUtils().isHorizontalFlow();
    auto& topOrLeft = isHorizontal ? flexItem.style().marginTop() : flexItem.style().marginLeft();
    auto& bottomOrRight = isHorizontal ? flexItem.style().marginBottom() : flexItem.style().marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        crossOffset += availableAlignmentSpace / 2;
        if (isHorizontal) {
            flexItem.setMarginTop(availableAlignmentSpace / 2);
            flexItem.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            flexItem.setMarginLeft(availableAlignmentSpace / 2);
            flexItem.setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    bool shouldAdjustTopOrLeft = true;
    if (flexLayoutUtils().isColumnFlow() && flexItem.writingMode().isInlineFlipped()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that the rtl-column flip in computeFlexItemRects
        // will do the right thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!flexLayoutUtils().isColumnFlow() && flexItem.writingMode().isBlockFlipped()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }

    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem.setMarginTop(availableAlignmentSpace);
        else
            flexItem.setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem.setMarginBottom(availableAlignmentSpace);
        else
            flexItem.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::applyStretchAlignmentToFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // 9.4. (#11) A stretched item's used cross size is its flex line's cross size minus the item's cross-axis
    // margins, clamped by the item's used min and max cross sizes. (An item with a definite cross size is not
    // stretched, but still gets the stretch min/max clamp via applyStretchMinMaxCrossSize.)
    auto flexLayoutScope = SetForScope(m_afterCrossAxisItemSizing, true);
    if (flexLayoutItem.mainAxisIsInlineAxis) {
        // Cross axis is block axis (height).
        if (!flexItem.style().logicalHeight().isAuto() && !flexItem.style().logicalHeight().isStretch())
            return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Block);

        auto stretchedLogicalHeight = std::max(flexItem.borderAndPaddingLogicalHeight(),
            lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));
        ASSERT(!flexItem.needsLayout());
        LayoutUnit desiredLogicalHeight = flexItem.constrainLogicalHeightByMinMax(stretchedLogicalHeight, flexItemContentLogicalHeight(flexItem));

        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        bool flexItemNeedsRelayout = desiredLogicalHeight != flexItem.logicalHeight();
        if (!flexItemNeedsRelayout && m_flexItemsWithCompletedLayout.contains(flexItem) && flexItemHasPercentHeightDescendants(flexItem)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            flexItemNeedsRelayout = true;
        }
        if (flexItemNeedsRelayout || !flexItem.overridingBorderBoxLogicalHeight())
            flexItem.setOverridingBorderBoxLogicalHeight(desiredLogicalHeight);
        if (flexItemNeedsRelayout) {
            SetForScope resetFlexItemLogicalHeight(m_shouldResetFlexItemLogicalHeightBeforeLayout, true);
            // We cache the child's content logical height to avoid it being
            // reset to the stretched height.
            // FIXME: This is fragile. RenderBoxes should be smart enough to
            // determine their content logical height correctly even when
            // there's an overrideHeight.
            LayoutUnit contentLogicalHeight = flexItemContentLogicalHeight(flexItem);
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
            dirtyPercentHeightDescendantsWithinFlexItem(flexItem);

            // Don't use layoutChildIfNeeded to avoid setting cross axis cached size twice.
            flexItem.layoutIfNeeded();

            if (canSetFlexItemContentLogicalHeight(flexItem))
                m_contentLogicalHeights.set(flexItem, contentLogicalHeight);
        }
        return desiredLogicalHeight;
    }

    // Cross axis is inline axis (width).
    if (!flexItem.style().logicalWidth().isAuto() && !flexItem.style().logicalWidth().isStretch())
        return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Inline);

    auto flexItemWidth = std::max(0_lu, lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));
    flexItemWidth = flexItem.constrainLogicalWidthByMinMax(flexItemWidth, flexLayoutUtils().crossAxisContentExtent(), *this);

    if (flexItemWidth != flexItem.logicalWidth()) {
        flexItem.setOverridingBorderBoxLogicalWidth(flexItemWidth);
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
        flexItem.layoutIfNeeded();
    }
    return flexItemWidth;
}

LayoutUnit RenderFlexibleBox::applyStretchMinMaxCrossSize(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis crossAxis)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // Clamp an item that has a definite cross size by its used min and max cross sizes, resolving a 'stretch'
    // keyword on either against the flex line's cross size (part of 9.4 #11).
    bool isBlockAxis = crossAxis == LogicalBoxAxis::Block;
    auto& style = flexItem.style();
    auto& min = isBlockAxis ? style.logicalMinHeight() : style.logicalMinWidth();
    auto& max = isBlockAxis ? style.logicalMaxHeight() : style.logicalMaxWidth();
    bool minIsStretch = min.isStretch();
    bool maxIsStretch = !max.isNone() && max.isStretch();
    if (!minIsStretch && !maxIsStretch)
        return flexLayoutUtils().crossAxisExtentForFlexItem(flexItem);

    // The block-axis floor ensures the stretched size never goes below border+padding,
    // matching the behavior in applyStretchAlignmentToFlexItem.
    auto stretchValue = std::max(isBlockAxis ? flexItem.borderAndPaddingLogicalHeight() : 0_lu,
        lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));

    auto computeBlockSize = [&](const auto& size, LayoutUnit fallback) {
        return flexItem.computeLogicalHeightUsing(size, std::nullopt).value_or(fallback);
    };
    auto computeInlineSize = [&](const auto& size) {
        return flexItem.computeLogicalWidthUsing(size, flexLayoutUtils().crossAxisContentExtent(), *this);
    };

    // Compute the specified cross-size, unclamped by stretch min/max.
    // We cannot use the current laid-out size because the initial layout
    // resolves stretch against the container, not the flex line.
    auto specifiedSize = isBlockAxis
        ? computeBlockSize(style.logicalHeight(), flexItem.logicalHeight())
        : computeInlineSize(style.logicalWidth());

    // Resolve each constraint: stretch resolves to the line cross size,
    // non-stretch constraints are computed normally.
    auto effectiveMax = [&] {
        if (maxIsStretch)
            return stretchValue;
        if (max.isNone())
            return LayoutUnit::max();
        return isBlockAxis ? computeBlockSize(max, LayoutUnit::max()) : computeInlineSize(max);
    }();

    // FIXME: The auto minimum does not account for aspect-ratio automatic
    // minimums, which are computed in constrainLogicalHeightByMinMax.
    auto effectiveMin = [&] {
        if (minIsStretch)
            return stretchValue;
        return isBlockAxis ? computeBlockSize(min, 0_lu) : computeInlineSize(min);
    }();

    auto newSize = std::max(std::min(specifiedSize, effectiveMax), effectiveMin);

    auto currentSize = isBlockAxis ? flexItem.logicalHeight() : flexItem.logicalWidth();
    if (newSize != currentSize) {
        if (isBlockAxis)
            flexItem.setOverridingBorderBoxLogicalHeight(newSize);
        else
            flexItem.setOverridingBorderBoxLogicalWidth(newSize);
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
        flexItem.layoutIfNeeded();
    }
    return newSize;
}

void RenderFlexibleBox::setOverridingMainSizeForFlexItem(RenderBox& flexItem, LayoutUnit preferredSize)
{
    if (flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem))
        flexItem.setOverridingBorderBoxLogicalWidth(preferredSize + flexItem.borderAndPaddingLogicalWidth());
    else
        flexItem.setOverridingBorderBoxLogicalHeight(preferredSize + flexItem.borderAndPaddingLogicalHeight());
}

void RenderFlexibleBox::clearFlexItemOverridingSizes()
{
    for (auto* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (!flexItem->isOutOfFlowPositioned())
            flexItem->clearOverridingSize();
    }
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

void RenderFlexibleBox::setFlowAwareLocationForFlexItem(RenderBox& flexItem, const LayoutPoint& location)
{
    if (flexLayoutUtils().isHorizontalFlow())
        flexItem.setLocation(location);
    else
        flexItem.setLocation(location.transposedPoint());
}

void RenderFlexibleBox::setFlexItemGeometry(FlexLayoutItem& flexLayoutItem, const LayoutPoint& location)
{
    setFlowAwareLocationForFlexItem(flexLayoutItem.renderer, location);
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

// This refers to https://drafts.csswg.org/css-flexbox-1/#definite-sizes, section 1).
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

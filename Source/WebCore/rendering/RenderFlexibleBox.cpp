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
    return downcast<RenderFlexibleBox>(*flexItem.parent()).isHorizontalFlow();
}

RenderFlexibleBox::FlexLayoutItem::FlexLayoutItem(RenderBox& flexItem, const FlexBaseAndHypotheticalMainSize& sizing, bool everHadLayout)
    : renderer(flexItem)
    , flexBaseContentSize(sizing.flexBaseContentSize)
    , mainAxisBorderAndPadding(flexContainerIsHorizontalFlow(flexItem) ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent())
    , mainAxisMargin(flexContainerIsHorizontalFlow(flexItem) ? flexItem.horizontalMarginExtent() : flexItem.verticalMarginExtent())
    , minMaxSizes(sizing.minMaxMainSizes)
    , hypotheticalMainContentSize(sizing.hypotheticalMainContentSize)
    , everHadLayout(everHadLayout)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::hypotheticalMainAxisMarginBoxSize() const
{
    return hypotheticalMainContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::flexBaseMarginBoxSize() const
{
    return flexBaseContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::flexedMarginBoxSize() const
{
    return flexedContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

const Style::ComputedStyle& RenderFlexibleBox::FlexLayoutItem::style() const
{
    return renderer->style();
}

LayoutUnit RenderFlexibleBox::FlexLayoutItem::constrainSizeByMinMax(const LayoutUnit size) const
{
    return std::max(minMaxSizes.first, std::min(size, minMaxSizes.second));
}

struct RenderFlexibleBox::LineState {
    LineState(LayoutUnit crossAxisOffset, LayoutUnit crossAxisExtent, std::optional<BaselineAlignmentState> baselineAlignmentState, FlexLayoutItems&& flexLayoutItems)
        : crossAxisOffset(crossAxisOffset)
        , crossAxisExtent(crossAxisExtent)
        , baselineAlignmentState(baselineAlignmentState)
        , flexLayoutItems(WTF::move(flexLayoutItems))
    {
    }
    
    LayoutUnit crossAxisOffset;
    LayoutUnit crossAxisExtent;
    std::optional<BaselineAlignmentState> baselineAlignmentState;
    FlexLayoutItems flexLayoutItems;
};

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

        if (!isColumnFlow()) {
            maxLogicalWidth += maxContentLogicalWidth;
            if (isMultiline()) {
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

    if (!isColumnFlow() && numItemsWithNormalLayout > 1) {
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * computeGap(GapType::BetweenItems);
        maxLogicalWidth += inlineGapSize;
        if (!isMultiline())
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
    if (flexBox.hasDefiniteCrossSizeForFlexItem(flexItem)) {
        auto axis = flexBox.mainAxisIsFlexItemInlineAxis(flexItem) ? OverridingSizesScope::Axis::Block : OverridingSizesScope::Axis::Inline;
        m_overridingScope.emplace(flexItem, axis, flexBox.innerCrossSizeForFlexItem(flexItem));
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

static void updateFlexItemDirtyBitsBeforeLayout(bool relayoutFlexItem, RenderBox& flexItem)
{
    if (flexItem.isOutOfFlowPositioned())
        return;

    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutFlexItem || flexItem.hasRelativeLogicalHeight())
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
}

std::optional<LayoutUnit> RenderFlexibleBox::firstLineBaseline() const
{
    if ((isWritingModeRoot() && !isFlexItem()) || !m_numberOfFlexItemsOnFirstLine || shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForFirstBaseline();
    if (!baselineFlexItem)
        return { };

    auto baseline = std::optional<LayoutUnit> { };
    if (!isColumnFlow() && !mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (isColumnFlow() && mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto firstLineBaseline = baselineFlexItem->firstLineBaseline())
        baseline = firstLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignmentState::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignmentState::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = (settings().subpixelInlineLayoutEnabled() ? LayoutUnit(baselineFlexItem->logicalTop()) : LayoutUnit(baselineFlexItem->logicalTop().toInt())) + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (isHorizontalFlow() ? isScrollContainerY() : isScrollContainerX())
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
    if (!isColumnFlow() && !mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (isColumnFlow() && mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto lastLineBaseline = baselineFlexItem->lastLineBaseline())
        baseline = lastLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignmentState::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignmentState::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = (settings().subpixelInlineLayoutEnabled() ? LayoutUnit(baselineFlexItem->logicalTop()) : LayoutUnit(baselineFlexItem->logicalTop().toInt())) + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (isHorizontalFlow() ? isScrollContainerY() : isScrollContainerX())
        return std::max(0_lu, std::min(result, logicalHeight()));
    return result;
}

static const StyleContentAlignmentData& NODELETE contentAlignmentNormalBehavior()
{
    // The justify-content property applies along the main axis, but since
    // flexing in the main axis is controlled by flex, stretch behaves as
    // flex-start (ignoring the specified fallback alignment, if any).
    // https://drafts.csswg.org/css-align/#distribution-flex
    static const StyleContentAlignmentData normalBehavior = { ContentPosition::Normal, ContentDistribution::Stretch};
    return normalBehavior;
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

void RenderFlexibleBox::clearFlexItemOverridingSizes()
{
    for (auto* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (!flexItem->isOutOfFlowPositioned())
            flexItem->clearOverridingSize();
    }
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

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForFlexItem, bool usePrintRect)
{
    for (RenderBox* flexItem = m_orderIterator.first(); flexItem; flexItem = m_orderIterator.next()) {
        if (!paintChild(*flexItem, paintInfo, paintOffset, paintInfoForFlexItem, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

LayoutUnit RenderFlexibleBox::resolveFlexibleLengthsForLineItems(FlexLayoutItems& lineItems, LayoutUnit containerMainInnerSize, LayoutUnit gapBetweenItems)
{
    double totalFlexGrow = 0;
    double totalFlexShrink = 0;
    double totalWeightedFlexShrink = 0;
    LayoutUnit sumFlexBaseSize;
    LayoutUnit sumHypotheticalMainSize;
    for (auto& flexLayoutItem : lineItems) {
        flexLayoutItem.flexedContentSize = flexLayoutItem.flexBaseContentSize;
        flexLayoutItem.frozen = false;
        totalFlexGrow += flexLayoutItem.style().flexGrow().value;
        totalFlexShrink += flexLayoutItem.style().flexShrink().value;
        totalWeightedFlexShrink += flexLayoutItem.style().flexShrink().value * flexLayoutItem.flexBaseContentSize;
        sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize();
        sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize();
    }
    if (lineItems.size() > 1) {
        auto totalGap = (lineItems.size() - 1) * gapBetweenItems;
        sumFlexBaseSize += totalGap;
        sumHypotheticalMainSize += totalGap;
    }

    auto remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
    auto flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? FlexSign::PositiveFlexibility : FlexSign::NegativeFlexibility;
    freezeInflexibleItems(flexSign, lineItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
    auto initialFreeSpace = remainingFreeSpace;
    while (!resolveFlexibleLengths(flexSign, lineItems, initialFreeSpace, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink)) { }

    return remainingFreeSpace;
}

void RenderFlexibleBox::distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(FlexLineStates& lineStates, LayoutUnit gapBetweenItems)
{
    // In multi-line column flex, the container's main size (height) is only known
    // after all lines are laid out. Lines whose items had flex-grow may not have
    // received enough space because the container height wasn't final during the
    // per-line pass. Re-resolve and relayout those lines now.
    if (!isMultiline() || !isColumnFlow())
        return;

    auto containerMainInnerSize = contentBoxLogicalHeight();
    for (auto& flexLine : lineStates) {
        auto lineMainSize = LayoutUnit { };
        for (auto& flexItem : flexLine.flexLayoutItems)
            lineMainSize += flexItem.flexedMarginBoxSize();
        lineMainSize += (flexLine.flexLayoutItems.size() - 1) * gapBetweenItems;
        if (lineMainSize >= containerMainInnerSize)
            continue;

        resolveFlexibleLengthsForLineItems(flexLine.flexLayoutItems, containerMainInnerSize, gapBetweenItems);

        auto remainingFreeSpace = containerMainInnerSize;
        for (auto& flexItem : flexLine.flexLayoutItems)
            remainingFreeSpace -= flexItem.flexedMarginBoxSize();
        remainingFreeSpace -= (flexLine.flexLayoutItems.size() - 1) * gapBetweenItems;

        auto crossAxisOffset = flexLine.crossAxisOffset;
        layoutAndPlaceFlexItems(crossAxisOffset, flexLine.flexLayoutItems, remainingFreeSpace, RelayoutChildren::No, gapBetweenItems);
    }
}

void RenderFlexibleBox::repositionLogicalHeightDependentFlexItems(FlexLineStates& lineStates, LayoutUnit gapBetweenLines)
{
    LayoutUnit crossAxisStartEdge = lineStates.isEmpty() ? 0_lu : lineStates[0].crossAxisOffset;
    // If we have a single line flexbox, the line height is all the available space. For flex-direction: row,
    // this means we need to use the height, so we do this after calling updateLogicalHeight.
    if (!isMultiline() && !lineStates.isEmpty())
        lineStates[0].crossAxisExtent = crossAxisContentExtent();

    alignFlexLines(lineStates, gapBetweenLines);

    alignFlexItems(lineStates);
    
    if (isWrapReverse())
        flipForWrapReverse(lineStates, crossAxisStartEdge);
    
    // direction:rtl + flex-direction:column means the cross-axis direction is
    // flipped.
    flipForRightToLeftColumn(lineStates);
}

bool RenderFlexibleBox::mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem) const
{
    return isHorizontalFlow() == flexItem.isHorizontalWritingMode();
}

bool RenderFlexibleBox::isColumnFlow() const
{
    return style().isColumnFlexDirection();
}

bool NODELETE RenderFlexibleBox::isColumnOrRowReverse() const
{
    return style().flexDirection() == FlexDirection::ColumnReverse || style().flexDirection() == FlexDirection::RowReverse;
}

bool NODELETE RenderFlexibleBox::isWrapReverse() const
{
    return style().flexWrap() == FlexWrap::Reverse;
}

bool NODELETE RenderFlexibleBox::isHorizontalFlow() const
{
    if (isHorizontalWritingMode())
        return !isColumnFlow();
    return isColumnFlow();
}

bool RenderFlexibleBox::isLeftToRightFlow() const
{
    if (isColumnFlow())
        return writingMode().blockDirection() == FlowDirection::TopToBottom || writingMode().blockDirection() == FlowDirection::LeftToRight;
    return writingMode().isLogicalLeftInlineStart() ^ (style().flexDirection() == FlexDirection::RowReverse);
}

RenderFlexibleBox::Direction NODELETE RenderFlexibleBox::crossAxisDirection() const
{
    auto crossAxisDirection = style().isRowFlexDirection() ? writingMode().blockDirection() : writingMode().inlineDirection();
    switch (crossAxisDirection) {
    case FlowDirection::TopToBottom:
        return isWrapReverse() ? Direction::BottomToTop : Direction::TopToBottom;
    case FlowDirection::BottomToTop:
        return isWrapReverse() ? Direction::TopToBottom : Direction::BottomToTop;
    case FlowDirection::LeftToRight:
        return isWrapReverse() ? Direction::RightToLeft : Direction::LeftToRight;
    case FlowDirection::RightToLeft:
        return isWrapReverse() ? Direction::LeftToRight : Direction::RightToLeft;
    default:
        ASSERT_NOT_REACHED();
        return Direction::TopToBottom;
    }
}

bool RenderFlexibleBox::isMultiline() const
{
    return style().flexWrap() != FlexWrap::NoWrap;
}

// https://drafts.csswg.org/css-flexbox/#min-size-auto
bool RenderFlexibleBox::useContentBasedMinimumSize(const RenderBox& flexItem) const
{
    auto minSize = minMainSizeLengthForFlexItem(flexItem);
    // min, max and fit-content are equivalent to the automatic size for block sizes https://drafts.csswg.org/css-sizing-3/#valdef-width-min-content.
    // Unlike auto, these are explicit author values so the overflow gate does not apply.
    bool flexItemBlockSizeIsEquivalentToAutomaticSize = !mainAxisIsFlexItemInlineAxis(flexItem) && (minSize.isMinContent() || minSize.isMaxContent() || minSize.isFitContent());
    if (flexItemBlockSizeIsEquivalentToAutomaticSize)
        return true;

    auto computedOverflowIsNotScrollable = [this, &flexItem]() {
        auto overflow = mainAxisOverflowForFlexItem(flexItem);
        return overflow == Overflow::Visible || overflow == Overflow::Clip;
    };

    return minSize.isAuto() && computedOverflowIsNotScrollable();
}

bool RenderFlexibleBox::useContentBasedMinimumBlockSize(const RenderBox& flexItem) const
{
    return !mainAxisIsFlexItemInlineAxis(flexItem) && useContentBasedMinimumSize(flexItem);
}

Style::FlexBasis RenderFlexibleBox::flexBasisForFlexItem(const RenderBox& flexItem) const
{
    auto flexBasis = flexItem.style().flexBasis();
    if (flexBasis.isAuto())
        flexBasis = preferredMainSizeLengthForFlexItem(flexItem).asFlexBasis();
    return flexBasis;
}

LayoutUnit RenderFlexibleBox::crossAxisExtentForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.borderBoxHeight() : flexItem.borderBoxWidth();
}

LayoutUnit RenderFlexibleBox::flexItemContentLogicalHeight(const RenderBox& flexItem) const
{
    if (CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(flexItem))
        return renderReplaced->intrinsicLogicalHeight();
    
    if (auto logicalHeight = m_contentLogicalHeights.getOptional(flexItem))
        return *logicalHeight;

    return flexItem.contentBoxLogicalHeight();
}

void RenderFlexibleBox::flexItemWillBeRemoved(const RenderBox& flexItem)
{
    m_contentLogicalHeights.remove(flexItem);
    m_blockAxisSize.remove(flexItem);
}

static bool canSetFlexItemContentLogicalHeight(const RenderBox& flexItem)
{
    return !flexItem.isFloatingOrOutOfFlowPositioned() && !flexItem.shouldComputeLogicalHeightFromAspectRatio() && !is<RenderReplaced>(flexItem);
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

LayoutUnit RenderFlexibleBox::flexItemIntrinsicLogicalHeight(RenderBox& flexItem) const
{
    // This should only be called if the logical height is the cross size
    ASSERT(mainAxisIsFlexItemInlineAxis(flexItem));
    if (needToStretchFlexItemLogicalHeight(flexItem)) {
        LayoutUnit flexItemContentHeight = flexItemContentLogicalHeight(flexItem);
        LayoutUnit flexItemLogicalHeight = flexItemContentHeight + flexItem.scrollbarLogicalHeight() + flexItem.borderAndPaddingLogicalHeight();
        return flexItem.constrainLogicalHeightByMinMax(flexItemLogicalHeight, flexItemContentHeight);
    }
    return flexItem.logicalHeight();
}

LayoutUnit RenderFlexibleBox::flexItemIntrinsicLogicalWidth(RenderBox& flexItem)
{
    // This should only be called if the logical width is the cross size
    ASSERT(!mainAxisIsFlexItemInlineAxis(flexItem));
    if (flexItemCrossSizeIsDefinite(flexItem, flexItem.style().logicalWidth()))
        return flexItem.logicalWidth();

    LogicalExtentComputedValues values;
    {
        OverridingSizesScope cleanOverridingWidthScope(flexItem, OverridingSizesScope::Axis::Inline);
        flexItem.computeLogicalWidth(values);
    }
    return values.extent;
}

LayoutUnit RenderFlexibleBox::crossAxisIntrinsicExtentForFlexItem(RenderBox& flexItem)
{
    return mainAxisIsFlexItemInlineAxis(flexItem) ? flexItemIntrinsicLogicalHeight(flexItem) : flexItemIntrinsicLogicalWidth(flexItem);
}

LayoutUnit RenderFlexibleBox::mainAxisExtentForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.borderBoxSize().width() : flexItem.borderBoxSize().height();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtentForFlexItemIncludingScrollbar(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.contentBoxWidth() + flexItem.verticalScrollbarWidth() : flexItem.contentBoxHeight() + flexItem.horizontalScrollbarHeight();
}

LayoutUnit RenderFlexibleBox::crossAxisExtent() const
{
    return isHorizontalFlow() ? borderBoxSize().height() : borderBoxSize().width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtent() const
{
    return isHorizontalFlow() ? borderBoxSize().width() : borderBoxSize().height();
}

LayoutUnit RenderFlexibleBox::crossAxisContentExtent() const
{
    return isHorizontalFlow() ? contentBoxHeight() : contentBoxWidth();
}

LayoutUnit RenderFlexibleBox::columnInnerMainSize(LayoutUnit hypotheticalMainSize)
{
    ASSERT(isColumnFlow());
    auto borderPaddingAndScrollbar = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
    auto logicalHeight = computeLogicalHeight(hypotheticalMainSize + borderPaddingAndScrollbar, logicalTop()).extent;
    return logicalHeight == LayoutUnit::max() ? logicalHeight : std::max(0_lu, logicalHeight - borderPaddingAndScrollbar);
}

LayoutUnit RenderFlexibleBox::mainAxisAvailableSpace()
{
    if (!isColumnFlow())
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

// FIXME: consider adding this check to RenderBox::hasIntrinsicAspectRatio(). We could even make it
// virtual returning false by default. RenderReplaced will overwrite it with the current implementation
// plus this extra check. See wkb.ug/231955.
static bool isSVGRootWithIntrinsicAspectRatio(const RenderBox& flexItem)
{
    if (!flexItem.isRenderOrLegacyRenderSVGRoot())
        return false;
    // It's common for some replaced elements, such as SVGs, to have intrinsic aspect ratios but no intrinsic sizes.
    // That's why it isn't enough just to check for intrinsic sizes in those cases.
    return flexItem.preferredAspectRatioAsSize().aspectRatioDouble() > 0;
};

static bool flexItemHasAspectRatio(const RenderBox& flexItem)
{
    return flexItem.hasIntrinsicAspectRatio()
        || flexItem.style().aspectRatio().hasRatio()
        || isSVGRootWithIntrinsicAspectRatio(flexItem);
}

bool RenderFlexibleBox::hasStretchedFlexItemWithAspectRatio() const
{
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (flexItem.isOutOfFlowPositioned() || flexItem.isExcludedFromNormalLayout())
            continue;
        if (!flexItemHasAspectRatio(flexItem))
            continue;
        if (alignmentForFlexItem(flexItem) == ItemPosition::Stretch
            && !hasAutoMarginsInCrossAxis(flexItem)
            && preferredCrossSizeLengthForFlexItem(flexItem).isAuto())
            return true;
    }
    return false;
}

template<typename SizeType> std::optional<LayoutUnit> RenderFlexibleBox::computeMainAxisExtentForFlexItem(RenderBox& flexItem, const SizeType& size)
{
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    if (!mainAxisIsFlexItemInlineAxis(flexItem)) {
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
    if (flexItem.style().logicalWidth().isAuto() && !flexItemHasAspectRatio(flexItem)) {
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

    auto mainAxisWidth = isColumnFlow() ? availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) : contentBoxLogicalWidth();
    return flexItem.computeLogicalWidthUsing(size, mainAxisWidth, *this) - flexItem.borderAndPaddingLogicalWidth();
}

FlowDirection RenderFlexibleBox::transformedBlockFlowDirection() const
{
    if (!isColumnFlow())
        return writingMode().blockDirection();
    return writingMode().inlineDirection();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderLeft() : borderRight();
    return isLeftToRightFlow() ? borderTop() : borderBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderRight() : borderLeft();
    return isLeftToRightFlow() ? borderBottom() : borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return borderTop();
    case FlowDirection::BottomToTop:
        return borderBottom();
    case FlowDirection::LeftToRight:
        return borderLeft();
    case FlowDirection::RightToLeft:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return borderBottom();
    case FlowDirection::BottomToTop:
        return borderTop();
    case FlowDirection::LeftToRight:
        return borderRight();
    case FlowDirection::RightToLeft:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingLeft() : paddingRight();
    return isLeftToRightFlow() ? paddingTop() : paddingBottom();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingRight() : paddingLeft();
    return isLeftToRightFlow() ? paddingBottom() : paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return paddingTop();
    case FlowDirection::BottomToTop:
        return paddingBottom();
    case FlowDirection::LeftToRight:
        return paddingLeft();
    case FlowDirection::RightToLeft:
        return paddingRight();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return paddingBottom();
    case FlowDirection::BottomToTop:
        return paddingTop();
    case FlowDirection::LeftToRight:
        return paddingRight();
    case FlowDirection::RightToLeft:
        return paddingLeft();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForFlexItem(const RenderBox& flexItem) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? flexItem.marginLeft() : flexItem.marginRight();
    return isLeftToRightFlow() ? flexItem.marginTop() : flexItem.marginBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginEndForFlexItem(const RenderBox& flexItem) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? flexItem.marginRight() : flexItem.marginLeft();
    return isLeftToRightFlow() ? flexItem.marginBottom() : flexItem.marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForFlexItem(const RenderBox& flexItem) const
{
    switch (transformedBlockFlowDirection()) {
    case FlowDirection::TopToBottom:
        return flexItem.marginTop();
    case FlowDirection::BottomToTop:
        return flexItem.marginBottom();
    case FlowDirection::LeftToRight:
        return flexItem.marginLeft();
    case FlowDirection::RightToLeft:
        return flexItem.marginRight();
    }
    ASSERT_NOT_REACHED();
    return marginTop();
}

void RenderFlexibleBox::initializeMarginTrimState()
{
    // When computeIntrinsicLogicalWidth goes through each of the children, it
    // will include the margins when computing the flexbox's min and max widths.
    // We need to trim the margins of the first and last child early so that
    // these margins do not incorrectly constribute to the box's min/max width
    auto marginTrim = style().marginTrim();
    auto isRowsFlexbox = isHorizontalFlow();
    if (auto flexItem = firstInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineStart))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineStart.add(*flexItem) : m_marginTrimItems.m_itemsOnFirstFlexLine.add(*flexItem);
    if (auto flexItem = lastInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineEnd))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineEnd.add(*flexItem) : m_marginTrimItems.m_itemsOnLastFlexLine.add(*flexItem);
}

bool RenderFlexibleBox::canFitItemWithTrimmedMarginEnd(const FlexLayoutItem& flexLayoutItem, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const
{
    auto marginTrim = style().marginTrim();
    if ((isHorizontalFlow() && marginTrim.contains(Style::MarginTrimSide::InlineEnd)) || (isColumnFlow() && marginTrim.contains(Style::MarginTrimSide::BlockEnd)))
        return sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize() - flowAwareMarginEndForFlexItem(flexLayoutItem.renderer) <= mainAxisAvailableSpace;
    return false;
}

void RenderFlexibleBox::removeMarginEndFromFlexSizes(FlexLayoutItem& flexLayoutItem, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const
{
    LayoutUnit margin;
    if (isHorizontalFlow())
        margin = flexLayoutItem.renderer->marginEnd(writingMode());
    else
        margin = flexLayoutItem.renderer->marginAfter(writingMode());
    sumFlexBaseSize -= margin;
    sumHypotheticalMainSize -= margin;
}

LayoutUnit RenderFlexibleBox::mainAxisMarginExtentForFlexItem(const RenderBox& flexItem) const
{
    if (!flexItem.needsLayout())
        return isHorizontalFlow() ? flexItem.horizontalMarginExtent() : flexItem.verticalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow())
        flexItem.computeInlineDirectionMargins(*this, flexItem.containingBlockLogicalWidthForContent(), flexItem.logicalWidth(), { }, marginStart, marginEnd);
    else
        flexItem.computeBlockDirectionMargins(*this, marginStart, marginEnd);
    return marginStart + marginEnd;
}

LayoutUnit RenderFlexibleBox::crossAxisMarginExtentForFlexItem(const RenderBox& flexItem) const
{
    if (!flexItem.needsLayout())
        return isHorizontalFlow() ? flexItem.verticalMarginExtent() : flexItem.horizontalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow())
        flexItem.computeBlockDirectionMargins(*this, marginStart, marginEnd);
    else
        flexItem.computeInlineDirectionMargins(*this, flexItem.containingBlockLogicalWidthForContent(), flexItem.logicalWidth(), { }, marginStart, marginEnd);
    return marginStart + marginEnd;
}

bool RenderFlexibleBox::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& flexItem) const
{
    ASSERT(style().marginTrim().contains(marginTrimSide));
    auto isMarginParallelWithMainAxis = [this](Style::MarginTrimSide marginTrimSide) {
        if (isHorizontalFlow())
            return marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::BlockEnd;
        return marginTrimSide == Style::MarginTrimSide::InlineStart || marginTrimSide == Style::MarginTrimSide::InlineEnd;
    };
    if (isMarginParallelWithMainAxis(marginTrimSide))
        return (marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::InlineStart) ? m_marginTrimItems.m_itemsOnFirstFlexLine.contains(flexItem) : m_marginTrimItems.m_itemsOnLastFlexLine.contains(flexItem);
    return (marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::InlineStart) ? m_marginTrimItems.m_itemsAtFlexLineStart.contains(flexItem) : m_marginTrimItems.m_itemsAtFlexLineEnd.contains(flexItem);
}

bool RenderFlexibleBox::shouldTrimMainAxisMarginStart() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(Style::MarginTrimSide::InlineStart);
    return style().marginTrim().contains(Style::MarginTrimSide::BlockStart);
}

bool RenderFlexibleBox::shouldTrimMainAxisMarginEnd() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(Style::MarginTrimSide::InlineEnd);
    return style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
}

bool RenderFlexibleBox::shouldTrimCrossAxisMarginStart() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(Style::MarginTrimSide::BlockStart);
    return style().marginTrim().contains(Style::MarginTrimSide::InlineStart);
}

bool RenderFlexibleBox::shouldTrimCrossAxisMarginEnd() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
    return style().marginTrim().contains(Style::MarginTrimSide::InlineEnd);
}

void RenderFlexibleBox::trimMainAxisMarginStart(const FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = isHorizontalFlow();
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? flexLayoutItem.renderer->marginStart(writingMode())
        : flexLayoutItem.renderer->marginBefore(writingMode());
    if (horizontalFlow)
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    m_marginTrimItems.m_itemsAtFlexLineStart.add(flexLayoutItem.renderer);
}

void RenderFlexibleBox::trimMainAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = isHorizontalFlow();
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
    if (isHorizontalFlow())
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    m_marginTrimItems.m_itemsOnFirstFlexLine.add(flexLayoutItem.renderer);
}

void RenderFlexibleBox::trimCrossAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    if (isHorizontalFlow())
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockEnd);
    else
        setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineEnd);
    m_marginTrimItems.m_itemsOnLastFlexLine.add(flexLayoutItem.renderer);
}

LayoutUnit RenderFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutUnit RenderFlexibleBox::mainAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();
}

LayoutPoint RenderFlexibleBox::flowAwareLocationForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.location() : flexItem.location().transposedPoint();
}

const Style::PreferredSize& RenderFlexibleBox::preferredCrossSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().height() : flexItem.style().width();
}

const Style::MinimumSize& RenderFlexibleBox::minCrossSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().minHeight() : flexItem.style().minWidth();
}

const Style::MaximumSize& RenderFlexibleBox::maxCrossSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().maxHeight() : flexItem.style().maxWidth();
}

const Style::PreferredSize& RenderFlexibleBox::preferredMainSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().width() : flexItem.style().height();
}

const Style::MinimumSize& RenderFlexibleBox::minMainSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().minWidth() : flexItem.style().minHeight();
}

const Style::MaximumSize& RenderFlexibleBox::maxMainSizeLengthForFlexItem(const RenderBox& flexItem) const
{
    return isHorizontalFlow() ? flexItem.style().maxWidth() : flexItem.style().maxHeight();
}

double RenderFlexibleBox::preferredAspectRatioForFlexItem(const RenderBox& flexItem) const
{
    auto flexItemAspectRatio = [&] {
        auto flexItemIntrinsicSize = LayoutSize { flexItem.intrinsicLogicalWidth(), flexItem.intrinsicLogicalHeight() };
        if (flexItem.isRenderOrLegacyRenderSVGRoot())
            return flexItem.preferredAspectRatioAsSize().aspectRatioDouble();
        if (flexItem.style().aspectRatio().isRatio() || (flexItem.style().aspectRatio().isAutoAndRatio() && flexItemIntrinsicSize.isEmpty()))
            return flexItem.style().logicalAspectRatio();
        if (is<RenderReplaced>(flexItem))
            return flexItem.preferredAspectRatioAsSize().aspectRatioDouble();

        ASSERT(flexItem.intrinsicLogicalHeight());
        return flexItem.intrinsicLogicalWidth().toDouble() / flexItem.intrinsicLogicalHeight().toDouble();
    };

    if (mainAxisIsFlexItemInlineAxis(flexItem))
        return flexItemAspectRatio();
    return 1 / flexItemAspectRatio();
}

// FIXME: computeMainSizeFromAspectRatioUsing may need to return an std::optional<LayoutUnit> in the future
// rather than returning indefinite sizes as 0/-1.
template<typename SizeType> LayoutUnit RenderFlexibleBox::computeMainSizeFromAspectRatioUsing(const RenderBox& flexItem, const SizeType& crossSizeLength) const
{
    ASSERT(flexItemHasAspectRatio(flexItem));
    auto flexItemCrossAxisBorderAndPadding = isHorizontalFlow() ? flexItem.verticalBorderAndPaddingExtent() : flexItem.horizontalBorderAndPaddingExtent();

    // All paths return border-box cross size.
    auto crossSizeOptional = WTF::switchOn(crossSizeLength,
        [&](const SizeType::Fixed& fixedCrossSizeLength) -> std::optional<LayoutUnit> {
            auto value = LayoutUnit { fixedCrossSizeLength.resolveZoom(flexItem.style().usedZoomForLength()) };
            if (flexItem.style().boxSizing() == BoxSizing::ContentBox)
                value += flexItemCrossAxisBorderAndPadding;
            return value;
        },
        [&](const SizeType::Percentage& percentageCrossSizeLength) -> std::optional<LayoutUnit> {
            return mainAxisIsFlexItemInlineAxis(flexItem)
                ? flexItem.computePercentageLogicalHeight(SizeType { percentageCrossSizeLength })
                : adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(percentageCrossSizeLength, contentBoxWidth()));
        },
        [&](const SizeType::Calc& calcCrossSizeLength) -> std::optional<LayoutUnit> {
            return mainAxisIsFlexItemInlineAxis(flexItem)
                ? flexItem.computePercentageLogicalHeight(calcCrossSizeLength)
                : adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(calcCrossSizeLength, contentBoxWidth(), flexItem.style().usedZoomForLength()));
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            ASSERT(hasDefiniteCrossSizeForFlexItem(flexItem));
            return innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> {
            // Resolve stretch against the flex container's cross-axis definite size.
            return innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return innerCrossSizeForFlexItem(flexItem);
        },
        [&](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_REACHED();
            return { };
        }
    );
    if (!crossSizeOptional)
        return 0_lu;

    auto crossSize = *crossSizeOptional;
    auto preferredAspectRatio = preferredAspectRatioForFlexItem(flexItem);

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
    auto flexItemMainAxisBorderAndPadding = isHorizontalFlow() ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio } - flexItemMainAxisBorderAndPadding);
}

void RenderFlexibleBox::setFlowAwareLocationForFlexItem(RenderBox& flexItem, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        flexItem.setLocation(location);
    else
        flexItem.setLocation(location.transposedPoint());
}

template<typename SizeType> bool RenderFlexibleBox::canComputePercentageFlexBasis(const RenderBox& flexItem, const SizeType& flexBasis, UpdatePercentageHeightDescendants updateDescendants)
{
    if (!isColumnFlow() || m_hasDefiniteHeight == SizeDefiniteness::Definite)
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
    if (!mainAxisIsFlexItemInlineAxis(flexItem) && (size.isIntrinsic() || size.isIntrinsicKeyword()))
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

bool RenderFlexibleBox::flexItemHasComputableAspectRatio(const RenderBox& flexItem) const
{
    if (!flexItemHasAspectRatio(flexItem))
        return false;
    return flexItem.preferredAspectRatioAsSize().aspectRatioDouble() > 0;
}

bool RenderFlexibleBox::flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const RenderBox& flexItem)
{
    return flexItemHasComputableAspectRatio(flexItem)
        && (flexItemCrossSizeIsDefinite(flexItem, preferredCrossSizeLengthForFlexItem(flexItem)) || hasDefiniteCrossSizeForFlexItem(flexItem));
}

static bool canResolveFullyConstrainedLogicalHeight(const RenderFlexibleBox& flexBox)
{
    // The height is fully constrained by insets (CSS Sizing 3 section 3.2.1), but we can
    // only resolve it if the containing block's height is itself definite right now.
    // In nested flex layouts, the containing block may be a flex item that hasn't been
    // stretched yet - its height is 0 at that point and computeLogicalHeight would give
    // a wrong answer. This happens when the nested out-of-flow flex is laid out as part of the
    // anector flex's main-axis sizing, before the stretch phase sets the final cross size.
    return flexBox.hasFullyConstrainedLogicalHeight() && flexBox.containingBlock()->hasDefiniteLogicalHeight();
}

bool RenderFlexibleBox::hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem) const
{
    // 9.8 https://drafts.csswg.org/css-flexbox/#definite-sizes
    // 1. If a single-line flex container has a definite cross size, the automatic preferred outer cross size of any
    // stretched flex items is the flex container's inner cross size (clamped to the flex item's min and max cross size)
    // and is considered definite.
    if (!isMultiline() && alignmentForFlexItem(flexItem) == ItemPosition::Stretch && !hasAutoMarginsInCrossAxis(flexItem) && preferredCrossSizeLengthForFlexItem(flexItem).isAuto()) {
        if (isColumnFlow())
            return true;
        // This must be kept in sync with computeMainSizeFromAspectRatioUsing().
        auto& crossSize = isHorizontalFlow() ? style().height() : style().width();
        if (crossSize.isFixed())
            return true;
        if (crossSize.isPercent() && availableLogicalHeightForPercentageComputation())
            return true;
        if (canResolveFullyConstrainedLogicalHeight(*this))
            return true;
        if (hasDefiniteLogicalWidthForAspectRatioCrossSize())
            return true;
    }
    return false;
}

bool RenderFlexibleBox::hasDefiniteLogicalWidthForAspectRatioCrossSize() const
{
    // A style-fixed logical width combined with aspect-ratio yields a definite
    // cross size derivable from style alone, with no layout-state dependency.
    // CSS Sizing 4 section 5.1.4: an aspect-ratio transferred size is definite
    // when its input axis is definite, and CSS Sizing 3 section 5.1 makes a
    // style-fixed width definite. The cross axis must be auto so the ratio is
    // actually consulted; min-content/max-content/fit-content ignore the ratio
    // per CSS Sizing 4 section 5.4.
    auto& crossSize = isHorizontalFlow() ? style().height() : style().width();
    return crossSize.isAuto() && style().logicalWidth().isFixed() && style().aspectRatio().hasRatio();
}

template<typename SizeType> bool RenderFlexibleBox::flexItemCrossSizeIsDefinite(const RenderBox& flexItem, const SizeType& size)
{
    if constexpr (!std::same_as<SizeType, Style::MaximumSize>) {
        if (size.isAuto())
            return false;
    }

    // Stretch is definite in the same cases as percentages, i.e. when the
    // container's cross size is definite. We use a dummy percentage for stretch
    // since computePercentageLogicalHeight evaluates the value as a percentage.
    auto crossSizeIsDefinite = [&](const auto& sizeForPercentageComputation) {
        if (!mainAxisIsFlexItemInlineAxis(flexItem) || m_hasDefiniteHeight == SizeDefiniteness::Definite)
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

void RenderFlexibleBox::invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem)
{
    m_blockAxisSize.remove(flexItem);
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
LayoutUnit RenderFlexibleBox::flexBaseSizeForFlexItem(RenderBox& flexItem)
{
    auto flexBasis = flexBasisForFlexItem(flexItem);
    ScopedFlexBasisAsFlexItemMainSize scoped(flexItem, flexBasis.tryPreferredSize().value_or(Style::PreferredSize { CSS::Keyword::MaxContent { } }), mainAxisIsFlexItemInlineAxis(flexItem));
    // FIXME: While we are supposed to ignore min/max here, the cached
    // m_blockAxisSize entry may hold a min/max-constrained size.
    SetForScope<bool> computingBaseSizesScope(m_isComputingFlexBaseSizes, true);
    ensureBlockAxisContentSizeForFlexItemIfNeeded(flexItem);

    // 9.2.3 A.
    if (flexItemMainSizeIsDefinite(flexItem, flexBasis))
        return std::max(0_lu, computeMainAxisExtentForFlexItem(flexItem, flexBasis).value());

    // 9.2.3 B.
    if (flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexItem)) {
        auto& crossSizeLength = preferredCrossSizeLengthForFlexItem(flexItem);
        return adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexItem, computeMainSizeFromAspectRatioUsing(flexItem, crossSizeLength));
    }

    // FIXME: 9.2.3 C.
    // FIXME: 9.2.3 D.

    // 9.2.3 E.
    if (!mainAxisIsFlexItemInlineAxis(flexItem)) {
        ASSERT(!flexItem.needsLayout());
        ASSERT(m_blockAxisSize.contains(flexItem));
        return m_blockAxisSize.getOptional(flexItem).value_or(0_lu);
    }

    // We don't need to add scrollbarLogicalWidth here because the preferred
    // width includes the scrollbar, even for overflow: auto.
    ScopedCrossAxisOverrideForFlexItem crossSizeScope(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
    auto mainAxisExtent = flexItem.maxContentLogicalWidthContribution();
    auto mainAxisBorderAndPadding = isHorizontalFlow() ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return mainAxisExtent - mainAxisBorderAndPadding;
}

void RenderFlexibleBox::performFlexLayout(RelayoutChildren relayoutChildren)
{
    if (layoutUsingFlexFormattingContext())
        return;

    // Set up our master list of flex items. All of the rest of the algorithm
    // should work off this list of a subset.
    // FIXME: That second part is not yet true.
    FlexLayoutItems allItems;
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
        allItems.append({ *flexItem, flexBaseAndHypotheticalMainSize(*flexItem), everHadLayout });
        // flexBaseAndHypotheticalMainSize might set the override containing block height so any value cached for definiteness might be incorrect.
        resetHasDefiniteHeight();
    }

    if (allItems.isEmpty()) {
        if (hasLineIfEmpty()) {
            auto minHeight = borderAndPaddingLogicalHeight() + lineHeight() + scrollbarLogicalHeight();
            if (borderBoxHeight() < minHeight)
                setLogicalHeight(minHeight);
        }
        updateLogicalHeight();
        return;
    }

    FlexLineStates lineStates;
    auto mainAxisAvailableSpace = this->mainAxisAvailableSpace();
    LayoutUnit gapBetweenItems = computeGap(GapType::BetweenItems);
    LayoutUnit gapBetweenLines = computeGap(GapType::BetweenLines);
    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    size_t nextIndex = 0;
    size_t numLines = 0;
    InspectorInstrumentation::flexibleBoxRendererBeganLayout(*this);
    while (auto lineData = computeNextFlexLine(nextIndex, allItems, mainAxisAvailableSpace, gapBetweenItems)) {
        ++numLines;
        InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(*this, nextIndex);

        auto& lineItems = lineData->lineItems;

        // Cross axis margins should only be trimmed if they are on the first/last flex line
        auto shouldTrimCrossAxisStart = shouldTrimCrossAxisMarginStart() && !lineStates.size();
        auto shouldTrimCrossAxisEnd = shouldTrimCrossAxisMarginEnd() && allItems.last().renderer.ptr() == lineItems.last().renderer.ptr();
        if (shouldTrimCrossAxisStart || shouldTrimCrossAxisEnd) {
            for (auto& flexLayoutItem : lineItems) {
                if (shouldTrimCrossAxisStart)
                    trimCrossAxisMarginStart(flexLayoutItem);
                if (shouldTrimCrossAxisEnd)
                    trimCrossAxisMarginEnd(flexLayoutItem);
            }
        }
        auto containerMainInnerSize = isColumnFlow() ? columnInnerMainSize(lineData->sumHypotheticalMainSize) : contentBoxLogicalWidth();
        auto remainingFreeSpace = resolveFlexibleLengthsForLineItems(lineItems, containerMainInnerSize, gapBetweenItems);

        // Recalculate the remaining free space. The adjustment for flex factors
        // between 0..1 means we can't just use remainingFreeSpace here.
        remainingFreeSpace = containerMainInnerSize;
        for (size_t i = 0; i < lineItems.size(); ++i) {
            auto& flexLayoutItem = lineItems[i];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            remainingFreeSpace -= flexLayoutItem.flexedMarginBoxSize();
        }
        remainingFreeSpace -= (lineItems.size() - 1) * gapBetweenItems;

        auto lineResult = layoutAndPlaceFlexItems(crossAxisOffset, lineItems, remainingFreeSpace, relayoutChildren, gapBetweenItems);
        lineStates.append(LineState(crossAxisOffset, lineResult.crossAxisExtent, lineResult.baselineAlignmentState, WTF::move(lineItems)));
        crossAxisOffset = lineResult.crossAxisOffsetForNextLine;
    }

    if (!lineStates.isEmpty()) {
        auto isWrapReverse = this->isWrapReverse();
        auto firstLineItemsCountInOriginalOrder = lineStates.first().flexLayoutItems.size();
        auto lastLineItemsCountInOriginalOrder = lineStates.last().flexLayoutItems.size();

        m_numberOfFlexItemsOnFirstLine = !isWrapReverse ? firstLineItemsCountInOriginalOrder : lastLineItemsCountInOriginalOrder;
        m_numberOfFlexItemsOnLastLine = !isWrapReverse ? lastLineItemsCountInOriginalOrder : firstLineItemsCountInOriginalOrder;
    }

    if (hasLineIfEmpty()) {
        // Even if computeNextFlexLine returns true, the flexbox might not have
        // a line because all our children might be out of flow positioned.
        // Instead of just checking if we have a line, make sure the flexbox
        // has at least a line's worth of height to cover this case.
        LayoutUnit minHeight = borderAndPaddingLogicalHeight() + lineHeight() + scrollbarLogicalHeight();
        if (borderBoxSize().height() < minHeight)
            setLogicalHeight(minHeight);
    }

    if (!isColumnFlow() && numLines > 1)
        setLogicalHeight(logicalHeight() + computeGap(GapType::BetweenLines) * (numLines - 1));

    updateLogicalHeight();
    distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(lineStates, gapBetweenItems);
    repositionLogicalHeightDependentFlexItems(lineStates, gapBetweenLines);
}

std::optional<RenderFlexibleBox::FlexingLineData> RenderFlexibleBox::computeNextFlexLine(size_t& nextIndex, const FlexLayoutItems& allItems, LayoutUnit mainAxisAvailableSpace, LayoutUnit gapBetweenItems)
{
    if (nextIndex >= allItems.size())
        return { };

    FlexingLineData lineData;
    // Trim main axis margin for item at the start of the flex line
    if (nextIndex < allItems.size() && shouldTrimMainAxisMarginStart())
        trimMainAxisMarginStart(allItems[nextIndex]);
    for (; nextIndex < allItems.size(); ++nextIndex) {
        const auto& flexLayoutItem = allItems[nextIndex];
        auto& style = flexLayoutItem.style();
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        if (isMultiline() && (lineData.sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize() > mainAxisAvailableSpace && !canFitItemWithTrimmedMarginEnd(flexLayoutItem, lineData.sumHypotheticalMainSize, mainAxisAvailableSpace)) && !lineData.lineItems.isEmpty())
            break;
        lineData.lineItems.append(flexLayoutItem);
        lineData.sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize() + gapBetweenItems;
        lineData.totalFlexGrow += style.flexGrow().value;
        lineData.totalFlexShrink += style.flexShrink().value;
        lineData.totalWeightedFlexShrink += style.flexShrink().value * flexLayoutItem.flexBaseContentSize;
        lineData.sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize() + gapBetweenItems;
    }

    if (!lineData.lineItems.isEmpty()) {
        // We added a gap after every item but there shouldn't be one after the last item, so subtract it here. Note that
        // sums might be negative here due to negative margins in flex items.
        lineData.sumHypotheticalMainSize -= gapBetweenItems;
        lineData.sumFlexBaseSize -= gapBetweenItems;
    }

    ASSERT(lineData.lineItems.size() > 0 || nextIndex == allItems.size());
    // Trim main axis margin for item at the end of the flex line
    if (lineData.lineItems.size() && shouldTrimMainAxisMarginEnd()) {
        auto& lastItem = lineData.lineItems.last();
        removeMarginEndFromFlexSizes(lastItem, lineData.sumFlexBaseSize, lineData.sumHypotheticalMainSize);
        trimMainAxisMarginEnd(lastItem);
    }
    return lineData;
}

LayoutUnit RenderFlexibleBox::autoMarginOffsetInMainAxis(const FlexLayoutItems& flexLayoutItems, LayoutUnit& availableFreeSpace)
{
    if (availableFreeSpace <= 0_lu)
        return 0_lu;
    
    int numberOfAutoMargins = 0;
    bool isHorizontal = isHorizontalFlow();
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
    
    if (isHorizontalFlow()) {
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

bool RenderFlexibleBox::hasAutoMarginsInCrossAxis(const RenderBox& flexItem) const
{
    if (isHorizontalFlow())
        return flexItem.style().marginTop().isAuto() || flexItem.style().marginBottom().isAuto();
    return flexItem.style().marginLeft().isAuto() || flexItem.style().marginRight().isAuto();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForFlexItem(LayoutUnit lineCrossAxisExtent, const RenderBox& flexItem)
{
    LayoutUnit flexItemCrossExtent = crossAxisMarginExtentForFlexItem(flexItem) + crossAxisExtentForFlexItem(flexItem);
    return lineCrossAxisExtent - flexItemCrossExtent;
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(RenderBox& flexItem, LayoutUnit availableAlignmentSpace)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);
    
    bool isHorizontal = isHorizontalFlow();
    auto& topOrLeft = isHorizontal ? flexItem.style().marginTop() : flexItem.style().marginLeft();
    auto& bottomOrRight = isHorizontal ? flexItem.style().marginBottom() : flexItem.style().marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        adjustAlignmentForFlexItem(flexItem, availableAlignmentSpace / 2);
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
    if (isColumnFlow() && flexItem.writingMode().isInlineFlipped()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that flipForRightToLeftColumn will do the right
        // thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!isColumnFlow() && flexItem.writingMode().isBlockFlipped()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }
    
    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            adjustAlignmentForFlexItem(flexItem, availableAlignmentSpace);
        
        if (isHorizontal)
            flexItem.setMarginTop(availableAlignmentSpace);
        else
            flexItem.setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            adjustAlignmentForFlexItem(flexItem, availableAlignmentSpace);
        
        if (isHorizontal)
            flexItem.setMarginBottom(availableAlignmentSpace);
        else
            flexItem.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForFlexItem(const RenderBox& flexItem)
{
    auto isHorizontalFlow = this->isHorizontalFlow();
    auto direction = isHorizontalFlow ? LineDirection::Horizontal : LineDirection::Vertical;

    if (!mainAxisIsFlexItemInlineAxis(flexItem)) {
        auto flexboxWritingMode = style().writingMode();
        auto alignmentContextAxis = style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        auto writingModeForSynthesis = BaselineAlignmentState::usedWritingModeForBaselineAlignment(alignmentContextAxis, flexboxWritingMode, flexItem.writingMode());
        return BaselineAlignmentState::synthesizedBaseline(flexItem, BaselineAlignmentState::dominantBaseline(flexboxWritingMode),
            writingModeForSynthesis, direction, BaselineSynthesisEdge::BorderBox) + flowAwareMarginBeforeForFlexItem(flexItem);
    }
    auto ascent = alignmentForFlexItem(flexItem) == ItemPosition::LastBaseline ? flexItem.lastLineBaseline() : flexItem.firstLineBaseline();
    if (!ascent) {
        auto flexboxWritingMode = style().writingMode();
        return BaselineAlignmentState::synthesizedBaseline(flexItem, BaselineAlignmentState::dominantBaseline(flexboxWritingMode),
            flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox) + flowAwareMarginBeforeForFlexItem(flexItem);
    }

    if (!flexItem.writingMode().isBlockMatchingAny(writingMode())) {
        // Baseline from flex item with opposite block direction needs to be resolved as if flex item had the same block direction.
        //  _____________________________ <- flex box top/left (e.g. writing-mode: vertical-rl)
        // |        __________________   |
        // |       |  20px |    80px  |<-- flex item with vertical-lr (top is at visual left)
        // |       |<----->|<-------->|  |
        // |       top     baseline   |  |
        // where computed baseline is 20px and resolved (as if flex item shares the block direction with flex box) is 80px.
        ascent = flexItem.logicalHeight() - *ascent;
    }

    if (isHorizontalFlow ? flexItem.isScrollContainerY() : flexItem.isScrollContainerX())
        return std::max(0_lu, std::min(*ascent, crossAxisExtentForFlexItem(flexItem))) + flowAwareMarginBeforeForFlexItem(flexItem);
    return *ascent + flowAwareMarginBeforeForFlexItem(flexItem);;
}

LayoutUnit RenderFlexibleBox::computeFlexItemMarginValue(const Style::MarginEdge& margin)
{
    // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
    // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
    return Style::evaluateMinimum<LayoutUnit>(margin, contentBoxLogicalWidth(), style().usedZoomForLength());
}

void RenderFlexibleBox::prepareOrderIteratorAndMargins()
{
    OrderIteratorPopulator populator(m_orderIterator);

    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (!populator.collectChild(flexItem))
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (isHorizontalFlow()) {
            flexItem.setMarginLeft(computeFlexItemMarginValue(flexItem.style().marginLeft()));
            flexItem.setMarginRight(computeFlexItemMarginValue(flexItem.style().marginRight()));
        } else {
            flexItem.setMarginTop(computeFlexItemMarginValue(flexItem.style().marginTop()));
            flexItem.setMarginBottom(computeFlexItemMarginValue(flexItem.style().marginBottom()));
        }
    }
}

std::optional<LayoutUnit> RenderFlexibleBox::computeUsedMaxMainSize(RenderBox& flexItem)
{
    auto max = maxMainSizeLengthForFlexItem(flexItem);
    if (max.isSpecified())
        return computeMainAxisExtentForFlexItem(flexItem, max);
    if (max.isIntrinsicOrStretch()) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        return computeMainAxisExtentForFlexItem(flexItem, max);
    }
    return { };
}

LayoutUnit RenderFlexibleBox::computeUsedNonAutoMinMainSize(RenderBox& flexItem, const Style::MinimumSize& min)
{
    // https://drafts.csswg.org/css-flexbox/#main-size-property
    // Resolves the used min main size for every case except min:auto. Three values
    // route here: a specified length/percentage, the stretch keyword, and intrinsic
    // keywords (min-content/max-content/fit-content) on the inline axis. Per CSS
    // Sizing 3 § 5.2, intrinsic keywords on the block axis behave like auto, so
    // those go through computeContentBasedMinMainSize instead.
    auto minExtent = [&] {
        if (min.isIntrinsicOrStretch()) {
            ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
            return computeMainAxisExtentForFlexItem(flexItem, min).value_or(0_lu);
        }
        return computeMainAxisExtentForFlexItem(flexItem, min).value_or(0_lu);
    }();

    // We must never return a min size smaller than the min preferred size for tables.
    if (flexItem.isRenderTable() && mainAxisIsFlexItemInlineAxis(flexItem)) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
        minExtent = std::max(minExtent, flexItem.minContentLogicalWidthContribution());
    }
    return minExtent;
}

LayoutUnit RenderFlexibleBox::computeContentBasedMinMainSize(RenderBox& flexItem, std::optional<LayoutUnit> maxExtent)
{
    // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForFlexItem and
    // ensure it's valid through the virtual calls of computeSizingKeywordLogicalContentHeightUsing.
    LayoutUnit contentSize;
    auto& flexItemCrossSizeLength = preferredCrossSizeLengthForFlexItem(flexItem);

    bool canComputeSizeThroughAspectRatio = flexItemHasComputableAspectRatio(flexItem) && flexItemCrossSizeIsDefinite(flexItem, flexItemCrossSizeLength);
    if (canComputeSizeThroughAspectRatio)
        contentSize = computeMainSizeFromAspectRatioUsing(flexItem, flexItemCrossSizeLength);

    if (!canComputeSizeThroughAspectRatio || !flexItem.isRenderReplaced()) {
        ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(*this, flexItem, ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        auto minContentSize = computeMainAxisExtentForFlexItem(flexItem, Style::MinimumSize { CSS::Keyword::MinContent { } }).value_or(0_lu);
        contentSize = std::max(contentSize, minContentSize);
    }

    if (flexItemHasAspectRatio(flexItem))
        contentSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexItem, contentSize);

    contentSize = std::max(0_lu, contentSize);
    ASSERT(contentSize >= 0);
    contentSize = std::min(contentSize, maxExtent.value_or(contentSize));

    auto mainSize = preferredMainSizeLengthForFlexItem(flexItem);
    if (flexItemMainSizeIsDefinite(flexItem, mainSize)) {
        auto resolvedMainSize = computeMainAxisExtentForFlexItem(flexItem, mainSize).value_or(0);
        ASSERT(resolvedMainSize >= 0);
        auto specifiedSize = std::min(resolvedMainSize, maxExtent.value_or(resolvedMainSize));
        return std::min(specifiedSize, contentSize);
    }

    if (flexItem.isRenderReplaced() && flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexItem)) {
        auto transferredSize = computeMainSizeFromAspectRatioUsing(flexItem, flexItemCrossSizeLength);
        transferredSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexItem, transferredSize);
        return std::min(transferredSize, contentSize);
    }

    return contentSize;
}

std::pair<LayoutUnit, LayoutUnit> RenderFlexibleBox::computeFlexItemMinMaxMainSizes(RenderBox& flexItem)
{
    auto maxExtent = computeUsedMaxMainSize(flexItem);
    auto resolvedMax = maxExtent.value_or(LayoutUnit::max());

    // useContentBasedMinimumSize covers both auto-equivalent cases: min:auto with
    // non-scrollable overflow (§ 4.5) and block-axis intrinsic keywords (CSS Sizing
    // 3 § 5.2 makes those behave like auto, regardless of overflow).
    if (useContentBasedMinimumSize(flexItem))
        return { computeContentBasedMinMainSize(flexItem, maxExtent), resolvedMax };

    auto min = minMainSizeLengthForFlexItem(flexItem);
    if (!min.isAuto())
        return { computeUsedNonAutoMinMainSize(flexItem, min), resolvedMax };

    // min:auto on a scroll container — spec says the automatic minimum size is zero.
    return { 0_lu, resolvedMax };
}

bool RenderFlexibleBox::canUseFlexItemForPercentageResolution(const RenderBox& flexItem)
{
    ASSERT(flexItem.isFlexItem());

    auto canUseByLayoutPhase = [&] {
        if (m_inFlexItemIntrinsicWidthComputation)
            return hasDefiniteCrossSizeForFlexItem(flexItem);

        if (m_afterMainAxisItemSizing) {
            // Final sizes for flex items are available only along the main axis.
            // Percentages can be resolved only against those items when they are orthogonal to the flex container (i.e., their logical height is computed and final)
            return !mainAxisIsFlexItemInlineAxis(flexItem);
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
            return !mainAxisIsFlexItemInlineAxis(flexItem);

        // Outside of layout (i.e. when using relative percentage positioning), base the decision on style.
        return !m_inLayout;
    };
    if (!canUseByLayoutPhase())
        return false;

    auto canUseByStyle = [&] {
        if (mainAxisIsFlexItemInlineAxis(flexItem))
            return alignmentForFlexItem(flexItem) == ItemPosition::Stretch;

        // Flexbox 9.8 rule 2: definite flex-basis makes post-flexing main size definite.
        if (flexItemMainSizeIsDefinite(flexItem, flexBasisForFlexItem(flexItem)))
            return true;

        // Flexbox 9.8 rule 1: definite container main size makes post-flexing sizes definite.
        return canComputePercentageFlexBasis(flexItem, Style::PreferredSize { 0_css_percentage }, UpdatePercentageHeightDescendants::Yes);
    };
    return canUseByStyle();
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

LayoutUnit RenderFlexibleBox::adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& flexItem, LayoutUnit flexItemSize)
{
    auto& crossMin = minCrossSizeLengthForFlexItem(flexItem);
    auto& crossMax = maxCrossSizeLengthForFlexItem(flexItem);

    if (flexItemCrossSizeIsDefinite(flexItem, crossMax)) {
        LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(flexItem, crossMax);
        flexItemSize = std::min(maxValue, flexItemSize);
    }

    if (flexItemCrossSizeIsDefinite(flexItem, crossMin)) {
        LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(flexItem, crossMin);
        flexItemSize = std::max(minValue, flexItemSize);
    }
    
    return flexItemSize;
}

void RenderFlexibleBox::ensureBlockAxisContentSizeForFlexItemIfNeeded(RenderBox& flexItem)
{
    if (!flexBaseSizeNeedsBlockAxisContentSize(flexItem))
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
        auto flexBasis = flexBasisForFlexItem(flexItem);
        if (flexBasis.isPercentOrCalculated() && !flexItemMainSizeIsDefinite(flexItem, flexBasis))
            return flexItemContentLogicalHeight(flexItem) + flexItem.scrollbarLogicalHeight();
        return flexItem.logicalHeight() - flexItem.borderAndPaddingLogicalHeight();
    }();

    m_blockAxisSize.set(flexItem, innerSize);
    m_flexItemsWithCompletedLayout.add(flexItem);
}

RenderFlexibleBox::FlexBaseAndHypotheticalMainSize RenderFlexibleBox::flexBaseAndHypotheticalMainSize(RenderBox& flexItem)
{
    auto flexBaseContentSize = flexBaseSizeForFlexItem(flexItem);
    auto minMaxMainSizes = computeFlexItemMinMaxMainSizes(flexItem);
    return { flexBaseContentSize, std::max(minMaxMainSizes.first, std::min(flexBaseContentSize, minMaxMainSizes.second)), minMaxMainSizes };
}
    
void RenderFlexibleBox::freezeViolations(Vector<FlexLayoutItem*, 4>& violations, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        ASSERT(!violations[i]->frozen);
        auto& flexItemStyle = violations[i]->style();
        LayoutUnit flexItemSize = violations[i]->flexedContentSize;
        availableFreeSpace -= flexItemSize - violations[i]->flexBaseContentSize;
        totalFlexGrow -= flexItemStyle.flexGrow().value;
        totalFlexShrink -= flexItemStyle.flexShrink().value;
        totalWeightedFlexShrink -= flexItemStyle.flexShrink().value * violations[i]->flexBaseContentSize;
        // totalWeightedFlexShrink can be negative when we exceed the precision of
        // a double when we initially calcuate totalWeightedFlexShrink. We then
        // subtract each child's weighted flex shrink with full precision, now
        // leading to a negative result. See
        // css3/flexbox/large-flex-shrink-assert.html
        totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
        violations[i]->frozen = true;
    }
}
    
void RenderFlexibleBox::freezeInflexibleItems(FlexSign flexSign, FlexLayoutItems& flexLayoutItems, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
    // we freeze all items with a flex factor of 0 as well as those with a min/max
    // size violation.
    Vector<FlexLayoutItem*, 4> newInflexibleItems;
    for (auto& flexLayoutItem : flexLayoutItems) {
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        ASSERT(!flexLayoutItem.frozen);
        float flexFactor = (flexSign == FlexSign::PositiveFlexibility) ? flexLayoutItem.style().flexGrow().value : flexLayoutItem.style().flexShrink().value;
        if (!flexFactor || (flexSign == FlexSign::PositiveFlexibility && flexLayoutItem.flexBaseContentSize > flexLayoutItem.hypotheticalMainContentSize) || (flexSign == FlexSign::NegativeFlexibility && flexLayoutItem.flexBaseContentSize < flexLayoutItem.hypotheticalMainContentSize)) {
            flexLayoutItem.flexedContentSize = flexLayoutItem.hypotheticalMainContentSize;
            newInflexibleItems.append(&flexLayoutItem);
        }
    }
    freezeViolations(newInflexibleItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, FlexLayoutItems& flexLayoutItems, LayoutUnit initialFreeSpace, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    LayoutUnit totalViolation;
    LayoutUnit usedFreeSpace;
    Vector<FlexLayoutItem*, 4> minViolations;
    Vector<FlexLayoutItem*, 4> maxViolations;

    double sumFlexFactors = (flexSign == FlexSign::PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
    if (sumFlexFactors > 0 && sumFlexFactors < 1) {
        LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
        if (fractional.abs() < remainingFreeSpace.abs())
            remainingFreeSpace = fractional;
    }

    for (auto& flexLayoutItem : flexLayoutItems) {
        // This check also covers out-of-flow children.
        if (flexLayoutItem.frozen)
            continue;
        
        auto& flexItemStyle = flexLayoutItem.style();
        LayoutUnit flexItemSize = flexLayoutItem.flexBaseContentSize;
        double extraSpace = 0;
        if (remainingFreeSpace > 0 && totalFlexGrow > 0 && flexSign == FlexSign::PositiveFlexibility && std::isfinite(totalFlexGrow))
            extraSpace = remainingFreeSpace * (flexItemStyle.flexGrow().value / totalFlexGrow);
        else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == FlexSign::NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && !flexItemStyle.flexShrink().isZero())
            extraSpace = remainingFreeSpace * flexItemStyle.flexShrink().value * flexLayoutItem.flexBaseContentSize / totalWeightedFlexShrink;
        if (std::isfinite(extraSpace))
            flexItemSize += LayoutUnit::fromFloatRound(extraSpace);

        LayoutUnit adjustedFlexItemSize = flexLayoutItem.constrainSizeByMinMax(flexItemSize);
        ASSERT(adjustedFlexItemSize >= 0);
        flexLayoutItem.flexedContentSize = adjustedFlexItemSize;
        usedFreeSpace += adjustedFlexItemSize - flexLayoutItem.flexBaseContentSize;
        
        LayoutUnit violation = adjustedFlexItemSize - flexItemSize;
        if (violation > 0)
            minViolations.append(&flexLayoutItem);
        else if (violation < 0)
            maxViolations.append(&flexLayoutItem);
        totalViolation += violation;
    }
    
    if (totalViolation)
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
    else
        remainingFreeSpace -= usedFreeSpace;
    
    return !totalViolation;
}

inline ContentPosition NODELETE resolveLeftRightAlignment(ContentPosition position, StyleContentAlignmentData justifyContent, const Style::ComputedStyle& style, bool isReversed)
{
    if (position == ContentPosition::Left || position == ContentPosition::Right) {
        auto leftRightAxisDirection = RenderFlexibleBox::leftRightAxisDirectionFromStyle(style);
        position = (justifyContent.isEndward(leftRightAxisDirection, isReversed))
            ? ContentPosition::End : ContentPosition::Start;
    }
    return position;
}

static LayoutUnit initialJustifyContentOffset(const Style::ComputedStyle& style, LayoutUnit availableFreeSpace, unsigned numberOfFlexItems, bool isReversed)
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

static LayoutUnit NODELETE justifyContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution justifyContentDistribution, unsigned numberOfFlexItems)
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

static LayoutUnit NODELETE alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition position, std::optional<LayoutUnit> ascent, std::optional<LayoutUnit> maxAscent, bool isWrapReverse)
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

void RenderFlexibleBox::setOverridingMainSizeForFlexItem(RenderBox& flexItem, LayoutUnit preferredSize)
{
    if (mainAxisIsFlexItemInlineAxis(flexItem))
        flexItem.setOverridingBorderBoxLogicalWidth(preferredSize + flexItem.borderAndPaddingLogicalWidth());
    else
        flexItem.setOverridingBorderBoxLogicalHeight(preferredSize + flexItem.borderAndPaddingLogicalHeight());
}

LayoutUnit RenderFlexibleBox::staticMainAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    auto flexItemMainExtent = mainAxisMarginExtentForFlexItem(flexItem) + mainAxisExtentForFlexItem(flexItem);
    auto mainAxisContentSize = isColumnFlow() ? contentBoxLogicalHeight() : contentBoxLogicalWidth();
    auto availableSpace = mainAxisContentSize - flexItemMainExtent;
    auto isReverse = isColumnOrRowReverse();
    LayoutUnit offset = initialJustifyContentOffset(style(), availableSpace, { }, isReverse);
    if (isReverse)
        offset = availableSpace - offset;
    return offset;
}

LayoutUnit RenderFlexibleBox::staticCrossAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    auto availableSpace = availableAlignmentSpaceForFlexItem(crossAxisContentExtent(), flexItem);
    auto safety = overflowAlignmentForFlexItem(flexItem);
    auto align = alignmentForFlexItem(flexItem);
    if (availableSpace < 0 && safety == OverflowAlignment::Safe)
        align = ItemPosition::FlexStart;
    return alignmentOffset(availableSpace, align, { }, { }, isWrapReverse());
}

LayoutUnit RenderFlexibleBox::staticInlinePositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return startOffsetForContent() + (isColumnFlow() ? staticCrossAxisPositionForPositionedFlexItem(flexItem) : staticMainAxisPositionForPositionedFlexItem(flexItem));
}

LayoutUnit RenderFlexibleBox::staticBlockPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return borderAndPaddingBefore() + (isColumnFlow() ? staticMainAxisPositionForPositionedFlexItem(flexItem) : staticCrossAxisPositionForPositionedFlexItem(flexItem));
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

// This refers to https://drafts.csswg.org/css-flexbox-1/#definite-sizes, section 1).
LayoutUnit RenderFlexibleBox::innerCrossSizeForFlexItem(const RenderBox& flexItem) const
{
    if (isColumnFlow())
        return contentBoxLogicalWidth();

    // Keep this sync'ed with hasDefiniteCrossSizeForFlexItem().
    auto flexContainerInnerCrossSize = [&] {
        auto isHorizontal = isHorizontalFlow();
        auto size = isHorizontal ? style().height() : style().width();
        auto innerCrossSize = LayoutUnit { };
        if (auto fixedSize = size.tryFixed())
            innerCrossSize = adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedSize->resolveZoom(style().usedZoomForLength()) });
        else if (size.isPercent())
            innerCrossSize = availableLogicalHeightForPercentageComputation().value_or(0_lu);
        else if (canResolveFullyConstrainedLogicalHeight(*this) || hasDefiniteLogicalWidthForAspectRatioCrossSize())
            innerCrossSize = std::max(0_lu, computeLogicalHeight(logicalHeight(), 0_lu).extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight());
        else
            ASSERT_NOT_REACHED();

        auto maximumSize = isHorizontal ? style().maxHeight() : style().maxWidth();
        if (auto fixedMaximumSize = maximumSize.tryFixed())
            innerCrossSize = std::min(innerCrossSize, adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedMaximumSize->resolveZoom(style().usedZoomForLength()) }));

        auto minimumSize = isHorizontal ? style().minHeight() : style().minWidth();
        if (auto fixedMinimumSize = minimumSize.tryFixed())
            innerCrossSize = std::max(innerCrossSize, adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedMinimumSize->resolveZoom(style().usedZoomForLength()) }));

        return innerCrossSize;
    };
    return std::max(0_lu, flexContainerInnerCrossSize() - crossAxisMarginExtentForFlexItem(flexItem));
}

void RenderFlexibleBox::prepareFlexItemForPositionedLayout(RenderBox& flexItem)
{
    ASSERT(flexItem.isOutOfFlowPositioned());
    flexItem.containingBlock()->addOutOfFlowBox(flexItem);
    CheckedPtr layer = flexItem.layer();
    LayoutUnit staticInlinePosition = flowAwareBorderStart() + flowAwarePaddingStart();
    if (layer->staticInlinePosition() != staticInlinePosition) {
        layer->setStaticInlinePosition(staticInlinePosition);
        if (flexItem.style().hasStaticInlinePosition(writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = flowAwareBorderBefore() + flowAwarePaddingBefore();
    if (layer->staticBlockPosition() != staticBlockPosition) {
        layer->setStaticBlockPosition(staticBlockPosition);
        if (flexItem.style().hasStaticBlockPosition(writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

inline OverflowAlignment RenderFlexibleBox::overflowAlignmentForFlexItem(const RenderBox& flexItem) const
{
    return flexItem.style().alignSelf().resolve(&style()).overflow();
}

ItemPosition RenderFlexibleBox::alignmentForFlexItem(const RenderBox& flexItem) const
{
    auto align = flexItem.style().alignSelf().resolve(&style()).position();
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
        bool hasSameDirection = isHorizontalFlow()
            ? writingMode().isAnyTopToBottom() == flexItem.writingMode().isAnyTopToBottom()
            : writingMode().isAnyLeftToRight() == flexItem.writingMode().isAnyLeftToRight();
        return hasSameDirection == (align == ItemPosition::SelfStart)
            ? ItemPosition::FlexStart : ItemPosition::FlexEnd;
    }

    if (isWrapReverse()) {
        if (align == ItemPosition::FlexStart)
            align = ItemPosition::FlexEnd;
        else if (align == ItemPosition::FlexEnd)
            align = ItemPosition::FlexStart;
    }

    return align;
}

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem)
{
    if (hasAutoMarginsInCrossAxis(flexItem)) {
        flexItem.updateLogicalHeight();
        if (isHorizontalFlow()) {
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

bool RenderFlexibleBox::willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode mode) const
{
    auto physicalAxis = mapAxisLogicalToPhysical(writingMode(), containingAxis);
    if (isHorizontalFlow() == (BoxAxis::Horizontal == physicalAxis))
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

bool RenderFlexibleBox::needToStretchFlexItemLogicalHeight(const RenderBox& flexItem) const
{
    // This function is a little bit magical. It relies on the fact that blocks
    // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
    // an implicit width: 100%. So the child will automatically stretch if our
    // cross axis is the child's inline axis. That's the case if:
    // - We are horizontal and the child is in vertical writing mode
    // - We are vertical and the child is in horizontal writing mode
    // Otherwise, we need to stretch if the cross axis size is auto.
    if (isHorizontalFlow() != flexItem.isHorizontalWritingMode())
        return false;

    // Aspect ratio is properly handled by RenderReplaced during layout.
    if (flexItem.isRenderReplaced() && flexItemHasAspectRatio(flexItem))
        return false;

    if (flexItem.style().logicalHeight().isStretch())
        return true;

    return alignmentForFlexItem(flexItem) == ItemPosition::Stretch
        && flexItem.style().logicalHeight().isAuto();
}

bool RenderFlexibleBox::flexBaseSizeNeedsBlockAxisContentSize(const RenderBox& flexItem)
{
    if (mainAxisIsFlexItemInlineAxis(flexItem))
        return false;

    auto flexBasis = flexBasisForFlexItem(flexItem);
    auto minSize = minMainSizeLengthForFlexItem(flexItem);
    auto maxSize = maxMainSizeLengthForFlexItem(flexItem);
    // FIXME: we must run flexItemMainSizeIsDefinite() because it might end up calling computePercentageLogicalHeight()
    // which has some side effects like calling addPercentHeightDescendant() for example so it is not possible to skip
    // the call for example by moving it to the end of the conditional expression. This is error-prone and we should
    // refactor computePercentageLogicalHeight() at some point so that it only computes stuff without those side effects.
    if (!flexItemMainSizeIsDefinite(flexItem, flexBasis) || minSize.isIntrinsic() || maxSize.isIntrinsic())
        return true;

    if (useContentBasedMinimumSize(flexItem))
        return true;

    return false;
}

Overflow RenderFlexibleBox::mainAxisOverflowForFlexItem(const RenderBox& flexItem) const
{
    if (isHorizontalFlow())
        return flexItem.style().overflowX();
    return flexItem.style().overflowY();
}

Overflow RenderFlexibleBox::crossAxisOverflowForFlexItem(const RenderBox& flexItem) const
{
    if (isHorizontalFlow())
        return flexItem.style().overflowY();
    return flexItem.style().overflowX();
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

static LayoutUnit NODELETE contentAlignmentStartOverflow(LayoutUnit availableFreeSpace, ContentPosition position, ContentDistribution distribution, OverflowAlignment safety, bool isReverse)
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

RenderFlexibleBox::FlexLineResult RenderFlexibleBox::layoutAndPlaceFlexItems(LayoutUnit crossAxisOffset, FlexLayoutItems& flexLayoutItems, LayoutUnit availableFreeSpace, RelayoutChildren relayoutChildren, LayoutUnit gapBetweenItems)
{
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(flexLayoutItems, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(style(), availableFreeSpace, flexLayoutItems.size(), isColumnOrRowReverse());
    if (style().flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    if (availableFreeSpace < 0) {
        auto resolvedJustifyContent = style().justifyContent().resolve(contentAlignmentNormalBehavior());
        auto distribution = resolvedJustifyContent.distribution();
        auto safety = resolvedJustifyContent.overflow();
        auto position = resolveLeftRightAlignment(resolvedJustifyContent.position(), resolvedJustifyContent, style(), isColumnOrRowReverse());
        LayoutUnit overflow = contentAlignmentStartOverflow(availableFreeSpace, position, distribution, safety, isColumnOrRowReverse());
        m_justifyContentStartOverflow = std::max(m_justifyContentStartOverflow, overflow);
    }

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxFlexItemCrossAxisExtent;

    LayoutUnit maxAscent;
    LayoutUnit maxDescent = LayoutUnit::min();
    LayoutUnit lastBaselineMaxAscent;
    std::optional<BaselineAlignmentState> baselineAlignmentState;

    auto resolvedJustifyContent = style().justifyContent().resolve(contentAlignmentNormalBehavior());
    auto distribution = resolvedJustifyContent.distribution();
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer.get();

        ASSERT(!flexItem.isOutOfFlowPositioned());

        layoutFlexItemAfterMainSizing(flexLayoutItems[i], relayoutChildren);

        updateAutoMarginsInMainAxis(flexItem, autoMarginOffset);

        LayoutUnit flexItemCrossAxisMarginBoxExtent;

        auto alignment = alignmentForFlexItem(flexItem);
        if ((alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline) && !hasAutoMarginsInCrossAxis(flexItem)) {
            LayoutUnit ascent = marginBoxAscentForFlexItem(flexItem);
            LayoutUnit descent = (crossAxisMarginExtentForFlexItem(flexItem) + crossAxisExtentForFlexItem(flexItem)) - ascent;
            maxDescent = std::max(maxDescent, descent);

            if (!baselineAlignmentState) {
                auto alignmentContextAxis = style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
                baselineAlignmentState = { flexItem, alignment, ascent, alignmentContextAxis, style().writingMode() };
            } else
                baselineAlignmentState->updateSharedGroup(flexItem, alignment, ascent);

            if (alignment == ItemPosition::Baseline) {
                maxAscent =  std::max(maxAscent, ascent);
                flexItemCrossAxisMarginBoxExtent = maxAscent + maxDescent;
            } else {
                lastBaselineMaxAscent = std::max(lastBaselineMaxAscent, ascent);
                flexItemCrossAxisMarginBoxExtent = lastBaselineMaxAscent + maxDescent;
            }

        } else
            flexItemCrossAxisMarginBoxExtent = crossAxisIntrinsicExtentForFlexItem(flexItem) + crossAxisMarginExtentForFlexItem(flexItem);

        if (!isColumnFlow())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + flexItemCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxFlexItemCrossAxisExtent = std::max(maxFlexItemCrossAxisExtent, flexItemCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForFlexItem(flexItem);

        LayoutUnit flexItemMainExtent = mainAxisExtentForFlexItem(flexItem);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later in flipForRightToLeftColumn.
        auto leadingScrollbarSize = writingMode().isInlineFlipped() && writingMode().isVertical() ? mainAxisScrollbarExtent() : LayoutUnit();
        LayoutPoint location(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - flexItemMainExtent - leadingScrollbarSize : mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForFlexItem(flexItem));
        setFlowAwareLocationForFlexItem(flexItem, location);
        mainAxisOffset += flexItemMainExtent + flowAwareMarginEndForFlexItem(flexItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }

        // FIXME: Deal with pagination.
    }

    if (isColumnFlow())
        setLogicalHeight(std::max(logicalHeight(), mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight()));

    if (style().flexDirection() == FlexDirection::ColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex
        // items since the start depends on the height of the flexbox, which we
        // only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(flexLayoutItems, crossAxisOffset, availableFreeSpace, gapBetweenItems);
    }

    return { crossAxisOffset + maxFlexItemCrossAxisExtent, maxFlexItemCrossAxisExtent, baselineAlignmentState };
}

void RenderFlexibleBox::layoutFlexItemAfterMainSizing(FlexLayoutItem& flexLayoutItem, RelayoutChildren relayoutChildren)
{
    auto& flexItem = flexLayoutItem.renderer.get();

    setOverridingMainSizeForFlexItem(flexItem, flexLayoutItem.flexedContentSize);
    // The flexed content size and the override size include the scrollbar
    // width, so we need to compare to the size including the scrollbar.
    // FIXME: Should it include the scrollbar?
    if (flexLayoutItem.flexedContentSize != mainAxisContentExtentForFlexItemIncludingScrollbar(flexItem))
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

void RenderFlexibleBox::layoutColumnReverse(const FlexLayoutItems& flexLayoutItems, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // This is similar to the logic in layoutAndPlaceFlexItems, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(style(), availableFreeSpace, flexLayoutItems.size(), isColumnOrRowReverse());
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    auto distribution = style().justifyContent().resolve(contentAlignmentNormalBehavior()).distribution();

    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer;
        ASSERT(!flexItem->isOutOfFlowPositioned());
        mainAxisOffset -= mainAxisExtentForFlexItem(flexItem) + flowAwareMarginEndForFlexItem(flexItem);
        setFlowAwareLocationForFlexItem(flexItem, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForFlexItem(flexItem)));
        mainAxisOffset -= flowAwareMarginStartForFlexItem(flexItem);
        
        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset -= justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }
    }
}

static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistribution alignContentDistribution, OverflowAlignment safety, unsigned numberOfLines, bool isReversed)
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

static LayoutUnit NODELETE alignContentSpaceBetweenFlexItems(LayoutUnit availableFreeSpace, ContentDistribution alignContentDistribution, unsigned numberOfLines)
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

void RenderFlexibleBox::alignFlexLines(FlexLineStates& lineStates, LayoutUnit gapBetweenLines)
{
    if (lineStates.isEmpty() || !isMultiline())
        return;

    auto alignedContent = style().alignContent().resolve(contentAlignmentNormalBehavior());
    auto position = alignedContent.position();
    auto distribution = alignedContent.distribution();
    auto safety = alignedContent.overflow();

    bool isWrapReverse = this->isWrapReverse();

    if (position == ContentPosition::FlexStart && !gapBetweenLines && safety != OverflowAlignment::Safe && !isWrapReverse)
        return;

    size_t numLines = lineStates.size();
    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent() - (numLines - 1) * gapBetweenLines;
    for (size_t i = 0; i < numLines; ++i)
        availableCrossAxisSpace -= lineStates[i].crossAxisExtent;

    m_alignContentStartOverflow = contentAlignmentStartOverflow(availableCrossAxisSpace, position, distribution, safety, isWrapReverse);
    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, position, distribution, safety, numLines, isWrapReverse);
    for (unsigned lineNumber = 0; lineNumber < numLines; ++lineNumber) {
        LineState& lineState = lineStates[lineNumber];
        lineState.crossAxisOffset += lineOffset;
        for (auto& flexLayoutItem : lineState.flexLayoutItems)
            adjustAlignmentForFlexItem(flexLayoutItem.renderer, lineOffset);
        
        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            lineStates[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(numLines);

        lineOffset += alignContentSpaceBetweenFlexItems(availableCrossAxisSpace, distribution, numLines) + gapBetweenLines;
    }
}
    
void RenderFlexibleBox::adjustAlignmentForFlexItem(RenderBox& flexItem, LayoutUnit delta)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
    setFlowAwareLocationForFlexItem(flexItem, flowAwareLocationForFlexItem(flexItem) + LayoutSize(0_lu, delta));
}
    
void RenderFlexibleBox::alignFlexItems(FlexLineStates& lineStates)
{
    for (LineState& lineState : lineStates) {
        LayoutUnit lineCrossAxisExtent = lineState.crossAxisExtent;
        auto baselineAlignmentState = lineState.baselineAlignmentState;

        if (lineState.baselineAlignmentState)
            performBaselineAlignment(lineState);

        for (auto& flexLayoutItem : lineState.flexLayoutItems) {
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());

            auto safety = overflowAlignmentForFlexItem(flexLayoutItem.renderer);
            auto position = alignmentForFlexItem(flexLayoutItem.renderer);
            if (updateAutoMarginsInCrossAxis(flexLayoutItem.renderer, std::max(0_lu, availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer))) || position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
                continue;
            
            if (position == ItemPosition::Stretch)
                applyStretchAlignmentToFlexItem(flexLayoutItem.renderer, lineCrossAxisExtent);
            LayoutUnit availableSpace = availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer);
            if (availableSpace < 0 && safety == OverflowAlignment::Safe)
                position = ItemPosition::FlexStart; // See Start == FlexStart assumption in alignmentForFlexItem().
            LayoutUnit offset = alignmentOffset(availableSpace, position, { }, { }, isWrapReverse());
            adjustAlignmentForFlexItem(flexLayoutItem.renderer, offset);
        }
    }
}

void RenderFlexibleBox::performBaselineAlignment(LineState& lineState)
{
    ASSERT(lineState.baselineAlignmentState);

    auto lineCrossAxisExtent = lineState.crossAxisExtent;
    bool containerHasWrapReverse = isWrapReverse();

    auto flexItemWritingModeForBaselineAlignment = [&](const RenderBox& flexItem) {
        if (mainAxisIsFlexItemInlineAxis(flexItem))
            return flexItem.style().writingMode();

        auto alignmentContextAxis = style().isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        return BaselineAlignmentState::usedWritingModeForBaselineAlignment(alignmentContextAxis, writingMode(), flexItem.writingMode());
    };

    auto shouldAdjustItemTowardsCrossAxisEnd = [&](const FlowDirection& flexItemBlockFlowDirection, ItemPosition alignment) {
        ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);

        // The direction in which we are aligning (i.e. direction of the cross axis) must be parallel with the direction of the flex item's used writing mode
        ASSERT_IMPLIES(crossAxisDirection() == RenderFlexibleBox::Direction::TopToBottom || crossAxisDirection() == RenderFlexibleBox::Direction::BottomToTop, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::TopToBottom || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::BottomToTop);
        ASSERT_IMPLIES(crossAxisDirection() == RenderFlexibleBox::Direction::LeftToRight || crossAxisDirection() == RenderFlexibleBox::Direction::RightToLeft, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::LeftToRight || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::RightToLeft);

        // For first baseline aligned items, if its block direction is the opposite of
        // the cross axis direction, then that means its fallback alignment (safe self-start)
        // is in the direction of the end of the cross axis
        //
        // For last baseline aligned items, if its block direction is in the same direction as
        // the cross axis direction, then that means its fallback alignment (safe self-end) is
        // in the direction of the end of the cross axis
        if (alignment == ItemPosition::Baseline)
            return crossAxisDirection() != flexItemBlockFlowDirection;
        return crossAxisDirection() == flexItemBlockFlowDirection;
    };

    for (auto& baselineSharingGroup : lineState.baselineAlignmentState.value().sharedGroups()) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        for (auto& flexItem : baselineSharingGroup) {
            auto position = alignmentForFlexItem(flexItem);
            ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
            auto offset = alignmentOffset(availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem), position, marginBoxAscentForFlexItem(flexItem), baselineSharingGroup.maxAscent(), containerHasWrapReverse);
            adjustAlignmentForFlexItem(flexItem, offset);

            if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), position))
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem) - offset);
        }
        // css-align-3 9.3 part 3:
        // Position the aligned baseline-sharing group within the alignment container according to its
        // fallback alignment. The fallback alignment of a baseline-sharing group is the fallback alignment
        // of its items as resolved to physical directions.
        if (minMarginAfterBaseline) {
            for (auto& flexItem : baselineSharingGroup) {
                if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), alignmentForFlexItem(flexItem)) && !hasAutoMarginsInCrossAxis(flexItem))
                    adjustAlignmentForFlexItem(flexItem, minMarginAfterBaseline);
            }
        }
    }
}

void RenderFlexibleBox::applyStretchMinMaxCrossSize(RenderBox& flexItem, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis crossAxis)
{
    bool isBlockAxis = crossAxis == LogicalBoxAxis::Block;
    auto& style = flexItem.style();
    auto& min = isBlockAxis ? style.logicalMinHeight() : style.logicalMinWidth();
    auto& max = isBlockAxis ? style.logicalMaxHeight() : style.logicalMaxWidth();
    bool minIsStretch = min.isStretch();
    bool maxIsStretch = !max.isNone() && max.isStretch();
    if (!minIsStretch && !maxIsStretch)
        return;

    // The block-axis floor ensures the stretched size never goes below border+padding,
    // matching the behavior in applyStretchAlignmentToFlexItem.
    auto stretchValue = std::max(isBlockAxis ? flexItem.borderAndPaddingLogicalHeight() : 0_lu,
        lineCrossAxisExtent - crossAxisMarginExtentForFlexItem(flexItem));

    auto computeBlockSize = [&](const auto& size, LayoutUnit fallback) {
        return flexItem.computeLogicalHeightUsing(size, std::nullopt).value_or(fallback);
    };
    auto computeInlineSize = [&](const auto& size) {
        return flexItem.computeLogicalWidthUsing(size, crossAxisContentExtent(), *this);
    };

    // Compute the specified cross-size, unclamped by stretch min/max.
    // We cannot use the current laid-out size because the initial layout
    // resolves stretch against the container, not the flex line.
    auto specifiedSize = isBlockAxis
        ? computeBlockSize(style.logicalHeight(), flexItem.logicalHeight())
        : computeInlineSize(style.logicalWidth());

    // Resolve each constraint: stretch resolves to the line cross size,
    // non-stretch constraints are computed normally.
    auto effectiveMax = maxIsStretch ? stretchValue
        : max.isNone() ? LayoutUnit::max()
        : isBlockAxis ? computeBlockSize(max, LayoutUnit::max())
        : computeInlineSize(max);

    // FIXME: The auto minimum does not account for aspect-ratio automatic
    // minimums, which are computed in constrainLogicalHeightByMinMax.
    auto effectiveMin = minIsStretch ? stretchValue
        : isBlockAxis ? computeBlockSize(min, 0_lu)
        : computeInlineSize(min);

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
}

void RenderFlexibleBox::applyStretchAlignmentToFlexItem(RenderBox& flexItem, LayoutUnit lineCrossAxisExtent)
{
    auto flexLayoutScope = SetForScope(m_afterCrossAxisItemSizing, true);
    if (mainAxisIsFlexItemInlineAxis(flexItem)) {
        // Cross axis is block axis (height).
        if (!flexItem.style().logicalHeight().isAuto() && !flexItem.style().logicalHeight().isStretch())
            return applyStretchMinMaxCrossSize(flexItem, lineCrossAxisExtent, LogicalBoxAxis::Block);

        auto stretchedLogicalHeight = std::max(flexItem.borderAndPaddingLogicalHeight(),
            lineCrossAxisExtent - crossAxisMarginExtentForFlexItem(flexItem));
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
        return;
    }

    // Cross axis is inline axis (width).
    if (!flexItem.style().logicalWidth().isAuto() && !flexItem.style().logicalWidth().isStretch())
        return applyStretchMinMaxCrossSize(flexItem, lineCrossAxisExtent, LogicalBoxAxis::Inline);

    auto flexItemWidth = std::max(0_lu, lineCrossAxisExtent - crossAxisMarginExtentForFlexItem(flexItem));
    flexItemWidth = flexItem.constrainLogicalWidthByMinMax(flexItemWidth, crossAxisContentExtent(), *this);

    if (flexItemWidth != flexItem.logicalWidth()) {
        flexItem.setOverridingBorderBoxLogicalWidth(flexItemWidth);
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
        flexItem.layoutIfNeeded();
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn(const FlexLineStates& lineStates)
{
    if (writingMode().isLogicalLeftInlineStart() || !isColumnFlow())
        return;
    
    LayoutUnit crossExtent = crossAxisExtent();
    for (size_t lineNumber = 0; lineNumber < lineStates.size(); ++lineNumber) {
        const LineState& lineState = lineStates[lineNumber];
        for (auto& flexLayoutItem : lineState.flexLayoutItems) {
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            
            LayoutPoint location = flowAwareLocationForFlexItem(flexLayoutItem.renderer);
            // For vertical flows, setFlowAwareLocationForFlexItem will transpose x and
            // y, so using the y axis for a column cross axis extent is correct.
            location.setY(crossExtent - crossAxisExtentForFlexItem(flexLayoutItem.renderer) - location.y());
            if (!isHorizontalWritingMode())
                location.move(LayoutSize(0, -horizontalScrollbarHeight()));
            setFlowAwareLocationForFlexItem(flexLayoutItem.renderer, location);
        }
    }
}

void RenderFlexibleBox::flipForWrapReverse(const FlexLineStates& lineStates, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    for (size_t lineNumber = 0; lineNumber < lineStates.size(); ++lineNumber) {
        const LineState& lineState = lineStates[lineNumber];
        for (auto& flexLayoutItem : lineState.flexLayoutItems) {
            LayoutUnit lineCrossAxisExtent = lineStates[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineStates[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForFlexItem(flexLayoutItem.renderer, newOffset - originalOffset);
        }
    }
}

std::optional<TextDirection> RenderFlexibleBox::leftRightAxisDirectionFromStyle(const Style::ComputedStyle& style)
{
    if (!style.isColumnFlexDirection()) // Prioritize text direction.
        return style.writingMode().bidiDirection();

    if (style.writingMode().isVertical()) { // Fall back to block direction if possible.
        return style.writingMode().isBlockLeftToRight()
            ? TextDirection::LTR
            : TextDirection::RTL;
    }

    return std::nullopt;
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

LayoutUnit RenderFlexibleBox::computeGap(RenderFlexibleBox::GapType gapType) const
{
    // row-gap is used for gaps between flex items in column flows or for gaps between lines in row flows.
    bool usesRowGap = (gapType == GapType::BetweenItems) == isColumnFlow();
    auto& gap = usesRowGap ? style().rowGap() : style().columnGap();
    if (gap.isNormal()) [[likely]]
        return { };

    auto availableSize = usesRowGap ? availableLogicalHeightForPercentageComputation().value_or(0_lu) : contentBoxLogicalWidth();
    return Style::evaluateMinimum<LayoutUnit>(gap, availableSize, Style::ZoomNeeded { });
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

const RenderBox* RenderFlexibleBox::firstBaselineCandidateOnLine(OrderIterator flexItemIterator, size_t numberOfItemsOnLine) const
{
    // Note that "first" here means in iterator order and not logical flex order (caller can pass in reversed order).
    size_t index = 0;
    const RenderBox* baselineFlexItem = nullptr;
    for (auto* flexItem = flexItemIterator.first(); flexItem; flexItem = flexItemIterator.next()) {
        if (flexItemIterator.shouldSkipChild(*flexItem))
            continue;
        auto flexItemPosition = alignmentForFlexItem(*flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && mainAxisIsFlexItemInlineAxis(*flexItem) && !hasAutoMarginsInCrossAxis(*flexItem))
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
        auto flexItemPosition = alignmentForFlexItem(*flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && mainAxisIsFlexItemInlineAxis(*flexItem) && !hasAutoMarginsInCrossAxis(*flexItem))
            baselineFlexItem = flexItem;
        if (++index == numberOfItemsOnLine)
            return baselineFlexItem ? baselineFlexItem : flexItem;
    }
    return nullptr;
}

const RenderBox* RenderFlexibleBox::flexItemForFirstBaseline() const
{
    // Looking for baseline flex candidate on visually first line.
    auto useLastLine = isWrapReverse();
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
    auto useLastLine = isWrapReverse();
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

}

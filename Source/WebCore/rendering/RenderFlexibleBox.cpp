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
#include "LayoutIntegrationFlexLayout.h"
#include "LayoutRepainter.h"
#include "LayoutUnit.h"
#include "LineClampUpdater.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "FlexFormattingContext.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectEnums.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderSVGRoot.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderFlexibleBox);

// Flow-aware geometry helpers used only by RenderFlexibleBox; the rest of flex's geometry queries live in
// FlexFormattingUtils (internal to the flex formatting context).
static LayoutUnit crossAxisExtent(const RenderFlexibleBox& flexBox)
{
    return FlexFormattingUtils::isHorizontalFlow(flexBox) ? flexBox.borderBoxSize().height() : flexBox.borderBoxSize().width();
}

static LayoutUnit computeFlexItemMarginValue(const RenderFlexibleBox& flexBox, const Style::MarginEdge& margin)
{
    // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
    // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
    return Style::evaluateMinimum<LayoutUnit>(margin, flexBox.contentBoxLogicalWidth(), flexBox.style().usedZoomForLength());
}

static bool canSetFlexItemContentLogicalHeight(const RenderBox& flexItem)
{
    return !flexItem.isFloatingOrOutOfFlowPositioned() && !flexItem.shouldComputeLogicalHeightFromAspectRatio() && !is<RenderReplaced>(flexItem);
}

#define SET_OR_CLEAR_OVERRIDING_SIZE(box, SizeType, size)       \
    {                                                           \
        if (size)                                               \
            box->setOverridingBorderBoxLogical##SizeType(*size); \
        else                                                    \
            box->clearOverridingBorderBoxLogical##SizeType();    \
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
RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::ScopedCrossAxisOverrideForFlexItem(RenderBox& flexItem, InvalidateContentWidths invalidateContentWidths)
    : m_intrinsicWidthComputation(downcast<RenderFlexibleBox>(*flexItem.parent()).m_inFlexItemIntrinsicWidthComputation, true)
#if ASSERT_ENABLED
    , m_flexItem(flexItem)
#endif
{
    if (FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem)) {
        auto axis = FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) ? OverridingSizesScope::Axis::Block : OverridingSizesScope::Axis::Inline;
        m_overridingScope.emplace(flexItem, axis, FlexFormattingUtils::innerCrossSizeForFlexItem(flexItem));
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

    SetForScope flexLayoutStateScope(m_flexLayoutState, FlexLayoutState { });

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

        m_justifyContentStartOverflow = 0;

        beginUpdateScrollInfoAfterLayoutTransaction();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        // This must run before prepareFlexItemsAndMargins so the legend's isExcludedFromNormalLayout
        // bit is set before we collect the flex items (the excluded legend is not a flex item).
        layoutExcludedChildren(relayoutChildren);

        prepareFlexItemsAndMargins();

        FlexItemBorderBoxRects oldFlexItemRects;
        appendFlexItemBorderBoxRects(oldFlexItemRects);

        m_flexLayout.layout(relayoutChildren);

        m_flexLayoutState->setPhase(FlexLayoutState::Phase::PostFlexScrollbarLayout);
        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

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

    repainter.repaintAfterLayout();
}

std::optional<LayoutUnit> RenderFlexibleBox::firstLineBaseline() const
{
    return m_flexLayout.firstLineBaseline();
}

std::optional<LayoutUnit> RenderFlexibleBox::lastLineBaseline() const
{
    return m_flexLayout.lastLineBaseline();
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

    auto hitTestChild = [&](RenderBox& child) {
        if (child.hasSelfPaintingLayer())
            return false;
        auto location = flipForWritingModeForChild(child, scrolledOffset);
        if (!child.hitTest(request, result, locationInContainer, location))
            return false;
        updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - adjustedLocation)));
        return true;
    };

    // Hit-testing visits children front-to-back, i.e. the reverse of paint order.
    for (size_t i = m_flexItems.size(); i--;) {
        if (CheckedPtr flexItem = m_flexItems[i].get(); flexItem && hitTestChild(*flexItem))
            return true;
    }

    // A fieldset's legend is excluded from normal layout (placed in the border), so it is not a flex item and is
    // not in m_flexItems; hit-test it separately.
    if (CheckedPtr legend = isFieldset() ? findFieldsetLegend() : nullptr; legend && legend->isExcludedFromNormalLayout())
        return hitTestChild(*legend);

    return false;
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForFlexItem, bool usePrintRect)
{
    for (auto& renderer : m_flexItems) {
        CheckedPtr flexItem = renderer.get();
        if (flexItem && !paintChild(*flexItem, paintInfo, paintOffset, paintInfoForFlexItem, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

bool RenderFlexibleBox::willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode mode) const
{
    auto physicalAxis = mapAxisLogicalToPhysical(writingMode(), containingAxis);
    if (FlexFormattingUtils::isHorizontalFlow(*this) == (BoxAxis::Horizontal == physicalAxis))
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

        auto [minContentInParentInlineAxis, maxContentInParentInlineAxis] = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
            if (writingMode().isOrthogonal(flexItem->writingMode())) {
                auto intrinsicBlockSize = flexItem->computeIntrinsicLogicalHeight();
                return { intrinsicBlockSize, intrinsicBlockSize };
            }
            return computeChildIntrinsicLogicalWidths(*flexItem);
        }();

        minContentInParentInlineAxis += margin;
        maxContentInParentInlineAxis += margin;

        if (!FlexFormattingUtils::isColumnFlow(*this)) {
            maxLogicalWidth += maxContentInParentInlineAxis;
            if (FlexFormattingUtils::isMultiline(*this)) {
                // For multiline, the min preferred width is if you put a break between
                // each item.
                minLogicalWidth = std::max(minLogicalWidth, minContentInParentInlineAxis);
            } else
                minLogicalWidth += minContentInParentInlineAxis;
        } else {
            minLogicalWidth = std::max(minContentInParentInlineAxis, minLogicalWidth);
            maxLogicalWidth = std::max(maxContentInParentInlineAxis, maxLogicalWidth);
        }
    }

    if (!FlexFormattingUtils::isColumnFlow(*this) && numItemsWithNormalLayout > 1) {
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * FlexFormattingUtils::computeGap(*this, FlexFormattingUtils::GapType::BetweenItems);
        maxLogicalWidth += inlineGapSize;
        if (!FlexFormattingUtils::isMultiline(*this))
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
            return FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem);

        if (m_flexLayoutState) {
            auto phase = m_flexLayoutState->phase();
            if (phase >= FlexLayoutState::Phase::CrossAxisItemSizing) {
                // Final sizes for flex items are known in both the main and cross directions, so it's fine to resolve percentage heights using those final values.
                // Note that we run layout on flex content _after_ performing flex layout (see endAndCommitUpdateScrollInfoAfterLayoutTransaction/updateScrollInfoAfterLayout).
                return true;
            }
            if (phase >= FlexLayoutState::Phase::MainAxisItemSizing) {
                // Final sizes for flex items are available only along the main axis.
                // Percentages can be resolved only against those items when they are orthogonal to the flex container (i.e., their logical height is computed and final)
                return !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem);
            }
        }

        if (m_inSimplifiedLayout) {
            // While in simplified layout, we should only re-compute overflow and/or re-position out-of-flow boxes, some renderers (e.g. RenderReplaced and subclasses)
            // currently ignore this optimization and run regular layout.
            // Final sizes for flex items are known in both the main and cross directions, computed during previous layout(s).
            return true;
        }

        if (&flexItem == view().frameView().layoutContext().subtreeLayoutRoot())
            return !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem);

        // Outside of layout (i.e. when using relative percentage positioning), base the decision on style.
        return !m_flexLayoutState;
    };
    if (!canUseByLayoutPhase())
        return false;

    auto canUseByStyle = [&] {
        if (FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem))
            return FlexFormattingUtils::alignmentForFlexItem(flexItem) == ItemPosition::Stretch;

        // Flexbox 9.8 rule 2: definite flex-basis makes post-flexing main size definite.
        if (flexItemMainSizeIsDefinite(flexItem, FlexFormattingUtils::flexBasisForFlexItem(flexItem)))
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
    m_flexLayoutState->setFlexItemHasCompletedLayout(flexItem);

    auto blockAxisContentSize = [&] {
        auto flexBasis = FlexFormattingUtils::flexBasisForFlexItem(flexItem);
        if (flexBasis.isPercentOrCalculated() && !flexItemMainSizeIsDefinite(flexItem, flexBasis))
            return flexItemContentLogicalHeight(flexItem) + flexItem.scrollbarLogicalHeight();
        return flexItem.logicalHeight() - flexItem.borderAndPaddingLogicalHeight();
    }();

    // Cache it so a later layout can skip re-laying-out this item while it stays clean, and record that we laid it out this iteration.
    setBlockAxisSizeForFlexItem(flexItem, blockAxisContentSize);
    return blockAxisContentSize;
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

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem)
{
    if (!FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexItem))
        return;

    flexItem.updateLogicalHeight();
    if (FlexFormattingUtils::isHorizontalFlow(*this)) {
        if (flexItem.style().marginTop().isAuto())
            flexItem.setMarginTop(0_lu);
        if (flexItem.style().marginBottom().isAuto())
            flexItem.setMarginBottom(0_lu);
        return;
    }

    if (flexItem.style().marginLeft().isAuto())
        flexItem.setMarginLeft(0_lu);
    if (flexItem.style().marginRight().isAuto())
        flexItem.setMarginRight(0_lu);
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

bool RenderFlexibleBox::setStaticPositionForPositionedLayout(const RenderBox& flexItem)
{
    return m_flexLayout.setStaticPositionForPositionedLayout(flexItem);
}

bool RenderFlexibleBox::useContentBasedMinimumBlockSize(const RenderBox& flexItem) const
{
    return !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) && FlexFormattingUtils::useContentBasedMinimumSize(flexItem);
}

bool RenderFlexibleBox::hasStretchedFlexItemWithAspectRatio() const
{
    for (CheckedRef flexItem : childrenOfType<RenderBox>(*this)) {
        if (flexItem->isOutOfFlowPositioned() || flexItem->isExcludedFromNormalLayout())
            continue;
        if (!FlexFormattingUtils::flexItemHasAspectRatio(flexItem))
            continue;
        if (FlexFormattingUtils::alignmentForFlexItem(flexItem) == ItemPosition::Stretch
            && !FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexItem)
            && FlexFormattingUtils::preferredCrossSizeLengthForFlexItem(flexItem).isAuto())
            return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::computeGap(FlexFormattingUtils::GapType gapType) const
{
    return FlexFormattingUtils::computeGap(*this, gapType);
}

bool RenderFlexibleBox::isHorizontalFlow() const
{
    return FlexFormattingUtils::isHorizontalFlow(*this);
}

bool RenderFlexibleBox::isMultiline() const
{
    return FlexFormattingUtils::isMultiline(*this);
}

bool RenderFlexibleBox::mainAxisIsFlexItemInlineAxis(const RenderBox& flexItem) const
{
    return FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem);
}

Style::FlexBasis RenderFlexibleBox::flexBasisForFlexItem(const RenderBox& flexItem) const
{
    return FlexFormattingUtils::flexBasisForFlexItem(flexItem);
}

ItemPosition RenderFlexibleBox::alignmentForFlexItem(const RenderBox& flexItem) const
{
    return FlexFormattingUtils::alignmentForFlexItem(flexItem);
}

bool RenderFlexibleBox::hasDefiniteCrossSizeForFlexItem(const RenderBox& flexItem) const
{
    return FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem);
}

void RenderFlexibleBox::appendFlexItemBorderBoxRects(FlexItemBorderBoxRects& flexItemBorderBoxRects)
{
    for (auto& renderer : m_flexItems) {
        if (CheckedPtr flexItem = renderer.get())
            flexItemBorderBoxRects.append(flexItem->borderBoxRectInContainer());
    }
}

void RenderFlexibleBox::repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects& oldFlexItemRects)
{
    size_t index = 0;
    for (auto& renderer : m_flexItems) {
        CheckedPtr flexItem = renderer.get();
        if (!flexItem)
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
    if (!FlexFormattingUtils::isColumnFlow(*this))
        return true;

    if (m_flexLayoutState) {
        if (m_flexLayoutState->isFlexBoxBlockSizeDefinite())
            return true;
        if (m_flexLayoutState->isFlexBoxBlockSizeIndefinite())
            return false;
    }

    auto isPercentResolveSuspended = view().frameView().layoutContext().isPercentHeightResolveDisabledFor(flexItem);
    ASSERT(!isPercentResolveSuspended || is<RenderBlock>(flexItem));

    bool definite = !isPercentResolveSuspended && flexItem.computePercentageLogicalHeight(flexBasis, updateDescendants).has_value();
    if (m_flexLayoutState && !writingMode().isOrthogonal(flexItem.writingMode()))
        m_flexLayoutState->setFlexBoxBlockSizeIsDefinite(definite);
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
    if (!FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) && (size.isIntrinsic() || size.isIntrinsicKeyword()))
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

// Explicit instantiations for the SizeTypes FlexFormattingContext resolves through RenderFlexibleBox from a separate translation unit.
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
    auto isRowsFlexbox = FlexFormattingUtils::isHorizontalFlow(*this);
    if (auto flexItem = firstInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineStart))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineStart.add(*flexItem) : m_marginTrimItems.m_itemsOnFirstFlexLine.add(*flexItem);
    if (auto flexItem = lastInFlowChildBox(); flexItem && marginTrim.contains(Style::MarginTrimSide::InlineEnd))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineEnd.add(*flexItem) : m_marginTrimItems.m_itemsOnLastFlexLine.add(*flexItem);
}

bool RenderFlexibleBox::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& flexItem) const
{
    ASSERT(style().marginTrim().contains(marginTrimSide));
    auto isMarginParallelWithMainAxis = [this](Style::MarginTrimSide marginTrimSide) {
        if (FlexFormattingUtils::isHorizontalFlow(*this))
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

void RenderFlexibleBox::prepareFlexItemsAndMargins()
{
    // Collect the in-flow flex items in order-modified document order (a stable sort by the used 'order' value
    // keeps document order among equal values). This is rebuilt every layout and replaces the order iterator.
    // Out-of-flow and excluded children are not flex items, so they are left out; the list holds weak pointers
    // because painting/hit-testing/baseline queries read it after layout, when a child may have been removed.
    m_flexItems.clear();
    for (auto& child : childrenOfType<RenderBox>(*this)) {
        if (!child.isOutOfFlowPositioned() && !child.isExcludedFromNormalLayout())
            m_flexItems.append(child);
    }
    std::stable_sort(m_flexItems.begin(), m_flexItems.end(), [](auto& a, auto& b) {
        return a->style().order().value < b->style().order().value;
    });

    for (auto& flexItem : m_flexItems) {
        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (FlexFormattingUtils::isHorizontalFlow(*this)) {
            flexItem->setMarginLeft(computeFlexItemMarginValue(*this, flexItem->style().marginLeft()));
            flexItem->setMarginRight(computeFlexItemMarginValue(*this, flexItem->style().marginRight()));
        } else {
            flexItem->setMarginTop(computeFlexItemMarginValue(*this, flexItem->style().marginTop()));
            flexItem->setMarginBottom(computeFlexItemMarginValue(*this, flexItem->style().marginBottom()));
        }
    }
}

FlexContainerUsedExtents RenderFlexibleBox::updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent)
{
    // Resolve the container's logical height to the largest of: what is already set, the block-axis extent FlexFormattingContext
    // built from its line sizes (row flow) or its column lines' main content extent (column flow), and the empty-line
    // minimum for a container that establishes a line with no in-flow items (e.g. all children are out of flow). The
    // empty-line minimum is a block-axis floor, so it is folded into the block-axis max here rather than compared
    // against the physical borderBoxHeight() (which is the inline extent in a vertical writing mode). Then resolve
    // against the container's own specified/min/max height and box-sizing, and return the used cross extents (line
    // positioning / item cross sizing / rtl-column flip) and block extents (column re-resolve / column-reverse
    // placement) so FlexFormattingContext takes them as values rather than reading them back off the container.
    auto minimumHeightForEmptyLine = hasLineIfEmpty() ? borderAndPaddingLogicalHeight() + lineHeight() + scrollbarLogicalHeight() : 0_lu;
    setLogicalHeight(std::max(minimumHeightForEmptyLine, std::max(logicalHeight(), borderAndPaddingLogicalHeight() + flexContentBlockExtent)));
    updateLogicalHeight();
    return { FlexFormattingUtils::crossAxisContentExtent(*this), crossAxisExtent(*this), contentBoxLogicalHeight(), logicalHeight() };
}

}

/*
 * Copyright (C) 2011, 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderGrid.h"

#include "GridArea.h"
#include "GridLayoutFunctions.h"
#include "GridMasonryLayout.h"
#include "GridPositionsResolver.h"
#include "GridTrackSizingAlgorithm.h"
#include "LayoutRepainter.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderGrid);

enum class TrackSizeRestriction : uint8_t {
    AllowInfinity,
    ForbidInfinity,
};

RenderGrid::RenderGrid(Element& element, RenderStyle&& style)
    : RenderBlock(Type::Grid, element, WTFMove(style), { })
    , m_grid(*this)
    , m_trackSizingAlgorithm(this, currentGrid())
    , m_masonryLayout(*this)
{
    ASSERT(isRenderGrid());
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid() = default;

StyleSelfAlignmentData RenderGrid::selfAlignmentForGridItem(GridAxis axis, const RenderBox& gridItem, const RenderStyle* gridStyle) const
{
    return axis == GridAxis::GridRowAxis ? justifySelfForGridItem(gridItem, StretchingMode::Any, gridStyle) : alignSelfForGridItem(gridItem, StretchingMode::Any, gridStyle);
}

bool RenderGrid::selfAlignmentChangedToStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& gridItem) const
{
    return selfAlignmentForGridItem(axis, gridItem, &oldStyle).position() != ItemPosition::Stretch
        && selfAlignmentForGridItem(axis, gridItem, &newStyle).position() == ItemPosition::Stretch;
}

bool RenderGrid::selfAlignmentChangedFromStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& gridItem) const
{
    return selfAlignmentForGridItem(axis, gridItem, &oldStyle).position() == ItemPosition::Stretch
        && selfAlignmentForGridItem(axis, gridItem, &newStyle).position() != ItemPosition::Stretch;
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifference::Layout)
        return;

    const RenderStyle& newStyle = this->style();

    auto hasDifferentTrackSizes = [&newStyle, &oldStyle](GridTrackSizingDirection direction) {
        return newStyle.gridTrackSizes(direction) != oldStyle->gridTrackSizes(direction);
    };

    if (hasDifferentTrackSizes(GridTrackSizingDirection::ForColumns) || hasDifferentTrackSizes(GridTrackSizingDirection::ForRows)) {
        for (auto& gridItem : childrenOfType<RenderBox>(*this))
            gridItem.setChildNeedsLayout();
    }

    if (oldStyle->resolvedAlignItems(selfAlignmentNormalBehavior(this)).position() == ItemPosition::Stretch) {
        // Style changes on the grid container implying stretching (to-stretch) or
        // shrinking (from-stretch) require the affected items to be laid out again.
        // These logic only applies to 'stretch' since the rest of the alignment
        // values don't change the size of the box.
        // In any case, the items' overrideSize will be cleared and recomputed (if
        // necessary)  as part of the Grid layout logic, triggered by this style
        // change.
        for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
            if (gridItem.isOutOfFlowPositioned())
                continue;
            if (selfAlignmentChangedToStretch(GridAxis::GridRowAxis, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedFromStretch(GridAxis::GridRowAxis, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedToStretch(GridAxis::GridColumnAxis, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedFromStretch(GridAxis::GridColumnAxis, *oldStyle, newStyle, gridItem)) {
                gridItem.setNeedsLayout();
            }
        }
    }

    auto subgridChanged = subgridDidChange(*oldStyle);
    if (explicitGridDidResize(*oldStyle) || namedGridLinesDefinitionDidChange(*oldStyle) || implicitGridLinesDefinitionDidChange(*oldStyle) || oldStyle->gridAutoFlow() != style().gridAutoFlow()
        || (style().gridAutoRepeatColumns().size() || style().gridAutoRepeatRows().size()) || subgridChanged)
        dirtyGrid(subgridChanged);
}

bool RenderGrid::subgridDidChange(const RenderStyle& oldStyle) const
{
    return oldStyle.gridSubgridRows() != style().gridSubgridRows()
        || oldStyle.gridSubgridColumns() != style().gridSubgridColumns();
}

bool RenderGrid::explicitGridDidResize(const RenderStyle& oldStyle) const
{
    return oldStyle.gridColumnTrackSizes().size() != style().gridColumnTrackSizes().size()
        || oldStyle.gridRowTrackSizes().size() != style().gridRowTrackSizes().size()
        || oldStyle.namedGridAreaColumnCount() != style().namedGridAreaColumnCount()
        || oldStyle.namedGridAreaRowCount() != style().namedGridAreaRowCount()
        || oldStyle.gridAutoRepeatColumns().size() != style().gridAutoRepeatColumns().size()
        || oldStyle.gridAutoRepeatRows().size() != style().gridAutoRepeatRows().size();
}

bool RenderGrid::namedGridLinesDefinitionDidChange(const RenderStyle& oldStyle) const
{
    return oldStyle.namedGridRowLines().map != style().namedGridRowLines().map
        || oldStyle.namedGridColumnLines().map != style().namedGridColumnLines().map;
}

bool RenderGrid::implicitGridLinesDefinitionDidChange(const RenderStyle& oldStyle) const
{
    return oldStyle.implicitNamedGridRowLines().map != style().implicitNamedGridRowLines().map
        || oldStyle.implicitNamedGridColumnLines().map != style().implicitNamedGridColumnLines().map;
}

// This method optimizes the gutters computation by skipping the available size
// call if gaps are fixed size (it's only needed for percentages).
std::optional<LayoutUnit> RenderGrid::availableSpaceForGutters(GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    const GapLength& gapLength = isRowAxis ? style().columnGap() : style().rowGap();
    if (gapLength.isNormal() || !gapLength.length().isPercentOrCalculated())
        return std::nullopt;

    return isRowAxis ? availableLogicalWidth() : contentLogicalHeight();
}

void RenderGrid::computeTrackSizesForDefiniteSize(GridTrackSizingDirection direction, LayoutUnit availableSpace, GridLayoutState& gridLayoutState)
{
    m_trackSizingAlgorithm.run(direction, numTracks(direction), SizingOperation::TrackSizing, availableSpace, gridLayoutState);
    ASSERT(m_trackSizingAlgorithm.tracksAreWiderThanMinTrackBreadth());
}

void RenderGrid::repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows, GridLayoutState& gridLayoutState)
{
    // In orthogonal flow cases column track's size is determined by using the computed
    // row track's size, which it was estimated during the first cycle of the sizing
    // algorithm. Hence we need to repeat computeUsedBreadthOfGridTracks for both,
    // columns and rows, to determine the final values.
    // TODO (lajava): orthogonal flows is just one of the cases which may require
    // a new cycle of the sizing algorithm; there may be more. In addition, not all the
    // cases with orthogonal flows require this extra cycle; we need a more specific
    // condition to detect whether grid item's min-content contribution has changed or not.
    // The complication with repeating the track sizing algorithm for flex max-sizing is that
    // it might change a grid item's status of participating in Baseline Alignment for
    // a cyclic sizing dependency case, which should be definitively excluded. See
    // https://github.com/w3c/csswg-drafts/issues/3046 for details.
    // FIXME: we are avoiding repeating the track sizing algorithm for grid item with baseline alignment
    // here in the case of using flex max-sizing functions. We probably also need to investigate whether
    // it is applicable for the case of percent-sized rows with indefinite height as well.
    if (gridLayoutState.needsSecondTrackSizingPass() || m_trackSizingAlgorithm.hasAnyPercentSizedRowsIndefiniteHeight() || (m_trackSizingAlgorithm.hasAnyFlexibleMaxTrackBreadth() && !m_trackSizingAlgorithm.hasAnyBaselineAlignmentItem()) || m_hasAspectRatioBlockSizeDependentItem) {

        populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows);
        computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForColumns, availableSpaceForColumns, gridLayoutState);
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForColumns, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForColumns).value(), nonCollapsedTracks(GridTrackSizingDirection::ForColumns));

        computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForRows, availableSpaceForRows, gridLayoutState);
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForRows, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForRows).value(), nonCollapsedTracks(GridTrackSizingDirection::ForRows));
    }
}

bool RenderGrid::canPerformSimplifiedLayout() const
{
    // We cannot perform a simplified layout if we need to position the items and we have some
    // positioned items to be laid out.
    if (currentGrid().needsItemsPlacement() && posChildNeedsLayout())
        return false;

    return RenderBlock::canPerformSimplifiedLayout();
}

template<typename F>
static void cacheBaselineAlignedGridItems(const RenderGrid& grid, GridTrackSizingAlgorithm& algorithm, uint32_t axes, F& callback, bool cachingRowSubgridsForRootGrid)
{
    ASSERT_IMPLIES(cachingRowSubgridsForRootGrid, !algorithm.renderGrid()->isSubgridRows() && (algorithm.renderGrid() == &grid || grid.isSubgridOf(GridLayoutFunctions::flowAwareDirectionForGridItem(*algorithm.renderGrid(), grid, GridTrackSizingDirection::ForRows), *algorithm.renderGrid())));

    for (auto* gridItem = grid.firstChildBox(); gridItem; gridItem = gridItem->nextSiblingBox()) {
        if (gridItem->isOutOfFlowPositioned() || gridItem->isLegend())
            continue;

        callback(gridItem);

        // We keep a cache of items with baseline as alignment values so that we only compute the baseline shims for
        // such items. This cache is needed for performance related reasons due to the cost of evaluating the item's
        // participation in a baseline context during the track sizing algorithm.
        uint32_t innerAxes = 0;
        CheckedPtr inner = dynamicDowncast<RenderGrid>(gridItem);

        if (axes & enumToUnderlyingType(GridAxis::GridColumnAxis)) {
            if (inner && inner->isSubgridInParentDirection(GridTrackSizingDirection::ForRows))
                innerAxes |= GridLayoutFunctions::isOrthogonalGridItem(grid, *gridItem) ? enumToUnderlyingType(GridAxis::GridRowAxis) : enumToUnderlyingType(GridAxis::GridColumnAxis);
            else if (grid.isBaselineAlignmentForGridItem(*gridItem, GridAxis::GridColumnAxis))
                algorithm.cacheBaselineAlignedItem(*gridItem, GridAxis::GridColumnAxis, cachingRowSubgridsForRootGrid);
        }

        if (axes & enumToUnderlyingType(GridAxis::GridRowAxis)) {
            if (inner && inner->isSubgridInParentDirection(GridTrackSizingDirection::ForColumns))
                innerAxes |= GridLayoutFunctions::isOrthogonalGridItem(grid, *gridItem) ? enumToUnderlyingType(GridAxis::GridColumnAxis) : enumToUnderlyingType(GridAxis::GridRowAxis);
            else if (grid.isBaselineAlignmentForGridItem(*gridItem, GridAxis::GridRowAxis))
                algorithm.cacheBaselineAlignedItem(*gridItem, GridAxis::GridRowAxis, cachingRowSubgridsForRootGrid);
        }

        if (inner && cachingRowSubgridsForRootGrid)
            cachingRowSubgridsForRootGrid = GridLayoutFunctions::isOrthogonalGridItem(*algorithm.renderGrid(), *inner) ? inner->isSubgridColumns() : inner->isSubgridRows();

        if (innerAxes)
            cacheBaselineAlignedGridItems(*inner, algorithm, innerAxes, callback, cachingRowSubgridsForRootGrid);
    }
}

Vector<RenderBox*> RenderGrid::computeAspectRatioDependentAndBaselineItems()
{
    Vector<RenderBox*> dependentGridItems;

    m_baselineItemsCached = true;
    m_hasAspectRatioBlockSizeDependentItem = false;

    auto computeOrthogonalAndDependentItems = [&](RenderBox* gridItem) {
        // Grid's layout logic controls the grid item's override content size, hence we need to
        // clear any override set previously, so it doesn't interfere in current layout
        // execution.
        gridItem->clearOverridingContentSize();

        // For a grid item that has an aspect-ratio and block-constraints such as the relative logical height,
        // when the grid width is auto, we may need get the real grid width before laying out the item.
        if (GridLayoutFunctions::isAspectRatioBlockSizeDependentGridItem(*gridItem) && (style().logicalWidth().isAuto() || style().logicalWidth().isMinContent() || style().logicalWidth().isMaxContent())) {
            dependentGridItems.append(gridItem);
            m_hasAspectRatioBlockSizeDependentItem = true;
        }
    };

    cacheBaselineAlignedGridItems(*this, m_trackSizingAlgorithm, enumToUnderlyingType(GridAxis::GridRowAxis) | enumToUnderlyingType(GridAxis::GridColumnAxis), computeOrthogonalAndDependentItems, !isSubgridRows());
    return dependentGridItems;
}

bool RenderGrid::canSetColumnAxisStretchRequirementForItem(const RenderBox& gridItem) const
{
    auto gridItemBlockFlowDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, GridTrackSizingDirection::ForRows);
    return gridItemBlockFlowDirection == GridTrackSizingDirection::ForRows && allowedToStretchGridItemAlongColumnAxis(gridItem);
}

void RenderGrid::computeLayoutRequirementsForItemsBeforeLayout(GridLayoutState& gridLayoutState) const
{
    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {

        auto gridItemAlignSelf = alignSelfForGridItem(gridItem).position();
        if (GridLayoutFunctions::isGridItemInlineSizeDependentOnBlockConstraints(gridItem, *this, gridItemAlignSelf)) {
            gridLayoutState.setNeedsSecondTrackSizingPass();
            gridLayoutState.setLayoutRequirementForGridItem(gridItem, ItemLayoutRequirement::MinContentContributionForSecondColumnPass);
        }

        if (!gridItem.needsLayout() || gridItem.isOutOfFlowPositioned() || gridItem.isExcludedFromNormalLayout())
            continue;

        if (canSetColumnAxisStretchRequirementForItem(gridItem))
            gridLayoutState.setLayoutRequirementForGridItem(gridItem, ItemLayoutRequirement::NeedsColumnAxisStretchAlignment);
    }
}

void RenderGrid::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // The layoutBlock was handling the layout of both the grid and masonry implementations.
    // This caused a huge amount of branching code to handle masonry specific cases. Splitting up the code
    // to layout will simplify both implementations.
    if (!isMasonry())
        layoutGrid(relayoutChildren);
    else
        layoutMasonry(relayoutChildren);
}

void RenderGrid::layoutGrid(bool relayoutChildren)
{
    LayoutRepainter repainter(*this);
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());

        GridLayoutState gridLayoutState;

        computeLayoutRequirementsForItemsBeforeLayout(gridLayoutState);

        preparePaginationBeforeBlockLayout(relayoutChildren);
        beginUpdateScrollInfoAfterLayoutTransaction();

        LayoutSize previousSize = size();

        // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() only but it does not work for positioned stuff.
        // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
        // FIXME: We might need to cache the hasDefiniteLogicalHeight if the call of RenderBlock::hasDefiniteLogicalHeight() causes a relevant performance regression.
        bool hasDefiniteLogicalHeight = RenderBlock::hasDefiniteLogicalHeight() || overridingLogicalHeight() || computeContentLogicalHeight(RenderBox::SizeType::MainOrPreferredSize, style().logicalHeight(), std::nullopt);

        auto aspectRatioBlockSizeDependentGridItems = computeAspectRatioDependentAndBaselineItems();

        resetLogicalHeightBeforeLayoutIfNeeded();

        updateLogicalWidth();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        LayoutUnit availableSpaceForColumns = availableLogicalWidth();
        placeItemsOnGrid(availableSpaceForColumns);

        m_trackSizingAlgorithm.setAvailableSpace(GridTrackSizingDirection::ForColumns, availableSpaceForColumns);
        performPreLayoutForGridItems(m_trackSizingAlgorithm, ShouldUpdateGridAreaLogicalSize::Yes);

        // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns. At this point the
        // logical width is always definite as the above call to updateLogicalWidth() properly resolves intrinsic
        // sizes. We cannot do the same for heights though because many code paths inside updateLogicalHeight() require
        // a previous call to setLogicalHeight() to resolve heights properly (like for positioned items for example).
        computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForColumns, availableSpaceForColumns, gridLayoutState);

        // 1.5. Compute Content Distribution offsets for column tracks
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForColumns, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForColumns).value(), nonCollapsedTracks(GridTrackSizingDirection::ForColumns));

        // 2. Next, the track sizing algorithm resolves the sizes of the grid rows,
        // using the grid column sizes calculated in the previous step.
        bool shouldRecomputeHeight = false;
        if (!hasDefiniteLogicalHeight) {
            computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows, gridLayoutState);
            if (shouldApplySizeContainment())
                shouldRecomputeHeight = true;
        } else {
            auto availableLogicalHeightForContentBox = [&] {
                if (auto overridingLogicalHeight = this->overridingLogicalHeight())
                    return constrainContentBoxLogicalHeightByMinMax(*overridingLogicalHeight - borderAndPaddingLogicalHeight(), { });
                return availableLogicalHeight(ExcludeMarginBorderPadding);
            };
            computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForRows, availableLogicalHeightForContentBox(), gridLayoutState);
        }

        LayoutUnit trackBasedLogicalHeight = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (auto size = explicitIntrinsicInnerLogicalSize(GridTrackSizingDirection::ForRows))
            trackBasedLogicalHeight += size.value();
        else
            trackBasedLogicalHeight += m_trackSizingAlgorithm.computeTrackBasedSize();

        if (shouldRecomputeHeight)
            computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForRows, trackBasedLogicalHeight, gridLayoutState);

        setLogicalHeight(trackBasedLogicalHeight);

        updateLogicalHeight();

        // Once grid's indefinite height is resolved, we can compute the
        // available free space for Content Alignment.
        if (!hasDefiniteLogicalHeight)
            m_trackSizingAlgorithm.setFreeSpace(GridTrackSizingDirection::ForRows, logicalHeight() - trackBasedLogicalHeight);

        // 2.5. Compute Content Distribution offsets for rows tracks
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForRows, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForRows).value(), nonCollapsedTracks(GridTrackSizingDirection::ForRows));

        if (!aspectRatioBlockSizeDependentGridItems.isEmpty()) {
            updateGridAreaForAspectRatioItems(aspectRatioBlockSizeDependentGridItems, gridLayoutState);
            updateLogicalWidth();
        }

        // 3. If the min-content contribution of any grid items have changed based on the row
        // sizes calculated in step 2, steps 1 and 2 are repeated with the new min-content
        // contribution (once only).
        repeatTracksSizingIfNeeded(availableSpaceForColumns, contentLogicalHeight(), gridLayoutState);

        // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
        if (hasLineIfEmpty()) {
            LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
                + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
                + scrollbarLogicalHeight();
            setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
        }

        layoutGridItems(gridLayoutState);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (size() != previousSize)
            relayoutChildren = true;

        m_outOfFlowItemColumn.clear();
        m_outOfFlowItemRow.clear();

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());
        m_trackSizingAlgorithm.reset();

        computeOverflow(layoutOverflowLogicalBottom(*this));

        updateDescendantTransformsAfterLayout();
    }

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();

    m_trackSizingAlgorithm.clearBaselineItemsCache();
    m_baselineItemsCached = false;
}

void RenderGrid::layoutMasonry(bool relayoutChildren)
{
    LayoutRepainter repainter(*this);
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());
        GridLayoutState gridLayoutState;

        preparePaginationBeforeBlockLayout(relayoutChildren);
        beginUpdateScrollInfoAfterLayoutTransaction();

        LayoutSize previousSize = size();

        // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() only but it does not work for positioned stuff.
        // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
        // FIXME: We might need to cache the hasDefiniteLogicalHeight if the call of RenderBlock::hasDefiniteLogicalHeight() causes a relevant performance regression.
        bool hasDefiniteLogicalHeight = RenderBlock::hasDefiniteLogicalHeight() || overridingLogicalHeight() || computeContentLogicalHeight(RenderBox::SizeType::MainOrPreferredSize, style().logicalHeight(), std::nullopt);

        auto aspectRatioBlockSizeDependentGridItems = computeAspectRatioDependentAndBaselineItems();

        resetLogicalHeightBeforeLayoutIfNeeded();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        updateLogicalWidth();

        LayoutUnit availableSpaceForColumns = availableLogicalWidth();
        placeItemsOnGrid(availableSpaceForColumns);

        m_trackSizingAlgorithm.setAvailableSpace(GridTrackSizingDirection::ForColumns, availableSpaceForColumns);
        performPreLayoutForGridItems(m_trackSizingAlgorithm, ShouldUpdateGridAreaLogicalSize::Yes);

        // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns. At this point the
        // logical width is always definite as the above call to updateLogicalWidth() properly resolves intrinsic
        // sizes. We cannot do the same for heights though because many code paths inside updateLogicalHeight() require
        // a previous call to setLogicalHeight() to resolve heights properly (like for positioned items for example).
        computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForColumns, availableSpaceForColumns, gridLayoutState);

        // 1.5. Compute Content Distribution offsets for column tracks
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForColumns, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForColumns).value(), nonCollapsedTracks(GridTrackSizingDirection::ForColumns));

        // 2. Next, the track sizing algorithm resolves the sizes of the grid rows,
        // using the grid column sizes calculated in the previous step.
        bool shouldRecomputeHeight = false;
        if (!hasDefiniteLogicalHeight) {
            computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows, gridLayoutState);
            if (shouldApplySizeContainment())
                shouldRecomputeHeight = true;
        } else
            computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForRows, availableLogicalHeight(ExcludeMarginBorderPadding), gridLayoutState);

        auto performMasonryPlacement = [&](const GridTrackSizingDirection masonryAxisDirection) {
            auto gridAxisDirection = masonryAxisDirection == GridTrackSizingDirection::ForRows ? GridTrackSizingDirection::ForColumns : GridTrackSizingDirection::ForRows;
            unsigned gridAxisTracksBeforeAutoPlacement = currentGrid().numTracks(gridAxisDirection);

            m_masonryLayout.performMasonryPlacement(m_trackSizingAlgorithm, gridAxisTracksBeforeAutoPlacement, masonryAxisDirection);
        };

        if (areMasonryRows())
            performMasonryPlacement(GridTrackSizingDirection::ForRows);
        else if (areMasonryColumns())
            performMasonryPlacement(GridTrackSizingDirection::ForColumns);

        LayoutUnit trackBasedLogicalHeight = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (auto size = explicitIntrinsicInnerLogicalSize(GridTrackSizingDirection::ForRows))
            trackBasedLogicalHeight += size.value();
        else {
            if (areMasonryRows())
                trackBasedLogicalHeight += m_masonryLayout.gridContentSize();
            else
                trackBasedLogicalHeight += m_trackSizingAlgorithm.computeTrackBasedSize();
        }
        if (shouldRecomputeHeight)
            computeTrackSizesForDefiniteSize(GridTrackSizingDirection::ForRows, trackBasedLogicalHeight, gridLayoutState);

        setLogicalHeight(trackBasedLogicalHeight);

        updateLogicalHeight();

        // Once grid's indefinite height is resolved, we can compute the
        // available free space for Content Alignment.
        if (!hasDefiniteLogicalHeight || areMasonryRows())
            m_trackSizingAlgorithm.setFreeSpace(GridTrackSizingDirection::ForRows, logicalHeight() - trackBasedLogicalHeight);

        // 2.5. Compute Content Distribution offsets for rows tracks
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(GridTrackSizingDirection::ForRows, m_trackSizingAlgorithm.freeSpace(GridTrackSizingDirection::ForRows).value(), nonCollapsedTracks(GridTrackSizingDirection::ForRows));

        if (!aspectRatioBlockSizeDependentGridItems.isEmpty()) {
            updateGridAreaForAspectRatioItems(aspectRatioBlockSizeDependentGridItems, gridLayoutState);
            updateLogicalWidth();
        }

        // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
        if (hasLineIfEmpty()) {
            LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
                + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
                + scrollbarLogicalHeight();
            setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
        }

        layoutMasonryItems(gridLayoutState);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (size() != previousSize)
            relayoutChildren = true;

        m_outOfFlowItemColumn.clear();
        m_outOfFlowItemRow.clear();

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());
        m_trackSizingAlgorithm.reset();

        computeOverflow(layoutOverflowLogicalBottom(*this));

        updateDescendantTransformsAfterLayout();
    }

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();

    m_trackSizingAlgorithm.clearBaselineItemsCache();
    m_baselineItemsCached = false;
}

LayoutUnit RenderGrid::gridGap(GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    ASSERT(!availableSize || *availableSize >= 0);
    const GapLength& gapLength = direction == GridTrackSizingDirection::ForColumns? style().columnGap() : style().rowGap();
    if (gapLength.isNormal()) {
        if (!isSubgrid(direction))
            return 0_lu;

        GridTrackSizingDirection parentDirection = GridLayoutFunctions::flowAwareDirectionForParent(*this, *parent(), direction);
        if (!availableSize)
            return downcast<RenderGrid>(parent())->gridGap(parentDirection, std::nullopt);
        return downcast<RenderGrid>(parent())->gridGap(parentDirection);
    }

    return valueForLength(gapLength.length(), availableSize.value_or(0));
}

LayoutUnit RenderGrid::gridGap(GridTrackSizingDirection direction) const
{
    return gridGap(direction, availableSpaceForGutters(direction));
}

LayoutUnit RenderGrid::gridItemOffset(GridTrackSizingDirection direction) const
{
    return direction == GridTrackSizingDirection::ForRows ? m_offsetBetweenRows.distributionOffset : m_offsetBetweenColumns.distributionOffset;
}

LayoutUnit RenderGrid::guttersSize(GridTrackSizingDirection direction, unsigned startLine, unsigned span, std::optional<LayoutUnit> availableSize) const
{
    if (span <= 1)
        return { };

    LayoutUnit gap = gridGap(direction, availableSize);

    // Fast path, no collapsing tracks.
    if (!currentGrid().hasAutoRepeatEmptyTracks(direction))
        return gap * (span - 1);

    // If there are collapsing tracks we need to be sure that gutters are properly collapsed. Apart
    // from that, if we have a collapsed track in the edges of the span we're considering, we need
    // to move forward (or backwards) in order to know whether the collapsed tracks reach the end of
    // the grid (so the gap becomes 0) or there is a non empty track before that.

    LayoutUnit gapAccumulator;
    unsigned endLine = startLine + span;

    for (unsigned line = startLine; line < endLine - 1; ++line) {
        if (!currentGrid().isEmptyAutoRepeatTrack(direction, line))
            gapAccumulator += gap;
    }

    // The above loop adds one extra gap for trailing collapsed tracks.
    if (gapAccumulator && currentGrid().isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        ASSERT(gapAccumulator >= gap);
        gapAccumulator -= gap;
    }

    // If the startLine is the start line of a collapsed track we need to go backwards till we reach
    // a non collapsed track. If we find a non collapsed track we need to add that gap.
    size_t nonEmptyTracksBeforeStartLine = 0;
    if (startLine && currentGrid().isEmptyAutoRepeatTrack(direction, startLine)) {
        nonEmptyTracksBeforeStartLine = startLine;
        auto begin = currentGrid().autoRepeatEmptyTracks(direction)->begin();
        for (auto it = begin; *it != startLine; ++it) {
            ASSERT(nonEmptyTracksBeforeStartLine);
            --nonEmptyTracksBeforeStartLine;
        }
        if (nonEmptyTracksBeforeStartLine)
            gapAccumulator += gap;
    }

    // If the endLine is the end line of a collapsed track we need to go forward till we reach a non
    // collapsed track. If we find a non collapsed track we need to add that gap.
    if (currentGrid().isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        unsigned nonEmptyTracksAfterEndLine = currentGrid().numTracks(direction) - endLine;
        auto currentEmptyTrack = currentGrid().autoRepeatEmptyTracks(direction)->find(endLine - 1);
        auto endEmptyTrack = currentGrid().autoRepeatEmptyTracks(direction)->end();
        // HashSet iterators do not implement operator- so we have to manually iterate to know the number of remaining empty tracks.
        for (auto it = ++currentEmptyTrack; it != endEmptyTrack; ++it) {
            ASSERT(nonEmptyTracksAfterEndLine >= 1);
            --nonEmptyTracksAfterEndLine;
        }
        if (nonEmptyTracksAfterEndLine) {
            // We shouldn't count the gap twice if the span starts and ends in a collapsed track between two non-empty tracks.
            if (!nonEmptyTracksBeforeStartLine)
                gapAccumulator += gap;
        } else if (nonEmptyTracksBeforeStartLine) {
            // We shouldn't count the gap if the span starts and ends in a collapsed but there isn't non-empty tracks afterwards (it's at the end of the grid).
            gapAccumulator -= gap;
        }
    }

    return gapAccumulator;
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    GridLayoutState gridLayoutState;

    LayoutUnit gridItemMinWidth;
    LayoutUnit gridItemMaxWidth;
    bool hadExcludedChildren = computePreferredWidthsForExcludedChildren(gridItemMinWidth, gridItemMaxWidth);

    Grid grid(const_cast<RenderGrid&>(*this));
    m_grid.m_currentGrid = std::ref(grid);
    GridTrackSizingAlgorithm algorithm(this, grid);
    // placeItemsOnGrid isn't const since it mutates our grid, but it's safe to do
    // so here since we've overridden m_currentGrid with a stack based temporary.
    const_cast<RenderGrid&>(*this).placeItemsOnGrid(std::nullopt);

    performPreLayoutForGridItems(algorithm, ShouldUpdateGridAreaLogicalSize::No);

    if (m_baselineItemsCached)
        algorithm.copyBaselineItemsCache(m_trackSizingAlgorithm, GridAxis::GridRowAxis);
    else {
        auto emptyCallback = [](RenderBox*) { };
        cacheBaselineAlignedGridItems(*this, algorithm, enumToUnderlyingType(GridAxis::GridRowAxis), emptyCallback, !isSubgridRows());
    }

    computeTrackSizesForIndefiniteSize(algorithm, GridTrackSizingDirection::ForColumns, gridLayoutState, &minLogicalWidth, &maxLogicalWidth);

    if (isMasonry(GridTrackSizingDirection::ForColumns)) {
        // The track sizing algorithm will only be run once in this case, since track sizing will not run in the masonry direction.
        computeTrackSizesForIndefiniteSize(algorithm, GridTrackSizingDirection::ForRows, gridLayoutState, &minLogicalWidth, &maxLogicalWidth);

        auto gridAxisTracksCountBeforeAutoPlacement = currentGrid().numTracks(GridTrackSizingDirection::ForRows);

        // To determine the width of the grid when we have a masonry layout in the column direction we need to perform a layout with the min and max
        // conent sizes. We will override the grid items widths to accomplish this and then calculate the final grid content size after placement.

        for (auto* gridItem = grid.orderIterator().first(); gridItem; gridItem = grid.orderIterator().next()) {
            if (grid.orderIterator().shouldSkipChild(*gridItem))
                continue;

            if (gridItem->style().logicalWidth().isAuto() || gridItem->style().logicalWidth().isPercent())
                gridItem->setOverridingLogicalWidth(gridItem->computeIntrinsicLogicalWidthUsing(Length(LengthType::MinContent), LayoutUnit(), gridItem->borderAndPaddingLogicalWidth()));
        }

        m_masonryLayout.performMasonryPlacement(algorithm, gridAxisTracksCountBeforeAutoPlacement, GridTrackSizingDirection::ForColumns);

        minLogicalWidth = m_masonryLayout.gridContentSize();

        for (auto* gridItem = grid.orderIterator().first(); gridItem; gridItem = grid.orderIterator().next()) {
            if (grid.orderIterator().shouldSkipChild(*gridItem))
                continue;

            if (gridItem->style().logicalWidth().isAuto() || gridItem->style().logicalWidth().isPercent())
                gridItem->setOverridingLogicalWidth(gridItem->computeIntrinsicLogicalWidthUsing(Length(LengthType::MaxContent), LayoutUnit(), gridItem->borderAndPaddingLogicalWidth()));
        }

        m_masonryLayout.performMasonryPlacement(algorithm, gridAxisTracksCountBeforeAutoPlacement, GridTrackSizingDirection::ForColumns);

        maxLogicalWidth = m_masonryLayout.gridContentSize();
    }

    m_grid.resetCurrentGrid();

    if (hadExcludedChildren) {
        minLogicalWidth = std::max(minLogicalWidth, gridItemMinWidth);
        maxLogicalWidth = std::max(maxLogicalWidth, gridItemMaxWidth);
    }

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidthIncludingGutter();
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;
}

void RenderGrid::computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm& algorithm, GridTrackSizingDirection direction, GridLayoutState& gridLayoutState, LayoutUnit* minIntrinsicSize, LayoutUnit* maxIntrinsicSize) const
{
    algorithm.run(direction, numTracks(direction), SizingOperation::IntrinsicSizeComputation, std::nullopt, gridLayoutState);

    size_t numberOfTracks = algorithm.tracks(direction).size();
    LayoutUnit totalGuttersSize = direction == GridTrackSizingDirection::ForColumns && explicitIntrinsicInnerLogicalSize(direction).has_value() ? 0_lu : guttersSize(direction, 0, numberOfTracks, std::nullopt);

    if (minIntrinsicSize)
        *minIntrinsicSize = algorithm.minContentSize() + totalGuttersSize;
    if (maxIntrinsicSize)
        *maxIntrinsicSize = algorithm.maxContentSize() + totalGuttersSize;

    ASSERT(algorithm.tracksAreWiderThanMinTrackBreadth());
}

bool RenderGrid::shouldCheckExplicitIntrinsicInnerLogicalSize(GridTrackSizingDirection direction) const
{
    return direction == GridTrackSizingDirection::ForColumns ? shouldApplySizeOrInlineSizeContainment() : shouldApplySizeContainment();
}

std::optional<LayoutUnit> RenderGrid::explicitIntrinsicInnerLogicalSize(GridTrackSizingDirection direction) const
{
    if (!shouldCheckExplicitIntrinsicInnerLogicalSize(direction))
        return std::nullopt;
    if (direction == GridTrackSizingDirection::ForColumns)
        return explicitIntrinsicInnerLogicalWidth();
    return explicitIntrinsicInnerLogicalHeight();
}

unsigned RenderGrid::computeAutoRepeatTracksCount(GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    ASSERT(!availableSize || availableSize.value() != -1);
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    if (isSubgrid(direction))
        return 0;

    const auto& autoRepeatTracks = isRowAxis ? style().gridAutoRepeatColumns() : style().gridAutoRepeatRows();
    unsigned autoRepeatTrackListLength = autoRepeatTracks.size();

    if (!autoRepeatTrackListLength)
        return 0;

    bool needsToFulfillMinimumSize = false;
    if (!availableSize) {
        const Length& maxSize = isRowAxis ? style().logicalMaxWidth() : style().logicalMaxHeight();
        std::optional<LayoutUnit> containingBlockAvailableSize;
        std::optional<LayoutUnit> availableMaxSize;
        if (maxSize.isSpecified()) {
            if (maxSize.isPercentOrCalculated())
                containingBlockAvailableSize = isRowAxis ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
            LayoutUnit maxSizeValue = valueForLength(maxSize, valueOrDefault(containingBlockAvailableSize));
            availableMaxSize = isRowAxis ? adjustContentBoxLogicalWidthForBoxSizing(maxSizeValue, maxSize.type()) : adjustContentBoxLogicalHeightForBoxSizing(maxSizeValue);
        }

        const Length& minSize = isRowAxis ? style().logicalMinWidth() : style().logicalMinHeight();
        const auto& minSizeForOrthogonalAxis = isRowAxis ? style().logicalMinHeight() : style().logicalMinWidth();
        bool shouldComputeMinSizeFromAspectRatio = minSizeForOrthogonalAxis.isSpecified() && !shouldIgnoreAspectRatio();
        auto explicitIntrinsicInnerSize = explicitIntrinsicInnerLogicalSize(direction);

        if (!availableMaxSize && !minSize.isSpecified() && !shouldComputeMinSizeFromAspectRatio && !explicitIntrinsicInnerSize)
            return autoRepeatTrackListLength;

        std::optional<LayoutUnit> availableMinSize;
        if (minSize.isSpecified()) {
            if (!containingBlockAvailableSize && minSize.isPercentOrCalculated())
                containingBlockAvailableSize = isRowAxis ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
            LayoutUnit minSizeValue = valueForLength(minSize, valueOrDefault(containingBlockAvailableSize));
            availableMinSize = isRowAxis ? adjustContentBoxLogicalWidthForBoxSizing(minSizeValue, minSize.type()) : adjustContentBoxLogicalHeightForBoxSizing(minSizeValue);
        } else if (shouldComputeMinSizeFromAspectRatio) {
            auto [logicalMinWidth, logicalMaxWidth] = computeMinMaxLogicalWidthFromAspectRatio();
            availableMinSize = logicalMinWidth;
        }
        if (!maxSize.isSpecified() || explicitIntrinsicInnerSize)
            needsToFulfillMinimumSize = true;

        availableSize = std::max(std::max(valueOrDefault(availableMinSize), valueOrDefault(availableMaxSize)), valueOrDefault(explicitIntrinsicInnerSize));
        if (maxSize.isSpecified() && availableMaxSize < availableSize)
            availableSize = std::max(availableMinSize, availableMaxSize);
    }

    LayoutUnit autoRepeatTracksSize;
    for (auto& autoTrackSize : autoRepeatTracks) {
        ASSERT(autoTrackSize.minTrackBreadth().isLength());
        ASSERT(!autoTrackSize.minTrackBreadth().isFlex());
        bool hasDefiniteMaxTrackSizingFunction = autoTrackSize.maxTrackBreadth().isLength() && !autoTrackSize.maxTrackBreadth().isContentSized();
        auto trackLength = hasDefiniteMaxTrackSizingFunction ? autoTrackSize.maxTrackBreadth().length() : autoTrackSize.minTrackBreadth().length();
        bool hasDefiniteMinTrackSizingFunction = autoTrackSize.minTrackBreadth().isLength() && !autoTrackSize.minTrackBreadth().isContentSized();
        if (hasDefiniteMinTrackSizingFunction && (trackLength.value() < autoTrackSize.minTrackBreadth().length().value()))
            trackLength = autoTrackSize.minTrackBreadth().length();
        autoRepeatTracksSize += valueForLength(trackLength, availableSize.value());
    }
    // For the purpose of finding the number of auto-repeated tracks, the UA must floor the track size to a UA-specified
    // value to avoid division by zero. It is suggested that this floor be 1px.
    autoRepeatTracksSize = std::max<LayoutUnit>(1_lu, autoRepeatTracksSize);

    // There will be always at least 1 auto-repeat track, so take it already into account when computing the total track size.
    LayoutUnit tracksSize = autoRepeatTracksSize;
    auto& trackSizes = isRowAxis ? style().gridColumnTrackSizes() : style().gridRowTrackSizes();

    for (const auto& track : trackSizes) {
        bool hasDefiniteMaxTrackBreadth = track.maxTrackBreadth().isLength() && !track.maxTrackBreadth().isContentSized();
        ASSERT(hasDefiniteMaxTrackBreadth || (track.minTrackBreadth().isLength() && !track.minTrackBreadth().isContentSized()));
        tracksSize += valueForLength(hasDefiniteMaxTrackBreadth ? track.maxTrackBreadth().length() : track.minTrackBreadth().length(), availableSize.value());
    }

    // Add gutters as if auto repeat tracks were only repeated once. Gaps between different repetitions will be added later when
    // computing the number of repetitions of the auto repeat().
    LayoutUnit gapSize = gridGap(direction, availableSize);
    tracksSize += gapSize * (trackSizes.size() + autoRepeatTrackListLength - 1);

    LayoutUnit freeSpace = availableSize.value() - tracksSize;
    if (freeSpace <= 0)
        return autoRepeatTrackListLength;

    LayoutUnit autoRepeatSizeWithGap = autoRepeatTracksSize + gapSize * autoRepeatTrackListLength;
    unsigned repetitions = 1 + (freeSpace / autoRepeatSizeWithGap).toUnsigned();
    freeSpace -= autoRepeatSizeWithGap * (repetitions - 1);
    ASSERT(freeSpace >= 0);

    // Provided the grid container does not have a definite size or max-size in the relevant axis,
    // if the min size is definite then the number of repetitions is the largest possible positive
    // integer that fulfills that minimum requirement.
    if (needsToFulfillMinimumSize && freeSpace)
        ++repetitions;

    return repetitions * autoRepeatTrackListLength;
}


std::unique_ptr<OrderedTrackIndexSet> RenderGrid::computeEmptyTracksForAutoRepeat(GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    if ((isRowAxis && autoRepeatColumnsType() != AutoRepeatType::Fit)
        || (!isRowAxis && autoRepeatRowsType() != AutoRepeatType::Fit))
        return nullptr;

    std::unique_ptr<OrderedTrackIndexSet> emptyTrackIndexes;
    unsigned insertionPoint = isRowAxis ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned firstAutoRepeatTrack = insertionPoint + currentGrid().explicitGridStart(direction);
    unsigned lastAutoRepeatTrack = firstAutoRepeatTrack + currentGrid().autoRepeatTracks(direction);

    if (!currentGrid().hasGridItems() || (shouldCheckExplicitIntrinsicInnerLogicalSize(direction) && !explicitIntrinsicInnerLogicalSize(direction))) {
        emptyTrackIndexes = makeUnique<OrderedTrackIndexSet>();
        for (unsigned trackIndex = firstAutoRepeatTrack; trackIndex < lastAutoRepeatTrack; ++trackIndex)
            emptyTrackIndexes->add(trackIndex);
    } else {
        for (unsigned trackIndex = firstAutoRepeatTrack; trackIndex < lastAutoRepeatTrack; ++trackIndex) {
            GridIterator iterator(currentGrid(), direction, trackIndex);
            if (!iterator.nextGridItem()) {
                if (!emptyTrackIndexes)
                    emptyTrackIndexes = makeUnique<OrderedTrackIndexSet>();
                emptyTrackIndexes->add(trackIndex);
            }
        }
    }
    return emptyTrackIndexes;
}

unsigned RenderGrid::clampAutoRepeatTracks(GridTrackSizingDirection direction, unsigned autoRepeatTracks) const
{
    if (!autoRepeatTracks)
        return 0;

    unsigned insertionPoint = direction == GridTrackSizingDirection::ForColumns ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned maxTracks = static_cast<unsigned>(GridPosition::max());

    if (!insertionPoint)
        return std::min(autoRepeatTracks, maxTracks);

    if (insertionPoint >= maxTracks)
        return 0;

    return std::min(autoRepeatTracks, maxTracks - insertionPoint);
}

void RenderGrid::placeItems()
{
    updateLogicalWidth();

    LayoutUnit availableSpaceForColumns = availableLogicalWidth();
    placeItemsOnGrid(availableSpaceForColumns);
}

static GridArea insertIntoGrid(Grid& grid, RenderBox& gridItem, const GridArea& area)
{
    GridArea clamped = grid.insert(gridItem, area);

    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (!renderGrid)
        return clamped;

    if (renderGrid->isSubgridRows() || renderGrid->isSubgridColumns())
        renderGrid->placeItems();
    return clamped;
}

bool RenderGrid::isMasonry() const
{
    return areMasonryRows() || areMasonryColumns();
}

bool RenderGrid::isMasonry(GridTrackSizingDirection direction) const
{
    if (areMasonryRows() && direction == GridTrackSizingDirection::ForRows)
        return true;

    if (areMasonryColumns() && direction == GridTrackSizingDirection::ForColumns)
        return true;

    return false;
}

bool RenderGrid::areMasonryRows() const
{
    // isSubgridRows will return false if the masonry axis is rows. Need to check style if we are a subgrid
    if (auto* parentGrid = dynamicDowncast<RenderGrid>(parent()); parentGrid && style().gridSubgridRows())
        return parentGrid->areMasonryRows();
    return style().gridMasonryRows();
}

// Masonry Spec Section 2
// "If masonry is specified for both grid-template-columns and grid-template-rows, then the used value for grid-template-columns is none,
// and thus the inline axis will be the grid axis."
bool RenderGrid::areMasonryColumns() const
{
    // isSubgridColumns will return false if the masonry axis is columns. Need to check style if we are a subgrid
    if (auto* parentGrid = dynamicDowncast<RenderGrid>(parent()); parentGrid && style().gridSubgridColumns())
        return parentGrid->areMasonryColumns();
    return !areMasonryRows() && style().gridMasonryColumns();
}

// Masonry Spec Section 2.3.1 repeat(auto-fit)
// "repeat(auto-fit) behaves as repeat(auto-fill) when the other axis is a masonry axis."
// We need to lie here that we are really an auto-fill instead of an auto-fit.
AutoRepeatType RenderGrid::autoRepeatColumnsType() const
{
    auto autoRepeatColumns = style().gridAutoRepeatColumnsType();

    if (areMasonryRows() && autoRepeatColumns == AutoRepeatType::Fit)
        return AutoRepeatType::Fill;

    return autoRepeatColumns;
}

AutoRepeatType RenderGrid::autoRepeatRowsType() const
{
    auto autoRepeatRow = style().gridAutoRepeatRowsType();

    if (areMasonryColumns() && autoRepeatRow == AutoRepeatType::Fit)
        return AutoRepeatType::Fill;

    return autoRepeatRow;
}


// FIXME: We shouldn't have to pass the available logical width as argument. The problem is that
// availableLogicalWidth() does always return a value even if we cannot resolve it like when
// computing the intrinsic size (preferred widths). That's why we pass the responsibility to the
// caller who does know whether the available logical width is indefinite or not.
void RenderGrid::placeItemsOnGrid(std::optional<LayoutUnit> availableLogicalWidth)
{
    unsigned autoRepeatColumns = computeAutoRepeatTracksCount(GridTrackSizingDirection::ForColumns, availableLogicalWidth);
    unsigned autoRepeatRows = computeAutoRepeatTracksCount(GridTrackSizingDirection::ForRows, availableLogicalHeightForPercentageComputation());
    autoRepeatRows = clampAutoRepeatTracks(GridTrackSizingDirection::ForRows, autoRepeatRows);
    autoRepeatColumns = clampAutoRepeatTracks(GridTrackSizingDirection::ForColumns, autoRepeatColumns);

    if (isSubgridInParentDirection(GridTrackSizingDirection::ForColumns) || isSubgridInParentDirection(GridTrackSizingDirection::ForRows)) {
        auto* parent = dynamicDowncast<RenderGrid>(this->parent());
        if (parent && parent->currentGrid().needsItemsPlacement())
            currentGrid().setNeedsItemsPlacement(true);
    }

    if (autoRepeatColumns != currentGrid().autoRepeatTracks(GridTrackSizingDirection::ForColumns)
        || autoRepeatRows != currentGrid().autoRepeatTracks(GridTrackSizingDirection::ForRows)
        || isMasonry()) {
        currentGrid().setNeedsItemsPlacement(true);
        currentGrid().setAutoRepeatTracks(autoRepeatRows, autoRepeatColumns);
    }

    if (!currentGrid().needsItemsPlacement())
        return;

    ASSERT(!currentGrid().hasGridItems());
    populateExplicitGridAndOrderIterator();

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    for (auto* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next()) {
        if (currentGrid().orderIterator().shouldSkipChild(*gridItem))
            continue;

        // Grid items should use the grid area sizes instead of the containing block (grid container)
        // sizes, we initialize the overrides here if needed to ensure it.
        if (!gridItem->overridingContainingBlockContentLogicalWidth())
            gridItem->setOverridingContainingBlockContentLogicalWidth(0_lu);
        if (!gridItem->overridingContainingBlockContentLogicalHeight())
            gridItem->setOverridingContainingBlockContentLogicalHeight(std::nullopt);

        GridArea area = currentGrid().gridItemArea(*gridItem);
        currentGrid().clampAreaToSubgridIfNeeded(area);
        if (!area.rows.isIndefinite())
            area.rows.translate(currentGrid().explicitGridStart(GridTrackSizingDirection::ForRows));
        if (!area.columns.isIndefinite())
            area.columns.translate(currentGrid().explicitGridStart(GridTrackSizingDirection::ForColumns));

        if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
            currentGrid().setGridItemArea(*gridItem, area);
            bool majorAxisDirectionIsForColumns = autoPlacementMajorAxisDirection() == GridTrackSizingDirection::ForColumns;
            if ((majorAxisDirectionIsForColumns && area.columns.isIndefinite())
                || (!majorAxisDirectionIsForColumns && area.rows.isIndefinite()))
                autoMajorAxisAutoGridItems.append(gridItem);
            else
                specifiedMajorAxisAutoGridItems.append(gridItem);
            continue;
        }
        insertIntoGrid(currentGrid(), *gridItem, { area.rows, area.columns });
    }

#if ASSERT_ENABLED
    if (currentGrid().hasGridItems()) {
        ASSERT(currentGrid().numTracks(GridTrackSizingDirection::ForRows) >= GridPositionsResolver::explicitGridRowCount(*this));
        ASSERT(currentGrid().numTracks(GridTrackSizingDirection::ForColumns) >= GridPositionsResolver::explicitGridColumnCount(*this));
    }
#endif

    auto performAutoPlacement = [&]() {
        placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
        placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
        // Compute collapsible tracks for auto-fit.
        currentGrid().setAutoRepeatEmptyColumns(computeEmptyTracksForAutoRepeat(GridTrackSizingDirection::ForColumns));
        currentGrid().setAutoRepeatEmptyRows(computeEmptyTracksForAutoRepeat(GridTrackSizingDirection::ForRows));

        currentGrid().setNeedsItemsPlacement(false);
    };

    performAutoPlacement();

#if ASSERT_ENABLED
    for (auto* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next()) {
        if (currentGrid().orderIterator().shouldSkipChild(*gridItem))
            continue;

        GridArea area = currentGrid().gridItemArea(*gridItem);
        ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    }
#endif
}

LayoutUnit RenderGrid::masonryContentSize() const
{
    return m_masonryLayout.gridContentSize();
}

Vector<LayoutRect> RenderGrid::gridItemsLayoutRects()
{
    Vector<LayoutRect> items;

    for (RenderBox* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next())
        items.append(gridItem->frameRect());

    return items;
}

void RenderGrid::performPreLayoutForGridItems(const GridTrackSizingAlgorithm& algorithm, const ShouldUpdateGridAreaLogicalSize shouldUpdateGridAreaLogicalSize) const
{
    ASSERT(!algorithm.grid().needsItemsPlacement());
    // FIXME: We need a way when we are calling this during intrinsic size computation before performing
    // the layout. Maybe using the PreLayout phase ?
    for (auto* gridItem = firstChildBox(); gridItem; gridItem = gridItem->nextSiblingBox()) {
        if (gridItem->isOutOfFlowPositioned())
            continue;
        // Orthogonal items should be laid out in order to properly compute content-sized tracks that may depend on item's intrinsic size.
        // We also need to properly estimate its grid area size, since it may affect to the baseline shims if such item participates in baseline alignment.
        if (GridLayoutFunctions::isOrthogonalGridItem(*this, *gridItem)) {
            updateGridAreaLogicalSize(*gridItem, algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, GridTrackSizingDirection::ForColumns), algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, GridTrackSizingDirection::ForRows));
            gridItem->layoutIfNeeded();
            continue;
        }
        // We need to layout the item to know whether it must synthesize its
        // baseline or not, which may imply a cyclic sizing dependency.
        // FIXME: Can we avoid it ?
        // FIXME: We also want to layout baseline aligned items within subgrids, but
        // we don't currently have a way to do that here.
        if (isBaselineAlignmentForGridItem(*gridItem)) {
            // FIXME: Hack to fix nested grid text size overflow during re-layouts.
            if (shouldUpdateGridAreaLogicalSize == ShouldUpdateGridAreaLogicalSize::Yes)
                updateGridAreaLogicalSize(*gridItem, algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, GridTrackSizingDirection::ForColumns), algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, GridTrackSizingDirection::ForRows));
            gridItem->layoutIfNeeded();
        }
    }
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(currentGrid().orderIterator());
    unsigned explicitRowStart = 0;
    unsigned explicitColumnStart = 0;
    unsigned maximumRowIndex = GridPositionsResolver::explicitGridRowCount(*this);
    unsigned maximumColumnIndex = GridPositionsResolver::explicitGridColumnCount(*this);

    for (RenderBox* gridItem = firstChildBox(); gridItem; gridItem = gridItem->nextSiblingBox()) {
        if (!populator.collectChild(*gridItem))
            continue;

        GridSpan rowPositions = GridPositionsResolver::resolveGridPositionsFromStyle(*this, *gridItem, GridTrackSizingDirection::ForRows);
        if (!isSubgridRows()) {
            if (!rowPositions.isIndefinite()) {
                explicitRowStart = std::max<int>(explicitRowStart, -rowPositions.untranslatedStartLine());
                maximumRowIndex = std::max<int>(maximumRowIndex, rowPositions.untranslatedEndLine());
            } else {
                // Grow the grid for items with a definite row span, getting the largest such span.
                unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*gridItem, GridTrackSizingDirection::ForRows);
                maximumRowIndex = std::max(maximumRowIndex, spanSize);
            }
        }

        GridSpan columnPositions = GridPositionsResolver::resolveGridPositionsFromStyle(*this, *gridItem, GridTrackSizingDirection::ForColumns);
        if (!isSubgridColumns()) {
            if (!columnPositions.isIndefinite()) {
                explicitColumnStart = std::max<int>(explicitColumnStart, -columnPositions.untranslatedStartLine());
                maximumColumnIndex = std::max<int>(maximumColumnIndex, columnPositions.untranslatedEndLine());
            } else {
                // Grow the grid for items with a definite column span, getting the largest such span.
                unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*gridItem, GridTrackSizingDirection::ForColumns);
                maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
            }
        }

        currentGrid().setGridItemArea(*gridItem, { rowPositions, columnPositions });
    }

    currentGrid().setExplicitGridStart(explicitRowStart, explicitColumnStart);
    currentGrid().ensureGridSize(maximumRowIndex + explicitRowStart, maximumColumnIndex + explicitColumnStart);
    currentGrid().setClampingForSubgrid(isSubgridRows() ? maximumRowIndex : 0, isSubgridColumns() ? maximumColumnIndex : 0);
}

GridArea RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == GridTrackSizingDirection::ForColumns ? GridTrackSizingDirection::ForRows : GridTrackSizingDirection::ForColumns;
    const unsigned endOfCrossDirection = currentGrid().numTracks(crossDirection);
    unsigned crossDirectionSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, crossDirection);
    GridSpan crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);
    return { specifiedDirection == GridTrackSizingDirection::ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == GridTrackSizingDirection::ForColumns ? specifiedPositions : crossDirectionPositions };
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    bool isForColumns = autoPlacementMajorAxisDirection() == GridTrackSizingDirection::ForColumns;
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    // Mapping between the major axis tracks (rows or columns) and the last auto-placed item's position inserted on
    // that track. This is needed to implement "sparse" packing for items locked to a given track.
    // See https://drafts.csswg.org/css-grid-2/#auto-placement-algo
    UncheckedKeyHashMap<unsigned, unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> minorAxisCursors;

    for (auto& autoGridItem : autoGridItems) {
        GridSpan majorAxisPositions = currentGrid().gridItemSpan(*autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(majorAxisPositions.isTranslatedDefinite());
        ASSERT(currentGrid().gridItemSpan(*autoGridItem, autoPlacementMinorAxisDirection()).isIndefinite());
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(*autoGridItem, autoPlacementMinorAxisDirection());
        unsigned majorAxisInitialPosition = majorAxisPositions.startLine();

        GridIterator iterator(currentGrid(), autoPlacementMajorAxisDirection(), majorAxisPositions.startLine(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        auto emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisSpanSize);
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), majorAxisPositions);

        emptyGridArea = insertIntoGrid(currentGrid(), *autoGridItem, *emptyGridArea);

        if (!isGridAutoFlowDense)
            minorAxisCursors.set(majorAxisInitialPosition, isForColumns ? emptyGridArea->rows.startLine() : emptyGridArea->columns.startLine());
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    AutoPlacementCursor autoPlacementCursor = {0, 0};
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    for (auto& autoGridItem : autoGridItems) {
        placeAutoMajorAxisItemOnGrid(*autoGridItem, autoPlacementCursor);

        if (isGridAutoFlowDense) {
            autoPlacementCursor.first = 0;
            autoPlacementCursor.second = 0;
        }
    }
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(RenderBox& gridItem, AutoPlacementCursor& autoPlacementCursor)
{
    ASSERT(currentGrid().gridItemSpan(gridItem, autoPlacementMajorAxisDirection()).isIndefinite());
    unsigned majorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, autoPlacementMajorAxisDirection());

    const unsigned endOfMajorAxis = currentGrid().numTracks(autoPlacementMajorAxisDirection());
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == GridTrackSizingDirection::ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == GridTrackSizingDirection::ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    auto emptyGridArea = std::optional<GridArea> { };
    GridSpan minorAxisPositions = currentGrid().gridItemSpan(gridItem, autoPlacementMinorAxisDirection());
    if (minorAxisPositions.isTranslatedDefinite()) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions.startLine() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(currentGrid(), autoPlacementMinorAxisDirection(), minorAxisPositions.startLine(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions.integerSpan(), majorAxisSpanSize);
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    } else {
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, autoPlacementMinorAxisDirection());

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(currentGrid(), autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                unsigned minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == GridTrackSizingDirection::ForColumns ? emptyGridArea->columns.endLine() : emptyGridArea->rows.endLine();
                const unsigned endOfMinorAxis = currentGrid().numTracks(autoPlacementMinorAxisDirection());
                if (minorAxisFinalPositionIndex <= endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = { };
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), GridSpan::translatedDefiniteGridSpan(0, minorAxisSpanSize));
    }

    emptyGridArea = insertIntoGrid(currentGrid(), gridItem, *emptyGridArea);
    autoPlacementCursor.first = emptyGridArea->rows.startLine();
    autoPlacementCursor.second = emptyGridArea->columns.startLine();
}

GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    if (areMasonryColumns())
        return GridTrackSizingDirection::ForColumns;
    if (areMasonryRows())
        return GridTrackSizingDirection::ForRows;

    return style().isGridAutoFlowDirectionColumn() ? GridTrackSizingDirection::ForColumns : GridTrackSizingDirection::ForRows;
}

GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    return (autoPlacementMajorAxisDirection() == GridTrackSizingDirection::ForColumns) ? GridTrackSizingDirection::ForRows : GridTrackSizingDirection::ForColumns;
}

void RenderGrid::dirtyGrid(bool subgridChanged)
{
    if (currentGrid().needsItemsPlacement())
        return;

    currentGrid().setNeedsItemsPlacement(true);

    auto currentChild = this;
    while (currentChild && (subgridChanged || currentChild->isSubgridRows() || currentChild->isSubgridColumns())) {
        currentChild = dynamicDowncast<RenderGrid>(currentChild->parent());
        if (currentChild)
            currentChild->currentGrid().setNeedsItemsPlacement(true);
        subgridChanged = false;
    }
}

Vector<LayoutUnit> RenderGrid::trackSizesForComputedStyle(GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    size_t numPositions = positions.size();
    LayoutUnit offsetBetweenTracks = isRowAxis ? m_offsetBetweenColumns.distributionOffset : m_offsetBetweenRows.distributionOffset;

    Vector<LayoutUnit> tracks;
    if (numPositions < 2)
        return tracks;

    ASSERT(!currentGrid().needsItemsPlacement());
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    LayoutUnit gap = !hasCollapsedTracks ? gridGap(direction) : 0_lu;
    tracks.reserveInitialCapacity(numPositions - 1);
    tracks.appendUsingFunctor(numPositions - 2, [&](size_t i) {
        return positions[i + 1] - positions[i] - offsetBetweenTracks - gap;
    });
    tracks.append(positions[numPositions - 1] - positions[numPositions - 2]);

    if (!hasCollapsedTracks)
        return tracks;

    size_t remainingEmptyTracks = currentGrid().autoRepeatEmptyTracks(direction)->size();
    size_t lastLine = tracks.size();
    gap = gridGap(direction);
    for (size_t i = 1; i < lastLine; ++i) {
        if (currentGrid().isEmptyAutoRepeatTrack(direction, i - 1))
            --remainingEmptyTracks;
        else {
            // Remove the gap between consecutive non empty tracks. Remove it also just once for an
            // arbitrary number of empty tracks between two non empty ones.
            bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
            if (!allRemainingTracksAreEmpty || !currentGrid().isEmptyAutoRepeatTrack(direction, i))
                tracks[i - 1] -= gap;
        }
    }

    return tracks;
}

static const StyleContentAlignmentData& contentAlignmentNormalBehaviorGrid()
{
    static const StyleContentAlignmentData normalBehavior = {ContentPosition::Normal, ContentDistribution::Stretch};
    return normalBehavior;
}

static bool overrideSizeChanged(const RenderBox& gridItem, GridTrackSizingDirection direction, std::optional<LayoutUnit> width, std::optional<LayoutUnit> height)
{
    if (direction == GridTrackSizingDirection::ForColumns) {
        if (auto overridingContainingBlockContentLogicalWidth = gridItem.overridingContainingBlockContentLogicalWidth())
            return *overridingContainingBlockContentLogicalWidth != width;
        return true;
    }
    if (auto overridingContainingBlockContentLogicalHeight = gridItem.overridingContainingBlockContentLogicalHeight())
        return *overridingContainingBlockContentLogicalHeight != height;
    return true;
}

static bool hasRelativeBlockAxisSize(const RenderGrid& grid, const RenderBox& gridItem)
{
    return GridLayoutFunctions::isOrthogonalGridItem(grid, gridItem) ? gridItem.hasRelativeLogicalWidth() || gridItem.style().logicalWidth().isAuto() : gridItem.hasRelativeLogicalHeight();
}

void RenderGrid::updateGridAreaLogicalSize(RenderBox& gridItem, std::optional<LayoutUnit> width, std::optional<LayoutUnit> height) const
{
    // Because the grid area cannot be styled, we don't need to adjust
    // the grid breadth to account for 'box-sizing'.
    bool gridAreaWidthChanged = overrideSizeChanged(gridItem, GridTrackSizingDirection::ForColumns, width, height);
    bool gridAreaHeightChanged = overrideSizeChanged(gridItem, GridTrackSizingDirection::ForRows, width, height);
    if (gridAreaWidthChanged || (gridAreaHeightChanged && hasRelativeBlockAxisSize(*this, gridItem)))
        gridItem.setNeedsLayout(MarkOnlyThis);

    gridItem.setOverridingContainingBlockContentLogicalWidth(width);
    gridItem.setOverridingContainingBlockContentLogicalHeight(height);
}

void RenderGrid::updateGridAreaForAspectRatioItems(const Vector<RenderBox*>& autoGridItems, GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForColumns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows);

    for (auto& autoGridItem : autoGridItems) {
        updateGridAreaLogicalSize(*autoGridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(*autoGridItem, GridTrackSizingDirection::ForColumns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(*autoGridItem, GridTrackSizingDirection::ForRows));
        // For an item with aspect-ratio, if it has stretch alignment that stretches to the definite row, we also need to transfer the size before laying out the grid item.
        if (autoGridItem->hasStretchedLogicalHeight())
            applyStretchAlignmentToGridItemIfNeeded(*autoGridItem, gridLayoutState);
    }
}

void RenderGrid::layoutGridItems(GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForColumns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows);

    for (RenderBox* gridItem = firstChildBox(); gridItem; gridItem = gridItem->nextSiblingBox()) {
        if (currentGrid().orderIterator().shouldSkipChild(*gridItem)) {
            if (gridItem->isOutOfFlowPositioned())
                prepareGridItemForPositionedLayout(*gridItem);
            continue;
        }

        auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem);
        if (renderGrid && (renderGrid->isSubgridColumns() || renderGrid->isSubgridRows()))
            gridItem->setNeedsLayout(MarkOnlyThis);

        // Setting the definite grid area's sizes. It may imply that the
        // item must perform a layout if its area differs from the one
        // used during the track sizing algorithm.
        updateGridAreaLogicalSize(*gridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(*gridItem, GridTrackSizingDirection::ForColumns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(*gridItem, GridTrackSizingDirection::ForRows));

        LayoutRect oldGridItemRect = gridItem->frameRect();

        // Stretching logic might force a grid item layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that grid item margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToGridItemIfNeeded(*gridItem, gridLayoutState);
        applySubgridStretchAlignmentToGridItemIfNeeded(*gridItem);

        gridItem->layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        updateAutoMarginsInColumnAxisIfNeeded(*gridItem);
        updateAutoMarginsInRowAxisIfNeeded(*gridItem);

        setLogicalPositionForGridItem(*gridItem);

        // If the grid item moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the grid item) anyway.
        if (!selfNeedsLayout() && gridItem->checkForRepaintDuringLayout())
            gridItem->repaintDuringLayoutIfMoved(oldGridItemRect);
    }
}

void RenderGrid::layoutMasonryItems(GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForColumns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, GridTrackSizingDirection::ForRows);

    for (RenderBox* gridItem = firstChildBox(); gridItem; gridItem = gridItem->nextSiblingBox()) {
        if (currentGrid().orderIterator().shouldSkipChild(*gridItem)) {
            if (gridItem->isOutOfFlowPositioned())
                prepareGridItemForPositionedLayout(*gridItem);
            continue;
        }

        auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem);
        if (renderGrid && (renderGrid->isSubgridColumns() || renderGrid->isSubgridRows()))
            gridItem->setNeedsLayout(MarkOnlyThis);

        // Setting the definite grid area's sizes. It may imply that the
        // item must perform a layout if its area differs from the one
        // used during the track sizing algorithm.
        updateGridAreaLogicalSize(*gridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(*gridItem, GridTrackSizingDirection::ForColumns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(*gridItem, GridTrackSizingDirection::ForRows));

        LayoutRect oldGridItemRect = gridItem->frameRect();

        // Stretching logic might force a grid item layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that grid item margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToGridItemIfNeeded(*gridItem, gridLayoutState);
        applySubgridStretchAlignmentToGridItemIfNeeded(*gridItem);

        gridItem->layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        updateAutoMarginsInColumnAxisIfNeeded(*gridItem);
        updateAutoMarginsInRowAxisIfNeeded(*gridItem);

        setLogicalPositionForGridItem(*gridItem);

        // If the grid item moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the grid item) anyway.
        if (!selfNeedsLayout() && gridItem->checkForRepaintDuringLayout())
            gridItem->repaintDuringLayoutIfMoved(oldGridItemRect);
    }
}

void RenderGrid::prepareGridItemForPositionedLayout(RenderBox& gridItem)
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    gridItem.containingBlock()->insertPositionedObject(gridItem);

    RenderLayer* gridItemLayer = gridItem.layer();
    // Static position of a positioned grid item should use the content-box (https://drafts.csswg.org/css-grid/#static-position).
    gridItemLayer->setStaticInlinePosition(borderAndPaddingStart());
    gridItemLayer->setStaticBlockPosition(borderAndPaddingBefore());
}

bool RenderGrid::hasStaticPositionForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    return direction == GridTrackSizingDirection::ForColumns ? gridItem.style().hasStaticInlinePosition(isHorizontalWritingMode()) : gridItem.style().hasStaticBlockPosition(isHorizontalWritingMode());
}

void RenderGrid::layoutPositionedObject(RenderBox& gridItem, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    if (isSkippedContentRoot()) {
        gridItem.clearNeedsLayoutForSkippedContent();
        return;
    }

    LayoutUnit columnBreadth = gridAreaBreadthForOutOfFlowGridItem(gridItem, GridTrackSizingDirection::ForColumns);
    LayoutUnit rowBreadth = gridAreaBreadthForOutOfFlowGridItem(gridItem, GridTrackSizingDirection::ForRows);

    gridItem.setOverridingContainingBlockContentLogicalWidth(columnBreadth);
    gridItem.setOverridingContainingBlockContentLogicalHeight(rowBreadth);

    // Mark for layout as we're resetting the position before and we relay in generic layout logic
    // for positioned items in order to get the offsets properly resolved.
    gridItem.setChildNeedsLayout(MarkOnlyThis);

    RenderBlock::layoutPositionedObject(gridItem, relayoutChildren, fixedPositionObjectsOnly);

    setLogicalOffsetForGridItem(gridItem, GridTrackSizingDirection::ForColumns);
    setLogicalOffsetForGridItem(gridItem, GridTrackSizingDirection::ForRows);
}

LayoutUnit RenderGrid::gridAreaBreadthForGridItemIncludingAlignmentOffsets(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    if (direction == GridTrackSizingDirection::ForRows) {
        if (areMasonryRows())
            return isHorizontalWritingMode() ? gridItem.height() + gridItem.verticalMarginExtent() : gridItem.width() + gridItem.horizontalMarginExtent();
    } else if (areMasonryColumns())
        return isHorizontalWritingMode() ? gridItem.width() + gridItem.horizontalMarginExtent() : gridItem.height() + gridItem.verticalMarginExtent();

    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    const auto& span = currentGrid().gridItemSpan(gridItem, direction);
    const auto& linePositions = (direction == GridTrackSizingDirection::ForColumns) ? m_columnPositions : m_rowPositions;

    LayoutUnit initialTrackPosition = linePositions[span.startLine()];
    LayoutUnit finalTrackPosition = linePositions[span.endLine() - 1];

    // Track Positions vector stores the 'start' grid line of each track, so we have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.endLine() - 1].baseSize();
}

void RenderGrid::populateGridPositionsForDirection(const GridTrackSizingAlgorithm& algorithm, GridTrackSizingDirection direction)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence, we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.

    // The grid container's frame elements (border, padding and <content-position> offset) are sensible to the
    // inline-axis flow direction. However, column lines positions are 'direction' unaware. This simplification
    // allows us to use the same indexes to identify the columns independently on the inline-axis direction.
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    auto& tracks = algorithm.tracks(direction);
    unsigned numberOfTracks = tracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? currentGrid().autoRepeatEmptyTracks(direction)->size() : 0;
    const auto& offset = direction == GridTrackSizingDirection::ForColumns ? m_offsetBetweenColumns : m_offsetBetweenRows;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    positions.resize(numberOfLines);

    auto borderAndPadding = isRowAxis ? borderAndPaddingStart() : borderAndPaddingBefore();

    positions[0] = borderAndPadding + offset.positionOffset;
    if (numberOfLines > 1) {
        // If we have collapsed tracks we just ignore gaps here and add them later as we might not
        // compute the gap between two consecutive tracks without examining the surrounding ones.
        LayoutUnit gap = !hasCollapsedTracks ? gridGap(direction) : 0_lu;
        unsigned nextToLastLine = numberOfLines - 2;

        for (unsigned i = 0; i < nextToLastLine; ++i)
            positions[i + 1] = positions[i] + offset.distributionOffset + tracks[i].unclampedBaseSize() + gap;
        positions[lastLine] = positions[nextToLastLine] + tracks[nextToLastLine].unclampedBaseSize();

        if (isMasonry(direction))
            positions[lastLine] = m_masonryLayout.gridContentSize() + positions[0];

        // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to collapse (they
        // coincide exactly) except on the edges of the grid where they become 0.
        if (hasCollapsedTracks) {
            gap = gridGap(direction);
            unsigned remainingEmptyTracks = numberOfCollapsedTracks;
            LayoutUnit offsetAccumulator;
            LayoutUnit gapAccumulator;
            for (unsigned i = 1; i < lastLine; ++i) {
                if (currentGrid().isEmptyAutoRepeatTrack(direction, i - 1)) {
                    --remainingEmptyTracks;
                    offsetAccumulator += offset.distributionOffset;
                } else {
                    // Add gap between consecutive non empty tracks. Add it also just once for an
                    // arbitrary number of empty tracks between two non empty ones.
                    bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
                    if (!allRemainingTracksAreEmpty || !currentGrid().isEmptyAutoRepeatTrack(direction, i))
                        gapAccumulator += gap;
                }
                positions[i] += gapAccumulator - offsetAccumulator;
            }
            positions[lastLine] += gapAccumulator - offsetAccumulator;
        }
    }
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackSize, LayoutUnit gridItemSize)
{
    LayoutUnit offset = trackSize - gridItemSize;
    switch (overflow) {
    case OverflowAlignment::Safe:
        // If overflow is 'safe', we have to make sure we don't overflow the 'start'
        // edge (potentially cause some data loss as the overflow is unreachable).
        return std::max<LayoutUnit>(0, offset);
    case OverflowAlignment::Unsafe:
    case OverflowAlignment::Default:
        // If we overflow our alignment container and overflow is 'true' (default), we
        // ignore the overflow and just return the value regardless (which may cause data
        // loss as we overflow the 'start' edge).
        return offset;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderGrid::availableAlignmentSpaceForGridItemBeforeStretching(LayoutUnit gridAreaBreadthForGridItem, const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // grid items are laid out, so we can't use the grid item cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    auto gridItemFlowDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, direction);
    return std::max(0_lu, gridAreaBreadthForGridItem - GridLayoutFunctions::marginLogicalSizeForGridItem(*this, gridItemFlowDirection, gridItem));
}

StyleSelfAlignmentData RenderGrid::alignSelfForGridItem(const RenderBox& gridItem, StretchingMode stretchingMode, const RenderStyle* gridStyle) const
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (renderGrid && renderGrid->isSubgridInParentDirection(GridTrackSizingDirection::ForRows))
        return { ItemPosition::Stretch, OverflowAlignment::Default };
    if (!gridStyle)
        gridStyle = &style();
    auto normalBehavior = stretchingMode == StretchingMode::Any ? selfAlignmentNormalBehavior(&gridItem) : ItemPosition::Normal;
    return gridItem.style().resolvedAlignSelf(gridStyle, normalBehavior);
}

StyleSelfAlignmentData RenderGrid::justifySelfForGridItem(const RenderBox& gridItem, StretchingMode stretchingMode, const RenderStyle* gridStyle) const
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (renderGrid && renderGrid->isSubgridInParentDirection(GridTrackSizingDirection::ForColumns))
        return { ItemPosition::Stretch, OverflowAlignment::Default };
    if (!gridStyle)
        gridStyle = &style();
    auto normalBehavior = stretchingMode == StretchingMode::Any ? selfAlignmentNormalBehavior(&gridItem) : ItemPosition::Normal;
    return gridItem.style().resolvedJustifySelf(gridStyle, normalBehavior);
}

bool RenderGrid::aspectRatioPrefersInline(const RenderBox& gridItem, bool blockFlowIsColumnAxis)
{
    if (!gridItem.style().hasAspectRatio())
        return false;
    bool hasExplicitInlineStretch = justifySelfForGridItem(gridItem, StretchingMode::Explicit).position() == ItemPosition::Stretch;
    bool hasExplicitBlockStretch = alignSelfForGridItem(gridItem, StretchingMode::Explicit).position() == ItemPosition::Stretch;
    if (!blockFlowIsColumnAxis)
        std::swap(hasExplicitInlineStretch, hasExplicitBlockStretch);
    return !hasExplicitBlockStretch;
}

inline bool RenderGrid::allowedToStretchGridItemAlongColumnAxis(const RenderBox& gridItem) const
{
    return alignSelfForGridItem(gridItem).position() == ItemPosition::Stretch && hasAutoSizeInColumnAxis(gridItem) && !hasAutoMarginsInColumnAxis(gridItem);
}

inline bool RenderGrid::allowedToStretchGridItemAlongRowAxis(const RenderBox& gridItem) const
{
    return justifySelfForGridItem(gridItem).position() == ItemPosition::Stretch && hasAutoSizeInRowAxis(gridItem) && !hasAutoMarginsInRowAxis(gridItem);
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::applyStretchAlignmentToGridItemIfNeeded(RenderBox& gridItem, GridLayoutState& gridLayoutState)
{
    ASSERT(gridItem.overridingContainingBlockContentLogicalHeight());
    ASSERT(gridItem.overridingContainingBlockContentLogicalWidth());

    // We clear height and width override values because we will decide now whether it's allowed or
    // not, evaluating the conditions which might have changed since the old values were set.
    gridItem.clearOverridingLogicalHeight();
    gridItem.clearOverridingLogicalWidth();

    GridTrackSizingDirection gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, GridTrackSizingDirection::ForRows);
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, GridTrackSizingDirection::ForColumns);
    bool blockFlowIsColumnAxis = gridItemBlockDirection == GridTrackSizingDirection::ForRows;
    bool allowedToStretchgridItemBlockSize = blockFlowIsColumnAxis ? allowedToStretchGridItemAlongColumnAxis(gridItem) : allowedToStretchGridItemAlongRowAxis(gridItem);
    if (allowedToStretchgridItemBlockSize && !aspectRatioPrefersInline(gridItem, blockFlowIsColumnAxis)) {
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemBlockDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        LayoutUnit stretchedLogicalHeight = availableAlignmentSpaceForGridItemBeforeStretching(overridingContainingBlockContentSizeForGridItem->value(), gridItem, GridTrackSizingDirection::ForRows);
        LayoutUnit desiredLogicalHeight = gridItem.constrainLogicalHeightByMinMax(stretchedLogicalHeight, std::nullopt);
        gridItem.setOverridingLogicalHeight(desiredLogicalHeight);

        auto itemNeedsRelayoutForStretchAlignment = [&]() {
            if (desiredLogicalHeight != gridItem.logicalHeight())
                return true;

            if (canSetColumnAxisStretchRequirementForItem(gridItem))
                return gridLayoutState.containsLayoutRequirementForGridItem(gridItem, ItemLayoutRequirement::NeedsColumnAxisStretchAlignment);

            return is<RenderBlock>(gridItem) && downcast<RenderBlock>(gridItem).hasPercentHeightDescendants();
        }();
        // Checking the logical-height of a grid item isn't enough. Setting an override logical-height
        // changes the definiteness, resulting in percentages to resolve differently.
        //
        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        if (itemNeedsRelayoutForStretchAlignment) {
            gridItem.setLogicalHeight(0_lu);
            gridItem.setNeedsLayout(MarkOnlyThis);
        }
    } else if (!allowedToStretchgridItemBlockSize && allowedToStretchGridItemAlongRowAxis(gridItem)) {
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        LayoutUnit stretchedLogicalWidth = availableAlignmentSpaceForGridItemBeforeStretching(overridingContainingBlockContentSizeForGridItem->value(), gridItem, GridTrackSizingDirection::ForColumns);
        LayoutUnit desiredLogicalWidth = gridItem.constrainLogicalWidthInFragmentByMinMax(stretchedLogicalWidth, contentWidth(), *this, nullptr);
        gridItem.setOverridingLogicalWidth(desiredLogicalWidth);
        if (desiredLogicalWidth != gridItem.logicalWidth())
            gridItem.setNeedsLayout(MarkOnlyThis);
    }
}

void RenderGrid::applySubgridStretchAlignmentToGridItemIfNeeded(RenderBox& gridItem)
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (!renderGrid)
        return;

    if (renderGrid->isSubgrid(GridTrackSizingDirection::ForRows)) {
        auto gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, GridTrackSizingDirection::ForRows);
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemBlockDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        auto stretchedLogicalHeight = availableAlignmentSpaceForGridItemBeforeStretching(overridingContainingBlockContentSizeForGridItem->value(), gridItem, GridTrackSizingDirection::ForRows);
        gridItem.setOverridingLogicalHeight(stretchedLogicalHeight);
    }

    if (renderGrid->isSubgrid(GridTrackSizingDirection::ForColumns)) {
        auto gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, GridTrackSizingDirection::ForColumns);
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        auto stretchedLogicalWidth = availableAlignmentSpaceForGridItemBeforeStretching(overridingContainingBlockContentSizeForGridItem->value(), gridItem, GridTrackSizingDirection::ForColumns);
        gridItem.setOverridingLogicalWidth(stretchedLogicalWidth);
    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::hasAutoMarginsInColumnAxis(const RenderBox& gridItem) const
{
    if (isHorizontalWritingMode())
        return gridItem.style().marginTop().isAuto() || gridItem.style().marginBottom().isAuto();
    return gridItem.style().marginLeft().isAuto() || gridItem.style().marginRight().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::hasAutoMarginsInRowAxis(const RenderBox& gridItem) const
{
    if (isHorizontalWritingMode())
        return gridItem.style().marginLeft().isAuto() || gridItem.style().marginRight().isAuto();
    return gridItem.style().marginTop().isAuto() || gridItem.style().marginBottom().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInRowAxisIfNeeded(RenderBox& gridItem)
{
    ASSERT(!gridItem.isOutOfFlowPositioned());

    const RenderStyle& parentStyle = style();
    Length marginStart = gridItem.style().marginStartUsing(&parentStyle);
    Length marginEnd = gridItem.style().marginEndUsing(&parentStyle);
    LayoutUnit marginLogicalWidth;
    // We should only consider computed margins if their specified value isn't
    // 'auto', since such computed value may come from a previous layout and may
    // be incorrect now.
    if (!marginStart.isAuto())
        marginLogicalWidth += gridItem.marginStart();
    if (!marginEnd.isAuto())
        marginLogicalWidth += gridItem.marginEnd();

    auto availableAlignmentSpace = gridItem.overridingContainingBlockContentLogicalWidth()->value() - gridItem.logicalWidth() - marginLogicalWidth;
    if (availableAlignmentSpace <= 0)
        return;

    if (marginStart.isAuto() && marginEnd.isAuto()) {
        gridItem.setMarginStart(availableAlignmentSpace / 2, &parentStyle);
        gridItem.setMarginEnd(availableAlignmentSpace / 2, &parentStyle);
    } else if (marginStart.isAuto()) {
        gridItem.setMarginStart(availableAlignmentSpace, &parentStyle);
    } else if (marginEnd.isAuto()) {
        gridItem.setMarginEnd(availableAlignmentSpace, &parentStyle);
    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInColumnAxisIfNeeded(RenderBox& gridItem)
{
    ASSERT(!gridItem.isOutOfFlowPositioned());

    const RenderStyle& parentStyle = style();
    Length marginBefore = gridItem.style().marginBeforeUsing(&parentStyle);
    Length marginAfter = gridItem.style().marginAfterUsing(&parentStyle);
    LayoutUnit marginLogicalHeight;
    // We should only consider computed margins if their specified value isn't
    // 'auto', since such computed value may come from a previous layout and may
    // be incorrect now.
    if (!marginBefore.isAuto())
        marginLogicalHeight += gridItem.marginBefore();
    if (!marginAfter.isAuto())
        marginLogicalHeight += gridItem.marginAfter();

    auto availableAlignmentSpace = gridItem.overridingContainingBlockContentLogicalHeight()->value() - gridItem.logicalHeight() - marginLogicalHeight;
    if (availableAlignmentSpace <= 0)
        return;

    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        gridItem.setMarginBefore(availableAlignmentSpace / 2, &parentStyle);
        gridItem.setMarginAfter(availableAlignmentSpace / 2, &parentStyle);
    } else if (marginBefore.isAuto()) {
        gridItem.setMarginBefore(availableAlignmentSpace, &parentStyle);
    } else if (marginAfter.isAuto()) {
        gridItem.setMarginAfter(availableAlignmentSpace, &parentStyle);
    }
}

bool RenderGrid::isChildEligibleForMarginTrim(MarginTrimType marginTrimType, const RenderBox& gridItem) const
{
    ASSERT(style().marginTrim().contains(marginTrimType));

    auto isTrimmingBlockDirection = marginTrimType == MarginTrimType::BlockStart || marginTrimType == MarginTrimType::BlockEnd;
    auto itemGridSpan = isTrimmingBlockDirection ? currentGrid().gridItemSpanIgnoringCollapsedTracks(gridItem, GridTrackSizingDirection::ForRows) : currentGrid().gridItemSpanIgnoringCollapsedTracks(gridItem, GridTrackSizingDirection::ForColumns);
    switch (marginTrimType) {
    case MarginTrimType::BlockStart:
    case MarginTrimType::InlineStart:
        return !itemGridSpan.startLine();
    case MarginTrimType::BlockEnd:
        return itemGridSpan.endLine() == currentGrid().numTracks(GridTrackSizingDirection::ForRows);
    case MarginTrimType::InlineEnd:
        return itemGridSpan.endLine() == currentGrid().numTracks(GridTrackSizingDirection::ForColumns);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderGrid::isBaselineAlignmentForGridItem(const RenderBox& gridItem) const
{
    return isBaselineAlignmentForGridItem(gridItem, GridAxis::GridRowAxis) || isBaselineAlignmentForGridItem(gridItem, GridAxis::GridColumnAxis);
}

bool RenderGrid::isBaselineAlignmentForGridItem(const RenderBox& gridItem, GridAxis baselineAxis, AllowedBaseLine allowed) const
{
    if (gridItem.isOutOfFlowPositioned())
        return false;
    ItemPosition align = selfAlignmentForGridItem(baselineAxis, gridItem).position();
    bool hasAutoMargins = baselineAxis == GridAxis::GridColumnAxis ? hasAutoMarginsInColumnAxis(gridItem) : hasAutoMarginsInRowAxis(gridItem);
    bool isBaseline = allowed == AllowedBaseLine::FirstLine ? isFirstBaselinePosition(align) : isBaselinePosition(align);
    return isBaseline && !hasAutoMargins;
}

// FIXME: This logic is shared by RenderFlexibleBox, so it might be refactored somehow.
LayoutUnit RenderGrid::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode mode) const
{
    ASSERT_UNUSED(mode, mode == PositionOnContainingLine);
    auto baseline = firstLineBaseline();
    if (!baseline)
        return synthesizedBaseline(*this, *parentStyle(), direction, BorderBox) + marginLogicalHeight();

    return baseline.value() + (direction == HorizontalLine ? marginTop() : marginRight()).toInt();
}

std::optional<LayoutUnit> RenderGrid::firstLineBaseline() const
{
    if ((isWritingModeRoot() && !isFlexItem()) || !currentGrid().hasGridItems() || shouldApplyLayoutContainment())
        return std::nullopt;

    // Finding the first grid item in grid order.
    auto baselineGridItem = getBaselineGridItem(ItemPosition::Baseline);

    if (!baselineGridItem)
        return std::nullopt;

    auto baseline = GridLayoutFunctions::isOrthogonalGridItem(*this, *baselineGridItem) ? std::nullopt : baselineGridItem->firstLineBaseline();
    // We take border-box's bottom if no valid baseline.
    if (!baseline) {
        // FIXME: We should pass |direction| into firstLineBaseline and stop bailing out if we're a writing
        // mode root. This would also fix some cases where the grid is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaseline(*baselineGridItem, style(), direction, BorderBox) + logicalTopForChild(*baselineGridItem);
    }
    return baseline.value() + baselineGridItem->logicalTop().toInt();
}

std::optional<LayoutUnit> RenderGrid::lastLineBaseline() const
{
    if (isWritingModeRoot() || !currentGrid().hasGridItems() || shouldApplyLayoutContainment())
        return std::nullopt;

    auto baselineGridItem = getBaselineGridItem(ItemPosition::LastBaseline);
    if (!baselineGridItem)
        return std::nullopt;

    auto baseline = GridLayoutFunctions::isOrthogonalGridItem(*this, *baselineGridItem) ? std::nullopt : baselineGridItem->lastLineBaseline();
    if (!baseline) {
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaseline(*baselineGridItem, style(), direction, BorderBox) + logicalTopForChild(*baselineGridItem);

    }

    return baseline.value() + baselineGridItem->logicalTop().toInt();
}

SingleThreadWeakPtr<RenderBox> RenderGrid::getBaselineGridItem(ItemPosition alignment) const
{
    ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);
    const RenderBox* baselineGridItem = nullptr;
    unsigned numColumns = currentGrid().numTracks(GridTrackSizingDirection::ForColumns);
    auto rowIndexDeterminingBaseline = alignment == ItemPosition::Baseline ? 0 : currentGrid().numTracks(GridTrackSizingDirection::ForRows) - 1;
    for (size_t column = 0; column < numColumns; column++) {
        auto cell = currentGrid().cell(rowIndexDeterminingBaseline, alignment == ItemPosition::Baseline ? column : numColumns - column - 1);

        for (auto& gridItem : cell) {
            ASSERT(gridItem.get());
            // If an item participates in baseline alignment, we select such item.
            if (isBaselineAlignmentForGridItem(*gridItem, GridAxis::GridColumnAxis, AllowedBaseLine::BothLines)) {
                auto gridItemAlignment = selfAlignmentForGridItem(GridAxis::GridColumnAxis, *gridItem).position();
                if (rowIndexDeterminingBaseline == GridLayoutFunctions::alignmentContextForBaselineAlignment(gridSpanForGridItem(*gridItem, GridTrackSizingDirection::ForRows), gridItemAlignment)) {
                    // FIXME: self-baseline and content-baseline alignment not implemented yet.
                    baselineGridItem = gridItem.get();
                    break;
                }
            }
            if (!baselineGridItem)
                baselineGridItem = gridItem.get();
        }
    }
    return baselineGridItem;
}

std::optional<LayoutUnit> RenderGrid::inlineBlockBaseline(LineDirectionMode) const
{
    return firstLineBaseline();
}

LayoutUnit RenderGrid::columnAxisBaselineOffsetForGridItem(const RenderBox& gridItem) const
{
    // FIXME : CSS Masonry does not properly handle baseline calculations currently.
    // We will just skip this running this step if we detect the RenderGrid is Masonry for now.
    if (isMasonry())
        return LayoutUnit { };

    if (isSubgridRows()) {
        RenderGrid* outer = downcast<RenderGrid>(parent());
        if (GridLayoutFunctions::isOrthogonalGridItem(*outer, *this))
            return outer->rowAxisBaselineOffsetForGridItem(gridItem);
        return outer->columnAxisBaselineOffsetForGridItem(gridItem);
    }
    return m_trackSizingAlgorithm.baselineOffsetForGridItem(gridItem, GridAxis::GridColumnAxis);
}

LayoutUnit RenderGrid::rowAxisBaselineOffsetForGridItem(const RenderBox& gridItem) const
{
    // FIXME : CSS Masonry does not properly handle baseline calculations currently.
    // We will just skip this running this step if we detect the RenderGrid is Masonry for now.
    if (isMasonry())
        return LayoutUnit { };

    if (isSubgridColumns()) {
        RenderGrid* outer = downcast<RenderGrid>(parent());
        if (GridLayoutFunctions::isOrthogonalGridItem(*outer, *this))
            return outer->columnAxisBaselineOffsetForGridItem(gridItem);
        return outer->rowAxisBaselineOffsetForGridItem(gridItem);
    }
    return m_trackSizingAlgorithm.baselineOffsetForGridItem(gridItem, GridAxis::GridRowAxis);
}

GridAxisPosition RenderGrid::columnAxisPositionForGridItem(const RenderBox& gridItem) const
{
    if (gridItem.isOutOfFlowPositioned() && !hasStaticPositionForGridItem(gridItem, GridTrackSizingDirection::ForRows))
        return GridAxisPosition::GridAxisStart;

    bool hasSameDirection = isHorizontalWritingMode()
        ? writingMode().isBlockTopToBottom() == gridItem.writingMode().isAnyTopToBottom()
        : writingMode().isBlockLeftToRight() == gridItem.writingMode().isAnyLeftToRight();

    switch (const auto gridItemAlignSelf = alignSelfForGridItem(gridItem).position()) {
    case ItemPosition::SelfStart:
        // self-start is based on the grid item's block-flow direction.
        return hasSameDirection ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
    case ItemPosition::SelfEnd:
        // self-end is based on the grid item's block-flow direction.
        return hasSameDirection ? GridAxisPosition::GridAxisEnd : GridAxisPosition::GridAxisStart;
    case ItemPosition::Left:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::Right:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::Center:
        return GridAxisPosition::GridAxisCenter;
    case ItemPosition::FlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (block-start) in the column axis.
    case ItemPosition::Start:
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::FlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (block-end) in the column axis.
    case ItemPosition::End:
        return GridAxisPosition::GridAxisEnd;
    case ItemPosition::Stretch:
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline: {
        auto fallbackAlignment = [&] {
            if (gridItemAlignSelf == ItemPosition::Baseline)
                return hasSameDirection ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
            return hasSameDirection ? GridAxisPosition::GridAxisEnd : GridAxisPosition::GridAxisStart;
        };
        if (GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem))
            return gridItemAlignSelf == ItemPosition::Baseline ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
        return fallbackAlignment();
    }
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        break;
    case ItemPosition::AnchorCenter:
        return GridAxisPosition::GridAxisStart; // TODO: Implement - see https://bugs.webkit.org/show_bug.cgi?id=275451.
    }

    ASSERT_NOT_REACHED();
    return GridAxisPosition::GridAxisStart;
}

GridAxisPosition RenderGrid::rowAxisPositionForGridItem(const RenderBox& gridItem) const
{
    if (gridItem.isOutOfFlowPositioned() && !hasStaticPositionForGridItem(gridItem, GridTrackSizingDirection::ForColumns))
        return GridAxisPosition::GridAxisStart;

    bool hasSameDirection = isHorizontalWritingMode()
        ? writingMode().isInlineLeftToRight() == gridItem.writingMode().isAnyLeftToRight()
        : writingMode().isInlineTopToBottom() == gridItem.writingMode().isAnyTopToBottom();

    switch (justifySelfForGridItem(gridItem).position()) {
    case ItemPosition::SelfStart:
        // self-start is based on the grid item's inline-flow direction.
        return hasSameDirection ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
    case ItemPosition::SelfEnd:
        // self-end is based on the grid item's inline-flow direction.
        return hasSameDirection ? GridAxisPosition::GridAxisEnd : GridAxisPosition::GridAxisStart;
    case ItemPosition::Left:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // We want the physical 'left' side, so we have to take account, container's inline-flow direction.
        return writingMode().isBidiLTR() ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
    case ItemPosition::Right:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // We want the physical 'right' side, so we have to take account, container's inline-flow direction.
        return writingMode().isBidiLTR() ? GridAxisPosition::GridAxisEnd : GridAxisPosition::GridAxisStart;
    case ItemPosition::Center:
        return GridAxisPosition::GridAxisCenter;
    case ItemPosition::FlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (inline-start) in the row axis.
    case ItemPosition::Start:
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::FlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (inline-end) in the row axis.
    case ItemPosition::End:
        return GridAxisPosition::GridAxisEnd;
    case ItemPosition::Stretch:
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the grid item.
        return GridAxisPosition::GridAxisStart;
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        break;
    case ItemPosition::AnchorCenter:
        return GridAxisPosition::GridAxisStart; // TODO: Implement - see https://bugs.webkit.org/show_bug.cgi?id=275451.
    }

    ASSERT_NOT_REACHED();
    return GridAxisPosition::GridAxisStart;
}

LayoutUnit RenderGrid::columnAxisOffsetForGridItem(const RenderBox& gridItem) const
{
    auto [startOfRow, endOfRow] = gridAreaPositionForGridItem(gridItem, GridTrackSizingDirection::ForRows);
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(gridItem);
    LayoutUnit columnAxisGridItemSize = GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalWidth() + gridItem.marginLogicalWidth() : gridItem.logicalHeight() + gridItem.marginLogicalHeight();
    LayoutUnit masonryOffset = areMasonryRows() ? m_masonryLayout.offsetForGridItem(gridItem) : 0_lu;
    auto overflow = alignSelfForGridItem(gridItem).overflow();
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfRow - startOfRow, columnAxisGridItemSize);
    if (hasAutoMarginsInColumnAxis(gridItem))
        return startPosition;
    GridAxisPosition axisPosition = columnAxisPositionForGridItem(gridItem);
    switch (axisPosition) {
    case GridAxisPosition::GridAxisStart:
        return startPosition + columnAxisBaselineOffsetForGridItem(gridItem) + masonryOffset;
    case GridAxisPosition::GridAxisEnd:
        return (startPosition + offsetFromStartPosition) - columnAxisBaselineOffsetForGridItem(gridItem);
    case GridAxisPosition::GridAxisCenter:
        return startPosition + (offsetFromStartPosition / 2);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderGrid::rowAxisOffsetForGridItem(const RenderBox& gridItem) const
{
    auto [startOfColumn, endOfColumn] = gridAreaPositionForGridItem(gridItem, GridTrackSizingDirection::ForColumns);
    LayoutUnit startPosition = startOfColumn + marginStartForChild(gridItem);
    LayoutUnit masonryOffset = areMasonryColumns() ? m_masonryLayout.offsetForGridItem(gridItem) : 0_lu;
    if (hasAutoMarginsInRowAxis(gridItem))
        return startPosition;
    GridAxisPosition axisPosition = rowAxisPositionForGridItem(gridItem);
    switch (axisPosition) {
    case GridAxisPosition::GridAxisStart:
        return startPosition + rowAxisBaselineOffsetForGridItem(gridItem) + masonryOffset;
    case GridAxisPosition::GridAxisEnd:
    case GridAxisPosition::GridAxisCenter: {
        LayoutUnit rowAxisGridItemSize = GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalHeight() + gridItem.marginLogicalHeight() : gridItem.logicalWidth() + gridItem.marginLogicalWidth();
        auto overflow = justifySelfForGridItem(gridItem).overflow();
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfColumn - startOfColumn, rowAxisGridItemSize);
        return startPosition + (axisPosition == GridAxisPosition::GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool RenderGrid::isSubgrid(GridTrackSizingDirection direction) const
{
    // If the grid container is forced to establish an independent formatting
    // context (like contain layout, or position:absolute), then the used value
    // of grid-template-rows/columns is 'none' and the container is not a subgrid.
    // https://drafts.csswg.org/css-grid-2/#subgrid-listing
    if (RenderElement::establishesIndependentFormattingContext())
        return false;
    if (direction == GridTrackSizingDirection::ForColumns ? !style().gridSubgridColumns() : !style().gridSubgridRows())
        return false;
    auto* renderGrid = dynamicDowncast<RenderGrid>(parent());
    if (!renderGrid)
        return false;
    return direction == GridTrackSizingDirection::ForRows ? !renderGrid->areMasonryRows() : !renderGrid->areMasonryColumns();
}

bool RenderGrid::isSubgridInParentDirection(GridTrackSizingDirection parentDirection) const
{
    auto* renderGrid = dynamicDowncast<RenderGrid>(parent());
    if (!renderGrid)
        return false;
    GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid, *this, parentDirection);
    return isSubgrid(direction);
}

bool RenderGrid::isSubgridOf(GridTrackSizingDirection direction, const RenderGrid& ancestor) const
{
    if (!isSubgrid(direction))
        return false;
    if (parent() == &ancestor)
        return true;

    auto& parentGrid = *downcast<RenderGrid>(parent());
    GridTrackSizingDirection parentDirection = GridLayoutFunctions::flowAwareDirectionForParent(parentGrid, *this, direction);
    return parentGrid.isSubgridOf(parentDirection, ancestor);
}

const Grid& RenderGrid::currentGrid() const
{
    return m_grid.m_currentGrid;
}

Grid& RenderGrid::currentGrid()
{
    return m_grid.m_currentGrid;
}

LayoutUnit RenderGrid::gridAreaBreadthForOutOfFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    int lastLine = numTracks(direction);

    int startLine, endLine;
    bool startIsAuto, endIsAuto;
    if (!computeGridPositionsForOutOfFlowGridItem(gridItem, direction, startLine, startIsAuto, endLine, endIsAuto))
        return isRowAxis ? clientLogicalWidth() : clientLogicalHeight();

    if (startIsAuto && endIsAuto)
        return isRowAxis ? clientLogicalWidth() : clientLogicalHeight();

    LayoutUnit start;
    LayoutUnit end;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    auto& outOfFlowItemLine = isRowAxis ? m_outOfFlowItemColumn : m_outOfFlowItemRow;
    LayoutUnit borderEdge = isRowAxis ? borderStart() : borderBefore();
    if (startIsAuto)
        start = borderEdge;
    else {
        outOfFlowItemLine.set(gridItem, startLine);
        start = positions[startLine];
    }
    if (endIsAuto)
        end = ((direction == GridTrackSizingDirection::ForRows) ? clientLogicalHeight() : clientLogicalWidth()) + borderEdge;
    else {
        end = positions[endLine];
        // These vectors store line positions including gaps, but we shouldn't consider them for the edges of the grid.
        std::optional<LayoutUnit> availableSizeForGutters = availableSpaceForGutters(direction);
        if (endLine > 0 && endLine < lastLine) {
            ASSERT(!currentGrid().needsItemsPlacement());
            end -= guttersSize(direction, endLine - 1, 2, availableSizeForGutters);
            end -= isRowAxis ? m_offsetBetweenColumns.distributionOffset : m_offsetBetweenRows.distributionOffset;
        }
    }
    return std::max(end - start, 0_lu);
}

LayoutUnit RenderGrid::logicalOffsetForOutOfFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction, LayoutUnit trackBreadth) const
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    if (hasStaticPositionForGridItem(gridItem, direction))
        return 0_lu;

    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    bool isFlowAwareRowAxis = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, direction) == GridTrackSizingDirection::ForColumns;
    LayoutUnit gridItemPosition = isFlowAwareRowAxis ? gridItem.logicalLeft() : gridItem.logicalTop();
    LayoutUnit gridBorder = isRowAxis ? borderLogicalLeft() : borderBefore();
    LayoutUnit gridItemMargin = isRowAxis ? gridItem.marginLogicalLeft(&style()) : gridItem.marginBefore(&style());
    LayoutUnit offset = gridItemPosition - gridBorder - gridItemMargin;
    if (!isRowAxis || writingMode().isLogicalLeftInlineStart())
        return offset;

    LayoutUnit gridItemBreadth = isFlowAwareRowAxis ? gridItem.logicalWidth() + gridItem.marginLogicalWidth() : gridItem.logicalHeight() + gridItem.marginLogicalHeight();
    return trackBreadth - offset - gridItemBreadth;
}

std::pair<LayoutUnit, LayoutUnit> RenderGrid::gridAreaPositionForOutOfFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    ASSERT(GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, direction));
    auto trackBreadth = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, direction)->value();
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    auto& outOfFlowItemLine = isRowAxis ? m_outOfFlowItemColumn : m_outOfFlowItemRow;
    auto start = isRowAxis ? borderStart() : borderBefore();
    if (auto line = outOfFlowItemLine.get(&gridItem)) {
        auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
        start = positions[line.value()];
    }
    start += logicalOffsetForOutOfFlowGridItem(gridItem, direction, trackBreadth);
    return { start, start + trackBreadth };
}

std::pair<LayoutUnit, LayoutUnit> RenderGrid::gridAreaPositionForInFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    ASSERT(!gridItem.isOutOfFlowPositioned());
    const GridSpan& span = currentGrid().gridItemSpan(gridItem, direction);
    // FIXME (lajava): This is a common pattern, why not defining a function like
    // positions(direction) ?
    auto& positions = direction == GridTrackSizingDirection::ForColumns ? m_columnPositions : m_rowPositions;
    auto start = positions[span.startLine()];
    auto end = positions[span.endLine()];
    // The 'positions' vector includes distribution offset (because of content
    // alignment) and gutters, so we need to subtract them to get the actual
    // end position for a given track (this does not have to be done for the
    // last track as there are no more positions' elements after it, nor for
    // collapsed tracks).
    if (span.endLine() < positions.size() - 1
        && !(currentGrid().hasAutoRepeatEmptyTracks(direction)
        && currentGrid().isEmptyAutoRepeatTrack(direction, span.endLine()))) {
        end -= gridGap(direction) + gridItemOffset(direction);
    }
    return { start, end };
}

std::pair<LayoutUnit, LayoutUnit> RenderGrid::gridAreaPositionForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    if (gridItem.isOutOfFlowPositioned())
        return gridAreaPositionForOutOfFlowGridItem(gridItem, direction);
    return gridAreaPositionForInFlowGridItem(gridItem, direction);
}

std::pair<OverflowAlignment, ContentPosition> static resolveContentDistributionFallback(ContentDistribution distribution)
{
    switch (distribution) {
    case ContentDistribution::SpaceBetween:
        return { OverflowAlignment::Default, ContentPosition::Start };
    case ContentDistribution::SpaceAround:
        return { OverflowAlignment::Safe, ContentPosition::Center };
    case ContentDistribution::SpaceEvenly:
        return { OverflowAlignment::Safe, ContentPosition::Center };
    case ContentDistribution::Stretch:
        return { OverflowAlignment::Default, ContentPosition::Start };
    case ContentDistribution::Default:
        return { OverflowAlignment::Default, ContentPosition::Normal };
    }

    ASSERT_NOT_REACHED();
    return { OverflowAlignment::Default, ContentPosition::Normal };
}

StyleContentAlignmentData RenderGrid::contentAlignment(GridTrackSizingDirection direction) const
{
    return direction == GridTrackSizingDirection::ForColumns ? style().resolvedJustifyContent(contentAlignmentNormalBehaviorGrid()) : style().resolvedAlignContent(contentAlignmentNormalBehaviorGrid());
}

ContentAlignmentData RenderGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    if (isRowAxis ? isSubgridColumns() : isSubgridRows())
        return { };

    auto contentAlignmentData = contentAlignment(direction);
    auto contentAlignmentDistribution = contentAlignmentData.distribution();

    // Apply <content-distribution> and return, or continue to fallback positioning if we can't distribute.
    if (contentAlignmentDistribution != ContentDistribution::Default) {
        if (availableFreeSpace > 0) {
            switch (contentAlignmentDistribution) {
            case ContentDistribution::SpaceBetween:
                if (numberOfGridTracks < 2)
                    break;
                return { 0_lu, availableFreeSpace / (numberOfGridTracks - 1) };
            case ContentDistribution::SpaceAround: {
                if (numberOfGridTracks < 1)
                    break;
                auto spaceBetweenTracks = availableFreeSpace / numberOfGridTracks;
                return { spaceBetweenTracks / 2, spaceBetweenTracks };
            }
            case ContentDistribution::SpaceEvenly: {
                auto spaceEvenlyDistribution = availableFreeSpace / (numberOfGridTracks + 1);
                return { spaceEvenlyDistribution, spaceEvenlyDistribution };
            }
            case ContentDistribution::Stretch:
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }

    auto [fallbackOverflow, fallbackContentPosition] = resolveContentDistributionFallback(contentAlignmentDistribution);
    auto contentAlignmentOverflow = contentAlignmentData.overflow();

    // Apply alignment safety.
    if (availableFreeSpace <= 0 && (contentAlignmentOverflow == OverflowAlignment::Safe || fallbackOverflow == OverflowAlignment::Safe))
        return { };

    auto usedContentPosition = contentAlignmentDistribution == ContentDistribution::Default ? contentAlignmentData.position() : fallbackContentPosition;
    // Apply <content-position> / fallback positioning.
    switch (usedContentPosition) {
    case ContentPosition::Left:
        ASSERT(isRowAxis);
        if (!writingMode().isBidiLTR())
            return { availableFreeSpace, 0_lu };
        return { };
    case ContentPosition::Right:
        ASSERT(isRowAxis);
        if (writingMode().isBidiLTR())
            return { availableFreeSpace, 0_lu };
        return { };
    case ContentPosition::Center:
        return { availableFreeSpace / 2, 0_lu };
    case ContentPosition::FlexEnd: // Only used in flex layout, for other layout, it's equivalent to 'end'.
    case ContentPosition::End:
        return { availableFreeSpace, 0_lu };
    case ContentPosition::FlexStart: // Only used in flex layout, for other layout, it's equivalent to 'start'.
    case ContentPosition::Start:
    case ContentPosition::Baseline:
    case ContentPosition::LastBaseline:
        // FIXME: Implement the baseline values. For now, we always 'start' align.
        // http://webkit.org/b/145566
        return { };
    case ContentPosition::Normal:
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

LayoutOptionalOutsets RenderGrid::allowedLayoutOverflow() const
{
    LayoutOptionalOutsets allowance = RenderBox::allowedLayoutOverflow();
    if (m_offsetBetweenColumns.positionOffset < 0)
        allowance.setStart(-m_offsetBetweenColumns.positionOffset, writingMode());

    if (m_offsetBetweenRows.positionOffset < 0) {
        if (isHorizontalWritingMode())
            allowance.setTop(-m_offsetBetweenRows.positionOffset);
        else
            allowance.setLeft(-m_offsetBetweenRows.positionOffset);
    }

    return allowance;
}

LayoutUnit RenderGrid::translateRTLCoordinate(LayoutUnit coordinate) const
{
    LayoutUnit width = borderLogicalLeft() + borderLogicalRight() + clientLogicalWidth();

#if !PLATFORM(IOS_FAMILY)
    // FIXME: Ideally scrollbarLogicalWidth() should return zero in iOS so we don't need this
    // (see bug https://webkit.org/b/191857).
    // If we are in horizontal writing mode and RTL direction the scrollbar is painted on the left,
    // so we need to take into account when computing the position of the columns.
    if (writingMode().isHorizontal())
        width += scrollbarLogicalWidth();
#endif

    return width - coordinate;
}

// FIXME: SetLogicalPositionForGridItem has only one caller, consider its refactoring in the future.
void RenderGrid::setLogicalPositionForGridItem(RenderBox& gridItem) const
{
    // "In the positioning phase [...] calculations are performed according to the writing mode of the containing block of the box establishing the
    // orthogonal flow." However, 'setLogicalLocation' will only take into account the grid item's writing-mode, so the position may need to be transposed.
    LayoutPoint gridItemLocation(logicalOffsetForGridItem(gridItem, GridTrackSizingDirection::ForColumns), logicalOffsetForGridItem(gridItem, GridTrackSizingDirection::ForRows));
    gridItem.setLogicalLocation(GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItemLocation.transposedPoint() : gridItemLocation);
}

void RenderGrid::setLogicalOffsetForGridItem(RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    if (gridItem.parent() != this && hasStaticPositionForGridItem(gridItem, direction))
        return;
    // 'setLogicalLeft' and 'setLogicalTop' only take into account the grid item's writing-mode, that's why 'flowAwareDirectionForGridItem' is needed.
    if (GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, direction) == GridTrackSizingDirection::ForColumns)
        gridItem.setLogicalLeft(logicalOffsetForGridItem(gridItem, direction));
    else
        gridItem.setLogicalTop(logicalOffsetForGridItem(gridItem, direction));
}

LayoutUnit RenderGrid::logicalOffsetForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    if (direction == GridTrackSizingDirection::ForRows)
        return columnAxisOffsetForGridItem(gridItem);
    LayoutUnit rowAxisOffset = rowAxisOffsetForGridItem(gridItem);
    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (writingMode().isInlineFlipped())
        rowAxisOffset = translateRTLCoordinate(rowAxisOffset) - (GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalHeight()  : gridItem.logicalWidth());
    return rowAxisOffset;
}

unsigned RenderGrid::nonCollapsedTracks(GridTrackSizingDirection direction) const
{
    auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    size_t numberOfTracks = tracks.size();
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? currentGrid().autoRepeatEmptyTracks(direction)->size() : 0;
    return numberOfTracks - numberOfCollapsedTracks;
}

unsigned RenderGrid::numTracks(GridTrackSizingDirection direction) const
{
    // Due to limitations in our internal representation, we cannot know the number of columns from
    // currentGrid *if* there is no row (because currentGrid would be empty). That's why in that case we need
    // to get it from the style. Note that we know for sure that there aren't any implicit tracks,
    // because not having rows implies that there are no "normal" grid items (out-of-flow grid items are
    // not stored in currentGrid).
    ASSERT(!currentGrid().needsItemsPlacement());
    if (direction == GridTrackSizingDirection::ForRows)
        return currentGrid().numTracks(GridTrackSizingDirection::ForRows);

    // FIXME: This still requires knowledge about currentGrid internals.
    return currentGrid().numTracks(GridTrackSizingDirection::ForRows) ? currentGrid().numTracks(GridTrackSizingDirection::ForColumns) : GridPositionsResolver::explicitGridColumnCount(*this);
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    ASSERT(!currentGrid().needsItemsPlacement());
    for (RenderBox* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next())
        paintChild(*gridItem, paintInfo, paintOffset, forChild, usePrintRect, PaintAsInlineBlock);
}

ASCIILiteral RenderGrid::renderName() const
{
    if (isFloating())
        return "RenderGrid (floating)"_s;
    if (isOutOfFlowPositioned())
        return "RenderGrid (positioned)"_s;
    if (isAnonymous())
        return "RenderGrid (generated)"_s;
    if (isRelativelyPositioned())
        return "RenderGrid (relative positioned)"_s;
    return "RenderGrid"_s;
}

bool RenderGrid::hasAutoSizeInColumnAxis(const RenderBox& gridItem) const
{
    if (gridItem.style().hasAspectRatio()) {
        // FIXME: should align-items + align-self: auto/justify-items + justify-self: auto be taken into account?
        if (isHorizontalWritingMode() == gridItem.isHorizontalWritingMode() && gridItem.style().alignSelf().position() != ItemPosition::Stretch) {
            // A non-auto inline size means the same for block size (column axis size) because of the aspect ratio.
            if (!gridItem.style().logicalWidth().isAuto())
                return false;
        } else if (gridItem.style().justifySelf().position() != ItemPosition::Stretch) {
            const Length& logicalHeight = gridItem.style().logicalHeight();
            if (logicalHeight.isFixed() || (logicalHeight.isPercentOrCalculated() && gridItem.percentageLogicalHeightIsResolvable()))
                return false;
        }
    }
    return isHorizontalWritingMode() ? gridItem.style().height().isAuto() : gridItem.style().width().isAuto();
}

bool RenderGrid::hasAutoSizeInRowAxis(const RenderBox& gridItem) const
{
    if (gridItem.style().hasAspectRatio()) {
        // FIXME: should align-items + align-self: auto/justify-items + justify-self: auto be taken into account?
        if (isHorizontalWritingMode() == gridItem.isHorizontalWritingMode() && gridItem.style().justifySelf().position() != ItemPosition::Stretch) {
            // A non-auto block size means the same for inline size (row axis size) because of the aspect ratio.
            const Length& logicalHeight = gridItem.style().logicalHeight();
            if (logicalHeight.isFixed() || (logicalHeight.isPercentOrCalculated() && gridItem.percentageLogicalHeightIsResolvable()))
                return false;
        } else if (gridItem.style().alignSelf().position() != ItemPosition::Stretch) {
            if (!gridItem.style().logicalWidth().isAuto())
                return false;
        }
    }
    return isHorizontalWritingMode() ? gridItem.style().width().isAuto() : gridItem.style().height().isAuto();
}

bool RenderGrid::computeGridPositionsForOutOfFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction, int& startLine, bool& startIsAuto, int& endLine, bool& endIsAuto) const
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    int lastLine = numTracks(direction);
    GridSpan span = GridPositionsResolver::resolveGridPositionsFromStyle(*this, gridItem, direction);
    if (span.isIndefinite())
        return false;

    unsigned explicitStart = currentGrid().explicitGridStart(direction);
    startLine = span.untranslatedStartLine() + explicitStart;
    endLine = span.untranslatedEndLine() + explicitStart;

    GridPosition startPosition = direction == GridTrackSizingDirection::ForColumns ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    GridPosition endPosition = direction == GridTrackSizingDirection::ForColumns ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();

    startIsAuto = startPosition.isAuto() || startLine < 0 || startLine > lastLine;
    endIsAuto = endPosition.isAuto() || endLine < 0 || endLine > lastLine;
    return true;
}

GridSpan RenderGrid::gridSpanForOutOfFlowGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    int lastLine = numTracks(direction);
    int startLine, endLine;
    bool startIsAuto, endIsAuto;
    if (!computeGridPositionsForOutOfFlowGridItem(gridItem, direction, startLine, startIsAuto, endLine, endIsAuto))
        return GridSpan::translatedDefiniteGridSpan(0, lastLine);
    return GridSpan::translatedDefiniteGridSpan(startIsAuto ? 0 : startLine, endIsAuto ? lastLine : endLine);
}

GridSpan RenderGrid::gridSpanForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    RenderGrid* renderGrid = downcast<RenderGrid>(gridItem.parent());
    // |direction| is specified relative to this grid, switch it if |gridItem|'s direct parent grid
    // is using a different writing mode.
    direction = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, *renderGrid, direction);
    GridSpan span = gridItem.isOutOfFlowPositioned() ? renderGrid->gridSpanForOutOfFlowGridItem(gridItem, direction) : renderGrid->currentGrid().gridItemSpan(gridItem, direction);

    while (renderGrid != this) {
        RenderGrid* parent = downcast<RenderGrid>(renderGrid->parent());

        bool isSubgrid = renderGrid->isSubgrid(direction);

        direction = GridLayoutFunctions::flowAwareDirectionForGridItem(*parent, *renderGrid, direction);

        GridSpan parentSpan = renderGrid->isOutOfFlowPositioned() ? parent->gridSpanForOutOfFlowGridItem(*renderGrid, direction) :  parent->currentGrid().gridItemSpan(*renderGrid, direction);
        if (isSubgrid)
            span.translateTo(parentSpan, GridLayoutFunctions::isSubgridReversedDirection(*parent, direction, *renderGrid));
        else
            span = parentSpan;
        renderGrid = parent;
    }
    return span;
}

bool RenderGrid::establishesIndependentFormattingContext() const
{
    // Grid items establish a new independent formatting context, unless
    // they're a subgrid
    // https://drafts.csswg.org/css-grid-2/#grid-item-display
    if (isGridItem()) {
        if (!isSubgridRows() && !isSubgridColumns())
            return true;
    }
    return RenderElement::establishesIndependentFormattingContext();
}

RenderGrid::GridWrapper::GridWrapper(RenderGrid& renderGrid)
    : m_layoutGrid(renderGrid)
{ }

void RenderGrid::GridWrapper::resetCurrentGrid() const
{
    m_currentGrid = std::ref(const_cast<Grid&>(m_layoutGrid));
}

void RenderGrid::computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats)
{
    RenderBlock::computeOverflow(oldClientAfterEdge, recomputeFloats);

    if (!hasPotentiallyScrollableOverflow() || isMasonry() || isSubgridRows() || isSubgridColumns())
        return;

    // FIXME: We should handle RTL and other writing modes also.
    if (writingMode().isBidiLTR() && isHorizontalWritingMode()) {
        auto gridAreaSize = LayoutSize { m_columnPositions.last(), m_rowPositions.last() };
        gridAreaSize += { paddingEnd(), paddingAfter() };
        addLayoutOverflow({ { }, gridAreaSize });
    }
}

} // namespace WebCore

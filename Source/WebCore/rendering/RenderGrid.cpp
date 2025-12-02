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

#include "BaselineAlignmentInlines.h"
#include "GridArea.h"
#include "GridLayoutFunctions.h"
#include "GridMasonryLayout.h"
#include "GridTrackSizingAlgorithm.h"
#include "HitTestLocation.h"
#include "LayoutIntegrationGridCoverage.h"
#include "LayoutIntegrationGridLayout.h"
#include "LayoutRange.h"
#include "LayoutRepainter.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleGridPositionsResolver.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <ranges>
#include <wtf/Range.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

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

bool RenderGrid::isExtrinsicallySized() const
{
    auto& gridStyle = style();
    auto allTracksAreExtrinsicallySized = [&] {
        for (auto& column : gridStyle.gridTemplateColumns().sizes) {
            if (column.isContentSized())
                return false;
        }

        for (auto& row : gridStyle.gridTemplateRows().sizes) {
            if (row.isContentSized())
                return false;
        }
        return true;
    };

    // Since we currently only check if the grid's logical width is auto, it being
    // extrinsically sized in this regard depends on the formatting context it
    // participates in. For now we only check and allow if it participates in block
    // layout since that is simple.
    auto participatesInBlockLayout = [&] {
        auto* containingBlock = this->containingBlock();
        return containingBlock && containingBlock->isBlockContainer() && !containingBlock->childrenInline();
    };

    if (!gridStyle.logicalWidth().isAuto()
        || !participatesInBlockLayout()
        || !gridStyle.logicalHeight().isFixed()
        || !allTracksAreExtrinsicallySized()
        || gridStyle.hasAspectRatio()
        || isSubgrid()
        || isMasonry())
        return false;

    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
        // FIXME: If we do not need to perform item placement we should be able
        // to check any implicitly created tracks as well.
        if (!isPlacedWithinExtrinsicallySizedExplicitTracks(gridItem))
            return false;
    }

    return true;
}

StyleSelfAlignmentData RenderGrid::selfAlignmentForGridItem(Style::GridTrackSizingDirection alignmentContextType, const RenderBox& gridItem, const RenderStyle* gridStyle) const
{
    return alignmentContextType == Style::GridTrackSizingDirection::Columns ? justifySelfForGridItem(gridItem, StretchingMode::Any, gridStyle) : alignSelfForGridItem(gridItem, StretchingMode::Any, gridStyle);
}

bool RenderGrid::selfAlignmentChangedToStretch(Style::GridTrackSizingDirection alignmentContextType, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& gridItem) const
{
    return selfAlignmentForGridItem(alignmentContextType, gridItem, &oldStyle).position() != ItemPosition::Stretch
        && selfAlignmentForGridItem(alignmentContextType, gridItem, &newStyle).position() == ItemPosition::Stretch;
}

bool RenderGrid::selfAlignmentChangedFromStretch(Style::GridTrackSizingDirection alignmentContextType, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& gridItem) const
{
    return selfAlignmentForGridItem(alignmentContextType, gridItem, &oldStyle).position() == ItemPosition::Stretch
        && selfAlignmentForGridItem(alignmentContextType, gridItem, &newStyle).position() != ItemPosition::Stretch;
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifference::Layout)
        return;

    m_intrinsicLogicalHeightsForRowSizingFirstPass.reset();
    const RenderStyle& newStyle = this->style();

    auto hasDifferentTrackSizes = [&newStyle, &oldStyle](Style::GridTrackSizingDirection direction) {
        return newStyle.gridTemplateList(direction).sizes != oldStyle->gridTemplateList(direction).sizes;
    };

    if (hasDifferentTrackSizes(Style::GridTrackSizingDirection::Columns) || hasDifferentTrackSizes(Style::GridTrackSizingDirection::Rows)) {
        for (auto& gridItem : childrenOfType<RenderBox>(*this))
            gridItem.setChildNeedsLayout();
    }

    if (oldStyle->alignItems().resolve(selfAlignmentNormalBehavior(this)).position() == ItemPosition::Stretch) {
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

            if (selfAlignmentChangedToStretch(Style::GridTrackSizingDirection::Columns, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedFromStretch(Style::GridTrackSizingDirection::Columns, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedToStretch(Style::GridTrackSizingDirection::Rows, *oldStyle, newStyle, gridItem)
                || selfAlignmentChangedFromStretch(Style::GridTrackSizingDirection::Rows, *oldStyle, newStyle, gridItem)) {
                gridItem.setNeedsLayout();
            }
        }
    }

    auto subgridDidChange = this->subgridDidChange(*oldStyle);
    auto isSubgridWithIndependentFormattingContextChange = [&] {
        if (newStyle.gridTemplateRows().subgrid || newStyle.gridTemplateColumns().subgrid)
            return establishesIndependentFormattingContextIgnoringDisplayType(*oldStyle) != establishesIndependentFormattingContextIgnoringDisplayType(style());
        return false;
    };
    if (explicitGridDidResize(*oldStyle)
        || namedGridLinesDefinitionDidChange(*oldStyle)
        || implicitGridLinesDefinitionDidChange(*oldStyle)
        || oldStyle->gridAutoFlow() != style().gridAutoFlow()
        || style().gridTemplateColumns().autoRepeatSizes.size()
        || style().gridTemplateRows().autoRepeatSizes.size()
        || subgridDidChange == SubgridDidChange::Yes
        || isSubgridWithIndependentFormattingContextChange())
        setNeedsItemPlacement(subgridDidChange);
}

SubgridDidChange RenderGrid::subgridDidChange(const RenderStyle& oldStyle) const
{
    if (oldStyle.gridTemplateRows().subgrid != style().gridTemplateRows().subgrid
        || oldStyle.gridTemplateColumns().subgrid != style().gridTemplateColumns().subgrid)
        return SubgridDidChange::Yes;
    return SubgridDidChange::No;
}

bool RenderGrid::explicitGridDidResize(const RenderStyle& oldStyle) const
{
    auto& oldGridTemplateColumns = oldStyle.gridTemplateColumns();
    auto& oldGridTemplateRows = oldStyle.gridTemplateRows();
    auto& oldGridTemplateAreas = oldStyle.gridTemplateAreas();
    auto& newGridTemplateColumns = style().gridTemplateColumns();
    auto& newGridTemplateRows = style().gridTemplateRows();
    auto& newGridTemplateAreas = style().gridTemplateAreas();

    return oldGridTemplateColumns.sizes.size() != newGridTemplateColumns.sizes.size()
        || oldGridTemplateColumns.autoRepeatSizes.size() != newGridTemplateColumns.autoRepeatSizes.size()
        || oldGridTemplateRows.sizes.size() != newGridTemplateRows.sizes.size()
        || oldGridTemplateRows.autoRepeatSizes.size() != newGridTemplateRows.autoRepeatSizes.size()
        || oldGridTemplateAreas.map.columnCount != newGridTemplateAreas.map.columnCount
        || oldGridTemplateAreas.map.rowCount != newGridTemplateAreas.map.rowCount;
}

bool RenderGrid::namedGridLinesDefinitionDidChange(const RenderStyle& oldStyle) const
{
    return oldStyle.gridTemplateRows().namedLines.map != style().gridTemplateRows().namedLines.map
        || oldStyle.gridTemplateColumns().namedLines.map != style().gridTemplateColumns().namedLines.map;
}

bool RenderGrid::implicitGridLinesDefinitionDidChange(const RenderStyle& oldStyle) const
{
    auto& oldGridTemplateAreas = oldStyle.gridTemplateAreas();
    auto& newGridTemplateAreas = style().gridTemplateAreas();

    return oldGridTemplateAreas.implicitNamedGridRowLines != newGridTemplateAreas.implicitNamedGridRowLines
        || oldGridTemplateAreas.implicitNamedGridColumnLines != newGridTemplateAreas.implicitNamedGridColumnLines;
}

// This method optimizes the gutters computation by skipping the available size
// call if gaps are fixed size (it's only needed for percentages).
std::optional<LayoutUnit> RenderGrid::availableSpaceForGutters(Style::GridTrackSizingDirection direction) const
{
    if (!style().gap(direction).isPercentOrCalculated())
        return std::nullopt;

    return direction == Style::GridTrackSizingDirection::Columns ? contentBoxLogicalWidth() : contentBoxLogicalHeight();
}

void RenderGrid::computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection direction, LayoutUnit availableSpace, GridLayoutState& gridLayoutState)
{
    auto autoMarginResolutionScope = SetForScope(m_isComputingTrackSizes, true);
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
    if (gridLayoutState.needsSecondTrackSizingPass() || m_trackSizingAlgorithm.hasAnyPercentSizedRowsIndefiniteHeight() || (m_trackSizingAlgorithm.hasAnyFlexibleMaxTrackBreadth() && !m_trackSizingAlgorithm.hasAnyBaselineAlignmentItem()) || gridLayoutState.hasAspectRatioBlockSizeDependentItem()) {

        populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows);
        computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Columns, availableSpaceForColumns, gridLayoutState);
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Columns, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Columns).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Columns));

        computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Rows, availableSpaceForRows, gridLayoutState);
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Rows, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Rows).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Rows));
    }
}

bool RenderGrid::canPerformSimplifiedLayout() const
{
    // We cannot perform a simplified layout if we need to position the items and we have some
    // positioned items to be laid out.
    if (currentGrid().needsItemsPlacement() && outOfFlowChildNeedsLayout())
        return false;

    return RenderBlock::canPerformSimplifiedLayout();
}

enum class AlignmentContextTypes : uint8_t {
    Columns = 1 << 0,
    Rows = 1 << 1
};

template<typename F>
static void cacheBaselineAlignedGridItems(const RenderGrid& grid, GridTrackSizingAlgorithm& algorithm, OptionSet<AlignmentContextTypes> alignmentContextTypes, F& callback, bool cachingRowSubgridsForRootGrid)
{
    ASSERT_IMPLIES(cachingRowSubgridsForRootGrid, !algorithm.renderGrid()->isSubgridRows() && (algorithm.renderGrid() == &grid || grid.isSubgridOf(GridLayoutFunctions::flowAwareDirectionForGridItem(*algorithm.renderGrid(), grid, Style::GridTrackSizingDirection::Rows), *algorithm.renderGrid())));

    for (auto& gridItem : childrenOfType<RenderBox>(grid)) {
        if (gridItem.isOutOfFlowPositioned() || gridItem.isLegend())
            continue;

        callback(const_cast<RenderBox*>(&gridItem));

        // We keep a cache of items with baseline as alignment values so that we only compute the baseline shims for
        // such items. This cache is needed for performance related reasons due to the cost of evaluating the item's
        // participation in a baseline context during the track sizing algorithm.
        OptionSet<AlignmentContextTypes> innerAlignmentContextTypes = { };
        CheckedPtr inner = dynamicDowncast<RenderGrid>(gridItem);

        if (alignmentContextTypes.contains(AlignmentContextTypes::Rows)) {
            if (inner && inner->isSubgridInParentDirection(Style::GridTrackSizingDirection::Rows))
                innerAlignmentContextTypes.add(GridLayoutFunctions::isOrthogonalGridItem(grid, gridItem) ? AlignmentContextTypes::Columns : AlignmentContextTypes::Rows);
            else if (grid.isBaselineAlignmentForGridItem(gridItem, Style::GridTrackSizingDirection::Rows))
                algorithm.cacheBaselineAlignedItem(gridItem, Style::GridTrackSizingDirection::Rows, cachingRowSubgridsForRootGrid);
        }

        if (alignmentContextTypes.contains(AlignmentContextTypes::Columns)) {
            if (inner && inner->isSubgridInParentDirection(Style::GridTrackSizingDirection::Columns))
                innerAlignmentContextTypes.add(GridLayoutFunctions::isOrthogonalGridItem(grid, gridItem) ? AlignmentContextTypes::Rows : AlignmentContextTypes::Columns);
            else if (grid.isBaselineAlignmentForGridItem(gridItem, Style::GridTrackSizingDirection::Columns))
                algorithm.cacheBaselineAlignedItem(gridItem, Style::GridTrackSizingDirection::Columns, cachingRowSubgridsForRootGrid);
        }

        if (inner && cachingRowSubgridsForRootGrid)
            cachingRowSubgridsForRootGrid = GridLayoutFunctions::isOrthogonalGridItem(*algorithm.renderGrid(), *inner) ? inner->isSubgridColumns() : inner->isSubgridRows();

        if (innerAlignmentContextTypes)
            cacheBaselineAlignedGridItems(*inner, algorithm, innerAlignmentContextTypes, callback, cachingRowSubgridsForRootGrid);
    }
}

Vector<RenderBox*> RenderGrid::computeAspectRatioDependentAndBaselineItems(GridLayoutState& gridLayoutState)
{
    Vector<RenderBox*> dependentGridItems;

    m_baselineItemsCached = true;

    auto computeOrthogonalAndDependentItems = [&](RenderBox* gridItem) {
        // For a grid item that has an aspect-ratio and block-constraints such as the relative logical height,
        // when the grid width is auto, we may need get the real grid width before laying out the item.
        if (GridLayoutFunctions::isAspectRatioBlockSizeDependentGridItem(*gridItem) && (style().logicalWidth().isAuto() || style().logicalWidth().isMinContent() || style().logicalWidth().isMaxContent())) {
            dependentGridItems.append(gridItem);
            gridLayoutState.setHasAspectRatioBlockSizeDependentItem();
        }
    };

    cacheBaselineAlignedGridItems(*this, m_trackSizingAlgorithm, { AlignmentContextTypes::Columns, AlignmentContextTypes::Rows }, computeOrthogonalAndDependentItems, !isSubgridRows());
    return dependentGridItems;
}

bool RenderGrid::canSetColumnAxisStretchRequirementForItem(const RenderBox& gridItem) const
{
    auto gridItemBlockFlowDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, Style::GridTrackSizingDirection::Rows);
    return gridItemBlockFlowDirection == Style::GridTrackSizingDirection::Rows && GridLayoutFunctions::allowedToStretchGridItemAlongColumnAxis(gridItem, alignSelfForGridItem(gridItem).position(), writingMode());
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

void RenderGrid::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    // The layoutBlock was handling the layout of both the grid and masonry implementations.
    // This caused a huge amount of branching code to handle masonry specific cases. Splitting up the code
    // to layout will simplify both implementations.
    if (!isMasonry())
        layoutGrid(relayoutChildren);
    else
        layoutMasonry(relayoutChildren);
}

static void clearGridItemOverridingSizesBeforeLayout(RenderGrid& renderGrid)
{
    // Grid's layout logic controls the grid item's override content size, hence we need to
    // clear any override set previously, so it doesn't interfere in current layout
    // execution.
    for (auto& gridItem : childrenOfType<RenderBox>(renderGrid)) {
        if (gridItem.isOutOfFlowPositioned() || gridItem.isLegend())
            continue;
        gridItem.clearOverridingSize();
    }
}


bool RenderGrid::hasDefiniteLogicalHeight() const
{
    // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() only but it does not work for out of flow content.
    return RenderBlock::hasDefiniteLogicalHeight() || overridingBorderBoxLogicalHeight() || computeContentLogicalHeight(style().logicalHeight(), std::nullopt) || shouldComputeLogicalHeightFromAspectRatio();
}

const std::optional<LayoutUnit> RenderGrid::availableLogicalHeightForContentBox() const
{
    if (!hasDefiniteLogicalHeight())
        return { };

    if (auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight())
        return constrainContentBoxLogicalHeightByMinMax(*overridingLogicalHeight - borderAndPaddingLogicalHeight(), { });
    return availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
}

void RenderGrid::layoutGrid(RelayoutChildren relayoutChildren)
{

    LayoutRepainter repainter(*this);
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());

        GridLayoutState gridLayoutState;

        updateIntrinsicLogicalHeightsForRowSizingFirstPassCacheAvailability();
        clearGridItemOverridingSizesBeforeLayout(*this);
        computeLayoutRequirementsForItemsBeforeLayout(gridLayoutState);

        preparePaginationBeforeBlockLayout(relayoutChildren);
        beginUpdateScrollInfoAfterLayoutTransaction();

        LayoutSize previousSize = size();

        auto aspectRatioBlockSizeDependentGridItems = computeAspectRatioDependentAndBaselineItems(gridLayoutState);

        resetLogicalHeightBeforeLayoutIfNeeded();

        updateLogicalWidth();

        if (layoutUsingGridFormattingContext())
            return;

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        LayoutUnit availableSpaceForColumns = contentBoxLogicalWidth();
        placeItemsOnGrid(availableSpaceForColumns);

        m_trackSizingAlgorithm.setAvailableSpace(Style::GridTrackSizingDirection::Columns, availableSpaceForColumns);
        performPreLayoutForGridItems(m_trackSizingAlgorithm, ShouldUpdateGridAreaLogicalSize::Yes);

        // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns. At this point the
        // logical width is always definite as the above call to updateLogicalWidth() properly resolves intrinsic
        // sizes. We cannot do the same for heights though because many code paths inside updateLogicalHeight() require
        // a previous call to setLogicalHeight() to resolve heights properly (like for positioned items for example).
        computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Columns, availableSpaceForColumns, gridLayoutState);

        // 1.5. Compute Content Distribution offsets for column tracks
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Columns, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Columns).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Columns));

        // 2. Next, the track sizing algorithm resolves the sizes of the grid rows,
        // using the grid column sizes calculated in the previous step.
        auto availableLogicalHeightForContentBox = this->availableLogicalHeightForContentBox();
        bool shouldRecomputeHeight = false;
        if (!availableLogicalHeightForContentBox) {
            computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows, gridLayoutState);
            if (shouldApplySizeContainment())
                shouldRecomputeHeight = true;
        } else
            computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Rows, *availableLogicalHeightForContentBox, gridLayoutState);

        LayoutUnit trackBasedLogicalHeight = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (auto size = explicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection::Rows))
            trackBasedLogicalHeight += size.value();
        else
            trackBasedLogicalHeight += m_trackSizingAlgorithm.computeTrackBasedSize();

        if (shouldRecomputeHeight)
            computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Rows, trackBasedLogicalHeight, gridLayoutState);

        setLogicalHeight(trackBasedLogicalHeight);

        updateLogicalHeight();

        // Once grid's indefinite height is resolved, we can compute the
        // available free space for Content Alignment.
        if (!availableLogicalHeightForContentBox)
            m_trackSizingAlgorithm.setFreeSpace(Style::GridTrackSizingDirection::Rows, logicalHeight() - trackBasedLogicalHeight);

        // 2.5. Compute Content Distribution offsets for rows tracks
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Rows, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Rows).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Rows));

        if (!aspectRatioBlockSizeDependentGridItems.isEmpty()) {
            updateGridAreaForAspectRatioItems(aspectRatioBlockSizeDependentGridItems, gridLayoutState);
            updateLogicalWidth();
        }

        // 3. If the min-content contribution of any grid items have changed based on the row
        // sizes calculated in step 2, steps 1 and 2 are repeated with the new min-content
        // contribution (once only).
        repeatTracksSizingIfNeeded(availableSpaceForColumns, contentBoxLogicalHeight(), gridLayoutState);

        // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
        if (hasLineIfEmpty()) {
            LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
                + lineHeight()
                + scrollbarLogicalHeight();
            setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
        }

        layoutGridItems(gridLayoutState);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (size() != previousSize)
            relayoutChildren = RelayoutChildren::Yes;

        if (isDocumentElementRenderer())
            layoutOutOfFlowBoxes(RelayoutChildren::Yes);
        else
            layoutOutOfFlowBoxes(relayoutChildren);

        m_trackSizingAlgorithm.reset();

        computeOverflow(layoutOverflowLogicalBottom(*this));

        updateDescendantTransformsAfterLayout();
    }

    updateLayerTransform();

    repainter.repaintAfterLayout();

    m_trackSizingAlgorithm.clearBaselineItemsCache();
    m_baselineItemsCached = false;
}

bool RenderGrid::layoutUsingGridFormattingContext()
{
    if (!m_hasGridFormattingContextLayout.has_value())
        m_hasGridFormattingContextLayout = LayoutIntegration::canUseForGridLayout(*this);

    if (!*m_hasGridFormattingContextLayout)
        return false;

    auto gridLayout = LayoutIntegration::GridLayout { *this };
    gridLayout.updateFormattingContextGeometries();

    gridLayout.layout();
    updateLogicalHeight();
    return true;
}

void RenderGrid::layoutMasonry(RelayoutChildren relayoutChildren)
{
    LayoutRepainter repainter(*this);
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());
        GridLayoutState gridLayoutState;

        clearGridItemOverridingSizesBeforeLayout(*this);

        preparePaginationBeforeBlockLayout(relayoutChildren);
        beginUpdateScrollInfoAfterLayoutTransaction();

        LayoutSize previousSize = size();

        // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() only but it does not work for positioned stuff.
        // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
        // FIXME: We might need to cache the hasDefiniteLogicalHeight if the call of RenderBlock::hasDefiniteLogicalHeight() causes a relevant performance regression.
        bool hasDefiniteLogicalHeight = RenderBlock::hasDefiniteLogicalHeight() || overridingBorderBoxLogicalHeight() || computeContentLogicalHeight(style().logicalHeight(), std::nullopt);

        auto aspectRatioBlockSizeDependentGridItems = computeAspectRatioDependentAndBaselineItems(gridLayoutState);

        resetLogicalHeightBeforeLayoutIfNeeded();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        updateLogicalWidth();

        LayoutUnit availableSpaceForColumns = contentBoxLogicalWidth();
        placeItemsOnGrid(availableSpaceForColumns);

        m_trackSizingAlgorithm.setAvailableSpace(Style::GridTrackSizingDirection::Columns, availableSpaceForColumns);
        performPreLayoutForGridItems(m_trackSizingAlgorithm, ShouldUpdateGridAreaLogicalSize::Yes);

        // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns. At this point the
        // logical width is always definite as the above call to updateLogicalWidth() properly resolves intrinsic
        // sizes. We cannot do the same for heights though because many code paths inside updateLogicalHeight() require
        // a previous call to setLogicalHeight() to resolve heights properly (like for positioned items for example).
        computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Columns, availableSpaceForColumns, gridLayoutState);

        // 1.5. Compute Content Distribution offsets for column tracks
        m_offsetBetweenColumns = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Columns, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Columns).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Columns));

        // 2. Next, the track sizing algorithm resolves the sizes of the grid rows,
        // using the grid column sizes calculated in the previous step.
        bool shouldRecomputeHeight = false;
        if (!hasDefiniteLogicalHeight) {
            computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows, gridLayoutState);
            if (shouldApplySizeContainment())
                shouldRecomputeHeight = true;
        } else
            computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Rows, availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding), gridLayoutState);

        auto performMasonryPlacement = [&](const Style::GridTrackSizingDirection masonryAxisDirection) {
            auto gridAxisDirection = masonryAxisDirection == Style::GridTrackSizingDirection::Rows ? Style::GridTrackSizingDirection::Columns : Style::GridTrackSizingDirection::Rows;
            unsigned gridAxisTracksBeforeAutoPlacement = currentGrid().numTracks(gridAxisDirection);

            m_masonryLayout.performMasonryPlacement(m_trackSizingAlgorithm, gridAxisTracksBeforeAutoPlacement, masonryAxisDirection, GridMasonryLayout::MasonryLayoutPhase::LayoutPhase);
        };

        if (areMasonryRows())
            performMasonryPlacement(Style::GridTrackSizingDirection::Rows);
        else if (areMasonryColumns())
            performMasonryPlacement(Style::GridTrackSizingDirection::Columns);

        LayoutUnit trackBasedLogicalHeight = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        if (auto size = explicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection::Rows))
            trackBasedLogicalHeight += size.value();
        else {
            if (areMasonryRows())
                trackBasedLogicalHeight += m_masonryLayout.gridContentSize();
            else
                trackBasedLogicalHeight += m_trackSizingAlgorithm.computeTrackBasedSize();
        }
        if (shouldRecomputeHeight)
            computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection::Rows, trackBasedLogicalHeight, gridLayoutState);

        setLogicalHeight(trackBasedLogicalHeight);

        updateLogicalHeight();

        // Once grid's indefinite height is resolved, we can compute the
        // available free space for Content Alignment.
        if (!hasDefiniteLogicalHeight || areMasonryRows())
            m_trackSizingAlgorithm.setFreeSpace(Style::GridTrackSizingDirection::Rows, logicalHeight() - trackBasedLogicalHeight);

        // 2.5. Compute Content Distribution offsets for rows tracks
        m_offsetBetweenRows = computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection::Rows, m_trackSizingAlgorithm.freeSpace(Style::GridTrackSizingDirection::Rows).value(), nonCollapsedTracks(Style::GridTrackSizingDirection::Rows));

        if (!aspectRatioBlockSizeDependentGridItems.isEmpty()) {
            updateGridAreaForAspectRatioItems(aspectRatioBlockSizeDependentGridItems, gridLayoutState);
            updateLogicalWidth();
        }

        // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
        if (hasLineIfEmpty()) {
            LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
                + lineHeight()
                + scrollbarLogicalHeight();
            setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
        }

        layoutMasonryItems(gridLayoutState);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (size() != previousSize)
            relayoutChildren = RelayoutChildren::Yes;

        if (isDocumentElementRenderer())
            layoutOutOfFlowBoxes(RelayoutChildren::Yes);
        else
            layoutOutOfFlowBoxes(relayoutChildren);

        m_trackSizingAlgorithm.reset();

        computeOverflow(layoutOverflowLogicalBottom(*this));

        updateDescendantTransformsAfterLayout();
    }

    updateLayerTransform();

    repainter.repaintAfterLayout();

    m_trackSizingAlgorithm.clearBaselineItemsCache();
    m_baselineItemsCached = false;
}

LayoutUnit RenderGrid::gridGap(Style::GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    ASSERT(!availableSize || *availableSize >= 0);
    auto& gap = style().gap(direction);
    if (gap.isNormal()) {
        if (!isSubgrid(direction))
            return 0_lu;

        auto parentDirection = GridLayoutFunctions::flowAwareDirectionForParent(*this, *parent(), direction);
        if (!availableSize)
            return downcast<RenderGrid>(parent())->gridGap(parentDirection, std::nullopt);
        return downcast<RenderGrid>(parent())->gridGap(parentDirection);
    }

    return Style::evaluate<LayoutUnit>(gap, availableSize.value_or(0_lu), Style::ZoomNeeded { });
}

LayoutUnit RenderGrid::gridGap(Style::GridTrackSizingDirection direction) const
{
    return gridGap(direction, availableSpaceForGutters(direction));
}

LayoutUnit RenderGrid::gridItemOffset(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Rows ? m_offsetBetweenRows.distributionOffset : m_offsetBetweenColumns.distributionOffset;
}

LayoutUnit RenderGrid::guttersSize(Style::GridTrackSizingDirection direction, unsigned startLine, unsigned span, std::optional<LayoutUnit> availableSize) const
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
        algorithm.copyBaselineItemsCache(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Columns);
    else {
        auto emptyCallback = [](RenderBox*) { };
        cacheBaselineAlignedGridItems(*this, algorithm, { AlignmentContextTypes::Columns }, emptyCallback, !isSubgridRows());
    }

    computeTrackSizesForIndefiniteSize(algorithm, Style::GridTrackSizingDirection::Columns, gridLayoutState, &minLogicalWidth, &maxLogicalWidth);

    if (isMasonry(Style::GridTrackSizingDirection::Columns)) {
        // The track sizing algorithm will only be run once in this case, since track sizing will not run in the masonry direction.
        computeTrackSizesForIndefiniteSize(algorithm, Style::GridTrackSizingDirection::Rows, gridLayoutState, &minLogicalWidth, &maxLogicalWidth);

        auto gridAxisTracksCountBeforeAutoPlacement = currentGrid().numTracks(Style::GridTrackSizingDirection::Rows);

        // To determine the width of the grid when we have a masonry layout in the column direction we need to perform a layout with the min and max
        // content sizes. We will override the grid items widths to accomplish this and then calculate the final grid content size after placement.
        m_masonryLayout.performMasonryPlacement(algorithm, gridAxisTracksCountBeforeAutoPlacement, Style::GridTrackSizingDirection::Columns, GridMasonryLayout::MasonryLayoutPhase::MinContentPhase);
        minLogicalWidth = m_masonryLayout.gridContentSize();

        m_masonryLayout.performMasonryPlacement(algorithm, gridAxisTracksCountBeforeAutoPlacement, Style::GridTrackSizingDirection::Columns, GridMasonryLayout::MasonryLayoutPhase::MaxContentPhase);
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

void RenderGrid::computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm& algorithm, Style::GridTrackSizingDirection direction, GridLayoutState& gridLayoutState, LayoutUnit* minIntrinsicSize, LayoutUnit* maxIntrinsicSize) const
{
    auto autoMarginResolutionScope = SetForScope(m_isComputingTrackSizes, true);
    algorithm.run(direction, numTracks(direction), SizingOperation::IntrinsicSizeComputation, std::nullopt, gridLayoutState);

    size_t numberOfTracks = algorithm.tracks(direction).size();
    LayoutUnit totalGuttersSize = direction == Style::GridTrackSizingDirection::Columns && explicitIntrinsicInnerLogicalSize(direction).has_value() ? 0_lu : guttersSize(direction, 0, numberOfTracks, std::nullopt);

    if (minIntrinsicSize)
        *minIntrinsicSize = algorithm.minContentSize() + totalGuttersSize;
    if (maxIntrinsicSize)
        *maxIntrinsicSize = algorithm.maxContentSize() + totalGuttersSize;

    ASSERT(algorithm.tracksAreWiderThanMinTrackBreadth());
}

bool RenderGrid::shouldCheckExplicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? shouldApplySizeOrInlineSizeContainment() : shouldApplySizeContainment();
}

std::optional<LayoutUnit> RenderGrid::explicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection direction) const
{
    if (!shouldCheckExplicitIntrinsicInnerLogicalSize(direction))
        return std::nullopt;
    if (direction == Style::GridTrackSizingDirection::Columns)
        return explicitIntrinsicInnerLogicalWidth();
    return explicitIntrinsicInnerLogicalHeight();
}

unsigned RenderGrid::computeAutoRepeatTracksCount(Style::GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    ASSERT(!availableSize || availableSize.value() != -1);
    bool isRowAxis = direction == Style::GridTrackSizingDirection::Columns;
    if (isSubgrid(direction))
        return 0;

    const auto& tracks = style().gridTemplateList(direction);
    const auto& autoRepeatTracks = tracks.autoRepeatSizes;
    unsigned autoRepeatTrackListLength = autoRepeatTracks.size();

    if (!autoRepeatTrackListLength)
        return 0;

    bool needsToFulfillMinimumSize = false;
    if (!availableSize) {
        // Both min-width/height and max-width/height calculations may need the containing block size, so it is cached if lazily computed.
        std::optional<LayoutUnit> cachedContainingBlockAvailableSize;
        auto containingBlockAvailableSize = [&] -> LayoutUnit {
            if (cachedContainingBlockAvailableSize)
                return *cachedContainingBlockAvailableSize;
            cachedContainingBlockAvailableSize = isRowAxis
                ? containingBlockLogicalWidthForContent()
                : containingBlockLogicalHeightForContent(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
            return *cachedContainingBlockAvailableSize;
        };

        auto& maxSize = isRowAxis ? style().logicalMaxWidth() : style().logicalMaxHeight();
        auto availableMaxSize = WTF::switchOn(maxSize,
            [&](const Style::MaximumSize::Fixed& fixedMaxSize) -> std::optional<LayoutUnit> {
                auto maxSizeValue = LayoutUnit { fixedMaxSize.resolveZoom(style().usedZoomForLength()) };
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(maxSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(maxSizeValue);
            },
            [&](const Style::MaximumSize::Percentage& percentageMaxSize) -> std::optional<LayoutUnit> {
                auto maxSizeValue = Style::evaluate<LayoutUnit>(percentageMaxSize, containingBlockAvailableSize());
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(maxSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(maxSizeValue);
            },
            [&](const Style::MaximumSize::Calc& calcMaxSize) -> std::optional<LayoutUnit> {
                auto maxSizeValue = Style::evaluate<LayoutUnit>(calcMaxSize, containingBlockAvailableSize(), style().usedZoomForLength());
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(maxSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(maxSizeValue);
            },
            [&](const auto&) -> std::optional<LayoutUnit> {
                return { };
            }
        );

        auto& minSize = isRowAxis ? style().logicalMinWidth() : style().logicalMinHeight();
        auto& minSizeForOrthogonalAxis = isRowAxis ? style().logicalMinHeight() : style().logicalMinWidth();
        bool shouldComputeMinSizeFromAspectRatio = minSizeForOrthogonalAxis.isSpecified() && !shouldIgnoreAspectRatio();
        auto explicitIntrinsicInnerSize = explicitIntrinsicInnerLogicalSize(direction);

        if (!availableMaxSize && !minSize.isSpecified() && !shouldComputeMinSizeFromAspectRatio && !explicitIntrinsicInnerSize)
            return autoRepeatTrackListLength;

        auto availableMinSize = WTF::switchOn(minSize,
            [&](const Style::MinimumSize::Fixed& fixedMinSize) -> std::optional<LayoutUnit> {
                auto minSizeValue = LayoutUnit { fixedMinSize.resolveZoom(style().usedZoomForLength()) };
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(minSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(minSizeValue);
            },
            [&](const Style::MinimumSize::Percentage& percentageMinSize) -> std::optional<LayoutUnit> {
                auto minSizeValue = Style::evaluate<LayoutUnit>(percentageMinSize, containingBlockAvailableSize());
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(minSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(minSizeValue);
            },
            [&](const Style::MinimumSize::Calc& calcMinSize) -> std::optional<LayoutUnit> {
                auto minSizeValue = Style::evaluate<LayoutUnit>(calcMinSize, containingBlockAvailableSize(), style().usedZoomForLength());
                return isRowAxis
                    ? adjustContentBoxLogicalWidthForBoxSizing(minSizeValue)
                    : adjustContentBoxLogicalHeightForBoxSizing(minSizeValue);
            },
            [&](const auto&) -> std::optional<LayoutUnit> {
                if (!shouldComputeMinSizeFromAspectRatio)
                    return { };
                auto [logicalMinWidth, logicalMaxWidth] = computeMinMaxLogicalWidthFromAspectRatio();
                return logicalMinWidth;
            }
        );

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

        auto& minTrackSizingFunction = autoTrackSize.minTrackBreadth();
        auto& maxTrackSizingFunction = autoTrackSize.maxTrackBreadth();
        bool hasDefiniteMaxTrackSizingFunction = maxTrackSizingFunction.isLength() && !maxTrackSizingFunction.isContentSized();
        bool hasDefiniteMinTrackSizingFunction = minTrackSizingFunction.isLength() && !minTrackSizingFunction.isContentSized();

        auto contributingTrackSize = [&] {
            if (hasDefiniteMaxTrackSizingFunction && hasDefiniteMinTrackSizingFunction)
                return std::max(Style::evaluate<LayoutUnit>(minTrackSizingFunction.length(), *availableSize, Style::ZoomNeeded { }), Style::evaluate<LayoutUnit>(maxTrackSizingFunction.length(), *availableSize, Style::ZoomNeeded { }));
            return hasDefiniteMaxTrackSizingFunction ? Style::evaluate<LayoutUnit>(maxTrackSizingFunction.length(), *availableSize, Style::ZoomNeeded { }) : Style::evaluate<LayoutUnit>(minTrackSizingFunction.length(), *availableSize, Style::ZoomNeeded { });
        };
        autoRepeatTracksSize += contributingTrackSize();
    }
    // For the purpose of finding the number of auto-repeated tracks, the UA must floor the track size to a UA-specified
    // value to avoid division by zero. It is suggested that this floor be 1px.
    autoRepeatTracksSize = std::max<LayoutUnit>(1_lu, autoRepeatTracksSize);

    // There will be always at least 1 auto-repeat track, so take it already into account when computing the total track size.
    LayoutUnit tracksSize = autoRepeatTracksSize;
    auto& trackSizes = tracks.sizes;

    for (const auto& track : trackSizes) {
        bool hasDefiniteMaxTrackBreadth = track.maxTrackBreadth().isLength() && !track.maxTrackBreadth().isContentSized();
        ASSERT(hasDefiniteMaxTrackBreadth || (track.minTrackBreadth().isLength() && !track.minTrackBreadth().isContentSized()));
        tracksSize += Style::evaluate<LayoutUnit>(hasDefiniteMaxTrackBreadth ? track.maxTrackBreadth().length() : track.minTrackBreadth().length(), availableSize.value(), Style::ZoomNeeded { });
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

WTF::Range<size_t> RenderGrid::autoRepeatTracksRange(Style::GridTrackSizingDirection direction) const
{
    auto insertionPoint = style().gridTemplateList(direction).autoRepeatInsertionPoint;
    auto firstAutoRepeatTrack = insertionPoint + currentGrid().explicitGridStart(direction);
    auto lastAutoRepeatTrack = firstAutoRepeatTrack + currentGrid().autoRepeatTracks(direction);

    return { firstAutoRepeatTrack, lastAutoRepeatTrack };
}

std::unique_ptr<OrderedTrackIndexSet> RenderGrid::computeEmptyTracksForAutoRepeat(Style::GridTrackSizingDirection direction) const
{
    if (autoRepeatType(direction) != AutoRepeatType::Fit)
        return nullptr;

    std::unique_ptr<OrderedTrackIndexSet> emptyTrackIndexes;
    auto autoRepeatTracksRange = this->autoRepeatTracksRange(direction);

    if (!currentGrid().hasGridItems()) {
        emptyTrackIndexes = makeUnique<OrderedTrackIndexSet>();
        for (auto trackIndex : std::views::iota(autoRepeatTracksRange.begin(), autoRepeatTracksRange.end()))
            emptyTrackIndexes->add(trackIndex);
    } else {
        for (auto trackIndex : std::views::iota(autoRepeatTracksRange.begin(), autoRepeatTracksRange.end())) {
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

unsigned RenderGrid::clampAutoRepeatTracks(Style::GridTrackSizingDirection direction, unsigned autoRepeatTracks) const
{
    if (!autoRepeatTracks)
        return 0;

    unsigned insertionPoint = style().gridTemplateList(direction).autoRepeatInsertionPoint;
    unsigned maxTracks = static_cast<unsigned>(Style::GridPosition::max());

    if (!insertionPoint)
        return std::min(autoRepeatTracks, maxTracks);

    if (insertionPoint >= maxTracks)
        return 0;

    return std::min(autoRepeatTracks, maxTracks - insertionPoint);
}

void RenderGrid::placeItems()
{
    updateLogicalWidth();

    LayoutUnit availableSpaceForColumns = contentBoxLogicalWidth();
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

// Masonry Spec Section 2
// "If masonry is specified for both grid-template-columns and grid-template-rows, then the used value for grid-template-columns is none,
// and thus the inline axis will be the grid axis."
bool RenderGrid::isMasonry(Style::GridTrackSizingDirection direction) const
{
    // isSubgrid will return false if the masonry axis matches. Need to check style if we are a subgrid
    auto& tracks = style().gridTemplateList(direction);
    if (auto* parentGrid = dynamicDowncast<RenderGrid>(parent()); parentGrid && tracks.subgrid)
        return parentGrid->isMasonry(direction);
    if (style().display() != DisplayType::GridLanes && style().display() != DisplayType::InlineGridLanes)
        return false;
    return (direction == Style::GridTrackSizingDirection::Columns) == style().gridAutoFlow().isColumn();
}

// Masonry Spec Section 2.3.1 repeat(auto-fit)
// "repeat(auto-fit) behaves as repeat(auto-fill) when the other axis is a masonry axis."
// We need to lie here that we are really an auto-fill instead of an auto-fit.
AutoRepeatType RenderGrid::autoRepeatType(Style::GridTrackSizingDirection direction) const
{
    auto autoRepeatType = style().gridTemplateList(direction).autoRepeatType;

    if (isMasonry(orthogonalDirection(direction)) && autoRepeatType == AutoRepeatType::Fit)
        return AutoRepeatType::Fill;

    return autoRepeatType;
}

// FIXME: We shouldn't have to pass the available logical width as argument. The problem is that
// contentBoxLogicalWidth() does always return a value even if we cannot resolve it like when
// computing the intrinsic size (preferred widths). That's why we pass the responsibility to the
// caller who does know whether the available logical width is indefinite or not.
void RenderGrid::placeItemsOnGrid(std::optional<LayoutUnit> availableLogicalWidth)
{
    unsigned autoRepeatColumns = computeAutoRepeatTracksCount(Style::GridTrackSizingDirection::Columns, availableLogicalWidth);
    unsigned autoRepeatRows = computeAutoRepeatTracksCount(Style::GridTrackSizingDirection::Rows, availableLogicalHeightForPercentageComputation());
    autoRepeatRows = clampAutoRepeatTracks(Style::GridTrackSizingDirection::Rows, autoRepeatRows);
    autoRepeatColumns = clampAutoRepeatTracks(Style::GridTrackSizingDirection::Columns, autoRepeatColumns);

    if (isSubgridInParentDirection(Style::GridTrackSizingDirection::Columns) || isSubgridInParentDirection(Style::GridTrackSizingDirection::Rows)) {
        auto* parent = dynamicDowncast<RenderGrid>(this->parent());
        if (parent && parent->currentGrid().needsItemsPlacement())
            currentGrid().setNeedsItemsPlacement(true);
    }

    if (autoRepeatColumns != currentGrid().autoRepeatTracks(Style::GridTrackSizingDirection::Columns)
        || autoRepeatRows != currentGrid().autoRepeatTracks(Style::GridTrackSizingDirection::Rows)
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
        if (!gridItem->gridAreaContentLogicalWidth())
            gridItem->setGridAreaContentLogicalWidth(0_lu);
        if (!gridItem->gridAreaContentLogicalHeight())
            gridItem->setGridAreaContentLogicalHeight(std::nullopt);

        GridArea area = currentGrid().gridItemArea(*gridItem);
        currentGrid().clampAreaToSubgridIfNeeded(area);
        if (!area.rows.isIndefinite())
            area.rows.translate(currentGrid().explicitGridStart(Style::GridTrackSizingDirection::Rows));
        if (!area.columns.isIndefinite())
            area.columns.translate(currentGrid().explicitGridStart(Style::GridTrackSizingDirection::Columns));

        if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
            currentGrid().setGridItemArea(*gridItem, area);
            bool majorAxisDirectionIsForColumns = autoPlacementMajorAxisDirection() == Style::GridTrackSizingDirection::Columns;
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
        ASSERT(currentGrid().numTracks(Style::GridTrackSizingDirection::Rows) >= Style::GridPositionsResolver::explicitGridCount(*this, Style::GridTrackSizingDirection::Rows));
        ASSERT(currentGrid().numTracks(Style::GridTrackSizingDirection::Columns) >= Style::GridPositionsResolver::explicitGridCount(*this, Style::GridTrackSizingDirection::Columns));
    }
#endif

    auto performAutoPlacement = [&]() {
        placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
        placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
        // Compute collapsible tracks for auto-fit.
        currentGrid().setAutoRepeatEmptyColumns(computeEmptyTracksForAutoRepeat(Style::GridTrackSizingDirection::Columns));
        currentGrid().setAutoRepeatEmptyRows(computeEmptyTracksForAutoRepeat(Style::GridTrackSizingDirection::Rows));

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
            updateGridAreaLogicalSize(*gridItem, algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, Style::GridTrackSizingDirection::Columns), algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, Style::GridTrackSizingDirection::Rows));
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
                updateGridAreaLogicalSize(*gridItem, algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, Style::GridTrackSizingDirection::Columns), algorithm.estimatedGridAreaBreadthForGridItem(*gridItem, Style::GridTrackSizingDirection::Rows));
            gridItem->layoutIfNeeded();
        }
    }
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(currentGrid().orderIterator());
    unsigned explicitRowStart = 0;
    unsigned explicitColumnStart = 0;
    unsigned maximumRowIndex = Style::GridPositionsResolver::explicitGridCount(*this, Style::GridTrackSizingDirection::Rows);
    unsigned maximumColumnIndex = Style::GridPositionsResolver::explicitGridCount(*this, Style::GridTrackSizingDirection::Columns);

    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
        if (!populator.collectChild(gridItem))
            continue;

        auto rowPositions = Style::GridPositionsResolver::resolveGridPositionsFromStyle(*this, gridItem, Style::GridTrackSizingDirection::Rows);
        if (!isSubgridRows()) {
            if (!rowPositions.isIndefinite()) {
                explicitRowStart = std::max<int>(explicitRowStart, -rowPositions.untranslatedStartLine());
                maximumRowIndex = std::max<int>(maximumRowIndex, rowPositions.untranslatedEndLine());
            } else {
                // Grow the grid for items with a definite row span, getting the largest such span.
                unsigned spanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, Style::GridTrackSizingDirection::Rows);
                maximumRowIndex = std::max(maximumRowIndex, spanSize);
            }
        }

        auto columnPositions = Style::GridPositionsResolver::resolveGridPositionsFromStyle(*this, gridItem, Style::GridTrackSizingDirection::Columns);
        if (!isSubgridColumns()) {
            if (!columnPositions.isIndefinite()) {
                explicitColumnStart = std::max<int>(explicitColumnStart, -columnPositions.untranslatedStartLine());
                maximumColumnIndex = std::max<int>(maximumColumnIndex, columnPositions.untranslatedEndLine());
            } else {
                // Grow the grid for items with a definite column span, getting the largest such span.
                unsigned spanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, Style::GridTrackSizingDirection::Columns);
                maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
            }
        }

        currentGrid().setGridItemArea(gridItem, { rowPositions, columnPositions });
    }

    currentGrid().setExplicitGridStart(explicitRowStart, explicitColumnStart);
    currentGrid().ensureGridSize(maximumRowIndex + explicitRowStart, maximumColumnIndex + explicitColumnStart);
    currentGrid().setClampingForSubgrid(isSubgridRows() ? maximumRowIndex : 0, isSubgridColumns() ? maximumColumnIndex : 0);
}

GridArea RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox& gridItem, Style::GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    auto crossDirection = orthogonalDirection(specifiedDirection);
    auto endOfCrossDirection = currentGrid().numTracks(crossDirection);
    auto crossDirectionSpanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, crossDirection);
    auto crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);

    return specifiedDirection == Style::GridTrackSizingDirection::Columns
        ? GridArea { crossDirectionPositions, specifiedPositions }
        : GridArea { specifiedPositions, crossDirectionPositions };
}

bool RenderGrid::isPlacedWithinExtrinsicallySizedExplicitTracks(const RenderBox& gridItem) const
{
    auto& currentGrid = this->currentGrid();
    if (currentGrid.needsItemsPlacement())
        return false;

    auto& gridStyle = style();
    auto gridItemArea = currentGrid.gridItemArea(gridItem);
    auto& gridColumnSizes = gridStyle.gridTemplateColumns().sizes;
    if (gridItemArea.columns.endLine() > gridColumnSizes.size())
        return false;

    for (auto columnIndex : gridItemArea.columns) {
        if (gridColumnSizes[columnIndex].isContentSized())
            return false;
    }

    auto& gridRowSizes = gridStyle.gridTemplateRows().sizes;
    if (gridItemArea.rows.endLine() > gridRowSizes.size())
        return false;

    for (auto rowIndex : gridItemArea.rows) {
        if (gridRowSizes[rowIndex].isContentSized())
            return false;
    }
    return true;
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    bool isForColumns = autoPlacementMajorAxisDirection() == Style::GridTrackSizingDirection::Columns;
    bool isGridAutoFlowDense = style().gridAutoFlow().isDense();

    // Mapping between the major axis tracks (rows or columns) and the last auto-placed item's position inserted on
    // that track. This is needed to implement "sparse" packing for items locked to a given track.
    // See https://drafts.csswg.org/css-grid-2/#auto-placement-algo
    HashMap<unsigned, unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> minorAxisCursors;

    for (auto& autoGridItem : autoGridItems) {
        GridSpan majorAxisPositions = currentGrid().gridItemSpan(*autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(majorAxisPositions.isTranslatedDefinite());
        ASSERT(currentGrid().gridItemSpan(*autoGridItem, autoPlacementMinorAxisDirection()).isIndefinite());
        unsigned minorAxisSpanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(*autoGridItem, autoPlacementMinorAxisDirection());
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
    bool isGridAutoFlowDense = style().gridAutoFlow().isDense();

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
    unsigned majorAxisSpanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, autoPlacementMajorAxisDirection());

    const unsigned endOfMajorAxis = currentGrid().numTracks(autoPlacementMajorAxisDirection());
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == Style::GridTrackSizingDirection::Columns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == Style::GridTrackSizingDirection::Columns ? autoPlacementCursor.first : autoPlacementCursor.second;

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
        unsigned minorAxisSpanSize = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(gridItem, autoPlacementMinorAxisDirection());

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(currentGrid(), autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                unsigned minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == Style::GridTrackSizingDirection::Columns ? emptyGridArea->columns.endLine() : emptyGridArea->rows.endLine();
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

Style::GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    return style().gridAutoFlow().isColumn() ? Style::GridTrackSizingDirection::Columns : Style::GridTrackSizingDirection::Rows;
}

Style::GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    return orthogonalDirection(autoPlacementMajorAxisDirection());
}

void RenderGrid::setNeedsItemPlacement(SubgridDidChange subgridDidChange)
{
    if (currentGrid().needsItemsPlacement())
        return;

    currentGrid().setNeedsItemsPlacement(true);

    auto currentChild = this;
    while (currentChild && (subgridDidChange == SubgridDidChange::Yes || currentChild->isSubgridRows() || currentChild->isSubgridColumns())) {
        currentChild = dynamicDowncast<RenderGrid>(currentChild->parent());
        if (currentChild)
            currentChild->currentGrid().setNeedsItemsPlacement(true);
        subgridDidChange = SubgridDidChange::No;
    }
}

Vector<LayoutUnit> RenderGrid::trackSizesForComputedStyle(Style::GridTrackSizingDirection direction) const
{
    const auto& positions = this->positions(direction);
    auto numPositions = positions.size();

    Vector<LayoutUnit> tracks;
    if (numPositions < 2)
        return tracks;

    ASSERT(!currentGrid().needsItemsPlacement());
    auto offsetBetweenTracks = this->offsetBetweenTracks(direction).distributionOffset;
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    auto gap = !hasCollapsedTracks ? gridGap(direction) : 0_lu;
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

static bool overrideSizeChanged(const RenderBox& gridItem, Style::GridTrackSizingDirection direction, std::optional<LayoutUnit> width, std::optional<LayoutUnit> height)
{
    if (direction == Style::GridTrackSizingDirection::Columns) {
        if (auto gridAreaContentLogicalWidth = gridItem.gridAreaContentLogicalWidth())
            return *gridAreaContentLogicalWidth != width;
        return true;
    }
    if (auto gridAreaContentLogicalHeight = gridItem.gridAreaContentLogicalHeight())
        return *gridAreaContentLogicalHeight != height;
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
    bool gridAreaWidthChanged = overrideSizeChanged(gridItem, Style::GridTrackSizingDirection::Columns, width, height);
    bool gridAreaHeightChanged = overrideSizeChanged(gridItem, Style::GridTrackSizingDirection::Rows, width, height);
    if (gridAreaWidthChanged || (gridAreaHeightChanged && hasRelativeBlockAxisSize(*this, gridItem)))
        gridItem.setNeedsLayout(MarkOnlyThis);

    gridItem.setGridAreaContentLogicalWidth(width);
    gridItem.setGridAreaContentLogicalHeight(height);
}

void RenderGrid::updateGridAreaForAspectRatioItems(const Vector<RenderBox*>& autoGridItems, GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Columns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows);

    for (auto& autoGridItem : autoGridItems) {
        updateGridAreaLogicalSize(*autoGridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(*autoGridItem, Style::GridTrackSizingDirection::Columns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(*autoGridItem, Style::GridTrackSizingDirection::Rows));
        // For an item with aspect-ratio, if it has stretch alignment that stretches to the definite row, we also need to transfer the size before laying out the grid item.
        if (autoGridItem->hasStretchedLogicalHeight())
            applyStretchAlignmentToGridItemIfNeeded(*autoGridItem, gridLayoutState);
    }
}

void RenderGrid::layoutGridItems(GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Columns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows);

    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
        if (currentGrid().orderIterator().shouldSkipChild(gridItem)) {
            if (gridItem.isOutOfFlowPositioned())
                prepareGridItemForPositionedLayout(gridItem);
            continue;
        }

        auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem);
        if (renderGrid && (renderGrid->isSubgridColumns() || renderGrid->isSubgridRows()))
            gridItem.setNeedsLayout(MarkOnlyThis);

        // Setting the definite grid area's sizes. It may imply that the
        // item must perform a layout if its area differs from the one
        // used during the track sizing algorithm.
        updateGridAreaLogicalSize(gridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(gridItem, Style::GridTrackSizingDirection::Columns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(gridItem, Style::GridTrackSizingDirection::Rows));

        LayoutRect oldGridItemRect = gridItem.frameRect();

        // Stretching logic might force a grid item layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that grid item margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToGridItemIfNeeded(gridItem, gridLayoutState);
        applySubgridStretchAlignmentToGridItemIfNeeded(gridItem);

        gridItem.layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        GridLayoutFunctions::updateAutoMarginsIfNeeded(gridItem, writingMode());

        setLogicalPositionForGridItem(gridItem);

        // If the grid item moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the grid item) anyway.
        if (!selfNeedsLayout() && gridItem.checkForRepaintDuringLayout())
            gridItem.repaintDuringLayoutIfMoved(oldGridItemRect);
    }
}

void RenderGrid::layoutMasonryItems(GridLayoutState& gridLayoutState)
{
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Columns);
    populateGridPositionsForDirection(m_trackSizingAlgorithm, Style::GridTrackSizingDirection::Rows);

    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
        if (currentGrid().orderIterator().shouldSkipChild(gridItem)) {
            if (gridItem.isOutOfFlowPositioned())
                prepareGridItemForPositionedLayout(gridItem);
            continue;
        }

        auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem);
        if (renderGrid && (renderGrid->isSubgridColumns() || renderGrid->isSubgridRows()))
            gridItem.setNeedsLayout(MarkOnlyThis);

        // Setting the definite grid area's sizes. It may imply that the
        // item must perform a layout if its area differs from the one
        // used during the track sizing algorithm.
        updateGridAreaLogicalSize(gridItem, gridAreaBreadthForGridItemIncludingAlignmentOffsets(gridItem, Style::GridTrackSizingDirection::Columns), gridAreaBreadthForGridItemIncludingAlignmentOffsets(gridItem, Style::GridTrackSizingDirection::Rows));

        LayoutRect oldGridItemRect = gridItem.frameRect();

        // Stretching logic might force a grid item layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that grid item margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToGridItemIfNeeded(gridItem, gridLayoutState);
        applySubgridStretchAlignmentToGridItemIfNeeded(gridItem);

        gridItem.layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        GridLayoutFunctions::updateAutoMarginsIfNeeded(gridItem, writingMode());

        setLogicalPositionForGridItem(gridItem);

        // If the grid item moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the grid item) anyway.
        if (!selfNeedsLayout() && gridItem.checkForRepaintDuringLayout())
            gridItem.repaintDuringLayoutIfMoved(oldGridItemRect);
    }
}

void RenderGrid::prepareGridItemForPositionedLayout(RenderBox& gridItem)
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    gridItem.containingBlock()->addOutOfFlowBox(gridItem);

    RenderLayer* gridItemLayer = gridItem.layer();
    // Static position of a positioned grid item should use the content-box (https://drafts.csswg.org/css-grid/#static-position).
    gridItemLayer->setStaticInlinePosition(borderAndPaddingStart());
    gridItemLayer->setStaticBlockPosition(borderAndPaddingBefore());
}

bool RenderGrid::hasStaticPositionForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItem.style().hasStaticInlinePosition(isHorizontalWritingMode()) : gridItem.style().hasStaticBlockPosition(isHorizontalWritingMode());
}

void RenderGrid::layoutOutOfFlowBox(RenderBox& gridItem, RelayoutChildren relayoutChildren, bool fixedPositionObjectsOnly)
{
    if (layoutContext().isSkippedContentRootForLayout(*this)) {
        gridItem.clearNeedsLayoutForSkippedContent();
        return;
    }

    LayoutRange columnBreadth = gridAreaRangeForOutOfFlow(gridItem, Style::GridTrackSizingDirection::Columns);
    LayoutRange rowBreadth = gridAreaRangeForOutOfFlow(gridItem, Style::GridTrackSizingDirection::Rows);

    gridItem.setGridAreaContentLogicalWidth(columnBreadth.size());
    gridItem.setGridAreaContentLogicalHeight(rowBreadth.size());

    // Mark for layout as we're resetting the position before and we relay in generic layout logic
    // for positioned items in order to get the offsets properly resolved.
    gridItem.setChildNeedsLayout(MarkOnlyThis);

    RenderBlock::layoutOutOfFlowBox(gridItem, relayoutChildren, fixedPositionObjectsOnly);
}

LayoutUnit RenderGrid::gridAreaBreadthForGridItemIncludingAlignmentOffsets(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    if (direction == Style::GridTrackSizingDirection::Rows) {
        if (areMasonryRows())
            return isHorizontalWritingMode() ? gridItem.height() + gridItem.verticalMarginExtent() : gridItem.width() + gridItem.horizontalMarginExtent();
    } else if (areMasonryColumns())
        return isHorizontalWritingMode() ? gridItem.width() + gridItem.horizontalMarginExtent() : gridItem.height() + gridItem.verticalMarginExtent();

    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    const auto& span = currentGrid().gridItemSpan(gridItem, direction);
    const auto& positions = this->positions(direction);

    auto initialTrackPosition = positions[span.startLine()];
    auto finalTrackPosition = positions[span.endLine() - 1];

    // Track Positions vector stores the 'start' grid line of each track, so we have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.endLine() - 1].baseSize();
}

void RenderGrid::populateGridPositionsForDirection(const GridTrackSizingAlgorithm& algorithm, Style::GridTrackSizingDirection direction)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence, we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.

    // The grid container's frame elements (border, padding and <content-position> offset) are sensible to the
    // inline-axis flow direction. However, column lines positions are 'direction' unaware. This simplification
    // allows us to use the same indexes to identify the columns independently on the inline-axis direction.
    bool isRowAxis = direction == Style::GridTrackSizingDirection::Columns;
    auto& tracks = algorithm.tracks(direction);
    unsigned numberOfTracks = tracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? currentGrid().autoRepeatEmptyTracks(direction)->size() : 0;
    const auto& offsetBetweenTracks = this->offsetBetweenTracks(direction);
    auto& positions = this->positions(direction);
    positions.resize(numberOfLines);

    auto borderAndPadding = isRowAxis ? borderAndPaddingStart() : borderAndPaddingBefore();

    positions[0] = borderAndPadding + offsetBetweenTracks.positionOffset;
    if (numberOfLines > 1) {
        // If we have collapsed tracks we just ignore gaps here and add them later as we might not
        // compute the gap between two consecutive tracks without examining the surrounding ones.
        LayoutUnit gap = !hasCollapsedTracks ? gridGap(direction) : 0_lu;
        unsigned nextToLastLine = numberOfLines - 2;

        for (unsigned i = 0; i < nextToLastLine; ++i)
            positions[i + 1] = positions[i] + offsetBetweenTracks.distributionOffset + tracks[i].unclampedBaseSize() + gap;
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
                    offsetAccumulator += offsetBetweenTracks.distributionOffset;
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

StyleSelfAlignmentData RenderGrid::alignSelfForGridItem(const RenderBox& gridItem, StretchingMode stretchingMode, const RenderStyle* gridStyle) const
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (renderGrid && renderGrid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Rows))
        return { ItemPosition::Stretch, OverflowAlignment::Default };

    auto normalBehavior = stretchingMode == StretchingMode::Any ? selfAlignmentNormalBehavior(&gridItem) : ItemPosition::Normal;

    if (!gridItem.style().alignSelf().isAuto())
        return gridItem.style().alignSelf().resolve(normalBehavior);

    if (!gridStyle)
        gridStyle = &style();
    return gridStyle->alignItems().resolve(normalBehavior);
}

StyleSelfAlignmentData RenderGrid::justifySelfForGridItem(const RenderBox& gridItem, StretchingMode stretchingMode, const RenderStyle* gridStyle) const
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (renderGrid && renderGrid->isSubgridInParentDirection(Style::GridTrackSizingDirection::Columns))
        return { ItemPosition::Stretch, OverflowAlignment::Default };

    auto normalBehavior = stretchingMode == StretchingMode::Any ? selfAlignmentNormalBehavior(&gridItem) : ItemPosition::Normal;

    if (!gridItem.style().justifySelf().isAuto())
        return gridItem.style().justifySelf().resolve(normalBehavior);

    if (!gridStyle)
        gridStyle = &style();
    return gridStyle->justifyItems().resolve(normalBehavior);
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

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::applyStretchAlignmentToGridItemIfNeeded(RenderBox& gridItem, GridLayoutState& gridLayoutState)
{
    ASSERT(gridItem.gridAreaContentLogicalHeight());
    ASSERT(gridItem.gridAreaContentLogicalWidth());

    // We clear height and width override values because we will decide now whether it's allowed or
    // not, evaluating the conditions which might have changed since the old values were set.
    gridItem.clearOverridingSize();

    auto gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, Style::GridTrackSizingDirection::Rows);
    auto gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, Style::GridTrackSizingDirection::Columns);
    bool blockFlowIsColumnAxis = gridItemBlockDirection == Style::GridTrackSizingDirection::Rows;
    bool allowedToStretchgridItemBlockSize = blockFlowIsColumnAxis ? GridLayoutFunctions::allowedToStretchGridItemAlongColumnAxis(gridItem, alignSelfForGridItem(gridItem).position(), writingMode()) : GridLayoutFunctions::allowedToStretchGridItemAlongRowAxis(gridItem, justifySelfForGridItem(gridItem).position(), writingMode());
    if (allowedToStretchgridItemBlockSize && !aspectRatioPrefersInline(gridItem, blockFlowIsColumnAxis)) {
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemBlockDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        LayoutUnit stretchedLogicalHeight = GridLayoutFunctions::availableAlignmentSpaceForGridItemBeforeStretching(*this, overridingContainingBlockContentSizeForGridItem->value(), gridItem, Style::GridTrackSizingDirection::Rows);
        LayoutUnit desiredLogicalHeight = gridItem.constrainLogicalHeightByMinMax(stretchedLogicalHeight, std::nullopt);
        gridItem.setOverridingBorderBoxLogicalHeight(desiredLogicalHeight);

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
    } else if (!allowedToStretchgridItemBlockSize && GridLayoutFunctions::allowedToStretchGridItemAlongRowAxis(gridItem, justifySelfForGridItem(gridItem).position(), writingMode())) {
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        LayoutUnit stretchedLogicalWidth = GridLayoutFunctions::availableAlignmentSpaceForGridItemBeforeStretching(*this, overridingContainingBlockContentSizeForGridItem->value(), gridItem, Style::GridTrackSizingDirection::Columns);
        LayoutUnit desiredLogicalWidth = gridItem.constrainLogicalWidthByMinMax(stretchedLogicalWidth, contentBoxWidth(), *this);
        gridItem.setOverridingBorderBoxLogicalWidth(desiredLogicalWidth);
        if (desiredLogicalWidth != gridItem.logicalWidth())
            gridItem.setNeedsLayout(MarkOnlyThis);
    }
}

void RenderGrid::applySubgridStretchAlignmentToGridItemIfNeeded(RenderBox& gridItem)
{
    CheckedPtr renderGrid = dynamicDowncast<RenderGrid>(gridItem);
    if (!renderGrid)
        return;

    if (renderGrid->isSubgrid(Style::GridTrackSizingDirection::Rows)) {
        auto gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, Style::GridTrackSizingDirection::Rows);
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemBlockDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        auto stretchedLogicalHeight = GridLayoutFunctions::availableAlignmentSpaceForGridItemBeforeStretching(*this, overridingContainingBlockContentSizeForGridItem->value(), gridItem, Style::GridTrackSizingDirection::Rows);
        gridItem.setOverridingBorderBoxLogicalHeight(stretchedLogicalHeight);
    }

    if (renderGrid->isSubgrid(Style::GridTrackSizingDirection::Columns)) {
        auto gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*this, gridItem, Style::GridTrackSizingDirection::Columns);
        auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection);
        ASSERT(overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem);
        auto stretchedLogicalWidth = GridLayoutFunctions::availableAlignmentSpaceForGridItemBeforeStretching(*this, overridingContainingBlockContentSizeForGridItem->value(), gridItem, Style::GridTrackSizingDirection::Columns);
        gridItem.setOverridingBorderBoxLogicalWidth(stretchedLogicalWidth);
    }
}

bool RenderGrid::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& gridItem) const
{
    ASSERT(style().marginTrim().contains(marginTrimSide));

    auto isTrimmingBlockDirection = marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::BlockEnd;
    auto itemGridSpan = isTrimmingBlockDirection ? currentGrid().gridItemSpanIgnoringCollapsedTracks(gridItem, Style::GridTrackSizingDirection::Rows) : currentGrid().gridItemSpanIgnoringCollapsedTracks(gridItem, Style::GridTrackSizingDirection::Columns);
    switch (marginTrimSide) {
    case Style::MarginTrimSide::BlockStart:
    case Style::MarginTrimSide::InlineStart:
        return !itemGridSpan.startLine();
    case Style::MarginTrimSide::BlockEnd:
        return itemGridSpan.endLine() == currentGrid().numTracks(Style::GridTrackSizingDirection::Rows);
    case Style::MarginTrimSide::InlineEnd:
        return itemGridSpan.endLine() == currentGrid().numTracks(Style::GridTrackSizingDirection::Columns);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool RenderGrid::isBaselineAlignmentForGridItem(const RenderBox& gridItem) const
{
    return isBaselineAlignmentForGridItem(gridItem, Style::GridTrackSizingDirection::Columns) || isBaselineAlignmentForGridItem(gridItem, Style::GridTrackSizingDirection::Rows);
}

bool RenderGrid::isBaselineAlignmentForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    if (gridItem.isOutOfFlowPositioned())
        return false;
    auto align = selfAlignmentForGridItem(alignmentContextType, gridItem).position();
    bool hasAutoMargins = alignmentContextType == Style::GridTrackSizingDirection::Rows ? GridLayoutFunctions::hasAutoMarginsInColumnAxis(gridItem, writingMode()) : GridLayoutFunctions::hasAutoMarginsInRowAxis(gridItem, writingMode());
    return isBaselinePosition(align) && !hasAutoMargins;
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
        auto gridWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignmentState::dominantBaseline(gridWritingMode);
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        return BaselineAlignmentState::synthesizedBaseline(*baselineGridItem, dominantBaseline, gridWritingMode, direction, BaselineSynthesisEdge::BorderBox) + logicalTopForChild(*baselineGridItem);
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
        auto direction = isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto gridWritingMode = style().writingMode();
        auto dominantBaseline = BaselineAlignmentState::dominantBaseline(gridWritingMode);
        return BaselineAlignmentState::synthesizedBaseline(*baselineGridItem, dominantBaseline, gridWritingMode, direction, BaselineSynthesisEdge::BorderBox) + logicalTopForChild(*baselineGridItem);

    }

    return baseline.value() + baselineGridItem->logicalTop().toInt();
}

SingleThreadWeakPtr<RenderBox> RenderGrid::getBaselineGridItem(ItemPosition alignment) const
{
    ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);
    const RenderBox* baselineGridItem = nullptr;
    unsigned numColumns = currentGrid().numTracks(Style::GridTrackSizingDirection::Columns);
    auto rowIndexDeterminingBaseline = alignment == ItemPosition::Baseline ? 0 : currentGrid().numTracks(Style::GridTrackSizingDirection::Rows) - 1;
    for (size_t column = 0; column < numColumns; column++) {
        auto cell = currentGrid().cell(rowIndexDeterminingBaseline, alignment == ItemPosition::Baseline ? column : numColumns - column - 1);

        for (auto& gridItem : cell) {
            ASSERT(gridItem.get());
            // If an item participates in baseline alignment, we select such item.
            if (isBaselineAlignmentForGridItem(*gridItem, Style::GridTrackSizingDirection::Rows)) {
                auto gridItemAlignment = selfAlignmentForGridItem(Style::GridTrackSizingDirection::Rows, *gridItem).position();
                if (rowIndexDeterminingBaseline == GridLayoutFunctions::alignmentContextForBaselineAlignment(gridSpanForGridItem(*gridItem, Style::GridTrackSizingDirection::Rows), gridItemAlignment)) {
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
    return m_trackSizingAlgorithm.baselineOffsetForGridItem(gridItem, Style::GridTrackSizingDirection::Rows);
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
    return m_trackSizingAlgorithm.baselineOffsetForGridItem(gridItem, Style::GridTrackSizingDirection::Columns);
}

GridAxisPosition RenderGrid::columnAxisPositionForGridItem(const RenderBox& gridItem) const
{
    if (gridItem.isOutOfFlowPositioned() && !hasStaticPositionForGridItem(gridItem, Style::GridTrackSizingDirection::Rows))
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
    case ItemPosition::AnchorCenter:
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
    }

    ASSERT_NOT_REACHED();
    return GridAxisPosition::GridAxisStart;
}

GridAxisPosition RenderGrid::rowAxisPositionForGridItem(const RenderBox& gridItem) const
{
    if (gridItem.isOutOfFlowPositioned() && !hasStaticPositionForGridItem(gridItem, Style::GridTrackSizingDirection::Columns))
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
    case ItemPosition::AnchorCenter:
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
        // FIXME: Handle non-inline matching orthogonal grid items properly.
        if (GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) && !WritingMode().isInlineMatchingAny(gridItem.writingMode()))
            return GridAxisPosition::GridAxisStart;
        return hasSameDirection ? GridAxisPosition::GridAxisStart : GridAxisPosition::GridAxisEnd;
    case ItemPosition::LastBaseline:
        // FIXME: Handle non-inline matching orthogonal grid items properly.
        if (GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) && !WritingMode().isInlineMatchingAny(gridItem.writingMode()))
            return GridAxisPosition::GridAxisStart;
        return hasSameDirection ? GridAxisPosition::GridAxisEnd : GridAxisPosition::GridAxisStart;
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisPosition::GridAxisStart;
}

LayoutUnit RenderGrid::columnAxisOffsetForGridItem(const RenderBox& gridItem) const
{
    auto [startOfRow, endOfRow] = gridAreaPositionForInFlowGridItem(gridItem, Style::GridTrackSizingDirection::Rows);
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(gridItem);
    LayoutUnit columnAxisGridItemSize = GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalWidth() + gridItem.marginLogicalWidth() : gridItem.logicalHeight() + gridItem.marginLogicalHeight();
    LayoutUnit masonryOffset = areMasonryRows() ? m_masonryLayout.offsetForGridItem(gridItem) : 0_lu;
    auto overflow = alignSelfForGridItem(gridItem).overflow();
    LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfRow - startOfRow, columnAxisGridItemSize);
    if (GridLayoutFunctions::hasAutoMarginsInColumnAxis(gridItem, writingMode()))
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
    auto [startOfColumn, endOfColumn] = gridAreaPositionForInFlowGridItem(gridItem, Style::GridTrackSizingDirection::Columns);
    LayoutUnit startPosition = startOfColumn + marginStartForChild(gridItem);
    LayoutUnit masonryOffset = areMasonryColumns() ? m_masonryLayout.offsetForGridItem(gridItem) : 0_lu;
    if (GridLayoutFunctions::hasAutoMarginsInRowAxis(gridItem, writingMode()))
        return startPosition;
    LayoutUnit rowAxisGridItemSize = GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalHeight() + gridItem.marginLogicalHeight() : gridItem.logicalWidth() + gridItem.marginLogicalWidth();
    auto overflow = justifySelfForGridItem(gridItem).overflow();
    auto rowAxisBaselineOffset = rowAxisBaselineOffsetForGridItem(gridItem);
    LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfColumn - startOfColumn, rowAxisGridItemSize);
    GridAxisPosition axisPosition = rowAxisPositionForGridItem(gridItem);
    switch (axisPosition) {
    case GridAxisPosition::GridAxisStart:
        return startPosition + rowAxisBaselineOffset + masonryOffset;
    case GridAxisPosition::GridAxisEnd:
        return startPosition + offsetFromStartPosition - rowAxisBaselineOffset;
    case GridAxisPosition::GridAxisCenter:
        return startPosition + offsetFromStartPosition / 2;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool RenderGrid::isSubgrid() const
{
    return isSubgrid(Style::GridTrackSizingDirection::Rows) || isSubgrid(Style::GridTrackSizingDirection::Columns);
}

bool RenderGrid::isSubgrid(Style::GridTrackSizingDirection direction) const
{
    // If the grid container is forced to establish an independent formatting
    // context (like contain layout, or position:absolute), then the used value
    // of grid-template-rows/columns is 'none' and the container is not a subgrid.
    // https://drafts.csswg.org/css-grid-2/#subgrid-listing
    if (establishesIndependentFormattingContextIgnoringDisplayType(style()))
        return false;
    if (!style().gridTemplateList(direction).subgrid)
        return false;
    auto* renderGrid = dynamicDowncast<RenderGrid>(parent());
    if (!renderGrid)
        return false;
    return !renderGrid->isMasonry(direction);
}

bool RenderGrid::isSubgridInParentDirection(Style::GridTrackSizingDirection parentDirection) const
{
    auto* renderGrid = dynamicDowncast<RenderGrid>(parent());
    if (!renderGrid)
        return false;
    auto direction = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid, *this, parentDirection);
    return isSubgrid(direction);
}

bool RenderGrid::isSubgridOf(Style::GridTrackSizingDirection direction, const RenderGrid& ancestor) const
{
    if (!isSubgrid(direction))
        return false;
    if (parent() == &ancestor)
        return true;

    auto& parentGrid = *downcast<RenderGrid>(parent());
    auto parentDirection = GridLayoutFunctions::flowAwareDirectionForParent(parentGrid, *this, direction);
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

LayoutRange RenderGrid::gridAreaRangeForOutOfFlow(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    bool isRowAxis = direction == Style::GridTrackSizingDirection::Columns;
    LayoutUnit borderEdge = isRowAxis ? borderStart() : borderBefore();
    if (currentGrid().needsItemsPlacement()) {
        // Haven't completed in-flow placement and grid sizing yet.
        // Return something basic that doesn't access unbuilt data structures.
        return LayoutRange(borderEdge, isRowAxis ? clientLogicalWidth() : clientLogicalHeight());
    }

    int startLine, endLine;
    bool startIsAuto, endIsAuto;
    if (!computeGridPositionsForOutOfFlowGridItem(gridItem, direction, startLine, startIsAuto, endLine, endIsAuto) || (startIsAuto && endIsAuto))
        return LayoutRange(borderEdge, isRowAxis ? clientLogicalWidth() : clientLogicalHeight());

    LayoutUnit start;
    LayoutUnit end;

    auto& positions = this->positions(direction);
    if (positions.isEmpty()) {
        ASSERT_WITH_SECURITY_IMPLICATION(!positions.isEmpty());
        return LayoutRange(borderEdge, isRowAxis ? clientLogicalWidth() : clientLogicalHeight());
    }

    if (startIsAuto)
        start = borderEdge;
    else {
        start = positions[startLine];
    }
    if (endIsAuto)
        end = (isRowAxis ? clientLogicalWidth() : clientLogicalHeight()) + borderEdge;
    else {
        end = positions[endLine];
        // These vectors store line positions including gaps, but we shouldn't consider them for the edges of the grid.
        std::optional<LayoutUnit> availableSizeForGutters = availableSpaceForGutters(direction);
        int lastLine = numTracks(direction);
        if (endLine > 0 && endLine < lastLine) {
            end -= guttersSize(direction, endLine - 1, 2, availableSizeForGutters);
            end -= isRowAxis ? m_offsetBetweenColumns.distributionOffset : m_offsetBetweenRows.distributionOffset;
        }
    }
    return LayoutRange(start, std::max(end - start, 0_lu));
}

std::pair<LayoutUnit, LayoutUnit> RenderGrid::gridAreaPositionForInFlowGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    ASSERT(!gridItem.isOutOfFlowPositioned());
    const auto& span = currentGrid().gridItemSpan(gridItem, direction);
    const auto& positions = this->positions(direction);
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

StyleContentAlignmentData RenderGrid::contentAlignment(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns
        ? style().justifyContent().resolve(contentAlignmentNormalBehaviorGrid())
        : style().alignContent().resolve(contentAlignmentNormalBehaviorGrid());
}

ContentAlignmentData RenderGrid::computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    if (isSubgrid(direction))
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
        ASSERT(direction == Style::GridTrackSizingDirection::Columns);
        if (!writingMode().isBidiLTR())
            return { availableFreeSpace, 0_lu };
        return { };
    case ContentPosition::Right:
        ASSERT(direction == Style::GridTrackSizingDirection::Columns);
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
    LayoutPoint gridItemLocation(logicalOffsetForGridItem(gridItem, Style::GridTrackSizingDirection::Columns), logicalOffsetForGridItem(gridItem, Style::GridTrackSizingDirection::Rows));
    gridItem.setLogicalLocation(GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItemLocation.transposedPoint() : gridItemLocation);
}

LayoutUnit RenderGrid::logicalOffsetForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    if (direction == Style::GridTrackSizingDirection::Rows)
        return columnAxisOffsetForGridItem(gridItem);
    LayoutUnit rowAxisOffset = rowAxisOffsetForGridItem(gridItem);
    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (writingMode().isInlineFlipped())
        rowAxisOffset = translateRTLCoordinate(rowAxisOffset) - (GridLayoutFunctions::isOrthogonalGridItem(*this, gridItem) ? gridItem.logicalHeight()  : gridItem.logicalWidth());
    return rowAxisOffset;
}

unsigned RenderGrid::nonCollapsedTracks(Style::GridTrackSizingDirection direction) const
{
    auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    size_t numberOfTracks = tracks.size();
    bool hasCollapsedTracks = currentGrid().hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? currentGrid().autoRepeatEmptyTracks(direction)->size() : 0;
    return numberOfTracks - numberOfCollapsedTracks;
}

unsigned RenderGrid::numTracks(Style::GridTrackSizingDirection direction) const
{
    // Due to limitations in our internal representation, we cannot know the number of columns from
    // currentGrid *if* there is no row (because currentGrid would be empty). That's why in that case we need
    // to get it from the style. Note that we know for sure that there aren't any implicit tracks,
    // because not having rows implies that there are no "normal" grid items (out-of-flow grid items are
    // not stored in currentGrid).
    ASSERT(!currentGrid().needsItemsPlacement());
    if (direction == Style::GridTrackSizingDirection::Rows)
        return currentGrid().numTracks(Style::GridTrackSizingDirection::Rows);

    // FIXME: This still requires knowledge about currentGrid internals.
    return currentGrid().numTracks(Style::GridTrackSizingDirection::Rows)
        ? currentGrid().numTracks(Style::GridTrackSizingDirection::Columns)
        : Style::GridPositionsResolver::explicitGridCount(*this, Style::GridTrackSizingDirection::Columns);
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    ASSERT(!currentGrid().needsItemsPlacement());
    for (RenderBox* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next())
        paintChild(*gridItem, paintInfo, paintOffset, forChild, usePrintRect, PaintAsInlineBlock);
}

bool RenderGrid::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    LayoutPoint scrolledOffset = hasNonVisibleOverflow() ? adjustedLocation - toLayoutSize(scrollPosition()) : adjustedLocation;

    Vector<RenderBox*> reversedOrderIteratorForHitTesting;
    for (auto* gridItem = currentGrid().orderIterator().first(); gridItem; gridItem = currentGrid().orderIterator().next()) {
        if (gridItem->isOutOfFlowPositioned())
            continue;
        reversedOrderIteratorForHitTesting.append(gridItem);
    }
    reversedOrderIteratorForHitTesting.reverse();

    for (auto* gridItem : reversedOrderIteratorForHitTesting) {
        if (gridItem->hasSelfPaintingLayer())
            continue;
        auto location = flipForWritingModeForChild(*gridItem, scrolledOffset);
        if (gridItem->hitTest(request, result, locationInContainer, location)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - adjustedLocation)));
            return true;
        }
    }

    return false;
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

bool RenderGrid::computeGridPositionsForOutOfFlowGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction, int& startLine, bool& startIsAuto, int& endLine, bool& endIsAuto) const
{
    ASSERT(gridItem.isOutOfFlowPositioned());
    int lastLine = numTracks(direction);
    auto span = Style::GridPositionsResolver::resolveGridPositionsFromStyle(*this, gridItem, direction);
    if (span.isIndefinite())
        return false;

    unsigned explicitStart = currentGrid().explicitGridStart(direction);
    startLine = span.untranslatedStartLine() + explicitStart;
    endLine = span.untranslatedEndLine() + explicitStart;
    startIsAuto = gridItem.style().gridItemStart(direction).isAuto() || startLine < 0 || startLine > lastLine;
    endIsAuto = gridItem.style().gridItemEnd(direction).isAuto() || endLine < 0 || endLine > lastLine;
    return true;
}

GridSpan RenderGrid::gridSpanForOutOfFlowGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    int lastLine = numTracks(direction);
    int startLine, endLine;
    bool startIsAuto, endIsAuto;
    if (!computeGridPositionsForOutOfFlowGridItem(gridItem, direction, startLine, startIsAuto, endLine, endIsAuto))
        return GridSpan::translatedDefiniteGridSpan(0, lastLine);
    return GridSpan::translatedDefiniteGridSpan(startIsAuto ? 0 : startLine, endIsAuto ? lastLine : endLine);
}

GridSpan RenderGrid::gridSpanForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
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

RenderGrid::GridWrapper::GridWrapper(RenderGrid& renderGrid)
    : m_layoutGrid(renderGrid)
{ }

void RenderGrid::GridWrapper::resetCurrentGrid() const
{
    m_currentGrid = std::ref(const_cast<Grid&>(m_layoutGrid));
}

void RenderGrid::computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> options)
{
    RenderBlock::computeOverflow(oldClientAfterEdge, options);

    if (!hasPotentiallyScrollableOverflow() || isMasonry() || isSubgridRows() || isSubgridColumns())
        return;

    // FIXME: We should handle RTL and other writing modes also.
    if (writingMode().isBidiLTR() && isHorizontalWritingMode()) {
        auto gridAreaSize = LayoutSize { m_columnPositions.last(), m_rowPositions.last() };
        gridAreaSize += { paddingEnd(), paddingAfter() };
        addLayoutOverflow({ { }, gridAreaSize });
    }
}

void RenderGrid::updateIntrinsicLogicalHeightsForRowSizingFirstPassCacheAvailability()
{
    auto canCreateIntrinsicLogicalHeightsCacheForRowSizingFirstPass = this->canCreateIntrinsicLogicalHeightsForRowSizingFirstPassCache();

    if (canCreateIntrinsicLogicalHeightsCacheForRowSizingFirstPass && m_intrinsicLogicalHeightsForRowSizingFirstPass) {
        for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
            if (gridItem.needsLayout())
                m_intrinsicLogicalHeightsForRowSizingFirstPass->invalidateSizeForItem(gridItem);
        }
    } else if (canCreateIntrinsicLogicalHeightsCacheForRowSizingFirstPass)
        m_intrinsicLogicalHeightsForRowSizingFirstPass.emplace();
    else
        m_intrinsicLogicalHeightsForRowSizingFirstPass.reset();
}

std::optional<GridItemSizeCache>& RenderGrid::intrinsicLogicalHeightsForRowSizingFirstPass() const
{
    ASSERT_IMPLIES(m_intrinsicLogicalHeightsForRowSizingFirstPass, canCreateIntrinsicLogicalHeightsForRowSizingFirstPassCache());
    return m_intrinsicLogicalHeightsForRowSizingFirstPass;
}

bool RenderGrid::canCreateIntrinsicLogicalHeightsForRowSizingFirstPassCache() const
{
    if (isMasonry())
        return false;

    if (isSubgridRows())
        return false;

    if (enclosingFragmentedFlow())
        return false;

    for (auto& gridItem : childrenOfType<RenderBox>(*this)) {
        if (auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem)) {
            if (renderGrid->isSubgridRows())
                return false;

            if (renderGrid->isSubgridColumns() && GridLayoutFunctions::isOrthogonalGridItem(*this, *renderGrid))
                return false;
        }

        if (isBaselineAlignmentForGridItem(gridItem))
            return false;
    }
    return true;
}

void GridItemSizeCache::setSizeForGridItem(const RenderBox& gridItem, LayoutUnit size)
{
    m_sizes.set(gridItem, size);
}

std::optional<LayoutUnit> GridItemSizeCache::sizeForItem(const RenderBox& gridItem) const
{
    return m_sizes.get(gridItem);
}

void GridItemSizeCache::invalidateSizeForItem(const RenderBox& gridItem)
{
    m_sizes.remove(gridItem);
}

} // namespace WebCore

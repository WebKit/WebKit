/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "GridPositionsResolver.h"
#include "GridTrackSizingAlgorithm.h"
#include "LayoutRepainter.h"
#include "LayoutState.h"
#include "RenderChildIterator.h"
#include "RenderLayer.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include <cstdlib>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderGrid);

enum TrackSizeRestriction {
    AllowInfinity,
    ForbidInfinity,
};

RenderGrid::RenderGrid(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
    , m_grid(*this)
    , m_trackSizingAlgorithm(this, m_grid)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid() = default;

StyleSelfAlignmentData RenderGrid::selfAlignmentForChild(GridAxis axis, const RenderBox& child, const RenderStyle* gridStyle) const
{
    return axis == GridRowAxis ? justifySelfForChild(child, gridStyle) : alignSelfForChild(child, gridStyle);
}

bool RenderGrid::selfAlignmentChangedToStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& child) const
{
    return selfAlignmentForChild(axis, child, &oldStyle).position() != ItemPosition::Stretch
        && selfAlignmentForChild(axis, child, &newStyle).position() == ItemPosition::Stretch;
}

bool RenderGrid::selfAlignmentChangedFromStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& child) const
{
    return selfAlignmentForChild(axis, child, &oldStyle).position() == ItemPosition::Stretch
        && selfAlignmentForChild(axis, child, &newStyle).position() != ItemPosition::Stretch;
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifference::Layout)
        return;

    const RenderStyle& newStyle = this->style();
    if (oldStyle->resolvedAlignItems(selfAlignmentNormalBehavior(this)).position() == ItemPosition::Stretch) {
        // Style changes on the grid container implying stretching (to-stretch) or
        // shrinking (from-stretch) require the affected items to be laid out again.
        // These logic only applies to 'stretch' since the rest of the alignment
        // values don't change the size of the box.
        // In any case, the items' overrideSize will be cleared and recomputed (if
        // necessary)  as part of the Grid layout logic, triggered by this style
        // change.
        for (auto& child : childrenOfType<RenderBox>(*this)) {
            if (child.isOutOfFlowPositioned())
                continue;
            if (selfAlignmentChangedToStretch(GridRowAxis, *oldStyle, newStyle, child)
                || selfAlignmentChangedFromStretch(GridRowAxis, *oldStyle, newStyle, child)
                || selfAlignmentChangedToStretch(GridColumnAxis, *oldStyle, newStyle, child)
                || selfAlignmentChangedFromStretch(GridColumnAxis, *oldStyle, newStyle, child)) {
                child.setNeedsLayout();
            }
        }
    }

    if (explicitGridDidResize(*oldStyle) || namedGridLinesDefinitionDidChange(*oldStyle) || oldStyle->gridAutoFlow() != style().gridAutoFlow()
        || (style().gridAutoRepeatColumns().size() || style().gridAutoRepeatRows().size()))
        dirtyGrid();
}

bool RenderGrid::explicitGridDidResize(const RenderStyle& oldStyle) const
{
    return oldStyle.gridColumns().size() != style().gridColumns().size()
        || oldStyle.gridRows().size() != style().gridRows().size()
        || oldStyle.namedGridAreaColumnCount() != style().namedGridAreaColumnCount()
        || oldStyle.namedGridAreaRowCount() != style().namedGridAreaRowCount()
        || oldStyle.gridAutoRepeatColumns().size() != style().gridAutoRepeatColumns().size()
        || oldStyle.gridAutoRepeatRows().size() != style().gridAutoRepeatRows().size();
}

bool RenderGrid::namedGridLinesDefinitionDidChange(const RenderStyle& oldStyle) const
{
    return oldStyle.namedGridRowLines() != style().namedGridRowLines()
        || oldStyle.namedGridColumnLines() != style().namedGridColumnLines();
}

// This method optimizes the gutters computation by skiping the available size
// call if gaps are fixed size (it's only needed for percentages).
std::optional<LayoutUnit> RenderGrid::availableSpaceForGutters(GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == ForColumns;
    const GapLength& gapLength = isRowAxis ? style().columnGap() : style().rowGap();
    if (gapLength.isNormal() || !gapLength.length().isPercentOrCalculated())
        return std::nullopt;

    return isRowAxis ? availableLogicalWidth() : contentLogicalHeight();
}

void RenderGrid::computeTrackSizesForDefiniteSize(GridTrackSizingDirection direction, LayoutUnit availableSpace)
{
    LayoutUnit totalGuttersSize = guttersSize(m_grid, direction, 0, m_grid.numTracks(direction), availableSpace);
    LayoutUnit freeSpace = availableSpace - totalGuttersSize;

    m_trackSizingAlgorithm.setup(direction, numTracks(direction, m_grid), TrackSizing, availableSpace, freeSpace);
    m_trackSizingAlgorithm.run();

    ASSERT(m_trackSizingAlgorithm.tracksAreWiderThanMinTrackBreadth());
}

void RenderGrid::repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows)
{
    // In orthogonal flow cases column track's size is determined by using the computed
    // row track's size, which it was estimated during the first cycle of the sizing
    // algorithm. Hence we need to repeat computeUsedBreadthOfGridTracks for both,
    // columns and rows, to determine the final values.
    // TODO (lajava): orthogonal flows is just one of the cases which may require
    // a new cycle of the sizing algorithm; there may be more. In addition, not all the
    // cases with orthogonal flows require this extra cycle; we need a more specific
    // condition to detect whether child's min-content contribution has changed or not.
    if (m_hasAnyOrthogonalItem || m_trackSizingAlgorithm.hasAnyPercentSizedRowsIndefiniteHeight()) {
        computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);
        computeContentPositionAndDistributionOffset(ForColumns, m_trackSizingAlgorithm.freeSpace(ForColumns).value(), nonCollapsedTracks(ForColumns));
        computeTrackSizesForDefiniteSize(ForRows, availableSpaceForRows);
        computeContentPositionAndDistributionOffset(ForRows, m_trackSizingAlgorithm.freeSpace(ForRows).value(), nonCollapsedTracks(ForRows));
    }
}

bool RenderGrid::canPerformSimplifiedLayout() const
{
    // We cannot perform a simplified layout if we need to position the items and we have some
    // positioned items to be laid out.
    if (m_grid.needsItemsPlacement() && posChildNeedsLayout())
        return false;

    return RenderBlock::canPerformSimplifiedLayout();
}

void RenderGrid::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

        preparePaginationBeforeBlockLayout(relayoutChildren);

        LayoutSize previousSize = size();
        // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() but it does not work for positioned stuff.
        // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
        bool hasDefiniteLogicalHeight = hasOverrideContentLogicalHeight() || computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), std::nullopt);

        m_hasAnyOrthogonalItem = false;
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned())
                continue;
            // Grid's layout logic controls the grid item's override height, hence we need to
            // clear any override height set previously, so it doesn't interfere in current layout
            // execution. Grid never uses the override width, that's why we don't need to clear  it.
            child->clearOverrideContentLogicalHeight();

            // We may need to repeat the track sizing in case of any grid item was orthogonal.
            if (GridLayoutFunctions::isOrthogonalChild(*this, *child))
                m_hasAnyOrthogonalItem = true;

            // We keep a cache of items with baseline as alignment values so
            // that we only compute the baseline shims for such items. This
            // cache is needed for performance related reasons due to the
            // cost of evaluating the item's participation in a baseline
            // context during the track sizing algorithm.
            if (isBaselineAlignmentForChild(*child, GridColumnAxis))
                m_trackSizingAlgorithm.cacheBaselineAlignedItem(*child, GridColumnAxis);
            if (isBaselineAlignmentForChild(*child, GridRowAxis))
                m_trackSizingAlgorithm.cacheBaselineAlignedItem(*child, GridRowAxis);
        }
        m_baselineItemsCached = true;
        setLogicalHeight(0);
        updateLogicalWidth();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        LayoutUnit availableSpaceForColumns = availableLogicalWidth();
        placeItemsOnGrid(m_trackSizingAlgorithm, availableSpaceForColumns);

        performGridItemsPreLayout(m_trackSizingAlgorithm);

        // 1- First, the track sizing algorithm is used to resolve the sizes of the
        // grid columns.
        // At this point the logical width is always definite as the above call to
        // updateLogicalWidth() properly resolves intrinsic sizes. We cannot do the
        // same for heights though because many code paths inside
        // updateLogicalHeight() require a previous call to setLogicalHeight() to
        // resolve heights properly (like for positioned items for example).
        computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);

        // 1.5- Compute Content Distribution offsets for column tracks
        computeContentPositionAndDistributionOffset(ForColumns, m_trackSizingAlgorithm.freeSpace(ForColumns).value(), nonCollapsedTracks(ForColumns));

        // 2- Next, the track sizing algorithm resolves the sizes of the grid rows,
        // using the grid column sizes calculated in the previous step.
        if (!hasDefiniteLogicalHeight) {
            m_minContentHeight = LayoutUnit();
            m_maxContentHeight = LayoutUnit();
            computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, ForRows, *m_minContentHeight, *m_maxContentHeight);
            // FIXME: This should be really added to the intrinsic height in RenderBox::computeContentAndScrollbarLogicalHeightUsing().
            // Remove this when that is fixed.
            ASSERT(m_minContentHeight);
            ASSERT(m_maxContentHeight);
            LayoutUnit scrollbarHeight = scrollbarLogicalHeight();
            *m_minContentHeight += scrollbarHeight;
            *m_maxContentHeight += scrollbarHeight;
        } else
            computeTrackSizesForDefiniteSize(ForRows, availableLogicalHeight(ExcludeMarginBorderPadding));
        LayoutUnit trackBasedLogicalHeight = m_trackSizingAlgorithm.computeTrackBasedSize() + borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
        setLogicalHeight(trackBasedLogicalHeight);

        LayoutUnit oldClientAfterEdge = clientLogicalBottom();
        updateLogicalHeight();

        // Once grid's indefinite height is resolved, we can compute the
        // available free space for Content Alignment.
        if (!hasDefiniteLogicalHeight)
            m_trackSizingAlgorithm.setFreeSpace(ForRows, logicalHeight() - trackBasedLogicalHeight);

        // 2.5- Compute Content Distribution offsets for rows tracks
        computeContentPositionAndDistributionOffset(ForRows, m_trackSizingAlgorithm.freeSpace(ForRows).value(), nonCollapsedTracks(ForRows));

        // 3- If the min-content contribution of any grid items have changed based on the row
        // sizes calculated in step 2, steps 1 and 2 are repeated with the new min-content
        // contribution (once only).
        repeatTracksSizingIfNeeded(availableSpaceForColumns, contentLogicalHeight());

        // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
        if (hasLineIfEmpty()) {
            LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
                + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
                + scrollbarLogicalHeight();
            setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
        }

        layoutGridItems();
        m_trackSizingAlgorithm.reset();

        if (size() != previousSize)
            relayoutChildren = true;

        m_outOfFlowItemColumn.clear();
        m_outOfFlowItemRow.clear();

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

        computeOverflow(oldClientAfterEdge);
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
    const GapLength& gapLength = direction == ForColumns? style().columnGap() : style().rowGap();
    if (gapLength.isNormal())
        return LayoutUnit();

    return valueForLength(gapLength.length(), availableSize.value_or(0));
}

LayoutUnit RenderGrid::gridGap(GridTrackSizingDirection direction) const
{
    return gridGap(direction, availableSpaceForGutters(direction));
}

LayoutUnit RenderGrid::gridItemOffset(GridTrackSizingDirection direction) const
{
    return direction == ForRows ? m_offsetBetweenRows.distributionOffset : m_offsetBetweenColumns.distributionOffset;
}

LayoutUnit RenderGrid::guttersSize(const Grid& grid, GridTrackSizingDirection direction, unsigned startLine, unsigned span, std::optional<LayoutUnit> availableSize) const
{
    if (span <= 1)
        return { };

    LayoutUnit gap = gridGap(direction, availableSize);

    // Fast path, no collapsing tracks.
    if (!grid.hasAutoRepeatEmptyTracks(direction))
        return gap * (span - 1);

    // If there are collapsing tracks we need to be sure that gutters are properly collapsed. Apart
    // from that, if we have a collapsed track in the edges of the span we're considering, we need
    // to move forward (or backwards) in order to know whether the collapsed tracks reach the end of
    // the grid (so the gap becomes 0) or there is a non empty track before that.

    LayoutUnit gapAccumulator;
    unsigned endLine = startLine + span;

    for (unsigned line = startLine; line < endLine - 1; ++line) {
        if (!grid.isEmptyAutoRepeatTrack(direction, line))
            gapAccumulator += gap;
    }

    // The above loop adds one extra gap for trailing collapsed tracks.
    if (gapAccumulator && grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        ASSERT(gapAccumulator >= gap);
        gapAccumulator -= gap;
    }

    // If the startLine is the start line of a collapsed track we need to go backwards till we reach
    // a non collapsed track. If we find a non collapsed track we need to add that gap.
    if (startLine && grid.isEmptyAutoRepeatTrack(direction, startLine)) {
        unsigned nonEmptyTracksBeforeStartLine = startLine;
        auto begin = grid.autoRepeatEmptyTracks(direction)->begin();
        for (auto it = begin; *it != startLine; ++it) {
            ASSERT(nonEmptyTracksBeforeStartLine);
            --nonEmptyTracksBeforeStartLine;
        }
        if (nonEmptyTracksBeforeStartLine)
            gapAccumulator += gap;
    }

    // If the endLine is the end line of a collapsed track we need to go forward till we reach a non
    // collapsed track. If we find a non collapsed track we need to add that gap.
    if (grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        unsigned nonEmptyTracksAfterEndLine = grid.numTracks(direction) - endLine;
        auto currentEmptyTrack = grid.autoRepeatEmptyTracks(direction)->find(endLine - 1);
        auto endEmptyTrack = grid.autoRepeatEmptyTracks(direction)->end();
        // HashSet iterators do not implement operator- so we have to manually iterate to know the number of remaining empty tracks.
        for (auto it = ++currentEmptyTrack; it != endEmptyTrack; ++it) {
            ASSERT(nonEmptyTracksAfterEndLine >= 1);
            --nonEmptyTracksAfterEndLine;
        }
        if (nonEmptyTracksAfterEndLine)
            gapAccumulator += gap;
    }

    return gapAccumulator;
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    LayoutUnit childMinWidth;
    LayoutUnit childMaxWidth;
    bool hadExcludedChildren = computePreferredWidthsForExcludedChildren(childMinWidth, childMaxWidth);

    Grid grid(const_cast<RenderGrid&>(*this));
    GridTrackSizingAlgorithm algorithm(this, grid);
    placeItemsOnGrid(algorithm, std::nullopt);

    performGridItemsPreLayout(algorithm);

    if (m_baselineItemsCached)
        algorithm.copyBaselineItemsCache(m_trackSizingAlgorithm, GridRowAxis);
    else {
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned())
                continue;
            if (isBaselineAlignmentForChild(*child, GridRowAxis))
                algorithm.cacheBaselineAlignedItem(*child, GridRowAxis);
        }
    }

    computeTrackSizesForIndefiniteSize(algorithm, ForColumns, minLogicalWidth, maxLogicalWidth);

    if (hadExcludedChildren) {
        minLogicalWidth = std::max(minLogicalWidth, childMinWidth);
        maxLogicalWidth = std::max(maxLogicalWidth, childMaxWidth);
    }

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;
}

void RenderGrid::computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm& algorithm, GridTrackSizingDirection direction, LayoutUnit& minIntrinsicSize, LayoutUnit& maxIntrinsicSize) const
{
    const Grid& grid = algorithm.grid();
    algorithm.setup(direction, numTracks(direction, grid), IntrinsicSizeComputation, std::nullopt, std::nullopt);
    algorithm.run();

    size_t numberOfTracks = algorithm.tracks(direction).size();
    LayoutUnit totalGuttersSize = guttersSize(grid, direction, 0, numberOfTracks, std::nullopt);

    minIntrinsicSize = algorithm.minContentSize() + totalGuttersSize;
    maxIntrinsicSize = algorithm.maxContentSize() + totalGuttersSize;

    ASSERT(algorithm.tracksAreWiderThanMinTrackBreadth());
}

std::optional<LayoutUnit> RenderGrid::computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, std::optional<LayoutUnit> intrinsicLogicalHeight, LayoutUnit borderAndPadding) const
{
    if (!intrinsicLogicalHeight)
        return std::nullopt;

    if (logicalHeightLength.isMinContent())
        return m_minContentHeight;

    if (logicalHeightLength.isMaxContent())
        return m_maxContentHeight;

    if (logicalHeightLength.isFitContent()) {
        LayoutUnit fillAvailableExtent = containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
        return std::min(m_maxContentHeight.value_or(0), std::max(m_minContentHeight.value_or(0), fillAvailableExtent));
    }

    if (logicalHeightLength.isFillAvailable())
        return containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding) - borderAndPadding;
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

unsigned RenderGrid::computeAutoRepeatTracksCount(GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    ASSERT(!availableSize || availableSize.value() != -1);
    bool isRowAxis = direction == ForColumns;
    const auto& autoRepeatTracks = isRowAxis ? style().gridAutoRepeatColumns() : style().gridAutoRepeatRows();
    unsigned autoRepeatTrackListLength = autoRepeatTracks.size();

    if (!autoRepeatTrackListLength)
        return 0;

    if (!isRowAxis && !availableSize) {
        const Length& maxLength = style().logicalMaxHeight();
        if (!maxLength.isUndefined()) {
            availableSize = computeContentLogicalHeight(MaxSize, maxLength, std::nullopt);
            if (availableSize)
                availableSize = constrainContentBoxLogicalHeightByMinMax(availableSize.value(), std::nullopt);
        }
    }

    bool needsToFulfillMinimumSize = false;
    if (!availableSize) {
        const Length& minSize = isRowAxis ? style().logicalMinWidth() : style().logicalMinHeight();
        if (!minSize.isSpecified())
            return autoRepeatTrackListLength;

        LayoutUnit containingBlockAvailableSize = isRowAxis ? containingBlockLogicalWidthForContent() : containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
        availableSize = valueForLength(minSize, containingBlockAvailableSize);
        needsToFulfillMinimumSize = true;
    }

    LayoutUnit autoRepeatTracksSize;
    for (auto& autoTrackSize : autoRepeatTracks) {
        ASSERT(autoTrackSize.minTrackBreadth().isLength());
        ASSERT(!autoTrackSize.minTrackBreadth().isFlex());
        bool hasDefiniteMaxTrackSizingFunction = autoTrackSize.maxTrackBreadth().isLength() && !autoTrackSize.maxTrackBreadth().isContentSized();
        auto trackLength = hasDefiniteMaxTrackSizingFunction ? autoTrackSize.maxTrackBreadth().length() : autoTrackSize.minTrackBreadth().length();
        autoRepeatTracksSize += valueForLength(trackLength, availableSize.value());
    }
    // For the purpose of finding the number of auto-repeated tracks, the UA must floor the track size to a UA-specified
    // value to avoid division by zero. It is suggested that this floor be 1px.
    autoRepeatTracksSize = std::max<LayoutUnit>(LayoutUnit(1), autoRepeatTracksSize);

    // There will be always at least 1 auto-repeat track, so take it already into account when computing the total track size.
    LayoutUnit tracksSize = autoRepeatTracksSize;
    auto& trackSizes = isRowAxis ? style().gridColumns() : style().gridRows();

    for (const auto& track : trackSizes) {
        bool hasDefiniteMaxTrackBreadth = track.maxTrackBreadth().isLength() && !track.maxTrackBreadth().isContentSized();
        ASSERT(hasDefiniteMaxTrackBreadth || (track.minTrackBreadth().isLength() && !track.minTrackBreadth().isContentSized()));
        tracksSize += valueForLength(hasDefiniteMaxTrackBreadth ? track.maxTrackBreadth().length() : track.minTrackBreadth().length(), availableSize.value());
    }

    // Add gutters as if there where only 1 auto repeat track. Gaps between auto repeat tracks will be added later when
    // computing the repetitions.
    LayoutUnit gapSize = gridGap(direction, availableSize);
    tracksSize += gapSize * trackSizes.size();

    LayoutUnit freeSpace = availableSize.value() - tracksSize;
    if (freeSpace <= 0)
        return autoRepeatTrackListLength;

    LayoutUnit autoRepeatSizeWithGap = autoRepeatTracksSize + gapSize;
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


std::unique_ptr<OrderedTrackIndexSet> RenderGrid::computeEmptyTracksForAutoRepeat(Grid& grid, GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == ForColumns;
    if ((isRowAxis && style().gridAutoRepeatColumnsType() != AutoRepeatType::Fit)
        || (!isRowAxis && style().gridAutoRepeatRowsType() != AutoRepeatType::Fit))
        return nullptr;

    std::unique_ptr<OrderedTrackIndexSet> emptyTrackIndexes;
    unsigned insertionPoint = isRowAxis ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned firstAutoRepeatTrack = insertionPoint + std::abs(grid.smallestTrackStart(direction));
    unsigned lastAutoRepeatTrack = firstAutoRepeatTrack + grid.autoRepeatTracks(direction);

    if (!grid.hasGridItems()) {
        emptyTrackIndexes = std::make_unique<OrderedTrackIndexSet>();
        for (unsigned trackIndex = firstAutoRepeatTrack; trackIndex < lastAutoRepeatTrack; ++trackIndex)
            emptyTrackIndexes->add(trackIndex);
    } else {
        for (unsigned trackIndex = firstAutoRepeatTrack; trackIndex < lastAutoRepeatTrack; ++trackIndex) {
            GridIterator iterator(grid, direction, trackIndex);
            if (!iterator.nextGridItem()) {
                if (!emptyTrackIndexes)
                    emptyTrackIndexes = std::make_unique<OrderedTrackIndexSet>();
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

    unsigned insertionPoint = direction == ForColumns ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned maxTracks = static_cast<unsigned>(GridPosition::max());

    if (!insertionPoint)
        return std::min(autoRepeatTracks, maxTracks);

    if (insertionPoint >= maxTracks)
        return 0;

    return std::min(autoRepeatTracks, maxTracks - insertionPoint);
}

// FIXME: We shouldn't have to pass the available logical width as argument. The problem is that
// availableLogicalWidth() does always return a value even if we cannot resolve it like when
// computing the intrinsic size (preferred widths). That's why we pass the responsibility to the
// caller who does know whether the available logical width is indefinite or not.
void RenderGrid::placeItemsOnGrid(GridTrackSizingAlgorithm& algorithm, std::optional<LayoutUnit> availableLogicalWidth) const
{
    Grid& grid = algorithm.mutableGrid();
    unsigned autoRepeatColumns = computeAutoRepeatTracksCount(ForColumns, availableLogicalWidth);
    unsigned autoRepeatRows = computeAutoRepeatTracksCount(ForRows, availableLogicalHeightForPercentageComputation());

    autoRepeatRows = clampAutoRepeatTracks(ForRows, autoRepeatRows);
    autoRepeatColumns = clampAutoRepeatTracks(ForColumns, autoRepeatColumns);

    if (autoRepeatColumns != grid.autoRepeatTracks(ForColumns) || autoRepeatRows != grid.autoRepeatTracks(ForRows)) {
        grid.setNeedsItemsPlacement(true);
        grid.setAutoRepeatTracks(autoRepeatRows, autoRepeatColumns);
    }

    if (!grid.needsItemsPlacement())
        return;

    ASSERT(!grid.hasGridItems());
    populateExplicitGridAndOrderIterator(grid);

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*child))
            continue;

        // Grid items should use the grid area sizes instead of the containing block (grid container)
        // sizes, we initialize the overrides here if needed to ensure it.
        if (!child->hasOverrideContainingBlockContentLogicalWidth())
            child->setOverrideContainingBlockContentLogicalWidth(LayoutUnit());
        if (!child->hasOverrideContainingBlockContentLogicalHeight())
            child->setOverrideContainingBlockContentLogicalHeight(LayoutUnit(-1));

        GridArea area = grid.gridItemArea(*child);
        if (!area.rows.isIndefinite())
            area.rows.translate(std::abs(grid.smallestTrackStart(ForRows)));
        if (!area.columns.isIndefinite())
            area.columns.translate(std::abs(grid.smallestTrackStart(ForColumns)));

        if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
            grid.setGridItemArea(*child, area);
            bool majorAxisDirectionIsForColumns = autoPlacementMajorAxisDirection() == ForColumns;
            if ((majorAxisDirectionIsForColumns && area.columns.isIndefinite())
                || (!majorAxisDirectionIsForColumns && area.rows.isIndefinite()))
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        grid.insert(*child, { area.rows, area.columns });
    }

#if !ASSERT_DISABLED
    if (grid.hasGridItems()) {
        ASSERT(grid.numTracks(ForRows) >= GridPositionsResolver::explicitGridRowCount(style(), grid.autoRepeatTracks(ForRows)));
        ASSERT(grid.numTracks(ForColumns) >= GridPositionsResolver::explicitGridColumnCount(style(), grid.autoRepeatTracks(ForColumns)));
    }
#endif

    placeSpecifiedMajorAxisItemsOnGrid(grid, specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(grid, autoMajorAxisAutoGridItems);

    // Compute collapsible tracks for auto-fit.
    grid.setAutoRepeatEmptyColumns(computeEmptyTracksForAutoRepeat(grid, ForColumns));
    grid.setAutoRepeatEmptyRows(computeEmptyTracksForAutoRepeat(grid, ForRows));

    grid.setNeedsItemsPlacement(false);

#if !ASSERT_DISABLED
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*child))
            continue;

        GridArea area = grid.gridItemArea(*child);
        ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    }
#endif
}

void RenderGrid::performGridItemsPreLayout(const GridTrackSizingAlgorithm& algorithm) const
{
    ASSERT(!algorithm.grid().needsItemsPlacement());
    // FIXME: We need a way when we are calling this during intrinsic size compuation before performing
    // the layout. Maybe using the PreLayout phase ?
    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        // Orthogonal items should be laid out in order to properly compute content-sized tracks that may depend on item's intrinsic size.
        // We also need to properly estimate its grid area size, since it may affect to the baseline shims if such item particiaptes in baseline alignment. 
        if (GridLayoutFunctions::isOrthogonalChild(*this, *child)) {
            updateGridAreaLogicalSize(*child, algorithm.estimatedGridAreaBreadthForChild(*child));
            child->layoutIfNeeded();
            continue;
        }
        // We need to layout the item to know whether it must synthesize its
        // baseline or not, which may imply a cyclic sizing dependency.
        // FIXME: Can we avoid it ?
        if (isBaselineAlignmentForChild(*child)) {
            updateGridAreaLogicalSize(*child, algorithm.estimatedGridAreaBreadthForChild(*child));
            child->layoutIfNeeded();
        }
    }
}

void RenderGrid::populateExplicitGridAndOrderIterator(Grid& grid) const
{
    OrderIteratorPopulator populator(grid.orderIterator());
    int smallestRowStart = 0;
    int smallestColumnStart = 0;
    unsigned autoRepeatRows = grid.autoRepeatTracks(ForRows);
    unsigned autoRepeatColumns = grid.autoRepeatTracks(ForColumns);
    unsigned maximumRowIndex = GridPositionsResolver::explicitGridRowCount(style(), autoRepeatRows);
    unsigned maximumColumnIndex = GridPositionsResolver::explicitGridColumnCount(style(), autoRepeatColumns);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!populator.collectChild(*child))
            continue;
        
        GridSpan rowPositions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), *child, ForRows, autoRepeatRows);
        if (!rowPositions.isIndefinite()) {
            smallestRowStart = std::min(smallestRowStart, rowPositions.untranslatedStartLine());
            maximumRowIndex = std::max<int>(maximumRowIndex, rowPositions.untranslatedEndLine());
        } else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *child, ForRows);
            maximumRowIndex = std::max(maximumRowIndex, spanSize);
        }

        GridSpan columnPositions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), *child, ForColumns, autoRepeatColumns);
        if (!columnPositions.isIndefinite()) {
            smallestColumnStart = std::min(smallestColumnStart, columnPositions.untranslatedStartLine());
            maximumColumnIndex = std::max<int>(maximumColumnIndex, columnPositions.untranslatedEndLine());
        } else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *child, ForColumns);
            maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
        }

        grid.setGridItemArea(*child, { rowPositions, columnPositions });
    }

    grid.setSmallestTracksStart(smallestRowStart, smallestColumnStart);
    grid.ensureGridSize(maximumRowIndex + std::abs(smallestRowStart), maximumColumnIndex + std::abs(smallestColumnStart));
}

std::unique_ptr<GridArea> RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(Grid& grid, const RenderBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const unsigned endOfCrossDirection = grid.numTracks(crossDirection);
    unsigned crossDirectionSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, crossDirection);
    GridSpan crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);
    return std::make_unique<GridArea>(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions);
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(Grid& grid, const Vector<RenderBox*>& autoGridItems) const
{
    bool isForColumns = autoPlacementMajorAxisDirection() == ForColumns;
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    // Mapping between the major axis tracks (rows or columns) and the last auto-placed item's position inserted on
    // that track. This is needed to implement "sparse" packing for items locked to a given track.
    // See http://dev.w3.org/csswg/css-grid/#auto-placement-algorithm
    HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> minorAxisCursors;

    for (auto& autoGridItem : autoGridItems) {
        GridSpan majorAxisPositions = grid.gridItemSpan(*autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(majorAxisPositions.isTranslatedDefinite());
        ASSERT(grid.gridItemSpan(*autoGridItem, autoPlacementMinorAxisDirection()).isIndefinite());
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *autoGridItem, autoPlacementMinorAxisDirection());
        unsigned majorAxisInitialPosition = majorAxisPositions.startLine();

        GridIterator iterator(grid, autoPlacementMajorAxisDirection(), majorAxisPositions.startLine(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        std::unique_ptr<GridArea> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisSpanSize);
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(grid, *autoGridItem, autoPlacementMajorAxisDirection(), majorAxisPositions);

        grid.insert(*autoGridItem, *emptyGridArea);

        if (!isGridAutoFlowDense)
            minorAxisCursors.set(majorAxisInitialPosition, isForColumns ? emptyGridArea->rows.startLine() : emptyGridArea->columns.startLine());
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(Grid& grid, const Vector<RenderBox*>& autoGridItems) const
{
    AutoPlacementCursor autoPlacementCursor = {0, 0};
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    for (auto& autoGridItem : autoGridItems) {
        placeAutoMajorAxisItemOnGrid(grid, *autoGridItem, autoPlacementCursor);

        if (isGridAutoFlowDense) {
            autoPlacementCursor.first = 0;
            autoPlacementCursor.second = 0;
        }
    }
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(Grid& grid, RenderBox& gridItem, AutoPlacementCursor& autoPlacementCursor) const
{
    ASSERT(grid.gridItemSpan(gridItem, autoPlacementMajorAxisDirection()).isIndefinite());
    unsigned majorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, autoPlacementMajorAxisDirection());

    const unsigned endOfMajorAxis = grid.numTracks(autoPlacementMajorAxisDirection());
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    std::unique_ptr<GridArea> emptyGridArea;
    GridSpan minorAxisPositions = grid.gridItemSpan(gridItem, autoPlacementMinorAxisDirection());
    if (minorAxisPositions.isTranslatedDefinite()) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions.startLine() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(grid, autoPlacementMinorAxisDirection(), minorAxisPositions.startLine(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions.integerSpan(), majorAxisSpanSize);
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(grid, gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    } else {
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, autoPlacementMinorAxisDirection());

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                unsigned minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.endLine() : emptyGridArea->rows.endLine();
                const unsigned endOfMinorAxis = grid.numTracks(autoPlacementMinorAxisDirection());
                if (minorAxisFinalPositionIndex <= endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = nullptr;
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(grid, gridItem, autoPlacementMinorAxisDirection(), GridSpan::translatedDefiniteGridSpan(0, minorAxisSpanSize));
    }

    grid.insert(gridItem, *emptyGridArea);
    autoPlacementCursor.first = emptyGridArea->rows.startLine();
    autoPlacementCursor.second = emptyGridArea->columns.startLine();
}

GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForColumns : ForRows;
}

GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForRows : ForColumns;
}

void RenderGrid::dirtyGrid()
{
    if (m_grid.needsItemsPlacement())
        return;

    m_grid.setNeedsItemsPlacement(true);
}

Vector<LayoutUnit> RenderGrid::trackSizesForComputedStyle(GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == ForColumns;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    size_t numPositions = positions.size();
    LayoutUnit offsetBetweenTracks = isRowAxis ? m_offsetBetweenColumns.distributionOffset : m_offsetBetweenRows.distributionOffset;

    Vector<LayoutUnit> tracks;
    if (numPositions < 2)
        return tracks;

    ASSERT(!m_grid.needsItemsPlacement());
    bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
    LayoutUnit gap = !hasCollapsedTracks ? gridGap(direction) : LayoutUnit();
    tracks.reserveCapacity(numPositions - 1);
    for (size_t i = 0; i < numPositions - 2; ++i)
        tracks.append(positions[i + 1] - positions[i] - offsetBetweenTracks - gap);
    tracks.append(positions[numPositions - 1] - positions[numPositions - 2]);

    if (!hasCollapsedTracks)
        return tracks;

    size_t remainingEmptyTracks = m_grid.autoRepeatEmptyTracks(direction)->size();
    size_t lastLine = tracks.size();
    gap = gridGap(direction);
    for (size_t i = 1; i < lastLine; ++i) {
        if (m_grid.isEmptyAutoRepeatTrack(direction, i - 1))
            --remainingEmptyTracks;
        else {
            // Remove the gap between consecutive non empty tracks. Remove it also just once for an
            // arbitrary number of empty tracks between two non empty ones.
            bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
            if (!allRemainingTracksAreEmpty || !m_grid.isEmptyAutoRepeatTrack(direction, i))
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

static bool overrideSizeChanged(const RenderBox& child, GridTrackSizingDirection direction, LayoutSize size)
{
    if (direction == ForColumns)
        return !child.hasOverrideContainingBlockContentLogicalWidth() || child.overrideContainingBlockContentLogicalWidth() != size.width();
    return !child.hasOverrideContainingBlockContentLogicalHeight() || child.overrideContainingBlockContentLogicalHeight() != size.height();
}

static bool hasRelativeBlockAxisSize(const RenderGrid& grid, const RenderBox& child)
{
    return GridLayoutFunctions::isOrthogonalChild(grid, child) ? child.hasRelativeLogicalWidth() || child.style().logicalWidth().isAuto() : child.hasRelativeLogicalHeight();
}

void RenderGrid::updateGridAreaLogicalSize(RenderBox& child, LayoutSize gridAreaLogicalSize) const
{
    // Because the grid area cannot be styled, we don't need to adjust
    // the grid breadth to account for 'box-sizing'.
    bool gridAreaWidthChanged = overrideSizeChanged(child, ForColumns, gridAreaLogicalSize);
    bool gridAreaHeightChanged = overrideSizeChanged(child, ForRows, gridAreaLogicalSize);
    if (gridAreaWidthChanged || (gridAreaHeightChanged && hasRelativeBlockAxisSize(*this, child)))
        child.setNeedsLayout(MarkOnlyThis);

    child.setOverrideContainingBlockContentLogicalWidth(gridAreaLogicalSize.width());
    child.setOverrideContainingBlockContentLogicalHeight(gridAreaLogicalSize.height());
}

void RenderGrid::layoutGridItems()
{
    populateGridPositionsForDirection(ForColumns);
    populateGridPositionsForDirection(ForRows);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        
        if (m_grid.orderIterator().shouldSkipChild(*child)) {
            if (child->isOutOfFlowPositioned())
                prepareChildForPositionedLayout(*child);
            continue;
        }

        // Setting the definite grid area's sizes. It may imply that the
        // item must perform a layout if its area differs from the one
        // used during the track sizing algorithm.
        updateGridAreaLogicalSize(*child, LayoutSize(gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForColumns), gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForRows)));

        LayoutRect oldChildRect = child->frameRect();

        // Stretching logic might force a child layout, so we need to run it before the layoutIfNeeded
        // call to avoid unnecessary relayouts. This might imply that child margins, needed to correctly
        // determine the available space before stretching, are not set yet.
        applyStretchAlignmentToChildIfNeeded(*child);

        child->layoutIfNeeded();

        // We need pending layouts to be done in order to compute auto-margins properly.
        updateAutoMarginsInColumnAxisIfNeeded(*child);
        updateAutoMarginsInRowAxisIfNeeded(*child);

        child->setLogicalLocation(findChildLogicalPosition(*child));

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRect);
    }
}

void RenderGrid::prepareChildForPositionedLayout(RenderBox& child)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(child);

    RenderLayer* childLayer = child.layer();
    // Static position of a positioned child should use the content-box (https://drafts.csswg.org/css-grid/#static-position).
    childLayer->setStaticInlinePosition(borderAndPaddingStart());
    childLayer->setStaticBlockPosition(borderAndPaddingBefore());
}

bool RenderGrid::hasStaticPositionForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? child.style().hasStaticInlinePosition(isHorizontalWritingMode()) : child.style().hasStaticBlockPosition(isHorizontalWritingMode());
}

void RenderGrid::layoutPositionedObject(RenderBox& child, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    LayoutUnit columnBreadth = gridAreaBreadthForOutOfFlowChild(child, ForColumns);
    LayoutUnit rowBreadth = gridAreaBreadthForOutOfFlowChild(child, ForRows);

    child.setOverrideContainingBlockContentLogicalWidth(columnBreadth);
    child.setOverrideContainingBlockContentLogicalHeight(rowBreadth);

    // Mark for layout as we're resetting the position before and we relay in generic layout logic
    // for positioned items in order to get the offsets properly resolved.
    child.setChildNeedsLayout(MarkOnlyThis);

    RenderBlock::layoutPositionedObject(child, relayoutChildren, fixedPositionObjectsOnly);

    if (child.isGridItem() || !hasStaticPositionForChild(child, ForColumns) || !hasStaticPositionForChild(child, ForRows))
        child.setLogicalLocation(findChildLogicalPosition(child));
}

LayoutUnit RenderGrid::gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox& child, GridTrackSizingDirection direction) const
{
    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    const auto& span = m_grid.gridItemSpan(child, direction);
    const auto& linePositions = (direction == ForColumns) ? m_columnPositions : m_rowPositions;

    LayoutUnit initialTrackPosition = linePositions[span.startLine()];
    LayoutUnit finalTrackPosition = linePositions[span.endLine() - 1];

    // Track Positions vector stores the 'start' grid line of each track, so we have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.endLine() - 1].baseSize();
}

void RenderGrid::populateGridPositionsForDirection(GridTrackSizingDirection direction)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.

    // The grid container's frame elements (border, padding and <content-position> offset) are sensible to the
    // inline-axis flow direction. However, column lines positions are 'direction' unaware. This simplification
    // allows us to use the same indexes to identify the columns independently on the inline-axis direction.
    bool isRowAxis = direction == ForColumns;
    auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    unsigned numberOfTracks = tracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;
    bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? m_grid.autoRepeatEmptyTracks(direction)->size() : 0;
    const auto& offset = direction == ForColumns ? m_offsetBetweenColumns : m_offsetBetweenRows;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    positions.resize(numberOfLines);

    auto borderAndPadding = isRowAxis ? borderAndPaddingLogicalLeft() : borderAndPaddingBefore();
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Ideally scrollbarLogicalWidth() should return zero in iOS so we don't need this
    // (see bug https://webkit.org/b/191857).
    // If we are in horizontal writing mode and RTL direction the scrollbar is painted on the left,
    // so we need to take into account when computing the position of the columns.
    if (isRowAxis && style().isHorizontalWritingMode() && !style().isLeftToRightDirection())
        borderAndPadding += scrollbarLogicalWidth();
#endif

    positions[0] = borderAndPadding + offset.positionOffset;
    if (numberOfLines > 1) {
        // If we have collapsed tracks we just ignore gaps here and add them later as we might not
        // compute the gap between two consecutive tracks without examining the surrounding ones.
        LayoutUnit gap = !hasCollapsedTracks ? gridGap(direction) : LayoutUnit();
        unsigned nextToLastLine = numberOfLines - 2;
        for (unsigned i = 0; i < nextToLastLine; ++i)
            positions[i + 1] = positions[i] + offset.distributionOffset + tracks[i].baseSize() + gap;
        positions[lastLine] = positions[nextToLastLine] + tracks[nextToLastLine].baseSize();

        // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to collapse (they
        // coincide exactly) except on the edges of the grid where they become 0.
        if (hasCollapsedTracks) {
            gap = gridGap(direction);
            unsigned remainingEmptyTracks = numberOfCollapsedTracks;
            LayoutUnit offsetAccumulator;
            LayoutUnit gapAccumulator;
            for (unsigned i = 1; i < lastLine; ++i) {
                if (m_grid.isEmptyAutoRepeatTrack(direction, i - 1)) {
                    --remainingEmptyTracks;
                    offsetAccumulator += offset.distributionOffset;
                } else {
                    // Add gap between consecutive non empty tracks. Add it also just once for an
                    // arbitrary number of empty tracks between two non empty ones.
                    bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
                    if (!allRemainingTracksAreEmpty || !m_grid.isEmptyAutoRepeatTrack(direction, i))
                        gapAccumulator += gap;
                }
                positions[i] += gapAccumulator - offsetAccumulator;
            }
            positions[lastLine] += gapAccumulator - offsetAccumulator;
        }
    }
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackSize, LayoutUnit childSize)
{
    LayoutUnit offset = trackSize - childSize;
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

LayoutUnit RenderGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox& child) const
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // children are laid out, so we can't use the child cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    GridTrackSizingDirection childBlockFlowDirection = GridLayoutFunctions::flowAwareDirectionForChild(*this, child, ForRows);
    return gridAreaBreadthForChild - GridLayoutFunctions::marginLogicalSizeForChild(*this, childBlockFlowDirection, child);
}

StyleSelfAlignmentData RenderGrid::alignSelfForChild(const RenderBox& child, const RenderStyle* gridStyle) const
{
    if (!gridStyle)
        gridStyle = &style();
    return child.style().resolvedAlignSelf(gridStyle, selfAlignmentNormalBehavior(&child));
}

StyleSelfAlignmentData RenderGrid::justifySelfForChild(const RenderBox& child, const RenderStyle* gridStyle) const
{
    if (!gridStyle)
        gridStyle = &style();
    return child.style().resolvedJustifySelf(gridStyle, selfAlignmentNormalBehavior(&child));
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::applyStretchAlignmentToChildIfNeeded(RenderBox& child)
{
    ASSERT(child.overrideContainingBlockContentLogicalHeight());

    // We clear height override values because we will decide now whether it's allowed or
    // not, evaluating the conditions which might have changed since the old values were set.
    child.clearOverrideContentLogicalHeight();

    GridTrackSizingDirection childBlockDirection = GridLayoutFunctions::flowAwareDirectionForChild(*this, child, ForRows);
    bool blockFlowIsColumnAxis = childBlockDirection == ForRows;
    bool allowedToStretchChildBlockSize = blockFlowIsColumnAxis ? allowedToStretchChildAlongColumnAxis(child) : allowedToStretchChildAlongRowAxis(child);
    if (allowedToStretchChildBlockSize) {
        LayoutUnit stretchedLogicalHeight = availableAlignmentSpaceForChildBeforeStretching(GridLayoutFunctions::overrideContainingBlockContentSizeForChild(child, childBlockDirection).value(), child);
        LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, LayoutUnit(-1));
        child.setOverrideContentLogicalHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
        if (desiredLogicalHeight != child.logicalHeight()) {
            // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
            child.setLogicalHeight(LayoutUnit());
            child.setNeedsLayout();
        }
    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::hasAutoMarginsInColumnAxis(const RenderBox& child) const
{
    if (isHorizontalWritingMode())
        return child.style().marginTop().isAuto() || child.style().marginBottom().isAuto();
    return child.style().marginLeft().isAuto() || child.style().marginRight().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::hasAutoMarginsInRowAxis(const RenderBox& child) const
{
    if (isHorizontalWritingMode())
        return child.style().marginLeft().isAuto() || child.style().marginRight().isAuto();
    return child.style().marginTop().isAuto() || child.style().marginBottom().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInRowAxisIfNeeded(RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalWidth().value() - child.logicalWidth() - child.marginLogicalWidth();
    if (availableAlignmentSpace <= 0)
        return;

    const RenderStyle& parentStyle = style();
    Length marginStart = child.style().marginStartUsing(&parentStyle);
    Length marginEnd = child.style().marginEndUsing(&parentStyle);
    if (marginStart.isAuto() && marginEnd.isAuto()) {
        child.setMarginStart(availableAlignmentSpace / 2, &parentStyle);
        child.setMarginEnd(availableAlignmentSpace / 2, &parentStyle);
    } else if (marginStart.isAuto()) {
        child.setMarginStart(availableAlignmentSpace, &parentStyle);
    } else if (marginEnd.isAuto()) {
        child.setMarginEnd(availableAlignmentSpace, &parentStyle);
    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInColumnAxisIfNeeded(RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalHeight().value() - child.logicalHeight() - child.marginLogicalHeight();
    if (availableAlignmentSpace <= 0)
        return;

    const RenderStyle& parentStyle = style();
    Length marginBefore = child.style().marginBeforeUsing(&parentStyle);
    Length marginAfter = child.style().marginAfterUsing(&parentStyle);
    if (marginBefore.isAuto() && marginAfter.isAuto()) {
        child.setMarginBefore(availableAlignmentSpace / 2, &parentStyle);
        child.setMarginAfter(availableAlignmentSpace / 2, &parentStyle);
    } else if (marginBefore.isAuto()) {
        child.setMarginBefore(availableAlignmentSpace, &parentStyle);
    } else if (marginAfter.isAuto()) {
        child.setMarginAfter(availableAlignmentSpace, &parentStyle);
    }
}

// FIXME: This logic could be refactored somehow and defined in RenderBox.
static int synthesizedBaselineFromBorderBox(const RenderBox& box, LineDirectionMode direction)
{
    return (direction == HorizontalLine ? box.size().height() : box.size().width()).toInt();
}

static int synthesizedBaselineFromMarginBox(const RenderBox& box, LineDirectionMode direction)
{
    return (direction == HorizontalLine ? box.size().height() + box.verticalMarginExtent() : box.size().width() + box.horizontalMarginExtent()).toInt();
}

bool RenderGrid::isBaselineAlignmentForChild(const RenderBox& child) const
{
    return isBaselineAlignmentForChild(child, GridRowAxis) || isBaselineAlignmentForChild(child, GridColumnAxis);
}

bool RenderGrid::isBaselineAlignmentForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    if (child.isOutOfFlowPositioned())
        return false;
    ItemPosition align = selfAlignmentForChild(baselineAxis, child).position();
    bool hasAutoMargins = baselineAxis == GridColumnAxis ? hasAutoMarginsInColumnAxis(child) : hasAutoMarginsInRowAxis(child);
    return isBaselinePosition(align) && !hasAutoMargins;
}

// FIXME: This logic is shared by RenderFlexibleBox, so it might be refactored somehow.
int RenderGrid::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode mode) const
{
#if !ASSERT_DISABLED
    ASSERT(mode == PositionOnContainingLine);
#else
    UNUSED_PARAM(mode);
#endif
    return firstLineBaseline().value_or(synthesizedBaselineFromMarginBox(*this, direction));
}

std::optional<int> RenderGrid::firstLineBaseline() const
{
    if (isWritingModeRoot() || !m_grid.hasGridItems())
        return std::nullopt;

    const RenderBox* baselineChild = nullptr;
    // Finding the first grid item in grid order.
    unsigned numColumns = m_grid.numTracks(ForColumns);
    for (size_t column = 0; column < numColumns; column++) {
        for (auto& child : m_grid.cell(0, column)) {
            ASSERT(child.get());
            // If an item participates in baseline alignment, we select such item.
            if (isBaselineAlignmentForChild(*child)) {
                // FIXME: self-baseline and content-baseline alignment not implemented yet.
                baselineChild = child.get();
                break;
            }
            if (!baselineChild)
                baselineChild = child.get();
        }
    }

    if (!baselineChild)
        return std::nullopt;

    auto baseline = GridLayoutFunctions::isOrthogonalChild(*this, *baselineChild) ? std::nullopt : baselineChild->firstLineBaseline();
    // We take border-box's bottom if no valid baseline.
    if (!baseline) {
        // FIXME: We should pass |direction| into firstLineBaseline and stop bailing out if we're a writing
        // mode root. This would also fix some cases where the grid is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaselineFromBorderBox(*baselineChild, direction) + baselineChild->logicalTop().toInt();
    }
    return baseline.value() + baselineChild->logicalTop().toInt();
}

std::optional<int> RenderGrid::inlineBlockBaseline(LineDirectionMode direction) const
{
    if (std::optional<int> baseline = firstLineBaseline())
        return baseline;

    int marginAscent = direction == HorizontalLine ? marginBottom() : marginRight();
    return synthesizedBaselineFromBorderBox(*this, direction) + marginAscent;
}

LayoutUnit RenderGrid::columnAxisBaselineOffsetForChild(const RenderBox& child) const
{
    return m_trackSizingAlgorithm.baselineOffsetForChild(child, GridColumnAxis);
}

LayoutUnit RenderGrid::rowAxisBaselineOffsetForChild(const RenderBox& child) const
{
    return m_trackSizingAlgorithm.baselineOffsetForChild(child, GridRowAxis);
}

GridAxisPosition RenderGrid::columnAxisPositionForChild(const RenderBox& child) const
{
    bool hasSameWritingMode = child.style().writingMode() == style().writingMode();
    bool childIsLTR = child.style().isLeftToRightDirection();
    if (!hasStaticPositionForChild(child, ForRows))
        return GridAxisStart;

    switch (alignSelfForChild(child).position()) {
    case ItemPosition::SelfStart:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'start' side in the column axis.
        if (GridLayoutFunctions::isOrthogonalChild(*this, child)) {
            // If orthogonal writing-modes, self-start will be based on the child's inline-axis
            // direction (inline-start), because it's the one parallel to the column axis.
            if (style().isFlippedBlocksWritingMode())
                return childIsLTR ? GridAxisEnd : GridAxisStart;
            return childIsLTR ? GridAxisStart : GridAxisEnd;
        }
        // self-start is based on the child's block-flow direction. That's why we need to check against the grid container's block-flow direction.
        return hasSameWritingMode ? GridAxisStart : GridAxisEnd;
    case ItemPosition::SelfEnd:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'end' side in the column axis.
        if (GridLayoutFunctions::isOrthogonalChild(*this, child)) {
            // If orthogonal writing-modes, self-end will be based on the child's inline-axis
            // direction, (inline-end) because it's the one parallel to the column axis.
            if (style().isFlippedBlocksWritingMode())
                return childIsLTR ? GridAxisStart : GridAxisEnd;
            return childIsLTR ? GridAxisEnd : GridAxisStart;
        }
        // self-end is based on the child's block-flow direction. That's why we need to check against the grid container's block-flow direction.
        return hasSameWritingMode ? GridAxisEnd : GridAxisStart;
    case ItemPosition::Left:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisStart;
    case ItemPosition::Right:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisStart;
    case ItemPosition::Center:
        return GridAxisCenter;
    case ItemPosition::FlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (block-start) in the column axis.
    case ItemPosition::Start:
        return GridAxisStart;
    case ItemPosition::FlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (block-end) in the column axis.
    case ItemPosition::End:
        return GridAxisEnd;
    case ItemPosition::Stretch:
        return GridAxisStart;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

GridAxisPosition RenderGrid::rowAxisPositionForChild(const RenderBox& child) const
{
    bool hasSameDirection = child.style().direction() == style().direction();
    bool gridIsLTR = style().isLeftToRightDirection();
    if (!hasStaticPositionForChild(child, ForColumns))
        return GridAxisStart;

    switch (justifySelfForChild(child).position()) {
    case ItemPosition::SelfStart:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'start' side in the row axis.
        if (GridLayoutFunctions::isOrthogonalChild(*this, child)) {
            // If orthogonal writing-modes, self-start will be based on the child's block-axis
            // direction, because it's the one parallel to the row axis.
            if (child.style().isFlippedBlocksWritingMode())
                return gridIsLTR ? GridAxisEnd : GridAxisStart;
            return gridIsLTR ? GridAxisStart : GridAxisEnd;
        }
        // self-start is based on the child's inline-flow direction. That's why we need to check against the grid container's direction.
        return hasSameDirection ? GridAxisStart : GridAxisEnd;
    case ItemPosition::SelfEnd:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'end' side in the row axis.
        if (GridLayoutFunctions::isOrthogonalChild(*this, child)) {
            // If orthogonal writing-modes, self-end will be based on the child's block-axis
            // direction, because it's the one parallel to the row axis.
            if (child.style().isFlippedBlocksWritingMode())
                return gridIsLTR ? GridAxisStart : GridAxisEnd;
            return gridIsLTR ? GridAxisEnd : GridAxisStart;
        }
        // self-end is based on the child's inline-flow direction. That's why we need to check against the grid container's direction.
        return hasSameDirection ? GridAxisEnd : GridAxisStart;
    case ItemPosition::Left:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // We want the physical 'left' side, so we have to take account, container's inline-flow direction.
        return gridIsLTR ? GridAxisStart : GridAxisEnd;
    case ItemPosition::Right:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // We want the physical 'right' side, so we have to take account, container's inline-flow direction.
        return gridIsLTR ? GridAxisEnd : GridAxisStart;
    case ItemPosition::Center:
        return GridAxisCenter;
    case ItemPosition::FlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (inline-start) in the row axis.
    case ItemPosition::Start:
        return GridAxisStart;
    case ItemPosition::FlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (inline-end) in the row axis.
    case ItemPosition::End:
        return GridAxisEnd;
    case ItemPosition::Stretch:
        return GridAxisStart;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

LayoutUnit RenderGrid::columnAxisOffsetForChild(const RenderBox& child) const
{
    LayoutUnit startOfRow;
    LayoutUnit endOfRow;
    gridAreaPositionForChild(child, ForRows, startOfRow, endOfRow);
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(child);
    if (hasAutoMarginsInColumnAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = columnAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition + columnAxisBaselineOffsetForChild(child);
    case GridAxisEnd:
    case GridAxisCenter: {
        LayoutUnit columnAxisChildSize = GridLayoutFunctions::isOrthogonalChild(*this, child) ? child.logicalWidth() + child.marginLogicalWidth() : child.logicalHeight() + child.marginLogicalHeight();
        auto overflow = alignSelfForChild(child).overflow();
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfRow - startOfRow, columnAxisChildSize);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderGrid::rowAxisOffsetForChild(const RenderBox& child) const
{
    LayoutUnit startOfColumn;
    LayoutUnit endOfColumn;
    gridAreaPositionForChild(child, ForColumns, startOfColumn, endOfColumn);
    LayoutUnit startPosition = startOfColumn + marginStartForChild(child);
    if (hasAutoMarginsInRowAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = rowAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition + rowAxisBaselineOffsetForChild(child);
    case GridAxisEnd:
    case GridAxisCenter: {
        LayoutUnit rowAxisChildSize = GridLayoutFunctions::isOrthogonalChild(*this, child) ? child.logicalHeight() + child.marginLogicalHeight() : child.logicalWidth() + child.marginLogicalWidth();
        auto overflow = justifySelfForChild(child).overflow();
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfColumn - startOfColumn, rowAxisChildSize);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool RenderGrid::gridPositionIsAutoForOutOfFlow(GridPosition position, GridTrackSizingDirection direction) const
{
    return position.isAuto() || (position.isNamedGridArea() && !NamedLineCollection::isValidNamedLineOrArea(position.namedGridLine(), style(), GridPositionsResolver::initialPositionSide(direction)));
}

LayoutUnit RenderGrid::resolveAutoStartGridPosition(GridTrackSizingDirection direction) const
{
    if (direction == ForRows || style().isLeftToRightDirection())
        return LayoutUnit();

    int lastLine = numTracks(ForColumns, m_grid);
    ContentPosition position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehaviorGrid());
    if (position == ContentPosition::End)
        return m_columnPositions[lastLine] - clientLogicalWidth();
    if (position == ContentPosition::Start || style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorGrid()) == ContentDistribution::Stretch)
        return m_columnPositions[0] - borderAndPaddingLogicalLeft();
    return LayoutUnit();
}

LayoutUnit RenderGrid::resolveAutoEndGridPosition(GridTrackSizingDirection direction) const
{
    if (direction == ForRows)
        return clientLogicalHeight();
    if (style().isLeftToRightDirection())
        return clientLogicalWidth();

    int lastLine = numTracks(ForColumns, m_grid);
    ContentPosition position = style().resolvedJustifyContentPosition(contentAlignmentNormalBehaviorGrid());
    if (position == ContentPosition::End)
        return m_columnPositions[lastLine];
    if (position == ContentPosition::Start || style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorGrid()) == ContentDistribution::Stretch)
        return m_columnPositions[0] - borderAndPaddingLogicalLeft() + clientLogicalWidth();
    return clientLogicalWidth();
}

LayoutUnit RenderGrid::gridAreaBreadthForOutOfFlowChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    ASSERT(child.isOutOfFlowPositioned());
    bool isRowAxis = direction == ForColumns;
    GridSpan span = GridPositionsResolver::resolveGridPositionsFromStyle(style(), child, direction, autoRepeatCountForDirection(direction));
    if (span.isIndefinite())
        return isRowAxis ? clientLogicalWidth() : clientLogicalHeight();

    int smallestStart = abs(m_grid.smallestTrackStart(direction));
    int startLine = span.untranslatedStartLine() + smallestStart;
    int endLine = span.untranslatedEndLine() + smallestStart;
    int lastLine = numTracks(direction, m_grid);
    GridPosition startPosition = direction == ForColumns ? child.style().gridItemColumnStart() : child.style().gridItemRowStart();
    GridPosition endPosition = direction == ForColumns ? child.style().gridItemColumnEnd() : child.style().gridItemRowEnd();

    bool startIsAuto = gridPositionIsAutoForOutOfFlow(startPosition, direction) || startLine < 0 || startLine > lastLine;
    bool endIsAuto = gridPositionIsAutoForOutOfFlow(endPosition, direction) || endLine < 0 || endLine > lastLine;

    if (startIsAuto && endIsAuto)
        return isRowAxis ? clientLogicalWidth() : clientLogicalHeight();

    LayoutUnit start;
    LayoutUnit end;
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    auto& outOfFlowItemLine = isRowAxis ? m_outOfFlowItemColumn : m_outOfFlowItemRow;
    LayoutUnit borderEdge = isRowAxis ? borderLogicalLeft() : borderBefore();
    if (startIsAuto)
        start = resolveAutoStartGridPosition(direction) + borderEdge;
    else {
        outOfFlowItemLine.set(&child, startLine);
        start = positions[startLine];
    }
    if (endIsAuto)
        end = resolveAutoEndGridPosition(direction) + borderEdge;
    else {
        end = positions[endLine];
        // These vectors store line positions including gaps, but we shouldn't consider them for the edges of the grid.
        std::optional<LayoutUnit> availableSizeForGutters = availableSpaceForGutters(direction);
        if (endLine > 0 && endLine < lastLine) {
            ASSERT(!m_grid.needsItemsPlacement());
            end -= guttersSize(m_grid, direction, endLine - 1, 2, availableSizeForGutters);
            end -= isRowAxis ? m_offsetBetweenColumns.distributionOffset : m_offsetBetweenRows.distributionOffset;
        }
    }
    return std::max(end - start, LayoutUnit());
}

LayoutUnit RenderGrid::logicalOffsetForChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit trackBreadth) const
{
    if (hasStaticPositionForChild(child, direction))
        return LayoutUnit();

    bool isRowAxis = direction == ForColumns;
    bool isFlowAwareRowAxis = GridLayoutFunctions::flowAwareDirectionForChild(*this, child, direction) == ForColumns;
    LayoutUnit childPosition = isFlowAwareRowAxis ? child.logicalLeft() : child.logicalTop();
    LayoutUnit gridBorder = isRowAxis ? borderLogicalLeft() : borderBefore();
    LayoutUnit childMargin = isFlowAwareRowAxis ? child.marginLogicalLeft() : child.marginBefore();
    LayoutUnit offset = childPosition - gridBorder - childMargin;
    if (!isRowAxis || style().isLeftToRightDirection())
        return offset;

    LayoutUnit childBreadth = isFlowAwareRowAxis ? child.logicalWidth() + child.marginLogicalWidth() : child.logicalHeight() + child.marginLogicalHeight();
    return trackBreadth - offset - childBreadth;
}

void RenderGrid::gridAreaPositionForOutOfFlowChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& start, LayoutUnit& end) const
{
    ASSERT(child.isOutOfFlowPositioned());
    ASSERT(GridLayoutFunctions::hasOverrideContainingBlockContentSizeForChild(child, direction));
    LayoutUnit trackBreadth = GridLayoutFunctions::overrideContainingBlockContentSizeForChild(child, direction).value();
    bool isRowAxis = direction == ForColumns;
    auto& outOfFlowItemLine = isRowAxis ? m_outOfFlowItemColumn : m_outOfFlowItemRow;
    start = isRowAxis ? borderLogicalLeft() : borderBefore();
    if (auto line = outOfFlowItemLine.get(&child)) {
        auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
        start = positions[line.value()];
    }
    start += logicalOffsetForChild(child, direction, trackBreadth);
    end = start + trackBreadth;
}

void RenderGrid::gridAreaPositionForInFlowChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& start, LayoutUnit& end) const
{
    ASSERT(!child.isOutOfFlowPositioned());
    const GridSpan& span = m_grid.gridItemSpan(child, direction);
    // FIXME (lajava): This is a common pattern, why not defining a function like
    // positions(direction) ?
    auto& positions = direction == ForColumns ? m_columnPositions : m_rowPositions;
    start = positions[span.startLine()];
    end = positions[span.endLine()];
    // The 'positions' vector includes distribution offset (because of content
    // alignment) and gutters so we need to subtract them to get the actual
    // end position for a given track (this does not have to be done for the
    // last track as there are no more positions's elements after it, nor for
    // collapsed tracks).
    if (span.endLine() < positions.size() - 1
        && !(m_grid.hasAutoRepeatEmptyTracks(direction)
        && m_grid.isEmptyAutoRepeatTrack(direction, span.endLine()))) {
        end -= gridGap(direction) + gridItemOffset(direction);
    }
}

void RenderGrid::gridAreaPositionForChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& start, LayoutUnit& end) const
{
    if (child.isOutOfFlowPositioned())
        gridAreaPositionForOutOfFlowChild(child, direction, start, end);
    else
        gridAreaPositionForInFlowChild(child, direction, start, end);
}

ContentPosition static resolveContentDistributionFallback(ContentDistribution distribution)
{
    switch (distribution) {
    case ContentDistribution::SpaceBetween:
        return ContentPosition::Start;
    case ContentDistribution::SpaceAround:
        return ContentPosition::Center;
    case ContentDistribution::SpaceEvenly:
        return ContentPosition::Center;
    case ContentDistribution::Stretch:
        return ContentPosition::Start;
    case ContentDistribution::Default:
        return ContentPosition::Normal;
    }

    ASSERT_NOT_REACHED();
    return ContentPosition::Normal;
}

static void contentDistributionOffset(ContentAlignmentData& offset, const LayoutUnit& availableFreeSpace, ContentPosition& fallbackPosition, ContentDistribution distribution, unsigned numberOfGridTracks)
{
    if (distribution != ContentDistribution::Default && fallbackPosition == ContentPosition::Normal)
        fallbackPosition = resolveContentDistributionFallback(distribution);

    // Initialize to an invalid offset.
    offset.positionOffset = LayoutUnit(-1);
    offset.distributionOffset = LayoutUnit(-1);
    if (availableFreeSpace <= 0)
        return;

    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
    switch (distribution) {
    case ContentDistribution::SpaceBetween:
        if (numberOfGridTracks < 2)
            return;
        distributionOffset = availableFreeSpace / (numberOfGridTracks - 1);
        positionOffset = LayoutUnit();
        break;
    case ContentDistribution::SpaceAround:
        if (numberOfGridTracks < 1)
            return;
        distributionOffset = availableFreeSpace / numberOfGridTracks;
        positionOffset = distributionOffset / 2;
        break;
    case ContentDistribution::SpaceEvenly:
        distributionOffset = availableFreeSpace / (numberOfGridTracks + 1);
        positionOffset = distributionOffset;
        break;
    case ContentDistribution::Stretch:
    case ContentDistribution::Default:
        return;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    offset.positionOffset = positionOffset;
    offset.distributionOffset = distributionOffset;
}

StyleContentAlignmentData RenderGrid::contentAlignment(GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? style().resolvedJustifyContent(contentAlignmentNormalBehaviorGrid()) : style().resolvedAlignContent(contentAlignmentNormalBehaviorGrid());
}

void RenderGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks)
{
    bool isRowAxis = direction == ForColumns;
    auto& offset =
        isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
    auto contentAlignmentData = contentAlignment(direction);
    auto position = contentAlignmentData.position();
    // If <content-distribution> value can't be applied, 'position' will become the associated
    // <content-position> fallback value.
    contentDistributionOffset(offset, availableFreeSpace, position, contentAlignmentData.distribution(), numberOfGridTracks);
    if (offset.isValid())
        return;

    if (availableFreeSpace <= 0 && contentAlignmentData.overflow() == OverflowAlignment::Safe) {
        offset.positionOffset = LayoutUnit();
        offset.distributionOffset = LayoutUnit();
        return;
    }

    LayoutUnit positionOffset;
    switch (position) {
    case ContentPosition::Left:
        ASSERT(isRowAxis);
        break;
    case ContentPosition::Right:
        ASSERT(isRowAxis);
        positionOffset = availableFreeSpace;
        break;
    case ContentPosition::Center:
        positionOffset = availableFreeSpace / 2;
        break;
    case ContentPosition::FlexEnd: // Only used in flex layout, for other layout, it's equivalent to 'end'.
    case ContentPosition::End:
        if (isRowAxis)
            positionOffset = style().isLeftToRightDirection() ? availableFreeSpace : LayoutUnit();
        else
            positionOffset = availableFreeSpace;
        break;
    case ContentPosition::FlexStart: // Only used in flex layout, for other layout, it's equivalent to 'start'.
    case ContentPosition::Start:
        if (isRowAxis)
            positionOffset = style().isLeftToRightDirection() ? LayoutUnit() : availableFreeSpace;
        break;
    case ContentPosition::Baseline:
    case ContentPosition::LastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align.
        // http://webkit.org/b/145566
        if (isRowAxis)
            positionOffset = style().isLeftToRightDirection() ? LayoutUnit() : availableFreeSpace;
        break;
    case ContentPosition::Normal:
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    offset.positionOffset = positionOffset;
    offset.distributionOffset = LayoutUnit();
}

LayoutUnit RenderGrid::translateOutOfFlowRTLCoordinate(const RenderBox& child, LayoutUnit coordinate) const
{
    ASSERT(child.isOutOfFlowPositioned());
    ASSERT(!style().isLeftToRightDirection());

    if (m_outOfFlowItemColumn.get(&child))
        return translateRTLCoordinate(coordinate);

    return borderLogicalLeft() + borderLogicalRight() + clientLogicalWidth() - coordinate;
}

LayoutUnit RenderGrid::translateRTLCoordinate(LayoutUnit coordinate) const
{
    ASSERT(!style().isLeftToRightDirection());

    LayoutUnit alignmentOffset = m_columnPositions[0];
    LayoutUnit rightGridEdgePosition = m_columnPositions[m_columnPositions.size() - 1];
    return rightGridEdgePosition + alignmentOffset - coordinate;
}

LayoutPoint RenderGrid::findChildLogicalPosition(const RenderBox& child) const
{
    LayoutUnit columnAxisOffset = columnAxisOffsetForChild(child);
    LayoutUnit rowAxisOffset = rowAxisOffsetForChild(child);
    bool isOrthogonalChild = GridLayoutFunctions::isOrthogonalChild(*this, child);

    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (!style().isLeftToRightDirection())
        rowAxisOffset = (child.isOutOfFlowPositioned() ? translateOutOfFlowRTLCoordinate(child, rowAxisOffset) : translateRTLCoordinate(rowAxisOffset)) - (isOrthogonalChild ? child.logicalHeight()  : child.logicalWidth());

    // "In the positioning phase [...] calculations are performed according to the writing mode
    // of the containing block of the box establishing the orthogonal flow." However, the
    // resulting LayoutPoint will be used in 'setLogicalPosition' in order to set the child's
    // logical position, which will only take into account the child's writing-mode.
    LayoutPoint childLocation(rowAxisOffset, columnAxisOffset);
    return isOrthogonalChild ? childLocation.transposedPoint() : childLocation;
}

unsigned RenderGrid::nonCollapsedTracks(GridTrackSizingDirection direction) const
{
    auto& tracks = m_trackSizingAlgorithm.tracks(direction);
    size_t numberOfTracks = tracks.size();
    bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
    size_t numberOfCollapsedTracks = hasCollapsedTracks ? m_grid.autoRepeatEmptyTracks(direction)->size() : 0;
    return numberOfTracks - numberOfCollapsedTracks;
}

unsigned RenderGrid::numTracks(GridTrackSizingDirection direction, const Grid& grid) const
{
    // Due to limitations in our internal representation, we cannot know the number of columns from
    // m_grid *if* there is no row (because m_grid would be empty). That's why in that case we need
    // to get it from the style. Note that we know for sure that there are't any implicit tracks,
    // because not having rows implies that there are no "normal" children (out-of-flow children are
    // not stored in m_grid).
    ASSERT(!grid.needsItemsPlacement());
    if (direction == ForRows)
        return grid.numTracks(ForRows);

    // FIXME: This still requires knowledge about m_grid internals.
    return grid.numTracks(ForRows) ? grid.numTracks(ForColumns) : GridPositionsResolver::explicitGridColumnCount(style(), grid.autoRepeatTracks(ForColumns));
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    ASSERT(!m_grid.needsItemsPlacement());
    for (RenderBox* child = m_grid.orderIterator().first(); child; child = m_grid.orderIterator().next())
        paintChild(*child, paintInfo, paintOffset, forChild, usePrintRect, PaintAsInlineBlock);
}

const char* RenderGrid::renderName() const
{
    if (isFloating())
        return "RenderGrid (floating)";
    if (isOutOfFlowPositioned())
        return "RenderGrid (positioned)";
    if (isAnonymous())
        return "RenderGrid (generated)";
    if (isRelativelyPositioned())
        return "RenderGrid (relative positioned)";
    return "RenderGrid";
}

} // namespace WebCore

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
#include "GridPositionsResolver.h"
#include "GridTrackSizingAlgorithm.h"
#include "LayoutRepainter.h"
#include "RenderChildIterator.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <cstdlib>

namespace WebCore {

enum TrackSizeRestriction {
    AllowInfinity,
    ForbidInfinity,
};

struct ContentAlignmentData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isValid() { return positionOffset >= 0 && distributionOffset >= 0; }
    static ContentAlignmentData defaultOffsets() { return {-1, -1}; }

    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
};

RenderGrid::RenderGrid(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
    , m_grid(*this)
    , m_trackSizingAlgorithm(this, m_grid)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid()
{
}

void RenderGrid::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlock::addChild(newChild, beforeChild);

    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are added.
    if (newChild->isOutOfFlowPositioned())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that
    // will change their position.
    dirtyGrid();
}

void RenderGrid::removeChild(RenderObject& child)
{
    RenderBlock::removeChild(child);

    // Positioned grid items do not take up space or otherwise participate in the layout of the grid,
    // for that reason we don't need to mark the grid as dirty when they are removed.
    if (child.isOutOfFlowPositioned())
        return;

    // The grid needs to be recomputed as it might contain auto-placed items that
    // will change their position.
    dirtyGrid();
}

StyleSelfAlignmentData RenderGrid::selfAlignmentForChild(GridAxis axis, const RenderBox& child, const RenderStyle* gridStyle) const
{
    return axis == GridRowAxis ? justifySelfForChild(child, gridStyle) : alignSelfForChild(child, gridStyle);
}

bool RenderGrid::selfAlignmentChangedToStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& child) const
{
    return selfAlignmentForChild(axis, child, &oldStyle).position() != ItemPositionStretch
        && selfAlignmentForChild(axis, child, &newStyle).position() == ItemPositionStretch;
}

bool RenderGrid::selfAlignmentChangedFromStretch(GridAxis axis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox& child) const
{
    return selfAlignmentForChild(axis, child, &oldStyle).position() == ItemPositionStretch
        && selfAlignmentForChild(axis, child, &newStyle).position() != ItemPositionStretch;
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifferenceLayout)
        return;

    const RenderStyle& newStyle = this->style();
    if (oldStyle->resolvedAlignItems(selfAlignmentNormalBehavior(this)).position() == ItemPositionStretch) {
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
    const Length& gap = isRowAxis ? style().gridColumnGap() : style().gridRowGap();
    if (!gap.isPercent())
        return std::nullopt;

    return isRowAxis ? availableLogicalWidth() : availableLogicalHeightForPercentageComputation();
}

LayoutUnit RenderGrid::computeTrackBasedLogicalHeight() const
{
    LayoutUnit logicalHeight;

    auto& allRows = m_trackSizingAlgorithm.tracks(ForRows);
    for (const auto& row : allRows)
        logicalHeight += row.baseSize();

    logicalHeight += guttersSize(m_grid, ForRows, 0, allRows.size(), availableSpaceForGutters(ForRows));

    return logicalHeight;
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
    if (m_grid.hasAnyOrthogonalGridItem()) {
        computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);
        computeTrackSizesForDefiniteSize(ForRows, availableSpaceForRows);
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
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    preparePaginationBeforeBlockLayout(relayoutChildren);

    LayoutSize previousSize = size();
    // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() but it does not work for positioned stuff.
    // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
    bool hasDefiniteLogicalHeight = hasOverrideLogicalContentHeight() || computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), std::nullopt);

    // We need to clear both own and containingBlock override sizes of orthogonal items to ensure we get the
    // same result when grid's intrinsic size is computed again in the updateLogicalWidth call bellow.
    if (sizesLogicalWidthToFitContent(MaxSize) || style().logicalWidth().isIntrinsicOrAuto()) {
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned() || !isOrthogonalChild(*child))
                continue;
            child->clearOverrideSize();
            child->clearContainingBlockOverrideSize();
            child->setNeedsLayout();
            child->layoutIfNeeded();
        }
    }

    setLogicalHeight(0);
    updateLogicalWidth();

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout. The same is true for ruby text.
    // It doesn't get included in the normal layout process but is instead skipped.
    layoutExcludedChildren(relayoutChildren);

    LayoutUnit availableSpaceForColumns = availableLogicalWidth();
    placeItemsOnGrid(m_grid, availableSpaceForColumns);

    // At this point the logical width is always definite as the above call to updateLogicalWidth()
    // properly resolves intrinsic sizes. We cannot do the same for heights though because many code
    // paths inside updateLogicalHeight() require a previous call to setLogicalHeight() to resolve
    // heights properly (like for positioned items for example).
    computeTrackSizesForDefiniteSize(ForColumns, availableSpaceForColumns);

    if (!hasDefiniteLogicalHeight) {
        m_minContentHeight = LayoutUnit();
        m_maxContentHeight = LayoutUnit();
        computeTrackSizesForIndefiniteSize(m_trackSizingAlgorithm, ForRows, m_grid, *m_minContentHeight, *m_maxContentHeight);
        // FIXME: This should be really added to the intrinsic height in RenderBox::computeContentAndScrollbarLogicalHeightUsing().
        // Remove this when that is fixed.
        ASSERT(m_minContentHeight);
        ASSERT(m_maxContentHeight);
        LayoutUnit scrollbarHeight = scrollbarLogicalHeight();
        *m_minContentHeight += scrollbarHeight;
        *m_maxContentHeight += scrollbarHeight;
    } else
        computeTrackSizesForDefiniteSize(ForRows, availableLogicalHeight(ExcludeMarginBorderPadding));
    LayoutUnit trackBasedLogicalHeight = computeTrackBasedLogicalHeight() + borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
    setLogicalHeight(trackBasedLogicalHeight);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    // Once grid's indefinite height is resolved, we can compute the
    // available free space for Content Alignment.
    if (!hasDefiniteLogicalHeight)
        m_trackSizingAlgorithm.setFreeSpace(ForRows, logicalHeight() - trackBasedLogicalHeight);

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

    layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

LayoutUnit RenderGrid::gridGap(GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSize) const
{
    const Length& gap = direction == ForColumns ? style().gridColumnGap() : style().gridRowGap();
    return valueForLength(gap, availableSize.value_or(0));
}

LayoutUnit RenderGrid::gridGap(GridTrackSizingDirection direction) const
{
    return gridGap(direction, availableSpaceForGutters(direction));
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
    placeItemsOnGrid(grid, std::nullopt);

    GridTrackSizingAlgorithm algorithm(this, grid);
    computeTrackSizesForIndefiniteSize(algorithm, ForColumns, grid, minLogicalWidth, maxLogicalWidth);

    if (hadExcludedChildren) {
        minLogicalWidth = std::max(minLogicalWidth, childMinWidth);
        maxLogicalWidth = std::max(maxLogicalWidth, childMaxWidth);
    }

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;
}

void RenderGrid::computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm& algorithm, GridTrackSizingDirection direction, Grid& grid, LayoutUnit& minIntrinsicSize, LayoutUnit& maxIntrinsicSize) const
{
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

static std::optional<LayoutUnit> overrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.overrideContainingBlockContentLogicalWidth() : child.overrideContainingBlockContentLogicalHeight();
}

bool RenderGrid::isOrthogonalChild(const RenderBox& child) const
{
    return child.isHorizontalWritingMode() != isHorizontalWritingMode();
}

GridTrackSizingDirection RenderGrid::flowAwareDirectionForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    return !isOrthogonalChild(child) ? direction : (direction == ForColumns ? ForRows : ForColumns);
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

    unsigned repetitions = 1 + (freeSpace / (autoRepeatTracksSize + gapSize)).toInt();

    // Provided the grid container does not have a definite size or max-size in the relevant axis,
    // if the min size is definite then the number of repetitions is the largest possible positive
    // integer that fulfills that minimum requirement.
    if (needsToFulfillMinimumSize)
        ++repetitions;

    return repetitions * autoRepeatTrackListLength;
}


std::unique_ptr<OrderedTrackIndexSet> RenderGrid::computeEmptyTracksForAutoRepeat(Grid& grid, GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == ForColumns;
    if ((isRowAxis && style().gridAutoRepeatColumnsType() != AutoFit)
        || (!isRowAxis && style().gridAutoRepeatRowsType() != AutoFit))
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

// FIXME): We shouldn't have to pass the available logical width as argument. The problem is that
// availableLogicalWidth() does always return a value even if we cannot resolve it like when
// computing the intrinsic size (preferred widths). That's why we pass the responsibility to the
// caller who does know whether the available logical width is indefinite or not.
void RenderGrid::placeItemsOnGrid(Grid& grid, std::optional<LayoutUnit> availableSpace) const
{
    unsigned autoRepeatColumns = computeAutoRepeatTracksCount(ForColumns, availableSpace);
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
    bool hasAnyOrthogonalGridItem = false;
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*child))
            continue;

        hasAnyOrthogonalGridItem = hasAnyOrthogonalGridItem || isOrthogonalChild(*child);

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
    grid.setHasAnyOrthogonalGridItem(hasAnyOrthogonalGridItem);

#if ENABLE(ASSERT)
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

#if ENABLE(ASSERT)
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*child))
            continue;

        GridArea area = grid.gridItemArea(*child);
        ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    }
#endif
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
    LayoutUnit offsetBetweenTracks = isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;

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
    static const StyleContentAlignmentData normalBehavior = {ContentPositionNormal, ContentDistributionStretch};
    return normalBehavior;
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

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        std::optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        std::optional<LayoutUnit> oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForColumns);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForRows);
        if (!oldOverrideContainingBlockContentLogicalWidth || oldOverrideContainingBlockContentLogicalWidth.value() != overrideContainingBlockContentLogicalWidth
            || ((!oldOverrideContainingBlockContentLogicalHeight || oldOverrideContainingBlockContentLogicalHeight.value() != overrideContainingBlockContentLogicalHeight)
                && child->hasRelativeLogicalHeight()))
            child->setNeedsLayout(MarkOnlyThis);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

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
    childLayer->setStaticInlinePosition(borderStart());
    childLayer->setStaticBlockPosition(borderBefore());
}

void RenderGrid::layoutPositionedObject(RenderBox& child, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    LayoutUnit columnOffset;
    LayoutUnit columnBreadth;
    offsetAndBreadthForPositionedChild(child, ForColumns, columnOffset, columnBreadth);
    LayoutUnit rowOffset;
    LayoutUnit rowBreadth;
    offsetAndBreadthForPositionedChild(child, ForRows, rowOffset, rowBreadth);

    child.setOverrideContainingBlockContentLogicalWidth(columnBreadth);
    child.setOverrideContainingBlockContentLogicalHeight(rowBreadth);

    // Mark for layout as we're resetting the position before and we relay in generic layout logic
    // for positioned items in order to get the offsets properly resolved.
    child.setChildNeedsLayout(MarkOnlyThis);

    // FIXME: If possible it'd be nice to avoid this layout here when it's not needed.
    RenderBlock::layoutPositionedObject(child, relayoutChildren, fixedPositionObjectsOnly);

    bool isOrthogonal = isOrthogonalChild(child);
    LayoutUnit logicalLeft = child.logicalLeft() + (isOrthogonal ? rowOffset : columnOffset);
    LayoutUnit logicalTop = child.logicalTop() + (isOrthogonal ? columnOffset : rowOffset);
    child.setLogicalLocation(LayoutPoint(logicalLeft, logicalTop));
}

void RenderGrid::offsetAndBreadthForPositionedChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& offset, LayoutUnit& breadth)
{
    bool isRowAxis = direction == ForColumns;

    unsigned autoRepeatCount = m_grid.autoRepeatTracks(direction);
    GridSpan positions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), child, direction, autoRepeatCount);
    if (positions.isIndefinite()) {
        offset = LayoutUnit();
        breadth = isRowAxis ? clientLogicalWidth() : clientLogicalHeight();
        return;
    }

    // For positioned items we cannot use GridSpan::translate() because we could end up with negative values, as the positioned items do not create implicit tracks per spec.
    int smallestStart = std::abs(m_grid.smallestTrackStart(direction));
    int startLine = positions.untranslatedStartLine() + smallestStart;
    int endLine = positions.untranslatedEndLine() + smallestStart;

    GridPosition startPosition = isRowAxis ? child.style().gridItemColumnStart() : child.style().gridItemRowStart();
    GridPosition endPosition = isRowAxis ? child.style().gridItemColumnEnd() : child.style().gridItemRowEnd();
    int lastLine = numTracks(direction, m_grid);

    bool startIsAuto = startPosition.isAuto()
        || (startPosition.isNamedGridArea() && !NamedLineCollection::isValidNamedLineOrArea(startPosition.namedGridLine(), style(), (direction == ForColumns) ? ColumnStartSide : RowStartSide))
        || (startLine < 0)
        || (startLine > lastLine);
    bool endIsAuto = endPosition.isAuto()
        || (endPosition.isNamedGridArea() && !NamedLineCollection::isValidNamedLineOrArea(endPosition.namedGridLine(), style(), (direction == ForColumns) ? ColumnEndSide : RowEndSide))
        || (endLine < 0)
        || (endLine > lastLine);

    // We're normalizing the positions to avoid issues with RTL (as they're stored in the same order than LTR but adding an offset).
    LayoutUnit start;
    if (!startIsAuto) {
        if (isRowAxis) {
            if (style().isLeftToRightDirection())
                start = m_columnPositions[startLine] - borderLogicalLeft();
            else
                start = logicalWidth() - translateRTLCoordinate(m_columnPositions[startLine]) - borderLogicalRight();
        } else
            start = m_rowPositions[startLine] - borderBefore();
    }

    std::optional<LayoutUnit> availableSizeForGutters = availableSpaceForGutters(direction);
    LayoutUnit end = isRowAxis ? clientLogicalWidth() : clientLogicalHeight();
    if (!endIsAuto) {
        if (isRowAxis) {
            if (style().isLeftToRightDirection())
                end = m_columnPositions[endLine] - borderLogicalLeft();
            else
                end = logicalWidth() - translateRTLCoordinate(m_columnPositions[endLine]) - borderLogicalRight();
        } else
            end = m_rowPositions[endLine] - borderBefore();

        // These vectors store line positions including gaps, but we shouldn't consider them for the edges of the grid.
        if (endLine > 0 && endLine < lastLine) {
            ASSERT(!m_grid.needsItemsPlacement());
            end -= guttersSize(m_grid, direction, endLine - 1, 2, availableSizeForGutters);
            end -= isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
        }
    }

    breadth = end - start;
    offset = start;

    if (isRowAxis && !style().isLeftToRightDirection()) {
        // We always want to calculate the static position from the left (even if we're in RTL).
        if (endIsAuto)
            offset = LayoutUnit();
        else {
            offset = translateRTLCoordinate(m_columnPositions[endLine]) - borderLogicalLeft();

            if (endLine > 0 && endLine < lastLine) {
                ASSERT(!m_grid.needsItemsPlacement());
                offset += guttersSize(m_grid, direction, endLine - 1, 2, availableSizeForGutters);
                offset += isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
            }
        }
    }
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
    ContentAlignmentData offset = computeContentPositionAndDistributionOffset(direction, m_trackSizingAlgorithm.freeSpace(direction).value(), numberOfTracks - numberOfCollapsedTracks);
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    positions.resize(numberOfLines);
    auto borderAndPadding = isRowAxis ? borderAndPaddingLogicalLeft() : borderAndPaddingBefore();
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
    auto& offsetBetweenTracks = isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
    offsetBetweenTracks = offset.distributionOffset;
}

static LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackSize, LayoutUnit childSize)
{
    LayoutUnit offset = trackSize - childSize;
    switch (overflow) {
    case OverflowAlignmentSafe:
        // If overflow is 'safe', we have to make sure we don't overflow the 'start'
        // edge (potentially cause some data loss as the overflow is unreachable).
        return std::max<LayoutUnit>(0, offset);
    case OverflowAlignmentUnsafe:
    case OverflowAlignmentDefault:
        // If we overflow our alignment container and overflow is 'true' (default), we
        // ignore the overflow and just return the value regardless (which may cause data
        // loss as we overflow the 'start' edge).
        return offset;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit RenderGrid::marginLogicalSizeForChild(GridTrackSizingDirection direction, const RenderBox& child) const
{
    return flowAwareDirectionForChild(child, direction) == ForColumns ? child.marginLogicalWidth() : child.marginLogicalHeight();
}

LayoutUnit RenderGrid::computeMarginLogicalSizeForChild(GridTrackSizingDirection direction, const RenderBox& child) const
{
    if (!child.style().hasMargin())
        return 0;

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (direction == ForColumns)
        child.computeInlineDirectionMargins(*this, child.containingBlockLogicalWidthForContentInRegion(nullptr), child.logicalWidth(), marginStart, marginEnd);
    else
        child.computeBlockDirectionMargins(*this, marginStart, marginEnd);

    return marginStart + marginEnd;
}

LayoutUnit RenderGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox& child) const
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // children are laid out, so we can't use the child cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    GridTrackSizingDirection childBlockFlowDirection = flowAwareDirectionForChild(child, ForRows);
    return gridAreaBreadthForChild - (child.needsLayout() ? computeMarginLogicalSizeForChild(childBlockFlowDirection, child) : marginLogicalSizeForChild(childBlockFlowDirection, child));
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
    child.clearOverrideLogicalContentHeight();

    GridTrackSizingDirection childBlockDirection = flowAwareDirectionForChild(child, ForRows);
    bool blockFlowIsColumnAxis = childBlockDirection == ForRows;
    bool allowedToStretchChildBlockSize = blockFlowIsColumnAxis ? allowedToStretchChildAlongColumnAxis(child) : allowedToStretchChildAlongRowAxis(child);
    if (allowedToStretchChildBlockSize) {
        LayoutUnit stretchedLogicalHeight = availableAlignmentSpaceForChildBeforeStretching(overrideContainingBlockContentSizeForChild(child, childBlockDirection).value(), child);
        LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, LayoutUnit(-1));
        child.setOverrideLogicalContentHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
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

bool RenderGrid::isInlineBaselineAlignedChild(const RenderBox& child) const
{
    return alignSelfForChild(child).position() == ItemPositionBaseline && !isOrthogonalChild(child) && !hasAutoMarginsInColumnAxis(child);
}

// FIXME: This logic is shared by RenderFlexibleBox, so it might be refactored somehow.
int RenderGrid::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode mode) const
{
#if ENABLE(ASSERT)
    ASSERT(mode == PositionOnContainingLine);
#else
    UNUSED_PARAM(mode);
#endif
    int baseline = firstLineBaseline().value_or(synthesizedBaselineFromBorderBox(*this, direction));

    int marginSize = direction == HorizontalLine ? verticalMarginExtent() : horizontalMarginExtent();
    return baseline + marginSize;
}

std::optional<int> RenderGrid::firstLineBaseline() const
{
    if (isWritingModeRoot() || !m_grid.hasGridItems())
        return std::nullopt;

    const RenderBox* baselineChild = nullptr;
    // Finding the first grid item in grid order.
    unsigned numColumns = m_grid.numTracks(ForColumns);
    for (size_t column = 0; column < numColumns; column++) {
        for (const auto* child : m_grid.cell(0, column)) {
            // If an item participates in baseline alignment, we select such item.
            if (isInlineBaselineAlignedChild(*child)) {
                // FIXME: self-baseline and content-baseline alignment not implemented yet.
                baselineChild = child;
                break;
            }
            if (!baselineChild)
                baselineChild = child;
        }
    }

    if (!baselineChild)
        return std::nullopt;

    auto baseline = isOrthogonalChild(*baselineChild) ? std::nullopt : baselineChild->firstLineBaseline();
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

    int marginAscent = direction == HorizontalLine ? marginTop() : marginRight();
    return synthesizedBaselineFromBorderBox(*this, direction) + marginAscent;
}

GridAxisPosition RenderGrid::columnAxisPositionForChild(const RenderBox& child) const
{
    bool hasSameWritingMode = child.style().writingMode() == style().writingMode();
    bool childIsLTR = child.style().isLeftToRightDirection();

    switch (alignSelfForChild(child).position()) {
    case ItemPositionSelfStart:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'start' side in the column axis.
        if (isOrthogonalChild(child)) {
            // If orthogonal writing-modes, self-start will be based on the child's inline-axis
            // direction (inline-start), because it's the one parallel to the column axis.
            if (style().isFlippedBlocksWritingMode())
                return childIsLTR ? GridAxisEnd : GridAxisStart;
            return childIsLTR ? GridAxisStart : GridAxisEnd;
        }
        // self-start is based on the child's block-flow direction. That's why we need to check against the grid container's block-flow direction.
        return hasSameWritingMode ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'end' side in the column axis.
        if (isOrthogonalChild(child)) {
            // If orthogonal writing-modes, self-end will be based on the child's inline-axis
            // direction, (inline-end) because it's the one parallel to the column axis.
            if (style().isFlippedBlocksWritingMode())
                return childIsLTR ? GridAxisStart : GridAxisEnd;
            return childIsLTR ? GridAxisEnd : GridAxisStart;
        }
        // self-end is based on the child's block-flow direction. That's why we need to check against the grid container's block-flow direction.
        return hasSameWritingMode ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisStart;
    case ItemPositionRight:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // The alignment axis (column axis) is always orthogonal to the inline axis, hence this value behaves as 'start'.
        return GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (block-start) in the column axis.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (block-end) in the column axis.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPositionAuto:
    case ItemPositionNormal:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

GridAxisPosition RenderGrid::rowAxisPositionForChild(const RenderBox& child) const
{
    bool hasSameDirection = child.style().direction() == style().direction();
    bool gridIsLTR = style().isLeftToRightDirection();

    switch (justifySelfForChild(child).position()) {
    case ItemPositionSelfStart:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'start' side in the row axis.
        if (isOrthogonalChild(child)) {
            // If orthogonal writing-modes, self-start will be based on the child's block-axis
            // direction, because it's the one parallel to the row axis.
            if (child.style().isFlippedBlocksWritingMode())
                return gridIsLTR ? GridAxisEnd : GridAxisStart;
            return gridIsLTR ? GridAxisStart : GridAxisEnd;
        }
        // self-start is based on the child's inline-flow direction. That's why we need to check against the grid container's direction.
        return hasSameDirection ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // FIXME: Should we implement this logic in a generic utility function ?
        // Aligns the alignment subject to be flush with the edge of the alignment container
        // corresponding to the alignment subject's 'end' side in the row axis.
        if (isOrthogonalChild(child)) {
            // If orthogonal writing-modes, self-end will be based on the child's block-axis
            // direction, because it's the one parallel to the row axis.
            if (child.style().isFlippedBlocksWritingMode())
                return gridIsLTR ? GridAxisStart : GridAxisEnd;
            return gridIsLTR ? GridAxisEnd : GridAxisStart;
        }
        // self-end is based on the child's inline-flow direction. That's why we need to check against the grid container's direction.
        return hasSameDirection ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        // Aligns the alignment subject to be flush with the alignment container's 'line-left' edge.
        // We want the physical 'left' side, so we have to take account, container's inline-flow direction.
        return gridIsLTR ? GridAxisStart : GridAxisEnd;
    case ItemPositionRight:
        // Aligns the alignment subject to be flush with the alignment container's 'line-right' edge.
        // We want the physical 'right' side, so we have to take account, container's inline-flow direction.
        return gridIsLTR ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
        // Aligns the alignment subject to be flush with the alignment container's 'start' edge (inline-start) in the row axis.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
        // Aligns the alignment subject to be flush with the alignment container's 'end' edge (inline-end) in the row axis.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPositionAuto:
    case ItemPositionNormal:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

LayoutUnit RenderGrid::columnAxisOffsetForChild(const RenderBox& child) const
{
    const GridSpan& rowsSpan = m_grid.gridItemSpan(child, ForRows);
    unsigned childStartLine = rowsSpan.startLine();
    LayoutUnit startOfRow = m_rowPositions[childStartLine];
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(child);
    if (hasAutoMarginsInColumnAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = columnAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        unsigned childEndLine = rowsSpan.endLine();
        LayoutUnit endOfRow = m_rowPositions[childEndLine];
        // m_rowPositions include distribution offset (because of content alignment) and gutters
        // so we need to subtract them to get the actual end position for a given row
        // (this does not have to be done for the last track as there are no more m_rowPositions after it).
        if (childEndLine < m_rowPositions.size() - 1)
            endOfRow -= gridGap(ForRows) + m_offsetBetweenRows;
        LayoutUnit columnAxisChildSize = isOrthogonalChild(child) ? child.logicalWidth() + child.marginLogicalWidth() : child.logicalHeight() + child.marginLogicalHeight();
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
    const GridSpan& columnsSpan = m_grid.gridItemSpan(child, ForColumns);
    unsigned childStartLine = columnsSpan.startLine();
    LayoutUnit startOfColumn = m_columnPositions[childStartLine];
    LayoutUnit startPosition = startOfColumn + marginStartForChild(child);
    if (hasAutoMarginsInRowAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = rowAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        unsigned childEndLine = columnsSpan.endLine();
        LayoutUnit endOfColumn = m_columnPositions[childEndLine];
        // m_columnPositions include distribution offset (because of content alignment) and gutters
        // so we need to subtract them to get the actual end position for a given column
        // (this does not have to be done for the last track as there are no more m_columnPositions after it).
        if (childEndLine < m_columnPositions.size() - 1)
            endOfColumn -= gridGap(ForColumns) + m_offsetBetweenColumns;
        LayoutUnit rowAxisChildSize = isOrthogonalChild(child) ? child.logicalHeight() + child.marginLogicalHeight() : child.logicalWidth() + child.marginLogicalWidth();
        auto overflow = justifySelfForChild(child).overflow();
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(overflow, endOfColumn - startOfColumn, rowAxisChildSize);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ContentPosition static resolveContentDistributionFallback(ContentDistributionType distribution)
{
    switch (distribution) {
    case ContentDistributionSpaceBetween:
        return ContentPositionStart;
    case ContentDistributionSpaceAround:
        return ContentPositionCenter;
    case ContentDistributionSpaceEvenly:
        return ContentPositionCenter;
    case ContentDistributionStretch:
        return ContentPositionStart;
    case ContentDistributionDefault:
        return ContentPositionNormal;
    }

    ASSERT_NOT_REACHED();
    return ContentPositionNormal;
}

static ContentAlignmentData contentDistributionOffset(const LayoutUnit& availableFreeSpace, ContentPosition& fallbackPosition, ContentDistributionType distribution, unsigned numberOfGridTracks)
{
    if (distribution != ContentDistributionDefault && fallbackPosition == ContentPositionNormal)
        fallbackPosition = resolveContentDistributionFallback(distribution);

    if (availableFreeSpace <= 0)
        return ContentAlignmentData::defaultOffsets();

    LayoutUnit distributionOffset;
    switch (distribution) {
    case ContentDistributionSpaceBetween:
        if (numberOfGridTracks < 2)
            return ContentAlignmentData::defaultOffsets();
        return {0, availableFreeSpace / (numberOfGridTracks - 1)};
    case ContentDistributionSpaceAround:
        if (numberOfGridTracks < 1)
            return ContentAlignmentData::defaultOffsets();
        distributionOffset = availableFreeSpace / numberOfGridTracks;
        return {distributionOffset / 2, distributionOffset};
    case ContentDistributionSpaceEvenly:
        distributionOffset = availableFreeSpace / (numberOfGridTracks + 1);
        return {distributionOffset, distributionOffset};
    case ContentDistributionStretch:
    case ContentDistributionDefault:
        return ContentAlignmentData::defaultOffsets();
    }

    ASSERT_NOT_REACHED();
    return ContentAlignmentData::defaultOffsets();
}

StyleContentAlignmentData RenderGrid::contentAlignment(GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? style().resolvedJustifyContent(contentAlignmentNormalBehaviorGrid()) : style().resolvedAlignContent(contentAlignmentNormalBehaviorGrid());
}

ContentAlignmentData RenderGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    bool isRowAxis = direction == ForColumns;
    auto contentAlignmentData = contentAlignment(direction);
    auto position = contentAlignmentData.position();
    // If <content-distribution> value can't be applied, 'position' will become the associated
    // <content-position> fallback value.
    auto contentAlignment = contentDistributionOffset(availableFreeSpace, position, contentAlignmentData.distribution(), numberOfGridTracks);
    if (contentAlignment.isValid())
        return contentAlignment;

    if (availableFreeSpace <= 0 && contentAlignmentData.overflow() == OverflowAlignmentSafe)
        return {0, 0};

    switch (position) {
    case ContentPositionLeft:
        // The align-content's axis is always orthogonal to the inline-axis.
        return {0, 0};
    case ContentPositionRight:
        if (isRowAxis)
            return {availableFreeSpace, 0};
        // The align-content's axis is always orthogonal to the inline-axis.
        return {0, 0};
    case ContentPositionCenter:
        return {availableFreeSpace / 2, 0};
    case ContentPositionFlexEnd: // Only used in flex layout, for other layout, it's equivalent to 'end'.
    case ContentPositionEnd:
        if (isRowAxis)
            return {style().isLeftToRightDirection() ? availableFreeSpace : LayoutUnit(), LayoutUnit()};
        return {availableFreeSpace, 0};
    case ContentPositionFlexStart: // Only used in flex layout, for other layout, it's equivalent to 'start'.
    case ContentPositionStart:
        if (isRowAxis)
            return {style().isLeftToRightDirection() ? LayoutUnit() : availableFreeSpace, LayoutUnit()};
        return {0, 0};
    case ContentPositionBaseline:
    case ContentPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align.
        // http://webkit.org/b/145566
        if (isRowAxis)
            return {style().isLeftToRightDirection() ? LayoutUnit() : availableFreeSpace, LayoutUnit()};
        return {0, 0};
    case ContentPositionNormal:
        break;
    }

    ASSERT_NOT_REACHED();
    return {0, 0};
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

    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (!style().isLeftToRightDirection())
        rowAxisOffset = translateRTLCoordinate(rowAxisOffset) - (isOrthogonalChild(child) ? child.logicalHeight()  : child.logicalWidth());

    // "In the positioning phase [...] calculations are performed according to the writing mode
    // of the containing block of the box establishing the orthogonal flow." However, the
    // resulting LayoutPoint will be used in 'setLogicalPosition' in order to set the child's
    // logical position, which will only take into account the child's writing-mode.
    LayoutPoint childLocation(rowAxisOffset, columnAxisOffset);
    return isOrthogonalChild(child) ? childLocation.transposedPoint() : childLocation;
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
    if (isRelPositioned())
        return "RenderGrid (relative positioned)";
    return "RenderGrid";
}

} // namespace WebCore

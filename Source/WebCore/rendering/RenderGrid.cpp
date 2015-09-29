/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
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

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridCoordinate.h"
#include "GridResolvedPosition.h"
#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const int infinity = -1;

class GridTrack {
public:
    GridTrack() {}

    const LayoutUnit& baseSize() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_baseSize;
    }

    const LayoutUnit& growthLimit() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_growthLimit;
    }

    void setBaseSize(LayoutUnit baseSize)
    {
        m_baseSize = baseSize;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void setGrowthLimit(LayoutUnit growthLimit)
    {
        m_growthLimit = growthLimit;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    bool growthLimitIsInfinite() const
    {
        return m_growthLimit == infinity;
    }

    bool infiniteGrowthPotential() const
    {
        return growthLimitIsInfinite() || m_infinitelyGrowable;
    }

    const LayoutUnit& growthLimitIfNotInfinite() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return (m_growthLimit == infinity) ? m_baseSize : m_growthLimit;
    }

    const LayoutUnit& plannedSize() const { return m_plannedSize; }

    void setPlannedSize(LayoutUnit plannedSize)
    {
        m_plannedSize = plannedSize;
    }

    LayoutUnit& tempSize() { return m_tempSize; }

    bool infinitelyGrowable() const { return m_infinitelyGrowable; }

    void setInfinitelyGrowable(bool infinitelyGrowable)
    {
        m_infinitelyGrowable = infinitelyGrowable;
    }

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= m_baseSize; }

    void ensureGrowthLimitIsBiggerThanBaseSize()
    {
        if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
            m_growthLimit = m_baseSize;
    }

    LayoutUnit m_baseSize { 0 };
    LayoutUnit m_growthLimit { 0 };
    LayoutUnit m_plannedSize { 0 };
    LayoutUnit m_tempSize { 0 };
    bool m_infinitelyGrowable { false };
};

class RenderGrid::GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const Vector<Vector<Vector<RenderBox*, 1>>>& grid, GridTrackSizingDirection direction, unsigned fixedTrackIndex, unsigned varyingTrackIndex = 0)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? varyingTrackIndex : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : varyingTrackIndex)
        , m_childIndex(0)
    {
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    RenderBox* nextGridItem()
    {
        if (!m_grid.size())
            return 0;

        unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const auto& children = m_grid[m_rowIndex][m_columnIndex];
            if (m_childIndex < children.size())
                return children[m_childIndex++];

            m_childIndex = 0;
        }
        return 0;
    }

    bool isEmptyAreaEnough(unsigned rowSpan, unsigned columnSpan) const
    {
        // Ignore cells outside current grid as we will grow it later if needed.
        unsigned maxRows = std::min<unsigned>(m_rowIndex + rowSpan, m_grid.size());
        unsigned maxColumns = std::min<unsigned>(m_columnIndex + columnSpan, m_grid[0].size());

        // This adds a O(N^2) behavior that shouldn't be a big deal as we expect spanning areas to be small.
        for (unsigned row = m_rowIndex; row < maxRows; ++row) {
            for (unsigned column = m_columnIndex; column < maxColumns; ++column) {
                auto& children = m_grid[row][column];
                if (!children.isEmpty())
                    return false;
            }
        }

        return true;
    }

    std::unique_ptr<GridCoordinate> nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan)
    {
        ASSERT(fixedTrackSpan >= 1 && varyingTrackSpan >= 1);

        if (m_grid.isEmpty())
            return nullptr;

        unsigned rowSpan = (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
        unsigned columnSpan = (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

        unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            if (isEmptyAreaEnough(rowSpan, columnSpan)) {
                std::unique_ptr<GridCoordinate> result = std::make_unique<GridCoordinate>(GridSpan(m_rowIndex, m_rowIndex + rowSpan - 1), GridSpan(m_columnIndex, m_columnIndex + columnSpan - 1));
                // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
                ++varyingTrackIndex;
                return result;
            }
        }
        return nullptr;
    }

private:
    const Vector<Vector<Vector<RenderBox*, 1>>>& m_grid;
    GridTrackSizingDirection m_direction;
    unsigned m_rowIndex;
    unsigned m_columnIndex;
    unsigned m_childIndex;
};

class RenderGrid::GridSizingData {
    WTF_MAKE_NONCOPYABLE(GridSizingData);
public:
    GridSizingData(unsigned gridColumnCount, unsigned gridRowCount)
        : columnTracks(gridColumnCount)
        , rowTracks(gridRowCount)
    {
    }

    Vector<GridTrack> columnTracks;
    Vector<GridTrack> rowTracks;
    Vector<unsigned> contentSizedTracksIndex;

    // Performance optimization: hold onto these Vectors until the end of Layout to avoid repeated malloc / free.
    Vector<GridTrack*> filteredTracks;
    Vector<GridTrack*> growBeyondGrowthLimitsTracks;
    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
};

RenderGrid::RenderGrid(Element& element, Ref<RenderStyle>&& style)
    : RenderBlock(element, WTF::move(style), 0)
    , m_orderIterator(*this)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid()
{
}

static inline bool defaultAlignmentIsStretch(ItemPosition position)
{
    return position == ItemPositionStretch || position == ItemPositionAuto;
}

static inline bool defaultAlignmentChangedToStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return !defaultAlignmentIsStretch(oldStyle.justifyItemsPosition()) && defaultAlignmentIsStretch(newStyle.justifyItemsPosition());
}

static inline bool defaultAlignmentChangedFromStretchInColumnAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return defaultAlignmentIsStretch(oldStyle.alignItemsPosition()) && !defaultAlignmentIsStretch(newStyle.alignItemsPosition());
}

static inline bool selfAlignmentChangedToStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return RenderStyle::resolveJustification(oldStyle, childStyle, ItemPositionStretch) != ItemPositionStretch
        && RenderStyle::resolveJustification(newStyle, childStyle, ItemPositionStretch) == ItemPositionStretch;
}

static inline bool selfAlignmentChangedFromStretchInColumnAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return RenderStyle::resolveAlignment(oldStyle, childStyle, ItemPositionStretch) == ItemPositionStretch
        && RenderStyle::resolveAlignment(newStyle, childStyle, ItemPositionStretch) != ItemPositionStretch;
}

void RenderGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifferenceLayout)
        return;

    const RenderStyle& newStyle = style();
    if (defaultAlignmentChangedToStretchInRowAxis(*oldStyle, newStyle) || defaultAlignmentChangedFromStretchInColumnAxis(*oldStyle, newStyle)) {
        // Grid items that were not previously stretched in row-axis need to be relayed out so we can compute new available space.
        // Grid items that were previously stretching in column-axis need to be relayed out so we can compute new available space.
        // This is only necessary for stretching since other alignment values don't change the size of the box.
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned())
                continue;
            if (selfAlignmentChangedToStretchInRowAxis(*oldStyle, newStyle, child->style()) || selfAlignmentChangedFromStretchInColumnAxis(*oldStyle, newStyle, child->style()))
                child->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

void RenderGrid::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // FIXME: Much of this method is boiler plate that matches RenderBox::layoutBlock and Render*FlexibleBox::layoutBlock.
    // It would be nice to refactor some of the duplicate code.
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    preparePaginationBeforeBlockLayout(relayoutChildren);

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    updateLogicalWidth();

    layoutGridItems();

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    bool wasPopulated = gridWasPopulated();
    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    LayoutUnit availableLogicalSpace = 0;
    const_cast<RenderGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableLogicalSpace);

    for (auto& column : sizingData.columnTracks) {
        LayoutUnit minTrackBreadth = column.baseSize();
        LayoutUnit maxTrackBreadth = column.growthLimit();

        minLogicalWidth += minTrackBreadth;
        maxLogicalWidth += maxTrackBreadth;
    }

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;

    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->clearGrid();
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    LayoutUnit availableLogicalSpace = (direction == ForColumns) ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding);
    computeUsedBreadthOfGridTracks(direction, sizingData, availableLogicalSpace);
}

bool RenderGrid::gridElementIsShrinkToFit()
{
    return isFloatingOrOutOfFlowPositioned();
}

static inline double normalizedFlexFraction(const GridTrack& track, double flexFactor)
{
    return track.baseSize() / std::max<double>(1, flexFactor);
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    const LayoutUnit initialAvailableLogicalSpace = availableLogicalSpace;
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // 1. Initialize per Grid track variables.
    for (unsigned i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.setBaseSize(computeUsedBreadthOfMinLength(direction, minTrackBreadth));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(direction, maxTrackBreadth, track.baseSize()));
        track.setInfinitelyGrowable(false);

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData);

    for (auto& track : tracks) {
        ASSERT(!track.growthLimitIsInfinite());
        availableLogicalSpace -= track.baseSize();
    }

    const bool hasUndefinedRemainingSpace = (direction == ForRows) ? style().logicalHeight().isAuto() : gridElementIsShrinkToFit();

    if (!hasUndefinedRemainingSpace && availableLogicalSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their UsedBreadth up to their MaxBreadth value until availableLogicalSpace is exhausted.
    if (!hasUndefinedRemainingSpace) {
        const unsigned tracksSize = tracks.size();
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (unsigned i = 0; i < tracksSize; ++i) {
            tracksForDistribution[i] = tracks.data() + i;
            tracksForDistribution[i]->setPlannedSize(tracksForDistribution[i]->baseSize());
        }

        distributeSpaceToTracks<MaximizeTracks>(tracksForDistribution, nullptr, availableLogicalSpace);

        for (auto* track : tracksForDistribution)
            track->setBaseSize(track->plannedSize());
    } else {
        for (auto& track : tracks)
            track.setBaseSize(track.growthLimit());
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double flexFraction = 0;
    if (!hasUndefinedRemainingSpace)
        flexFraction = findFlexFactorUnitSize(tracks, GridSpan(0, tracks.size() - 1), direction, initialAvailableLogicalSpace);
    else {
        for (const auto& trackIndex : flexibleSizedTracksIndex)
            flexFraction = std::max(flexFraction, normalizedFlexFraction(tracks[trackIndex], gridTrackSize(direction, trackIndex).maxTrackBreadth().flex()));

        for (unsigned i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (RenderBox* gridItem = iterator.nextGridItem()) {
                const GridCoordinate coordinate = cachedGridCoordinate(*gridItem);
                const GridSpan span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

                // Do not include already processed items.
                if (i > 0 && span.resolvedInitialPosition.toInt() <= flexibleSizedTracksIndex[i - 1])
                    continue;

                flexFraction = std::max(flexFraction, findFlexFactorUnitSize(tracks, span, direction, maxContentForChild(*gridItem, direction, sizingData.columnTracks)));
            }
        }
    }

    for (auto trackIndex : flexibleSizedTracksIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
        GridTrack& track = tracks[trackIndex];
        LayoutUnit baseSize = std::max<LayoutUnit>(track.baseSize(), flexFraction * trackSize.maxTrackBreadth().flex());
        track.setBaseSize(baseSize);
        availableLogicalSpace -= baseSize;
    }
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(GridTrackSizingDirection direction, const GridLength& gridLength) const
{
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return computeUsedBreadthOfSpecifiedLength(direction, trackLength);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(GridTrackSizingDirection direction, const GridLength& gridLength, LayoutUnit usedBreadth) const
{
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified()) {
        LayoutUnit computedBreadth = computeUsedBreadthOfSpecifiedLength(direction, trackLength);
        ASSERT(computedBreadth != infinity);
        return computedBreadth;
    }

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return infinity;
}

LayoutUnit RenderGrid::computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection direction, const Length& trackLength) const
{
    ASSERT(trackLength.isSpecified());
    if (direction == ForColumns)
        return valueForLength(trackLength, contentLogicalWidth());
    return valueForLength(trackLength, computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), Nullopt).valueOr(0));
}

double RenderGrid::computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, GridTrackSizingDirection direction, double flexFactorSum, LayoutUnit leftOverSpace, const Vector<size_t, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible) const
{
    // We want to avoid the effect of flex factors sum below 1 making the factor unit size to grow exponentially.
    double hypotheticalFactorUnitSize = leftOverSpace / std::max<double>(1, flexFactorSum);

    // product of the hypothetical "flex factor unit" and any flexible track's "flex factor" must be grater than such track's "base size".
    bool validFlexFactorUnit = true;
    for (auto index : flexibleTracksIndexes) {
        if (tracksToTreatAsInflexible && tracksToTreatAsInflexible->contains(index))
            continue;
        LayoutUnit baseSize = tracks[index].baseSize();
        double flexFactor = gridTrackSize(direction, index).maxTrackBreadth().flex();
        // treating all such tracks as inflexible.
        if (baseSize > hypotheticalFactorUnitSize * flexFactor) {
            leftOverSpace -= baseSize;
            flexFactorSum -= flexFactor;
            if (!tracksToTreatAsInflexible)
                tracksToTreatAsInflexible = std::unique_ptr<TrackIndexSet>(new TrackIndexSet());
            tracksToTreatAsInflexible->add(index);
            validFlexFactorUnit = false;
        }
    }
    if (!validFlexFactorUnit)
        return computeFlexFactorUnitSize(tracks, direction, flexFactorSum, leftOverSpace, flexibleTracksIndexes, WTF::move(tracksToTreatAsInflexible));
    return hypotheticalFactorUnitSize;
}

double RenderGrid::findFlexFactorUnitSize(const Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit leftOverSpace) const
{
    if (leftOverSpace <= 0)
        return 0;

    double flexFactorSum = 0;
    Vector<size_t, 8> flexibleTracksIndexes;
    for (const auto& resolvedPosition : tracksSpan) {
        size_t trackIndex = resolvedPosition.toInt();
        GridTrackSize trackSize = gridTrackSize(direction, trackIndex);
        if (!trackSize.maxTrackBreadth().isFlex())
            leftOverSpace -= tracks[trackIndex].baseSize();
        else {
            double flexFactor = trackSize.maxTrackBreadth().flex();
            flexibleTracksIndexes.append(trackIndex);
            flexFactorSum += flexFactor;
        }
    }

    // The function is not called if we don't have <flex> grid tracks
    ASSERT(!flexibleTracksIndexes.isEmpty());

    return computeFlexFactorUnitSize(tracks, direction, flexFactorSum, leftOverSpace, flexibleTracksIndexes);
}

bool RenderGrid::hasDefiniteLogicalSize(GridTrackSizingDirection direction) const
{
    return (direction == ForRows) ? hasDefiniteLogicalHeight() : hasDefiniteLogicalWidth();
}

GridTrackSize RenderGrid::gridTrackSize(GridTrackSizingDirection direction, unsigned i) const
{
    bool isForColumns = (direction == ForColumns);
    auto& trackStyles =  isForColumns ? style().gridColumns() : style().gridRows();
    auto& trackSize = (i >= trackStyles.size()) ? (isForColumns ? style().gridAutoColumns() : style().gridAutoRows()) : trackStyles[i];

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();

    if (minTrackBreadth.isPercentage() || maxTrackBreadth.isPercentage()) {
        if (!hasDefiniteLogicalSize(direction)) {
            if (minTrackBreadth.isPercentage())
                minTrackBreadth = Length(MinContent);
            if (maxTrackBreadth.isPercentage())
                maxTrackBreadth = Length(MaxContent);
        }
    }

    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox& child, Vector<GridTrack>& columnTracks)
{
    Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child.hasOverrideContainingBlockLogicalWidth() ? child.overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child.hasRelativeLogicalHeight() || !oldOverrideContainingBlockContentLogicalWidth || oldOverrideContainingBlockContentLogicalWidth.value() != overrideContainingBlockContentLogicalWidth) {
        child.setNeedsLayout(MarkOnlyThis);
        // We need to clear the stretched height to properly compute logical height during layout.
        child.clearOverrideLogicalContentHeight();
    }

    child.setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to Nullopt (no possible resolution).
    if (child.hasRelativeLogicalHeight())
        child.setOverrideContainingBlockContentLogicalHeight(Nullopt);
    child.layoutIfNeeded();
    return child.logicalHeight() + child.marginLogicalHeight();
}

LayoutUnit RenderGrid::minSizeForChild(RenderBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return { };

    const Length& childMinSize = direction == ForColumns ? child.style().logicalMinWidth() : child.style().logicalMinHeight();
    if (childMinSize.isAuto()) {
        // FIXME: Implement intrinsic aspect ratio support (transferred size in specs).
        return minContentForChild(child, direction, columnTracks);
    }

    if (direction == ForColumns)
        return child.computeLogicalWidthInRegionUsing(MinSize, childMinSize, contentLogicalWidth(), this, nullptr);

    return child.computeContentAndScrollbarLogicalHeightUsing(MinSize, childMinSize, child.logicalHeight()).valueOr(0);
}

LayoutUnit RenderGrid::minContentForChild(RenderBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to Nullopt (no possible resolution).
        if (child.hasRelativeLogicalWidth())
            child.setOverrideContainingBlockContentLogicalWidth(Nullopt);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to Nullopt (no possible resolution).
        if (child.hasRelativeLogicalWidth())
            child.setOverrideContainingBlockContentLogicalWidth(Nullopt);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

class GridItemWithSpan {
public:
    GridItemWithSpan(RenderBox& gridItem, GridCoordinate coordinate, GridTrackSizingDirection direction)
        : m_gridItem(gridItem)
        , m_coordinate(coordinate)
    {
        const GridSpan& span = (direction == ForRows) ? coordinate.rows : coordinate.columns;
        m_span = span.resolvedFinalPosition.toInt() - span.resolvedInitialPosition.toInt() + 1;
    }

    RenderBox& gridItem() const { return m_gridItem; }
    GridCoordinate coordinate() const { return m_coordinate; }
#if !ASSERT_DISABLED
    size_t span() const { return m_span; }
#endif

    bool operator<(const GridItemWithSpan other) const
    {
        return m_span < other.m_span;
    }

private:
    std::reference_wrapper<RenderBox> m_gridItem;
    GridCoordinate m_coordinate;
    unsigned m_span;
};

bool RenderGrid::spanningItemCrossesFlexibleSizedTracks(const GridCoordinate& coordinate, GridTrackSizingDirection direction) const
{
    const GridSpan itemSpan = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    for (auto trackPosition : itemSpan) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition.toInt());
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

static inline unsigned integerSpanForDirection(const GridCoordinate& coordinate, GridTrackSizingDirection direction)
{
    return (direction == ForRows) ? coordinate.rows.integerSpan() : coordinate.columns.integerSpan();
}

struct GridItemsSpanGroupRange {
    Vector<GridItemWithSpan>::iterator rangeStart;
    Vector<GridItemWithSpan>::iterator rangeEnd;
};

void RenderGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    sizingData.itemsSortedByIncreasingSpan.shrink(0);
    HashSet<RenderBox*> itemsSet;
    for (auto trackIndex : sizingData.contentSizedTracksIndex) {
        GridIterator iterator(m_grid, direction, trackIndex);
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];

        while (RenderBox* gridItem = iterator.nextGridItem()) {
            if (itemsSet.add(gridItem).isNewEntry) {
                const GridCoordinate& coordinate = cachedGridCoordinate(*gridItem);
                if (integerSpanForDirection(coordinate, direction) == 1)
                    resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, coordinate, *gridItem, track, sizingData.columnTracks);
                else if (!spanningItemCrossesFlexibleSizedTracks(coordinate, direction))
                    sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, coordinate, direction));
            }
        }
    }
    std::sort(sizingData.itemsSortedByIncreasingSpan.begin(), sizingData.itemsSortedByIncreasingSpan.end());

    auto it = sizingData.itemsSortedByIncreasingSpan.begin();
    auto end = sizingData.itemsSortedByIncreasingSpan.end();
    while (it != end) {
        GridItemsSpanGroupRange spanGroupRange = { it, std::upper_bound(it, end, *it) };
        resolveContentBasedTrackSizingFunctionsForItems<ResolveIntrinsicMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveContentBasedMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveMaxContentMinimums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveIntrinsicMaximums>(direction, sizingData, spanGroupRange);
        resolveContentBasedTrackSizingFunctionsForItems<ResolveMaxContentMaximums>(direction, sizingData, spanGroupRange);
        it = spanGroupRange.rangeEnd;
    }

    for (auto trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        if (track.growthLimitIsInfinite())
            track.setGrowthLimit(track.baseSize());
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridCoordinate& coordinate, RenderBox& gridItem, GridTrack& track, Vector<GridTrack>& columnTracks)
{
    const GridResolvedPosition trackPosition = (direction == ForColumns) ? coordinate.columns.resolvedInitialPosition : coordinate.rows.resolvedInitialPosition;
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition.toInt());

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), maxContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasAutoMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minSizeForChild(gridItem, direction, columnTracks)));

    if (trackSize.hasMinContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), maxContentForChild(gridItem, direction, columnTracks)));
}

const LayoutUnit& RenderGrid::trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track, TrackSizeRestriction restriction)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
    case MaximizeTracks:
        return track.baseSize();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        return restriction == AllowInfinity ? track.growthLimit() : track.growthLimitIfNotInfinite();
    }

    ASSERT_NOT_REACHED();
    return track.baseSize();
}

bool RenderGrid::shouldProcessTrackForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return trackSize.hasIntrinsicMinTrackBreadth();
    case ResolveContentBasedMinimums:
        return trackSize.hasMinOrMaxContentMinTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadth();
    case ResolveIntrinsicMaximums:
        return trackSize.hasMinOrMaxContentMaxTrackBreadth();
    case ResolveMaxContentMaximums:
        return trackSize.hasMaxContentOrAutoMaxTrackBreadth();
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool RenderGrid::trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
        return trackSize.hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        return true;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void RenderGrid::markAsInfinitelyGrowableForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        return;
    case ResolveIntrinsicMaximums:
        if (trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity) == infinity  && track.plannedSize() != infinity)
            track.setInfinitelyGrowable(true);
        return;
    case ResolveMaxContentMaximums:
        if (track.infinitelyGrowable())
            track.setInfinitelyGrowable(false);
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

void RenderGrid::updateTrackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        track.setBaseSize(track.plannedSize());
        return;
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        track.setGrowthLimit(track.plannedSize());
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

LayoutUnit RenderGrid::currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, RenderBox& gridItem, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return minSizeForChild(gridItem, direction, columnTracks);
    case ResolveContentBasedMinimums:
    case ResolveIntrinsicMaximums:
        return minContentForChild(gridItem, direction, columnTracks);
    case ResolveMaxContentMinimums:
    case ResolveMaxContentMaximums:
        return maxContentForChild(gridItem, direction, columnTracks);
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

template <RenderGrid::TrackSizeComputationPhase phase>
void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, const GridItemsSpanGroupRange& gridItemsWithSpan)
{
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = tracks[trackIndex];
        track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity));
    }

    for (auto it = gridItemsWithSpan.rangeStart; it != gridItemsWithSpan.rangeEnd; ++it) {
        GridItemWithSpan& gridItemWithSpan = *it;
        ASSERT(gridItemWithSpan.span() > 1);
        const GridCoordinate& coordinate = gridItemWithSpan.coordinate();
        const GridSpan& itemSpan = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

        sizingData.filteredTracks.shrink(0);
        sizingData.growBeyondGrowthLimitsTracks.shrink(0);
        LayoutUnit spanningTracksSize;
        for (auto& trackPosition : itemSpan) {
            const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition.toInt());
            GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackPosition.toInt()] : sizingData.rowTracks[trackPosition.toInt()];
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                continue;

            sizingData.filteredTracks.append(&track);

            if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                sizingData.growBeyondGrowthLimitsTracks.append(&track);
        }

        if (sizingData.filteredTracks.isEmpty())
            continue;

        LayoutUnit extraSpace = currentItemSizeForTrackSizeComputationPhase(phase, gridItemWithSpan.gridItem(), direction, sizingData.columnTracks) - spanningTracksSize;
        extraSpace = std::max<LayoutUnit>(extraSpace, 0);
        auto& tracksToGrowBeyondGrowthLimits = sizingData.growBeyondGrowthLimitsTracks.isEmpty() ? sizingData.filteredTracks : sizingData.growBeyondGrowthLimitsTracks;
        distributeSpaceToTracks<phase>(sizingData.filteredTracks, &tracksToGrowBeyondGrowthLimits, extraSpace);
    }

    for (const auto& trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = tracks[trackIndex];
        markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
        updateTrackSizeForTrackSizeComputationPhase(phase, track);
    }
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    // This check ensures that we respect the irreflexivity property of the strict weak ordering required by std::sort
    // (forall x: NOT x < x).
    if (track1->infiniteGrowthPotential() && track2->infiniteGrowthPotential())
        return false;

    if (track1->infiniteGrowthPotential() || track2->infiniteGrowthPotential())
        return track2->infiniteGrowthPotential();

    return (track1->growthLimit() - track1->baseSize()) < (track2->growthLimit() - track2->baseSize());
}

template <RenderGrid::TrackSizeComputationPhase phase>
void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& availableLogicalSpace)
{
    ASSERT(availableLogicalSpace >= 0);

    for (auto* track : tracks)
        track->tempSize() = trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity);

    if (availableLogicalSpace > 0) {
        std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

        unsigned tracksSize = tracks.size();
        for (unsigned i = 0; i < tracksSize; ++i) {
            GridTrack& track = *tracks[i];
            const LayoutUnit& trackBreadth = trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            bool infiniteGrowthPotential = track.infiniteGrowthPotential();
            LayoutUnit trackGrowthPotential = infiniteGrowthPotential ? track.growthLimit() : track.growthLimit() - trackBreadth;
            // Let's avoid computing availableLogicalSpaceShare as much as possible as it's a hot spot in performance tests.
            if (trackGrowthPotential > 0 || infiniteGrowthPotential) {
                LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
                LayoutUnit growthShare = infiniteGrowthPotential ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, trackGrowthPotential);
                ASSERT_WITH_MESSAGE(growthShare >= 0, "We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function. We can still have 0 as growthShare if the amount of tracks greatly exceeds the availableLogicalSpace.");
                track.tempSize() += growthShare;
                availableLogicalSpace -= growthShare;
            }
        }
    }

    if (availableLogicalSpace > 0 && growBeyondGrowthLimitsTracks) {
        unsigned tracksGrowingBeyondGrowthLimitsSize = growBeyondGrowthLimitsTracks->size();
        for (unsigned i = 0; i < tracksGrowingBeyondGrowthLimitsSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = availableLogicalSpace / (tracksGrowingBeyondGrowthLimitsSize - i);
            track->tempSize() += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    for (auto* track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->tempSize() : std::max(track->plannedSize(), track->tempSize()));
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

void RenderGrid::ensureGridSize(unsigned maximumRowIndex, unsigned maximumColumnIndex)
{
    const unsigned oldRowCount = gridRowCount();
    if (maximumRowIndex >= oldRowCount) {
        m_grid.grow(maximumRowIndex + 1);
        for (unsigned row = oldRowCount; row < gridRowCount(); ++row)
            m_grid[row].grow(gridColumnCount());
    }

    if (maximumColumnIndex >= gridColumnCount()) {
        for (unsigned row = 0; row < gridRowCount(); ++row)
            m_grid[row].grow(maximumColumnIndex + 1);
    }
}

void RenderGrid::insertItemIntoGrid(RenderBox& child, const GridCoordinate& coordinate)
{
    ensureGridSize(coordinate.rows.resolvedFinalPosition.toInt(), coordinate.columns.resolvedFinalPosition.toInt());

    for (auto& row : coordinate.rows) {
        for (auto& column : coordinate.columns)
            m_grid[row.toInt()][column.toInt()].append(&child);
    }
    m_gridItemCoordinate.set(&child, coordinate);
}

void RenderGrid::placeItemsOnGrid()
{
    ASSERT(!gridWasPopulated());
    ASSERT(m_gridItemCoordinate.isEmpty());

    populateExplicitGridAndOrderIterator();

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        auto unresolvedRowPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), *child, ForRows);
        auto unresolvedColumnPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), *child, ForColumns);

        if (unresolvedRowPositions.requiresAutoPlacement() || unresolvedColumnPositions.requiresAutoPlacement()) {

            bool majorAxisDirectionIsForColumns = autoPlacementMajorAxisDirection() == ForColumns;
            if ((majorAxisDirectionIsForColumns && unresolvedColumnPositions.requiresAutoPlacement())
                || (!majorAxisDirectionIsForColumns && unresolvedRowPositions.requiresAutoPlacement()))
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        GridSpan rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedRowPositions, style());
        GridSpan columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedColumnPositions, style());
        insertItemIntoGrid(*child, GridCoordinate(rowPositions, columnPositions));
    }

    ASSERT(gridRowCount() >= GridResolvedPosition::explicitGridRowCount(style()));
    ASSERT(gridColumnCount() >= GridResolvedPosition::explicitGridColumnCount(style()));

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);
    unsigned maximumRowIndex = std::max<unsigned>(1, GridResolvedPosition::explicitGridRowCount(style()));
    unsigned maximumColumnIndex = std::max<unsigned>(1, GridResolvedPosition::explicitGridColumnCount(style()));

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        populator.collectChild(*child);

        auto unresolvedRowPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), *child, ForRows);
        if (!unresolvedRowPositions.requiresAutoPlacement()) {
            GridSpan rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedRowPositions, style());
            maximumRowIndex = std::max(maximumRowIndex, rowPositions.resolvedFinalPosition.next().toInt());
        } else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *child, ForRows, GridResolvedPosition(0));
            maximumRowIndex = std::max(maximumRowIndex, positions.resolvedFinalPosition.next().toInt());
        }

        auto unresolvedColumnPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), *child, ForColumns);
        if (!unresolvedColumnPositions.requiresAutoPlacement()) {
            GridSpan columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedColumnPositions, style());
            maximumColumnIndex = std::max(maximumColumnIndex, columnPositions.resolvedFinalPosition.next().toInt());
        } else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *child, ForColumns, GridResolvedPosition(0));
            maximumColumnIndex = std::max(maximumColumnIndex, positions.resolvedFinalPosition.next().toInt());
        }
    }

    m_grid.grow(maximumRowIndex);
    for (auto& column : m_grid)
        column.grow(maximumColumnIndex);
}

std::unique_ptr<GridCoordinate> RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const unsigned endOfCrossDirection = crossDirection == ForColumns ? gridColumnCount() : gridRowCount();
    GridSpan crossDirectionPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, crossDirection, GridResolvedPosition(endOfCrossDirection));
    return std::make_unique<GridCoordinate>(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions);
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    bool isForColumns = autoPlacementMajorAxisDirection() == ForColumns;
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    // Mapping between the major axis tracks (rows or columns) and the last auto-placed item's position inserted on
    // that track. This is needed to implement "sparse" packing for items locked to a given track.
    // See http://dev.w3.org/csswg/css-grid/#auto-placement-algo
    HashMap<unsigned, unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> minorAxisCursors;

    for (auto& autoGridItem : autoGridItems) {
        auto unresolvedMajorAxisPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), *autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(!unresolvedMajorAxisPositions.requiresAutoPlacement());
        GridSpan majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedMajorAxisPositions, style());
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *autoGridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));
        unsigned majorAxisInitialPosition = majorAxisPositions.resolvedInitialPosition.toInt();

        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions.resolvedInitialPosition.toInt(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisPositions.integerSpan());
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), majorAxisPositions);
        insertItemIntoGrid(*autoGridItem, *emptyGridArea);

        if (!isGridAutoFlowDense)
            minorAxisCursors.set(majorAxisInitialPosition, isForColumns ? emptyGridArea->rows.resolvedInitialPosition.toInt() : emptyGridArea->columns.resolvedInitialPosition.toInt());
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
    ASSERT(GridResolvedPosition::unresolvedSpanFromStyle(style(), gridItem, autoPlacementMajorAxisDirection()).requiresAutoPlacement());
    GridSpan majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, autoPlacementMajorAxisDirection(), GridResolvedPosition(0));

    const unsigned endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    std::unique_ptr<GridCoordinate> emptyGridArea;
    auto unresolvedMinorAxisPositions = GridResolvedPosition::unresolvedSpanFromStyle(style(), gridItem, autoPlacementMinorAxisDirection());
    if (!unresolvedMinorAxisPositions.requiresAutoPlacement()) {
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(unresolvedMinorAxisPositions, style());

        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions.resolvedInitialPosition.toInt() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisPositions.resolvedInitialPosition.toInt(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions.integerSpan(), majorAxisPositions.integerSpan());
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    } else {
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisPositions.integerSpan());

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                GridResolvedPosition minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.resolvedFinalPosition : emptyGridArea->rows.resolvedFinalPosition;
                const unsigned endOfMinorAxis = autoPlacementMinorAxisDirection() == ForColumns ? gridColumnCount() : gridRowCount();
                if (minorAxisFinalPositionIndex.toInt() < endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = nullptr;
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    }

    insertItemIntoGrid(gridItem, *emptyGridArea);
    autoPlacementCursor.first = emptyGridArea->rows.resolvedInitialPosition.toInt();
    autoPlacementCursor.second = emptyGridArea->columns.resolvedInitialPosition.toInt();
}

GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForColumns : ForRows;
}

GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForRows : ForColumns;
}

void RenderGrid::clearGrid()
{
    m_grid.clear();
    m_gridItemCoordinate.clear();
}

void RenderGrid::layoutGridItems()
{
    placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    computeUsedBreadthOfGridTracks(ForColumns, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData.columnTracks));
    computeUsedBreadthOfGridTracks(ForRows, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData.rowTracks));

    populateGridPositions(sizingData);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(*child, ForColumns, sizingData.columnTracks);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChild(*child, ForRows, sizingData.rowTracks);
        if (!oldOverrideContainingBlockContentLogicalWidth || oldOverrideContainingBlockContentLogicalWidth.value() != overrideContainingBlockContentLogicalWidth
            || ((!oldOverrideContainingBlockContentLogicalHeight || oldOverrideContainingBlockContentLogicalHeight.value() != overrideContainingBlockContentLogicalHeight)
                && child->hasRelativeLogicalHeight()))
            child->setNeedsLayout(MarkOnlyThis);
        else
            resetAutoMarginsAndLogicalTopInColumnAxis(*child);

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

    for (auto& row : sizingData.rowTracks)
        setLogicalHeight(logicalHeight() + row.baseSize());

    // min / max logical height is handled in updateLogicalHeight().
    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
    if (hasLineIfEmpty()) {
        LayoutUnit minHeight = borderAndPaddingLogicalHeight()
            + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
            + scrollbarLogicalHeight();
        if (height() < minHeight)
            setLogicalHeight(minHeight);
    }

    clearGrid();
}

GridCoordinate RenderGrid::cachedGridCoordinate(const RenderBox& gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(&gridItem));
    return m_gridItemCoordinate.get(&gridItem);
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    const GridSpan& span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    LayoutUnit gridAreaBreadth = 0;
    for (auto& trackPosition : span)
        gridAreaBreadth += tracks[trackPosition.toInt()].baseSize();
    return gridAreaBreadth;
}

void RenderGrid::populateGridPositions(const GridSizingData& sizingData)
{
    m_columnPositions.resizeToFit(sizingData.columnTracks.size() + 1);
    m_columnPositions[0] = borderAndPaddingStart();
    for (unsigned i = 0; i < m_columnPositions.size() - 1; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + sizingData.columnTracks[i].baseSize();

    m_rowPositions.resizeToFit(sizingData.rowTracks.size() + 1);
    m_rowPositions[0] = borderAndPaddingBefore();
    for (unsigned i = 0; i < m_rowPositions.size() - 1; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + sizingData.rowTracks[i].baseSize();
}

static inline LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackBreadth, LayoutUnit childBreadth)
{
    LayoutUnit offset = trackBreadth - childBreadth;
    switch (overflow) {
    case OverflowAlignmentSafe:
        // If overflow is 'safe', we have to make sure we don't overflow the 'start'
        // edge (potentially cause some data loss as the overflow is unreachable).
        return std::max<LayoutUnit>(0, offset);
    case OverflowAlignmentTrue:
    case OverflowAlignmentDefault:
        // If we overflow our alignment container and overflow is 'true' (default), we
        // ignore the overflow and just return the value regardless (which may cause data
        // loss as we overflow the 'start' edge).
        return offset;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::needToStretchChildLogicalHeight(const RenderBox& child) const
{
    if (RenderStyle::resolveAlignment(style(), child.style(), ItemPositionStretch) != ItemPositionStretch)
        return false;

    return isHorizontalWritingMode() && child.style().height().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
LayoutUnit RenderGrid::marginLogicalHeightForChild(const RenderBox& child) const
{
    return isHorizontalWritingMode() ? child.verticalMarginExtent() : child.horizontalMarginExtent();
}

LayoutUnit RenderGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox& child) const
{
    return gridAreaBreadthForChild - marginLogicalHeightForChild(child);
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::applyStretchAlignmentToChildIfNeeded(RenderBox& child)
{
    ASSERT(child.overrideContainingBlockContentLogicalWidth() && child.overrideContainingBlockContentLogicalHeight());

    // We clear both width and height override values because we will decide now whether they
    // are allowed or not, evaluating the conditions which might have changed since the old
    // values were set.
    child.clearOverrideSize();

    auto& gridStyle = style();
    auto& childStyle = child.style();
    bool isHorizontalMode = isHorizontalWritingMode();
    bool hasAutoSizeInRowAxis = isHorizontalMode ? childStyle.width().isAuto() : childStyle.height().isAuto();
    bool allowedToStretchChildAlongRowAxis = hasAutoSizeInRowAxis && !childStyle.marginStartUsing(&gridStyle).isAuto() && !childStyle.marginEndUsing(&gridStyle).isAuto();
    if (!allowedToStretchChildAlongRowAxis || RenderStyle::resolveJustification(gridStyle, childStyle, ItemPositionStretch) != ItemPositionStretch) {
        bool hasAutoMinSizeInRowAxis = isHorizontalMode ? childStyle.minWidth().isAuto() : childStyle.minHeight().isAuto();
        bool canShrinkToFitInRowAxisForChild = !hasAutoMinSizeInRowAxis || child.minPreferredLogicalWidth() <= child.overrideContainingBlockContentLogicalWidth().value();
        // TODO(lajava): how to handle orthogonality in this case ?.
        // TODO(lajava): grid track sizing and positioning do not support orthogonal modes yet.
        if (hasAutoSizeInRowAxis && canShrinkToFitInRowAxisForChild) {
            LayoutUnit childWidthToFitContent = std::max(std::min(child.maxPreferredLogicalWidth(), child.overrideContainingBlockContentLogicalWidth().value() - child.marginLogicalWidth()), child.minPreferredLogicalWidth());
            LayoutUnit desiredLogicalWidth = child.constrainLogicalHeightByMinMax(childWidthToFitContent, Nullopt);
            child.setOverrideLogicalContentWidth(desiredLogicalWidth - child.borderAndPaddingLogicalWidth());
            if (desiredLogicalWidth != child.logicalWidth())
                child.setNeedsLayout();
        }
    }

    bool hasAutoSizeInColumnAxis = isHorizontalMode ? childStyle.height().isAuto() : childStyle.width().isAuto();
    bool allowedToStretchChildAlongColumnAxis = hasAutoSizeInColumnAxis && !childStyle.marginBeforeUsing(&gridStyle).isAuto() && !childStyle.marginAfterUsing(&gridStyle).isAuto();
    if (allowedToStretchChildAlongColumnAxis && RenderStyle::resolveAlignment(gridStyle, childStyle, ItemPositionStretch) == ItemPositionStretch) {
        // TODO (lajava): If the child has orthogonal flow, then it already has an override height set, so use it.
        // TODO (lajava): grid track sizing and positioning do not support orthogonal modes yet.
        if (child.isHorizontalWritingMode() == isHorizontalMode) {
            LayoutUnit stretchedLogicalHeight = availableAlignmentSpaceForChildBeforeStretching(child.overrideContainingBlockContentLogicalHeight().value(), child);
            LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, Nullopt);
            child.setOverrideLogicalContentHeight(desiredLogicalHeight - child.borderAndPaddingLogicalHeight());
            if (desiredLogicalHeight != child.logicalHeight()) {
                // TODO (lajava): Can avoid laying out here in some cases. See https://webkit.org/b/87905.
                child.setLogicalHeight(0);
                child.setNeedsLayout();
            }
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
void RenderGrid::resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child)
{
    if (hasAutoMarginsInColumnAxis(child) || child.needsLayout()) {
        child.clearOverrideLogicalContentHeight();
        child.updateLogicalHeight();
        if (isHorizontalWritingMode()) {
            if (child.style().marginTop().isAuto())
                child.setMarginTop(0);
            if (child.style().marginBottom().isAuto())
                child.setMarginBottom(0);
        } else {
            if (child.style().marginLeft().isAuto())
                child.setMarginLeft(0);
            if (child.style().marginRight().isAuto())
                child.setMarginRight(0);
        }

    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInRowAxisIfNeeded(RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(child.overrideContainingBlockContentLogicalWidth());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalWidth().value() - child.logicalWidth();
    if (availableAlignmentSpace <= 0)
        return;

    bool isHorizontal = isHorizontalWritingMode();
    Length topOrLeft = isHorizontal ? child.style().marginLeft() : child.style().marginTop();
    Length bottomOrRight = isHorizontal ? child.style().marginRight() : child.style().marginBottom();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        if (isHorizontal) {
            child.setMarginLeft(availableAlignmentSpace / 2);
            child.setMarginRight(availableAlignmentSpace / 2);
        } else {
            child.setMarginTop(availableAlignmentSpace / 2);
            child.setMarginBottom(availableAlignmentSpace / 2);
        }
    } else if (topOrLeft.isAuto()) {
        if (isHorizontal)
            child.setMarginLeft(availableAlignmentSpace);
        else
            child.setMarginTop(availableAlignmentSpace);
    } else if (bottomOrRight.isAuto()) {
        if (isHorizontal)
            child.setMarginRight(availableAlignmentSpace);
        else
            child.setMarginBottom(availableAlignmentSpace);
    }
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::updateAutoMarginsInColumnAxisIfNeeded(RenderBox& child)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(child.overrideContainingBlockContentLogicalHeight());

    LayoutUnit availableAlignmentSpace = child.overrideContainingBlockContentLogicalHeight().value() - child.logicalHeight();
    if (availableAlignmentSpace <= 0)
        return;

    bool isHorizontal = isHorizontalWritingMode();
    Length topOrLeft = isHorizontal ? child.style().marginTop() : child.style().marginLeft();
    Length bottomOrRight = isHorizontal ? child.style().marginBottom() : child.style().marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        if (isHorizontal) {
            child.setMarginTop(availableAlignmentSpace / 2);
            child.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            child.setMarginLeft(availableAlignmentSpace / 2);
            child.setMarginRight(availableAlignmentSpace / 2);
        }
    } else if (topOrLeft.isAuto()) {
        if (isHorizontal)
            child.setMarginTop(availableAlignmentSpace);
        else
            child.setMarginLeft(availableAlignmentSpace);
    } else if (bottomOrRight.isAuto()) {
        if (isHorizontal)
            child.setMarginBottom(availableAlignmentSpace);
        else
            child.setMarginRight(availableAlignmentSpace);
    }
}

GridAxisPosition RenderGrid::columnAxisPositionForChild(const RenderBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    bool hasSameWritingMode = child.style().writingMode() == style().writingMode();

    switch (RenderStyle::resolveAlignment(style(), child.style(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // If orthogonal writing-modes, this computes to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-start is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        return (hasOrthogonalWritingMode || hasSameWritingMode) ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // If orthogonal writing-modes, this computes to 'end'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-end is based on the child's block axis direction. That's why we need to check against the grid container's block flow.
        return (hasOrthogonalWritingMode || hasSameWritingMode) ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode. Otherwise this this is equivalent to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return GridAxisStart;
    case ItemPositionRight:
        // The alignment axis (column axis) and the inline axis are parallell in
        // orthogonal writing mode. Otherwise this this is equivalent to 'start'.
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return hasOrthogonalWritingMode ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

GridAxisPosition RenderGrid::rowAxisPositionForChild(const RenderBox& child) const
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    bool hasSameDirection = child.style().direction() == style().direction();
    bool isLTR = style().isLeftToRightDirection();

    switch (RenderStyle::resolveJustification(style(), child.style(), ItemPositionStretch)) {
    case ItemPositionSelfStart:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        // self-start is based on the child's direction. That's why we need to check against the grid container's direction.
        return (hasOrthogonalWritingMode || hasSameDirection) ? GridAxisStart : GridAxisEnd;
    case ItemPositionSelfEnd:
        // For orthogonal writing-modes, this computes to 'start'
        // FIXME: grid track sizing and positioning do not support orthogonal modes yet.
        return (hasOrthogonalWritingMode || hasSameDirection) ? GridAxisEnd : GridAxisStart;
    case ItemPositionLeft:
        return isLTR ? GridAxisStart : GridAxisEnd;
    case ItemPositionRight:
        return isLTR ? GridAxisEnd : GridAxisStart;
    case ItemPositionCenter:
        return GridAxisCenter;
    case ItemPositionFlexStart: // Only used in flex layout, otherwise equivalent to 'start'.
    case ItemPositionStart:
        return GridAxisStart;
    case ItemPositionFlexEnd: // Only used in flex layout, otherwise equivalent to 'end'.
    case ItemPositionEnd:
        return GridAxisEnd;
    case ItemPositionStretch:
        return GridAxisStart;
    case ItemPositionBaseline:
    case ItemPositionLastBaseline:
        // FIXME: Implement the previous values. For now, we always 'start' align the child.
        return GridAxisStart;
    case ItemPositionAuto:
        break;
    }

    ASSERT_NOT_REACHED();
    return GridAxisStart;
}

LayoutUnit RenderGrid::columnAxisOffsetForChild(const RenderBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfRow = m_rowPositions[coordinate.rows.resolvedInitialPosition.toInt()];
    LayoutUnit startPosition = startOfRow + marginBeforeForChild(child);
    if (hasAutoMarginsInColumnAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = columnAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        LayoutUnit endOfRow = m_rowPositions[coordinate.rows.resolvedFinalPosition.next().toInt()];
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(RenderStyle::resolveAlignmentOverflow(style(), child.style()), endOfRow - startOfRow, child.logicalHeight() + child.marginLogicalHeight());
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}


LayoutUnit RenderGrid::rowAxisOffsetForChild(const RenderBox& child) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    LayoutUnit startOfColumn = m_columnPositions[coordinate.columns.resolvedInitialPosition.toInt()];
    LayoutUnit startPosition = startOfColumn + marginStartForChild(child);
    if (hasAutoMarginsInRowAxis(child))
        return startPosition;
    GridAxisPosition axisPosition = rowAxisPositionForChild(child);
    switch (axisPosition) {
    case GridAxisStart:
        return startPosition;
    case GridAxisEnd:
    case GridAxisCenter: {
        LayoutUnit endOfColumn = m_columnPositions[coordinate.columns.resolvedFinalPosition.next().toInt()];
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(RenderStyle::resolveJustificationOverflow(style(), child.style()), endOfColumn - startOfColumn, child.logicalWidth() + child.marginLogicalWidth());
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint RenderGrid::findChildLogicalPosition(const RenderBox& child) const
{
    LayoutUnit rowAxisOffset = rowAxisOffsetForChild(child);
    // We stored m_columnPositions's data ignoring the direction, hence we might need now
    // to translate positions from RTL to LTR, as it's more convenient for painting.
    if (!style().isLeftToRightDirection()) {
        LayoutUnit alignmentOffset =  m_columnPositions[0] - borderAndPaddingStart();
        LayoutUnit rightGridEdgePosition = m_columnPositions[m_columnPositions.size() - 1] + alignmentOffset + borderAndPaddingLogicalLeft();
        rowAxisOffset = rightGridEdgePosition - (rowAxisOffset + child.logicalWidth());
    }

    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    return LayoutPoint(rowAxisOffset, columnAxisOffsetForChild(child));
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next())
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

#endif /* ENABLE(CSS_GRID_LAYOUT) */

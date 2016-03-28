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

#include "GridArea.h"
#include "GridPositionsResolver.h"
#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <cstdlib>

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

struct ContentAlignmentData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isValid() { return positionOffset >= 0 && distributionOffset >= 0; }
    static ContentAlignmentData defaultOffsets() { return {-1, -1}; }

    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
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

    std::unique_ptr<GridArea> nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan)
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
                std::unique_ptr<GridArea> result = std::make_unique<GridArea>(GridSpan::translatedDefiniteGridSpan(m_rowIndex, m_rowIndex + rowSpan), GridSpan::translatedDefiniteGridSpan(m_columnIndex, m_columnIndex + columnSpan));
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

    Optional<LayoutUnit> freeSpaceForDirection(GridTrackSizingDirection direction) { return direction == ForColumns ? freeSpaceForColumns : freeSpaceForRows; }
    void setFreeSpaceForDirection(GridTrackSizingDirection, Optional<LayoutUnit> freeSpace);

private:
    Optional<LayoutUnit> freeSpaceForColumns;
    Optional<LayoutUnit> freeSpaceForRows;
};

void RenderGrid::GridSizingData::setFreeSpaceForDirection(GridTrackSizingDirection direction, Optional<LayoutUnit> freeSpace)
{
    if (direction == ForColumns)
        freeSpaceForColumns = freeSpace;
    else
        freeSpaceForRows = freeSpace;
}

RenderGrid::RenderGrid(Element& element, Ref<RenderStyle>&& style)
    : RenderBlock(element, WTFMove(style), 0)
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

static inline bool defaultAlignmentChangedFromStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return defaultAlignmentIsStretch(oldStyle.justifyItemsPosition()) && !defaultAlignmentIsStretch(newStyle.justifyItemsPosition());
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

static inline bool selfAlignmentChangedFromStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return RenderStyle::resolveJustification(oldStyle, childStyle, ItemPositionStretch) == ItemPositionStretch
        && RenderStyle::resolveJustification(newStyle, childStyle, ItemPositionStretch) != ItemPositionStretch;
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
    if (defaultAlignmentChangedToStretchInRowAxis(*oldStyle, newStyle) || defaultAlignmentChangedFromStretchInRowAxis(*oldStyle, newStyle)
        || defaultAlignmentChangedFromStretchInColumnAxis(*oldStyle, newStyle)) {
        // Grid items that were not previously stretched in row-axis need to be relayed out so we can compute new available space.
        // Grid items that were previously stretching in column-axis need to be relayed out so we can compute new available space.
        // This is only necessary for stretching since other alignment values don't change the size of the box.
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isOutOfFlowPositioned())
                continue;
            if (selfAlignmentChangedToStretchInRowAxis(*oldStyle, newStyle, child->style()) || selfAlignmentChangedFromStretchInRowAxis(*oldStyle, newStyle, child->style())
                || selfAlignmentChangedFromStretchInColumnAxis(*oldStyle, newStyle, child->style())) {
                child->setChildNeedsLayout(MarkOnlyThis);
            }
        }
    }
}

LayoutUnit RenderGrid::computeTrackBasedLogicalHeight(const GridSizingData& sizingData) const
{
    LayoutUnit logicalHeight;

    for (const auto& row : sizingData.rowTracks)
        logicalHeight += row.baseSize();

    logicalHeight += guttersSize(ForRows, sizingData.rowTracks.size());

    return logicalHeight;
}

void RenderGrid::computeTrackSizesForDirection(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit freeSpace)
{
    LayoutUnit totalGuttersSize = guttersSize(direction, direction == ForRows ? gridRowCount() : gridColumnCount());
    sizingData.setFreeSpaceForDirection(direction, freeSpace - totalGuttersSize);

    LayoutUnit baseSizes, growthLimits;
    computeUsedBreadthOfGridTracks(direction, sizingData, baseSizes, growthLimits);
    ASSERT(tracksAreWiderThanMinTrackBreadth(direction, sizingData));
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

    setLogicalHeight(0);
    updateLogicalWidth();
    bool logicalHeightWasIndefinite = !computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), Nullopt);

    placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());

    // At this point the logical width is always definite as the above call to updateLogicalWidth()
    // properly resolves intrinsic sizes. We cannot do the same for heights though because many code
    // paths inside updateLogicalHeight() require a previous call to setLogicalHeight() to resolve
    // heights properly (like for positioned items for example).
    computeTrackSizesForDirection(ForColumns, sizingData, availableLogicalWidth());

    if (logicalHeightWasIndefinite)
        computeIntrinsicLogicalHeight(sizingData);
    else
        computeTrackSizesForDirection(ForRows, sizingData, availableLogicalHeight(ExcludeMarginBorderPadding));
    setLogicalHeight(computeTrackBasedLogicalHeight(sizingData) + borderAndPaddingLogicalHeight());

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    // The above call might have changed the grid's logical height depending on min|max height restrictions.
    // Update the sizes of the rows whose size depends on the logical height (also on definite|indefinite sizes).
    if (logicalHeightWasIndefinite)
        computeTrackSizesForDirection(ForRows, sizingData, contentLogicalHeight());

    // Grid container should have the minimum height of a line if it's editable. That does not affect track sizing though.
    if (hasLineIfEmpty()) {
        LayoutUnit minHeightForEmptyLine = borderAndPaddingLogicalHeight()
            + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
            + scrollbarLogicalHeight();
        setLogicalHeight(std::max(logicalHeight(), minHeightForEmptyLine));
    }

    applyStretchAlignmentToTracksIfNeeded(ForColumns, sizingData);
    applyStretchAlignmentToTracksIfNeeded(ForRows, sizingData);

    layoutGridItems(sizingData);

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

    clearGrid();

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

LayoutUnit RenderGrid::guttersSize(GridTrackSizingDirection direction, size_t span) const
{
    ASSERT(span >= 1);

    if (span == 1)
        return { };

    const Length& trackGap = direction == ForColumns ? style().gridColumnGap() : style().gridRowGap();
    return valueForLength(trackGap, 0) * (span - 1);
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    bool wasPopulated = gridWasPopulated();
    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    sizingData.setFreeSpaceForDirection(ForColumns, Nullopt);
    const_cast<RenderGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, minLogicalWidth, maxLogicalWidth);

    LayoutUnit totalGuttersSize = guttersSize(ForColumns, sizingData.columnTracks.size());
    minLogicalWidth += totalGuttersSize;
    maxLogicalWidth += totalGuttersSize;

    LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
    minLogicalWidth += scrollbarWidth;
    maxLogicalWidth += scrollbarWidth;

    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->clearGrid();
}

void RenderGrid::computeIntrinsicLogicalHeight(GridSizingData& sizingData)
{
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData));
    sizingData.setFreeSpaceForDirection(ForRows, Nullopt);
    LayoutUnit minHeight, maxHeight;
    computeUsedBreadthOfGridTracks(ForRows, sizingData, minHeight, maxHeight);

    // FIXME: This should be really added to the intrinsic height in RenderBox::computeContentAndScrollbarLogicalHeightUsing().
    // Remove this when that is fixed.
    LayoutUnit scrollbarHeight = scrollbarLogicalHeight();
    minHeight += scrollbarHeight;
    maxHeight += scrollbarHeight;

    LayoutUnit totalGuttersSize = guttersSize(ForRows, gridRowCount());
    minHeight += totalGuttersSize;
    maxHeight += totalGuttersSize;

    m_minContentHeight = minHeight;
    m_maxContentHeight = maxHeight;

    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData));
}

Optional<LayoutUnit> RenderGrid::computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, Optional<LayoutUnit> intrinsicLogicalHeight, LayoutUnit borderAndPadding) const
{
    if (!intrinsicLogicalHeight)
        return Nullopt;

    if (logicalHeightLength.isMinContent())
        return m_minContentHeight;

    if (logicalHeightLength.isMaxContent())
        return m_maxContentHeight;

    if (logicalHeightLength.isFitContent()) {
        LayoutUnit fillAvailableExtent = containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
        return std::min(m_maxContentHeight.valueOr(0), std::max(m_minContentHeight.valueOr(0), fillAvailableExtent));
    }

    if (logicalHeightLength.isFillAvailable())
        return containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding) - borderAndPadding;
    ASSERT_NOT_REACHED();
    return Nullopt;
}

static inline double normalizedFlexFraction(const GridTrack& track, double flexFactor)
{
    return track.baseSize() / std::max<double>(1, flexFactor);
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization)
{
    const Optional<LayoutUnit> initialFreeSpace = sizingData.freeSpaceForDirection(direction);
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    LayoutUnit maxSize = initialFreeSpace.valueOr(0);
    // Grid gutters were removed from freeSpace by the caller (if freeSpace is definite),
    // but we must use them to compute relative (i.e. percentages) sizes.
    if (initialFreeSpace)
        maxSize += guttersSize(direction, direction == ForColumns ? gridColumnCount() : gridRowCount());

    // 1. Initialize per Grid track variables.
    for (unsigned i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.setBaseSize(computeUsedBreadthOfMinLength(minTrackBreadth, maxSize));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(maxTrackBreadth, track.baseSize(), maxSize));
        track.setInfinitelyGrowable(false);

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData);

    baseSizesWithoutMaximization = growthLimitsWithoutMaximization = 0;

    for (auto& track : tracks) {
        ASSERT(!track.growthLimitIsInfinite());
        baseSizesWithoutMaximization += track.baseSize();
        growthLimitsWithoutMaximization += track.growthLimit();
    }
    LayoutUnit freeSpace = initialFreeSpace ? initialFreeSpace.value() - baseSizesWithoutMaximization : LayoutUnit(0);

    const bool hasDefiniteFreeSpace = !!initialFreeSpace;

    if (hasDefiniteFreeSpace && freeSpace <= 0) {
        sizingData.setFreeSpaceForDirection(direction, freeSpace);
        return;
    }

    // 3. Grow all Grid tracks in GridTracks from their UsedBreadth up to their MaxBreadth value until freeSpace is exhausted.
    if (hasDefiniteFreeSpace) {
        const unsigned tracksSize = tracks.size();
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (unsigned i = 0; i < tracksSize; ++i) {
            tracksForDistribution[i] = tracks.data() + i;
            tracksForDistribution[i]->setPlannedSize(tracksForDistribution[i]->baseSize());
        }

        distributeSpaceToTracks<MaximizeTracks>(tracksForDistribution, nullptr, freeSpace);

        for (auto* track : tracksForDistribution)
            track->setBaseSize(track->plannedSize());
    } else {
        for (auto& track : tracks)
            track.setBaseSize(track.growthLimit());
    }

    if (flexibleSizedTracksIndex.isEmpty()) {
        sizingData.setFreeSpaceForDirection(direction, freeSpace);
        return;
    }

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double flexFraction = 0;
    if (hasDefiniteFreeSpace)
        flexFraction = findFlexFactorUnitSize(tracks, GridSpan::translatedDefiniteGridSpan(0, tracks.size()), direction, initialFreeSpace.value());
    else {
        for (const auto& trackIndex : flexibleSizedTracksIndex)
            flexFraction = std::max(flexFraction, normalizedFlexFraction(tracks[trackIndex], gridTrackSize(direction, trackIndex).maxTrackBreadth().flex()));

        for (unsigned i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (RenderBox* gridItem = iterator.nextGridItem()) {
                const GridSpan span = cachedGridSpan(*gridItem, direction);

                // Do not include already processed items.
                if (i > 0 && span.startLine() <= flexibleSizedTracksIndex[i - 1])
                    continue;

                flexFraction = std::max(flexFraction, findFlexFactorUnitSize(tracks, span, direction, maxContentForChild(*gridItem, direction, sizingData.columnTracks)));
            }
        }
    }

    for (auto trackIndex : flexibleSizedTracksIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
        GridTrack& track = tracks[trackIndex];
        LayoutUnit oldBaseSize = track.baseSize();
        LayoutUnit baseSize = std::max<LayoutUnit>(oldBaseSize, flexFraction * trackSize.maxTrackBreadth().flex());
        if (LayoutUnit increment = baseSize - oldBaseSize) {
            track.setBaseSize(baseSize);
            freeSpace -= increment;

            baseSizesWithoutMaximization += increment;
            growthLimitsWithoutMaximization += increment;
        }
    }
    sizingData.setFreeSpaceForDirection(direction, freeSpace);
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(const GridLength& gridLength, LayoutUnit maxSize) const
{
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(const GridLength& gridLength, LayoutUnit usedBreadth, LayoutUnit maxSize) const
{
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return infinity;
}

double RenderGrid::computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, GridTrackSizingDirection direction, double flexFactorSum, LayoutUnit leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible) const
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
        return computeFlexFactorUnitSize(tracks, direction, flexFactorSum, leftOverSpace, flexibleTracksIndexes, WTFMove(tracksToTreatAsInflexible));
    return hypotheticalFactorUnitSize;
}

double RenderGrid::findFlexFactorUnitSize(const Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit leftOverSpace) const
{
    if (leftOverSpace <= 0)
        return 0;

    double flexFactorSum = 0;
    Vector<unsigned, 8> flexibleTracksIndexes;
    for (auto trackIndex : tracksSpan) {
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

    auto& autoTrackSize = isForColumns ? style().gridAutoColumns() : style().gridAutoRows();
    int translatedIndex = i + (isForColumns ? m_smallestColumnStart : m_smallestRowStart);
    auto& trackSize = (translatedIndex < 0 || translatedIndex >= static_cast<int>(trackStyles.size())) ? autoTrackSize : trackStyles[translatedIndex];

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();

    if (minTrackBreadth.isPercentage() || maxTrackBreadth.isPercentage()) {
        if (!hasDefiniteLogicalSize(direction)) {
            if (minTrackBreadth.isPercentage())
                minTrackBreadth = Length(Auto);
            if (maxTrackBreadth.isPercentage())
                maxTrackBreadth = Length(Auto);
        }
    }

    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox& child, Vector<GridTrack>& columnTracks)
{
    Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child.hasOverrideContainingBlockLogicalWidth() ? child.overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child.hasOverrideLogicalContentHeight() || child.hasRelativeLogicalHeight() || !oldOverrideContainingBlockContentLogicalWidth || oldOverrideContainingBlockContentLogicalWidth.value() != overrideContainingBlockContentLogicalWidth)
        child.setNeedsLayout(MarkOnlyThis);

    // We need to clear the stretched height to properly compute logical height during layout.
    if (child.needsLayout())
        child.clearOverrideLogicalContentHeight();

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

    bool isRowAxis = direction == ForColumns;
    const Length& childMinSize = isRowAxis ? child.style().logicalMinWidth() : child.style().logicalMinHeight();
    const Length& childSize = isRowAxis ? child.style().logicalWidth() : child.style().logicalHeight();
    if (!childSize.isAuto() || childMinSize.isAuto())
        return minContentForChild(child, direction, columnTracks);

    if (isRowAxis)
        return child.computeLogicalWidthInRegionUsing(MinSize, childMinSize, contentLogicalWidth(), *this, nullptr);

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
    GridItemWithSpan(RenderBox& gridItem, GridSpan span)
        : m_gridItem(gridItem)
        , m_span(span)
    {
    }

    RenderBox& gridItem() const { return m_gridItem; }
    GridSpan span() const { return m_span; }

    bool operator<(const GridItemWithSpan other) const
    {
        return m_span.integerSpan() < other.m_span.integerSpan();
    }

private:
    std::reference_wrapper<RenderBox> m_gridItem;
    GridSpan m_span;
};

bool RenderGrid::spanningItemCrossesFlexibleSizedTracks(const GridSpan& itemSpan, GridTrackSizingDirection direction) const
{
    for (auto trackPosition : itemSpan) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition);
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
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
                const GridSpan& span = cachedGridSpan(*gridItem, direction);
                if (span.integerSpan() == 1)
                    resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, span, *gridItem, track, sizingData.columnTracks);
                else if (!spanningItemCrossesFlexibleSizedTracks(span, direction))
                    sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
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

void RenderGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridSpan& span, RenderBox& gridItem, GridTrack& track, Vector<GridTrack>& columnTracks)
{
    unsigned trackPosition = span.startLine();
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition);

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
        ASSERT(gridItemWithSpan.span().integerSpan() > 1);
        const GridSpan& itemSpan = gridItemWithSpan.span();

        sizingData.filteredTracks.shrink(0);
        sizingData.growBeyondGrowthLimitsTracks.shrink(0);
        LayoutUnit spanningTracksSize;
        for (auto trackPosition : itemSpan) {
            const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition);
            GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackPosition] : sizingData.rowTracks[trackPosition];
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                continue;

            sizingData.filteredTracks.append(&track);

            if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                sizingData.growBeyondGrowthLimitsTracks.append(&track);
        }

        if (sizingData.filteredTracks.isEmpty())
            continue;

        spanningTracksSize += guttersSize(direction, itemSpan.integerSpan());

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
void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace)
{
    ASSERT(freeSpace >= 0);

    for (auto* track : tracks)
        track->tempSize() = trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity);

    if (freeSpace > 0) {
        std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

        unsigned tracksSize = tracks.size();
        for (unsigned i = 0; i < tracksSize; ++i) {
            GridTrack& track = *tracks[i];
            const LayoutUnit& trackBreadth = trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            bool infiniteGrowthPotential = track.infiniteGrowthPotential();
            LayoutUnit trackGrowthPotential = infiniteGrowthPotential ? track.growthLimit() : track.growthLimit() - trackBreadth;
            // Let's avoid computing availableLogicalSpaceShare as much as possible as it's a hot spot in performance tests.
            if (trackGrowthPotential > 0 || infiniteGrowthPotential) {
                LayoutUnit availableLogicalSpaceShare = freeSpace / (tracksSize - i);
                LayoutUnit growthShare = infiniteGrowthPotential ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, trackGrowthPotential);
                ASSERT_WITH_MESSAGE(growthShare >= 0, "We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function. We can still have 0 as growthShare if the amount of tracks greatly exceeds the freeSpace.");
                track.tempSize() += growthShare;
                freeSpace -= growthShare;
            }
        }
    }

    if (freeSpace > 0 && growBeyondGrowthLimitsTracks) {
        unsigned tracksGrowingBeyondGrowthLimitsSize = growBeyondGrowthLimitsTracks->size();
        for (unsigned i = 0; i < tracksGrowingBeyondGrowthLimitsSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = freeSpace / (tracksGrowingBeyondGrowthLimitsSize - i);
            track->tempSize() += growthShare;
            freeSpace -= growthShare;
        }
    }

    for (auto* track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->tempSize() : std::max(track->plannedSize(), track->tempSize()));
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    const Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    const LayoutUnit maxSize = sizingData.freeSpaceForDirection(direction).valueOr(0);
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(minTrackBreadth, maxSize) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

void RenderGrid::ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize)
{
    const unsigned oldRowCount = gridRowCount();
    if (maximumRowSize > oldRowCount) {
        m_grid.grow(maximumRowSize);
        for (unsigned row = oldRowCount; row < gridRowCount(); ++row)
            m_grid[row].grow(gridColumnCount());
    }

    if (maximumColumnSize > gridColumnCount()) {
        for (unsigned row = 0; row < gridRowCount(); ++row)
            m_grid[row].grow(maximumColumnSize);
    }
}

void RenderGrid::insertItemIntoGrid(RenderBox& child, const GridArea& area)
{
    ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    ensureGridSize(area.rows.endLine(), area.columns.endLine());

    for (auto row : area.rows) {
        for (auto column : area.columns)
            m_grid[row][column].append(&child);
    }
}

void RenderGrid::placeItemsOnGrid()
{
    ASSERT(!gridWasPopulated());
    ASSERT(m_gridItemArea.isEmpty());

    populateExplicitGridAndOrderIterator();

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        GridArea area = cachedGridArea(*child);
        if (!area.rows.isIndefinite())
            area.rows.translate(std::abs(m_smallestRowStart));
        if (!area.columns.isIndefinite())
            area.columns.translate(std::abs(m_smallestColumnStart));
        m_gridItemArea.set(child, area);

        if (area.rows.isIndefinite() || area.columns.isIndefinite()) {
            bool majorAxisDirectionIsForColumns = autoPlacementMajorAxisDirection() == ForColumns;
            if ((majorAxisDirectionIsForColumns && area.columns.isIndefinite())
                || (!majorAxisDirectionIsForColumns && area.rows.isIndefinite()))
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(*child, GridArea(area.rows, area.columns));
    }

    ASSERT(gridRowCount() >= GridPositionsResolver::explicitGridRowCount(style()));
    ASSERT(gridColumnCount() >= GridPositionsResolver::explicitGridColumnCount(style()));

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);

#if ENABLE(ASSERT)
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        GridArea area = cachedGridArea(*child);
        ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    }
#endif
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);
    m_smallestRowStart = m_smallestColumnStart = 0;
    unsigned maximumRowIndex = std::max<unsigned>(1, GridPositionsResolver::explicitGridRowCount(style()));
    unsigned maximumColumnIndex = std::max<unsigned>(1, GridPositionsResolver::explicitGridColumnCount(style()));

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;

        populator.collectChild(*child);

        GridSpan rowPositions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), *child, ForRows);
        if (!rowPositions.isIndefinite()) {
            m_smallestRowStart = std::min(m_smallestRowStart, rowPositions.untranslatedStartLine());
            maximumRowIndex = std::max<int>(maximumRowIndex, rowPositions.untranslatedEndLine());
        } else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *child, ForRows);
            maximumRowIndex = std::max(maximumRowIndex, spanSize);
        }

        GridSpan columnPositions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), *child, ForColumns);
        if (!columnPositions.isIndefinite()) {
            m_smallestColumnStart = std::min(m_smallestColumnStart, columnPositions.untranslatedStartLine());
            maximumColumnIndex = std::max<int>(maximumColumnIndex, columnPositions.untranslatedEndLine());
        } else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            unsigned spanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *child, ForColumns);
            maximumColumnIndex = std::max(maximumColumnIndex, spanSize);
        }

        m_gridItemArea.set(child, GridArea(rowPositions, columnPositions));
    }

    m_grid.grow(maximumRowIndex + std::abs(m_smallestRowStart));
    for (auto& column : m_grid)
        column.grow(maximumColumnIndex + std::abs(m_smallestColumnStart));
}

std::unique_ptr<GridArea> RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const unsigned endOfCrossDirection = crossDirection == ForColumns ? gridColumnCount() : gridRowCount();
    unsigned crossDirectionSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, crossDirection);
    GridSpan crossDirectionPositions = GridSpan::translatedDefiniteGridSpan(endOfCrossDirection, endOfCrossDirection + crossDirectionSpanSize);
    return std::make_unique<GridArea>(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions);
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
        GridSpan majorAxisPositions = cachedGridSpan(*autoGridItem, autoPlacementMajorAxisDirection());
        ASSERT(majorAxisPositions.isTranslatedDefinite());
        ASSERT(cachedGridSpan(*autoGridItem, autoPlacementMinorAxisDirection()).isIndefinite());
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), *autoGridItem, autoPlacementMinorAxisDirection());
        unsigned majorAxisInitialPosition = majorAxisPositions.startLine();

        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions.startLine(), isGridAutoFlowDense ? 0 : minorAxisCursors.get(majorAxisInitialPosition));
        std::unique_ptr<GridArea> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisSpanSize);
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), majorAxisPositions);

        m_gridItemArea.set(autoGridItem, *emptyGridArea);
        insertItemIntoGrid(*autoGridItem, *emptyGridArea);

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
    ASSERT(cachedGridSpan(gridItem, autoPlacementMajorAxisDirection()).isIndefinite());
    unsigned majorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, autoPlacementMajorAxisDirection());

    const unsigned endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    std::unique_ptr<GridArea> emptyGridArea;
    GridSpan minorAxisPositions = cachedGridSpan(gridItem, autoPlacementMinorAxisDirection());
    if (minorAxisPositions.isTranslatedDefinite()) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions.startLine() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisPositions.startLine(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions.integerSpan(), majorAxisSpanSize);
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    } else {
        unsigned minorAxisSpanSize = GridPositionsResolver::spanSizeForAutoPlacedItem(style(), gridItem, autoPlacementMinorAxisDirection());

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisSpanSize, minorAxisSpanSize);

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                unsigned minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.endLine() : emptyGridArea->rows.endLine();
                const unsigned endOfMinorAxis = autoPlacementMinorAxisDirection() == ForColumns ? gridColumnCount() : gridRowCount();
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
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), GridSpan::translatedDefiniteGridSpan(0, minorAxisSpanSize));
    }

    m_gridItemArea.set(&gridItem, *emptyGridArea);
    insertItemIntoGrid(gridItem, *emptyGridArea);
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

void RenderGrid::clearGrid()
{
    m_grid.clear();
    m_gridItemArea.clear();
}

static const StyleContentAlignmentData& normalValueBehaviorGrid()
{
    static const StyleContentAlignmentData normalBehavior = {ContentPositionNormal, ContentDistributionStretch};
    return normalBehavior;
}

void RenderGrid::applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    Optional<LayoutUnit> freeSpace = sizingData.freeSpaceForDirection(direction);
    if (!freeSpace
        || freeSpace.value() <= 0
        || (direction == ForColumns && style().resolvedJustifyContentDistribution(normalValueBehaviorGrid()) != ContentDistributionStretch)
        || (direction == ForRows && style().resolvedAlignContentDistribution(normalValueBehaviorGrid()) != ContentDistributionStretch))
        return;

    // Spec defines auto-sized tracks as the ones with an 'auto' max-sizing function.
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> autoSizedTracksIndex;
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        if (trackSize.hasAutoMaxTrackBreadth())
            autoSizedTracksIndex.append(i);
    }

    unsigned numberOfAutoSizedTracks = autoSizedTracksIndex.size();
    if (numberOfAutoSizedTracks < 1)
        return;

    LayoutUnit sizeToIncrease = freeSpace.value() / numberOfAutoSizedTracks;
    for (const auto& trackIndex : autoSizedTracksIndex) {
        auto& track = tracks[trackIndex];
        track.setBaseSize(track.baseSize() + sizeToIncrease);
    }
    sizingData.setFreeSpaceForDirection(direction, Optional<LayoutUnit>(0));
}

void RenderGrid::layoutGridItems(GridSizingData& sizingData)
{
    populateGridPositions(sizingData);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(*child);
            continue;
        }

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        Optional<LayoutUnit> oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForColumns, sizingData);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChildIncludingAlignmentOffsets(*child, ForRows, sizingData);
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
    childLayer->setStaticInlinePosition(borderAndPaddingStart());
    childLayer->setStaticBlockPosition(borderAndPaddingBefore());
}

void RenderGrid::layoutPositionedObject(RenderBox& child, bool relayoutChildren, bool fixedPositionObjectsOnly)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (!hasOrthogonalWritingMode) {
        LayoutUnit columnOffset = LayoutUnit();
        LayoutUnit columnBreadth = LayoutUnit();
        offsetAndBreadthForPositionedChild(child, ForColumns, columnOffset, columnBreadth);
        LayoutUnit rowOffset = LayoutUnit();
        LayoutUnit rowBreadth = LayoutUnit();
        offsetAndBreadthForPositionedChild(child, ForRows, rowOffset, rowBreadth);

        child.setOverrideContainingBlockContentLogicalWidth(columnBreadth);
        child.setOverrideContainingBlockContentLogicalHeight(rowBreadth);
        child.setExtraInlineOffset(columnOffset);
        child.setExtraBlockOffset(rowOffset);
    }

    RenderBlock::layoutPositionedObject(child, relayoutChildren, fixedPositionObjectsOnly);
}

void RenderGrid::offsetAndBreadthForPositionedChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& offset, LayoutUnit& breadth)
{
    ASSERT(child.isHorizontalWritingMode() == isHorizontalWritingMode());
    bool isRowAxis = direction == ForColumns;

    GridSpan positions = GridPositionsResolver::resolveGridPositionsFromStyle(style(), child, direction);
    if (positions.isIndefinite()) {
        offset = LayoutUnit();
        breadth = isRowAxis ? clientLogicalWidth() : clientLogicalHeight();
        return;
    }

    // For positioned items we cannot use GridSpan::translate() because we could end up with negative values, as the positioned items do not create implicit tracks per spec.
    int smallestStart = std::abs(isRowAxis ? m_smallestColumnStart : m_smallestRowStart);
    int startLine = positions.untranslatedStartLine() + smallestStart;
    int endLine = positions.untranslatedEndLine() + smallestStart;

    GridPosition startPosition = isRowAxis ? child.style().gridItemColumnStart() : child.style().gridItemRowStart();
    GridPosition endPosition = isRowAxis ? child.style().gridItemColumnEnd() : child.style().gridItemRowEnd();
    int firstExplicitLine = smallestStart;
    int lastExplicitLine = (isRowAxis ? GridPositionsResolver::explicitGridColumnCount(style()) : GridPositionsResolver::explicitGridRowCount(style())) + smallestStart;

    bool startIsAuto = startPosition.isAuto()
        || (startPosition.isNamedGridArea() && GridPositionsResolver::isNonExistentNamedLineOrArea(startPosition.namedGridLine(), style(), (direction == ForColumns) ? ColumnStartSide : RowStartSide))
        || (startLine < firstExplicitLine)
        || (startLine > lastExplicitLine);
    bool endIsAuto = endPosition.isAuto()
        || (endPosition.isNamedGridArea() && GridPositionsResolver::isNonExistentNamedLineOrArea(endPosition.namedGridLine(), style(), (direction == ForColumns) ? ColumnEndSide : RowEndSide))
        || (endLine < firstExplicitLine)
        || (endLine > lastExplicitLine);

    LayoutUnit start = startIsAuto ? LayoutUnit() : (isRowAxis ?  m_columnPositions[startLine] : m_rowPositions[startLine]);
    LayoutUnit end = endIsAuto ? (isRowAxis ? logicalWidth() : logicalHeight()) : (isRowAxis ?  m_columnPositions[endLine] : m_rowPositions[endLine]);

    breadth = end - start;

    if (startIsAuto)
        breadth -= isRowAxis ? borderStart() : borderBefore();
    else
        start -= isRowAxis ? borderStart() : borderBefore();

    if (endIsAuto) {
        breadth -= isRowAxis ? borderEnd() : borderAfter();
        breadth -= scrollbarLogicalWidth();
    }

    offset = start;

    if (child.parent() == this && !startIsAuto) {
        // If column/row start is "auto" the static position has been already set in prepareChildForPositionedLayout().
        RenderLayer* childLayer = child.layer();
        if (isRowAxis)
            childLayer->setStaticInlinePosition(borderStart() + offset);
        else
            childLayer->setStaticBlockPosition(borderBefore() + offset);
    }
}

GridArea RenderGrid::cachedGridArea(const RenderBox& gridItem) const
{
    ASSERT(m_gridItemArea.contains(&gridItem));
    return m_gridItemArea.get(&gridItem);
}

GridSpan RenderGrid::cachedGridSpan(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    GridArea area = cachedGridArea(gridItem);
    return direction == ForColumns ? area.columns : area.rows;
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridSpan& span = cachedGridSpan(child, direction);
    LayoutUnit gridAreaBreadth = 0;
    for (auto trackPosition : span)
        gridAreaBreadth += tracks[trackPosition].baseSize();

    gridAreaBreadth += guttersSize(direction, span.integerSpan());

    return gridAreaBreadth;
}

LayoutUnit RenderGrid::gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox& child, GridTrackSizingDirection direction, const GridSizingData& sizingData) const
{
    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const auto& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    const auto& span = cachedGridSpan(child, direction);
    const auto& linePositions = (direction == ForColumns) ? m_columnPositions : m_rowPositions;

    LayoutUnit initialTrackPosition = linePositions[span.startLine()];
    LayoutUnit finalTrackPosition = linePositions[span.endLine() - 1];

    // Track Positions vector stores the 'start' grid line of each track, so we have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.endLine() - 1].baseSize();
}

void RenderGrid::populateGridPositions(GridSizingData& sizingData)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.
    // FIXME: This will affect the computed style value of grid tracks size, since we are
    // using these positions to compute them.

    unsigned numberOfTracks = sizingData.columnTracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;
    unsigned nextToLastLine = numberOfLines - 2;
    ContentAlignmentData offset = computeContentPositionAndDistributionOffset(ForColumns, sizingData.freeSpaceForDirection(ForColumns).value(), numberOfTracks);
    LayoutUnit trackGap = guttersSize(ForColumns, 2);
    m_columnPositions.resize(numberOfLines);
    m_columnPositions[0] = borderAndPaddingStart() + offset.positionOffset;
    for (unsigned i = 0; i < lastLine; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + offset.distributionOffset + sizingData.columnTracks[i].baseSize() + trackGap;
    m_columnPositions[lastLine] = m_columnPositions[nextToLastLine] + sizingData.columnTracks[nextToLastLine].baseSize();

    numberOfTracks = sizingData.rowTracks.size();
    numberOfLines = numberOfTracks + 1;
    lastLine = numberOfLines - 1;
    nextToLastLine = numberOfLines - 2;
    offset = computeContentPositionAndDistributionOffset(ForRows, sizingData.freeSpaceForDirection(ForRows).value(), numberOfTracks);
    trackGap = guttersSize(ForRows, 2);
    m_rowPositions.resize(numberOfLines);
    m_rowPositions[0] = borderAndPaddingBefore() + offset.positionOffset;
    for (unsigned i = 0; i < lastLine; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + offset.distributionOffset + sizingData.rowTracks[i].baseSize() + trackGap;
    m_rowPositions[lastLine] = m_rowPositions[nextToLastLine] + sizingData.rowTracks[nextToLastLine].baseSize();
}

static inline LayoutUnit computeOverflowAlignmentOffset(OverflowAlignment overflow, LayoutUnit trackBreadth, LayoutUnit childBreadth)
{
    LayoutUnit offset = trackBreadth - childBreadth;
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

LayoutUnit RenderGrid::computeMarginLogicalHeightForChild(const RenderBox& child) const
{
    if (!child.style().hasMargin())
        return 0;

    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    child.computeBlockDirectionMargins(*this, marginBefore, marginAfter);

    return marginBefore + marginAfter;
}

LayoutUnit RenderGrid::availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox& child) const
{
    // Because we want to avoid multiple layouts, stretching logic might be performed before
    // children are laid out, so we can't use the child cached values. Hence, we need to
    // compute margins in order to determine the available height before stretching.
    return gridAreaBreadthForChild - (child.needsLayout() ? computeMarginLogicalHeightForChild(child) : marginLogicalHeightForChild(child));
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
void RenderGrid::applyStretchAlignmentToChildIfNeeded(RenderBox& child)
{
    ASSERT(child.overrideContainingBlockContentLogicalHeight());

    // We clear height override values because we will decide now whether it's allowed or
    // not, evaluating the conditions which might have changed since the old values were set.
    child.clearOverrideLogicalContentHeight();

    auto& gridStyle = style();
    auto& childStyle = child.style();
    bool isHorizontalMode = isHorizontalWritingMode();
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

static inline LayoutUnit offsetBetweenTracks(ContentDistributionType distribution, const Vector<LayoutUnit>& trackPositions, const LayoutUnit& childBreadth)
{
    return (distribution == ContentDistributionStretch || ContentDistributionStretch == ContentDistributionDefault) ? LayoutUnit() : trackPositions[1] - trackPositions[0] - childBreadth;
}

LayoutUnit RenderGrid::columnAxisOffsetForChild(const RenderBox& child) const
{
    const GridSpan& rowsSpan = cachedGridSpan(child, ForRows);
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
        // m_rowPositions include gutters so we need to substract them to get the actual end position for a given
        // row (this does not have to be done for the last track as there are no more m_rowPositions after it)
        if (childEndLine < m_rowPositions.size() - 1)
            endOfRow -= guttersSize(ForRows, 2);
        LayoutUnit childBreadth = child.logicalHeight() + child.marginLogicalHeight();
        // In order to properly adjust the Self Alignment values we need to consider the offset between tracks.
        if (childEndLine - childStartLine > 1 && childEndLine < m_rowPositions.size() - 1)
            endOfRow -= offsetBetweenTracks(style().resolvedAlignContentDistribution(normalValueBehaviorGrid()), m_rowPositions, childBreadth);
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(RenderStyle::resolveAlignmentOverflow(style(), child.style()), endOfRow - startOfRow, childBreadth);
        return startPosition + (axisPosition == GridAxisEnd ? offsetFromStartPosition : offsetFromStartPosition / 2);
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}


LayoutUnit RenderGrid::rowAxisOffsetForChild(const RenderBox& child) const
{
    const GridSpan& columnsSpan = cachedGridSpan(child, ForColumns);
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
        // m_columnPositions include gutters so we need to substract them to get the actual end position for a given
        // column (this does not have to be done for the last track as there are no more m_columnPositions after it)
        if (childEndLine < m_columnPositions.size() - 1)
            endOfColumn -= guttersSize(ForColumns, 2);
        LayoutUnit childBreadth = child.logicalWidth() + child.marginLogicalWidth();
        // In order to properly adjust the Self Alignment values we need to consider the offset between tracks.
        if (childEndLine - childStartLine > 1 && childEndLine < m_columnPositions.size() - 1)
            endOfColumn -= offsetBetweenTracks(style().resolvedJustifyContentDistribution(normalValueBehaviorGrid()), m_columnPositions, childBreadth);
        LayoutUnit offsetFromStartPosition = computeOverflowAlignmentOffset(RenderStyle::resolveJustificationOverflow(style(), child.style()), endOfColumn - startOfColumn, childBreadth);
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

ContentAlignmentData RenderGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    bool isRowAxis = direction == ForColumns;
    ContentPosition position = isRowAxis ? style().resolvedJustifyContentPosition(normalValueBehaviorGrid()) : style().resolvedAlignContentPosition(normalValueBehaviorGrid());
    ContentDistributionType distribution = isRowAxis ? style().resolvedJustifyContentDistribution(normalValueBehaviorGrid()) : style().resolvedAlignContentDistribution(normalValueBehaviorGrid());
    // If <content-distribution> value can't be applied, 'position' will become the associated
    // <content-position> fallback value.
    ContentAlignmentData contentAlignment = contentDistributionOffset(availableFreeSpace, position, distribution, numberOfGridTracks);
    if (contentAlignment.isValid())
        return contentAlignment;

    OverflowAlignment overflow = isRowAxis ? style().justifyContentOverflowAlignment() : style().alignContentOverflowAlignment();
    if (availableFreeSpace <= 0 && overflow == OverflowAlignmentSafe)
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

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
static constexpr ItemPosition selfAlignmentNormalBehavior = ItemPositionStretch;

enum TrackSizeRestriction {
    AllowInfinity,
    ForbidInfinity,
};

unsigned RenderGrid::Grid::numTracks(GridTrackSizingDirection direction) const
{
    if (direction == ForRows)
        return m_grid.size();
    return m_grid.size() ? m_grid[0].size() : 0;
}

void RenderGrid::Grid::ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize)
{
    const size_t oldColumnSize = numTracks(ForColumns);
    const size_t oldRowSize = numTracks(ForRows);
    if (maximumRowSize > oldRowSize) {
        m_grid.grow(maximumRowSize);
        for (size_t row = oldRowSize; row < maximumRowSize; ++row)
            m_grid[row].grow(oldColumnSize);
    }

    if (maximumColumnSize > oldColumnSize) {
        for (size_t row = 0; row < numTracks(ForRows); ++row)
            m_grid[row].grow(maximumColumnSize);
    }
}

void RenderGrid::Grid::insert(RenderBox& child, const GridArea& area)
{
    ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    ensureGridSize(area.rows.endLine(), area.columns.endLine());

    for (const auto& row : area.rows) {
        for (const auto& column : area.columns)
            m_grid[row][column].append(&child);
    }

    setGridItemArea(child, area);
}

void RenderGrid::Grid::setSmallestTracksStart(int rowStart, int columnStart)
{
    m_smallestRowStart = rowStart;
    m_smallestColumnStart = columnStart;
}

int RenderGrid::Grid::smallestTrackStart(GridTrackSizingDirection direction) const
{
    return direction == ForRows ? m_smallestRowStart : m_smallestColumnStart;
}

GridArea RenderGrid::Grid::gridItemArea(const RenderBox& item) const
{
    ASSERT(m_gridItemArea.contains(&item));
    return m_gridItemArea.get(&item);
}

void RenderGrid::Grid::setGridItemArea(const RenderBox& item, GridArea area)
{
    m_gridItemArea.set(&item, area);
}

void RenderGrid::Grid::setAutoRepeatTracks(unsigned autoRepeatRows, unsigned autoRepeatColumns)
{
    m_autoRepeatRows = autoRepeatRows;
    m_autoRepeatColumns =  autoRepeatColumns;
}

unsigned RenderGrid::Grid::autoRepeatTracks(GridTrackSizingDirection direction) const
{
    return direction == ForRows ? m_autoRepeatRows : m_autoRepeatColumns;
}

void RenderGrid::Grid::setAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyColumns)
{
    m_autoRepeatEmptyColumns = WTFMove(autoRepeatEmptyColumns);
}

void RenderGrid::Grid::setAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyRows)
{
    m_autoRepeatEmptyRows = WTFMove(autoRepeatEmptyRows);
}

bool RenderGrid::Grid::hasAutoRepeatEmptyTracks(GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? !!m_autoRepeatEmptyColumns : !!m_autoRepeatEmptyRows;
}

bool RenderGrid::Grid::isEmptyAutoRepeatTrack(GridTrackSizingDirection direction, unsigned line) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return autoRepeatEmptyTracks(direction)->contains(line);
}

RenderGrid::OrderedTrackIndexSet* RenderGrid::Grid::autoRepeatEmptyTracks(GridTrackSizingDirection direction) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return direction == ForColumns ? m_autoRepeatEmptyColumns.get() : m_autoRepeatEmptyRows.get();
}

GridSpan RenderGrid::Grid::gridItemSpan(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    GridArea area = gridItemArea(gridItem);
    return direction == ForColumns ? area.columns : area.rows;
}

void RenderGrid::Grid::setNeedsItemsPlacement(bool needsItemsPlacement)
{
    m_needsItemsPlacement = needsItemsPlacement;

    if (needsItemsPlacement)
        clear();
}

void RenderGrid::Grid::clear()
{
    m_grid.resize(0);
    m_gridItemArea.clear();
    m_hasAnyOrthogonalGridItem = false;
    m_smallestRowStart = 0;
    m_smallestColumnStart = 0;
    // FIXME: clear these once m_grid survives layout. We cannot clear them now because they're
    // needed after layout.
    // m_autoRepeatEmptyColumns = nullptr;
    // m_autoRepeatEmptyRows = nullptr;
    // m_autoRepeatColumns = 0;
    // m_autoRepeatRows = 0;
}

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
        ASSERT(!m_growthLimitCap || m_growthLimitCap.value() >= m_growthLimit || m_baseSize >= m_growthLimitCap.value());
        return m_growthLimit;
    }

    void setBaseSize(LayoutUnit baseSize)
    {
        m_baseSize = baseSize;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void setGrowthLimit(LayoutUnit growthLimit)
    {
        m_growthLimit = growthLimit == infinity ? growthLimit : std::min(growthLimit, m_growthLimitCap.value_or(growthLimit));
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    bool infiniteGrowthPotential() const { return growthLimitIsInfinite() || m_infinitelyGrowable; }

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

    const LayoutUnit& tempSize() const { return m_tempSize; }

    void setTempSize(const LayoutUnit& tempSize)
    {
        ASSERT(tempSize >= 0);
        ASSERT(growthLimitIsInfinite() || growthLimit() >= tempSize);
        m_tempSize = tempSize;
    }

    void growTempSize(const LayoutUnit& tempSize)
    {
        ASSERT(tempSize >= 0);
        m_tempSize += tempSize;
    }

    bool infinitelyGrowable() const { return m_infinitelyGrowable; }
    void setInfinitelyGrowable(bool infinitelyGrowable) { m_infinitelyGrowable = infinitelyGrowable; }

    void setGrowthLimitCap(std::optional<LayoutUnit> growthLimitCap)
    {
        ASSERT(!growthLimitCap || growthLimitCap.value() >= 0);
        m_growthLimitCap = growthLimitCap;
    }

    std::optional<LayoutUnit> growthLimitCap() const { return m_growthLimitCap; }

private:
    bool growthLimitIsInfinite() const { return m_growthLimit == infinity; }
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
    std::optional<LayoutUnit> m_growthLimitCap;
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
    GridIterator(const Grid& grid, GridTrackSizingDirection direction, unsigned fixedTrackIndex, unsigned varyingTrackIndex = 0)
        : m_grid(grid.m_grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? varyingTrackIndex : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : varyingTrackIndex)
        , m_childIndex(0)
    {
        ASSERT(!m_grid.isEmpty());
        ASSERT(!m_grid[0].isEmpty());
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    RenderBox* nextGridItem()
    {
        ASSERT(!m_grid.isEmpty());
        ASSERT(!m_grid[0].isEmpty());

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
        ASSERT(!m_grid.isEmpty());
        ASSERT(!m_grid[0].isEmpty());

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
        ASSERT(!m_grid.isEmpty());
        ASSERT(!m_grid[0].isEmpty());
        ASSERT(fixedTrackSpan >= 1);
        ASSERT(varyingTrackSpan >= 1);

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
    const GridAsMatrix& m_grid;
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

    std::optional<LayoutUnit> freeSpace(GridTrackSizingDirection direction) { return direction == ForColumns ? freeSpaceForColumns : freeSpaceForRows; }
    void setFreeSpace(GridTrackSizingDirection, std::optional<LayoutUnit> freeSpace);

    std::optional<LayoutUnit> availableSpace() const { return m_availableSpace; }
    void setAvailableSpace(std::optional<LayoutUnit> availableSpace) { m_availableSpace = availableSpace; }

    SizingOperation sizingOperation { TrackSizing };

    enum SizingState { ColumnSizingFirstIteration, RowSizingFirstIteration, ColumnSizingSecondIteration, RowSizingSecondIteration};
    SizingState sizingState { ColumnSizingFirstIteration };
    void advanceNextState()
    {
        switch (sizingState) {
        case ColumnSizingFirstIteration:
            sizingState = RowSizingFirstIteration;
            return;
        case RowSizingFirstIteration:
            sizingState = ColumnSizingSecondIteration;
            return;
        case ColumnSizingSecondIteration:
            sizingState = RowSizingSecondIteration;
            return;
        case RowSizingSecondIteration:
            sizingState = ColumnSizingFirstIteration;
            return;
        }
        ASSERT_NOT_REACHED();
        sizingState = ColumnSizingFirstIteration;
    }
    bool isValidTransition(GridTrackSizingDirection direction) const
    {
        switch (sizingState) {
        case ColumnSizingFirstIteration:
        case ColumnSizingSecondIteration:
            return direction == ForColumns;
        case RowSizingFirstIteration:
        case RowSizingSecondIteration:
            return direction == ForRows;
        }
        ASSERT_NOT_REACHED();
        return false;
    }

private:
    std::optional<LayoutUnit> freeSpaceForColumns;
    std::optional<LayoutUnit> freeSpaceForRows;
    // No need to store one per direction as it will be only used for computations during each axis
    // track sizing. It's cached here because we need it to compute relative sizes.
    std::optional<LayoutUnit> m_availableSpace;
};

void RenderGrid::GridSizingData::setFreeSpace(GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace)
{
    if (direction == ForColumns)
        freeSpaceForColumns = freeSpace;
    else
        freeSpaceForRows = freeSpace;
}

RenderGrid::RenderGrid(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
    , m_grid(*this)
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
    return !defaultAlignmentIsStretch(oldStyle.justifyItems().position()) && defaultAlignmentIsStretch(newStyle.justifyItems().position());
}

static inline bool defaultAlignmentChangedFromStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return defaultAlignmentIsStretch(oldStyle.justifyItems().position()) && !defaultAlignmentIsStretch(newStyle.justifyItems().position());
}

static inline bool defaultAlignmentChangedFromStretchInColumnAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return defaultAlignmentIsStretch(oldStyle.alignItems().position()) && !defaultAlignmentIsStretch(newStyle.alignItems().position());
}

static inline bool selfAlignmentChangedToStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return childStyle.resolvedJustifySelf(oldStyle, selfAlignmentNormalBehavior).position() != ItemPositionStretch
        && childStyle.resolvedJustifySelf(newStyle, selfAlignmentNormalBehavior).position() == ItemPositionStretch;
}

static inline bool selfAlignmentChangedFromStretchInRowAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return childStyle.resolvedJustifySelf(oldStyle, selfAlignmentNormalBehavior).position() == ItemPositionStretch
        && childStyle.resolvedJustifySelf(newStyle, selfAlignmentNormalBehavior).position() != ItemPositionStretch;
}

static inline bool selfAlignmentChangedFromStretchInColumnAxis(const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderStyle& childStyle)
{
    return childStyle.resolvedAlignSelf(oldStyle, selfAlignmentNormalBehavior).position() == ItemPositionStretch
        && childStyle.resolvedAlignSelf(newStyle, selfAlignmentNormalBehavior).position() != ItemPositionStretch;
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

    logicalHeight += guttersSize(ForRows, 0, sizingData.rowTracks.size());

    return logicalHeight;
}

void RenderGrid::computeTrackSizesForDirection(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit availableSpace)
{
    ASSERT(sizingData.isValidTransition(direction));
    LayoutUnit totalGuttersSize = guttersSize(direction, 0, m_grid.numTracks(direction));
    sizingData.setAvailableSpace(availableSpace);
    sizingData.setFreeSpace(direction, availableSpace - totalGuttersSize);
    sizingData.sizingOperation = TrackSizing;

    LayoutUnit baseSizes, growthLimits;
    computeUsedBreadthOfGridTracks(direction, sizingData, baseSizes, growthLimits);
    ASSERT(tracksAreWiderThanMinTrackBreadth(direction, sizingData));
    sizingData.advanceNextState();
}

void RenderGrid::repeatTracksSizingIfNeeded(GridSizingData& sizingData, LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows)
{
    ASSERT(!m_grid.needsItemsPlacement());
    ASSERT(sizingData.sizingState > GridSizingData::RowSizingFirstIteration);

    // In orthogonal flow cases column track's size is determined by using the computed
    // row track's size, which it was estimated during the first cycle of the sizing
    // algorithm. Hence we need to repeat computeUsedBreadthOfGridTracks for both,
    // columns and rows, to determine the final values.
    // TODO (lajava): orthogonal flows is just one of the cases which may require
    // a new cycle of the sizing algorithm; there may be more. In addition, not all the
    // cases with orthogonal flows require this extra cycle; we need a more specific
    // condition to detect whether child's min-content contribution has changed or not.
    if (m_grid.hasAnyOrthogonalGridItem()) {
        computeTrackSizesForDirection(ForColumns, sizingData, availableSpaceForColumns);
        computeTrackSizesForDirection(ForRows, sizingData, availableSpaceForRows);
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

    placeItemsOnGrid(m_grid, TrackSizing);

    GridSizingData sizingData(numTracks(ForColumns, m_grid), numTracks(ForRows, m_grid));

    // At this point the logical width is always definite as the above call to updateLogicalWidth()
    // properly resolves intrinsic sizes. We cannot do the same for heights though because many code
    // paths inside updateLogicalHeight() require a previous call to setLogicalHeight() to resolve
    // heights properly (like for positioned items for example).
    LayoutUnit availableSpaceForColumns = availableLogicalWidth();
    computeTrackSizesForDirection(ForColumns, sizingData, availableSpaceForColumns);

    // FIXME: We should use RenderBlock::hasDefiniteLogicalHeight() but it does not work for positioned stuff.
    // FIXME: Consider caching the hasDefiniteLogicalHeight value throughout the layout.
    bool hasDefiniteLogicalHeight = hasOverrideLogicalContentHeight() || computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), std::nullopt);
    if (!hasDefiniteLogicalHeight)
        computeIntrinsicLogicalHeight(sizingData);
    else
        computeTrackSizesForDirection(ForRows, sizingData, availableLogicalHeight(ExcludeMarginBorderPadding));
    LayoutUnit trackBasedLogicalHeight = computeTrackBasedLogicalHeight(sizingData) + borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
    setLogicalHeight(trackBasedLogicalHeight);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    // Once grid's indefinite height is resolved, we can compute the
    // available free space for Content Alignment.
    if (!hasDefiniteLogicalHeight)
        sizingData.setFreeSpace(ForRows, logicalHeight() - trackBasedLogicalHeight);

    // 3- If the min-content contribution of any grid items have changed based on the row
    // sizes calculated in step 2, steps 1 and 2 are repeated with the new min-content
    // contribution (once only).
    repeatTracksSizingIfNeeded(sizingData, availableSpaceForColumns, contentLogicalHeight());

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

LayoutUnit RenderGrid::gridGapForDirection(GridTrackSizingDirection direction) const
{
    return valueForLength(direction == ForColumns ? style().gridColumnGap() : style().gridRowGap(), LayoutUnit());
}

LayoutUnit RenderGrid::guttersSize(GridTrackSizingDirection direction, unsigned startLine, unsigned span) const
{
    if (span <= 1)
        return { };

    LayoutUnit gap = gridGapForDirection(direction);

    // Fast path, no collapsing tracks.
    if (!m_grid.hasAutoRepeatEmptyTracks(direction))
        return gap * (span - 1);

    // If there are collapsing tracks we need to be sure that gutters are properly collapsed. Apart
    // from that, if we have a collapsed track in the edges of the span we're considering, we need
    // to move forward (or backwards) in order to know whether the collapsed tracks reach the end of
    // the grid (so the gap becomes 0) or there is a non empty track before that.

    LayoutUnit gapAccumulator;
    unsigned endLine = startLine + span;

    for (unsigned line = startLine; line < endLine - 1; ++line) {
        if (!m_grid.isEmptyAutoRepeatTrack(direction, line))
            gapAccumulator += gap;
    }

    // The above loop adds one extra gap for trailing collapsed tracks.
    if (gapAccumulator && m_grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        ASSERT(gapAccumulator >= gap);
        gapAccumulator -= gap;
    }

    // If the startLine is the start line of a collapsed track we need to go backwards till we reach
    // a non collapsed track. If we find a non collapsed track we need to add that gap.
    if (startLine && m_grid.isEmptyAutoRepeatTrack(direction, startLine)) {
        unsigned nonEmptyTracksBeforeStartLine = startLine;
        auto begin = m_grid.autoRepeatEmptyTracks(direction)->begin();
        for (auto it = begin; *it != startLine; ++it) {
            ASSERT(nonEmptyTracksBeforeStartLine);
            --nonEmptyTracksBeforeStartLine;
        }
        if (nonEmptyTracksBeforeStartLine)
            gapAccumulator += gap;
    }

    // If the endLine is the end line of a collapsed track we need to go forward till we reach a non
    // collapsed track. If we find a non collapsed track we need to add that gap.
    if (m_grid.isEmptyAutoRepeatTrack(direction, endLine - 1)) {
        unsigned nonEmptyTracksAfterEndLine = m_grid.numTracks(direction) - endLine;
        auto currentEmptyTrack = m_grid.autoRepeatEmptyTracks(direction)->find(endLine - 1);
        auto endEmptyTrack = m_grid.autoRepeatEmptyTracks(direction)->end();
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
    bool wasPopulated = !m_grid.needsItemsPlacement();
    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->placeItemsOnGrid(const_cast<Grid&>(m_grid), IntrinsicSizeComputation);

    GridSizingData sizingData(numTracks(ForColumns, m_grid), numTracks(ForRows, m_grid));
    sizingData.setAvailableSpace(std::nullopt);
    sizingData.setFreeSpace(ForColumns, std::nullopt);
    sizingData.sizingOperation = IntrinsicSizeComputation;
    computeUsedBreadthOfGridTracks(ForColumns, sizingData, minLogicalWidth, maxLogicalWidth);

    LayoutUnit totalGuttersSize = guttersSize(ForColumns, 0, sizingData.columnTracks.size());
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
    ASSERT(sizingData.isValidTransition(ForRows));
    sizingData.setAvailableSpace(std::nullopt);
    sizingData.setFreeSpace(ForRows, std::nullopt);
    sizingData.sizingOperation = IntrinsicSizeComputation;
    LayoutUnit minHeight, maxHeight;
    computeUsedBreadthOfGridTracks(ForRows, sizingData, minHeight, maxHeight);

    // FIXME: This should be really added to the intrinsic height in RenderBox::computeContentAndScrollbarLogicalHeightUsing().
    // Remove this when that is fixed.
    LayoutUnit scrollbarHeight = scrollbarLogicalHeight();
    minHeight += scrollbarHeight;
    maxHeight += scrollbarHeight;

    LayoutUnit totalGuttersSize = guttersSize(ForRows, 0, m_grid.numTracks(ForRows));
    minHeight += totalGuttersSize;
    maxHeight += totalGuttersSize;

    m_minContentHeight = minHeight;
    m_maxContentHeight = maxHeight;

    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData));
    sizingData.advanceNextState();
    sizingData.sizingOperation = TrackSizing;
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

static inline double normalizedFlexFraction(const GridTrack& track, double flexFactor)
{
    return track.baseSize() / std::max<double>(1, flexFactor);
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization) const
{
    const std::optional<LayoutUnit> initialFreeSpace = sizingData.freeSpace(direction);
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // Grid gutters were removed from freeSpace by the caller (if freeSpace is definite),
    // but we must use them to compute relative (i.e. percentages) sizes.
    LayoutUnit maxSize = std::max(LayoutUnit(), sizingData.availableSpace().value_or(LayoutUnit()));
    const bool hasDefiniteFreeSpace = sizingData.sizingOperation == TrackSizing;

    // 1. Initialize per Grid track variables.
    for (unsigned i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i, sizingData.sizingOperation);

        track.setBaseSize(computeUsedBreadthOfMinLength(trackSize, maxSize));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(trackSize, track.baseSize(), maxSize));
        track.setInfinitelyGrowable(false);

        if (trackSize.isFitContent()) {
            GridLength gridLength = trackSize.fitContentTrackBreadth();
            if (!gridLength.isPercentage() || hasDefiniteFreeSpace)
                track.setGrowthLimitCap(valueForLength(gridLength.length(), maxSize));
        }
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
        ASSERT(!track.infiniteGrowthPotential());
        baseSizesWithoutMaximization += track.baseSize();
        growthLimitsWithoutMaximization += track.growthLimit();
        // The growth limit caps must be cleared now in order to properly sort tracks by growth
        // potential on an eventual "Maximize Tracks".
        track.setGrowthLimitCap(std::nullopt);
    }
    LayoutUnit freeSpace = initialFreeSpace ? initialFreeSpace.value() - baseSizesWithoutMaximization : LayoutUnit(0);

    if (hasDefiniteFreeSpace && freeSpace <= 0) {
        sizingData.setFreeSpace(direction, freeSpace);
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
        sizingData.setFreeSpace(direction, freeSpace);
        return;
    }

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double flexFraction = 0;
    if (hasDefiniteFreeSpace)
        flexFraction = findFlexFactorUnitSize(tracks, GridSpan::translatedDefiniteGridSpan(0, tracks.size()), direction, sizingData.sizingOperation, initialFreeSpace.value());
    else {
        for (const auto& trackIndex : flexibleSizedTracksIndex)
            flexFraction = std::max(flexFraction, normalizedFlexFraction(tracks[trackIndex], gridTrackSize(direction, trackIndex, sizingData.sizingOperation).maxTrackBreadth().flex()));

        if (m_grid.hasGridItems()) {
            for (unsigned i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
                GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
                while (auto* gridItem = iterator.nextGridItem()) {
                    const GridSpan& span = m_grid.gridItemSpan(*gridItem, direction);

                    // Do not include already processed items.
                    if (i > 0 && span.startLine() <= flexibleSizedTracksIndex[i - 1])
                        continue;

                    flexFraction = std::max(flexFraction, findFlexFactorUnitSize(tracks, span, direction, sizingData.sizingOperation, maxContentForChild(*gridItem, direction, sizingData)));
                }
            }
        }
    }
    LayoutUnit totalGrowth;
    Vector<LayoutUnit> increments;
    increments.grow(flexibleSizedTracksIndex.size());
    computeFlexSizedTracksGrowth(direction, sizingData.sizingOperation, tracks, flexibleSizedTracksIndex, flexFraction, increments, totalGrowth);

    // We only need to redo the flex fraction computation for indefinite heights (definite sizes are
    // already constrained by min/max sizes). Regarding widths, they are always definite at layout
    // time so we shouldn't ever have to do this.
    if (!hasDefiniteFreeSpace && direction == ForRows) {
        auto minSize = computeContentLogicalHeight(MinSize, style().logicalMinHeight(), LayoutUnit(-1));
        auto maxSize = computeContentLogicalHeight(MaxSize, style().logicalMaxHeight(), LayoutUnit(-1));

        // Redo the flex fraction computation using min|max-height as definite available space in
        // case the total height is smaller than min-height or larger than max-height.
        LayoutUnit rowsSize = totalGrowth + computeTrackBasedLogicalHeight(sizingData);
        bool checkMinSize = minSize && rowsSize < minSize.value();
        bool checkMaxSize = maxSize && rowsSize > maxSize.value();
        if (checkMinSize || checkMaxSize) {
            LayoutUnit constrainedFreeSpace = checkMaxSize ? maxSize.value() : LayoutUnit(-1);
            constrainedFreeSpace = std::max(constrainedFreeSpace, minSize.value()) - guttersSize(ForRows, 0, m_grid.numTracks(ForRows));
            flexFraction = findFlexFactorUnitSize(tracks, GridSpan::translatedDefiniteGridSpan(0, tracks.size()), ForRows, sizingData.sizingOperation, constrainedFreeSpace);

            totalGrowth = LayoutUnit(0);
            computeFlexSizedTracksGrowth(ForRows, sizingData.sizingOperation, tracks, flexibleSizedTracksIndex, flexFraction, increments, totalGrowth);
        }
    }

    for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
        if (LayoutUnit increment = increments[i]) {
            auto& track = tracks[flexibleSizedTracksIndex[i]];
            track.setBaseSize(track.baseSize() + increment);
        }
    }
    freeSpace -= totalGrowth;
    growthLimitsWithoutMaximization += totalGrowth;
    sizingData.setFreeSpace(direction, freeSpace);
}

void RenderGrid::computeFlexSizedTracksGrowth(GridTrackSizingDirection direction, SizingOperation sizingOperation, Vector<GridTrack>& tracks, const Vector<unsigned>& flexibleSizedTracksIndex, double flexFraction, Vector<LayoutUnit>& increments, LayoutUnit& totalGrowth) const
{
    size_t numFlexTracks = flexibleSizedTracksIndex.size();
    ASSERT(increments.size() == numFlexTracks);
    for (size_t i = 0; i < numFlexTracks; ++i) {
        unsigned trackIndex = flexibleSizedTracksIndex[i];
        auto trackSize = gridTrackSize(direction, trackIndex, sizingOperation);
        ASSERT(trackSize.maxTrackBreadth().isFlex());
        LayoutUnit oldBaseSize = tracks[trackIndex].baseSize();
        LayoutUnit newBaseSize = std::max(oldBaseSize, LayoutUnit(flexFraction * trackSize.maxTrackBreadth().flex()));
        increments[i] = newBaseSize - oldBaseSize;
        totalGrowth += increments[i];
    }
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(const GridTrackSize& trackSize, LayoutUnit maxSize) const
{
    const GridLength& gridLength = trackSize.minTrackBreadth();
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(const GridTrackSize& trackSize, LayoutUnit usedBreadth, LayoutUnit maxSize) const
{
    const GridLength& gridLength = trackSize.maxTrackBreadth();
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, maxSize);

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return infinity;
}

double RenderGrid::computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, GridTrackSizingDirection direction, SizingOperation sizingOperation, double flexFactorSum, LayoutUnit leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible) const
{
    // We want to avoid the effect of flex factors sum below 1 making the factor unit size to grow exponentially.
    double hypotheticalFactorUnitSize = leftOverSpace / std::max<double>(1, flexFactorSum);

    // product of the hypothetical "flex factor unit" and any flexible track's "flex factor" must be grater than such track's "base size".
    bool validFlexFactorUnit = true;
    for (auto index : flexibleTracksIndexes) {
        if (tracksToTreatAsInflexible && tracksToTreatAsInflexible->contains(index))
            continue;
        LayoutUnit baseSize = tracks[index].baseSize();
        double flexFactor = gridTrackSize(direction, index, sizingOperation).maxTrackBreadth().flex();
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
        return computeFlexFactorUnitSize(tracks, direction, sizingOperation, flexFactorSum, leftOverSpace, flexibleTracksIndexes, WTFMove(tracksToTreatAsInflexible));
    return hypotheticalFactorUnitSize;
}

double RenderGrid::findFlexFactorUnitSize(const Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, SizingOperation sizingOperation, LayoutUnit leftOverSpace) const
{
    if (leftOverSpace <= 0)
        return 0;

    double flexFactorSum = 0;
    Vector<unsigned, 8> flexibleTracksIndexes;
    for (auto trackIndex : tracksSpan) {
        GridTrackSize trackSize = gridTrackSize(direction, trackIndex, sizingOperation);
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

    return computeFlexFactorUnitSize(tracks, direction, sizingOperation, flexFactorSum, leftOverSpace, flexibleTracksIndexes);
}

static bool hasOverrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.hasOverrideContainingBlockLogicalWidth() : child.hasOverrideContainingBlockLogicalHeight();
}

static std::optional<LayoutUnit> overrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.overrideContainingBlockContentLogicalWidth() : child.overrideContainingBlockContentLogicalHeight();
}

static void setOverrideContainingBlockContentSizeForChild(RenderBox& child, GridTrackSizingDirection direction, std::optional<LayoutUnit> size)
{
    if (direction == ForColumns)
        child.setOverrideContainingBlockContentLogicalWidth(size);
    else
        child.setOverrideContainingBlockContentLogicalHeight(size);
}

static bool shouldClearOverrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return child.hasRelativeLogicalWidth() || child.style().logicalWidth().isIntrinsicOrAuto();
    return child.hasRelativeLogicalHeight() || child.style().logicalHeight().isIntrinsicOrAuto();
}

const GridTrackSize& RenderGrid::rawGridTrackSize(GridTrackSizingDirection direction, unsigned translatedIndex) const
{
    bool isRowAxis = direction == ForColumns;
    auto& trackStyles = isRowAxis ? style().gridColumns() : style().gridRows();
    auto& autoRepeatTrackStyles = isRowAxis ? style().gridAutoRepeatColumns() : style().gridAutoRepeatRows();
    auto& autoTrackStyles = isRowAxis ? style().gridAutoColumns() : style().gridAutoRows();
    unsigned insertionPoint = isRowAxis ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned autoRepeatTracksCount = m_grid.autoRepeatTracks(direction);

    // We should not use GridPositionsResolver::explicitGridXXXCount() for this because the
    // explicit grid might be larger than the number of tracks in grid-template-rows|columns (if
    // grid-template-areas is specified for example).
    unsigned explicitTracksCount = trackStyles.size() + autoRepeatTracksCount;

    int untranslatedIndexAsInt = translatedIndex + m_grid.smallestTrackStart(direction);
    unsigned autoTrackStylesSize = autoTrackStyles.size();
    if (untranslatedIndexAsInt < 0) {
        int index = untranslatedIndexAsInt % static_cast<int>(autoTrackStylesSize);
        // We need to traspose the index because the first negative implicit line will get the last defined auto track and so on.
        index += index ? autoTrackStylesSize : 0;
        return autoTrackStyles[index];
    }

    unsigned untranslatedIndex = static_cast<unsigned>(untranslatedIndexAsInt);
    if (untranslatedIndex >= explicitTracksCount)
        return autoTrackStyles[(untranslatedIndex - explicitTracksCount) % autoTrackStylesSize];

    if (!autoRepeatTracksCount || untranslatedIndex < insertionPoint)
        return trackStyles[untranslatedIndex];

    if (untranslatedIndex < (insertionPoint + autoRepeatTracksCount)) {
        unsigned autoRepeatLocalIndex = untranslatedIndexAsInt - insertionPoint;
        return autoRepeatTrackStyles[autoRepeatLocalIndex % autoRepeatTrackStyles.size()];
    }

    return trackStyles[untranslatedIndex - autoRepeatTracksCount];
}

GridTrackSize RenderGrid::gridTrackSize(GridTrackSizingDirection direction, unsigned translatedIndex, SizingOperation sizingOperation) const
{
    // Collapse empty auto repeat tracks if auto-fit.
    if (m_grid.hasAutoRepeatEmptyTracks(direction) && m_grid.isEmptyAutoRepeatTrack(direction, translatedIndex))
        return { Length(Fixed), LengthTrackSizing };

    auto& trackSize = rawGridTrackSize(direction, translatedIndex);
    if (trackSize.isFitContent())
        return trackSize;

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();

    // FIXME: Ensure this condition for determining whether a size is indefinite or not is working correctly for orthogonal flows.
    if (minTrackBreadth.isPercentage() || maxTrackBreadth.isPercentage()) {
        // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto>.
        // For the inline axis this only happens when we're computing the intrinsic sizes (IntrinsicSizeComputation).
        if (sizingOperation == IntrinsicSizeComputation || (direction == ForRows && !hasDefiniteLogicalHeight())) {
            if (minTrackBreadth.isPercentage())
                minTrackBreadth = Length(Auto);
            if (maxTrackBreadth.isPercentage())
                maxTrackBreadth = Length(Auto);
        }
    }

    // Flex sizes are invalid as a min sizing function. However we still can have a flexible |minTrackBreadth|
    // if the track size is just a flex size (e.g. "1fr"), the spec says that in this case it implies an automatic minimum.
    if (minTrackBreadth.isFlex())
        minTrackBreadth = Length(Auto);

    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

bool RenderGrid::isOrthogonalChild(const RenderBox& child) const
{
    return child.isHorizontalWritingMode() != isHorizontalWritingMode();
}

GridTrackSizingDirection RenderGrid::flowAwareDirectionForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    return !isOrthogonalChild(child) ? direction : (direction == ForColumns ? ForRows : ForColumns);
}

LayoutUnit RenderGrid::logicalHeightForChild(RenderBox& child) const
{
    GridTrackSizingDirection childBlockDirection = flowAwareDirectionForChild(child, ForRows);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the block-axis override size to -1 (no possible resolution).
    if (shouldClearOverrideContainingBlockContentSizeForChild(child, ForRows)) {
        setOverrideContainingBlockContentSizeForChild(child, childBlockDirection, std::nullopt);
        child.setNeedsLayout(MarkOnlyThis);
    }

    // We need to clear the stretched height to properly compute logical height during layout.
    if (child.needsLayout())
        child.clearOverrideLogicalContentHeight();

    child.layoutIfNeeded();
    return child.logicalHeight() + child.marginLogicalHeight();
}

LayoutUnit RenderGrid::minSizeForChild(RenderBox& child, GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    GridTrackSizingDirection childInlineDirection = flowAwareDirectionForChild(child, ForColumns);
    bool isRowAxis = direction == childInlineDirection;
    const Length& childMinSize = isRowAxis ? child.style().logicalMinWidth() : child.style().logicalMinHeight();
    const Length& childSize = isRowAxis ? child.style().logicalWidth() : child.style().logicalHeight();
    if (!childSize.isAuto() || childMinSize.isAuto())
        return minContentForChild(child, direction, sizingData);

    bool overrideSizeHasChanged = updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection, sizingData);
    if (isRowAxis) {
        LayoutUnit marginLogicalWidth = sizingData.sizingOperation == TrackSizing ? computeMarginLogicalSizeForChild(childInlineDirection, child) : marginIntrinsicLogicalWidthForChild(child);
        return child.computeLogicalWidthInRegionUsing(MinSize, childMinSize, overrideContainingBlockContentSizeForChild(child, childInlineDirection).value_or(0), *this, nullptr) + marginLogicalWidth;
    }

    if (overrideSizeHasChanged && (direction != ForColumns || sizingData.sizingOperation != IntrinsicSizeComputation))
        child.setNeedsLayout(MarkOnlyThis);
    child.layoutIfNeeded();
    return child.computeLogicalHeightUsing(MinSize, childMinSize, std::nullopt).value_or(0) + child.marginLogicalHeight() + child.scrollbarLogicalHeight();
}

bool RenderGrid::updateOverrideContainingBlockContentSizeForChild(RenderBox& child, GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    LayoutUnit overrideSize = gridAreaBreadthForChild(child, direction, sizingData);
    if (hasOverrideContainingBlockContentSizeForChild(child, direction) && overrideContainingBlockContentSizeForChild(child, direction) == overrideSize)
        return false;

    setOverrideContainingBlockContentSizeForChild(child, direction, overrideSize);
    return true;
}

LayoutUnit RenderGrid::minContentForChild(RenderBox& child, GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    GridTrackSizingDirection childInlineDirection = flowAwareDirectionForChild(child, ForColumns);
    if (direction == childInlineDirection) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the override logical width to std::nullopt (no possible resolution).
        if (shouldClearOverrideContainingBlockContentSizeForChild(child, ForColumns))
            setOverrideContainingBlockContentSizeForChild(child, childInlineDirection, std::nullopt);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    // All orthogonal flow boxes were already laid out during an early layout phase performed in FrameView::performLayout.
    // It's true that grid track sizing was not completed at that time and it may afffect the final height of a
    // grid item, but since it's forbidden to perform a layout during intrinsic width computation, we have to use
    // that computed height for now.
    if (direction == ForColumns && sizingData.sizingOperation == IntrinsicSizeComputation) {
        ASSERT(isOrthogonalChild(child));
        return child.logicalHeight() + child.marginLogicalHeight();
    }

    if (updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection, sizingData))
        child.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForChild(child);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox& child, GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    GridTrackSizingDirection childInlineDirection = flowAwareDirectionForChild(child, ForColumns);
    if (direction == childInlineDirection) {
        // If |child| has a relative logical width, we shouldn't let it override its intrinsic width, which is
        // what we are interested in here. Thus we need to set the inline-axis override size to -1 (no possible resolution).
        if (shouldClearOverrideContainingBlockContentSizeForChild(child, ForColumns))
            setOverrideContainingBlockContentSizeForChild(child, childInlineDirection, std::nullopt);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    // All orthogonal flow boxes were already laid out during an early layout phase performed in FrameView::performLayout.
    // It's true that grid track sizing was not completed at that time and it may afffect the final height of a
    // grid item, but since it's forbidden to perform a layout during intrinsic width computation, we have to use
    // that computed height for now.
    if (direction == ForColumns && sizingData.sizingOperation == IntrinsicSizeComputation) {
        ASSERT(isOrthogonalChild(child));
        return child.logicalHeight() + child.marginLogicalHeight();
    }

    if (updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection, sizingData))
        child.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForChild(child);
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

bool RenderGrid::spanningItemCrossesFlexibleSizedTracks(const GridSpan& itemSpan, GridTrackSizingDirection direction, SizingOperation sizingOperation) const
{
    for (auto trackPosition : itemSpan) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition, sizingOperation);
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

struct GridItemsSpanGroupRange {
    Vector<GridItemWithSpan>::iterator rangeStart;
    Vector<GridItemWithSpan>::iterator rangeEnd;
};

void RenderGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    sizingData.itemsSortedByIncreasingSpan.shrink(0);
    HashSet<RenderBox*> itemsSet;
    if (m_grid.hasGridItems()) {
        for (auto trackIndex : sizingData.contentSizedTracksIndex) {
            GridIterator iterator(m_grid, direction, trackIndex);
            GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];

            while (auto* gridItem = iterator.nextGridItem()) {
                if (itemsSet.add(gridItem).isNewEntry) {
                    const GridSpan& span = m_grid.gridItemSpan(*gridItem, direction);
                    if (span.integerSpan() == 1)
                        resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, span, *gridItem, track, sizingData);
                    else if (!spanningItemCrossesFlexibleSizedTracks(span, direction, sizingData.sizingOperation))
                        sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
                }
            }
        }
        std::sort(sizingData.itemsSortedByIncreasingSpan.begin(), sizingData.itemsSortedByIncreasingSpan.end());
    }

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
        if (track.growthLimit() == infinity)
            track.setGrowthLimit(track.baseSize());
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridSpan& span, RenderBox& gridItem, GridTrack& track, GridSizingData& sizingData) const
{
    unsigned trackPosition = span.startLine();
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition, sizingData.sizingOperation);

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minContentForChild(gridItem, direction, sizingData)));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), maxContentForChild(gridItem, direction, sizingData)));
    else if (trackSize.hasAutoMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minSizeForChild(gridItem, direction, sizingData)));

    if (trackSize.hasMinContentMaxTrackBreadth()) {
        track.setGrowthLimit(std::max(track.growthLimit(), minContentForChild(gridItem, direction, sizingData)));
    } else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth()) {
        LayoutUnit growthLimit = maxContentForChild(gridItem, direction, sizingData);
        if (trackSize.isFitContent())
            growthLimit = std::min(growthLimit, valueForLength(trackSize.fitContentTrackBreadth().length(), sizingData.availableSpace().value_or(0)));
        track.setGrowthLimit(std::max(track.growthLimit(), growthLimit));
    }
}

static LayoutUnit trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track, TrackSizeRestriction restriction)
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
        return trackSize.hasIntrinsicMaxTrackBreadth();
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

LayoutUnit RenderGrid::currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, RenderBox& gridItem, GridTrackSizingDirection direction, GridSizingData& sizingData) const
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveIntrinsicMaximums:
        return minSizeForChild(gridItem, direction, sizingData);
    case ResolveContentBasedMinimums:
        return minContentForChild(gridItem, direction, sizingData);
    case ResolveMaxContentMinimums:
    case ResolveMaxContentMaximums:
        return maxContentForChild(gridItem, direction, sizingData);
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

template <TrackSizeComputationPhase phase>
void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, const GridItemsSpanGroupRange& gridItemsWithSpan) const
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
            const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition, sizingData.sizingOperation);
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

        spanningTracksSize += guttersSize(direction, itemSpan.startLine(), itemSpan.integerSpan());

        LayoutUnit extraSpace = currentItemSizeForTrackSizeComputationPhase(phase, gridItemWithSpan.gridItem(), direction, sizingData) - spanningTracksSize;
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
    bool track1HasInfiniteGrowthPotentialWithoutCap = track1->infiniteGrowthPotential() && !track1->growthLimitCap();
    bool track2HasInfiniteGrowthPotentialWithoutCap = track2->infiniteGrowthPotential() && !track2->growthLimitCap();

    if (track1HasInfiniteGrowthPotentialWithoutCap && track2HasInfiniteGrowthPotentialWithoutCap)
        return false;

    if (track1HasInfiniteGrowthPotentialWithoutCap || track2HasInfiniteGrowthPotentialWithoutCap)
        return track2HasInfiniteGrowthPotentialWithoutCap;

    LayoutUnit track1Limit = track1->growthLimitCap().value_or(track1->growthLimit());
    LayoutUnit track2Limit = track2->growthLimitCap().value_or(track2->growthLimit());
    return (track1Limit - track1->baseSize()) < (track2Limit - track2->baseSize());
}

static void clampGrowthShareIfNeeded(TrackSizeComputationPhase phase, const GridTrack& track, LayoutUnit& growthShare)
{
    if (phase != ResolveMaxContentMaximums || !track.growthLimitCap())
        return;

    LayoutUnit distanceToCap = track.growthLimitCap().value() - track.tempSize();
    if (distanceToCap <= 0)
        return;

    growthShare = std::min(growthShare, distanceToCap);
}

template <TrackSizeComputationPhase phase>
void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace) const
{
    ASSERT(freeSpace >= 0);

    for (auto* track : tracks)
        track->setTempSize(trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity));

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
                clampGrowthShareIfNeeded(phase, track, growthShare);
                ASSERT_WITH_MESSAGE(growthShare >= 0, "We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function. We can still have 0 as growthShare if the amount of tracks greatly exceeds the freeSpace.");
                track.growTempSize(growthShare);
                freeSpace -= growthShare;
            }
        }
    }

    if (freeSpace > 0 && growBeyondGrowthLimitsTracks) {
        // We need to sort them because there might be tracks with growth limit caps (like the ones
        // with fit-content()) which cannot indefinitely grow over the limits.
        if (phase == ResolveMaxContentMaximums)
            std::sort(growBeyondGrowthLimitsTracks->begin(), growBeyondGrowthLimitsTracks->end(), sortByGridTrackGrowthPotential);

        unsigned tracksGrowingBeyondGrowthLimitsSize = growBeyondGrowthLimitsTracks->size();
        for (unsigned i = 0; i < tracksGrowingBeyondGrowthLimitsSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = freeSpace / (tracksGrowingBeyondGrowthLimitsSize - i);
            clampGrowthShareIfNeeded(phase, *track, growthShare);
            track->growTempSize(growthShare);
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
    const LayoutUnit maxSize = sizingData.availableSpace().value_or(0);
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i, sizingData.sizingOperation);
        if (computeUsedBreadthOfMinLength(trackSize, maxSize) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

unsigned RenderGrid::computeAutoRepeatTracksCount(GridTrackSizingDirection direction, SizingOperation sizingOperation) const
{
    bool isRowAxis = direction == ForColumns;
    const auto& autoRepeatTracks = isRowAxis ? style().gridAutoRepeatColumns() : style().gridAutoRepeatRows();
    unsigned autoRepeatTrackListLength = autoRepeatTracks.size();

    if (!autoRepeatTrackListLength)
        return 0;

    std::optional<LayoutUnit> availableSize;
    if (isRowAxis) {
        if (sizingOperation != IntrinsicSizeComputation)
            availableSize =  availableLogicalWidth();
    } else {
        availableSize = computeContentLogicalHeight(MainOrPreferredSize, style().logicalHeight(), std::nullopt);
        if (!availableSize) {
            const Length& maxLength = style().logicalMaxHeight();
            if (!maxLength.isUndefined())
                availableSize = computeContentLogicalHeight(MaxSize, maxLength, std::nullopt);
        }
        if (availableSize)
            availableSize = constrainContentBoxLogicalHeightByMinMax(availableSize.value(), std::nullopt);
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
    LayoutUnit gapSize = gridGapForDirection(direction);
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


std::unique_ptr<RenderGrid::OrderedTrackIndexSet> RenderGrid::computeEmptyTracksForAutoRepeat(Grid& grid, GridTrackSizingDirection direction) const
{
    bool isRowAxis = direction == ForColumns;
    if ((isRowAxis && style().gridAutoRepeatColumnsType() != AutoFit)
        || (!isRowAxis && style().gridAutoRepeatRowsType() != AutoFit))
        return nullptr;

    std::unique_ptr<OrderedTrackIndexSet> emptyTrackIndexes;
    unsigned insertionPoint = isRowAxis ? style().gridAutoRepeatColumnsInsertionPoint() : style().gridAutoRepeatRowsInsertionPoint();
    unsigned firstAutoRepeatTrack = insertionPoint + std::abs(grid.smallestTrackStart(direction));
    unsigned lastAutoRepeatTrack = firstAutoRepeatTrack + grid.autoRepeatTracks(direction);

    if (!m_grid.hasGridItems()) {
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

void RenderGrid::placeItemsOnGrid(Grid& grid, SizingOperation sizingOperation)
{
    ASSERT(grid.needsItemsPlacement());
    ASSERT(!grid.hasGridItems());

    unsigned autoRepeatColumns = computeAutoRepeatTracksCount(ForColumns, sizingOperation);
    unsigned autoRepeatRows = computeAutoRepeatTracksCount(ForRows, sizingOperation);
    grid.setAutoRepeatTracks(autoRepeatRows, autoRepeatColumns);

    populateExplicitGridAndOrderIterator(grid);

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    bool hasAnyOrthogonalGridItem = false;
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (child->isOutOfFlowPositioned())
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
        if (child->isOutOfFlowPositioned())
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
        if (child->isOutOfFlowPositioned())
            continue;

        populator.collectChild(*child);

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
    // See http://dev.w3.org/csswg/css-grid/#auto-placement-algo
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

void RenderGrid::clearGrid()
{
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

    bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
    LayoutUnit gap = !hasCollapsedTracks ? gridGapForDirection(direction) : LayoutUnit();
    tracks.reserveCapacity(numPositions - 1);
    for (size_t i = 0; i < numPositions - 2; ++i)
        tracks.append(positions[i + 1] - positions[i] - offsetBetweenTracks - gap);
    tracks.append(positions[numPositions - 1] - positions[numPositions - 2]);

    if (!hasCollapsedTracks)
        return tracks;

    size_t remainingEmptyTracks = m_grid.autoRepeatEmptyTracks(direction)->size();
    size_t lastLine = tracks.size();
    gap = gridGapForDirection(direction);
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

void RenderGrid::applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    std::optional<LayoutUnit> freeSpace = sizingData.freeSpace(direction);
    if (!freeSpace
        || freeSpace.value() <= 0
        || (direction == ForColumns && style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorGrid()) != ContentDistributionStretch)
        || (direction == ForRows && style().resolvedAlignContentDistribution(contentAlignmentNormalBehaviorGrid()) != ContentDistributionStretch))
        return;

    // Spec defines auto-sized tracks as the ones with an 'auto' max-sizing function.
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> autoSizedTracksIndex;
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i, sizingData.sizingOperation);
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
    sizingData.setFreeSpace(direction, std::optional<LayoutUnit>(0));
}

void RenderGrid::layoutGridItems(GridSizingData& sizingData)
{
    ASSERT(sizingData.sizingOperation == TrackSizing);
    populateGridPositionsForDirection(sizingData, ForColumns);
    populateGridPositionsForDirection(sizingData, ForRows);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned()) {
            prepareChildForPositionedLayout(*child);
            continue;
        }

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        std::optional<LayoutUnit> oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        std::optional<LayoutUnit> oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

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
    // FIXME: Properly support orthogonal writing mode.
    if (!isOrthogonalChild(child)) {
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

        if (child.parent() == this) {
            auto& childLayer = *child.layer();
            childLayer.setStaticInlinePosition(borderStart() + columnOffset);
            childLayer.setStaticBlockPosition(borderBefore() + rowOffset);
        }
    }

    RenderBlock::layoutPositionedObject(child, relayoutChildren, fixedPositionObjectsOnly);
}

void RenderGrid::offsetAndBreadthForPositionedChild(const RenderBox& child, GridTrackSizingDirection direction, LayoutUnit& offset, LayoutUnit& breadth)
{
    ASSERT(!isOrthogonalChild(child));
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
            end -= guttersSize(direction, endLine - 1, 2);
            end -= isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
        }
    }

    breadth = end - start;
    offset = start;

    if (isRowAxis && !style().isLeftToRightDirection() && !child.style().hasStaticInlinePosition(child.isHorizontalWritingMode())) {
        // If the child doesn't have a static inline position (i.e. "left" and/or "right" aren't "auto",
        // we need to calculate the offset from the left (even if we're in RTL).
        if (endIsAuto)
            offset = LayoutUnit();
        else {
            offset = translateRTLCoordinate(m_columnPositions[endLine]) - borderLogicalLeft();

            if (endLine > 0 && endLine < lastLine) {
                offset += guttersSize(direction, endLine - 1, 2);
                offset += isRowAxis ? m_offsetBetweenColumns : m_offsetBetweenRows;
            }
        }
    }
}

LayoutUnit RenderGrid::assumedRowsSizeForOrthogonalChild(const RenderBox& child, SizingOperation sizingOperation) const
{
    ASSERT(isOrthogonalChild(child));
    const GridSpan& span = m_grid.gridItemSpan(child, ForRows);
    LayoutUnit gridAreaSize;
    bool gridAreaIsIndefinite = false;
    LayoutUnit containingBlockAvailableSize = containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
    for (auto trackPosition : span) {
        GridLength maxTrackSize = gridTrackSize(ForRows, trackPosition, sizingOperation).maxTrackBreadth();
        if (maxTrackSize.isContentSized() || maxTrackSize.isFlex())
            gridAreaIsIndefinite = true;
        else
            gridAreaSize += valueForLength(maxTrackSize.length(), containingBlockAvailableSize);
    }

    gridAreaSize += guttersSize(ForRows, span.startLine(), span.integerSpan());

    return gridAreaIsIndefinite ? std::max(child.maxPreferredLogicalWidth(), gridAreaSize) : gridAreaSize;
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction, const GridSizingData& sizingData) const
{
    // To determine the column track's size based on an orthogonal grid item we need it's logical height, which
    // may depend on the row track's size. It's possible that the row tracks sizing logic has not been performed yet,
    // so we will need to do an estimation.
    if (direction == ForRows && sizingData.sizingState == GridSizingData::ColumnSizingFirstIteration)
        return assumedRowsSizeForOrthogonalChild(child, sizingData.sizingOperation);

    const Vector<GridTrack>& tracks = direction == ForColumns ? sizingData.columnTracks : sizingData.rowTracks;
    const GridSpan& span = m_grid.gridItemSpan(child, direction);
    LayoutUnit gridAreaBreadth = 0;
    for (auto trackPosition : span)
        gridAreaBreadth += tracks[trackPosition].baseSize();

    gridAreaBreadth += guttersSize(direction, span.startLine(), span.integerSpan());

    return gridAreaBreadth;
}

LayoutUnit RenderGrid::gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox& child, GridTrackSizingDirection direction, const GridSizingData& sizingData) const
{
    // We need the cached value when available because Content Distribution alignment properties
    // may have some influence in the final grid area breadth.
    const auto& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    const auto& span = m_grid.gridItemSpan(child, direction);
    const auto& linePositions = (direction == ForColumns) ? m_columnPositions : m_rowPositions;

    LayoutUnit initialTrackPosition = linePositions[span.startLine()];
    LayoutUnit finalTrackPosition = linePositions[span.endLine() - 1];

    // Track Positions vector stores the 'start' grid line of each track, so we have to add last track's baseSize.
    return finalTrackPosition - initialTrackPosition + tracks[span.endLine() - 1].baseSize();
}

void RenderGrid::populateGridPositionsForDirection(GridSizingData& sizingData, GridTrackSizingDirection direction)
{
    // Since we add alignment offsets and track gutters, grid lines are not always adjacent. Hence we will have to
    // assume from now on that we just store positions of the initial grid lines of each track,
    // except the last one, which is the only one considered as a final grid line of a track.

    // The grid container's frame elements (border, padding and <content-position> offset) are sensible to the
    // inline-axis flow direction. However, column lines positions are 'direction' unaware. This simplification
    // allows us to use the same indexes to identify the columns independently on the inline-axis direction.
    bool isRowAxis = direction == ForColumns;
    auto& tracks = isRowAxis ? sizingData.columnTracks : sizingData.rowTracks;
    unsigned numberOfTracks = tracks.size();
    unsigned numberOfLines = numberOfTracks + 1;
    unsigned lastLine = numberOfLines - 1;

    ContentAlignmentData offset = computeContentPositionAndDistributionOffset(direction, sizingData.freeSpace(direction).value(), numberOfTracks);
    auto& positions = isRowAxis ? m_columnPositions : m_rowPositions;
    positions.resize(numberOfLines);
    auto borderAndPadding = isRowAxis ? borderAndPaddingLogicalLeft() : borderAndPaddingBefore();
    positions[0] = borderAndPadding + offset.positionOffset;
    if (numberOfLines > 1) {
        // If we have collapsed tracks we just ignore gaps here and add them later as we might not
        // compute the gap between two consecutive tracks without examining the surrounding ones.
        bool hasCollapsedTracks = m_grid.hasAutoRepeatEmptyTracks(direction);
        LayoutUnit gap = !hasCollapsedTracks ? gridGapForDirection(direction) : LayoutUnit();
        unsigned nextToLastLine = numberOfLines - 2;
        for (unsigned i = 0; i < nextToLastLine; ++i)
            positions[i + 1] = positions[i] + offset.distributionOffset + tracks[i].baseSize() + gap;
        positions[lastLine] = positions[nextToLastLine] + tracks[nextToLastLine].baseSize();

        // Adjust collapsed gaps. Collapsed tracks cause the surrounding gutters to collapse (they
        // coincide exactly) except on the edges of the grid where they become 0.
        if (hasCollapsedTracks) {
            gap = gridGapForDirection(direction);
            unsigned remainingEmptyTracks = m_grid.autoRepeatEmptyTracks(direction)->size();
            LayoutUnit gapAccumulator;
            for (unsigned i = 1; i < lastLine; ++i) {
                if (m_grid.isEmptyAutoRepeatTrack(direction, i - 1))
                    --remainingEmptyTracks;
                else {
                    // Add gap between consecutive non empty tracks. Add it also just once for an
                    // arbitrary number of empty tracks between two non empty ones.
                    bool allRemainingTracksAreEmpty = remainingEmptyTracks == (lastLine - i);
                    if (!allRemainingTracksAreEmpty || !m_grid.isEmptyAutoRepeatTrack(direction, i))
                        gapAccumulator += gap;
                }
                positions[i] += gapAccumulator;
            }
            positions[lastLine] += gapAccumulator;
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

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
bool RenderGrid::needToStretchChildLogicalHeight(const RenderBox& child) const
{
    if (child.style().resolvedAlignSelf(style(), selfAlignmentNormalBehavior).position() != ItemPositionStretch)
        return false;

    return isHorizontalWritingMode() && child.style().height().isAuto();
}

// FIXME: This logic is shared by RenderFlexibleBox, so it should be moved to RenderBox.
LayoutUnit RenderGrid::marginLogicalHeightForChild(const RenderBox& child) const
{
    return isHorizontalWritingMode() ? child.verticalMarginExtent() : child.horizontalMarginExtent();
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
    return gridAreaBreadthForChild - (child.needsLayout() ? computeMarginLogicalSizeForChild(ForRows, child) : marginLogicalHeightForChild(child));
}

StyleSelfAlignmentData RenderGrid::alignSelfForChild(const RenderBox& child) const
{
    return child.style().resolvedAlignSelf(style(), selfAlignmentNormalBehavior);
}

StyleSelfAlignmentData RenderGrid::justifySelfForChild(const RenderBox& child) const
{
    return child.style().resolvedJustifySelf(style(), selfAlignmentNormalBehavior);
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

GridAxisPosition RenderGrid::columnAxisPositionForChild(const RenderBox& child) const
{
    bool hasSameWritingMode = child.style().writingMode() == style().writingMode();
    bool childIsLTR = child.style().isLeftToRightDirection();

    switch (child.style().resolvedAlignSelf(style(), selfAlignmentNormalBehavior).position()) {
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

    switch (child.style().resolvedJustifySelf(style(), selfAlignmentNormalBehavior).position()) {
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
            endOfRow -= gridGapForDirection(ForRows) + m_offsetBetweenRows;
        LayoutUnit columnAxisChildSize = isOrthogonalChild(child) ? child.logicalWidth() + child.marginLogicalWidth() : child.logicalHeight() + child.marginLogicalHeight();
        auto overflow = child.style().resolvedAlignSelf(style(), selfAlignmentNormalBehavior).overflow();
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
            endOfColumn -= gridGapForDirection(ForColumns) + m_offsetBetweenColumns;
        LayoutUnit rowAxisChildSize = isOrthogonalChild(child) ? child.logicalHeight() + child.marginLogicalHeight() : child.logicalWidth() + child.marginLogicalWidth();
        auto overflow = child.style().resolvedJustifySelf(style(), selfAlignmentNormalBehavior).overflow();
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

ContentAlignmentData RenderGrid::computeContentPositionAndDistributionOffset(GridTrackSizingDirection direction, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const
{
    bool isRowAxis = direction == ForColumns;
    auto position = isRowAxis ? style().resolvedJustifyContentPosition(contentAlignmentNormalBehaviorGrid()) : style().resolvedAlignContentPosition(contentAlignmentNormalBehaviorGrid());
    auto distribution = isRowAxis ? style().resolvedJustifyContentDistribution(contentAlignmentNormalBehaviorGrid()) : style().resolvedAlignContentDistribution(contentAlignmentNormalBehaviorGrid());
    // If <content-distribution> value can't be applied, 'position' will become the associated
    // <content-position> fallback value.
    auto contentAlignment = contentDistributionOffset(availableFreeSpace, position, distribution, numberOfGridTracks);
    if (contentAlignment.isValid())
        return contentAlignment;

    auto overflow = isRowAxis ? style().justifyContentOverflowAlignment() : style().alignContentOverflowAlignment();
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
    if (direction == ForRows)
        return grid.numTracks(ForRows);

    // FIXME: This still requires knowledge about m_grid internals.
    return grid.numTracks(ForRows) ? grid.numTracks(ForColumns) : GridPositionsResolver::explicitGridColumnCount(style(), grid.autoRepeatTracks(ForColumns));
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
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

#endif /* ENABLE(CSS_GRID_LAYOUT) */

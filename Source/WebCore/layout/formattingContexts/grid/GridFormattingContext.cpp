/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridFormattingContext.h"

#include "GridItemRect.h"
#include "GridLayout.h"
#include "GridLayoutContext.h"
#include "GridLayoutContextBuilder.h"
#include "GridLayoutUtils.h"
#include "GridSizer.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleGapGutter.h"
#include "StylePrimitiveNumeric.h"
#include "TrackSizingFunctions.h"
#include "UsedTrackSizes.h"

#include <wtf/Vector.h>

namespace WebCore {
namespace Layout {

GridFormattingContext::GridFormattingContext(const ElementBox& gridBox, LayoutState& layoutState)
    : m_gridBox(gridBox)
    , m_globalLayoutState(layoutState)
    , m_integrationUtils(layoutState)
    , m_intrinsicWidthSizingPath(classifyIntrinsicWidthSizingPath())
{
}

NODELETE static AutoRepeatConstraint autoRepeatConstraint(const AxisConstraint& axisConstraint)
{
    auto containerMinimumSize = axisConstraint.containerMinimumSize();
    auto containerMaximumSize = axisConstraint.containerMaximumSize();
    if (axisConstraint.scenario() == AxisConstraint::FreeSpaceScenario::Definite)
        return { axisConstraint.availableSpace(), containerMinimumSize, containerMaximumSize };

    return { { }, containerMinimumSize, containerMaximumSize };
}

// https://drafts.csswg.org/css-grid-1/#grid-baselines
// The offsets are relative to the grid container's content box, the space gridItemRects is in.
static GridContainerBaselines computeGridContainerBaselines(const GridItemRects& gridItemRects, const LogicalGridItems& logicalGridItems, const ElementBox& gridContainer, const IntegrationUtils& integrationUtils)
{
    // "If the grid container does not contain any grid items, the grid container has no first (last) baseline set".
    if (gridItemRects.isEmpty())
        return { };

    // "Find the first (last) row of the grid container containing at least one grid item."
    auto firstRow = std::numeric_limits<size_t>::max();
    size_t lastRow = 0;
    for (auto& gridItemRect : gridItemRects) {
        firstRow = std::min(firstRow, gridItemRect.lineNumbersForGridArea.rowStartLine);
        lastRow = std::max(lastRow, gridItemRect.lineNumbersForGridArea.rowEndLine - 1);
    }

    // FIXME: "If any of the grid items intersecting this row participate in baseline alignment in that row, the grid
    // container's baseline set is generated from the shared alignment baseline of those grid items." This is unreachable
    // for now: a grid item whose resolved align-self is baseline or last baseline gets
    // GridItemHasUnsupportedBlockAxisAlignment (and one with such a justify-self gets
    // GridItemHasUnsupportedInlineAxisAlignment), so it never reaches GFC. Only the second rule below is needed. The
    // ASSERT in baselineOfItemInRow guards this.

    // "Otherwise, the grid container's first (last) baseline set is generated from the alignment baseline of the first (last)
    // grid item in row-major grid order."
    // Row-major grid order visits the cells of the row front to back for the first baseline and back to front for the
    // last one, so an item is encountered at the column where it starts (first) or ends (last).
    // "If two items are encountered at the same time, they are taken in order-modified document order",
    // which is the order of logicalGridItems. gridItemRects is in placement order, not in that order, so
    // logicalGridItems is only consulted to break ties.
    auto orderModifiedDocumentOrderIndex = [&](const ElementBox& gridItem) {
        auto index = logicalGridItems.findIf([&](auto& logicalGridItem) {
            return logicalGridItem.ptr() == &gridItem;
        });
        ASSERT(index != notFound);
        return index;
    };

    auto baselineOfItemInRow = [&](size_t row, bool isFirst) -> std::optional<LayoutUnit> {
        const GridItemRect* baselineItemRect = nullptr;
        auto isEncounteredBefore = [&](const GridItemRect& a, const GridItemRect& b) {
            auto& aLines = a.lineNumbersForGridArea;
            auto& bLines = b.lineNumbersForGridArea;
            auto aColumn = isFirst ? aLines.columnStartLine : aLines.columnEndLine;
            auto bColumn = isFirst ? bLines.columnStartLine : bLines.columnEndLine;
            if (aColumn != bColumn)
                return aColumn < bColumn;
            return orderModifiedDocumentOrderIndex(a.layoutBox) < orderModifiedDocumentOrderIndex(b.layoutBox);
        };
        CheckedRef gridContainerStyle = gridContainer.style();
        for (auto& gridItemRect : gridItemRects) {
            auto& lines = gridItemRect.lineNumbersForGridArea;
            if (row < lines.rowStartLine || row >= lines.rowEndLine)
                continue;
            auto alignSelfPosition = gridItemRect.layoutBox->style().alignSelf().resolve(gridContainerStyle.ptr()).position();
            ASSERT_UNUSED(alignSelfPosition, alignSelfPosition != ItemPosition::Baseline && alignSelfPosition != ItemPosition::LastBaseline);
            if (!baselineItemRect || (isFirst ? isEncounteredBefore(gridItemRect, *baselineItemRect) : isEncounteredBefore(*baselineItemRect, gridItemRect)))
                baselineItemRect = &gridItemRect;
        }
        ASSERT(baselineItemRect);

        // "If the grid item has no alignment baseline in the grid's inline axis, then one is first synthesized from its border
        // edges." Orthogonal items are gated by coverage. An orthogonal item has no baseline in the grid's inline axis, so one
        // would be synthesized from its border box the same way.
        auto& borderBoxRect = baselineItemRect->borderBoxRect;
        auto itemBaseline = isFirst ? integrationUtils.firstLineBaselineForGridItem(baselineItemRect->layoutBox) : integrationUtils.lastLineBaselineForGridItem(baselineItemRect->layoutBox);
        return borderBoxRect.y() + itemBaseline.value_or(borderBoxRect.height());
    };

    return { baselineOfItemInRow(firstRow, true), baselineOfItemInRow(lastRow, false) };
}

GridLayoutResult GridFormattingContext::layout(GridLayoutConstraints layoutConstraints)
{
    auto inputForAxis = [](const AxisConstraint& axisConstraint) -> GridAxisLayoutContextInput {
        return { .autoRepeat = autoRepeatConstraint(axisConstraint), .containerSizeDependsOnTracks = axisConstraint.scenario() != AxisConstraint::FreeSpaceScenario::Definite };
    };
    // 1. Run the Grid Item Placement Algorithm, and set up the track sizing: see GridLayoutContextBuilder.
    const auto layoutContext = GridLayoutContextBuilder { root() }.build({ .inlineAxis = inputForAxis(layoutConstraints.inlineAxis), .blockAxis = inputForAxis(layoutConstraints.blockAxis), .keepSpecifiedRowTrackSizes = true });

    auto& placedGridItems = layoutContext.placedGridItems;
    auto& columnTrackSizingFunctions = layoutContext.columnTrackSizingFunctions;

    // 2. Find the size of the grid container.
    auto usedTrackSizes = GridSizer { *this, layoutContext, layoutConstraints }.sizeGrid(placedGridItems, columnTrackSizingFunctions, layoutContext.rowTrackSizingFunctions);
    auto usedBlockContentSize = [&] {
        auto& blockAxisConstraint = layoutConstraints.blockAxis;
        if (blockAxisConstraint.scenario() == AxisConstraint::FreeSpaceScenario::Definite)
            return blockAxisConstraint.availableSpace();

        auto& rowSizes = usedTrackSizes.rowSizes;
        auto blockContentSize = std::reduce(rowSizes.begin(), rowSizes.end()) + GridLayoutUtils::totalGuttersSize(rowSizes.size(), layoutContext.collapsedRowTracksList, layoutContext.usedRowGap);
        if (auto containerMaximumSize = blockAxisConstraint.containerMaximumSize())
            blockContentSize = std::min(blockContentSize, *containerMaximumSize);
        if (auto containerMinimumSize = blockAxisConstraint.containerMinimumSize())
            blockContentSize = std::max(blockContentSize, *containerMinimumSize);
        return blockContentSize;
    }();

    // 3. Given the resulting grid container size, run the Grid Sizing Algorithm to size the grid.
    // The block axis' percentages were treated as auto when the container's block size depends on its tracks.
    bool blockAxisDependsOnTracks = layoutConstraints.blockAxis.scenario() != AxisConstraint::FreeSpaceScenario::Definite;
    ASSERT(!blockAxisDependsOnTracks || layoutContext.specifiedRows);
    bool rowsHadPercentages = layoutContext.specifiedRows && layoutContext.specifiedRows->hasPercentages;
    // Flexible rows can size differently against the grid container's now definite block size than
    // against indefinite free space, e.g. when their flex factors sum to less than 1.
    bool hasFlexibleRows = blockAxisDependsOnTracks && layoutContext.rowTrackSizingFunctions.containsIf([](auto& trackSizingFunctions) {
        return trackSizingFunctions.max.isFlex();
    });
    std::optional<TrackSizingFunctionsList> resolvedRowTrackSizingFunctions;
    if (rowsHadPercentages || hasFlexibleRows) {
        resolvedRowTrackSizingFunctions = layoutContext.rowTrackSizingFunctionsAsSpecified();
        auto& blockAxisConstraint = layoutConstraints.blockAxis;
        GridLayoutConstraints layoutConstraintsForGridSizing {
            layoutConstraints.inlineAxis,
            AxisConstraint::definite(usedBlockContentSize, blockAxisConstraint.containerMinimumSize(), blockAxisConstraint.containerMaximumSize())
        };
        usedTrackSizes = GridSizer { *this, layoutContext, layoutConstraintsForGridSizing }.sizeGrid(placedGridItems, columnTrackSizingFunctions, *resolvedRowTrackSizingFunctions);
    }
    auto& rowTrackSizingFunctions = resolvedRowTrackSizingFunctions ? *resolvedRowTrackSizingFunctions : layoutContext.rowTrackSizingFunctions;

    // 4. Lay out the grid items into their respective containing blocks. Each grid area’s
    // width and height are considered definite for this purpose.
    GridLinePositions columnLines { usedTrackSizes.columnSizes, layoutContext.collapsedColumnTracksList, layoutContext.usedColumnGap };
    GridLinePositions rowLines { usedTrackSizes.rowSizes, layoutContext.collapsedRowTracksList, layoutContext.usedRowGap };
    auto gridItemRects = GridLayout { *this }.layout(placedGridItems, columnTrackSizingFunctions, rowTrackSizingFunctions, columnLines, rowLines);

    // Grid layout positions each item within its containing block which is the grid area.
    // Here we translate it to the coordinate space of the grid.
    auto mapGridItemLocationsToGrid = [&] {

        for (auto& gridItemRect : gridItemRects) {
            auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
            auto columnPosition = columnLines.positionAsStartLine(lineNumbersForGridArea.columnStartLine);
            auto rowPosition = rowLines.positionAsStartLine(lineNumbersForGridArea.rowStartLine);

            gridItemRect.borderBoxRect.moveBy(LayoutPoint { columnPosition, rowPosition });
        }
    };
    mapGridItemLocationsToGrid();
    setGridItemGeometries(gridItemRects);

    auto baselines = computeGridContainerBaselines(gridItemRects, layoutContext.logicalGridItems, root(), integrationUtils());

    auto autoRepeatColumnsCount = layoutContext.gridDefinition.explicitGridTrackSizes.columnAutoRepeatTracksIndexes.distance();
    auto autoRepeatRowsCount = layoutContext.gridDefinition.explicitGridTrackSizes.rowAutoRepeatTracksIndexes.distance();
    return { WTF::move(usedTrackSizes), WTF::move(columnLines), WTF::move(rowLines), WTF::move(gridItemRects), usedBlockContentSize,
        autoRepeatColumnsCount, autoRepeatRowsCount,
        layoutContext.gridDefinition.explicitGridTrackSizes.columnAutoRepeatTruncatedByLimit, layoutContext.gridDefinition.explicitGridTrackSizes.rowAutoRepeatTruncatedByLimit,
        layoutContext.leadingImplicitTracks.columnsCount, layoutContext.leadingImplicitTracks.rowsCount,
        layoutContext.collapsedColumnTracksList, layoutContext.collapsedRowTracksList, baselines };
}

BoxGeometry& GridFormattingContext::geometryForGridItem(const ElementBox& layoutBox)
{
    ASSERT(layoutBox.isGridItem());
    return m_globalLayoutState->ensureGeometryForBox(layoutBox);
}

void GridFormattingContext::setGridItemGeometries(const GridItemRects& gridItemRects)
{
    for (auto& gridItemRect : gridItemRects) {
        auto& boxGeometry = geometryForGridItem(gridItemRect.layoutBox);
        auto& gridItemBorderBox = gridItemRect.borderBoxRect;

        auto& margins = gridItemRect.margins;
        boxGeometry.setHorizontalMargin({ margins.left(), margins.right() });
        boxGeometry.setVerticalMargin({ margins.top(), margins.bottom() });

        boxGeometry.setTopLeft(gridItemBorderBox.location());
        auto contentBoxInlineSize = gridItemBorderBox.width() - boxGeometry.horizontalBorderAndPadding();
        auto contentBoxBlockSize = gridItemBorderBox.height() - boxGeometry.verticalBorderAndPadding();

        boxGeometry.setContentBoxSize({ contentBoxInlineSize, contentBoxBlockSize });
    }
}

IntrinsicWidthSizingPath GridFormattingContext::classifyIntrinsicWidthSizingPath() const
{
    auto containerWritingMode = writingMode();

    auto anyGridItemInlineContributionMayRequireFullSizingAlgorithm = [&] {
        for (CheckedRef gridItem : childrenOfType<ElementBox>(m_gridBox)) {
            if (gridItem->isOutOfFlowPositioned() || gridItem->isExcludedFromGridFormattingContextForIntegration())
                continue;
            if (GridLayoutUtils::inlineContributionMayRequireFullSizingAlgorithmForIntrinsicWidth(gridItem, containerWritingMode))
                return true;
        }
        return false;
    };

    return anyGridItemInlineContributionMayRequireFullSizingAlgorithm()
        ? IntrinsicWidthSizingPath::NeedsFullSizing
        : IntrinsicWidthSizingPath::ColumnsOnly;
}

// https://drafts.csswg.org/css-grid-1/#intrinsic-sizes
// The max-content size (min-content size) of a grid container is the sum of
// the grid container's track sizes (including gutters) in the appropriate axis,
// when the grid is sized under a max-content constraint (min-content constraint).
GridFormattingContext::IntrinsicWidths GridFormattingContext::computeIntrinsicWidths(const AutoRepeatConstraint& inlineAxisAutoRepeatConstraint, const AutoRepeatConstraint& blockAxisAutoRepeatConstraint)
{
    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // For intrinsic sizing, percentages in track sizes must be treated as auto. The context is shared by
    // both intrinsic sizing scenarios and neither is definite, so it treats the percentages in both axes as auto.
    const auto layoutContext = GridLayoutContextBuilder { root() }.build({ .inlineAxis = { .autoRepeat = inlineAxisAutoRepeatConstraint, .containerSizeDependsOnTracks = true }, .blockAxis = { .autoRepeat = blockAxisAutoRepeatConstraint, .containerSizeDependsOnTracks = true } });

    auto columnSizesForConstraint = [&](AxisConstraint intrinsicConstraint) -> TrackSizes {
        GridLayoutConstraints layoutConstraints {
            .inlineAxis = intrinsicConstraint,
            .blockAxis = intrinsicConstraint
        };
        auto& placedGridItems = layoutContext.placedGridItems;
        auto& columnTrackSizingFunctions = layoutContext.columnTrackSizingFunctions;
        auto& rowTrackSizingFunctions = layoutContext.rowTrackSizingFunctions;

        // When no grid item's inline contribution depends on its own block size, the column sizes are
        // final after step 1 of the grid sizing algorithm, so run that step alone.
        if (m_intrinsicWidthSizingPath == IntrinsicWidthSizingPath::ColumnsOnly)
            return GridSizer { *this, layoutContext, layoutConstraints }.sizeColumnsOnlyFastPath(placedGridItems, columnTrackSizingFunctions, rowTrackSizingFunctions);

        auto usedTrackSizes = GridSizer { *this, layoutContext, layoutConstraints }.sizeGrid(placedGridItems, columnTrackSizingFunctions, rowTrackSizingFunctions);
        // FIXME: Laying out the grid items cannot change the column sizes, so this is only here to
        // preserve the side effects of laying them out during intrinsic sizing.
        GridLinePositions columnLines { usedTrackSizes.columnSizes, layoutContext.collapsedColumnTracksList, layoutContext.usedColumnGap };
        GridLinePositions rowLines { usedTrackSizes.rowSizes, layoutContext.collapsedRowTracksList, layoutContext.usedRowGap };
        GridLayout { *this }.layout(placedGridItems, columnTrackSizingFunctions, rowTrackSizingFunctions, columnLines, rowLines);
        return WTF::move(usedTrackSizes.columnSizes);
    };

    TrackSizes minContentColumnSizes = columnSizesForConstraint(AxisConstraint::minContent());
    TrackSizes maxContentColumnSizes = columnSizesForConstraint(AxisConstraint::maxContent());

    // Sum track sizes and add gaps
    auto computeIntrinsicWidth = [&](const TrackSizes& trackSizes) -> LayoutUnit {
        auto sumOfTrackSizes = 0_lu;
        for (auto trackSize : trackSizes)
            sumOfTrackSizes += trackSize;
        return sumOfTrackSizes + GridLayoutUtils::totalGuttersSize(trackSizes.size(), layoutContext.collapsedColumnTracksList, layoutContext.usedColumnGap);
    };

    return IntrinsicWidths {
        .minimum = computeIntrinsicWidth(minContentColumnSizes),
        .maximum = computeIntrinsicWidth(maxContentColumnSizes)
    };
}

} // namespace Layout
} // namespace WebCore

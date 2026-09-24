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

#include "ExplicitGridResolver.h"
#include "GridItemPlacer.h"
#include "GridItemRect.h"
#include "GridLayout.h"
#include "GridLayoutState.h"
#include "GridLayoutUtils.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleGapGutter.h"
#include "StylePrimitiveNumeric.h"
#include "UnplacedGridItem.h"
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

static LogicalGridItems constructLogicalGridItems(const ElementBox& gridBox)
{
    LogicalGridItems logicalGridItems;
    for (CheckedRef gridItem : childrenOfType<ElementBox>(gridBox)) {
        if (gridItem->isOutOfFlowPositioned())
            continue;

        logicalGridItems.append(gridItem);
    }

    std::ranges::stable_sort(logicalGridItems, { }, [](auto& gridItem) {
        return gridItem->style().order().value;
    });
    return logicalGridItems;
}

static LeadingImplicitTracks computeLeadingImplicitTracks(const LogicalGridItems& logicalGridItems, const ExplicitGridTrackSizes& explicitGridTrackSizes)
{
    // Negative line placements are resolved against the explicit grid track count, which can still
    // produce a negative line when the placement counts past the start edge of the explicit grid.
    // Those are normalized by shifting every line forward by the magnitude of the most-negative
    // resolved line, so e.g. with 3 explicit columns a column-start of -5 resolves to line -1,
    // which shifts all column lines forward by 1 and maps to matrix column 0. That magnitude is
    // also the number of leading implicit tracks the grid needs to generate.
    auto explicitColumnCount = explicitGridTrackSizes.columnsCount();
    auto explicitRowCount = explicitGridTrackSizes.rowsCount();
    int minimumColumnLine = 0;
    int minimumRowLine = 0;
    for (auto& gridItem : logicalGridItems) {
        CheckedRef gridItemStyle = gridItem->style();
        if (auto columnRange = UnplacedGridItem::resolveDefinitePosition(gridItemStyle->gridItemColumnStart(), gridItemStyle->gridItemColumnEnd(), explicitColumnCount)) {
            auto [startLine, endLine] = *columnRange;
            minimumColumnLine = std::min({ minimumColumnLine, startLine, endLine });
        }
        if (auto rowRange = UnplacedGridItem::resolveDefinitePosition(gridItemStyle->gridItemRowStart(), gridItemStyle->gridItemRowEnd(), explicitRowCount)) {
            auto [startLine, endLine] = *rowRange;
            minimumRowLine = std::min({ minimumRowLine, startLine, endLine });
        }
    }

    return {
        minimumColumnLine < 0 ? static_cast<size_t>(-minimumColumnLine) : 0,
        minimumRowLine < 0 ? static_cast<size_t>(-minimumRowLine) : 0
    };
}

static Style::GridTrackSizes gridAutoTrackSizesWithPercentagesConvertedToAuto(const Style::GridTrackSizes& gridAutoTrackSizes)
{
    return Style::GridTrackSizes { Style::GridTrackSizeList::map(gridAutoTrackSizes, GridLayoutUtils::trackSizeWithPercentagesConvertedToAuto) };
}

GridLayoutResult GridFormattingContext::layout(GridLayoutConstraints layoutConstraints)
{
    auto logicalGridItems = constructLogicalGridItems(root());
    CheckedRef gridStyle = root().style();
    auto explicitGridTrackSizes = ExplicitGridResolver::resolve(gridStyle, layoutConstraints);
    auto leadingImplicitTracks = computeLeadingImplicitTracks(logicalGridItems, explicitGridTrackSizes);

    GridAutoFlowOptions autoFlowOptions {
        .strategy = gridStyle->gridAutoFlow().isDense() ? PackingStrategy::Dense : PackingStrategy::Sparse,
        .direction = gridStyle->gridAutoFlow().isRow() ? GridAutoFlowDirection::Row : GridAutoFlowDirection::Column
    };

    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // If the size of the grid container depends on the size of its tracks, then the
    // <percentage> must be treated as auto, for the purpose of calculating the intrinsic
    // sizes of the grid container and then resolve against that resulting grid container
    // size for the purpose of laying out the grid and its items.
    // This is evaluated per-axis: percentages in column tracks depend on inline-axis constraints,
    // and percentages in row tracks depend on block-axis constraints.
    auto inlineAxisDependsOnTracks = layoutConstraints.inlineAxis.scenario() != AxisConstraint::FreeSpaceScenario::Definite;
    auto blockAxisDependsOnTracks = layoutConstraints.blockAxis.scenario() != AxisConstraint::FreeSpaceScenario::Definite;

    auto gridAutoColumns = inlineAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoColumns()) : gridStyle->gridAutoColumns();
    auto gridAutoRows = blockAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoRows()) : gridStyle->gridAutoRows();

    GridDefinition gridDefinition { WTF::move(explicitGridTrackSizes), gridAutoColumns, gridAutoRows, autoFlowOptions, gridStyle->usedZoomForLength() };

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    GridLayoutState layoutState { layoutConstraints, gridDefinition, usedJustifyContent, usedAlignContent, usedGapValue(gridStyle->columnGap(), gridStyle), usedGapValue(gridStyle->rowGap(), gridStyle) };

    // https://drafts.csswg.org/css-grid-1/#layout-algorithm
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto gridItemPlacementResult = GridItemPlacer { autoFlowOptions }.placeItems(logicalGridItems, leadingImplicitTracks, gridDefinition.explicitGridTrackSizes.columnsCount(), gridDefinition.explicitGridTrackSizes.rowsCount());

    auto [ usedTrackSizes, gridItemRects ] = GridLayout { *this }.layout(gridItemPlacementResult, leadingImplicitTracks, layoutState);

    // Grid layout positions each item within its containing block which is the grid area.
    // Here we translate it to the coordinate space of the grid.
    auto mapGridItemLocationsToGrid = [&] {

        for (auto& gridItemRect : gridItemRects) {
            auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
            auto columnPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.columnStartLine, usedTrackSizes.columnSizes, layoutState.usedColumnGap);
            auto rowPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.rowStartLine, usedTrackSizes.rowSizes, layoutState.usedRowGap);

            gridItemRect.borderBoxRect.moveBy(LayoutPoint { columnPosition, rowPosition });
        }
    };
    mapGridItemLocationsToGrid();
    setGridItemGeometries(gridItemRects);
    return { WTF::move(usedTrackSizes), WTF::move(gridItemRects) };
}

PlacedGridItems GridFormattingContext::constructPlacedGridItems(const GridAreas& gridAreas) const
{
    PlacedGridItems placedGridItems;
    placedGridItems.reserveInitialCapacity(gridAreas.size());
    CheckedRef formattingContextStyle = root().style();
    for (auto& [ unplacedGridItem, gridAreaLines ] : gridAreas) {
        CheckedRef gridItem = unplacedGridItem.m_layoutBox;
        CheckedRef gridContainerStyle = this->gridContainerStyle();
        placedGridItems.constructAndAppend(gridItem, gridAreaLines, gridContainerStyle);
    }
    return placedGridItems;
}

const BoxGeometry& GridFormattingContext::geometryForGridItem(const ElementBox& layoutBox) const
{
    ASSERT(layoutBox.isGridItem());
    return layoutState().geometryForBox(layoutBox);
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
            if (gridItem->isOutOfFlowPositioned())
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
GridFormattingContext::IntrinsicWidths GridFormattingContext::computeIntrinsicWidths()
{
    CheckedRef gridStyle = root().style();
    GridAutoFlowOptions autoFlowOptions {
        .strategy = gridStyle->gridAutoFlow().isDense() ? PackingStrategy::Dense : PackingStrategy::Sparse,
        .direction = gridStyle->gridAutoFlow().isRow() ? GridAutoFlowDirection::Row : GridAutoFlowDirection::Column
    };

    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // For intrinsic sizing, percentages in track sizes must be treated as auto. The explicit grid is
    // shared by both intrinsic sizing scenarios and neither is definite, so resolving it against the
    // min-content constraint treats its percentages as auto for both.
    GridDefinition gridDefinition {
        ExplicitGridResolver::resolve(gridStyle, { AxisConstraint::minContent(), AxisConstraint::minContent() }),
        gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoColumns()),
        gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoRows()),
        autoFlowOptions,
        gridStyle->usedZoomForLength(),
    };

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    auto usedColumnGap = usedGapValue(gridStyle->columnGap(), gridStyle);
    auto usedRowGap = usedGapValue(gridStyle->rowGap(), gridStyle);

    auto logicalGridItems = constructLogicalGridItems(root());
    auto leadingImplicitTracks = computeLeadingImplicitTracks(logicalGridItems, gridDefinition.explicitGridTrackSizes);

    // https://drafts.csswg.org/css-grid-1/#layout-algorithm
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    // The placement does not depend on the axis constraints, so it is shared by both intrinsic sizing scenarios.
    auto gridItemPlacementResult = GridItemPlacer { autoFlowOptions }.placeItems(logicalGridItems, leadingImplicitTracks, gridDefinition.explicitGridTrackSizes.columnsCount(), gridDefinition.explicitGridTrackSizes.rowsCount());

    auto columnSizesForConstraint = [&](AxisConstraint intrinsicConstraint) -> TrackSizes {
        GridLayoutConstraints layoutConstraints {
            .inlineAxis = intrinsicConstraint,
            .blockAxis = intrinsicConstraint
        };
        GridLayoutState layoutState { layoutConstraints, gridDefinition, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap };

        // When no grid item's inline contribution depends on its own block size, the column sizes are
        // final after step 1 of the grid sizing algorithm, so ask GridLayout for that step alone.
        auto scope = m_intrinsicWidthSizingPath == IntrinsicWidthSizingPath::ColumnsOnly
            ? GridLayoutScope::ColumnSizingOnly
            : GridLayoutScope::Full;

        return GridLayout { *this }.layout(gridItemPlacementResult, leadingImplicitTracks, layoutState, scope).usedTrackSizes.columnSizes;
    };

    TrackSizes minContentColumnSizes = columnSizesForConstraint(AxisConstraint::minContent());
    TrackSizes maxContentColumnSizes = columnSizesForConstraint(AxisConstraint::maxContent());

    // Sum track sizes and add gaps
    auto computeIntrinsicWidth = [&](const TrackSizes& trackSizes) -> LayoutUnit {
        auto sumOfTrackSizes = 0_lu;
        for (auto trackSize : trackSizes)
            sumOfTrackSizes += trackSize;
        auto totalGutters = trackSizes.size() > 1 ? usedColumnGap * LayoutUnit(trackSizes.size() - 1) : 0_lu;
        return sumOfTrackSizes + totalGutters;
    };

    return IntrinsicWidths {
        .minimum = computeIntrinsicWidth(minContentColumnSizes),
        .maximum = computeIntrinsicWidth(maxContentColumnSizes)
    };
}

} // namespace Layout
} // namespace WebCore

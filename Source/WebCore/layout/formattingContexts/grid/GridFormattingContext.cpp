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
#include "GridSizer.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleGapGutter.h"
#include "StylePrimitiveNumeric.h"
#include "TrackSizingFunctions.h"
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

static Vector<Style::GridTrackSize> trackSizesWithPercentagesConvertedToAuto(const Vector<Style::GridTrackSize>& trackSizes)
{
    return trackSizes.map(GridLayoutUtils::trackSizeWithPercentagesConvertedToAuto);
}

static bool hasPercentageTrackSize(const auto& trackSizes)
{
    return trackSizes.containsIf([](auto& trackSize) {
        if (trackSize.isFitContent())
            return trackSize.fitContentTrackLength().isPercentOrCalculated();
        return trackSize.minTrackBreadth().isPercentOrCalculated() || trackSize.maxTrackBreadth().isPercentOrCalculated();
    });
}

static AutoRepeatConstraint LEGACY_NODELETE autoRepeatConstraint(const AxisConstraint& axisConstraint)
{
    auto containerMinimumSize = axisConstraint.containerMinimumSize();
    auto containerMaximumSize = axisConstraint.containerMaximumSize();
    if (axisConstraint.scenario() == AxisConstraint::FreeSpaceScenario::Definite)
        return { axisConstraint.availableSpace(), containerMinimumSize, containerMaximumSize };

    return { { }, containerMinimumSize, containerMaximumSize };
}

static ExplicitGridTrackSizes explicitGridTrackSizesForIntrinsicWidths(const Style::ComputedStyle& gridStyle, const AutoRepeatConstraint& inlineAxisAutoRepeatConstraint, const AutoRepeatConstraint& blockAxisAutoRepeatConstraint, LayoutUnit usedColumnGap, LayoutUnit usedRowGap)
{
    auto explicitGridTrackSizes = ExplicitGridResolver::resolve(gridStyle, inlineAxisAutoRepeatConstraint, blockAxisAutoRepeatConstraint, usedColumnGap, usedRowGap);
    return {
        trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.columnTrackSizes),
        trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.rowTrackSizes),
        explicitGridTrackSizes.columnAutoRepeatTracksIndexes,
        explicitGridTrackSizes.rowAutoRepeatTracksIndexes
    };
}

static TrackSizingFunctions convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize& gridTrackSize, const Style::ZoomFactor& zoom)
{
    auto minTrackSizingFunction = [&]() {
        // If the track was sized with a minmax() function, this is the first argument to that function.
        if (gridTrackSize.isMinMax())
            return gridTrackSize.minTrackBreadth();

        // If the track was sized with a <flex> value or fit-content() function, auto.
        if (gridTrackSize.isFitContent() || gridTrackSize.minTrackBreadth().isFlex())
            return Style::GridTrackBreadth { CSS::Keyword::Auto { } };

        // Otherwise, the track's sizing function.
        return gridTrackSize.minTrackBreadth();
    };

    auto maxTrackSizingFunction = [&]() -> MaxTrackSizingFunction {
        // If the track was sized with a minmax() function, this is the second argument to that function.
        if (gridTrackSize.isMinMax())
            return gridTrackSize.maxTrackBreadth();

        // Otherwise, the track’s sizing function. In all cases, treat auto and fit-content() as max-content,
        // except where specified otherwise for fit-content().
        // Note: This special treatment is handled inside of TrackSizingAlgorithm.
        if (gridTrackSize.isFitContent())
            return Style::GridTrackSize::FitContent { gridTrackSize.fitContentTrackLength() };

        return gridTrackSize.maxTrackBreadth();
    };

    return TrackSizingFunctions { minTrackSizingFunction(), maxTrackSizingFunction(), zoom };
}

// Generates track sizing functions for implicitTracksCount implicit tracks using
// grid-auto-{columns,rows}, cycling forwards through the provided sizes starting from the first.
static TrackSizingFunctionsList generateImplicitTrackSizingFunctions(size_t implicitTracksCount, const Style::GridTrackSizes& gridAutoTrackSizes, const Style::ZoomFactor& zoom)
{
    TrackSizingFunctionsList trackSizingFunctionsForImplicitGrid;
    trackSizingFunctionsForImplicitGrid.reserveInitialCapacity(implicitTracksCount);

    // Cycle through grid-auto-{columns,rows} values using modulo.
    for (size_t i = 0; i < implicitTracksCount; ++i) {
        size_t autoTrackIndex = i % gridAutoTrackSizes.size();
        trackSizingFunctionsForImplicitGrid.append(convertGridTrackSizeToTrackSizingFunctions(gridAutoTrackSizes[autoTrackIndex], zoom));
    }

    return trackSizingFunctionsForImplicitGrid;
}

static TrackSizingFunctionsList trackSizingFunctions(size_t totalTracksCount, size_t leadingImplicitTracksCount, const Vector<Style::GridTrackSize>& gridTemplateTrackSizes, const Style::GridTrackSizes& gridAutoTrackSizes, const BitVector& collapsedTracksList, const Style::ZoomFactor& zoom)
{
    auto explicitTracksCount = gridTemplateTrackSizes.size();
    ASSERT_WITH_MESSAGE(totalTracksCount >= leadingImplicitTracksCount + explicitTracksCount, "Total tracks should be at least as many as the leading implicit tracks plus the explicit tracks");

    TrackSizingFunctionsList trackSizingFunctions;
    trackSizingFunctions.reserveInitialCapacity(totalTracksCount);

    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    // "The last implicit grid track before the explicit grid receives the last specified size, and so on backwards"
    // Check for leading tracks before reversing: Style::reversedTrackSizes copies
    // grid-auto-{columns,rows}, and most grids have nothing before the explicit grid to spend it on.
    if (leadingImplicitTracksCount) {
        auto leadingTrackSizingFunctions = generateImplicitTrackSizingFunctions(leadingImplicitTracksCount, Style::reversedTrackSizes(gridAutoTrackSizes), zoom);
        trackSizingFunctions.appendRange(leadingTrackSizingFunctions.rbegin(), leadingTrackSizingFunctions.rend());
    }

    // https://drafts.csswg.org/css-grid-1/#algo-terms
    // Map explicit tracks from grid-template-{columns,rows}
    for (auto& gridTrackSize : gridTemplateTrackSizes)
        trackSizingFunctions.append(convertGridTrackSizeToTrackSizingFunctions(gridTrackSize, zoom));

    // https://drafts.csswg.org/css-grid-1/#auto-tracks
    // "The first track after the last explicitly-sized track receives the first specified size, and so on forwards"
    auto trailingImplicitTracksCount = totalTracksCount - leadingImplicitTracksCount - explicitTracksCount;
    trackSizingFunctions.appendVector(generateImplicitTrackSizingFunctions(trailingImplicitTracksCount, gridAutoTrackSizes, zoom));

    // https://drafts.csswg.org/css-grid-1/#collapsed-track
    for (auto collapsedTrackIndex : collapsedTracksList)
        trackSizingFunctions[collapsedTrackIndex] = convertGridTrackSizeToTrackSizingFunctions(Style::GridTrackSize { CSS::Literals::px(0) }, zoom);

    ASSERT(trackSizingFunctions.size() == totalTracksCount);
    return trackSizingFunctions;
}

// https://drafts.csswg.org/css-grid-1/#valdef-repeat-auto-fit
static BitVector collapsedTracksList(WTF::Range<size_t> autoFitTracksIndexes, size_t leadingImplicitTracksCount, const PlacedGridItemSpanList& gridItemSpans)
{
    ASSERT(autoFitTracksIndexes);

    // The auto-fit tracks are indexed within the explicit grid, which starts after the leading implicit tracks.
    auto firstAutoFitTrackIndex = leadingImplicitTracksCount + autoFitTracksIndexes.begin();
    auto lastAutoFitTrackIndex = leadingImplicitTracksCount + autoFitTracksIndexes.end() - 1;

    // "...after grid item placement any empty repeated tracks are collapsed. An empty track is one with no
    // in-flow grid items placed into or spanning across it."
    BitVector collapsedTracks;
    collapsedTracks.ensureSize(lastAutoFitTrackIndex + 1);
    for (auto trackIndex = firstAutoFitTrackIndex; trackIndex <= lastAutoFitTrackIndex; ++trackIndex) {
        bool isEmptyTrack = std::ranges::none_of(gridItemSpans, [&](auto& gridItemSpan) {
            return gridItemSpan.contains(trackIndex);
        });
        if (isEmptyTrack)
            collapsedTracks.quickSet(trackIndex);
    }
    return collapsedTracks;
}

static BitVector collapsedColumnTracksList(const ExplicitGridTrackSizes& explicitGridTrackSizes, LeadingImplicitTracks leadingImplicitTracks, const GridAreas& gridAreas)
{
    ASSERT(explicitGridTrackSizes.columnAutoRepeatTracksIndexes);
    auto columnSpans = gridAreas.map([](auto& gridArea) {
        return WTF::Range<size_t> { gridArea.second.columnStartLine, gridArea.second.columnEndLine };
    });
    return collapsedTracksList(explicitGridTrackSizes.columnAutoRepeatTracksIndexes, leadingImplicitTracks.columnsCount, columnSpans);
}

static BitVector collapsedRowTracksList(const ExplicitGridTrackSizes& explicitGridTrackSizes, LeadingImplicitTracks leadingImplicitTracks, const GridAreas& gridAreas)
{
    ASSERT(explicitGridTrackSizes.rowAutoRepeatTracksIndexes);
    auto rowSpans = gridAreas.map([](auto& gridArea) {
        return WTF::Range<size_t> { gridArea.second.rowStartLine, gridArea.second.rowEndLine };
    });
    return collapsedTracksList(explicitGridTrackSizes.rowAutoRepeatTracksIndexes, leadingImplicitTracks.rowsCount, rowSpans);
}

GridLayoutResult GridFormattingContext::layout(GridLayoutConstraints layoutConstraints)
{
    auto logicalGridItems = constructLogicalGridItems(root());
    CheckedRef gridStyle = root().style();
    auto usedColumnGap = usedGapValue(gridStyle->columnGap(), gridStyle);
    auto usedRowGap = usedGapValue(gridStyle->rowGap(), gridStyle);
    auto explicitGridTrackSizes = ExplicitGridResolver::resolve(gridStyle, autoRepeatConstraint(layoutConstraints.inlineAxis), autoRepeatConstraint(layoutConstraints.blockAxis), usedColumnGap, usedRowGap);
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

    if (inlineAxisDependsOnTracks)
        explicitGridTrackSizes.columnTrackSizes = trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.columnTrackSizes);
    // The rows as specified are kept for the second grid sizing pass, which resolves their percentages.
    Vector<Style::GridTrackSize> specifiedRowTrackSizes;
    if (blockAxisDependsOnTracks)
        specifiedRowTrackSizes = std::exchange(explicitGridTrackSizes.rowTrackSizes, trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.rowTrackSizes));

    auto gridAutoColumns = inlineAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoColumns()) : gridStyle->gridAutoColumns();
    auto gridAutoRows = blockAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoRows()) : gridStyle->gridAutoRows();

    GridDefinition gridDefinition { WTF::move(explicitGridTrackSizes), gridAutoColumns, gridAutoRows, autoFlowOptions, gridStyle->usedZoomForLength() };

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    // https://drafts.csswg.org/css-grid-1/#layout-algorithm
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    auto gridItemPlacementResult = GridItemPlacer { autoFlowOptions }.placeItems(logicalGridItems, leadingImplicitTracks, gridDefinition.explicitGridTrackSizes.columnsCount(), gridDefinition.explicitGridTrackSizes.rowsCount());

    auto& gridAreas = gridItemPlacementResult.gridAreas;
    auto collapsedColumnTracks = gridStyle->gridTemplateColumns().autoRepeatType == AutoRepeatType::Fit ? collapsedColumnTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };
    auto collapsedRowTracks = gridStyle->gridTemplateRows().autoRepeatType == AutoRepeatType::Fit ? collapsedRowTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };
    GridLayoutState layoutState { layoutConstraints, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap, WTF::move(collapsedColumnTracks), WTF::move(collapsedRowTracks) };

    auto placedGridItems = constructPlacedGridItems(gridAreas);
    auto columnTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.columnsCount, leadingImplicitTracks.columnsCount, gridDefinition.explicitGridTrackSizes.columnTrackSizes, gridDefinition.gridAutoColumns, layoutState.collapsedColumnTracksList, gridDefinition.zoom);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.rowsCount, leadingImplicitTracks.rowsCount, gridDefinition.explicitGridTrackSizes.rowTrackSizes, gridDefinition.gridAutoRows, layoutState.collapsedRowTracksList, gridDefinition.zoom);

    // 2. Find the size of the grid container.
    auto usedTrackSizes = GridSizer { *this, layoutState }.sizeGrid(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);
    auto usedBlockContentSize = [&] {
        auto& blockAxisConstraint = layoutConstraints.blockAxis;
        if (blockAxisConstraint.scenario() == AxisConstraint::FreeSpaceScenario::Definite)
            return blockAxisConstraint.availableSpace();

        auto& rowSizes = usedTrackSizes.rowSizes;
        auto blockContentSize = std::reduce(rowSizes.begin(), rowSizes.end()) + GridLayoutUtils::totalGuttersSize(rowSizes.size(), layoutState.collapsedRowTracksList, layoutState.usedRowGap);
        if (auto containerMaximumSize = blockAxisConstraint.containerMaximumSize())
            blockContentSize = std::min(blockContentSize, *containerMaximumSize);
        if (auto containerMinimumSize = blockAxisConstraint.containerMinimumSize())
            blockContentSize = std::max(blockContentSize, *containerMinimumSize);
        return blockContentSize;
    }();

    // 3. Given the resulting grid container size, run the Grid Sizing Algorithm to size the grid.
    bool rowsHadPercentages = blockAxisDependsOnTracks
        && (hasPercentageTrackSize(specifiedRowTrackSizes) || hasPercentageTrackSize(gridStyle->gridAutoRows()));
    // Flexible rows can size differently against the grid container's now definite block size than
    // against indefinite free space, e.g. when their flex factors sum to less than 1.
    bool hasFlexibleRows = blockAxisDependsOnTracks && rowTrackSizingFunctionsList.containsIf([](auto& trackSizingFunctions) {
        return trackSizingFunctions.max.isFlex();
    });
    if (rowsHadPercentages || hasFlexibleRows) {
        auto resolvedRowTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.rowsCount, leadingImplicitTracks.rowsCount, specifiedRowTrackSizes, gridStyle->gridAutoRows(), layoutState.collapsedRowTracksList, gridDefinition.zoom);
        auto& blockAxisConstraint = layoutConstraints.blockAxis;
        GridLayoutConstraints layoutConstraintsForGridSizing {
            layoutConstraints.inlineAxis,
            AxisConstraint::definite(usedBlockContentSize, blockAxisConstraint.containerMinimumSize(), blockAxisConstraint.containerMaximumSize())
        };
        GridLayoutState layoutStateForGridSizing { layoutConstraintsForGridSizing, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap, layoutState.collapsedColumnTracksList, layoutState.collapsedRowTracksList };
        usedTrackSizes = GridSizer { *this, layoutStateForGridSizing }.sizeGrid(placedGridItems, columnTrackSizingFunctionsList, resolvedRowTrackSizingFunctionsList);
        rowTrackSizingFunctionsList = WTF::move(resolvedRowTrackSizingFunctionsList);
    }

    // 4. Lay out the grid items into their respective containing blocks. Each grid area’s
    // width and height are considered definite for this purpose.
    auto gridItemRects = GridLayout { *this }.layout(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList, usedTrackSizes, layoutState);

    // Grid layout positions each item within its containing block which is the grid area.
    // Here we translate it to the coordinate space of the grid.
    auto mapGridItemLocationsToGrid = [&] {

        for (auto& gridItemRect : gridItemRects) {
            auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
            auto columnPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.columnStartLine, usedTrackSizes.columnSizes, layoutState.collapsedColumnTracksList, layoutState.usedColumnGap);
            auto rowPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.rowStartLine, usedTrackSizes.rowSizes, layoutState.collapsedRowTracksList, layoutState.usedRowGap);

            gridItemRect.borderBoxRect.moveBy(LayoutPoint { columnPosition, rowPosition });
        }
    };
    mapGridItemLocationsToGrid();
    setGridItemGeometries(gridItemRects);

    auto autoRepeatColumnsCount = gridDefinition.explicitGridTrackSizes.columnAutoRepeatTracksIndexes.distance();
    auto autoRepeatRowsCount = gridDefinition.explicitGridTrackSizes.rowAutoRepeatTracksIndexes.distance();
    return { WTF::move(usedTrackSizes), WTF::move(gridItemRects), usedBlockContentSize,
        autoRepeatColumnsCount, autoRepeatRowsCount,
        leadingImplicitTracks.columnsCount, leadingImplicitTracks.rowsCount };
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
GridFormattingContext::IntrinsicWidths GridFormattingContext::computeIntrinsicWidths(const AutoRepeatConstraint& inlineAxisAutoRepeatConstraint, const AutoRepeatConstraint& blockAxisAutoRepeatConstraint)
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
    auto usedColumnGap = usedGapValue(gridStyle->columnGap(), gridStyle);
    auto usedRowGap = usedGapValue(gridStyle->rowGap(), gridStyle);
    GridDefinition gridDefinition {
        explicitGridTrackSizesForIntrinsicWidths(gridStyle, inlineAxisAutoRepeatConstraint, blockAxisAutoRepeatConstraint, usedColumnGap, usedRowGap),
        gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoColumns()),
        gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoRows()),
        autoFlowOptions,
        gridStyle->usedZoomForLength(),
    };

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    auto logicalGridItems = constructLogicalGridItems(root());
    auto leadingImplicitTracks = computeLeadingImplicitTracks(logicalGridItems, gridDefinition.explicitGridTrackSizes);

    // https://drafts.csswg.org/css-grid-1/#layout-algorithm
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    // The placement does not depend on the axis constraints, so it is shared by both intrinsic sizing scenarios.
    auto gridItemPlacementResult = GridItemPlacer { autoFlowOptions }.placeItems(logicalGridItems, leadingImplicitTracks, gridDefinition.explicitGridTrackSizes.columnsCount(), gridDefinition.explicitGridTrackSizes.rowsCount());
    auto& gridAreas = gridItemPlacementResult.gridAreas;
    auto collapsedColumnTracks = gridStyle->gridTemplateColumns().autoRepeatType == AutoRepeatType::Fit ? collapsedColumnTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };
    auto collapsedRowTracks = gridStyle->gridTemplateRows().autoRepeatType == AutoRepeatType::Fit ? collapsedRowTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };

    auto columnSizesForConstraint = [&](AxisConstraint intrinsicConstraint) -> TrackSizes {
        GridLayoutConstraints layoutConstraints {
            .inlineAxis = intrinsicConstraint,
            .blockAxis = intrinsicConstraint
        };
        GridLayoutState layoutState { layoutConstraints, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap, collapsedColumnTracks, collapsedRowTracks };

        auto placedGridItems = constructPlacedGridItems(gridAreas);
        auto columnTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.columnsCount, leadingImplicitTracks.columnsCount, gridDefinition.explicitGridTrackSizes.columnTrackSizes, gridDefinition.gridAutoColumns, collapsedColumnTracks, gridDefinition.zoom);
        auto rowTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.rowsCount, leadingImplicitTracks.rowsCount, gridDefinition.explicitGridTrackSizes.rowTrackSizes, gridDefinition.gridAutoRows, collapsedRowTracks, gridDefinition.zoom);

        // When no grid item's inline contribution depends on its own block size, the column sizes are
        // final after step 1 of the grid sizing algorithm, so run that step alone.
        if (m_intrinsicWidthSizingPath == IntrinsicWidthSizingPath::ColumnsOnly)
            return GridSizer { *this, layoutState }.sizeColumnsOnlyFastPath(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);

        auto usedTrackSizes = GridSizer { *this, layoutState }.sizeGrid(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList);
        // FIXME: Laying out the grid items cannot change the column sizes, so this is only here to
        // preserve the side effects of laying them out during intrinsic sizing.
        GridLayout { *this }.layout(placedGridItems, columnTrackSizingFunctionsList, rowTrackSizingFunctionsList, usedTrackSizes, layoutState);
        return WTF::move(usedTrackSizes.columnSizes);
    };

    TrackSizes minContentColumnSizes = columnSizesForConstraint(AxisConstraint::minContent());
    TrackSizes maxContentColumnSizes = columnSizesForConstraint(AxisConstraint::maxContent());

    // Sum track sizes and add gaps
    auto computeIntrinsicWidth = [&](const TrackSizes& trackSizes) -> LayoutUnit {
        auto sumOfTrackSizes = 0_lu;
        for (auto trackSize : trackSizes)
            sumOfTrackSizes += trackSize;
        return sumOfTrackSizes + GridLayoutUtils::totalGuttersSize(trackSizes.size(), collapsedColumnTracks, usedColumnGap);
    };

    return IntrinsicWidths {
        .minimum = computeIntrinsicWidth(minContentColumnSizes),
        .maximum = computeIntrinsicWidth(maxContentColumnSizes)
    };
}

} // namespace Layout
} // namespace WebCore

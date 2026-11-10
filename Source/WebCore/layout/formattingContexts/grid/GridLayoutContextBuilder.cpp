/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "GridLayoutContextBuilder.h"

#include "ExplicitGridResolver.h"
#include "GridLayoutUtils.h"
#include "GridLineResolution.h"
#include "LayoutChildIterator.h"
#include "NamedGridLines.h"
#include "NamedLineResolver.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

static LogicalGridItems constructLogicalGridItems(const ElementBox& gridBox)
{
    LogicalGridItems logicalGridItems;
    for (CheckedRef gridItem : childrenOfType<ElementBox>(gridBox)) {
        if (gridItem->isOutOfFlowPositioned() || gridItem->isExcludedFromGridFormattingContextForIntegration())
            continue;

        logicalGridItems.append(gridItem);
    }

    std::ranges::stable_sort(logicalGridItems, { }, [](auto& gridItem) {
        return gridItem->style().order().value;
    });
    return logicalGridItems;
}

// https://drafts.csswg.org/css-grid-1/#gutters
// https://drafts.csswg.org/css-gaps-1/#gap-percent
// Only fixed lengths are handled: a percentage gap resolves against the content box in its axis, which is not known here.
static LayoutUnit resolveGap(const Style::GapGutter& gap, const Style::ComputedStyle& style)
{
    if (gap.isNormal())
        return { };

    if (auto fixedGap = gap.tryFixed())
        return Style::evaluate<LayoutUnit>(*fixedGap, 0_lu, style.usedZoomForLength());

    ASSERT_NOT_REACHED();
    return { };
}

// https://drafts.csswg.org/css-grid-1/#overlarge-grids
// The UA-defined limit on the number of tracks. It is the one Style used to clamp line numbers and repeat() counts when it built
// the style, so the layout and the style agree on it. Read once per layout and passed down by value.
static size_t maxGridTracks()
{
    return static_cast<size_t>(Style::GridPosition::max());
}

static LeadingImplicitTracks computeLeadingImplicitTracks(const LogicalGridItems& logicalGridItems, const NamedLineResolver& columnNamedLineResolver, const NamedLineResolver& rowNamedLineResolver, size_t maxTracks)
{
    // Negative line placements are resolved against the explicit grid track count, which can still
    // produce a negative line when the placement counts past the start edge of the explicit grid.
    // Those are normalized by shifting every line forward by the magnitude of the most-negative
    // resolved line, so e.g. with 3 explicit columns a column-start of -5 resolves to line -1,
    // which shifts all column lines forward by 1 and maps to matrix column 0. That magnitude is
    // also the number of leading implicit tracks the grid needs to generate.
    int minimumColumnLine = 0;
    int minimumRowLine = 0;
    for (auto& gridItem : logicalGridItems) {
        CheckedRef gridItemStyle = gridItem->style();
        if (auto columnRange = resolveDefinitePosition(gridItemStyle->gridItemColumnStart(), gridItemStyle->gridItemColumnEnd(), columnNamedLineResolver, maxTracks)) {
            auto [startLine, endLine] = *columnRange;
            minimumColumnLine = std::min({ minimumColumnLine, startLine, endLine });
        }
        if (auto rowRange = resolveDefinitePosition(gridItemStyle->gridItemRowStart(), gridItemStyle->gridItemRowEnd(), rowNamedLineResolver, maxTracks)) {
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

static TrackSizingFunctions convertGridTrackSizeToTrackSizingFunctions(const Style::GridTrackSize& gridTrackSize, const Style::ZoomFactor& zoom)
{
    auto minTrackSizingFunction = [&] {
        // If the track was sized with a minmax() function, this is the first argument to that function.
        if (gridTrackSize.isMinMax())
            return gridTrackSize.minTrackBreadth();

        // If the track was sized with a <flex> value or fit-content() function, auto.
        if (gridTrackSize.isFitContent() || gridTrackSize.minTrackBreadth().isFlex())
            return Style::GridTrackBreadth { CSS::Keyword::Auto { } };

        // Otherwise, the track's sizing function.
        return gridTrackSize.minTrackBreadth();
    };

    auto maxTrackSizingFunction = [&] -> MaxTrackSizingFunction {
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
    TrackSizingFunctionsList trackSizingFunctionsForImplicitGrid(ReserveCapacityTag { }, implicitTracksCount);

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

    TrackSizingFunctionsList trackSizingFunctions(ReserveCapacityTag { }, totalTracksCount);

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
    // The grid size limit leaves no auto-fit tracks when the tracks before the repeat() already reach it.
    if (!autoFitTracksIndexes)
        return { };

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
    auto columnSpans = gridAreas.map([](auto& gridArea) {
        return WTF::Range<size_t> { gridArea.second.columnStartLine, gridArea.second.columnEndLine };
    });
    return collapsedTracksList(explicitGridTrackSizes.columnAutoRepeatTracksIndexes, leadingImplicitTracks.columnsCount, columnSpans);
}

static BitVector collapsedRowTracksList(const ExplicitGridTrackSizes& explicitGridTrackSizes, LeadingImplicitTracks leadingImplicitTracks, const GridAreas& gridAreas)
{
    auto rowSpans = gridAreas.map([](auto& gridArea) {
        return WTF::Range<size_t> { gridArea.second.rowStartLine, gridArea.second.rowEndLine };
    });
    return collapsedTracksList(explicitGridTrackSizes.rowAutoRepeatTracksIndexes, leadingImplicitTracks.rowsCount, rowSpans);
}

GridLayoutContextBuilder::GridLayoutContextBuilder(const ElementBox& gridBox)
    : m_gridBox(gridBox)
{
}

GridLayoutContext GridLayoutContextBuilder::build(const GridLayoutContextInput& input) const
{
    auto logicalGridItems = constructLogicalGridItems(m_gridBox);
    CheckedRef gridStyle = m_gridBox->style();
    auto usedColumnGap = resolveGap(gridStyle->columnGap(), gridStyle);
    auto usedRowGap = resolveGap(gridStyle->rowGap(), gridStyle);
    auto maxTracks = maxGridTracks();
    auto explicitGridTrackSizes = ExplicitGridResolver::resolve(gridStyle, input.inlineAxis.autoRepeat, input.blockAxis.autoRepeat, usedColumnGap, usedRowGap, maxTracks);
    auto namedColumnLines = NamedGridLines::create(gridStyle->gridTemplateColumns(), explicitGridTrackSizes.columnAutoRepeatTracksIndexes.distance(), explicitGridTrackSizes.columnAutoRepeatTruncatedByLimit, explicitGridTrackSizes.columnsCount());
    auto namedRowLines = NamedGridLines::create(gridStyle->gridTemplateRows(), explicitGridTrackSizes.rowAutoRepeatTracksIndexes.distance(), explicitGridTrackSizes.rowAutoRepeatTruncatedByLimit, explicitGridTrackSizes.rowsCount());
    // The resolvers refer to the NamedGridLines above, which stay alive until the end of the function.
    NamedLineResolver columnNamedLineResolver { namedColumnLines, explicitGridTrackSizes.columnsCount() };
    NamedLineResolver rowNamedLineResolver { namedRowLines, explicitGridTrackSizes.rowsCount() };
    auto leadingImplicitTracks = computeLeadingImplicitTracks(logicalGridItems, columnNamedLineResolver, rowNamedLineResolver, maxTracks);

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
    auto inlineAxisDependsOnTracks = input.inlineAxis.containerSizeDependsOnTracks;
    auto blockAxisDependsOnTracks = input.blockAxis.containerSizeDependsOnTracks;

    if (inlineAxisDependsOnTracks)
        explicitGridTrackSizes.columnTrackSizes = trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.columnTrackSizes);
    // The rows as specified are kept for the second grid sizing pass, which resolves their percentages.
    std::optional<GridLayoutContext::SpecifiedRows> specifiedRows;
    if (blockAxisDependsOnTracks) {
        auto convertedRowTrackSizes = trackSizesWithPercentagesConvertedToAuto(explicitGridTrackSizes.rowTrackSizes);
        if (input.keepSpecifiedRowTrackSizes) {
            auto hasPercentages = hasPercentageTrackSize(explicitGridTrackSizes.rowTrackSizes) || hasPercentageTrackSize(gridStyle->gridAutoRows());
            specifiedRows = GridLayoutContext::SpecifiedRows { std::exchange(explicitGridTrackSizes.rowTrackSizes, WTF::move(convertedRowTrackSizes)), gridStyle->gridAutoRows(), hasPercentages };
        } else
            explicitGridTrackSizes.rowTrackSizes = WTF::move(convertedRowTrackSizes);
    }

    auto gridAutoColumns = inlineAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoColumns()) : gridStyle->gridAutoColumns();
    auto gridAutoRows = blockAxisDependsOnTracks ? gridAutoTrackSizesWithPercentagesConvertedToAuto(gridStyle->gridAutoRows()) : gridStyle->gridAutoRows();

    GridDefinition gridDefinition { WTF::move(explicitGridTrackSizes), gridAutoColumns, gridAutoRows, autoFlowOptions, gridStyle->usedZoomForLength() };

    // https://drafts.csswg.org/css-grid-1/#layout-algorithm
    // 1. Run the Grid Item Placement Algorithm to resolve the placement of all grid items in the grid.
    // The placement does not depend on the axis constraints.
    auto gridItemPlacementResult = GridItemPlacer { autoFlowOptions }.placeItems(logicalGridItems, leadingImplicitTracks, columnNamedLineResolver, rowNamedLineResolver, maxTracks);

    auto& gridAreas = gridItemPlacementResult.gridAreas;
    auto collapsedColumnTracks = gridStyle->gridTemplateColumns().autoRepeatType == AutoRepeatType::Fit ? collapsedColumnTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };
    auto collapsedRowTracks = gridStyle->gridTemplateRows().autoRepeatType == AutoRepeatType::Fit ? collapsedRowTracksList(gridDefinition.explicitGridTrackSizes, leadingImplicitTracks, gridAreas) : BitVector { };

    auto placedGridItems = constructPlacedGridItems(gridAreas);
    auto columnTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.columnsCount, leadingImplicitTracks.columnsCount, gridDefinition.explicitGridTrackSizes.columnTrackSizes, gridDefinition.gridAutoColumns, collapsedColumnTracks, gridDefinition.zoom);
    auto rowTrackSizingFunctionsList = trackSizingFunctions(gridItemPlacementResult.rowsCount, leadingImplicitTracks.rowsCount, gridDefinition.explicitGridTrackSizes.rowTrackSizes, gridDefinition.gridAutoRows, collapsedRowTracks, gridDefinition.zoom);

    // The track sizing algorithm and GridLayoutUtils resolve no percentages in an axis whose container size depends on its tracks.
    ASSERT(!inlineAxisDependsOnTracks || (!hasPercentageTrackSize(gridDefinition.explicitGridTrackSizes.columnTrackSizes) && !hasPercentageTrackSize(gridDefinition.gridAutoColumns)));
    ASSERT(!blockAxisDependsOnTracks || (!hasPercentageTrackSize(gridDefinition.explicitGridTrackSizes.rowTrackSizes) && !hasPercentageTrackSize(gridDefinition.gridAutoRows)));

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    return {
        WTF::move(logicalGridItems),
        WTF::move(gridDefinition),
        leadingImplicitTracks,
        WTF::move(gridItemPlacementResult),
        WTF::move(placedGridItems),
        usedColumnGap,
        usedRowGap,
        usedJustifyContent,
        usedAlignContent,
        WTF::move(collapsedColumnTracks),
        WTF::move(collapsedRowTracks),
        WTF::move(columnTrackSizingFunctionsList),
        WTF::move(rowTrackSizingFunctionsList),
        WTF::move(specifiedRows)
    };
}

PlacedGridItems GridLayoutContextBuilder::constructPlacedGridItems(const GridAreas& gridAreas) const
{
    PlacedGridItems placedGridItems(ReserveCapacityTag { }, gridAreas.size());
    CheckedRef gridContainerStyle = m_gridBox->style();
    for (auto& [ unplacedGridItem, gridAreaLines ] : gridAreas) {
        CheckedRef gridItem = unplacedGridItem.layoutBox();
        placedGridItems.constructAndAppend(gridItem, gridAreaLines, gridContainerStyle);
    }
    return placedGridItems;
}

TrackSizingFunctionsList GridLayoutContext::rowTrackSizingFunctionsAsSpecified() const
{
    ASSERT(specifiedRows);
    return trackSizingFunctions(placementResult.rowsCount, leadingImplicitTracks.rowsCount, specifiedRows->trackSizes, specifiedRows->autoRows, collapsedRowTracksList, gridDefinition.zoom);
}

} // namespace Layout
} // namespace WebCore

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
#include "GridLayoutState.h"
#include "GridLayoutUtils.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "RenderStyle+GettersInlines.h"
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
{
}

UnplacedGridItems GridFormattingContext::constructUnplacedGridItems() const
{
    struct GridItem {
        CheckedRef<const ElementBox> layoutBox;
        int order;
    };

    Vector<GridItem> gridItems;
    for (CheckedRef gridItem : childrenOfType<ElementBox>(m_gridBox)) {
        if (gridItem->isOutOfFlowPositioned())
            continue;

        gridItems.append({ gridItem, gridItem->style().order().value });
    }

    std::ranges::stable_sort(gridItems, { }, &GridItem::order);

    UnplacedGridItems unplacedGridItems;
    for (auto& gridItem : gridItems) {
        CheckedRef gridItemStyle = gridItem.layoutBox->style();

        auto gridItemColumnStart = gridItemStyle->gridItemColumnStart();
        auto gridItemColumnEnd = gridItemStyle->gridItemColumnEnd();
        auto gridItemRowStart = gridItemStyle->gridItemRowStart();
        auto gridItemRowEnd = gridItemStyle->gridItemRowEnd();

        UnplacedGridItem unplacedGridItem {
            gridItem.layoutBox,
            gridItemColumnStart,
            gridItemColumnEnd,
            gridItemRowStart,
            gridItemRowEnd
        };

        // Check if this item is fully explicitly positioned
        bool fullyExplicitlyPositionedItem = gridItemColumnStart.isExplicit()
            && gridItemColumnEnd.isExplicit()
            && gridItemRowStart.isExplicit()
            && gridItemRowEnd.isExplicit();

        // FIXME: support definite row/column positioning
        // We should place items with definite row or column positions
        // but currently we only support fully explicitly positioned items.
        // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo
        if (fullyExplicitlyPositionedItem) {
            unplacedGridItems.nonAutoPositionedItems.append(unplacedGridItem);
        } else if (unplacedGridItem.hasDefiniteRowPosition()) {
            unplacedGridItems.definiteRowPositionedItems.append(unplacedGridItem);
        } else {
            unplacedGridItems.autoPositionedItems.append(unplacedGridItem);
        }
    }
    return unplacedGridItems;
}

static Style::GridTrackSize trackSizeWithPercentagesConvertedToAuto(const Style::GridTrackSize& trackSize)
{
    return WTF::switchOn(trackSize,
        [&trackSize](const Style::GridTrackBreadth& breadth) {
            if (breadth.isPercentOrCalculated())
                return Style::GridTrackSize { CSS::Keyword::Auto { } };
            return trackSize;
        },
        [&trackSize](const Style::GridTrackSize::FitContent& fitContent) {
            if (fitContent->isPercentOrCalculated())
                return Style::GridTrackSize { CSS::Keyword::Auto { } };
            return trackSize;
        },
        [&trackSize](const Style::GridTrackBreadth::Flex&) {
            return trackSize;
        },
        [](const Style::GridTrackSize::MinMax& minMax) {
            auto minTrackSizingFunction = !minMax->min.isPercentOrCalculated() ? minMax->min : Style::GridTrackBreadth { CSS::Keyword::Auto { } };
            auto maxTrackSizingFunction = !minMax->max.isPercentOrCalculated() ? minMax->max : Style::GridTrackBreadth { CSS::Keyword::Auto { } };
            return Style::GridTrackSize { Style::GridTrackSize::MinMax { minTrackSizingFunction, maxTrackSizingFunction } };
        });
}

static Style::RepeatTrackList repeatTrackListWithPercentagesConvertedToAuto(const Style::RepeatTrackList& repeatList)
{
    return repeatList.map([](const Style::RepeatEntry& entry) {
        return WTF::switchOn(entry,
            [](const Style::GridTrackSize& trackSize) -> Style::RepeatEntry {
                return trackSizeWithPercentagesConvertedToAuto(trackSize);
            },
            [](const Vector<String>& lineNames) -> Style::RepeatEntry {
                return lineNames;
            });
    });
}

static Style::GridTemplateList gridTemplateListWithPercentagesConvertedToAuto(const Style::GridTemplateList& computedGridTemplateList)
{
    Style::GridTrackList transformedList = computedGridTemplateList.list.map([](const Style::GridTrackEntry& entry) {
        return WTF::switchOn(entry,
            [](const Style::GridTrackSize& trackSize) -> Style::GridTrackEntry {
                return trackSizeWithPercentagesConvertedToAuto(trackSize);
            },
            [](const Vector<String>& lineNames) -> Style::GridTrackEntry {
                return lineNames;
            },
            [](const Style::GridTrackEntryRepeat& repeat) -> Style::GridTrackEntry {
                return Style::GridTrackEntryRepeat { repeat.repeats, repeatTrackListWithPercentagesConvertedToAuto(repeat.list) };
            },
            [](const Style::GridTrackEntryAutoRepeat& autoRepeat) -> Style::GridTrackEntry {
                return Style::GridTrackEntryAutoRepeat { autoRepeat.type, repeatTrackListWithPercentagesConvertedToAuto(autoRepeat.list) };
            },
            [](const Style::GridTrackEntrySubgrid& subgrid) -> Style::GridTrackEntry {
                return subgrid;
            });
    });
    return Style::GridTemplateList { WTF::move(transformedList) };
}

UsedTrackSizes GridFormattingContext::layout(GridLayoutConstraints layoutConstraints)
{
    auto unplacedGridItems = constructUnplacedGridItems();
    CheckedRef gridStyle = root().style();

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

    auto gridTemplateColumns = inlineAxisDependsOnTracks ? gridTemplateListWithPercentagesConvertedToAuto(gridStyle->gridTemplateColumns()) : gridStyle->gridTemplateColumns();
    auto gridTemplateRows = blockAxisDependsOnTracks ? gridTemplateListWithPercentagesConvertedToAuto(gridStyle->gridTemplateRows()) : gridStyle->gridTemplateRows();

    GridDefinition gridDefinition { gridTemplateColumns, gridTemplateRows, gridStyle->gridAutoColumns(), gridStyle->gridAutoRows(), autoFlowOptions };

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    GridLayoutState layoutState { layoutConstraints, gridDefinition, usedJustifyContent, usedAlignContent, usedGapValue(gridStyle->columnGap()), usedGapValue(gridStyle->rowGap()) };

    auto [ usedTrackSizes, gridItemRects ] = GridLayout { *this }.layout(unplacedGridItems, layoutState);

    // Grid layout positions each item within its containing block which is the grid area.
    // Here we translate it to the coordinate space of the grid.
    auto mapGridItemLocationsToGrid = [&] {

        for (auto& gridItemRect : gridItemRects) {
            auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
            auto columnPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.columnStartLine, usedTrackSizes.columnSizes, layoutState.usedColumnGap);
            auto rowPosition = GridLayoutUtils::computeGridLinePosition(lineNumbersForGridArea.rowStartLine, usedTrackSizes.rowSizes, layoutState.usedRowGap);

            gridItemRect.borderBoxRect.moveBy({ columnPosition, rowPosition });
        }
    };
    mapGridItemLocationsToGrid();
    setGridItemGeometries(gridItemRects);
    return usedTrackSizes;
}

PlacedGridItems GridFormattingContext::constructPlacedGridItems(const GridAreas& gridAreas) const
{
    PlacedGridItems placedGridItems;
    placedGridItems.reserveInitialCapacity(gridAreas.size());
    CheckedRef formattingContextStyle = root().style();
    for (auto [ unplacedGridItem, gridAreaLines ] : gridAreas) {
        CheckedRef gridItem = unplacedGridItem.m_layoutBox;
        CheckedRef gridContainerStyle = this->gridContainerStyle();
        auto& boxGeometry = geometryForGridItem(gridItem);
        placedGridItems.constructAndAppend(gridItem, gridAreaLines, boxGeometry, gridContainerStyle);
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

// https://drafts.csswg.org/css-grid-1/#intrinsic-sizes
// The max-content size (min-content size) of a grid container is the sum of
// the grid container's track sizes (including gutters) in the appropriate axis,
// when the grid is sized under a max-content constraint (min-content constraint).
GridFormattingContext::IntrinsicWidths GridFormattingContext::computeIntrinsicWidths()
{
    auto unplacedGridItems = constructUnplacedGridItems();

    CheckedRef gridStyle = root().style();
    GridAutoFlowOptions autoFlowOptions {
        .strategy = gridStyle->gridAutoFlow().isDense() ? PackingStrategy::Dense : PackingStrategy::Sparse,
        .direction = gridStyle->gridAutoFlow().isRow() ? GridAutoFlowDirection::Row : GridAutoFlowDirection::Column
    };

    // https://drafts.csswg.org/css-grid-1/#track-sizes
    // For intrinsic sizing, percentages in track sizes must be treated as auto
    GridDefinition gridDefinition {
        gridTemplateListWithPercentagesConvertedToAuto(gridStyle->gridTemplateColumns()),
        gridTemplateListWithPercentagesConvertedToAuto(gridStyle->gridTemplateRows()),
        gridStyle->gridAutoColumns(),
        gridStyle->gridAutoRows(),
        autoFlowOptions
    };

    // Clone items for second pass since layout() consumes them
    auto unplacedGridItemsForMaxContent = unplacedGridItems;

    auto usedJustifyContent = gridStyle->justifyContent().resolve();
    auto usedAlignContent = gridStyle->alignContent().resolve();

    auto usedColumnGap = usedGapValue(gridStyle->columnGap());
    auto usedRowGap = usedGapValue(gridStyle->rowGap());

    // Compute min-content width by running the full grid sizing algorithm with MinContent scenario
    GridLayoutConstraints minContentConstraints {
        .inlineAxis = AxisConstraint::minContent(),
        .blockAxis = AxisConstraint::minContent()
    };
    GridLayoutState minContentLayoutState { minContentConstraints, gridDefinition, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap };
    auto [minContentTrackSizes, minContentGridItemRects] = GridLayout { *this }.layout(unplacedGridItems, minContentLayoutState);
    UNUSED_PARAM(minContentGridItemRects);

    // Compute max-content width by running the full grid sizing algorithm with MaxContent scenario
    GridLayoutConstraints maxContentConstraints {
        .inlineAxis = AxisConstraint::maxContent(),
        .blockAxis = AxisConstraint::maxContent()
    };
    GridLayoutState maxContentLayoutState { maxContentConstraints, gridDefinition, usedJustifyContent, usedAlignContent, usedColumnGap, usedRowGap };
    auto [maxContentTrackSizes, maxContentGridItemRects] = GridLayout { *this }.layout(unplacedGridItemsForMaxContent, maxContentLayoutState);
    UNUSED_PARAM(maxContentGridItemRects);

    // Sum track sizes and add gaps
    auto computeIntrinsicWidth = [&](const TrackSizes& trackSizes) -> LayoutUnit {
        auto sumOfTrackSizes = 0_lu;
        for (auto trackSize : trackSizes)
            sumOfTrackSizes += trackSize;
        auto totalGutters = trackSizes.size() > 1 ? usedColumnGap * LayoutUnit(trackSizes.size() - 1) : 0_lu;
        return sumOfTrackSizes + totalGutters;
    };

    return IntrinsicWidths {
        .minimum = computeIntrinsicWidth(minContentTrackSizes.columnSizes),
        .maximum = computeIntrinsicWidth(maxContentTrackSizes.columnSizes)
    };
}

} // namespace Layout
} // namespace WebCore

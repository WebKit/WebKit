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
#include "LayoutIntegrationGridLayout.h"

#include "FormattingContextBoxIterator.h"
#include "GridFormattingContext.h"
#include "GridItemRect.h"
#include "GridLayoutConstraints.h"
#include "GridLayoutUtils.h"
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationBoxTreeUpdater.h"
#include "LocalFrameViewInlines.h"
#include "RenderChildIterator.h"
#include "RenderGrid.h"
#include "RenderView.h"
#include "StyleGridPositionsResolver.h"
#include "StyleKeyword+Logging.h"
#include "UsedTrackSizes.h"
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

namespace LayoutIntegration {

GridLayout::GridLayout(RenderGrid& renderGrid)
    : m_gridBox(&renderGrid.layoutBox())
    , m_layoutState(renderGrid.view().layoutState())
{
    gridBox().setIsGridIntegrationRoot(true);
    // The rendered legend of a fieldset sits in the border and is not a grid item, so the grid formatting context leaves it alone.
    // BoxTreeUpdater::tearDown() clears this.
    if (CheckedPtr renderedLegend = renderGrid.renderedFieldsetLegend())
        renderedLegend->layoutBox().setIsExcludedFromGridFormattingContextForIntegration(true);
}

GridLayout::~GridLayout()
{
    BoxTreeUpdater { protect(gridBoxRenderer()) }.tearDown();
    gridBox().setIsGridIntegrationRoot(false);
}

void GridLayout::updateFormattingContextGeometries()
{
    auto boxGeometryUpdater = BoxGeometryUpdater { protect(layoutState()), gridBox() };

    // Percentage padding and margin resolve against the same basis RenderBox uses for the grid container's own box model values.
    // That is the grid area for a grid item, the padding box for an out-of-flow box, and the containing block's content box otherwise.
    boxGeometryUpdater.setFormattingContextRootGeometry(CheckedRef { gridBoxRenderer() }->containingBlockLogicalWidthForContent());
    boxGeometryUpdater.setFormattingContextContentGeometry(CheckedRef { layoutState() }->geometryForBox(gridBox()).contentBoxWidth(), { });
}

static std::optional<LayoutUnit> minimumSizeConstraint(const Style::MinimumSize& computedMinimumSize, std::optional<LayoutUnit> percentageResolutionSize, BoxSizing boxSizing, LayoutUnit borderAndPadding, const Style::ZoomFactor& gridContainerZoom)
{
    auto minimumSize = WTF::switchOn(computedMinimumSize,
        [&gridContainerZoom](const Style::MinimumSize::Fixed& fixedValue) -> std::optional<LayoutUnit> {
            return Style::evaluate<LayoutUnit>(fixedValue, gridContainerZoom);
        },
        [&percentageResolutionSize](const Style::MinimumSize::Percentage& percentageValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize)
                return { };
            return Style::evaluate<LayoutUnit>(percentageValue, *percentageResolutionSize);
        },
        [&percentageResolutionSize, &gridContainerZoom](const Style::MinimumSize::Calc& calcValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize)
                return { };
            return Style::evaluate<LayoutUnit>(calcValue, *percentageResolutionSize, gridContainerZoom);
        },
        [](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            return 0_lu;
        },
        [&percentageResolutionSize, &gridContainerZoom](const Style::MinimumSize::CalcSize& calcSizeValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize && calcSizeValue.hasPercentage())
                return { };
            return Style::evaluate<LayoutUnit>(calcSizeValue, percentageResolutionSize.value_or(0_lu), gridContainerZoom);
        },
        // https://drafts.csswg.org/css-grid-1/#auto-repeat
        // The intrinsic keywords are not definite sizes, so they give no constraint to the number of auto-repeat tracks.
        // RenderBox applies them to the used size once the content size is known. stretch and -webkit-fill-available
        // are resolved by inlineAxisMinMaxSizeConstraints and blockAxisMinMaxSizeConstraints.
        [](const CSS::Keyword::MinContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::MaxContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::FitContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::Intrinsic&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::MinIntrinsic&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> { return { }; }
    );
    return minimumSize.transform([&](LayoutUnit size) {
        if (boxSizing == BoxSizing::BorderBox)
            size -= borderAndPadding;
        return std::max({ }, size);
    });
}

static std::optional<LayoutUnit> maximumSizeConstraint(const Style::MaximumSize& computedMaximumSize, std::optional<LayoutUnit> percentageResolutionSize, BoxSizing boxSizing, LayoutUnit borderAndPadding, const Style::ZoomFactor& gridContainerZoom)
{
    auto maximumSize = WTF::switchOn(computedMaximumSize,
        [&gridContainerZoom](const Style::MaximumSize::Fixed& fixedValue) -> std::optional<LayoutUnit> {
            return Style::evaluate<LayoutUnit>(fixedValue, gridContainerZoom);
        },
        [&percentageResolutionSize](const Style::MaximumSize::Percentage& percentageValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize)
                return { };
            return Style::evaluate<LayoutUnit>(percentageValue, *percentageResolutionSize);
        },
        [&percentageResolutionSize, &gridContainerZoom](const Style::MaximumSize::Calc& calcValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize)
                return { };
            return Style::evaluate<LayoutUnit>(calcValue, *percentageResolutionSize, gridContainerZoom);
        },
        [](const CSS::Keyword::None&) -> std::optional<LayoutUnit> {
            return { };
        },
        [&percentageResolutionSize, &gridContainerZoom](const Style::MaximumSize::CalcSize& calcSizeValue) -> std::optional<LayoutUnit> {
            if (!percentageResolutionSize && calcSizeValue.hasPercentage())
                return { };
            return Style::evaluate<LayoutUnit>(calcSizeValue, percentageResolutionSize.value_or(0_lu), gridContainerZoom);
        },
        // https://drafts.csswg.org/css-grid-1/#auto-repeat
        // The intrinsic keywords are not definite sizes, so they give no constraint to the number of auto-repeat tracks.
        // RenderBox applies them to the used size once the content size is known. stretch and -webkit-fill-available
        // are resolved by inlineAxisMinMaxSizeConstraints and blockAxisMinMaxSizeConstraints.
        [](const CSS::Keyword::MinContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::MaxContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::FitContent&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::Intrinsic&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::MinIntrinsic&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> { return { }; },
        [](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> { return { }; }
    );
    return maximumSize.transform([&](LayoutUnit size) {
        if (boxSizing == BoxSizing::BorderBox)
            size -= borderAndPadding;
        return std::max({ }, size);
    });
}

struct MinMaxSizeConstraints {
    std::optional<LayoutUnit> minimum;
    std::optional<LayoutUnit> maximum;
};

// https://drafts.csswg.org/css-sizing-3/#cyclic-percentage-contribution
// A percentage whose containing block size depends on this box's contribution is cyclic: it is resolved against zero in a minimum
// size and behaves as none in a maximum size. A cyclic stretch size behaves as auto
// (https://drafts.csswg.org/css-sizing-4/#stretch-fit-sizing).
// The containing block's width depends on the grid container when an ancestor is measuring its intrinsic widths and has no specified
// width, as RenderBlock::availableLogicalHeightForPercentageComputation tests with isComputingIntrinsicLogicalWidthFor. A fixed width on the way
// up makes the containing block's width definite whatever its content.
// This is the physical width, so it only applies when the containing block has the grid's writing mode (an orthogonal grid resolves
// against the containing block's block size, see inlineAxisContainingBlockSize). The walk stops at an ancestor whose writing mode differs:
// its physical width is its block size, which is not what isComputingIntrinsicLogicalWidthFor or logicalWidth() describe, and that block
// size is computed by laying out its content, not by measuring intrinsic widths, so no cycle goes through it.
static bool containingBlockInlineSizeDependsOnContent(const RenderGrid& gridContainerRenderer)
{
    CheckedRef layoutContext = gridContainerRenderer.view().frameView().layoutContext();
    for (CheckedPtr ancestor = gridContainerRenderer.containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor->isHorizontalWritingMode() != gridContainerRenderer.isHorizontalWritingMode())
            return false;
        auto& ancestorWidth = ancestor->style().logicalWidth();
        if (ancestorWidth.isFixed())
            return false;
        if (!ancestorWidth.isSpecified() && layoutContext->isComputingIntrinsicLogicalWidthFor(*ancestor))
            return true;
    }
    return false;
}

// The size the grid container's inline-axis percentages and stretch resolve against: what RenderBox::computeLogicalWidth passes as
// containerWidthInInlineDirection. Empty when it is not known yet.
static std::optional<LayoutUnit> inlineAxisContainingBlockSize(const RenderGrid& gridContainerRenderer)
{
    if (gridContainerRenderer.isHorizontalWritingMode() != protect(gridContainerRenderer.containingBlock())->isHorizontalWritingMode()) {
        // The inline axis is the containing block's block axis.
        return gridContainerRenderer.perpendicularContainingBlockLogicalHeight();
    }
    if (gridContainerRenderer.isGridItem()) {
        // containingBlockLogicalWidthForContent() returns 0 for a grid area that is not resolved yet.
        if (auto gridAreaWidth = gridContainerRenderer.gridAreaContentLogicalWidth(); gridAreaWidth && !*gridAreaWidth)
            return { };
    }
    return gridContainerRenderer.containingBlockLogicalWidthForContent();
}

// The grid container's content box minimum and maximum size in the inline axis.
// https://drafts.csswg.org/css-sizing-4/#stretch-fit-sizing
// RenderBox resolves percentages, stretch and -webkit-fill-available of the used width against the containing block (against its
// block size when the containing block is orthogonal). Intrinsic sizing has no used width to fall back to, and there the
// containing block's size can depend on this grid, in which case nothing is resolved.
static MinMaxSizeConstraints inlineAxisMinMaxSizeConstraints(const RenderGrid& gridContainerRenderer, IsComputingIntrinsicSize isComputingIntrinsicSize)
{
    CheckedRef gridContainerStyle = gridContainerRenderer.style();
    auto gridContainerZoom = gridContainerStyle->usedZoomForLength();
    auto boxSizing = gridContainerStyle->boxSizing();

    auto& minWidthStyle = gridContainerStyle->logicalMinWidth();
    auto& maxWidthStyle = gridContainerStyle->logicalMaxWidth();

    // Only a percentage, calc(), stretch or calc-size() resolves against the containing block, so only those can be cyclic.
    auto resolvesAgainstContainingBlock = [](auto& size) {
        return size.isStretch() || size.isPercentOrCalculated() || size.isCalcSize();
    };
    auto containingBlockSize = inlineAxisContainingBlockSize(gridContainerRenderer);
    if (isComputingIntrinsicSize == IsComputingIntrinsicSize::Yes && (resolvesAgainstContainingBlock(minWidthStyle) || resolvesAgainstContainingBlock(maxWidthStyle)) && containingBlockInlineSizeDependsOnContent(gridContainerRenderer))
        containingBlockSize = { };
    auto borderAndPaddingWidth = gridContainerRenderer.borderAndPaddingLogicalWidth();

    auto stretchedContentBoxWidth = [&](auto& size) -> LayoutUnit {
        return std::max(0_lu, gridContainerRenderer.computeSizingKeywordLogicalWidthUsing(size, *containingBlockSize, borderAndPaddingWidth) - borderAndPaddingWidth);
    };
    // Without a known size a percentage in a minimum size resolves against zero, and one in a maximum size is no constraint.
    auto minimumPercentageBasis = containingBlockSize.value_or(0_lu);
    return {
        minWidthStyle.isStretch() ? (containingBlockSize ? std::optional { stretchedContentBoxWidth(minWidthStyle) } : std::nullopt) : minimumSizeConstraint(minWidthStyle, minimumPercentageBasis, boxSizing, borderAndPaddingWidth, gridContainerZoom),
        maxWidthStyle.isStretch() ? (containingBlockSize ? std::optional { stretchedContentBoxWidth(maxWidthStyle) } : std::nullopt) : maximumSizeConstraint(maxWidthStyle, containingBlockSize, boxSizing, borderAndPaddingWidth, gridContainerZoom)
    };
}

// The grid container's content box minimum and maximum size in the block axis, as RenderBox::computeContentLogicalHeight resolves
// them for the used height, with no intrinsic content height.
// Every keyword goes through it with its own value, so that GFC cannot disagree with RenderBox about the box's height: for example,
// -webkit-fill-available subtracts no margins and does not need a definite containing block, unlike stretch (see
// RenderBox::isResolveableStretchSize). The result is already a content box size.
// https://drafts.csswg.org/css-grid-1/#auto-repeat
// The same definite minimum and maximum sizes decide the number of auto-repeat tracks: with a definite maximum size the repetition
// count is the largest that does not overflow, and with a definite minimum size the smallest that fulfills it. This intentionally
// differs from the legacy path, which does not resolve stretch or -webkit-fill-available for the count.
static MinMaxSizeConstraints blockAxisMinMaxSizeConstraints(const RenderGrid& gridContainerRenderer)
{
    CheckedRef gridContainerStyle = gridContainerRenderer.style();
    return {
        gridContainerRenderer.computeContentLogicalHeight(gridContainerStyle->logicalMinHeight(), std::nullopt),
        gridContainerRenderer.computeContentLogicalHeight(gridContainerStyle->logicalMaxHeight(), std::nullopt)
    };
}

static inline Layout::GridLayoutConstraints constraintsForGridContent(const Layout::ElementBox& gridContainer)
{
    CheckedRef gridContainerRenderer = downcast<RenderGrid>(*gridContainer.rendererForIntegration());

    auto availableInlineSpace = [&]() -> LayoutUnit {
        if (auto overridingWidth = gridContainerRenderer->overridingBorderBoxLogicalWidth())
            return gridContainerRenderer->contentBoxLogicalWidth(*overridingWidth);
        return gridContainerRenderer->contentBoxLogicalWidth();
    }();
    auto availableBlockSpace = gridContainerRenderer->availableLogicalHeightForContentBox();

    auto [ minWidth, maxWidth ] = inlineAxisMinMaxSizeConstraints(gridContainerRenderer, IsComputingIntrinsicSize::No);
    auto inlineAxisConstraint = Layout::AxisConstraint::definite(availableInlineSpace, minWidth, maxWidth);

    auto [ minHeight, maxHeight ] = blockAxisMinMaxSizeConstraints(gridContainerRenderer);
    auto blockAxisConstraint = availableBlockSpace
        ? Layout::AxisConstraint::definite(*availableBlockSpace, minHeight, maxHeight)
        : Layout::AxisConstraint::maxContent(minHeight, maxHeight);

    return { inlineAxisConstraint, blockAxisConstraint };
}

GridLayout::GridItemBorderBoxRects GridLayout::gridItemBorderBoxRects() const
{
    GridItemBorderBoxRects gridItemBorderBoxRects;
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        if (layoutBox->isExcludedFromGridFormattingContextForIntegration())
            continue;
        gridItemBorderBoxRects.append(CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) }->borderBoxRectInContainer());
    }
    return gridItemBorderBoxRects;
}

// The offset of the grid container's content box from its border box, in the grid container's own logical axes.
struct LogicalContentBoxOffset {
    LayoutUnit inlineStart;
    LayoutUnit blockStart;
};

// Read from the renderer, as populateGridPositions does, and not from the root BoxGeometry's edges.
static LogicalContentBoxOffset logicalContentBoxOffset(const RenderGrid& renderGrid)
{
    // FIXME: A vertical scrollbar on the left shifts the content box, as IFC's formattingContextConstraints does
    // (BoxGeometryUpdater.cpp): add its width to inlineStart when the writing mode isInlineLeftToRight(), to blockStart when it
    // isBlockLeftToRight(), and to neither otherwise. A grid container with a scrollbar is not laid out by GFC yet
    // (GridHasNonVisibleOverflow).
    return { renderGrid.borderAndPaddingStart(), renderGrid.borderAndPaddingBefore() };
}

// The one place where a grid item's geometry goes from the grid container's logical axes to the renderer's physical rect.
// https://drafts.csswg.org/css-writing-modes-4/#logical-to-physical
// The inline axis is mirrored when it runs right to left or bottom to top, and the axes swap for a vertical writing mode.
// The block axis is deliberately not mirrored: renderer coordinates are unflipped and flipForWritingMode is applied where they are used.
static LayoutRect physicalBorderBoxRect(const Layout::BoxGeometry& gridItemGeometry, LogicalContentBoxOffset contentBoxOffset, LayoutUnit gridContainerBorderBoxInlineSize, WritingMode gridContainerWritingMode)
{
    auto logicalBorderBox = Layout::BoxGeometry::borderBoxRect(gridItemGeometry);
    auto inlineOffset = contentBoxOffset.inlineStart + logicalBorderBox.left();
    auto blockOffset = contentBoxOffset.blockStart + logicalBorderBox.top();
    if (gridContainerWritingMode.isInlineFlipped())
        inlineOffset = gridContainerBorderBoxInlineSize - inlineOffset - logicalBorderBox.width();
    if (gridContainerWritingMode.isHorizontal())
        return { inlineOffset, blockOffset, logicalBorderBox.width(), logicalBorderBox.height() };
    return { blockOffset, inlineOffset, logicalBorderBox.height(), logicalBorderBox.width() };
}

void GridLayout::updateGridItemRenderers(const GridItemBorderBoxRects& previousGridItemRects)
{
    CheckedRef renderGrid = gridBoxRenderer();
    CheckedRef layoutState = this->layoutState();
    auto gridContainerWritingMode = renderGrid->writingMode();
    auto contentBoxOffset = logicalContentBoxOffset(renderGrid);
    auto gridContainerBorderBoxInlineSize = renderGrid->logicalWidth();

    size_t gridItemIndex = 0;
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        if (layoutBox->isExcludedFromGridFormattingContextForIntegration())
            continue;
        CheckedRef renderer = downcast<RenderBox>(*layoutBox->rendererForIntegration());
        auto& gridItemGeometry = layoutState->geometryForBox(layoutBox);

        renderer->setBorderBoxInContainer(physicalBorderBoxRect(gridItemGeometry, contentBoxOffset, gridContainerBorderBoxInlineSize, gridContainerWritingMode));

        // The margins are logical in the grid container's writing mode, and so is the renderer's margin box when it is given that mode.
        renderer->setMarginBefore(gridItemGeometry.marginBefore(), gridContainerWritingMode);
        renderer->setMarginAfter(gridItemGeometry.marginAfter(), gridContainerWritingMode);
        renderer->setMarginStart(gridItemGeometry.marginStart(), gridContainerWritingMode);
        renderer->setMarginEnd(gridItemGeometry.marginEnd(), gridContainerWritingMode);

        if (!renderGrid->selfNeedsLayout() && renderer->checkForRepaintDuringLayout())
            renderer->repaintDuringLayoutIfMoved(previousGridItemRects[gridItemIndex]);
        ++gridItemIndex;
    }
    ASSERT(gridItemIndex == previousGridItemRects.size());
}

void GridLayout::updateFormattingContextRootRendererBeforeLayout()
{
    gridBoxRenderer().currentGrid().setNeedsItemsPlacement(true);
}

void GridLayout::clearGridItemGridAreas()
{
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        if (layoutBox->isExcludedFromGridFormattingContextForIntegration())
            continue;
        CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) }->clearGridAreaContentSize();
    }
}

// Some code (e.g. relative positioning) needs the grid areas after layout is done.
void GridLayout::updateGridItemGridAreas(const Layout::GridLinePositions& columnLines, const Layout::GridLinePositions& rowLines, const Layout::GridItemRects& gridItemRects)
{
    for (auto& gridItemRect : gridItemRects) {
        auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
        CheckedRef renderer = downcast<RenderBox>(*gridItemRect.layoutBox->rendererForIntegration());
        renderer->setGridAreaContentLogicalWidth(columnLines.areaSize(lineNumbersForGridArea.columnStartLine, lineNumbersForGridArea.columnEndLine));
        renderer->setGridAreaContentLogicalHeight(rowLines.areaSize(lineNumbersForGridArea.rowStartLine, lineNumbersForGridArea.rowEndLine));
    }
}

void GridLayout::updateFormattingContextRootRenderer(const Layout::UsedTrackSizes& usedTrackSizes, LayoutUnit usedBlockContentSize, const Layout::GridItemRects& gridItemRects, size_t autoRepeatColumnsCount, size_t autoRepeatRowsCount, bool autoRepeatColumnsTruncatedByLimit, bool autoRepeatRowsTruncatedByLimit, size_t leadingImplicitColumnsCount, size_t leadingImplicitRowsCount, const BitVector& collapsedColumnTracks, const BitVector& collapsedRowTracks)
{
    CheckedRef renderGrid = gridBoxRenderer();
    auto& currentGrid = renderGrid->currentGrid();

    // Render tree clients which consult the grid area of a grid item outside of layout (e.g.
    // RenderGrid::isExtrinsicallySized) would otherwise read from an empty map.
    for (auto& gridItemRect : gridItemRects) {
        auto& lineNumbersForGridArea = gridItemRect.lineNumbersForGridArea;
        auto gridArea = GridArea {
            GridSpan::translatedDefiniteGridSpan(static_cast<unsigned>(lineNumbersForGridArea.rowStartLine), static_cast<unsigned>(lineNumbersForGridArea.rowEndLine)),
            GridSpan::translatedDefiniteGridSpan(static_cast<unsigned>(lineNumbersForGridArea.columnStartLine), static_cast<unsigned>(lineNumbersForGridArea.columnEndLine))
        };
        currentGrid.setGridItemArea(downcast<RenderBox>(*protect(gridItemRect.layoutBox->rendererForIntegration())), gridArea);
    }
    currentGrid.setNeedsItemsPlacement(false);

    // The grid was emptied before layout, so this sizes it to the used tracks, whether or not there are out-of-flow boxes:
    // gridAreaRangeForOutOfFlow relies on numTracks() to tell the last line from an interior one.
    currentGrid.ensureGridSize(usedTrackSizes.rowSizes.size(), usedTrackSizes.columnSizes.size());

    // Out-of-flow boxes are placed by RenderGrid, which resolves their grid lines against the explicit
    // grid it computes from style plus these auto-repeated track counts, offset by the explicit grid start.
    currentGrid.setAutoRepeatTracks(autoRepeatRowsCount, autoRepeatColumnsCount);
    currentGrid.setAutoRepeatTruncatedByLimit(autoRepeatRowsTruncatedByLimit, autoRepeatColumnsTruncatedByLimit);
    // The used tracks hold the leading implicit tracks and the explicit grid, whose size RenderGrid reads from style and these auto-repeated track counts.
    ASSERT(currentGrid.numTracks(Style::GridTrackSizingDirection::Rows) >= leadingImplicitRowsCount + Style::GridPositionsResolver::explicitGridCount(renderGrid.get(), Style::GridTrackSizingDirection::Rows));
    ASSERT(currentGrid.numTracks(Style::GridTrackSizingDirection::Columns) >= leadingImplicitColumnsCount + Style::GridPositionsResolver::explicitGridCount(renderGrid.get(), Style::GridTrackSizingDirection::Columns));
    currentGrid.setExplicitGridStart(leadingImplicitRowsCount, leadingImplicitColumnsCount);

    auto emptyTrackIndexes = [](const BitVector& collapsedTracks) -> std::unique_ptr<OrderedTrackIndexSet> {
        if (collapsedTracks.isEmpty())
            return { };
        auto trackIndexes = makeUnique<OrderedTrackIndexSet>();
        for (auto trackIndex : collapsedTracks)
            trackIndexes->add(trackIndex);
        return trackIndexes;
    };
    currentGrid.setAutoRepeatEmptyColumns(emptyTrackIndexes(collapsedColumnTracks));
    currentGrid.setAutoRepeatEmptyRows(emptyTrackIndexes(collapsedRowTracks));

    OrderIteratorPopulator orderIteratorPopulator(currentGrid.orderIterator());

    renderGrid->setLogicalHeight(usedBlockContentSize + renderGrid->borderAndPaddingLogicalHeight());

    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        if (layoutBox->isExcludedFromGridFormattingContextForIntegration())
            continue;
        orderIteratorPopulator.collectChild(CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) });
    }
}

// Layout takes the grid formatting context or the legacy path depending on the grid as it is now, so it can switch
// between two layouts. Each path leaves state in the render tree that the other one does not overwrite: GFC sets the
// placement as done (so legacy would skip placing the items) and its baselines, legacy fills the grid's cells and
// computes the content distribution offsets that GFC leaves alone. Reset all of it when the path changes, before either runs.
void GridLayout::resetStateOnLayoutPathChange(RenderGrid& renderGrid)
{
    renderGrid.currentGrid().setNeedsItemsPlacement(true);
    renderGrid.setGridFormattingContextBaselines({ }, { });
    renderGrid.offsetBetweenTracks(Style::GridTrackSizingDirection::Columns) = { };
    renderGrid.offsetBetweenTracks(Style::GridTrackSizingDirection::Rows) = { };

    clearGridItemGridAreasLeftByGridFormattingContext(renderGrid);
}

// The items' grid areas were set by whichever path laid out the grid last. Legacy layout and legacy intrinsic sizing only
// give an item one that has none, so one the grid formatting context left would be read as if they had set it.
void GridLayout::clearGridItemGridAreasLeftByGridFormattingContext(RenderGrid& renderGrid)
{
    for (CheckedRef gridItem : childrenOfType<RenderBox>(renderGrid)) {
        if (!gridItem->isOutOfFlowPositioned())
            gridItem->clearGridAreaContentSize();
    }
}

std::pair<LayoutUnit, LayoutUnit> GridLayout::computeIntrinsicWidths()
{
    CheckedRef gridContainerRenderer = gridBoxRenderer();
    auto [ minWidth, maxWidth ] = inlineAxisMinMaxSizeConstraints(gridContainerRenderer, IsComputingIntrinsicSize::Yes);
    auto inlineAxisAutoRepeatConstraint = Layout::AutoRepeatConstraint { { }, minWidth, maxWidth };

    auto [ minHeight, maxHeight ] = blockAxisMinMaxSizeConstraints(gridContainerRenderer);
    auto blockAxisAutoRepeatConstraint = Layout::AutoRepeatConstraint { gridContainerRenderer->availableLogicalHeightForContentBox(), minHeight, maxHeight };

    // Measuring the grid items sets and then clears their grid area, which would also drop the one the last layout left
    // for relative positioning and percentage box model values. Put it back.
    struct GridAreaContentSize {
        CheckedRef<RenderBox> renderer;
        std::optional<RenderBox::GridAreaSize> logicalWidth;
        std::optional<RenderBox::GridAreaSize> logicalHeight;
    };
    Vector<GridAreaContentSize> previousGridAreaContentSizes;
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        if (layoutBox->isExcludedFromGridFormattingContextForIntegration())
            continue;
        CheckedRef renderer = downcast<RenderBox>(*layoutBox->rendererForIntegration());
        previousGridAreaContentSizes.append({ renderer, renderer->gridAreaContentLogicalWidth(), renderer->gridAreaContentLogicalHeight() });
    }

    auto gridFormattingContext = Layout::GridFormattingContext { gridBox(), protect(layoutState()) };
    auto intrinsicWidths = gridFormattingContext.computeIntrinsicWidths(inlineAxisAutoRepeatConstraint, blockAxisAutoRepeatConstraint);

    for (auto& previous : previousGridAreaContentSizes) {
        CheckedRef renderer = previous.renderer;
        renderer->clearGridAreaContentSize();
        if (previous.logicalWidth)
            renderer->setGridAreaContentLogicalWidth(*previous.logicalWidth);
        if (previous.logicalHeight)
            renderer->setGridAreaContentLogicalHeight(*previous.logicalHeight);
    }
    return { intrinsicWidths.minimum, intrinsicWidths.maximum };
}

void GridLayout::layout()
{
    updateFormattingContextRootRendererBeforeLayout();

    auto gridLayoutConstraints = constraintsForGridContent(gridBox());

    auto previousGridItemRects = gridItemBorderBoxRects();

    auto result = Layout::GridFormattingContext { gridBox(), protect(layoutState()) }.layout(gridLayoutConstraints);
    updateGridItemRenderers(previousGridItemRects);
    updateGridItemGridAreas(result.columnLines, result.rowLines, result.gridItemRects);
    updateFormattingContextRootRenderer(result.usedTrackSizes, result.usedBlockContentSize, result.gridItemRects, result.autoRepeatColumnsCount, result.autoRepeatRowsCount, result.autoRepeatColumnsTruncatedByLimit, result.autoRepeatRowsTruncatedByLimit, result.leadingImplicitColumnsCount, result.leadingImplicitRowsCount, result.collapsedColumnTracks, result.collapsedRowTracks);
    auto contentBoxBlockStart = logicalContentBoxOffset(protect(gridBoxRenderer())).blockStart;
    auto borderBoxOffset = [&](std::optional<LayoutUnit> baseline) -> std::optional<LayoutUnit> {
        if (!baseline)
            return { };
        return contentBoxBlockStart + *baseline;
    };
    gridBoxRenderer().setGridFormattingContextBaselines(borderBoxOffset(result.baselines.first), borderBoxOffset(result.baselines.last));
    populateGridPositions(result.columnLines, result.rowLines);

    CheckedRef renderGrid = gridBoxRenderer();
    // Closes the transaction RenderGrid::layoutGrid() began. Like the other containers, commit it before the transforms
    // and the overflow are updated: committing relays out a box whose scrollbar changed, and that box registers
    // with this container again if its offset-path depends on its containing block.
    renderGrid->endAndCommitUpdateScrollInfoAfterLayoutTransaction();
    // Taking the boxes registered for this container drains both the in-flow and the out-of-flow ones, and an
    // out-of-flow box registers while it is laid out, so the in-flow update has to run before out-of-flow layout.
    renderGrid->updateInFlowDescendantTransformsAfterLayout();
    layoutOutOfFlowBoxes();
    renderGrid->updateOutOfFlowDescendantTransformsAfterLayout();

    renderGrid->updateLogicalHeight();
    updateOverflow(renderGrid);

    // https://drafts.csswg.org/css-grid-2/#resolved-track-list
    renderGrid->setResolvedTrackSizes(WTF::move(result.usedTrackSizes.columnSizes), WTF::move(result.usedTrackSizes.rowSizes));
}

void GridLayout::updateOverflow(RenderGrid& renderGrid)
{
    ASSERT(renderGrid.style().isOverflowVisible());

    // https://drafts.csswg.org/css-overflow-3/#scrollable-overflow-calculation
    // The grid items' and out-of-flow boxes' overflow, their transforms, the container's own visual effects
    // (box-shadow, outline, filters, theme) are renderer concepts. Propagate them with the same generic code
    // that simplified layout runs so that the result does not depend on which kind of layout ran last.
    // FIXME: For scroll containers (GridHasNonVisibleOverflow), the content area has to be the grid itself
    // (css-grid-1 5.3), computed from GFC's own track positions.
    renderGrid.computeInFlowOverflow(renderGrid.flippedContentBoxRect(), RenderBox::ComputeOverflowOptions::MarginsExtendLayoutOverflow);
    renderGrid.addOverflowFromOutOfFlowBoxes();
}

void GridLayout::layoutOutOfFlowBoxes()
{
    CheckedRef renderGrid = gridBoxRenderer();
    auto* outOfFlowDescendants = renderGrid->outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;

    // gridAreaRangeForOutOfFlow (called from PositionedLayoutConstraints::captureGridArea) resolves grid lines
    // against the positions, so they have to match the track counts.
    ASSERT(renderGrid->positions(Style::GridTrackSizingDirection::Columns).size() == renderGrid->numTracks(Style::GridTrackSizingDirection::Columns) + 1);
    ASSERT(renderGrid->positions(Style::GridTrackSizingDirection::Rows).size() == renderGrid->numTracks(Style::GridTrackSizingDirection::Rows) + 1);

    // FIXME: Determine whether we should conditionally use RelayoutChildren::No
    // based on whether the grid container size changed, matching the legacy path's
    // logic (which uses RelayoutChildren::Yes only when size changed).
    renderGrid->layoutOutOfFlowBoxes(RelayoutChildren::Yes);
}

// Keep RenderGrid's grid state consistent with the result of layout, whether or not the grid has out-of-flow
// boxes, since clients other than out-of-flow layout read it (e.g. getComputedStyle, the Web Inspector grid overlay).
void GridLayout::populateGridPositions(const Layout::GridLinePositions& columnLines, const Layout::GridLinePositions& rowLines)
{
    CheckedRef renderGrid = gridBoxRenderer();

    // positions[i] is the start of track i, after the gutter that precedes it, and positions[trackCount] is the end edge of the grid.
    auto populate = [&](Style::GridTrackSizingDirection direction) {
        bool isColumns = direction == Style::GridTrackSizingDirection::Columns;
        auto& lines = isColumns ? columnLines : rowLines;
        auto borderAndPadding = isColumns ? renderGrid->borderAndPaddingStart() : renderGrid->borderAndPaddingBefore();

        auto& positions = renderGrid->positions(direction);
        positions.resize(lines.trackCount() + 1);
        for (size_t lineIndex = 0; lineIndex <= lines.trackCount(); ++lineIndex)
            positions[lineIndex] = borderAndPadding + lines.positionAsStartLine(lineIndex);
    };

    populate(Style::GridTrackSizingDirection::Columns);
    populate(Style::GridTrackSizingDirection::Rows);
}

TextStream& operator<<(TextStream& stream, const GridLayout& layout)
{
    stream << "GridLayout@" << &layout;
    stream << " gridBox=" << &layout.gridBox();
    size_t index = 0;
    for (CheckedRef box : Layout::formattingContextBoxes(layout.gridBox())) {
        stream << "\n  [" << index++ << "] box=" << box.ptr();
        stream << " anonymous=" << (box->isAnonymous() ? "yes" : "no");
        stream << " establishesContext=" << (box->establishesFormattingContext() ? "yes" : "no");
        stream << " display=" << box->style().display();
        if (CheckedPtr renderer = box->rendererForIntegration())
            stream << " renderer=" << renderer->renderName() << '@' << renderer.get();
        else
            stream << " renderer=<null>";
    }
    return stream;
}

} // namespace LayoutIntegration

} // namespace WebCore

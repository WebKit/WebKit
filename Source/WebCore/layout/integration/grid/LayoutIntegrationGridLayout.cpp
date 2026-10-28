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
#include "LayoutIntegrationBoxGeometryUpdater.h"
#include "LayoutIntegrationBoxTreeUpdater.h"
#include "RenderGrid.h"
#include "RenderView.h"
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
    BoxTreeUpdater { renderGrid }.build();
}

GridLayout::~GridLayout()
{
    BoxTreeUpdater { protect(gridBoxRenderer()) }.tearDown();
    gridBox().setIsGridIntegrationRoot(false);
}

void GridLayout::updateFormattingContextGeometries()
{
    auto boxGeometryUpdater = BoxGeometryUpdater { protect(layoutState()), gridBox() };
    CheckedPtr gridBoxContainingBlock = CheckedRef { gridBoxRenderer() }->containingBlock();

    boxGeometryUpdater.setFormattingContextRootGeometry(gridBoxContainingBlock->contentBoxLogicalWidth());
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
        [](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
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
        [](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        }
    );
    return maximumSize.transform([&](LayoutUnit size) {
        if (boxSizing == BoxSizing::BorderBox)
            size -= borderAndPadding;
        return std::max({ }, size);
    });
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

    CheckedRef gridContainerStyle = gridContainerRenderer->style();
    auto gridContainerZoom = gridContainerStyle->usedZoomForLength();
    auto boxSizing = gridContainerStyle->boxSizing();

    auto containingBlockWidth = gridContainerRenderer->containingBlockLogicalWidthForContent();
    auto borderAndPaddingWidth = gridContainerRenderer->borderAndPaddingLogicalWidth();
    auto inlineAxisConstraint = Layout::AxisConstraint::definite(availableInlineSpace,
        minimumSizeConstraint(gridContainerStyle->minWidth(), containingBlockWidth, boxSizing, borderAndPaddingWidth, gridContainerZoom),
        maximumSizeConstraint(gridContainerStyle->maxWidth(), containingBlockWidth, boxSizing, borderAndPaddingWidth, gridContainerZoom));

    auto blockAxisConstraint = [&]() -> Layout::AxisConstraint {
        auto containingBlockHeight = protect(gridContainerRenderer->containingBlock())->availableLogicalHeightForPercentageComputation();
        auto borderAndPaddingHeight = gridContainerRenderer->borderAndPaddingLogicalHeight();
        auto minHeight = minimumSizeConstraint(gridContainerStyle->minHeight(), containingBlockHeight, boxSizing, borderAndPaddingHeight, gridContainerZoom);
        auto maxHeight = maximumSizeConstraint(gridContainerStyle->maxHeight(), containingBlockHeight, boxSizing, borderAndPaddingHeight, gridContainerZoom);
        if (availableBlockSpace)
            return Layout::AxisConstraint::definite(*availableBlockSpace, minHeight, maxHeight);
        return Layout::AxisConstraint::maxContent(minHeight, maxHeight);
    }();

    return { inlineAxisConstraint, blockAxisConstraint };
}

GridLayout::GridItemBorderBoxRects GridLayout::gridItemBorderBoxRects() const
{
    GridItemBorderBoxRects gridItemBorderBoxRects;
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox()))
        gridItemBorderBoxRects.append(CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) }->borderBoxRectInContainer());
    return gridItemBorderBoxRects;
}

void GridLayout::updateGridItemRenderers(const GridItemBorderBoxRects& previousGridItemRects)
{
    CheckedRef renderGrid = gridBoxRenderer();
    CheckedRef layoutState = this->layoutState();
    auto& gridBoxGeometry = layoutState->geometryForBox(gridBox());
    auto contentBoxOffset = LayoutPoint(gridBoxGeometry.contentBoxLeft(), gridBoxGeometry.contentBoxTop());

    size_t gridItemIndex = 0;
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        CheckedRef renderer = downcast<RenderBox>(*layoutBox->rendererForIntegration());
        auto& gridItemGeometry = layoutState->geometryForBox(layoutBox);
        auto borderBoxRect = Layout::BoxGeometry::borderBoxRect(gridItemGeometry);

        renderer->setLocation(contentBoxOffset + borderBoxRect.topLeft());
        renderer->setBorderBoxWidth(borderBoxRect.width());
        renderer->setBorderBoxHeight(borderBoxRect.height());

        renderer->setMarginBefore(gridItemGeometry.marginBefore());
        renderer->setMarginAfter(gridItemGeometry.marginAfter());
        renderer->setMarginStart(gridItemGeometry.marginStart());
        renderer->setMarginEnd(gridItemGeometry.marginEnd());

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

void GridLayout::updateFormattingContextRootRenderer(LayoutUnit usedBlockContentSize, const Layout::GridItemRects& gridItemRects, size_t autoRepeatColumnsCount, size_t autoRepeatRowsCount, size_t leadingImplicitColumnsCount, size_t leadingImplicitRowsCount, const BitVector& collapsedColumnTracks, const BitVector& collapsedRowTracks)
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

    // Out-of-flow boxes are placed by RenderGrid, which resolves their grid lines against the explicit
    // grid it computes from style plus these auto-repeated track counts, offset by the explicit grid start.
    currentGrid.setAutoRepeatTracks(autoRepeatRowsCount, autoRepeatColumnsCount);
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

    renderGrid->setBorderBoxHeight(usedBlockContentSize + renderGrid->borderAndPaddingLogicalHeight());

    for (CheckedRef layoutBox : formattingContextBoxes(gridBox()))
        orderIteratorPopulator.collectChild(CheckedRef { downcast<RenderBox>(*layoutBox->rendererForIntegration()) });
}

// updateFormattingContextRootRenderer may mutate bits of RenderGrid to properly reflect
// the state and result of layout since certain clients may query this information
// (see RenderGrid::paintChildren). Undo this state if we are falling back to legacy
// so that we run a full layout.
void GridLayout::invalidateFormattingContextRootRenderer(RenderGrid& renderGrid)
{
    renderGrid.currentGrid().setNeedsItemsPlacement(true);
}

std::pair<LayoutUnit, LayoutUnit> GridLayout::computeIntrinsicWidths()
{
    CheckedRef gridContainerRenderer = gridBoxRenderer();
    CheckedRef gridContainerStyle = gridContainerRenderer->style();
    auto gridContainerZoom = gridContainerStyle->usedZoomForLength();
    auto boxSizing = gridContainerStyle->boxSizing();

    auto containingBlockWidth = gridContainerRenderer->containingBlockLogicalWidthForContent();
    auto borderAndPaddingWidth = gridContainerRenderer->borderAndPaddingLogicalWidth();
    auto inlineAxisAutoRepeatConstraint = Layout::AutoRepeatConstraint { { },
        minimumSizeConstraint(gridContainerStyle->minWidth(), containingBlockWidth, boxSizing, borderAndPaddingWidth, gridContainerZoom),
        maximumSizeConstraint(gridContainerStyle->maxWidth(), containingBlockWidth, boxSizing, borderAndPaddingWidth, gridContainerZoom) };

    auto containingBlockHeight = protect(gridContainerRenderer->containingBlock())->availableLogicalHeightForPercentageComputation();
    auto borderAndPaddingHeight = gridContainerRenderer->borderAndPaddingLogicalHeight();
    auto blockAxisAutoRepeatConstraint = Layout::AutoRepeatConstraint { gridContainerRenderer->availableLogicalHeightForContentBox(),
        minimumSizeConstraint(gridContainerStyle->minHeight(), containingBlockHeight, boxSizing, borderAndPaddingHeight, gridContainerZoom),
        maximumSizeConstraint(gridContainerStyle->maxHeight(), containingBlockHeight, boxSizing, borderAndPaddingHeight, gridContainerZoom) };

    auto gridFormattingContext = Layout::GridFormattingContext { gridBox(), protect(layoutState()) };
    auto intrinsicWidths = gridFormattingContext.computeIntrinsicWidths(inlineAxisAutoRepeatConstraint, blockAxisAutoRepeatConstraint);
    return { intrinsicWidths.minimum, intrinsicWidths.maximum };
}

void GridLayout::layout()
{
    updateFormattingContextRootRendererBeforeLayout();

    auto gridLayoutConstraints = constraintsForGridContent(gridBox());

    auto previousGridItemRects = gridItemBorderBoxRects();

    auto [ usedTrackSizes, gridItemRects, usedBlockContentSize,
        autoRepeatColumnsCount, autoRepeatRowsCount,
        leadingImplicitColumnsCount, leadingImplicitRowsCount,
        collapsedColumnTracks, collapsedRowTracks ] = Layout::GridFormattingContext { gridBox(), protect(layoutState()) }.layout(gridLayoutConstraints);
    updateGridItemRenderers(previousGridItemRects);
    updateFormattingContextRootRenderer(usedBlockContentSize, gridItemRects, autoRepeatColumnsCount, autoRepeatRowsCount, leadingImplicitColumnsCount, leadingImplicitRowsCount, collapsedColumnTracks, collapsedRowTracks);
    layoutOutOfFlowBoxes(usedTrackSizes, collapsedColumnTracks, collapsedRowTracks);

    CheckedRef renderGrid = gridBoxRenderer();
    renderGrid->updateLogicalHeight();
    updateOverflow(renderGrid);

    // https://drafts.csswg.org/css-grid-2/#resolved-track-list
    renderGrid->setResolvedTrackSizes(WTF::move(usedTrackSizes.columnSizes), WTF::move(usedTrackSizes.rowSizes));
}

void GridLayout::updateOverflow(RenderGrid& renderGrid)
{
    ASSERT(renderGrid.style().isOverflowVisible());
    renderGrid.clearOverflow();

    CheckedRef layoutState = this->layoutState();
    auto& gridBoxGeometry = layoutState->geometryForBox(gridBox());
    auto contentBoxOffset = LayoutSize { gridBoxGeometry.contentBoxLeft(), gridBoxGeometry.contentBoxTop() };

    // https://drafts.csswg.org/css-overflow-3/#scrollable
    auto gridItemsLayoutOverflowRect = LayoutRect { };
    auto gridItemsVisualOverflowRect = LayoutRect { };
    for (CheckedRef layoutBox : formattingContextBoxes(gridBox())) {
        LayoutRect gridItemBorderBoxRect = Layout::BoxGeometry::borderBoxRect(layoutState->geometryForBox(layoutBox));
        gridItemBorderBoxRect.move(contentBoxOffset);
        gridItemsVisualOverflowRect.unite(gridItemBorderBoxRect);

        CheckedRef gridItemRenderer = downcast<RenderBox>(*layoutBox->rendererForIntegration());

        // A grid item's content is laid out outside of this formatting context, so the overflow that
        // its own subtree generates is only available from the renderer. The returned rect already
        // includes the grid item's border box.
        auto gridItemLayoutOverflowRect = gridItemRenderer->layoutOverflowRectForPropagation(renderGrid.writingMode());
        gridItemLayoutOverflowRect.move(gridItemRenderer->locationOffset());
        gridItemsLayoutOverflowRect.unite(gridItemLayoutOverflowRect);

        if (gridItemRenderer->hasVisualOverflow()) {
            auto gridItemVisualOverflowRect = gridItemRenderer->visualOverflowRectForPropagation(renderGrid.writingMode());
            gridItemVisualOverflowRect.move(gridItemRenderer->locationOffset());
            gridItemsVisualOverflowRect.unite(gridItemVisualOverflowRect);
        }
    }

    renderGrid.addLayoutOverflow(gridItemsLayoutOverflowRect);

    // Out-of-flow boxes whose containing block is the grid container are laid out by
    // layoutOutOfFlowBoxes, also outside of this formatting context.
    auto outOfFlowBoxesLayoutOverflowRect = LayoutRect { };
    if (auto* outOfFlowDescendants = renderGrid.outOfFlowBoxes()) {
        for (CheckedRef outOfFlowBox : *outOfFlowDescendants) {
            // Fixed positioned boxes do not scroll with the content, so they never contribute to the
            // scrollable overflow area.
            if (outOfFlowBox->isFixedPositioned())
                continue;
            auto outOfFlowBoxLayoutOverflowRect = outOfFlowBox->layoutOverflowRectForPropagation(renderGrid.writingMode());
            outOfFlowBoxLayoutOverflowRect.move(outOfFlowBox->locationOffset());
            outOfFlowBoxesLayoutOverflowRect.unite(outOfFlowBoxLayoutOverflowRect);
        }
    }
    renderGrid.addLayoutOverflow(outOfFlowBoxesLayoutOverflowRect);

    if (!renderGrid.borderBoxRect().contains(gridItemsVisualOverflowRect))
        renderGrid.addVisualOverflow(gridItemsVisualOverflowRect);
}

void GridLayout::layoutOutOfFlowBoxes(const Layout::UsedTrackSizes& usedTrackSizes, const BitVector& collapsedColumnTracks, const BitVector& collapsedRowTracks)
{
    CheckedRef renderGrid = gridBoxRenderer();
    auto* outOfFlowDescendants = renderGrid->outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;

    // Populate grid positions so that gridAreaRangeForOutOfFlow (called from
    // PositionedLayoutConstraints::captureGridArea during positioned layout)
    // can resolve grid lines to their positions in the grid.
    populateGridPositionsForOutOfFlowLayout(usedTrackSizes, collapsedColumnTracks, collapsedRowTracks);

    // FIXME: Determine whether we should conditionally use RelayoutChildren::No
    // based on whether the grid container size changed, matching the legacy path's
    // logic (which uses RelayoutChildren::Yes only when size changed).
    renderGrid->layoutOutOfFlowBoxes(RelayoutChildren::Yes);
}

void GridLayout::populateGridPositionsForOutOfFlowLayout(const Layout::UsedTrackSizes& usedTrackSizes, const BitVector& collapsedColumnTracks, const BitVector& collapsedRowTracks)
{
    CheckedRef renderGrid = gridBoxRenderer();

    // Ensure numTracks() returns the correct value for each direction.
    renderGrid->currentGrid().ensureGridSize(usedTrackSizes.rowSizes.size(), usedTrackSizes.columnSizes.size());

    auto populate = [&](Style::GridTrackSizingDirection direction) {
        bool isColumns = direction == Style::GridTrackSizingDirection::Columns;
        auto& trackSizes = isColumns ? usedTrackSizes.columnSizes : usedTrackSizes.rowSizes;
        auto& collapsedTracks = isColumns ? collapsedColumnTracks : collapsedRowTracks;
        auto gap = Layout::GridFormattingContext::usedGapValue(isColumns ? renderGrid->style().columnGap() : renderGrid->style().rowGap(), renderGrid->style());
        auto borderAndPadding = isColumns ? renderGrid->borderAndPaddingStart() : renderGrid->borderAndPaddingBefore();
        auto numberOfTracks = trackSizes.size();
        auto numberOfNonCollapsedTracks = numberOfTracks - collapsedTracks.bitCount();

        auto& positions = renderGrid->positions(direction);
        positions.resize(numberOfTracks + 1);
        positions[0] = borderAndPadding;
        size_t nonCollapsedTracksSoFar = 0;
        for (auto [trackIndex, trackSize] : WTF::indexedRange(trackSizes)) {
            bool hasGutterAfterTrack = !collapsedTracks.get(trackIndex) && ++nonCollapsedTracksSoFar < numberOfNonCollapsedTracks;
            positions[trackIndex + 1] = positions[trackIndex] + trackSize + (hasGutterAfterTrack ? gap : 0_lu);
        }
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

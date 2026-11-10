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
#include "LayoutIntegrationGridCoverage.h"

#include "BaselineAlignmentInlines.h"
#include "Document.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderGrid.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include <pal/Logging.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace LayoutIntegration {

enum class ReasonCollectionMode : bool {
    FirstOnly,
    All
};

enum class GridAvoidanceReason : uint8_t {
    GridHasVerticalLRWritingMode,
    GridHasVerticalRLWritingMode,
    GridHasSidewaysWritingMode,
    GridHasHorizontalBTWritingMode,
    GridHasRTLDirection, // http://webkit.org/b/317334
    GridNeedsBaseline,
    GridHasOutOfFlowChild,
    GridHasNonVisibleOverflow,
    GridItemIsReplacedElement,
    GridItemIsSubgrid,
    GridHasGridTemplateAreas,
    GridHasNonFixedGaps,
    GridIsOutOfFlow,
    GridHasSizeOrInlineSizeContainment,
    GridHasUnsupportedGridTemplateColumns,
    GridHasUnsupportedGridTemplateRows,
    GridHasUnsupportedJustifyContent,
    GridHasUnsupportedAlignContent,
    GridHasUnsupportedMinHeight,
    GridHasUnsupportedMaxHeight,
    GridHasPercentageRowsWithIndefiniteHeight,
    GridItemHasUnsupportedMaxWidth,
    GridItemHasUnsupportedMaxHeight,
    GridItemHasUnsupportedMargin,
    GridItemHasBorderBoxSizing,
    GridItemIsOrthogonal,
    GridItemHasOpposingBlockFlow,
    GridItemHasOpposingInlineDirection,
    GridItemHasAspectRatio,
    GridItemHasUnsupportedInlineAxisAlignment,
    GridItemHasUnsupportedBlockAxisAlignment,
    GridItemHasNonVisibleOverflow,
    GridItemHasContainsSize,
    GridItemNeedsSecondColumnSizingPass,
    RelativeGridItemHasPercentageInset,

    GridItemHasUnsupportedWidthValue,
    GridItemHasUnsupportedAutomaticInlineSizing,
    GridItemHasUnsupportedHeightValue,
    GridItemHasUnsupportedAutomaticBlockSizing,
    GridItemHasUnsupportedMinWidth,
    GridItemHasUnsupportedMinHeight,
    NotAGrid,
    GridFormattingContextIntegrationDisabled,
};

#ifndef NDEBUG
#undef ADD_REASON_AND_RETURN_IF_NEEDED
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, reasonCollectionMode) { \
        reasons.add(reason); \
        if (reasonCollectionMode == ReasonCollectionMode::FirstOnly) \
            return reasons; \
    }
#else
#undef ADD_REASON_AND_RETURN_IF_NEEDED
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, reasonCollectionMode) { \
        ASSERT_UNUSED(reasonCollectionMode, reasonCollectionMode == ReasonCollectionMode::FirstOnly); \
        reasons.add(reason); \
        return reasons; \
    }
#endif

static bool gridItemHasValidWidth(const Style::PreferredSize& width)
{
    return WTF::switchOn(width,
        [&](const CSS::Keyword::Auto&) {
            return true;
        },
        [](const Style::PreferredSize::Fixed&) {
            return true;
        },
        [](const auto&) {
            return false;
        }
    );
}

// GFC treats a box as replaced when its renderer is a RenderReplaced (img, video, canvas, iframe,
// embed/object, outer svg, model, or any element whose content property produced an image), so the
// renderer has to be consulted. The element check is conservative: it also rejects an element that
// is replaced but whose renderer is not a RenderReplaced (e.g. an <object> showing its fallback
// content, or <img style="content: 'x'">), since GFC's replaced sizing does not handle those either.
// Works on any RenderBox, so it is used for grid items and for their children.
static bool isReplacedBox(const RenderBox& box)
{
    if (box.isRenderReplaced())
        return true;
    RefPtr element = box.element();
    return element && element->isReplaced();
}

// A grid item with an automatic size which is not stretched is sized to its fit-content size in
// the axis. GFC does not implement the remaining cases from grid item sizing, where a replaced
// item uses its natural size and an item with a preferred aspect ratio transfers its size through
// that ratio.
// Note: the per-item loop rejects replaced items with GridItemIsReplacedElement before it reaches
// this check, so the replaced test here only matters when collecting every reason
// (ReasonCollectionMode::All).
// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
static bool canComputeAutomaticSize(const RenderBox& gridItem)
{
    return !isReplacedBox(gridItem)
        && !gridItem.style().aspectRatio().hasRatio();
}

static bool gridItemHasValidHeight(const Style::PreferredSize& height)
{
    return WTF::switchOn(height,
        [](const CSS::Keyword::Auto&) {
            return true;
        },
        [](const Style::PreferredSize::Fixed&) {
            return true;
        },
        [](const auto&) {
            return false;
        }
    );
}

NODELETE static bool isUnsupportedGridTrackSize(const Style::GridTrackSize& trackSize)
{
    return trackSize.isFitContent();
}

static bool hasUnsupportedRepeatEntry(const Style::RepeatTrackList& repeatTrackList)
{
    return repeatTrackList.containsIf([](auto& repeatEntry) {
        return WTF::switchOn(repeatEntry,
            [](const Style::GridTrackSize& trackSize) {
                return isUnsupportedGridTrackSize(trackSize);
            },
            [](const Style::GridLineNames&) {
                return false;
            }
        );
    });
}

static bool hasUnsupportedGridTrackListEntry(const Style::GridTrackList& trackList)
{
    return trackList.containsIf([](auto& trackListEntry) {
        return WTF::switchOn(trackListEntry,
            [](const Style::GridTrackSize& trackSize) {
                return isUnsupportedGridTrackSize(trackSize);
            },
            [](const Style::GridLineNames&) {
                return false;
            },
            [](const Style::GridTrackEntryRepeat& repeat) {
                return hasUnsupportedRepeatEntry(repeat.list);
            },
            [](const Style::GridTrackEntryAutoRepeat& autoRepeat) {
                return hasUnsupportedRepeatEntry(autoRepeat.list);
            },
            [](const Style::GridTrackEntrySubgrid&) {
                return true;
            }
        );
    });
}

static EnumSet<GridAvoidanceReason> gridLayoutAvoidanceReason(const RenderGrid& renderGrid, ReasonCollectionMode reasonCollectionMode)
{
    auto reasons = EnumSet<GridAvoidanceReason> { };

    if (!renderGrid.document().settings().gridFormattingContextIntegrationEnabled())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridFormattingContextIntegrationDisabled, reasons, reasonCollectionMode);

    CheckedRef renderGridStyle = renderGrid.style();
    CheckedPtr gridParentStyle = renderGrid.parent() ? &renderGrid.parent()->style() : nullptr;
    if (renderGridStyle->display() == Style::DisplayType::InlineGrid
        || isBaselinePosition(renderGridStyle->justifySelf().resolve(gridParentStyle).position())
        || isBaselinePosition(renderGridStyle->alignSelf().resolve(gridParentStyle).position()))
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridNeedsBaseline, reasons, reasonCollectionMode);

    if (renderGridStyle->display() != Style::DisplayType::BlockGrid)
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::NotAGrid, reasons, reasonCollectionMode);

    auto gridContainerWritingMode = renderGridStyle->writingMode();
    switch (gridContainerWritingMode.computedWritingMode()) {
    case StyleWritingMode::HorizontalTb:
        break;
    case StyleWritingMode::VerticalLr:
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasVerticalLRWritingMode, reasons, reasonCollectionMode);
        break;
    case StyleWritingMode::VerticalRl:
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasVerticalRLWritingMode, reasons, reasonCollectionMode);
        break;
    case StyleWritingMode::SidewaysLr:
    case StyleWritingMode::SidewaysRl:
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasSidewaysWritingMode, reasons, reasonCollectionMode);
        break;
    case StyleWritingMode::HorizontalBt:
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasHorizontalBTWritingMode, reasons, reasonCollectionMode);
        break;
    }

    if (gridContainerWritingMode.bidiDirection() == TextDirection::RTL)
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasRTLDirection, reasons, reasonCollectionMode);

    if (!renderGridStyle->isOverflowVisible())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasNonVisibleOverflow, reasons, reasonCollectionMode);

    // Check for non-fixed gaps. GFC currently only supports fixed-length gaps.
    if (!renderGridStyle->rowGap().isNormal()) {
        if (!renderGridStyle->rowGap().tryFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasNonFixedGaps, reasons, reasonCollectionMode);
    }

    if (!renderGridStyle->columnGap().isNormal()) {
        if (!renderGridStyle->columnGap().tryFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasNonFixedGaps, reasons, reasonCollectionMode);
    }

    if (renderGrid.isOutOfFlowPositioned())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridIsOutOfFlow, reasons, reasonCollectionMode);

    if (!renderGridStyle->gridTemplateAreas().isNone())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasGridTemplateAreas, reasons, reasonCollectionMode);

    auto& gridTemplateColumns = renderGridStyle->gridTemplateColumns();
    auto& gridTemplateColumnsTrackList = gridTemplateColumns.list;
    if (hasUnsupportedGridTrackListEntry(gridTemplateColumnsTrackList))
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns, reasons, reasonCollectionMode);

    auto& gridTemplateRows = renderGridStyle->gridTemplateRows();
    if (hasUnsupportedGridTrackListEntry(gridTemplateRows.list))
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedGridTemplateRows, reasons, reasonCollectionMode);

    // These are the forms of containment GFC cannot honour: it would size these grids' tracks from
    // their grid items rather than from contain-intrinsic-size.
    if (renderGrid.shouldApplySizeOrInlineSizeContainment())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasSizeOrInlineSizeContainment, reasons, reasonCollectionMode);

    if (!renderGridStyle->justifyContent().isNormal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedJustifyContent, reasons, reasonCollectionMode);

    if (!renderGridStyle->alignContent().isNormal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedAlignContent, reasons, reasonCollectionMode);

    // GFC cannot yet resolve intrinsic (min-content/max-content/fit-content) block-axis sizing on the grid container itself. Intrinsic
    // inline-axis min and max sizes are resolved by RenderBox before GFC runs.
    if (renderGridStyle->minHeight().isIntrinsic())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedMinHeight, reasons, reasonCollectionMode);

    if (renderGridStyle->maxHeight().isIntrinsic())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedMaxHeight, reasons, reasonCollectionMode);

    // If the contianer has an indefinite (e.g. auto) height and a row with a percentage,
    // then in order to properly handle this case we need to run the "Find the size of
    // the grid container," portion of the spec fully before "Given the resulting grid
    // container size, run the Grid Sizing Algorithm to size the grid." so that the
    // grid has a height to resolve the percentages again in the second step.
    auto gridBlockSizeIsIndefinite = renderGridStyle->height().isAuto() || renderGridStyle->height().isIntrinsic();
    // Check the expanded track sizes so tracks inside repeat() are included. Style keeps the
    // auto-repeated tracks apart from the rest, so check those too.
    auto hasPercentageRowTrack = [&] {
        auto isPercentageTrack = [](auto& trackSize) {
            return trackSize.minTrackBreadth().isPercentOrCalculated() || trackSize.maxTrackBreadth().isPercentOrCalculated();
        };
        return gridTemplateRows.sizes.containsIf(isPercentageTrack) || gridTemplateRows.autoRepeatSizes.containsIf(isPercentageTrack);
    };
    if (gridBlockSizeIsIndefinite && hasPercentageRowTrack())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasPercentageRowsWithIndefiniteHeight, reasons, reasonCollectionMode);

    // The rendered legend of a fieldset is not a grid item: it sits in the border and GFC does not lay it out.
    CheckedPtr renderedLegend = renderGrid.renderedFieldsetLegend();
    for (CheckedRef gridItem : childrenOfType<RenderBox>(renderGrid)) {
        if (&gridItem.get() == renderedLegend.get())
            continue;

        // We do not yet support grid item sizing spec for replaced elements.
        // See: https://drafts.csswg.org/css-grid/#grid-item-sizing
        if (isReplacedBox(gridItem))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemIsReplacedElement, reasons, reasonCollectionMode);

        // GFC has no subgrid support, so a subgrid item would fall through to the legacy
        // RenderGrid path, which crashes when its grid-item-area map has not been populated
        // by a legacy parent grid.
        if (CheckedPtr renderGridItem = dynamicDowncast<RenderGrid>(gridItem.get()); renderGridItem && renderGridItem->isSubgrid())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemIsSubgrid, reasons, reasonCollectionMode);

        CheckedRef gridItemStyle = gridItem->style();

        auto usedJustifySelf = gridItemStyle->justifySelf().resolve(renderGridStyle.ptr());

        // Every non-baseline self-alignment value is handled by GridLayout's self alignment,
        // including the safe overflow alignment. Baseline alignment additionally requires the
        // track sizing algorithm to form baseline-sharing groups and to grow the tracks which
        // those groups span, which GFC does not implement yet.
        if (isBaselinePosition(usedJustifySelf.position()))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedInlineAxisAlignment, reasons, reasonCollectionMode);

        auto& gridItemWidth = gridItemStyle->width();
        if (!gridItemHasValidWidth(gridItemWidth))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedWidthValue, reasons, reasonCollectionMode);

        if (gridItemWidth.isAuto() && !canComputeAutomaticSize(gridItem))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedAutomaticInlineSizing, reasons, reasonCollectionMode);

        auto usedAlignSelf = gridItemStyle->alignSelf().resolve(renderGridStyle.ptr());

        if (isBaselinePosition(usedAlignSelf.position()))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedBlockAxisAlignment, reasons, reasonCollectionMode);

        auto& gridItemHeight = gridItemStyle->height();
        if (!gridItemHasValidHeight(gridItemHeight))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedHeightValue, reasons, reasonCollectionMode);

        if (gridItemHeight.isAuto() && !canComputeAutomaticSize(gridItem))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedAutomaticBlockSizing, reasons, reasonCollectionMode);

        auto& minWidth = gridItemStyle->minWidth();
        if (!minWidth.isFixed() && !minWidth.isAuto())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMinWidth, reasons, reasonCollectionMode);

        auto& maxWidth = gridItemStyle->maxWidth();
        if (!maxWidth.isFixed() && !maxWidth.isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMaxWidth, reasons, reasonCollectionMode);

        auto& minHeight = gridItemStyle->minHeight();
        if (!minHeight.isFixed() && !minHeight.isAuto())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMinHeight, reasons, reasonCollectionMode);

        auto& maxHeight = gridItemStyle->maxHeight();
        if (!maxHeight.isFixed() && !maxHeight.isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMaxHeight, reasons, reasonCollectionMode);

        // These are the margin edges GridLayoutUtils::usedMarginsForAxis() can resolve.
        auto gridItemHasUnsupportedMargins = [&] {
            return gridItemStyle->marginBox().anyOf([](const Style::MarginEdge& marginEdge) {
                return !marginEdge.isFixed() && !marginEdge.isKnownZero();
            });
        };
        if (gridItemHasUnsupportedMargins())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMargin, reasons, reasonCollectionMode);

        if (gridItemStyle->boxSizing() == BoxSizing::BorderBox)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasBorderBoxSizing, reasons, reasonCollectionMode);

        // Items inherit writing-mode and direction, so these are expressed relative to the grid container.
        auto gridItemWritingMode = gridItemStyle->writingMode();
        bool gridItemIsOrthogonal = gridItemWritingMode.isOrthogonal(gridContainerWritingMode);
        if (gridItemIsOrthogonal)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemIsOrthogonal, reasons, reasonCollectionMode);

        if (!gridItemIsOrthogonal && gridItemWritingMode.isBlockOpposing(gridContainerWritingMode))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasOpposingBlockFlow, reasons, reasonCollectionMode);

        if (!gridItemIsOrthogonal && gridItemWritingMode.isInlineOpposing(gridContainerWritingMode))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasOpposingInlineDirection, reasons, reasonCollectionMode);

        if (gridItem->isOutOfFlowPositioned())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasOutOfFlowChild, reasons, reasonCollectionMode);

        // A relatively (or sticky) positioned grid item resolves percentage inset offsets against
        // its containing block, which for a grid item is its grid area. GFC does not yet set the
        // grid-area size on the item, so such percentages would incorrectly resolve against the
        // grid container's content box. Keep these items on the legacy path.
        if (gridItemStyle->position() == PositionType::Relative || gridItemStyle->position() == PositionType::Sticky) {
            auto hasPercentageInsetOffset = gridItemStyle->insetBox().anyOf([](const Style::InsetEdge& insetEdge) {
                return insetEdge.isPercentOrCalculated();
            });
            if (hasPercentageInsetOffset)
                ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::RelativeGridItemHasPercentageInset, reasons, reasonCollectionMode);
        }

        // https://drafts.csswg.org/css-sizing-4/#aspect-ratio: the preferred aspect ratio only matters when an automatic size is
        // computed from the other axis, so it has no effect on an item whose width and height are both fixed.
        if (gridItemStyle->aspectRatio().hasRatio() && !(gridItemStyle->width().isFixed() && gridItemStyle->height().isFixed()))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasAspectRatio, reasons, reasonCollectionMode);

        if (!gridItemStyle->isOverflowVisible())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasNonVisibleOverflow, reasons, reasonCollectionMode);

        if (gridItemStyle->usedContain().contains(Style::ContainValue::Size))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasContainsSize, reasons, reasonCollectionMode);

        auto gridItemIsStretchedInBlockAxis = [&] {
            return gridItemHeight.isAuto() && usedAlignSelf.isStretchy(ItemPosition::Stretch);
        };

        auto gridItemHasChildWithInlineSizeComputedFromAspectRatio = [&] {
            for (CheckedRef gridItemChild : childrenOfType<RenderBox>(gridItem.get())) {
                CheckedRef gridItemChildStyle = gridItemChild->style();
                bool hasAspectRatio = isReplacedBox(gridItemChild) || gridItemChildStyle->aspectRatio().hasRatio();
                if (hasAspectRatio && gridItemChildStyle->width().isAuto() && gridItemChildStyle->height().isPercent())
                    return true;
            }
            return false;
        };

        // A stretched item's child can resolve its inline size from its aspect ratio against the item's
        // stretched block size, changing the item's inline contribution. This requires a second column
        // sizing pass, which GFC does not yet support.
        if (gridItemIsStretchedInBlockAxis() && gridItemHasChildWithInlineSizeComputedFromAspectRatio())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemNeedsSecondColumnSizingPass, reasons, reasonCollectionMode);
    }
    return reasons;
}

#ifndef NDEBUG
namespace GridCoverageInternal {
static void printTextForSubtree(const RenderElement& renderer, size_t& charactersLeft, TextStream& stream)
{
    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(renderer))) {
        if (is<RenderText>(child)) {
            auto text = downcast<RenderText>(child).text();
            auto textView = StringView { text }.trim(isASCIIWhitespace<char16_t>);
            auto length = std::min<size_t>(charactersLeft, textView.length());
            stream << textView.left(length);
            charactersLeft -= length;
            continue;
        }
        printTextForSubtree(downcast<RenderElement>(child), charactersLeft, stream);
    }
}
} // namespace GridCoverageInternal

static Vector<const RenderGrid*> collectGridsForCurrentPage()
{
    Vector<const RenderGrid*> grids;
    for (auto document : Document::allDocuments()) {
        if (!document->renderView() || document->backForwardCacheState() != Document::NotInBackForwardCache)
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        for (auto& descendant : descendantsOfType<RenderGrid>(*document->renderView()))
            grids.append(&descendant);
    }
    return grids;
}

static void printReason(GridAvoidanceReason reason, TextStream& stream)
{
    switch (reason) {
    case GridAvoidanceReason::GridFormattingContextIntegrationDisabled:
        stream << "grid formatting context integration is disabled";
        break;
    case GridAvoidanceReason::GridHasVerticalLRWritingMode:
        stream << "grid has vertical-lr writing mode";
        break;
    case GridAvoidanceReason::GridHasVerticalRLWritingMode:
        stream << "grid has vertical-rl writing mode";
        break;
    case GridAvoidanceReason::GridHasSidewaysWritingMode:
        stream << "grid has sideways-lr or sideways-rl writing mode";
        break;
    case GridAvoidanceReason::GridHasHorizontalBTWritingMode:
        stream << "grid has horizontal-bt writing mode";
        break;
    case GridAvoidanceReason::GridHasRTLDirection:
        stream << "grid has RTL direction";
        break;
    case GridAvoidanceReason::GridNeedsBaseline:
        stream << "inline grid needs baseline";
        break;
    case GridAvoidanceReason::GridHasOutOfFlowChild:
        stream << "grid has out-of-flow child";
        break;
    case GridAvoidanceReason::GridHasNonVisibleOverflow:
        stream << "grid has non-visible overflow";
        break;
    case GridAvoidanceReason::GridItemIsReplacedElement:
        stream << "grid item is a replaced element";
        break;
    case GridAvoidanceReason::GridItemIsSubgrid:
        stream << "grid item is a subgrid";
        break;
    case GridAvoidanceReason::GridHasGridTemplateAreas:
        stream << "grid has grid-template-areas";
        break;
    case GridAvoidanceReason::GridHasNonFixedGaps:
        stream << "grid has non-fixed gaps";
        break;
    case GridAvoidanceReason::GridIsOutOfFlow:
        stream << "grid is out-of-flow";
        break;
    case GridAvoidanceReason::GridHasSizeOrInlineSizeContainment:
        stream << "grid has size or inline-size containment";
        break;
    case GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns:
        stream << "grid has unsupported grid-template-columns";
        break;
    case GridAvoidanceReason::GridHasUnsupportedGridTemplateRows:
        stream << "grid has unsupported grid-template-rows";
        break;
    case GridAvoidanceReason::GridHasUnsupportedJustifyContent:
        stream << "grid has unsupported justify-content";
        break;
    case GridAvoidanceReason::GridHasUnsupportedAlignContent:
        stream << "grid has unsupported align-content";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedWidthValue:
        stream << "grid item has unsupported width value";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedAutomaticInlineSizing:
        stream << "grid item has unsupported automatic inline sizing";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedHeightValue:
        stream << "grid item has unsupported height value";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedAutomaticBlockSizing:
        stream << "grid item has unsupported automatic block sizing";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMaxWidth:
        stream << "grid item has unsupported max-width";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMaxHeight:
        stream << "grid item has unsupported max-height";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMargin:
        stream << "grid item has unsupported margin";
        break;
    case GridAvoidanceReason::GridItemHasBorderBoxSizing:
        stream << "grid item has border-box box-sizing";
        break;
    case GridAvoidanceReason::GridItemIsOrthogonal:
        stream << "grid item is orthogonal to the grid";
        break;
    case GridAvoidanceReason::GridItemHasOpposingBlockFlow:
        stream << "grid item has a block flow opposing the grid's";
        break;
    case GridAvoidanceReason::GridItemHasOpposingInlineDirection:
        stream << "grid item has an inline direction opposing the grid's";
        break;
    case GridAvoidanceReason::GridItemHasAspectRatio:
        stream << "grid item has aspect-ratio";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedInlineAxisAlignment:
        stream << "grid item has unsupported inline-axis alignment";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedBlockAxisAlignment:
        stream << "grid item has unsupported block-axis alignment";
        break;
    case GridAvoidanceReason::GridItemHasNonVisibleOverflow:
        stream << "grid item has non-visible overflow";
        break;
    case GridAvoidanceReason::GridItemHasContainsSize:
        stream << "grid item has contains: size";
        break;
    case GridAvoidanceReason::GridItemNeedsSecondColumnSizingPass:
        stream << "grid item needs second column sizing support";
        break;
    case GridAvoidanceReason::RelativeGridItemHasPercentageInset:
        stream << "relative grid item has percentage inset";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMinWidth:
        stream << "grid item has unsupported min-width";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMinHeight:
        stream << "grid item has unsupported min-height";
        break;
    case GridAvoidanceReason::GridHasUnsupportedMinHeight:
        stream << "grid container has unsupported min-height";
        break;
    case GridAvoidanceReason::GridHasUnsupportedMaxHeight:
        stream << "grid container has unsupported max-height";
        break;
    case GridAvoidanceReason::GridHasPercentageRowsWithIndefiniteHeight:
        stream << "grid has percentage rows with indefinite height";
        break;
    case GridAvoidanceReason::NotAGrid:
        stream << "not a grid";
        break;
    }
}

static void printReasons(EnumSet<GridAvoidanceReason> reasons, TextStream& stream)
{
    stream << " ";
    for (auto reason : reasons) {
        printReason(reason, stream);
        stream << ", ";
    }
}

static void printLegacyGridReasons()
{
    auto grids = collectGridsForCurrentPage();
    if (!grids.size()) {
        WTFLogAlways("No grid found in this document\n");
        return;
    }
    TextStream stream;
    stream << "---------------------------------------------------\n";
    for (auto* grid : grids) {
        auto reasons = gridLayoutAvoidanceReason(*grid, ReasonCollectionMode::All);
        if (reasons.isEmpty())
            continue;
        size_t printedLength = 30;
        stream << "\"";
        GridCoverageInternal::printTextForSubtree(*grid, printedLength, stream);
        stream << "...\"";
        for (; printedLength > 0; --printedLength)
            stream << " ";
        printReasons(reasons, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    SAFE_WTFLOGALWAYS("%s", stream.release().utf8());
}
#endif

bool canUseForGridLayout(const RenderGrid& renderGrid)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showLegacyGridReasons"_s, Function<void()> { printLegacyGridReasons });
    });
#endif
    auto reasons = gridLayoutAvoidanceReason(renderGrid, ReasonCollectionMode::FirstOnly);
    return reasons.isEmpty();
}

} // namespace LayoutIntegration
} // namespace WebCore

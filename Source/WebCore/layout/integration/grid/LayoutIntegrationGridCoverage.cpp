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
#include "RenderGrid.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "UnplacedGridItem.h"
#include <pal/Logging.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace LayoutIntegration {

enum class ReasonCollectionMode : bool {
    FirstOnly,
    All
};

enum class GridAvoidanceReason : uint8_t {
    GridHasUnsupportedWritingMode,
    GridHasRTLDirection, // http://webkit.org/b/317334
    GridHasMarginTrim,
    GridNeedsBaseline,
    GridHasOutOfFlowChild,
    GridHasNonVisibleOverflow,
    GridItemIsReplacedElement,
    GridItemDoesNotHaveElement,
    GridItemIsSubgrid,
    GridIsEmpty,
    GridHasGridTemplateAreas,
    GridHasColumnAutoFlow,
    GridHasNonFixedGaps,
    GridIsOutOfFlow,
    GridHasContainsSize,
    GridHasUnsupportedGridTemplateColumns,
    GridHasUnsupportedGridTemplateRows,
    GridHasUnsupportedJustifyContent,
    GridHasUnsupportedAlignContent,
    GridHasUnsupportedMinWidth,
    GridHasUnsupportedMaxWidth,
    GridHasUnsupportedMinHeight,
    GridHasUnsupportedMaxHeight,
    GridHasPercentageRowsWithIndefiniteHeight,
    GridItemHasNonInitialMaxWidth,
    GridItemHasNonInitialMaxHeight,
    GridItemHasMargin,
    GridItemHasBorderBoxSizing,
    GridItemHasUnsupportedWritingMode,
    GridItemHasRTLDirection,
    GridItemHasAspectRatio,
    GridItemHasUnsupportedInlineAxisAlignment,
    GridItemHasUnsupportedBlockAxisAlignment,
    GridItemHasNonVisibleOverflow,
    GridItemHasContainsSize,
    GridItemNeedsSecondColumnSizingPass,
    RelativeGridItemHasPercentageInset,

    GridItemColumnStartHasLineName,
    GridItemColumnStartHasSpan,
    GridItemHasColumnStartOutsideExplicitGrid,
    GridItemNeedsLeadingImplicitColumnsWithMultipleAutoColumns,
    GridItemNeedsTooManyLeadingImplicitColumns,
    GridItemHasUnsupportedColumnEnd,

    GridNeedsImplicitColumnsForItemsLockedToRow,
    GridItemRowStartHasLineName,
    GridItemRowStartHasSpan,
    GridItemHasRowStartOutsideExplicitGrid,
    GridItemNeedsLeadingImplicitRowsWithMultipleAutoRows,
    GridItemNeedsTooManyLeadingImplicitRows,
    GridItemHasUnsupportedRowEnd,

    GridItemHasUnsupportedWidthValue,
    GridItemHasUnsupportedAutomaticInlineSizing,
    GridItemHasUnsupportedHeightValue,
    GridItemHasUnsupportedAutomaticBlockSizing,
    GridItemHasUnsupportedMinWidth,
    GridItemHasUnsupportedMinHeight,
    NotAGrid,
    GridFormattingContextIntegrationDisabled,
};

#if ASSERT_ENABLED
static bool avoidanceReasonIsColumnPlacementRelated(GridAvoidanceReason gridAvoidanceReason)
{
    switch (gridAvoidanceReason) {
    case GridAvoidanceReason::GridItemColumnStartHasLineName:
    case GridAvoidanceReason::GridItemColumnStartHasSpan:
    case GridAvoidanceReason::GridItemHasColumnStartOutsideExplicitGrid:
    case GridAvoidanceReason::GridItemNeedsLeadingImplicitColumnsWithMultipleAutoColumns:
    case GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitColumns:
    case GridAvoidanceReason::GridItemHasUnsupportedColumnEnd:
        return true;
    default:
        return false;
    }
}

static bool avoidanceReasonIsRowPlacementRelated(GridAvoidanceReason gridAvoidanceReason)
{
    switch (gridAvoidanceReason) {
    case GridAvoidanceReason::GridItemRowStartHasLineName:
    case GridAvoidanceReason::GridItemRowStartHasSpan:
    case GridAvoidanceReason::GridItemHasRowStartOutsideExplicitGrid:
    case GridAvoidanceReason::GridItemNeedsLeadingImplicitRowsWithMultipleAutoRows:
    case GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitRows:
    case GridAvoidanceReason::GridItemHasUnsupportedRowEnd:
        return true;
    default:
        return false;
    }
}
#endif

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

// GFC's implicit grid is a dense matrix: ImplicitGrid's constructor allocates every cell of a
// rows x columns matrix up front. The legacy Grid instead grows a row's storage only once that row
// is used (see Grid::ensureStorageForRow), so it tolerates a sparse grid with a huge track count
// that GFC cannot. A negative line may resolve as far as Style::GridPosition::min() lines before
// the explicit grid, so leaving the leading implicit track count unbounded would let content force
// a quadratic allocation. Cap what GFC takes on and leave the rest to the legacy path.
static constexpr int maximumLeadingImplicitTracksCount = 10;

// Resolves the 0-based line range for an axis whose start references an explicit line, using the
// same mapping GFC applies in UnplacedGridItem::resolveDefinitePosition(). A negative CSS line
// counts backward from the end of the explicit grid, so the resolved start line may be negative
// when the placement counts past the start edge; those lines generate leading implicit tracks.
static std::pair<int, int> resolveExplicitStartLineRange(const Style::GridPosition& explicitStart, const Style::GridPosition& end, size_t explicitTrackCount)
{
    ASSERT(explicitStart.isExplicit());
    auto range = Layout::UnplacedGridItem::resolveDefinitePosition(explicitStart, end, explicitTrackCount);
    ASSERT(range);
    return *range;
}

// Resolves a start line that references an explicit line to its 0-based index. Whenever the start
// is explicit its resolved line does not depend on the end at all, so pairing it with an auto end
// makes this safe to call before the end kind has been validated.
static int resolveExplicitStartLine(const Style::GridPosition& explicitStart, size_t explicitTrackCount)
{
    return resolveExplicitStartLineRange(explicitStart, CSS::Keyword::Auto { }, explicitTrackCount).first;
}

static bool hasValidColumnEnd(const Style::GridPosition& explicitColumnStart, const Style::GridPosition columnEnd, size_t explicitColumnCount)
{
    return WTF::switchOn(columnEnd,
        [](const CSS::Keyword::Auto&) {
            return false;
        },
        [&](const Style::GridPositionExplicit&) {
            if (!columnEnd.namedGridLine().value.isEmpty())
                return false;

            auto [startLine, endLine] = resolveExplicitStartLineRange(explicitColumnStart, columnEnd, explicitColumnCount);

            // A negative end line may resolve before the start of the explicit grid, which generates
            // leading implicit columns and is supported. Lines past the end edge are not.
            if (endLine > static_cast<int>(explicitColumnCount))
                return false;

            // FIXME: Multi-span items are not yet supported in intrinsic sizing
            // (see TrackSizingAlgorithm::sizeTracksForIntrinsicSizing).
            // Only accept items that span a single column.
            return endLine - startLine == 1;
        },
        [&](const Style::GridPositionSpan&) {
            return false;
        },
        [&](const Style::CustomIdent&) {
            return false;
        }
    );
}

static bool hasValidColumnEnd(const CSS::Keyword::Auto& autoColumnStart, const Style::GridPosition columnEnd)
{
    UNUSED_PARAM(autoColumnStart);

    return WTF::switchOn(columnEnd,
        [](const CSS::Keyword::Auto&) {
            return true;
        },
        [](const Style::GridPositionExplicit&) {
            return false;
        },
        [](const Style::GridPositionSpan&) {
            return false;
        },
        [](const Style::CustomIdent&) {
            return false;
        }
    );
}

static bool hasValidRowEnd(const CSS::Keyword::Auto& autoRowStart, const Style::GridPosition rowEnd)
{
    UNUSED_PARAM(autoRowStart);

    return WTF::switchOn(rowEnd,
        [](const CSS::Keyword::Auto&) {
            return true;
        },
        [](const Style::GridPositionExplicit&) {
            return false;
        },
        [](const Style::GridPositionSpan&) {
            return false;
        },
        [](const Style::CustomIdent&) {
            return false;
        }
    );
}

static bool hasValidRowEnd(const Style::GridPosition& explicitRowStart, const Style::GridPosition rowEnd, size_t explicitRowCount)
{
    return WTF::switchOn(rowEnd,
        [&](const CSS::Keyword::Auto&) {
            return true;
        },
        [&](const Style::GridPositionExplicit&) {
            if (!rowEnd.namedGridLine().value.isEmpty())
                return false;

            auto [startLine, endLine] = resolveExplicitStartLineRange(explicitRowStart, rowEnd, explicitRowCount);

            // A negative end line may resolve before the start of the explicit grid, which generates
            // leading implicit rows and is supported. Lines past the end edge are not.
            if (endLine > static_cast<int>(explicitRowCount))
                return false;

            // FIXME: Multi-span items are not yet supported in intrinsic sizing
            // (see TrackSizingAlgorithm::sizeTracksForIntrinsicSizing).
            // Only accept items that span a single row.
            return endLine - startLine == 1;
        },
        [&](const Style::GridPositionSpan&) {
            return false;
        },
        [&](const Style::CustomIdent&) {
            return false;
        }
    );
}

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

// A grid item with an automatic size which is not stretched is sized to its fit-content size in
// the axis. GFC does not implement the remaining cases from grid item sizing, where a replaced
// item uses its natural size and an item with a preferred aspect ratio transfers its size through
// that ratio.
// https://drafts.csswg.org/css-grid-1/#grid-item-sizing
static bool canComputeAutomaticSize(const RenderBox& gridItem)
{
    return !protect(gridItem.element())->isReplaced()
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

    if (!renderGridStyle->writingMode().isHorizontal() || renderGridStyle->writingMode().isBlockFlipped())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedWritingMode, reasons, reasonCollectionMode);

    if (renderGridStyle->writingMode().bidiDirection() == TextDirection::RTL)
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasRTLDirection, reasons, reasonCollectionMode);

    if (!renderGridStyle->marginTrim().isNone())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasMarginTrim, reasons, reasonCollectionMode);

    if (!renderGridStyle->isOverflowVisible())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasNonVisibleOverflow, reasons, reasonCollectionMode);

    if (!renderGrid.firstInFlowChild())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridIsEmpty, reasons, reasonCollectionMode);

    // GFC currently supports grid-auto-flow: row and row dense
    // Column auto-flow is not yet supported
    auto gridAutoFlow = renderGridStyle->gridAutoFlow();
    if (gridAutoFlow.isColumn())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasColumnAutoFlow, reasons, reasonCollectionMode);

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
    if (gridTemplateColumnsTrackList.isEmpty())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns, reasons, reasonCollectionMode);

    for (auto& columnsTrackListEntry : gridTemplateColumnsTrackList) {
        auto avoidanceReason = WTF::switchOn(columnsTrackListEntry,
            [&](const Style::GridTrackSize& trackSize) -> std::optional<GridAvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;
                return { };
            },
            [&](const Style::GridLineNames& names) -> std::optional<GridAvoidanceReason> {
                if (!names.isEmpty())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;
                return std::nullopt;
            },
            [&](const Style::GridTrackEntryRepeat&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns);
            },
            [&](const Style::GridTrackEntryAutoRepeat&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns);
            },
            [&](const Style::GridTrackEntrySubgrid&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns);
            }
        );

        if (avoidanceReason) {
            reasons.add(*avoidanceReason);
            if (reasonCollectionMode == ReasonCollectionMode::FirstOnly)
                return reasons;
        }
    }

    auto& gridTemplateRows = renderGridStyle->gridTemplateRows();
    auto& gridTemplateRowsTrackList = gridTemplateRows.list;

    for (auto& rowsTrackListEntry : gridTemplateRowsTrackList) {
        auto avoidanceReason = WTF::switchOn(rowsTrackListEntry,
            [&](const Style::GridTrackSize& trackSize) -> std::optional<GridAvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;
                return { };
            },
            [&](const Style::GridLineNames& names) -> std::optional<GridAvoidanceReason> {
                if (!names.isEmpty())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;
                return std::nullopt;
            },
            [&](const Style::GridTrackEntryRepeat&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateRows);
            },
            [&](const Style::GridTrackEntryAutoRepeat&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateRows);
            },
            [&](const Style::GridTrackEntrySubgrid&) {
                return std::make_optional(GridAvoidanceReason::GridHasUnsupportedGridTemplateRows);
            }
        );

        if (avoidanceReason) {
            reasons.add(*avoidanceReason);
            if (reasonCollectionMode == ReasonCollectionMode::FirstOnly)
                return reasons;
        }
    }

    if (renderGridStyle->usedContain().contains(Style::ContainValue::Size))
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasContainsSize, reasons, reasonCollectionMode);

    if (!renderGridStyle->justifyContent().isNormal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedJustifyContent, reasons, reasonCollectionMode);

    if (!renderGridStyle->alignContent().isNormal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedAlignContent, reasons, reasonCollectionMode);

    // GFC cannot yet resolve intrinsic (min-content/max-content/fit-content) sizing on the grid container itself.
    if (renderGridStyle->minWidth().isIntrinsic())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedMinWidth, reasons, reasonCollectionMode);

    if (renderGridStyle->maxWidth().isIntrinsic())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasUnsupportedMaxWidth, reasons, reasonCollectionMode);

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
    auto hasPercentageRowTrack = [&] {
        return gridTemplateRowsTrackList.containsIf([&](const auto& rowsTrackListEntry) {
            if (auto* trackSize = std::get_if<Style::GridTrackSize>(&rowsTrackListEntry))
                return trackSize->minTrackBreadth().isPercentOrCalculated() || trackSize->maxTrackBreadth().isPercentOrCalculated();
            return false;
        });
    };
    if (gridBlockSizeIsIndefinite && hasPercentageRowTrack())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridHasPercentageRowsWithIndefiniteHeight, reasons, reasonCollectionMode);

    ASSERT(renderGridStyle->gridAutoFlow().isRow(),
        "If we end up supporting column auto flow before broader implicit grid support then the logic using explicitlyPlacedItemsInRowCount will need to be reworked to be based upon the auto flow direction");
    // Keyed by the resolved 0-based row line, which is negative for placements that count past the
    // start edge of the explicit grid, so zero is a valid key.
    HashMap<int, size_t, DefaultHash<int>, WTF::SignedWithZeroKeyHashTraits<int>> explicitlyPlacedItemsInRowCount;

    for (CheckedRef gridItem : childrenOfType<RenderBox>(renderGrid)) {
        // We do not yet support grid item sizing spec for replaced elements.
        // See: https://drafts.csswg.org/css-grid/#grid-item-sizing
        RefPtr gridItemElement = gridItem->element();
        if (!gridItemElement)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemDoesNotHaveElement, reasons, reasonCollectionMode);

        if (gridItemElement->isReplaced())
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

        if (!gridItemStyle->maxWidth().isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasNonInitialMaxWidth, reasons, reasonCollectionMode);

        auto& minHeight = gridItemStyle->minHeight();
        if (!minHeight.isFixed() && !minHeight.isAuto())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedMinHeight, reasons, reasonCollectionMode);

        if (!gridItemStyle->maxHeight().isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasNonInitialMaxHeight, reasons, reasonCollectionMode);

        auto gridItemHasMargins = [&] {
            return gridItemStyle->marginBox().anyOf([](const Style::MarginEdge& marginEdge) {
                return marginEdge.isAuto() || marginEdge.isCalculated() || !marginEdge.isPossiblyZero();
            });
        };
        if (gridItemHasMargins())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasMargin, reasons, reasonCollectionMode);

        if (gridItemStyle->boxSizing() == BoxSizing::BorderBox)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasBorderBoxSizing, reasons, reasonCollectionMode);

        auto explicitColumnCount = gridTemplateColumns.sizes.size();
        auto explicitRowCount = gridTemplateRows.sizes.size();
        auto& columnStart = gridItemStyle->gridItemColumnStart();
        auto columnPositioningAvoidanceReason = WTF::switchOn(columnStart,
            [&](const CSS::Keyword::Auto& autoPosition) -> std::optional<GridAvoidanceReason> {
                auto& columnEnd = gridItemStyle->gridItemColumnEnd();
                if (!hasValidColumnEnd(autoPosition, columnEnd))
                    return GridAvoidanceReason::GridItemHasUnsupportedColumnEnd;
                return { };
            },
            [&](const Style::GridPositionExplicit&) -> std::optional<GridAvoidanceReason> {
                if (!columnStart.namedGridLine().value.isEmpty())
                    return GridAvoidanceReason::GridItemColumnStartHasLineName;

                auto columnStartLine = resolveExplicitStartLine(columnStart, explicitColumnCount);
                if (columnStartLine > static_cast<int>(explicitColumnCount))
                    return GridAvoidanceReason::GridItemHasColumnStartOutsideExplicitGrid;

                auto& columnEnd = gridItemStyle->gridItemColumnEnd();
                if (!hasValidColumnEnd(columnStart, columnEnd, explicitColumnCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedColumnEnd;

                // A start line resolving before the explicit grid generates leading implicit columns.
                // The item's own line is the most negative of its two, so capping per item also caps
                // the count GridFormattingContext::computeLeadingImplicitTracks() arrives at.
                if (columnStartLine < -maximumLeadingImplicitTracksCount)
                    return GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitColumns;

                // Per spec leading implicit tracks cycle grid-auto-columns backwards from the
                // explicit grid, but GridLayout::generateImplicitTrackSizingFunctions() always
                // cycles forwards, so it only produces the right sizes when there is a single track
                // size to cycle through.
                if (columnStartLine < 0 && renderGridStyle->gridAutoColumns().size() > 1)
                    return GridAvoidanceReason::GridItemNeedsLeadingImplicitColumnsWithMultipleAutoColumns;
                return { };
            },
            [&](const Style::GridPositionSpan&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemColumnStartHasSpan;
            },
            [&](const Style::CustomIdent&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemColumnStartHasLineName;
            }
        );

        if (columnPositioningAvoidanceReason) {
            ASSERT(avoidanceReasonIsColumnPlacementRelated(*columnPositioningAvoidanceReason));
            ADD_REASON_AND_RETURN_IF_NEEDED(*columnPositioningAvoidanceReason, reasons, reasonCollectionMode);
        }

        auto& rowStart = gridItemStyle->gridItemRowStart();
        auto rowPositioningAvoidanceReason = WTF::switchOn(rowStart,
            [&](const CSS::Keyword::Auto& autoPosition) -> std::optional<GridAvoidanceReason> {
                if (!hasValidRowEnd(autoPosition, gridItemStyle->gridItemRowEnd()))
                    return GridAvoidanceReason::GridItemHasUnsupportedRowEnd;
                return { };
            },
            [&](const Style::GridPositionExplicit&) -> std::optional<GridAvoidanceReason> {
                if (!rowStart.namedGridLine().value.isEmpty())
                    return GridAvoidanceReason::GridItemRowStartHasLineName;

                auto rowStartLine = resolveExplicitStartLine(rowStart, explicitRowCount);
                if (rowStartLine > static_cast<int>(explicitRowCount))
                    return GridAvoidanceReason::GridItemHasRowStartOutsideExplicitGrid;

                auto& rowEnd = gridItemStyle->gridItemRowEnd();
                if (!hasValidRowEnd(rowStart, rowEnd, explicitRowCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedRowEnd;

                // See the grid-auto-columns comments above for why leading implicit rows are capped
                // and why they need a single grid-auto-rows track size.
                if (rowStartLine < -maximumLeadingImplicitTracksCount)
                    return GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitRows;

                if (rowStartLine < 0 && renderGridStyle->gridAutoRows().size() > 1)
                    return GridAvoidanceReason::GridItemNeedsLeadingImplicitRowsWithMultipleAutoRows;

                // Key by the resolved line rather than the specified one so that two placements
                // naming the same row through different line numbers (e.g. 1 and -4 on a grid with
                // three explicit rows) share a bucket.
                ++explicitlyPlacedItemsInRowCount.add(rowStartLine, 0).iterator->value;
                return { };
            },
            [&](const Style::GridPositionSpan&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemRowStartHasSpan;
            },
            [&](const Style::CustomIdent&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemRowStartHasLineName;
            }
        );

        if (rowPositioningAvoidanceReason) {
            ASSERT(avoidanceReasonIsRowPlacementRelated(*rowPositioningAvoidanceReason));
            ADD_REASON_AND_RETURN_IF_NEEDED(*rowPositioningAvoidanceReason, reasons, reasonCollectionMode);
        }

        // If there are too many items in a given row compared to the total number of columns in the
        // explicit grid, then we may need to add additional columns to the implicit grid to place
        // them properly. We can be more fine grained than what we are doing now, but this is
        // a good start as we allow more complex placements.
        auto hasRowWithTooManyExplicitlyPlacedItems = [&] {
            for (auto itemsInRowCount : explicitlyPlacedItemsInRowCount.values()) {
                if (itemsInRowCount >= explicitColumnCount + 1)
                    return true;
            }
            return false;
        };
        if (hasRowWithTooManyExplicitlyPlacedItems())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridNeedsImplicitColumnsForItemsLockedToRow, reasons, reasonCollectionMode);

        if (gridItemStyle->writingMode().isVertical() || gridItemStyle->writingMode().isBlockFlipped())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasUnsupportedWritingMode, reasons, reasonCollectionMode);

        if (gridItemStyle->writingMode().bidiDirection() == TextDirection::RTL)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridAvoidanceReason::GridItemHasRTLDirection, reasons, reasonCollectionMode);

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

        if (gridItemStyle->aspectRatio().hasRatio())
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
                RefPtr gridItemChildElement = gridItemChild->element();
                bool hasAspectRatio = (gridItemChildElement && gridItemChildElement->isReplaced()) || gridItemChildStyle->aspectRatio().hasRatio();
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
    case GridAvoidanceReason::GridHasUnsupportedWritingMode:
        stream << "grid has unsupported writing mode";
        break;
    case GridAvoidanceReason::GridHasRTLDirection:
        stream << "grid has RTL direction";
        break;
    case GridAvoidanceReason::GridHasMarginTrim:
        stream << "grid has margin-trim";
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
    case GridAvoidanceReason::GridItemDoesNotHaveElement:
        stream << "grid item does not have a corresponding element";
        break;
    case GridAvoidanceReason::GridItemIsReplacedElement:
        stream << "grid item is a replaced element";
        break;
    case GridAvoidanceReason::GridItemIsSubgrid:
        stream << "grid item is a subgrid";
        break;
    case GridAvoidanceReason::GridIsEmpty:
        stream << "grid is empty";
        break;
    case GridAvoidanceReason::GridHasGridTemplateAreas:
        stream << "grid has grid-template-areas";
        break;
    case GridAvoidanceReason::GridHasColumnAutoFlow:
        stream << "grid has column auto-flow";
        break;
    case GridAvoidanceReason::GridHasNonFixedGaps:
        stream << "grid has non-fixed gaps";
        break;
    case GridAvoidanceReason::GridIsOutOfFlow:
        stream << "grid is out-of-flow";
        break;
    case GridAvoidanceReason::GridHasContainsSize:
        stream << "grid has contains: size";
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
    case GridAvoidanceReason::GridItemHasNonInitialMaxWidth:
        stream << "grid item has non-initial max-width";
        break;
    case GridAvoidanceReason::GridItemHasNonInitialMaxHeight:
        stream << "grid item has non-initial max-height";
        break;
    case GridAvoidanceReason::GridItemHasMargin:
        stream << "grid item has margin";
        break;
    case GridAvoidanceReason::GridItemHasBorderBoxSizing:
        stream << "grid item has border-box box-sizing";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedWritingMode:
        stream << "grid item has unsupported writing mode";
        break;
    case GridAvoidanceReason::GridItemHasRTLDirection:
        stream << "grid item has RTL direction";
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
    case GridAvoidanceReason::GridItemColumnStartHasLineName:
        stream << "grid item column start has line name";
        break;
    case GridAvoidanceReason::GridItemColumnStartHasSpan:
        stream << "grid item column start has span";
        break;
    case GridAvoidanceReason::GridItemHasColumnStartOutsideExplicitGrid:
        stream << "grid item has column start outside explicit grid";
        break;
    case GridAvoidanceReason::GridItemNeedsLeadingImplicitColumnsWithMultipleAutoColumns:
        stream << "grid item needs leading implicit columns with multiple grid-auto-columns track sizes";
        break;
    case GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitColumns:
        stream << "grid item needs too many leading implicit columns";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedColumnEnd:
        stream << "grid item has unsupported column end";
        break;
    case GridAvoidanceReason::GridNeedsImplicitColumnsForItemsLockedToRow:
        stream << "grid needs implicit columns for items locked to row";
        break;
    case GridAvoidanceReason::GridItemRowStartHasLineName:
        stream << "grid item row start has line name";
        break;
    case GridAvoidanceReason::GridItemRowStartHasSpan:
        stream << "grid item row start has span";
        break;
    case GridAvoidanceReason::GridItemHasRowStartOutsideExplicitGrid:
        stream << "grid item has row start outside explicit grid";
        break;
    case GridAvoidanceReason::GridItemNeedsLeadingImplicitRowsWithMultipleAutoRows:
        stream << "grid item needs leading implicit rows with multiple grid-auto-rows track sizes";
        break;
    case GridAvoidanceReason::GridItemNeedsTooManyLeadingImplicitRows:
        stream << "grid item needs too many leading implicit rows";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedRowEnd:
        stream << "grid item has unsupported row end";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMinWidth:
        stream << "grid item has unsupported min-width";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedMinHeight:
        stream << "grid item has unsupported min-height";
        break;
    case GridAvoidanceReason::GridHasUnsupportedMinWidth:
        stream << "grid container has unsupported min-width";
        break;
    case GridAvoidanceReason::GridHasUnsupportedMaxWidth:
        stream << "grid container has unsupported max-width";
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
    WTFLogAlways("%s", stream.release().utf8().data());
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

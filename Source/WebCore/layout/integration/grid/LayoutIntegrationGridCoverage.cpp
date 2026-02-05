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

#include "Document.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
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
    GridHasNonFixedWidth,
    GridHasNonFixedHeight,
    GridHasVerticalWritingMode,
    GridHasMarginTrim,
    GridNeedsBaseline,
    GridHasOutOfFlowChild,
    GridHasNonVisibleOverflow,
    GridHasUnsupportedRenderer,
    GridIsEmpty,
    GridHasNonInitialMinWidth,
    GridHasNonInitialMaxWidth,
    GridHasNonInitialMinHeight,
    GridHasNonInitialMaxHeight,
    GridHasNonZeroMinWidth,
    GridHasGridTemplateAreas,
    GridHasColumnAutoFlow,
    GridHasNonFixedGaps,
    GridIsOutOfFlow,
    GridHasContainsSize,
    GridHasUnsupportedGridTemplateColumns,
    GridHasUnsupportedGridTemplateRows,
    GridItemHasNonFixedWidth,
    GridItemHasNonFixedHeight,
    GridItemHasNonInitialMaxWidth,
    GridItemHasNonZeroMinHeight,
    GridItemHasNonInitialMaxHeight,
    GridItemHasBorder,
    GridItemHasPadding,
    GridItemHasMargin,
    GridItemHasVerticalWritingMode,
    GridItemHasAspectRatio,
    GridItemHasUnsupportedInlineAxisAlignment,
    GridItemHasUnsupportedBlockAxisAlignment,
    GridItemHasNonVisibleOverflow,
    GridItemHasContainsSize,
    GridItemHasUnsupportedColumnPlacement,
    GridItemHasUnsupportedRowPlacement,
    NotAGrid,
    GridFormattingContextIntegrationDisabled,
};

#ifndef NDEBUG
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, reasonCollectionMode) { \
        reasons.add(GridAvoidanceReason::reason); \
        if (reasonCollectionMode == ReasonCollectionMode::FirstOnly) \
            return reasons; \
    }
#else
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, reasonCollectionMode) { \
        ASSERT_UNUSED(reasonCollectionMode, reasonCollectionMode == ReasonCollectionMode::FirstOnly); \
        reasons.add(GridAvoidanceReason::reason); \
        return reasons; \
    }
#endif


static bool hasValidColumnEnd(const Style::GridPositionExplicit& explicitColumnStart, const Style::GridPosition columnEnd, size_t linesFromGridTemplateColumnsCount)
{
    UNUSED_PARAM(explicitColumnStart);

    return WTF::switchOn(columnEnd,
        [&](const CSS::Keyword::Auto&) {
            return false;
        },
        [&](const Style::GridPositionExplicit&) {
            if (!columnEnd.namedGridLine().isEmpty() || columnEnd.explicitPosition() < 0 || columnEnd.explicitPosition() > static_cast<int>(linesFromGridTemplateColumnsCount))
                return false;
            return true;
        },
        [&](const Style::GridPositionSpan&) {
            return false;
        },
        [&](const CustomIdentifier&) {
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
        [](const CustomIdentifier&) {
            return false;
        }
    );
}

static bool hasValidRowEnd(const Style::GridPositionExplicit&, const Style::GridPosition rowEnd, size_t linesFromGridTemplateRowsCount)
{
    return WTF::switchOn(rowEnd,
        [&](const CSS::Keyword::Auto&) {
            return false;
        },
        [&](const Style::GridPositionExplicit&) {
            if (!rowEnd.namedGridLine().isEmpty() || rowEnd.explicitPosition() < 0 || rowEnd.explicitPosition() > static_cast<int>(linesFromGridTemplateRowsCount))
                return false;
            return true;
        },
        [&](const Style::GridPositionSpan&) {
            return false;
        },
        [&](const CustomIdentifier&) {
            return false;
        }
    );
}

static EnumSet<GridAvoidanceReason> gridLayoutAvoidanceReason(const RenderGrid& renderGrid, ReasonCollectionMode reasonCollectionMode)
{
    auto reasons = EnumSet<GridAvoidanceReason> { };

    if (!renderGrid.document().settings().gridFormattingContextIntegrationEnabled())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridFormattingContextIntegrationDisabled, reasons, reasonCollectionMode);

    CheckedRef renderGridStyle = renderGrid.style();

    if (!renderGridStyle->width().isFixed())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonFixedWidth, reasons, reasonCollectionMode);

    if (!renderGridStyle->height().isFixed())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonFixedHeight, reasons, reasonCollectionMode);

    if (renderGridStyle->display() == DisplayType::InlineGrid)
        ADD_REASON_AND_RETURN_IF_NEEDED(GridNeedsBaseline, reasons, reasonCollectionMode);

    if (renderGridStyle->display() != DisplayType::Grid)
        ADD_REASON_AND_RETURN_IF_NEEDED(NotAGrid, reasons, reasonCollectionMode);

    if (!renderGridStyle->writingMode().isHorizontal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasVerticalWritingMode, reasons, reasonCollectionMode);

    if (!renderGridStyle->marginTrim().isNone())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasMarginTrim, reasons, reasonCollectionMode);

    if (!renderGridStyle->isOverflowVisible())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonVisibleOverflow, reasons, reasonCollectionMode);

    if (!renderGrid.firstInFlowChild())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridIsEmpty, reasons, reasonCollectionMode);

    // GFC currently supports grid-auto-flow: row and row dense
    // Column auto-flow is not yet supported
    auto gridAutoFlow = renderGridStyle->gridAutoFlow();
    if (gridAutoFlow.isColumn())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasColumnAutoFlow, reasons, reasonCollectionMode);

    // Check for non-fixed gaps. GFC currently only supports fixed-length gaps.
    if (!renderGridStyle->rowGap().isNormal()) {
        if (!renderGridStyle->rowGap().tryFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonFixedGaps, reasons, reasonCollectionMode);
    }

    if (!renderGridStyle->columnGap().isNormal()) {
        if (!renderGridStyle->columnGap().tryFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonFixedGaps, reasons, reasonCollectionMode);
    }

    if (renderGrid.isOutOfFlowPositioned())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridIsOutOfFlow, reasons, reasonCollectionMode);

    if (!renderGridStyle->gridTemplateAreas().isNone())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasGridTemplateAreas, reasons, reasonCollectionMode);

    auto& gridTemplateColumns = renderGridStyle->gridTemplateColumns();
    auto& gridTemplateColumnsTrackList = gridTemplateColumns.list;
    if (gridTemplateColumnsTrackList.isEmpty())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasUnsupportedGridTemplateColumns, reasons, reasonCollectionMode);

    for (auto& columnsTrackListEntry : gridTemplateColumnsTrackList) {
        auto avoidanceReason = WTF::switchOn(columnsTrackListEntry,
            [&](const Style::GridTrackSize& trackSize) -> std::optional<GridAvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;

                auto& minBreadth = trackSize.minTrackBreadth();

                if (minBreadth.isLength()) {
                    auto& gridTrackBreadthLength = minBreadth.length();
                    // Length types like auto, min-content, max-content, etc. not yet supported
                    if (!gridTrackBreadthLength.isFixed())
                        return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;
                    return std::nullopt;
                }

                if (minBreadth.isFlex()) {
                    // Flex tracks (fr units) are now supported
                    return std::nullopt;
                }

                ASSERT_NOT_REACHED();
                return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;
            },
            [&](const Vector<String>& names) -> std::optional<GridAvoidanceReason> {
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
    if (gridTemplateRowsTrackList.isEmpty())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasUnsupportedGridTemplateRows, reasons, reasonCollectionMode);

    for (auto& rowsTrackListEntry : gridTemplateRowsTrackList) {
        auto avoidanceReason = WTF::switchOn(rowsTrackListEntry,
            [&](const Style::GridTrackSize& trackSize) -> std::optional<GridAvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;

                auto& minBreadth = trackSize.minTrackBreadth();

                if (minBreadth.isLength()) {
                    auto& gridTrackBreadthLength = minBreadth.length();
                    // Length types like auto, min-content, max-content, etc. not yet supported
                    if (!gridTrackBreadthLength.isFixed())
                        return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;
                    return std::nullopt;
                }

                if (minBreadth.isFlex()) {
                    // Flex tracks (fr units) are now supported
                    return std::nullopt;
                }

                ASSERT_NOT_REACHED();
                return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;
            },
            [&](const Vector<String>& names) -> std::optional<GridAvoidanceReason> {
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
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasContainsSize, reasons, reasonCollectionMode);

    ASSERT(renderGridStyle->gridAutoFlow().isRow(),
        "If we end up supporting column auto flow before broader implicit grid support then the logic using explicitlyPlacedItemsInRowCount will need to be reworked to be based upon the auto flow direction");
    Vector<size_t> explicitlyPlacedItemsInRowCount;

    for (CheckedRef gridItem : childrenOfType<RenderBox>(renderGrid)) {
        if (!gridItem->isRenderBlockFlow())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasUnsupportedRenderer, reasons, reasonCollectionMode);

        CheckedRef gridItemStyle = gridItem->style();

        if (!gridItemStyle->width().isFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonFixedWidth, reasons, reasonCollectionMode);

        if (!gridItemStyle->height().isFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonFixedHeight, reasons, reasonCollectionMode);

        if (auto fixedMinWidth = gridItemStyle->minWidth().tryFixed(); fixedMinWidth && fixedMinWidth->unresolvedValue())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonZeroMinWidth, reasons, reasonCollectionMode);

        if (!gridItemStyle->maxWidth().isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonInitialMaxWidth, reasons, reasonCollectionMode);

        if (auto fixedMinHeight = gridItemStyle->minHeight().tryFixed(); fixedMinHeight && fixedMinHeight->unresolvedValue())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonZeroMinHeight, reasons, reasonCollectionMode);

        if (!gridItemStyle->maxHeight().isNone())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonInitialMaxHeight, reasons, reasonCollectionMode);

        if (gridItemStyle->border().hasBorder())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasBorder, reasons, reasonCollectionMode);

        auto gridItemHasPadding = [&] {
            return gridItemStyle->paddingBox().anyOf([](const Style::PaddingEdge& paddingEdge) {
                return !paddingEdge.isPossiblyZero();
            });
        };
        if (gridItemHasPadding())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasPadding, reasons, reasonCollectionMode);

        auto gridItemHasMargins = [&] {
            return gridItemStyle->marginBox().anyOf([](const Style::MarginEdge& marginEdge) {
                return !marginEdge.isPossiblyZero();
            });
        };
        if (gridItemHasMargins())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasMargin, reasons, reasonCollectionMode);

        auto justifySelf = gridItemStyle->justifySelf().resolve();
        if (justifySelf.position() != ItemPosition::Start && justifySelf.overflow() != OverflowAlignment::Default
            && justifySelf.positionType() != ItemPositionType::NonLegacy)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedInlineAxisAlignment, reasons, reasonCollectionMode);

        auto alignSelf = gridItemStyle->alignSelf().resolve();
        if (alignSelf.position() != ItemPosition::Start && alignSelf.overflow() != OverflowAlignment::Default
            && alignSelf.positionType() != ItemPositionType::NonLegacy)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedBlockAxisAlignment, reasons, reasonCollectionMode);

        auto linesFromGridTemplateColumnsCount = gridTemplateColumns.sizes.size() + 1;
        auto linesFromGridTemplateRowsCount = gridTemplateRows.sizes.size() + 1;
        auto& columnStart = gridItemStyle->gridItemColumnStart();
        auto columnPositioningAvoidanceReason = WTF::switchOn(columnStart,
            [&](const CSS::Keyword::Auto& autoPosition) -> std::optional<GridAvoidanceReason> {
                auto& columnEnd = gridItemStyle->gridItemColumnEnd();
                if (!hasValidColumnEnd(autoPosition, columnEnd))
                    return GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement;
                return { };
            },
            [&](const Style::GridPositionExplicit& explicitPosition) -> std::optional<GridAvoidanceReason> {
                if (!columnStart.namedGridLine().isEmpty() || columnStart.explicitPosition() < 0 || columnStart.explicitPosition() > static_cast<int>(linesFromGridTemplateColumnsCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement;
                if (!hasValidColumnEnd(explicitPosition, gridItemStyle->gridItemColumnEnd(), linesFromGridTemplateColumnsCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement;
                return { };
            },
            [&](const Style::GridPositionSpan&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement;
            },
            [&](const CustomIdentifier&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement;
            }
        );

        if (columnPositioningAvoidanceReason) {
            ASSERT(columnPositioningAvoidanceReason == GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement);
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedColumnPlacement, reasons, reasonCollectionMode);
        }

        auto& rowStart = gridItemStyle->gridItemRowStart();
        auto rowPositioningAvoidanceReason = WTF::switchOn(rowStart,
            [&](const CSS::Keyword::Auto&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemHasUnsupportedRowPlacement;
            },
            [&](const Style::GridPositionExplicit& explicitPosition) -> std::optional<GridAvoidanceReason> {
                auto rowStartLineNumber = explicitPosition.position.value;
                if (!rowStart.namedGridLine().isEmpty() || rowStartLineNumber < 0 || rowStartLineNumber > static_cast<int>(linesFromGridTemplateRowsCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedRowPlacement;

                auto rowEnd = gridItemStyle->gridItemRowEnd();
                if (!hasValidRowEnd(explicitPosition, rowEnd, linesFromGridTemplateRowsCount))
                    return GridAvoidanceReason::GridItemHasUnsupportedRowPlacement;

                ASSERT(rowEnd.isExplicit());
                explicitlyPlacedItemsInRowCount.resize(rowStartLineNumber + 1);
                ++explicitlyPlacedItemsInRowCount[rowStartLineNumber];
                return { };
            },
            [&](const Style::GridPositionSpan&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemHasUnsupportedRowPlacement;
            },
            [&](const CustomIdentifier&) -> std::optional<GridAvoidanceReason> {
                return GridAvoidanceReason::GridItemHasUnsupportedRowPlacement;
            }
        );

        if (rowPositioningAvoidanceReason) {
            ASSERT(rowPositioningAvoidanceReason == GridAvoidanceReason::GridItemHasUnsupportedRowPlacement);
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedRowPlacement, reasons, reasonCollectionMode);
        }

        // If there are too many items in a given row compared to the total number of columns in the
        // explicit grid, then we may need to add additional columns to the implicit grid to place
        // them properly. We can be more fine grained than what we are doing now, but this is
        // a good start as we allow more complex placements.
        auto ineligibleRowIndex = explicitlyPlacedItemsInRowCount.findIf([&](size_t itemsInRowCount) {
            return itemsInRowCount >= linesFromGridTemplateColumnsCount;
        });
        if (ineligibleRowIndex != notFound)
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedRowPlacement, reasons, reasonCollectionMode);

        if (gridItemStyle->writingMode().isVertical())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasVerticalWritingMode, reasons, reasonCollectionMode);

        if (gridItem->isOutOfFlowPositioned())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasOutOfFlowChild, reasons, reasonCollectionMode);

        if (gridItemStyle->hasAspectRatio())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasAspectRatio, reasons, reasonCollectionMode);

        if (!gridItemStyle->isOverflowVisible())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonVisibleOverflow, reasons, reasonCollectionMode);

        if (gridItemStyle->usedContain().contains(Style::ContainValue::Size))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasContainsSize, reasons, reasonCollectionMode);
    }
    return reasons;
}

#ifndef NDEBUG
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
    case GridAvoidanceReason::GridHasNonFixedWidth:
        stream << "grid has non-fixed width";
        break;
    case GridAvoidanceReason::GridHasNonFixedHeight:
        stream << "grid has non-fixed height";
        break;
    case GridAvoidanceReason::GridHasVerticalWritingMode:
        stream << "grid has vertical writing mode";
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
    case GridAvoidanceReason::GridHasUnsupportedRenderer:
        stream << "grid has unsupported renderer";
        break;
    case GridAvoidanceReason::GridIsEmpty:
        stream << "grid is empty";
        break;
    case GridAvoidanceReason::GridHasNonInitialMinWidth:
        stream << "grid has non-initial min-width";
        break;
    case GridAvoidanceReason::GridHasNonInitialMaxWidth:
        stream << "grid has non-initial max-width";
        break;
    case GridAvoidanceReason::GridHasNonInitialMinHeight:
        stream << "grid has non-initial min-height";
        break;
    case GridAvoidanceReason::GridHasNonInitialMaxHeight:
        stream << "grid has non-initial max-height";
        break;
    case GridAvoidanceReason::GridHasNonZeroMinWidth:
        stream << "grid has non-zero min-width";
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
    case GridAvoidanceReason::GridItemHasNonFixedWidth:
        stream << "grid item has non-fixed width";
        break;
    case GridAvoidanceReason::GridItemHasNonFixedHeight:
        stream << "grid item has non-fixed height";
        break;
    case GridAvoidanceReason::GridItemHasNonInitialMaxWidth:
        stream << "grid item has non-initial max-width";
        break;
    case GridAvoidanceReason::GridItemHasNonZeroMinHeight:
        stream << "grid item has non-zero min-height";
        break;
    case GridAvoidanceReason::GridItemHasNonInitialMaxHeight:
        stream << "grid item has non-initial max-height";
        break;
    case GridAvoidanceReason::GridItemHasBorder:
        stream << "grid item has border";
        break;
    case GridAvoidanceReason::GridItemHasPadding:
        stream << "grid item has padding";
        break;
    case GridAvoidanceReason::GridItemHasMargin:
        stream << "grid item has margin";
        break;
    case GridAvoidanceReason::GridItemHasVerticalWritingMode:
        stream << "grid item has vertical writing mode";
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
    case GridAvoidanceReason::GridItemHasUnsupportedColumnPlacement:
        stream << "grid item has unsupported column placement";
        break;
    case GridAvoidanceReason::GridItemHasUnsupportedRowPlacement:
        stream << "grid item has unsupported row placement";
        break;
    default:
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
        printTextForSubtree(*grid, printedLength, stream);
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

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

enum class GridAvoidanceReason : uint64_t {
    GridHasNonFixedWidth = 1LLU << 0,
    GridHasNonFixedHeight = 1LLU << 1,
    GridHasVerticalWritingMode = 1LLU << 2,
    GridHasMarginTrim = 1LLU << 3,
    GridIsNotInBFC = 1LLU << 4,
    GridNeedsBaseline = 1LLU << 5,
    GridHasOutOfFlowChild = 1LLU << 6,
    GridHasNonVisibleOverflow = 1LLU << 7,
    GridHasUnsupportedRenderer = 1LLU << 8,
    GridIsEmpty = 1LLU << 9,
    GridHasNonInitialMinWidth = 1LLU << 10,
    GridHasNonInitialMaxWidth = 1LLU << 11,
    GridHasNonInitialMinHeight = 1LLU << 12,
    GridHasNonInitialMaxHeight = 1LLU << 13,
    GridHasNonZeroMinWidth = 1LLU << 14,
    GridHasGridTemplateAreas = 1LLU << 15,
    GridHasNonInitialGridAutoFlow = 1LLU << 16,
    GridHasGaps = 1LLU << 17,
    GridIsOutOfFlow = 1LLU << 18,
    GridHasContainsSize = 1LLU << 19,
    GridHasUnsupportedGridTemplateColumns = 1LLU << 20,
    GridHasUnsupportedGridTemplateRows = 1LLU << 21,
    GridItemHasNonFixedWidth = 1LLU << 22,
    GridItemHasNonFixedHeight = 1LLU << 23,
    GridItemHasNonInitialMaxWidth = 1LLU << 24,
    GridItemHasNonZeroMinHeight = 1LLU << 25,
    GridItemHasNonInitialMaxHeight = 1LLU << 26,
    GridItemHasBorder = 1LLU << 27,
    GridItemHasPadding = 1LLU << 28,
    GridItemHasMargin = 1LLU << 29,
    GridItemHasVerticalWritingMode = 1LLU << 30,
    GridItemHasAspectRatio = 1LLU << 31,
    GridItemHasUnsupportedInlineAxisAlignment = 1LLU << 32,
    GridItemHasUnsupportedBlockAxisAlignment = 1LLU << 33,
    GridItemHasNonVisibleOverflow = 1LLU << 34,
    GridItemHasContainsSize = 1LLU << 35,
    GridItemHasUnsupportedColumnPlacement = 1LLU << 36,
    GridItemHasUnsupportedRowPlacement = 1LLU << 37,
    NotAGrid = 1LLU << 38,
    GridFormattingContextIntegrationDisabled = 1LLU << 39,
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

static OptionSet<GridAvoidanceReason> gridLayoutAvoidanceReason(const RenderGrid& renderGrid, ReasonCollectionMode reasonCollectionMode)
{
    auto reasons = OptionSet<GridAvoidanceReason> { };

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

    if (!renderGridStyle->marginTrim().isEmpty())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasMarginTrim, reasons, reasonCollectionMode);

    if (!renderGridStyle->isOverflowVisible())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonVisibleOverflow, reasons, reasonCollectionMode);

    if (!renderGrid.firstInFlowChild())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridIsEmpty, reasons, reasonCollectionMode);

    if (renderGridStyle->gridAutoFlow() != RenderStyle::initialGridAutoFlow())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasNonInitialGridAutoFlow, reasons, reasonCollectionMode);

    if (!renderGridStyle->rowGap().isNormal() || !renderGridStyle->columnGap().isNormal())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasGaps, reasons, reasonCollectionMode);

    if (renderGrid.isOutOfFlowPositioned())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridIsOutOfFlow, reasons, reasonCollectionMode);

    if (!renderGridStyle->gridTemplateAreas().isNone())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasGridTemplateAreas, reasons, reasonCollectionMode);

    auto isInBFC = [&] {
        for (CheckedPtr containingBlock = renderGrid.containingBlock(); containingBlock && !is<RenderView>(*containingBlock); containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->style().display() != DisplayType::Block)
                return false;
            if (containingBlock->createsNewFormattingContext())
                return true;
        }
        return true;
    };

    if (!isInBFC())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridIsNotInBFC, reasons, reasonCollectionMode);

    auto& gridTemplateColumns = renderGridStyle->gridTemplateColumns();
    auto& gridTemplateColumnsTrackList = gridTemplateColumns.list;
    if (gridTemplateColumnsTrackList.isEmpty())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasUnsupportedGridTemplateColumns, reasons, reasonCollectionMode);

    for (auto& columnsTrackListEntry : gridTemplateColumnsTrackList) {

        auto avoidanceReason = WTF::switchOn(columnsTrackListEntry,
            [&](const Style::GridTrackSize& trackSize) -> std::optional<GridAvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth() || !trackSize.minTrackBreadth().isLength())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;

                auto& gridTrackBreadthLength = trackSize.minTrackBreadth().length();
                if (!gridTrackBreadthLength.isFixed())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateColumns;
                return std::nullopt;
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
                if (!trackSize.isBreadth() || !trackSize.minTrackBreadth().isLength())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;

                auto& gridTrackBreadthLength = trackSize.minTrackBreadth().length();
                if (!gridTrackBreadthLength.isFixed())
                    return GridAvoidanceReason::GridHasUnsupportedGridTemplateRows;
                return std::nullopt;
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

    if (renderGridStyle->containsSize())
        ADD_REASON_AND_RETURN_IF_NEEDED(GridHasContainsSize, reasons, reasonCollectionMode);

    auto linesFromGridTemplateColumnsCount = gridTemplateColumns.sizes.size() + 1;
    auto linesFromGridTemplateRowsCount = gridTemplateRows.sizes.size() + 1;
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

        auto& columnStart = gridItemStyle->gridItemColumnStart();
        if (!columnStart.isExplicit() || !columnStart.namedGridLine().isEmpty() || columnStart.explicitPosition() < 0
            || columnStart.explicitPosition() > static_cast<int>(linesFromGridTemplateColumnsCount))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedColumnPlacement, reasons, reasonCollectionMode);

        auto& columnEnd = gridItemStyle->gridItemColumnEnd();
        if (!columnEnd.isExplicit() || !columnEnd.namedGridLine().isEmpty() || columnEnd.explicitPosition() < 0
            || columnEnd.explicitPosition() > static_cast<int>(linesFromGridTemplateColumnsCount))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedColumnPlacement, reasons, reasonCollectionMode);

        auto& rowStart = gridItemStyle->gridItemRowStart();
        if (!rowStart.isExplicit() || !rowStart.namedGridLine().isEmpty() || rowStart.explicitPosition() < 0
            || rowStart.explicitPosition() > static_cast<int>(linesFromGridTemplateRowsCount))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedRowPlacement, reasons, reasonCollectionMode);

        auto& rowEnd = gridItemStyle->gridItemRowEnd();
        if (!rowEnd.isExplicit() || !rowEnd.namedGridLine().isEmpty() || rowEnd.explicitPosition() < 0
            || rowEnd.explicitPosition() > static_cast<int>(linesFromGridTemplateRowsCount))
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasUnsupportedRowPlacement, reasons, reasonCollectionMode);

        if (gridItemStyle->writingMode().isVertical())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasVerticalWritingMode, reasons, reasonCollectionMode);

        if (gridItem->isOutOfFlowPositioned())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridHasOutOfFlowChild, reasons, reasonCollectionMode);

        if (gridItemStyle->hasAspectRatio())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasAspectRatio, reasons, reasonCollectionMode);

        if (!gridItemStyle->isOverflowVisible())
            ADD_REASON_AND_RETURN_IF_NEEDED(GridItemHasNonVisibleOverflow, reasons, reasonCollectionMode);

        if (gridItemStyle->containsSize())
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
    case GridAvoidanceReason::GridIsNotInBFC:
        stream << "grid is not in BFC";
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
    case GridAvoidanceReason::GridHasNonInitialGridAutoFlow:
        stream << "grid has non-initial grid-auto-flow";
        break;
    case GridAvoidanceReason::GridHasGaps:
        stream << "grid has gaps";
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

static void printReasons(OptionSet<GridAvoidanceReason> reasons, TextStream& stream)
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

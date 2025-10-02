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

#include "RenderChildIterator.h"
#include "RenderGrid.h"
#include "StyleLengthWrapperData.h"

namespace WebCore {
namespace LayoutIntegration {

enum class AvoidanceReason : uint64_t {
    GridHasNonFixedWidth = 1LLU << 0,
    GridHasNonFixedHeight = 1LLU << 1,
    GridHasVerticalWritingMode = 1LLU << 2,
    GridHasMarginTrim = 1LLU << 3,
    GridIsNotInBFC = 1LLU << 6,
    GridNeedsBaseline = 1LLU << 7,
    GridHasOutOfFlowChild = 1LLU << 8,
    GridHasNonVisibleOverflow = 1LLU << 9,
    GridHasUnsupportedRenderer = 1LLU << 10,
    GridIsEmpty = 1LLU << 11,
    GridHasNonInitialMinWidth = 1LLU << 12,
    GridHasNonInitialMaxWidth = 1LLU << 13,
    GridHasNonInitialMinHeight = 1LLU << 14,
    GridHasNonInitialMaxHeight = 1LLU << 15,
    GridItemHasNonFixedWidth = 1LLU << 16,
    GridItemHasNonFixedHeight = 1LLU << 17,
    GridHasNonZeroMinWidth = 1LLU << 18,
    GridItemHasNonInitialMaxWidth = 1LLU << 19,
    GridItemHasNonZeroMinHeight = 1LLU << 20,
    GridItemHasNonInitialMaxHeight = 1LLU << 21,
    GridItemHasBorder = 1LLU << 22,
    GridItemHasPadding = 1LLU << 23,
    GridItemHasMargin = 1LLU << 24,
    GridItemHasVerticalWritingMode = 1LLU << 25,
    GridItemHasAspectRatio = 1LLU << 26,
    GridItemHasUnsupportedInlineAxisAlignment = 1LLU << 27,
    GridItemHasUnsupportedBlockAxisAlignment = 1LLU << 28,
    GridItemHasNonVisibleOverflow = 1LLU << 29,
    GridItemHasContainsSize = 1LLU << 30,
    GridItemHasUnsupportedColumnPlacement = 1LLU << 31,
    GridItemHasUnsupportedRowPlacement = 1LLU << 32,
    GridItemSpansMultipleColumns = 1LLU << 33,
    GridItemSpansMultipleRows = 1LLU << 34,
    GridItemIsPlacedOutsideExplicitGrid = 1LLU << 35,
    GridHasNonInitialGridTemplateAreas = 1LLU << 36,
    GridHasNamedLines = 1LLU << 37,
    GridHasNonInitialGridAutoFlow = 1LLU << 38,
    GridHasGaps = 1LLU << 39,
    GridHasNonInitialInlineAxisContentAlignment = 1LLU << 40,
    GridHasNonInitialBlockAxisContentAlignment = 1LLU << 41,
    GridIsOutOfFlow = 1LLU << 44,
    GridHasContainsSize = 1LLU << 45,
    GridHasUnsupportedGridTemplateColumns = 1LLU << 46,
    GridHasUnsupportedGridTemplateRows = 1LLU << 47
};

static std::optional<AvoidanceReason> gridLayoutAvoidanceReason(const RenderGrid& renderGrid)
{
    auto& renderGridStyle = renderGrid.style();
    
    if (!renderGridStyle.writingMode().isHorizontal())
        return AvoidanceReason::GridHasVerticalWritingMode;

    if (!renderGridStyle.width().isFixed())
        return AvoidanceReason::GridHasNonFixedWidth;

    if (!renderGridStyle.height().isFixed())
        return AvoidanceReason::GridHasNonFixedHeight;

    if (!renderGridStyle.marginTrim().isEmpty())
        return AvoidanceReason::GridHasMarginTrim;

    if (!renderGridStyle.isOverflowVisible())
        return AvoidanceReason::GridHasNonVisibleOverflow;

    if (!renderGrid.firstInFlowChild())
        return AvoidanceReason::GridIsEmpty;

    if (renderGridStyle.display() == DisplayType::InlineGrid)
        return AvoidanceReason::GridNeedsBaseline;

    if (renderGridStyle.gridAutoFlow() != GridAutoFlow::AutoFlowRow)
        return AvoidanceReason::GridHasNonInitialGridAutoFlow;

    if (!renderGridStyle.rowGap().isNormal() || !renderGridStyle.columnGap().isNormal())
        return AvoidanceReason::GridHasGaps;

    if (renderGrid.isOutOfFlowPositioned())
        return AvoidanceReason::GridIsOutOfFlow;

    if (!renderGridStyle.gridTemplateAreas().isNone())
        return AvoidanceReason::GridHasNonInitialGridTemplateAreas;

    auto& gridTemplateColumns = renderGridStyle.gridTemplateColumns();
    auto& gridTemplateColumnsTrackList = gridTemplateColumns.list;
    if (gridTemplateColumnsTrackList.isEmpty())
        return AvoidanceReason::GridHasUnsupportedGridTemplateColumns;

    for (auto& columnsTrackListEntry : gridTemplateColumnsTrackList) {

        auto avoidanceReason = WTF::switchOn(columnsTrackListEntry, 
            [&](const Style::GridTrackSize& trackSize) -> std::optional<AvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth() || !trackSize.minTrackBreadth().isLength())
                      return AvoidanceReason::GridHasUnsupportedGridTemplateColumns;

                auto& gridTrackBreadthLength = trackSize.minTrackBreadth().length();
                if (!gridTrackBreadthLength.isFixed())
                      return AvoidanceReason::GridHasUnsupportedGridTemplateColumns;
                return std::nullopt;
            },
            [&](const Vector<String>& names) -> std::optional<AvoidanceReason> {
                for (auto& name : names) {
                    if (!name.isEmpty())
                        return AvoidanceReason::GridHasUnsupportedGridTemplateColumns; 
                }
                return std::nullopt; 
            },
            [&](const Style::GridTrackEntryRepeat&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntryAutoRepeat&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntrySubgrid&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntryMasonry&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            }
        );

        if (avoidanceReason)
            return avoidanceReason;
    }

    auto& gridTemplateRows = renderGridStyle.gridTemplateRows();
    auto& gridTemplateRowsTrackList = gridTemplateRows.list;
    if (gridTemplateRowsTrackList.isEmpty())
        return AvoidanceReason::GridHasUnsupportedGridTemplateRows;

    for (auto& rowsTrackListEntry : gridTemplateRowsTrackList) {
        auto avoidanceReason = WTF::switchOn(rowsTrackListEntry, 
            [&](const Style::GridTrackSize& trackSize) -> std::optional<AvoidanceReason> {
                // Since a GridTrackSize type of Breadth sets the MinTrackBreadth and
                // MaxTrackBreadth to the same value we only need to check one.
                if (!trackSize.isBreadth() || !trackSize.minTrackBreadth().isLength())
                      return AvoidanceReason::GridHasUnsupportedGridTemplateRows;

                auto& gridTrackBreadthLength = trackSize.minTrackBreadth().length();
                if (!gridTrackBreadthLength.isFixed())
                      return AvoidanceReason::GridHasUnsupportedGridTemplateRows;
                return std::nullopt;
            },
            [&](const Vector<String>& names) -> std::optional<AvoidanceReason> {
                for (auto& name : names) {
                    if (!name.isEmpty())
                        return AvoidanceReason::GridHasUnsupportedGridTemplateColumns; 
                }
                return std::nullopt; 
            },
            [&](const Style::GridTrackEntryRepeat&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntryAutoRepeat&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntrySubgrid&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            },
            [&](const Style::GridTrackEntryMasonry&) {
               return std::make_optional(AvoidanceReason::GridHasUnsupportedGridTemplateColumns); 
            }
        );

        if (avoidanceReason)
            return avoidanceReason;
    }
    
    auto isInBFC = [&] {
        // FIXME: This may be somewhat expensive in some deeply nested cases.
        for (auto* containingBlock = renderGrid.containingBlock(); containingBlock && !is<RenderView>(*containingBlock); containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->style().display() != DisplayType::Block)
                return false;
            if (containingBlock->createsNewFormattingContext())
                return true;
        }
        return true;
    };

    if (!isInBFC())
        return AvoidanceReason::GridIsNotInBFC;

    if (renderGridStyle.containsSize())
        return AvoidanceReason::GridHasContainsSize;


    auto explicitColumnsCount = gridTemplateColumns.sizes.size() + 1;
    auto explicitRowsCount = gridTemplateRows.sizes.size() + 1;
    for (auto& gridItem : childrenOfType<RenderBox>(renderGrid)) {
        if (!gridItem.isRenderBlockFlow())
            return AvoidanceReason::GridHasUnsupportedRenderer;

        if (gridItem.isOutOfFlowPositioned()) 
            return AvoidanceReason::GridHasOutOfFlowChild;

        auto& gridItemStyle = gridItem.style();

        auto& columnStart = gridItemStyle.gridItemColumnStart();
        if (!columnStart.isExplicit() || !columnStart.namedGridLine().isEmpty() || columnStart.explicitPosition() < 0 || columnStart.explicitPosition() > static_cast<int>(explicitColumnsCount))
            return AvoidanceReason::GridItemHasUnsupportedColumnPlacement;

        auto& columnEnd = gridItemStyle.gridItemColumnEnd();
        if (!columnEnd.isExplicit() || !columnEnd.namedGridLine().isEmpty() || columnEnd.explicitPosition() < 0 || columnEnd.explicitPosition() > static_cast<int>(explicitColumnsCount))
            return AvoidanceReason::GridItemHasUnsupportedColumnPlacement;

        auto& rowStart = gridItemStyle.gridItemRowStart();
        if (!rowStart.isExplicit() || !rowStart.namedGridLine().isEmpty() || rowStart.explicitPosition() < 0 || rowStart.explicitPosition() > static_cast<int>(explicitRowsCount))
            return AvoidanceReason::GridItemHasUnsupportedRowPlacement;

        auto& rowEnd = gridItemStyle.gridItemRowEnd();
        if (!rowEnd.isExplicit() || !rowEnd.namedGridLine().isEmpty() || rowEnd.explicitPosition() < 0 || rowEnd.explicitPosition() > static_cast<int>(explicitRowsCount))
            return AvoidanceReason::GridItemHasUnsupportedRowPlacement;

        if (!gridItemStyle.width().isFixed())
            return AvoidanceReason::GridItemHasNonFixedWidth;

        if (gridItemStyle.writingMode().isVertical())
            return AvoidanceReason::GridItemHasVerticalWritingMode;

        if (!gridItemStyle.height().isFixed())
            return AvoidanceReason::GridItemHasNonFixedHeight;

        if (gridItemStyle.hasAspectRatio())
            return AvoidanceReason::GridItemHasAspectRatio;

        if (auto fixedMinWidth = gridItemStyle.minWidth().tryFixed(); fixedMinWidth && fixedMinWidth->unresolvedValue())
            return AvoidanceReason::GridHasNonZeroMinWidth;

        if (!gridItemStyle.maxWidth().isNone())
            return AvoidanceReason::GridItemHasNonInitialMaxWidth;

        if (auto fixedMinHeight = gridItemStyle.minHeight().tryFixed(); fixedMinHeight && fixedMinHeight->unresolvedValue())
            return AvoidanceReason::GridItemHasNonZeroMinHeight;

        if (!gridItemStyle.maxHeight().isNone())
            return AvoidanceReason::GridItemHasNonInitialMaxHeight;

        if (!gridItemStyle.isOverflowVisible())
            return AvoidanceReason::GridItemHasNonVisibleOverflow;

        auto& justifySelf = gridItemStyle.justifySelf();
        if (justifySelf.position() != ItemPosition::Start && justifySelf.overflow() != OverflowAlignment::Default
            && justifySelf.positionType() != ItemPositionType::NonLegacy)
            return AvoidanceReason::GridItemHasUnsupportedInlineAxisAlignment;

        auto& alignSelf = gridItemStyle.alignSelf();
        if (alignSelf.position() != ItemPosition::Start && alignSelf.overflow() != OverflowAlignment::Default
            && alignSelf.positionType() != ItemPositionType::NonLegacy)
            return AvoidanceReason::GridItemHasUnsupportedBlockAxisAlignment;

        if (gridItemStyle.containsSize())
            return AvoidanceReason::GridItemHasContainsSize;

        if (gridItemStyle.border().hasBorder())
            return AvoidanceReason::GridItemHasBorder;

        auto gridItemHasPadding = [&] {
            return gridItemStyle.paddingBox().anyOf([](const Style::PaddingEdge& paddingEdge) {
                return paddingEdge.isPossiblyZero();
            });
        };
        if (gridItemHasPadding())
            return AvoidanceReason::GridItemHasPadding;

        auto gridItemHasMargins = [&] {
            return gridItemStyle.marginBox().anyOf([](const Style::MarginEdge& marginEdge) {
                return marginEdge.isPossiblyZero();
            });
        };
        if (gridItemHasMargins())
            return AvoidanceReason::GridItemHasMargin;
    }
    return { };
}

bool canUseForGridLayout(const RenderGrid& renderGrid)
{
    // Grid integration is not enabled yet.
    //if (!renderGrid.document().settings().gridFormattingContextIntegrationEnabled())
    //    return false;

    return !gridLayoutAvoidanceReason(renderGrid);
}

} // namespace LayoutIntegration
} // namespace WebCore

/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "TableWrapperBlockFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingState.h"
#include "DisplayBox.h"
#include "InvalidationState.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "TableFormattingContext.h"
#include "TableFormattingState.h"

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableWrapperBlockFormattingContext);

TableWrapperBlockFormattingContext::TableWrapperBlockFormattingContext(const ContainerBox& formattingContextRoot, BlockFormattingState& formattingState)
    : BlockFormattingContext(formattingContextRoot, formattingState)
{
}

void TableWrapperBlockFormattingContext::layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent& constraints)
{
    // The table generates a principal block container box called the table wrapper box that contains the table box itself and any caption boxes
    // (in document order). The table box is a block-level box that contains the table's internal table boxes.
    // The caption boxes are principal block-level boxes that retain their own content, padding, margin, and border areas, and are rendered
    // as normal block boxes inside the table wrapper box. Whether the caption boxes are placed before or after the table box is decided by
    // the 'caption-side' property, as described below.
    for (auto& child : childrenOfType<ContainerBox>(root())) {
        if (child.isTableBox())
            layoutTableBox(child, constraints);
        else if (child.isTableCaption())
            ASSERT_NOT_IMPLEMENTED_YET();
        else
            ASSERT_NOT_REACHED();
    }
}

void TableWrapperBlockFormattingContext::layoutTableBox(const ContainerBox& tableBox, const ConstraintsForInFlowContent& constraints)
{
    layoutState().ensureTableFormattingState(tableBox);

    computeBorderAndPaddingForTableBox(tableBox, constraints.horizontal);
    computeStaticVerticalPosition(tableBox, constraints.vertical);
    computeWidthAndMarginForTableBox(tableBox, constraints.horizontal);
    computeStaticHorizontalPosition(tableBox, constraints.horizontal);

    if (tableBox.hasChild()) {
        auto invalidationState = InvalidationState { };
        LayoutContext::createFormattingContext(tableBox, layoutState())->layoutInFlowContent(invalidationState, geometry().constraintsForInFlowContent(tableBox));
    }

    computeHeightAndMarginForTableBox(tableBox, constraints);
}

void TableWrapperBlockFormattingContext::computeBorderAndPaddingForTableBox(const ContainerBox& tableBox, const HorizontalConstraints& horizontalConstraints)
{
    ASSERT(tableBox.isTableBox());
    if (!tableBox.hasChild() || tableBox.style().borderCollapse() == BorderCollapse::Separate) {
        BlockFormattingContext::computeBorderAndPadding(tableBox, horizontalConstraints);
        return;
    }
    // UAs must compute an initial left and right border width for the table by examining
    // the first and last cells in the first row of the table.
    // The left border width of the table is half of the first cell's collapsed left border,
    // and the right border width of the table is half of the last cell's collapsed right border.
    // The top border width of the table is computed by examining all cells who collapse their top
    // borders with the top border of the table. The top border width of the table is equal to half of the
    // maximum collapsed top border. The bottom border width is computed by examining all cells whose bottom borders collapse
    // with the bottom of the table. The bottom border width is equal to half of the maximum collapsed bottom border.
    auto& grid = layoutState().establishedTableFormattingState(tableBox).tableGrid();
    auto tableBorder = geometry().computedBorder(tableBox);

    auto& firstColumnFirstRowBox = grid.slot({ 0 , 0 })->cell().box();
    auto leftBorder = std::max(tableBorder.horizontal.left, geometry().computedBorder(firstColumnFirstRowBox).horizontal.left);

    auto& lastColumnFirstRow = grid.slot({ grid.columns().size() - 1, 0 })->cell().box();
    auto rightBorder = std::max(tableBorder.horizontal.right, geometry().computedBorder(lastColumnFirstRow).horizontal.right);

    auto topBorder = tableBorder.vertical.top;
    auto bottomBorder = tableBorder.vertical.bottom;
    auto lastRowIndex = grid.rows().size() - 1;
    for (size_t columnIndex = 0; columnIndex < grid.columns().size(); ++columnIndex) {
        auto& boxInFirstRox = grid.slot({ columnIndex, 0 })->cell().box();
        auto& boxInLastRow = grid.slot({ columnIndex, lastRowIndex })->cell().box();

        topBorder = std::max(topBorder, geometry().computedBorder(boxInFirstRox).vertical.top);
        bottomBorder = std::max(bottomBorder, geometry().computedBorder(boxInLastRow).vertical.bottom);
    }

    topBorder = std::max(topBorder, geometry().computedBorder(*tableBox.firstChild()).vertical.top);
    for (auto& section : childrenOfType<ContainerBox>(tableBox)) {
        auto horiztonalBorder = geometry().computedBorder(section).horizontal;
        leftBorder = std::max(leftBorder, horiztonalBorder.left);
        rightBorder = std::max(rightBorder, horiztonalBorder.right);
    }
    bottomBorder = std::max(bottomBorder, geometry().computedBorder(*tableBox.lastChild()).vertical.bottom);

    auto& rows = grid.rows().list();
    topBorder = std::max(topBorder, geometry().computedBorder(rows.first().box()).vertical.top);
    for (auto& row : rows) {
        auto horiztonalBorder = geometry().computedBorder(row.box()).horizontal;
        leftBorder = std::max(leftBorder, horiztonalBorder.left);
        rightBorder = std::max(rightBorder, horiztonalBorder.right);
    }
    bottomBorder = std::max(bottomBorder, geometry().computedBorder(rows.last().box()).vertical.bottom);

    auto collapsedBorder = Edges { { leftBorder, rightBorder }, { topBorder, bottomBorder } };
    grid.setCollapsedBorder(collapsedBorder);

    auto& displayBox = formattingState().displayBox(tableBox);
    displayBox.setBorder(collapsedBorder / 2);
    displayBox.setPadding(geometry().computedPadding(tableBox, horizontalConstraints.logicalWidth));
}

void TableWrapperBlockFormattingContext::computeWidthAndMarginForTableBox(const ContainerBox& tableBox, const HorizontalConstraints& horizontalConstraints)
{
    ASSERT(tableBox.isTableBox());
    // This is a special table "fit-content size" behavior handling. Not in the spec though.
    // Table returns its final width as min/max. Use this final width value to computed horizontal margins etc.
    auto& formattingStateForTableBox = layoutState().establishedTableFormattingState(tableBox);
    auto intrinsicWidthConstraints = IntrinsicWidthConstraints { };
    if (auto precomputedIntrinsicWidthConstraints = formattingStateForTableBox.intrinsicWidthConstraints())
        intrinsicWidthConstraints = *precomputedIntrinsicWidthConstraints;
    else {
        if (tableBox.hasChild())
            intrinsicWidthConstraints = LayoutContext::createFormattingContext(tableBox, layoutState())->computedIntrinsicWidthConstraints();
        formattingStateForTableBox.setIntrinsicWidthConstraints(intrinsicWidthConstraints);
    }

    auto availableHorizontalSpace = horizontalConstraints.logicalWidth;
    auto geometry = this->geometry();
    auto computedWidth = geometry.computedWidth(tableBox, availableHorizontalSpace);
    auto computedMaxWidth = geometry.computedMaxWidth(tableBox, availableHorizontalSpace);
    auto computedMinWidth = geometry.computedMinWidth(tableBox, availableHorizontalSpace);
    // Use the generic shrink-to-fit-width logic as the initial width for the table.
    auto usedWidth = std::min(std::max(intrinsicWidthConstraints.minimum, availableHorizontalSpace), intrinsicWidthConstraints.maximum);
    if (computedWidth || computedMinWidth || computedMaxWidth) {
        if (computedWidth) {
            // Normalize the computed width value first.
            if (computedMaxWidth && *computedWidth > *computedMaxWidth)
                computedWidth = computedMaxWidth;
            if (computedMinWidth && *computedWidth < *computedMinWidth)
                computedWidth = computedMinWidth;
            usedWidth = *computedWidth < intrinsicWidthConstraints.minimum ? intrinsicWidthConstraints.minimum : *computedWidth;
        }

        if (computedMaxWidth && *computedMaxWidth < usedWidth)
            usedWidth = intrinsicWidthConstraints.minimum;
        if (computedMinWidth && *computedMinWidth > usedWidth)
            usedWidth = *computedMinWidth;
    }

    auto contentWidthAndMargin = geometry.inFlowWidthAndMargin(tableBox, horizontalConstraints, OverrideHorizontalValues { usedWidth, { } });

    auto& displayBox = formattingState().displayBox(tableBox);
    displayBox.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin(contentWidthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(contentWidthAndMargin.computedMargin);
}

void TableWrapperBlockFormattingContext::computeHeightAndMarginForTableBox(const ContainerBox& tableBox, const ConstraintsForInFlowContent& constraints)
{
    ASSERT(tableBox.isTableBox());
    // Table is a special BFC content. Its height is mainly driven by the content. Computed height, min-height and max-height are all
    // already been taken into account during the TFC layout.
    auto heightAndMargin = geometry().inFlowHeightAndMargin(tableBox, constraints.horizontal, { quirks().overrideTableHeight(tableBox) });

    auto marginCollapse = this->marginCollapse();
    auto collapsedAndPositiveNegativeValues = marginCollapse.collapsedVerticalValues(tableBox, heightAndMargin.nonCollapsedMargin);
    // Cache the computed positive and negative margin value pair.
    formattingState().setPositiveAndNegativeVerticalMargin(tableBox, collapsedAndPositiveNegativeValues.positiveAndNegativeVerticalValues);
    auto verticalMargin = UsedVerticalMargin { heightAndMargin.nonCollapsedMargin, collapsedAndPositiveNegativeValues.collapsedValues };

    auto& displayBox = formattingState().displayBox(tableBox);
    displayBox.setTop(verticalPositionWithMargin(tableBox, verticalMargin, constraints.vertical));
    displayBox.setContentBoxHeight(heightAndMargin.contentHeight);
    displayBox.setVerticalMargin(verticalMargin);
    // Adjust the previous sibling's margin bottom now that this box's vertical margin is computed.
    MarginCollapse::updateMarginAfterForPreviousSibling(*this, marginCollapse, tableBox);
}

}
}

#endif

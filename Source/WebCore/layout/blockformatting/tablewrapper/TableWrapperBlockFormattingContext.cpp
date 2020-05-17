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
    computeBorderAndPadding(tableBox, constraints.horizontal);
    computeStaticVerticalPosition(tableBox, constraints.vertical);
    computeWidthAndMarginForTableBox(tableBox, constraints);
    computeStaticHorizontalPosition(tableBox, constraints.horizontal);

    auto invalidationState = InvalidationState { };
    LayoutContext::createFormattingContext(tableBox, layoutState())->layoutInFlowContent(invalidationState, geometry().constraintsForInFlowContent(tableBox));

    computeHeightAndMarginForTableBox(tableBox, constraints);
}

void TableWrapperBlockFormattingContext::computeWidthAndMarginForTableBox(const ContainerBox& tableBox, const ConstraintsForInFlowContent& constraints)
{
    ASSERT(tableBox.isTableBox());
    // This is a special table "fit-content size" behavior handling. Not in the spec though.
    // Table returns its final width as min/max. Use this final width value to computed horizontal margins etc.
    auto& formattingStateForTableBox = layoutState().ensureFormattingState(tableBox);
    auto intrinsicWidthConstraints = IntrinsicWidthConstraints { };
    if (auto precomputedIntrinsicWidthConstraints = formattingStateForTableBox.intrinsicWidthConstraints())
        intrinsicWidthConstraints = *precomputedIntrinsicWidthConstraints;
    else
        intrinsicWidthConstraints = LayoutContext::createFormattingContext(tableBox, layoutState())->computedIntrinsicWidthConstraints();
    auto computedTableWidth = geometry().computedWidth(tableBox, constraints.horizontal.logicalWidth);
    auto usedWidth = computedTableWidth;
    if (computedTableWidth && intrinsicWidthConstraints.minimum > computedTableWidth) {
        // Table content needs more space than the table has.
        usedWidth = intrinsicWidthConstraints.minimum;
    } else if (!computedTableWidth) {
        // Use the generic shrink-to-fit-width logic.
        usedWidth = std::min(std::max(intrinsicWidthConstraints.minimum, constraints.horizontal.logicalWidth), intrinsicWidthConstraints.maximum);
    }
    auto contentWidthAndMargin = geometry().inFlowWidthAndMargin(tableBox, constraints.horizontal, OverrideHorizontalValues { usedWidth, { } });

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
    auto usedHeight = geometry().contentHeightForFormattingContextRoot(tableBox);
    auto heightAndMargin = geometry().inFlowHeightAndMargin(tableBox, constraints.horizontal, { usedHeight });

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

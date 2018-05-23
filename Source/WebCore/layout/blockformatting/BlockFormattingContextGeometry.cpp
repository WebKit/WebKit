/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "BlockFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FormattingContext.h"

namespace WebCore {
namespace Layout {

LayoutUnit BlockFormattingContext::Geometry::inFlowNonReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());

    // https://www.w3.org/TR/CSS22/visudet.html
    // If 'height' is 'auto', the height depends on whether the element has any block-level children and whether it has padding or borders:
    // The element's height is the distance from its top content edge to the first applicable of the following:
    // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
    // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin
    //    does not collapse with the element's bottom margin
    // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
    // 4. zero, otherwise
    // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
    // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.
    if (!layoutBox.style().logicalHeight().isAuto()) {
        // FIXME: Only fixed values yet.
        return layoutBox.style().logicalHeight().value();
    }

    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return 0;

    // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
    if (layoutBox.establishesInlineFormattingContext()) {
        // height = lastLineBox().bottom();
        return 0;
    }

    // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin...
    auto* lastInFlowChild = downcast<Container>(layoutBox).lastInFlowChild();
    ASSERT(lastInFlowChild);
    if (!BlockMarginCollapse::isMarginBottomCollapsedWithParent(*lastInFlowChild)) {
        auto* lastInFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*lastInFlowChild);
        ASSERT(lastInFlowDisplayBox);
        return lastInFlowDisplayBox->bottom() + lastInFlowDisplayBox->marginBottom();
    }

    // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
    auto* inFlowChild = lastInFlowChild;
    while (inFlowChild && BlockMarginCollapse::isMarginTopCollapsedWithParentMarginBottom(*inFlowChild))
        inFlowChild = inFlowChild->previousInFlowSibling();
    if (inFlowChild) {
        auto* inFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*inFlowChild);
        ASSERT(inFlowDisplayBox);
        return inFlowDisplayBox->top() + inFlowDisplayBox->borderBox().height();
    }

    // 4. zero, otherwise
    return 0;
}

LayoutUnit BlockFormattingContext::Geometry::inFlowNonReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());

    // 10.3.3 Block-level, non-replaced elements in normal flow
    // The following constraints must hold among the used values of the other properties:
    // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' = width of containing block

    // If 'width' is set to 'auto', any other 'auto' values become '0' and 'width' follows from the resulting equality.
    auto& style = layoutBox.style();
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->width();

    LayoutUnit computedWidthValue;
    auto width = style.logicalWidth();
    if (width.isAuto()) {
        auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
        auto marginLeft = displayBox.marginLeft();
        auto marginRight = displayBox.marginRight();

        auto paddingLeft = displayBox.paddingLeft();
        auto paddingRight = displayBox.paddingRight();

        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();

        computedWidthValue = containingBlockWidth - (marginLeft + borderLeft + paddingLeft + paddingRight + borderRight + marginRight);
    } else
        computedWidthValue = valueForLength(width, containingBlockWidth);

    return computedWidthValue;
}

LayoutPoint BlockFormattingContext::Geometry::staticPosition(LayoutContext& layoutContext, const Box& layoutBox)
{
    // https://www.w3.org/TR/CSS22/visuren.html#block-formatting
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    // In a block formatting context, each box's left outer edge touches the left edge of the containing block (for right-to-left formatting, right edges touch).
    auto containingBlockContentBox = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->contentBox();
    // Start from the top of the container's content box.
    auto top = containingBlockContentBox.y();
    auto left = containingBlockContentBox.x();
    if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
        auto& previousInFlowDisplayBox = *layoutContext.displayBoxForLayoutBox(*previousInFlowSibling);
        top = previousInFlowDisplayBox.bottom() + previousInFlowDisplayBox.marginBottom();
    }
    auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
    LayoutPoint topLeft = { top, left };
    topLeft.moveBy({ displayBox.marginLeft(), displayBox.marginTop() });
    return topLeft;
}

}
}

#endif

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

static bool isStretchedToViewport(const LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());
    // In quirks mode, body and html stretch to the viewport.
    if (!layoutContext.inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() || !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

static const Container& initialContainingBlock(const Box& layoutBox)
{
    auto* containingBlock = layoutBox.containingBlock();
    while (containingBlock->containingBlock())
        containingBlock = containingBlock->containingBlock();
    return *containingBlock;
}

LayoutUnit BlockFormattingContext::Geometry::inFlowNonReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());

    auto compute = [&]() -> LayoutUnit {
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
        if (!BlockFormattingContext::MarginCollapse::isMarginBottomCollapsedWithParent(*lastInFlowChild)) {
            auto* lastInFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*lastInFlowChild);
            ASSERT(lastInFlowDisplayBox);
            return lastInFlowDisplayBox->bottom() + lastInFlowDisplayBox->marginBottom();
        }

        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        auto* inFlowChild = lastInFlowChild;
        while (inFlowChild && BlockFormattingContext::MarginCollapse::isMarginTopCollapsedWithParentMarginBottom(*inFlowChild))
            inFlowChild = inFlowChild->previousInFlowSibling();
        if (inFlowChild) {
            auto* inFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*inFlowChild);
            ASSERT(inFlowDisplayBox);
            return inFlowDisplayBox->top() + inFlowDisplayBox->borderBox().height();
        }

        // 4. zero, otherwise
        return 0;
    };

    auto computedHeight = compute();
    if (!isStretchedToViewport(layoutContext, layoutBox))
        return computedHeight;
    auto initialContainingBlockHeight = layoutContext.displayBoxForLayoutBox(initialContainingBlock(layoutBox))->contentBox().height();
    return std::max(computedHeight, initialContainingBlockHeight);
}

FormattingContext::Geometry::WidthAndMargin BlockFormattingContext::Geometry::inFlowNonReplacedWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox,
    std::optional<LayoutUnit> precomputedWidth)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());

    auto compute = [&]() {
        // 10.3.3 Block-level, non-replaced elements in normal flow
        // The following constraints must hold among the used values of the other properties:
        // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' = width of containing block
        //
        // 1. If 'width' is not 'auto' and 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' 
        //    (plus any of 'margin-left' or 'margin-right' that are not 'auto') is larger than the width of the containing block, then
        //    any 'auto' values for 'margin-left' or 'margin-right' are, for the following rules, treated as zero.
        //
        // 2. If all of the above have a computed value other than 'auto', the values are said to be "over-constrained" and one of the used values will
        //    have to be different from its computed value. If the 'direction' property of the containing block has the value 'ltr', the specified value
        //    of 'margin-right' is ignored and the value is calculated so as to make the equality true. If the value of 'direction' is 'rtl',
        //    this happens to 'margin-left' instead.
        //
        // 3. If there is exactly one value specified as 'auto', its used value follows from the equality.
        //
        // 4. If 'width' is set to 'auto', any other 'auto' values become '0' and 'width' follows from the resulting equality.
        //
        // 5. If both 'margin-left' and 'margin-right' are 'auto', their used values are equal. This horizontally centers the element with respect to the
        //    edges of the containing block.

        auto& style = layoutBox.style();
        auto width = precomputedWidth ? Length { precomputedWidth.value(), Fixed } : style.logicalWidth();
        auto* containingBlock = layoutBox.containingBlock();
        auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*containingBlock)->width();
        auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);

        LayoutUnit computedWidthValue;
        std::optional<LayoutUnit> computedMarginLeftValue;
        std::optional<LayoutUnit> computedMarginRightValue;

        auto marginLeft = style.marginLeft();
        if (!marginLeft.isAuto())
            computedMarginLeftValue = valueForLength(marginLeft, containingBlockWidth);

        auto marginRight = style.marginRight();
        if (!marginRight.isAuto())
            computedMarginRightValue = valueForLength(marginRight, containingBlockWidth);

        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();
        auto paddingLeft = displayBox.paddingLeft();
        auto paddingRight = displayBox.paddingRight();

        // #1
        if (!width.isAuto()) {
            computedWidthValue = valueForLength(width, containingBlockWidth);
            auto horizontalSpaceForMargin = containingBlockWidth - (computedMarginLeftValue.value_or(0) + borderLeft + paddingLeft 
                + computedWidthValue + paddingRight + borderRight + computedMarginRightValue.value_or(0));

            if (horizontalSpaceForMargin < 0) {
                if (!computedMarginLeftValue)
                    computedMarginLeftValue = LayoutUnit(0);
                if (!computedMarginRightValue)
                    computedMarginRightValue = LayoutUnit(0);
            }
        }

        // #2
        if (!width.isAuto() && !marginLeft.isAuto() && !marginRight.isAuto()) {
            ASSERT(computedMarginLeftValue);
            ASSERT(computedMarginRightValue);

            if (containingBlock->style().isLeftToRightDirection())
                computedMarginRightValue = containingBlockWidth - (*computedMarginLeftValue + borderLeft + paddingLeft  + computedWidthValue + paddingRight + borderRight);
            else
                computedMarginLeftValue = containingBlockWidth - (borderLeft + paddingLeft  + computedWidthValue + paddingRight + borderRight + *computedMarginRightValue);
        }

        // #3
        if (!computedMarginLeftValue && !marginRight.isAuto() && !width.isAuto()) {
            ASSERT(computedMarginRightValue);
            computedMarginLeftValue = containingBlockWidth - (borderLeft + paddingLeft  + computedWidthValue + paddingRight + borderRight + *computedMarginRightValue);
        } else if (!computedMarginRightValue && !marginLeft.isAuto() && !width.isAuto()) {
            ASSERT(computedMarginLeftValue);
            computedMarginRightValue = containingBlockWidth - (*computedMarginLeftValue + borderLeft + paddingLeft  + computedWidthValue + paddingRight + borderRight);
        }

        // #4
        if (width.isAuto())
            computedWidthValue = containingBlockWidth - (computedMarginLeftValue.value_or(0) + borderLeft + paddingLeft + paddingRight + borderRight + computedMarginRightValue.value_or(0));

        // #5
        if (!computedMarginLeftValue && !computedMarginRightValue) {
            auto horizontalSpaceForMargin = containingBlockWidth - (borderLeft + paddingLeft  + computedWidthValue + paddingRight + borderRight);
            computedMarginLeftValue = computedMarginRightValue = horizontalSpaceForMargin / 2;
        }

        ASSERT(computedMarginLeftValue);
        ASSERT(computedMarginRightValue);

        return FormattingContext::Geometry::WidthAndMargin { computedWidthValue, { *computedMarginLeftValue, *computedMarginRightValue } };
    };

    auto computedWidthAndMarginValue = compute();
    if (!isStretchedToViewport(layoutContext, layoutBox))
        return computedWidthAndMarginValue;
    auto initialContainingBlockWidth = layoutContext.displayBoxForLayoutBox(initialContainingBlock(layoutBox))->contentBox().width();
    return FormattingContext::Geometry::WidthAndMargin { std::max(computedWidthAndMarginValue.width, initialContainingBlockWidth), { computedWidthAndMarginValue.margin } };
}

FormattingContext::Geometry::WidthAndMargin BlockFormattingContext::Geometry::inFlowReplacedWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow() && layoutBox.replaced());

    // 10.3.4 Block-level, replaced elements in normal flow
    //
    // 1. The used value of 'width' is determined as for inline replaced elements.
    // 2. Then the rules for non-replaced block-level elements are applied to determine the margins.

    // #1
    auto inlineReplacedWidthAndMargin = FormattingContext::Geometry::inlineReplacedWidthAndMargin(layoutContext, layoutBox);
    // #2
    auto inlineReplacedWidthAndBlockNonReplacedMargin = inFlowNonReplacedWidthAndMargin(layoutContext, layoutBox, inlineReplacedWidthAndMargin.width);
    ASSERT(inlineReplacedWidthAndMargin.width == inlineReplacedWidthAndBlockNonReplacedMargin.width);
    return inlineReplacedWidthAndBlockNonReplacedMargin;
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
    auto top = containingBlockContentBox.top();
    auto left = containingBlockContentBox.left();
    if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
        auto& previousInFlowDisplayBox = *layoutContext.displayBoxForLayoutBox(*previousInFlowSibling);
        top = previousInFlowDisplayBox.bottom() + previousInFlowDisplayBox.marginBottom();
    }
    auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
    LayoutPoint topLeft = { top, left };
    topLeft.moveBy({ displayBox.marginLeft(), displayBox.marginTop() });
    return topLeft;
}

LayoutPoint BlockFormattingContext::Geometry::inFlowPositionedPosition(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlowPositioned());
    // 9.4.3 Relative positioning
    //
    // The 'top' and 'bottom' properties move relatively positioned element(s) up or down without changing their size.
    // Top' moves the boxes down, and 'bottom' moves them up. Since boxes are not split or stretched as a result of 'top' or 'bottom', the used values are always: top = -bottom.
    //
    // 1. If both are 'auto', their used values are both '0'.
    // 2. If one of them is 'auto', it becomes the negative of the other.
    // 3. If neither is 'auto', 'bottom' is ignored (i.e., the used value of 'bottom' will be minus the value of 'top').
    auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
    auto& style = layoutBox.style();

    auto top = style.logicalTop();
    auto bottom = style.logicalBottom();
    LayoutUnit topDelta;

    if (top.isAuto() && bottom.isAuto()) {
        // #1
        topDelta = 0;
    } else if (top.isAuto()) {
        // #2
        topDelta = -bottom.value();
    } else if (bottom.isAuto()) {
        // #3 #4
        topDelta = top.value();
    }

    // For relatively positioned elements, 'left' and 'right' move the box(es) horizontally, without changing their size.
    // 'Left' moves the boxes to the right, and 'right' moves them to the left.
    // Since boxes are not split or stretched as a result of 'left' or 'right', the used values are always: left = -right.
    //
    // 1. If both 'left' and 'right' are 'auto' (their initial values), the used values are '0' (i.e., the boxes stay in their original position).
    // 2. If 'left' is 'auto', its used value is minus the value of 'right' (i.e., the boxes move to the left by the value of 'right').
    // 3. If 'right' is specified as 'auto', its used value is minus the value of 'left'.
    // 4. If neither 'left' nor 'right' is 'auto', the position is over-constrained, and one of them has to be ignored.
    //    If the 'direction' property of the containing block is 'ltr', the value of 'left' wins and 'right' becomes -'left'.
    //    If 'direction' of the containing block is 'rtl', 'right' wins and 'left' is ignored.
    //
    auto left = style.logicalLeft();
    auto right = style.logicalRight();
    LayoutUnit leftDelta;

    if (left.isAuto() && right.isAuto()) {
        // #1
        leftDelta = 0;
    } else if (left.isAuto()) {
        // #2
        leftDelta = -right.value();
    } else if (right.isAuto()) {
        // #3
        leftDelta = left.value();
    } else {
        // #4
        // FIXME: take direction into account
        leftDelta = left.value();
    }

    return { displayBox.left() + leftDelta, displayBox.top() + topDelta };
}

LayoutUnit BlockFormattingContext::Geometry::inFlowHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());

    if (!layoutBox.replaced())
        return inFlowNonReplacedHeight(layoutContext, layoutBox);
    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block'
    // replaced elements in normal flow and floating replaced elements
    return FormattingContext::Geometry::inlineReplacedHeight(layoutContext, layoutBox);
}

FormattingContext::Geometry::WidthAndMargin BlockFormattingContext::Geometry::inFlowWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());

    if (!layoutBox.replaced())
        return inFlowNonReplacedWidthAndMargin(layoutContext, layoutBox);
    return inFlowReplacedWidthAndMargin(layoutContext, layoutBox);
}

}
}

#endif

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
#include "InlineFormattingState.h"
#include "LayoutChildIterator.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static const Container& initialContainingBlock(const Box& layoutBox)
{
    auto* containingBlock = layoutBox.containingBlock();
    while (containingBlock->containingBlock())
        containingBlock = containingBlock->containingBlock();
    return *containingBlock;
}

static bool isStretchedToInitialContainingBlock(const LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());
    // In quirks mode, body and html stretch to the viewport.
    if (!layoutContext.inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

static HeightAndMargin stretchHeightToInitialContainingBlock(HeightAndMargin heightAndMargin, LayoutUnit initialContainingBlockHeight)
{
    auto verticalMargins = heightAndMargin.margin.top + heightAndMargin.margin.bottom;
    // Stretch but never overstretch with the margins.
    if (heightAndMargin.height + verticalMargins < initialContainingBlockHeight)
        heightAndMargin.height = initialContainingBlockHeight - verticalMargins;

    return heightAndMargin;
}

static WidthAndMargin stretchWidthToInitialContainingBlock(WidthAndMargin widthAndMargin, LayoutUnit initialContainingBlockWidth)
{
    auto horizontalMargins = widthAndMargin.margin.left + widthAndMargin.margin.right;
    // Stretch but never overstretch with the margins.
    if (widthAndMargin.width + horizontalMargins < initialContainingBlockWidth)
        widthAndMargin.width = initialContainingBlockWidth - horizontalMargins;

    return widthAndMargin;
}

HeightAndMargin BlockFormattingContext::Geometry::inFlowNonReplacedHeightAndMargin(const LayoutContext& layoutContext, const Box& layoutBox, std::optional<LayoutUnit> usedHeight)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());
    ASSERT(layoutBox.isOverflowVisible());

    auto compute = [&]() -> HeightAndMargin {

        // 10.6.3 Block-level non-replaced elements in normal flow when 'overflow' computes to 'visible'
        //
        // If 'margin-top', or 'margin-bottom' are 'auto', their used value is 0.
        // If 'height' is 'auto', the height depends on whether the element has any block-level children and whether it has padding or borders:
        // The element's height is the distance from its top content edge to the first applicable of the following:
        // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
        // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin
        //    does not collapse with the element's bottom margin
        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        // 4. zero, otherwise
        // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
        // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.

        auto& style = layoutBox.style();
        auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth();
        auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);

        VerticalEdges nonCollapsedMargin = { computedValueIfNotAuto(style.marginTop(), containingBlockWidth).value_or(0),
            computedValueIfNotAuto(style.marginBottom(), containingBlockWidth).value_or(0) }; 
        VerticalEdges collapsedMargin = { MarginCollapse::marginTop(layoutContext, layoutBox), MarginCollapse::marginBottom(layoutContext, layoutBox) };
        auto borderAndPaddingTop = displayBox.borderTop() + displayBox.paddingTop().value_or(0);
        
        auto height = usedHeight ? Length { usedHeight.value(), Fixed } : style.logicalHeight();
        if (!height.isAuto()) {
            if (height.isFixed())
                return { height.value(), nonCollapsedMargin, collapsedMargin };

            // Most notably height percentage.
            ASSERT_NOT_IMPLEMENTED_YET();
        }

        if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
            return { 0, nonCollapsedMargin, collapsedMargin };

        // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
        if (layoutBox.establishesInlineFormattingContext()) {
            // This is temp and will be replaced by the correct display box once inline runs move over to the display tree.
            auto& lastInlineRun = downcast<InlineFormattingState>(layoutContext.establishedFormattingState(layoutBox)).inlineRuns().last();
            return { lastInlineRun.logicalBottom(), nonCollapsedMargin, collapsedMargin };
        }

        // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin...
        auto* lastInFlowChild = downcast<Container>(layoutBox).lastInFlowChild();
        ASSERT(lastInFlowChild);
        if (!MarginCollapse::isMarginBottomCollapsedWithParent(layoutContext, *lastInFlowChild)) {
            auto& lastInFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*lastInFlowChild);
            return { lastInFlowDisplayBox.bottom() + lastInFlowDisplayBox.marginBottom() - borderAndPaddingTop, nonCollapsedMargin, collapsedMargin };
        }

        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        auto* inFlowChild = lastInFlowChild;
        while (inFlowChild && MarginCollapse::isMarginTopCollapsedWithParentMarginBottom(*inFlowChild))
            inFlowChild = inFlowChild->previousInFlowSibling();
        if (inFlowChild) {
            auto& inFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*inFlowChild);
            return { inFlowDisplayBox.top() + inFlowDisplayBox.borderBox().height() - borderAndPaddingTop, nonCollapsedMargin, collapsedMargin };
        }

        // 4. zero, otherwise
        return { 0, nonCollapsedMargin, collapsedMargin };
    };

    auto heightAndMargin = compute();

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> inflow non-replaced -> height(" << heightAndMargin.height << "px) margin(" << heightAndMargin.margin.top << "px, " << heightAndMargin.margin.bottom << "px) -> layoutBox(" << &layoutBox << ")");
    return heightAndMargin;
}

WidthAndMargin BlockFormattingContext::Geometry::inFlowNonReplacedWidthAndMargin(const LayoutContext& layoutContext, const Box& layoutBox, std::optional<LayoutUnit> usedWidth)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.replaced());

    auto compute = [&]() {

        // 10.3.3 Block-level, non-replaced elements in normal flow
        //
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
        auto* containingBlock = layoutBox.containingBlock();
        auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*containingBlock).contentBoxWidth();
        auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);

        auto width = computedValueIfNotAuto(usedWidth ? Length { usedWidth.value(), Fixed } : style.logicalWidth(), containingBlockWidth);
        auto marginLeft = computedValueIfNotAuto(style.marginLeft(), containingBlockWidth);
        auto marginRight = computedValueIfNotAuto(style.marginRight(), containingBlockWidth);
        auto nonComputedMarginLeft = marginLeft.value_or(0);
        auto nonComputedMarginRight = marginRight.value_or(0);
        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();
        auto paddingLeft = displayBox.paddingLeft().value_or(0);
        auto paddingRight = displayBox.paddingRight().value_or(0);

        // #1
        if (width) {
            auto horizontalSpaceForMargin = containingBlockWidth - (marginLeft.value_or(0) + borderLeft + paddingLeft + *width + paddingRight + borderRight + marginRight.value_or(0));
            if (horizontalSpaceForMargin < 0) {
                marginLeft = marginLeft.value_or(0);
                marginRight = marginRight.value_or(0);
            }
        }

        // #2
        if (width && marginLeft && marginRight) {
            if (containingBlock->style().isLeftToRightDirection())
                marginRight = containingBlockWidth - (*marginLeft + borderLeft + paddingLeft  + *width + paddingRight + borderRight);
            else
                marginLeft = containingBlockWidth - (borderLeft + paddingLeft + *width + paddingRight + borderRight + *marginRight);
        }

        // #3
        if (!marginLeft && width && marginRight)
            marginLeft = containingBlockWidth - (borderLeft + paddingLeft  + *width + paddingRight + borderRight + *marginRight);
        else if (marginLeft && !width && marginRight)
            width = containingBlockWidth - (*marginLeft + borderLeft + paddingLeft + paddingRight + borderRight + *marginRight);
        else if (marginLeft && width && !marginRight)
            marginRight = containingBlockWidth - (*marginLeft + borderLeft + paddingLeft + *width + paddingRight + borderRight);

        // #4
        if (!width) {
            marginLeft = marginLeft.value_or(0);
            marginRight = marginRight.value_or(0);
            width = containingBlockWidth - (*marginLeft + borderLeft + paddingLeft + paddingRight + borderRight + *marginRight);
        }

        // #5
        if (!marginLeft && !marginRight) {
            auto horizontalSpaceForMargin = containingBlockWidth - (borderLeft + paddingLeft  + *width + paddingRight + borderRight);
            marginLeft = marginRight = horizontalSpaceForMargin / 2;
        }

        ASSERT(width);
        ASSERT(marginLeft);
        ASSERT(marginRight);

        return WidthAndMargin { *width, { *marginLeft, *marginRight }, { nonComputedMarginLeft, nonComputedMarginRight } };
    };

    auto widthAndMargin = compute();
    if (!isStretchedToInitialContainingBlock(layoutContext, layoutBox)) {
        LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow non-replaced -> width(" << widthAndMargin.width << "px) margin(" << widthAndMargin.margin.left << "px, " << widthAndMargin.margin.right << "px) -> layoutBox(" << &layoutBox << ")");
        return widthAndMargin;
    }

    auto initialContainingBlockWidth = layoutContext.displayBoxForLayoutBox(initialContainingBlock(layoutBox)).contentBoxWidth();
    widthAndMargin = stretchWidthToInitialContainingBlock(widthAndMargin, initialContainingBlockWidth);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow non-replaced -> streched to viewport-> width(" << widthAndMargin.width << "px) margin(" << widthAndMargin.margin.left << "px, " << widthAndMargin.margin.right << "px) -> layoutBox(" << &layoutBox << ")");
    return widthAndMargin;
}

WidthAndMargin BlockFormattingContext::Geometry::inFlowReplacedWidthAndMargin(const LayoutContext& layoutContext, const Box& layoutBox, std::optional<LayoutUnit> usedWidth)
{
    ASSERT(layoutBox.isInFlow() && layoutBox.replaced());

    // 10.3.4 Block-level, replaced elements in normal flow
    //
    // 1. The used value of 'width' is determined as for inline replaced elements.
    // 2. Then the rules for non-replaced block-level elements are applied to determine the margins.

    // #1
    auto width = inlineReplacedWidthAndMargin(layoutContext, layoutBox, usedWidth).width;
    // #2
    auto nonReplacedWidthAndMargin = inFlowNonReplacedWidthAndMargin(layoutContext, layoutBox, width);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow replaced -> width(" << width << "px) margin(" << nonReplacedWidthAndMargin.margin.left << "px, " << nonReplacedWidthAndMargin.margin.right << "px) -> layoutBox(" << &layoutBox << ")");
    return { width, nonReplacedWidthAndMargin.margin, nonReplacedWidthAndMargin.nonComputedMargin };
}

Position BlockFormattingContext::Geometry::staticPosition(const LayoutContext& layoutContext, const Box& layoutBox)
{
    // https://www.w3.org/TR/CSS22/visuren.html#block-formatting
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    // In a block formatting context, each box's left outer edge touches the left edge of the containing block (for right-to-left formatting, right edges touch).

    LayoutUnit top;
    auto& containingBlockDisplayBox = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock());
    if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
        auto& previousInFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*previousInFlowSibling);
        top = previousInFlowDisplayBox.bottom() + previousInFlowDisplayBox.marginBottom();
    } else
        top = containingBlockDisplayBox.contentBoxTop();

    auto left = containingBlockDisplayBox.contentBoxLeft();
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position] -> static -> top(" << top << "px) left(" << left << "px) layoutBox(" << &layoutBox << ")");
    return { left, top };
}

Position BlockFormattingContext::Geometry::inFlowPositionedPosition(const LayoutContext& layoutContext, const Box& layoutBox)
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

    auto& style = layoutBox.style();
    auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
    auto& containingBlock = *layoutBox.containingBlock();
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(containingBlock).contentBoxWidth();

    auto top = computedValueIfNotAuto(style.logicalTop(), containingBlockWidth);
    auto bottom = computedValueIfNotAuto(style.logicalBottom(), containingBlockWidth);

    if (!top && !bottom) {
        // #1
        top = bottom = { 0 };
    } else if (!top) {
        // #2
        top = -*bottom;
    } else if (!bottom) {
        // #3
        bottom = -*top;
    } else {
        // #4
        bottom = std::nullopt;
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

    auto left = computedValueIfNotAuto(style.logicalLeft(), containingBlockWidth);
    auto right = computedValueIfNotAuto(style.logicalRight(), containingBlockWidth);

    if (!left && !right) {
        // #1
        left = right = { 0 };
    } else if (!left) {
        // #2
        left = -*right;
    } else if (!right) {
        // #3
        right = -*left;
    } else {
        // #4
        auto isLeftToRightDirection = containingBlock.style().isLeftToRightDirection();
        if (isLeftToRightDirection)
            right = -*left;
        else
            left = std::nullopt;
    }

    ASSERT(!bottom || *top == -*bottom);
    ASSERT(!left || *left == -*right);

    auto newTopPosition = displayBox.top() + *top;
    auto newLeftPosition = displayBox.left() + left.value_or(-*right);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position] -> positioned inflow -> top(" << newTopPosition << "px) left(" << newLeftPosition << "px) layoutBox(" << &layoutBox << ")");
    return { newLeftPosition, newTopPosition };
}

HeightAndMargin BlockFormattingContext::Geometry::inFlowHeightAndMargin(const LayoutContext& layoutContext, const Box& layoutBox, std::optional<LayoutUnit> usedHeight)
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block'
    // replaced elements in normal flow and floating replaced elements
    if (layoutBox.replaced())
        return inlineReplacedHeightAndMargin(layoutContext, layoutBox, usedHeight);

    HeightAndMargin heightAndMargin;
    // TODO: Figure out the case for the document element. Let's just complicated-case it for now.
    if (layoutBox.isOverflowVisible() && !layoutBox.isDocumentBox())
        heightAndMargin = inFlowNonReplacedHeightAndMargin(layoutContext, layoutBox, usedHeight);
    else {
        // 10.6.6 Complicated cases
        // Block-level, non-replaced elements in normal flow when 'overflow' does not compute to 'visible' (except if the 'overflow' property's value has been propagated to the viewport).
        heightAndMargin = complicatedCases(layoutContext, layoutBox, usedHeight);
    }

    if (!isStretchedToInitialContainingBlock(layoutContext, layoutBox))
        return heightAndMargin;

    auto initialContainingBlockHeight = layoutContext.displayBoxForLayoutBox(initialContainingBlock(layoutBox)).contentBoxHeight();
    heightAndMargin = stretchHeightToInitialContainingBlock(heightAndMargin, initialContainingBlockHeight);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> inflow non-replaced -> streched to viewport -> height(" << heightAndMargin.height << "px) margin(" << heightAndMargin.margin.top << "px, " << heightAndMargin.margin.bottom << "px) -> layoutBox(" << &layoutBox << ")");
    return heightAndMargin;
}

WidthAndMargin BlockFormattingContext::Geometry::inFlowWidthAndMargin(const LayoutContext& layoutContext, const Box& layoutBox, std::optional<LayoutUnit> usedWidth)
{
    ASSERT(layoutBox.isInFlow());

    if (!layoutBox.replaced())
        return inFlowNonReplacedWidthAndMargin(layoutContext, layoutBox, usedWidth);
    return inFlowReplacedWidthAndMargin(layoutContext, layoutBox, usedWidth);
}

bool BlockFormattingContext::Geometry::instrinsicWidthConstraintsNeedChildrenWidth(const Box& layoutBox)
{
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
        return false;
    return layoutBox.style().width().isAuto();
}

FormattingContext::InstrinsicWidthConstraints BlockFormattingContext::Geometry::instrinsicWidthConstraints(const LayoutContext& layoutContext, const Box& layoutBox)
{
    auto& style = layoutBox.style();
    if (auto width = fixedValue(style.logicalWidth()))
        return { *width, *width };

    // Minimum/maximum width can't be depending on the containing block's width.
    if (!style.logicalWidth().isAuto())
        return { };

    if (!is<Container>(layoutBox))
        return { };

    LayoutUnit minimumIntrinsicWidth;
    LayoutUnit maximumIntrinsicWidth;

    for (auto& child : childrenOfType<Box>(downcast<Container>(layoutBox))) {
        if (child.isOutOfFlowPositioned())
            continue;
        auto& formattingState = layoutContext.formattingStateForBox(child);
        ASSERT(formattingState.isBlockFormattingState());
        auto childInstrinsicWidthConstraints = formattingState.instrinsicWidthConstraints(child);
        ASSERT(childInstrinsicWidthConstraints);
        
        auto& style = child.style();
        auto horizontalMarginBorderAndPadding = fixedValue(style.marginLeft()).value_or(0)
            + LayoutUnit { style.borderLeftWidth() }
            + fixedValue(style.paddingLeft()).value_or(0)
            + fixedValue(style.paddingRight()).value_or(0)
            + LayoutUnit { style.borderRightWidth() }
            + fixedValue(style.marginRight()).value_or(0);

        minimumIntrinsicWidth = std::max(minimumIntrinsicWidth, childInstrinsicWidthConstraints->minimum + horizontalMarginBorderAndPadding); 
        maximumIntrinsicWidth = std::max(maximumIntrinsicWidth, childInstrinsicWidthConstraints->maximum + horizontalMarginBorderAndPadding); 
    }

    return { minimumIntrinsicWidth, maximumIntrinsicWidth };
}

LayoutUnit BlockFormattingContext::Geometry::estimatedMarginTop(const LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());
    // Can't estimate vertical margins for out of flow boxes (and we shouldn't need to do it for float boxes).
    ASSERT(layoutBox.isInFlow());
    // Can't cross block formatting context boundary.
    ASSERT(!layoutBox.establishesBlockFormattingContext());

    // Let's just use the normal path for now.
    return MarginCollapse::marginTop(layoutContext, layoutBox);
}


}
}

#endif

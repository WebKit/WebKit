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
#include "FormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

static LayoutUnit contentHeightForFormattingContextRoot(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.style().logicalHeight().isAuto() && layoutBox.establishesFormattingContext());
    // 10.6.7 'Auto' heights for block formatting context roots

    // If it only has inline-level children, the height is the distance between the top of the topmost line box and the bottom of the bottommost line box.
    // If it has block-level children, the height is the distance between the top margin-edge of the topmost block-level
    // child box and the bottom margin-edge of the bottommost block-level child box.

    // In addition, if the element has any floating descendants whose bottom margin edge is below the element's bottom content edge,
    // then the height is increased to include those edges. Only floats that participate in this block formatting context are taken
    // into account, e.g., floats inside absolutely positioned descendants or other floats are not.
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
        return 0;

    auto& formattingRootContainer = downcast<Container>(layoutBox);
    if (formattingRootContainer.establishesInlineFormattingContext())
        return 0;

    auto* firstDisplayBox = layoutContext.displayBoxForLayoutBox(*formattingRootContainer.firstInFlowChild());
    auto* lastDisplayBox = layoutContext.displayBoxForLayoutBox(*formattingRootContainer.lastInFlowChild());

    auto top = firstDisplayBox->marginBox().top();
    auto bottom = lastDisplayBox->marginBox().bottom();
    // FIXME: add floating support.
    return bottom - top;
}

static LayoutUnit shrinkToFitWidth(LayoutContext&, const Box&)
{
    return { };
}

LayoutUnit FormattingContext::Geometry::outOfFlowNonReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced());

    // 10.6.4 Absolutely positioned, non-replaced elements
    //
    // For absolutely positioned elements, the used values of the vertical dimensions must satisfy this constraint:
    // 'top' + 'margin-top' + 'border-top-width' + 'padding-top' + 'height' + 'padding-bottom' + 'border-bottom-width' + 'margin-bottom' + 'bottom'
    // = height of containing block

    // If all three of 'top', 'height', and 'bottom' are auto, set 'top' to the static position and apply rule number three below.

    // If none of the three are 'auto': If both 'margin-top' and 'margin-bottom' are 'auto', solve the equation under the extra
    // constraint that the two margins get equal values. If one of 'margin-top' or 'margin-bottom' is 'auto', solve the equation for that value.
    // If the values are over-constrained, ignore the value for 'bottom' and solve for that value.

    // Otherwise, pick the one of the following six rules that applies.

    // 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then the height is based on the content per 10.6.7,
    //     set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then set 'top' to the static position, set 'auto' values for
    //    'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then the height is based on the content per 10.6.7, set 'auto'
    //     values for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', then 'auto' values for 'margin-top' and 'margin-bottom' are set to 0 and solve for 'height'
    // 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0 and solve for 'bottom'
    auto& style = layoutBox.style();
    auto top = style.logicalTop();
    auto bottom = style.logicalBottom();
    auto height = style.logicalHeight(); 

    auto containingBlockHeight = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->height();
    LayoutUnit computedHeightValue;

    if (!height.isAuto())
        computedHeightValue = valueForLength(height, containingBlockHeight);
    else if ((top.isAuto() && bottom.isAuto())
        || (top.isAuto() && !bottom.isAuto())
        || (!top.isAuto() && bottom.isAuto())) {
        // All auto (#3), #1 and #3
        computedHeightValue = contentHeightForFormattingContextRoot(layoutContext, layoutBox);
    } else if (!top.isAuto() && !bottom.isAuto()) {
        // #5
        auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);

        auto marginTop = displayBox.marginTop();
        auto marginBottom = displayBox.marginBottom();
    
        auto paddingTop = displayBox.paddingTop();
        auto paddingBottom = displayBox.paddingBottom();

        auto borderTop = displayBox.borderTop();
        auto borderBottom = displayBox.borderBottom();

        computedHeightValue = containingBlockHeight - (top.value() + marginTop + borderTop + paddingTop + paddingBottom + borderBottom + marginBottom + bottom.value());
    } else {
        // #2 #4 #6 have height != auto
        ASSERT_NOT_REACHED();
    }

    return computedHeightValue;
}

LayoutUnit FormattingContext::Geometry::outOfFlowNonReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced());
    
    // 10.3.7 Absolutely positioned, non-replaced elements
    //
    // 'left' + 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' + 'right'
    // = width of containing block

    // If all three of 'left', 'width', and 'right' are 'auto': First set any 'auto' values for 'margin-left' and 'margin-right' to 0.
    // Then, if the 'direction' property of the element establishing the static-position containing block is 'ltr' set 'left' to the static
    // position and apply rule number three below; otherwise, set 'right' to the static position and apply rule number one below.

    // 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the width is shrink-to-fit. Then solve for 'left'
    // 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if the 'direction' property of the element establishing the static-position 
    //    containing block is 'ltr' set 'left' to the static position, otherwise set 'right' to the static position.
    //    Then solve for 'left' (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
    // 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the width is shrink-to-fit . Then solve for 'right'
    // 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
    // 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
    // 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'
    auto& style = layoutBox.style();
    auto left = style.logicalLeft();
    auto right = style.logicalRight();
    auto width = style.logicalWidth();

    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->width();
    LayoutUnit computedWidthValue;

    if (!width.isAuto())
        computedWidthValue = valueForLength(width, containingBlockWidth);
    else if ((left.isAuto() && right.isAuto())
        || (left.isAuto() && !right.isAuto())
        || (!left.isAuto() && right.isAuto())) {
        // All auto (#1), #1 and #3
        computedWidthValue = shrinkToFitWidth(layoutContext, layoutBox);
    } else if (!left.isAuto() && !right.isAuto()) {
        // #5
        auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);

        auto marginLeft = displayBox.marginLeft();
        auto marginRight = displayBox.marginRight();
    
        auto paddingLeft = displayBox.paddingLeft();
        auto paddingRight = displayBox.paddingRight();

        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();

        computedWidthValue = containingBlockWidth - (left.value() + marginLeft + borderLeft + paddingLeft + paddingRight + borderRight + marginRight + right.value());
    } else {
        // #2 #4 #6 have width != auto
        ASSERT_NOT_REACHED();
    }

    return computedWidthValue;
}

LayoutUnit FormattingContext::Geometry::outOfFlowReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && layoutBox.replaced());
    // 10.6.5 Absolutely positioned, replaced elements
    //
    // The used value of 'height' is determined as for inline replaced elements.
    return replacedHeight(layoutContext, layoutBox);
}

LayoutUnit FormattingContext::Geometry::outOfFlowReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && layoutBox.replaced());
    // 10.3.8 Absolutely positioned, replaced elements
    //
    // The used value of 'width' is determined as for inline replaced elements.
    return replacedWidth(layoutContext, layoutBox);
}

LayoutUnit FormattingContext::Geometry::floatingNonReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isFloatingPositioned() && !layoutBox.replaced());
    // 10.6.6 Complicated cases
    //
    // Floating, non-replaced elements.
    //
    // If 'height' is 'auto', the height depends on the element's descendants per 10.6.7.
    auto height = layoutBox.style().logicalHeight();
    return height.isAuto() ? contentHeightForFormattingContextRoot(layoutContext, layoutBox) : LayoutUnit(height.value());
}

LayoutUnit FormattingContext::Geometry::floatingNonReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isFloatingPositioned() && !layoutBox.replaced());
    // 10.3.5 Floating, non-replaced elements

    // If 'width' is computed as 'auto', the used value is the "shrink-to-fit" width.
    auto width = layoutBox.style().logicalWidth();
    return width.isAuto() ? shrinkToFitWidth(layoutContext, layoutBox) : LayoutUnit(width.value());
}

LayoutUnit FormattingContext::Geometry::floatingReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isFloatingPositioned() && layoutBox.replaced());
    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block'
    // replaced elements in normal flow and floating replaced elements
    return replacedHeight(layoutContext, layoutBox);
}

LayoutUnit FormattingContext::Geometry::floatingReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isFloatingPositioned() && layoutBox.replaced());
    // 10.3.6 Floating, replaced elements
    //
    // The used value of 'width' is determined as for inline replaced elements.
    return replacedWidth(layoutContext, layoutBox);
}

LayoutPoint FormattingContext::Geometry::outOfFlowNonReplacedPosition(LayoutContext& layoutContext, const Box& layoutBox)
{
    // 10.3.7 Absolutely positioned, non-replaced elements (left/right)
    // 10.6.4 Absolutely positioned, non-replaced elements (top/bottom)

    // At this point we've the size computed.
    auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
    auto size = displayBox.size();
    auto& style = layoutBox.style();

    // 10.6.4 Absolutely positioned, non-replaced elements
    auto top = style.logicalTop();
    auto bottom = style.logicalBottom();
    auto containingBlockHeight = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->height();

    // 'top' + 'margin-top' + 'border-top-width' + 'padding-top' + 'height' + 'padding-bottom' + 'border-bottom-width' + 'margin-bottom' + 'bottom'
    // = height of containing block
    //
    // 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then the height is based on the content per 10.6.7,
    //     set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then set 'top' to the static position, set 'auto' values for
    //    'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then the height is based on the content per 10.6.7, set 'auto'
    //     values for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', then 'auto' values for 'margin-top' and 'margin-bottom' are set to 0 and solve for 'height'
    // 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0 and solve for 'bottom'
    LayoutUnit computedTopValue;
    if (top.isAuto() && !bottom.isAuto()) {
        // #1 #4
        auto marginTop = displayBox.marginTop();
        auto marginBottom = displayBox.marginBottom();
    
        auto paddingTop = displayBox.paddingTop();
        auto paddingBottom = displayBox.paddingBottom();

        auto borderTop = displayBox.borderTop();
        auto borderBottom = displayBox.borderBottom();

        computedTopValue = containingBlockHeight - (marginTop + borderTop + paddingTop + size.height() + paddingBottom + borderBottom + marginBottom + bottom.value());
    } else if (top.isAuto() && bottom.isAuto()) {
        // #2
        // Already computed as part of the computeStaticPosition();
        computedTopValue = displayBox.top();
    } else {
        // #3 #5 #6 have top != auto
        computedTopValue = valueForLength(top, containingBlockHeight);
    }


    // 10.3.7 Absolutely positioned, non-replaced elements
    auto left = style.logicalLeft();
    auto right = style.logicalRight();
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->width();

    // 'left' + 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' + 'right'
    // = width of containing block
    //
    // If all three of 'left', 'width', and 'right' are 'auto': First set any 'auto' values for 'margin-left' and 'margin-right' to 0.
    // Then, if the 'direction' property of the element establishing the static-position containing block is 'ltr' set 'left' to the static
    // position and apply rule number three below; otherwise, set 'right' to the static position and apply rule number one below.

    // 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the width is shrink-to-fit. Then solve for 'left'
    // 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if the 'direction' property of the element establishing the static-position 
    //    containing block is 'ltr' set 'left' to the static position, otherwise set 'right' to the static position.
    //    Then solve for 'left' (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
    // 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the width is shrink-to-fit . Then solve for 'right'
    // 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
    // 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
    // 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'
    LayoutUnit computedLeftValue;
    if (left.isAuto() && !right.isAuto()) {
        // #1 #4
        auto marginLeft = displayBox.marginLeft();
        auto marginRight = displayBox.marginRight();
    
        auto paddingLeft = displayBox.paddingLeft();
        auto paddingRight = displayBox.paddingRight();

        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();

        computedLeftValue = containingBlockWidth - (marginLeft + borderLeft + paddingLeft + size.width() + paddingRight + borderRight + marginRight + right.value());
    } else if (left.isAuto() && right.isAuto()) {
        // #2
        // FIXME: rtl
        computedLeftValue = displayBox.left();
    } else {
        // #3 #5 #6 have left != auto
        computedLeftValue = valueForLength(left, containingBlockWidth);
    }

    return { computedLeftValue, computedTopValue };
}

LayoutPoint FormattingContext::Geometry::outOfFlowReplacedPosition(LayoutContext& layoutContext, const Box& layoutBox)
{
    // 10.6.5 Absolutely positioned, replaced elements (top/bottom)
    // 10.3.8 Absolutely positioned, replaced elements (left/right)

    // At this point we've the size computed.
    auto& displayBox = *layoutContext.displayBoxForLayoutBox(layoutBox);
    auto size = displayBox.size();
    auto& style = layoutBox.style();

    // 10.6.5 Absolutely positioned, replaced elements
    //
    // This situation is similar to the previous one, except that the element has an intrinsic height. The sequence of substitutions is now:
    // The used value of 'height' is determined as for inline replaced elements. If 'margin-top' or 'margin-bottom' is specified as 'auto'
    // its used value is determined by the rules below.
    //
    // 1. If both 'top' and 'bottom' have the value 'auto', replace 'top' with the element's static position.
    // 2. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or 'margin-bottom' with '0'.
    // 3. If at this point both 'margin-top' and 'margin-bottom' are still 'auto', solve the equation under the extra constraint that the two margins must get equal values.
    // 4. If at this point there is only one 'auto' left, solve the equation for that value.
    // 5. If at this point the values are over-constrained, ignore the value for 'bottom' and solve for that value.
    auto top = style.logicalTop();
    auto bottom = style.logicalBottom();
    auto containingBlockHeight = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->height();
    LayoutUnit computedTopValue;

    if (!top.isAuto())
        computedTopValue = valueForLength(top, containingBlockHeight);
    else if (bottom.isAuto()) {
        // #1
        computedTopValue = displayBox.top();
    } else {
        // #4
        auto marginTop = displayBox.marginTop();
        auto marginBottom = displayBox.marginBottom();

        auto paddingTop = displayBox.paddingTop();
        auto paddingBottom = displayBox.paddingBottom();

        auto borderTop = displayBox.borderTop();
        auto borderBottom = displayBox.borderBottom();

        computedTopValue = containingBlockHeight - (marginTop + borderTop + paddingTop + size.height() + paddingBottom + borderBottom + marginBottom + bottom.value());
    }


    // 10.3.8 Absolutely positioned, replaced elements
    //
    // In this case, section 10.3.7 applies up through and including the constraint equation, but the rest of section 10.3.7 is replaced by the following rules:
    //
    // The used value of 'width' is determined as for inline replaced elements. 
    //
    // 1. If 'margin-left' or 'margin-right' is specified as 'auto' its used value is determined by the rules below.
    // 2. If both 'left' and 'right' have the value 'auto', then if the 'direction' property of the element establishing the
    //    static-position containing block is 'ltr', set 'left' to the static position; else if 'direction' is 'rtl', set 'right' to the static position.
    // 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left' or 'margin-right' with '0'.
    // 4. If at this point both 'margin-left' and 'margin-right' are still 'auto', solve the equation under the extra constraint
    //    that the two margins must get equal values, unless this would make them negative, in which case when the direction of
    //    the containing block is 'ltr' ('rtl'), set 'margin-left' ('margin-right') to zero and solve for 'margin-right' ('margin-left').
    // 5. If at this point there is an 'auto' left, solve the equation for that value.
    // 6. If at this point the values are over-constrained, ignore the value for either 'left' (in case the 'direction'
    //    property of the containing block is 'rtl') or 'right' (in case 'direction' is 'ltr') and solve for that value.
    auto left = style.logicalLeft();
    auto right = style.logicalRight();
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->width();
    LayoutUnit computedLeftValue;

    if (!left.isAuto())   
        computedLeftValue = valueForLength(left, containingBlockWidth);
    else if (right.isAuto()) {
        // FIXME: take direction into account
        computedLeftValue = displayBox.left();
    } else {
        // #5
        auto marginLeft = displayBox.marginLeft();
        auto marginRight = displayBox.marginRight();
    
        auto paddingLeft = displayBox.paddingLeft();
        auto paddingRight = displayBox.paddingRight();

        auto borderLeft = displayBox.borderLeft();
        auto borderRight = displayBox.borderRight();

        computedLeftValue = containingBlockWidth - (marginLeft + borderLeft + paddingLeft + size.width() + paddingRight + borderRight + marginRight + right.value());
    }

    return { computedLeftValue, computedTopValue };
}

LayoutUnit FormattingContext::Geometry::replacedHeight(LayoutContext&, const Box& layoutBox)
{
    ASSERT((layoutBox.isOutOfFlowPositioned() || layoutBox.isFloatingPositioned() || layoutBox.isInFlow()) && layoutBox.replaced());
    // 10.6.5 Absolutely positioned, replaced elements. The used value of 'height' is determined as for inline replaced elements.

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    //
    // 1. If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    //
    // 2. Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    //    (used width) / (intrinsic ratio)
    //
    // 3. Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
    //
    // 4. Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to
    //    the height of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.
    auto& style = layoutBox.style();
    auto width = style.logicalWidth();
    auto height = style.logicalHeight();

    LayoutUnit computedHeightValue;
    auto replaced = layoutBox.replaced();
    ASSERT(replaced);

    if (height.isAuto()) {
        if (width.isAuto() && replaced->hasIntrinsicHeight()) {
            // #1
            computedHeightValue = replaced->intrinsicHeight();
        } else if (replaced->hasIntrinsicRatio()) {
            // #2
            computedHeightValue = width.value() / replaced->intrinsicRatio();
        } else if (replaced->hasIntrinsicHeight()) {
            // #3
            computedHeightValue = replaced->intrinsicHeight();
        } else {
            // #4
            computedHeightValue = 150;
        }
    } else
        computedHeightValue = height.value();

    return computedHeightValue;
}

LayoutUnit FormattingContext::Geometry::replacedWidth(LayoutContext&, const Box& layoutBox)
{
    ASSERT((layoutBox.isOutOfFlowPositioned() || layoutBox.isFloatingPositioned() || layoutBox.isInFlow()) && layoutBox.replaced());

    // 10.3.4 Block-level, replaced elements in normal flow: The used value of 'width' is determined as for inline replaced elements.
    // 10.3.6 Floating, replaced elements: The used value of 'width' is determined as for inline replaced elements.
    // 10.3.8 Absolutely positioned, replaced elements: The used value of 'width' is determined as for inline replaced elements.

    // 10.3.2 Inline, replaced elements
    //
    // 1. If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
    //
    // 2. If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
    //    or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio;
    //    then the used value of 'width' is: (used height) * (intrinsic ratio)
    //
    // 3. If 'height' and 'width' both have computed values of 'auto' and the element has an intrinsic ratio but no intrinsic height or width,
    //    then the used value of 'width' is undefined in CSS 2.2. However, it is suggested that, if the containing block's width does not itself depend on the replaced
    //    element's width, then the used value of 'width' is calculated from the constraint equation used for block-level, non-replaced elements in normal flow.
    //
    // 4. Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
    //
    // 5. Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px.
    //    If 300px is too wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.
    auto& style = layoutBox.style();
    auto width = style.logicalWidth();
    auto height = style.logicalHeight();

    LayoutUnit computedWidthValue;
    auto replaced = layoutBox.replaced();
    ASSERT(replaced);

    if (width.isAuto() && height.isAuto() && replaced->hasIntrinsicWidth()) {
        // #1
        computedWidthValue = replaced->intrinsicWidth();
    } else if (width.isAuto() && (height.isCalculated() || replaced->hasIntrinsicHeight()) && replaced->hasIntrinsicRatio()) {
        // #2
        auto usedHeight = height.isCalculated() ? LayoutUnit(height.value()) : replaced->intrinsicHeight();
        computedWidthValue = usedHeight * replaced->intrinsicRatio();
    } else if (width.isAuto() && height.isAuto() && replaced->hasIntrinsicRatio()) {
        // #3
        // FIXME: undefined but surely doable.
        ASSERT_NOT_IMPLEMENTED_YET();
    } else if (width.isAuto() && replaced->hasIntrinsicWidth()) {
        // #4
        computedWidthValue = replaced->intrinsicWidth();
    } else {
        // #5
        computedWidthValue = 300;
    }

    return computedWidthValue;
}

Display::Box::Edges FormattingContext::Geometry::computedBorder(LayoutContext&, const Box& layoutBox)
{
    auto& style = layoutBox.style();
    return {
        style.borderTop().width(),
        style.borderLeft().width(),
        style.borderBottom().width(),
        style.borderRight().width()
    };
}

std::optional<Display::Box::Edges> FormattingContext::Geometry::computedPadding(LayoutContext& layoutContext, const Box& layoutBox)
{
    if (!layoutBox.isPaddingApplicable())
        return std::nullopt;

    auto& style = layoutBox.style();
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->width();
    return Display::Box::Edges(
        valueForLength(style.paddingTop(), containingBlockWidth),
        valueForLength(style.paddingLeft(), containingBlockWidth),
        valueForLength(style.paddingBottom(), containingBlockWidth),
        valueForLength(style.paddingRight(), containingBlockWidth)
    );
}

}
}
#endif

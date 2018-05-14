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

#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FormattingContext);

FormattingContext::FormattingContext(const Box& formattingContextRoot)
    : m_root(makeWeakPtr(const_cast<Box&>(formattingContextRoot)))
{
}

FormattingContext::~FormattingContext()
{
}

void FormattingContext::computeStaticPosition(LayoutContext&, const Box&, Display::Box&) const
{
}

void FormattingContext::computeInFlowPositionedPosition(const Box&, Display::Box&) const
{
}

void FormattingContext::computeOutOfFlowPosition(const Box&, Display::Box&) const
{
}

void FormattingContext::computeWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowWidth(layoutContext, layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingWidth(layoutBox, displayBox);
    return computeInFlowWidth(layoutBox, displayBox);
}

void FormattingContext::computeHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowHeight(layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingHeight(layoutBox, displayBox);
    return computeInFlowHeight(layoutContext, layoutBox, displayBox);
}

void FormattingContext::computeOutOfFlowWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (!layoutBox.isReplaced()) {
        computeOutOfFlowNonReplacedWidth(layoutContext, layoutBox, displayBox);
        return;
    }
    ASSERT_NOT_REACHED();
}

void FormattingContext::computeFloatingWidth(const Box&, Display::Box&) const
{
}

void FormattingContext::computeOutOfFlowHeight(const Box&, Display::Box&) const
{
}

void FormattingContext::computeFloatingHeight(const Box&, Display::Box&) const
{
}

LayoutUnit FormattingContext::marginTop(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginLeft(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginBottom(const Box&) const
{
    return 0;
}

LayoutUnit FormattingContext::marginRight(const Box&) const
{
    return 0;
}

void FormattingContext::placeInFlowPositionedChildren(const Container&) const
{
}

void FormattingContext::layoutOutOfFlowDescendants(LayoutContext& layoutContext) const
{
    if (!is<Container>(m_root.get()))
        return;
    for (auto& outOfFlowBox : downcast<Container>(*m_root).outOfFlowDescendants()) {
        auto& layoutBox = *outOfFlowBox;
        auto& displayBox = layoutContext.createDisplayBox(layoutBox);

        computeOutOfFlowPosition(layoutBox, displayBox);
        computeOutOfFlowWidth(layoutContext, layoutBox, displayBox);

        ASSERT(layoutBox.establishesFormattingContext());
        auto formattingContext = layoutContext.formattingContext(layoutBox);
        formattingContext->layout(layoutContext, layoutContext.establishedFormattingState(layoutBox, *formattingContext));

        computeOutOfFlowHeight(layoutBox, displayBox);
    }
}

void FormattingContext::computeOutOfFlowNonReplacedWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
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

    if ((left.isAuto() && width.isAuto() && right.isAuto())
        || (left.isAuto() && width.isAuto() && !right.isAuto())
        || (!left.isAuto() && width.isAuto() && right.isAuto())) {
        // All auto (#1), #1 and #3
        computedWidthValue = shrinkToFitWidth(layoutContext, layoutBox);
    } else if (!left.isAuto() && width.isAuto() && !right.isAuto()) {
        // #5
        auto marginLeft = style.marginLeft().isAuto() ? LayoutUnit() : valueForLength(style.marginLeft(), containingBlockWidth);
        auto marginRight = style.marginRight().isAuto() ? LayoutUnit() : valueForLength(style.marginRight(), containingBlockWidth);
    
        auto paddingLeft = valueForLength(style.paddingTop(), containingBlockWidth);
        auto paddingRight = valueForLength(style.paddingBottom(), containingBlockWidth);

        auto borderLeftWidth = style.borderStartWidth();
        auto borderRightWidth = style.borderEndWidth();

        computedWidthValue = containingBlockWidth - (left.value() + marginLeft + borderLeftWidth + paddingLeft + paddingRight + borderRightWidth + marginRight + right.value());
    } else if (!width.isAuto())
        computedWidthValue = valueForLength(width, containingBlockWidth);
    else
        ASSERT_NOT_REACHED();

    displayBox.setWidth(computedWidthValue);
}

LayoutUnit FormattingContext::shrinkToFitWidth(LayoutContext&, const Box&) const
{
    return 0;
}

}
}
#endif

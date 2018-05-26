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
#include "LayoutDescendantIterator.h"
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

void FormattingContext::computeInFlowPositionedPosition(LayoutContext&, const Box&, Display::Box&) const
{
}

void FormattingContext::computeOutOfFlowPosition(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    LayoutPoint computedTopLeft;

    if (layoutBox.replaced())
        computedTopLeft = Geometry::outOfFlowReplacedPosition(layoutContext, layoutBox);
    else
        computedTopLeft = Geometry::outOfFlowNonReplacedPosition(layoutContext, layoutBox);

    displayBox.setTopLeft(computedTopLeft);
}

void FormattingContext::computeWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowWidth(layoutContext, layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingWidth(layoutContext, layoutBox, displayBox);
    return computeInFlowWidth(layoutContext, layoutBox, displayBox);
}

void FormattingContext::computeHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isOutOfFlowPositioned())
        return computeOutOfFlowHeight(layoutContext, layoutBox, displayBox);
    if (layoutBox.isFloatingPositioned())
        return computeFloatingHeight(layoutContext, layoutBox, displayBox);
    return computeInFlowHeight(layoutContext, layoutBox, displayBox);
}

void FormattingContext::computeOutOfFlowWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    LayoutUnit computedWidth;

    if (layoutBox.replaced())
        computedWidth = Geometry::outOfFlowReplacedWidth(layoutContext, layoutBox);
    else 
        computedWidth = Geometry::outOfFlowNonReplacedWidth(layoutContext, layoutBox);

    displayBox.setWidth(computedWidth);
}

void FormattingContext::computeFloatingWidth(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    LayoutUnit computedWidth;

    if (layoutBox.replaced())
        computedWidth = Geometry::floatingReplacedWidth(layoutContext, layoutBox);
    else
        computedWidth = Geometry::floatingNonReplacedWidth(layoutContext, layoutBox);

    displayBox.setWidth(computedWidth);
}

void FormattingContext::computeOutOfFlowHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    LayoutUnit computedHeight;

    if (layoutBox.replaced())
        computedHeight = Geometry::outOfFlowReplacedHeight(layoutContext, layoutBox);
    else
        computedHeight = Geometry::outOfFlowNonReplacedHeight(layoutContext, layoutBox);

    displayBox.setHeight(computedHeight);
}

void FormattingContext::computeFloatingHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    LayoutUnit computedHeight;

    if (layoutBox.replaced())
        computedHeight = Geometry::floatingReplacedHeight(layoutContext, layoutBox);
    else
        computedHeight = Geometry::floatingNonReplacedHeight(layoutContext, layoutBox);

    displayBox.setHeight(computedHeight);
}

void FormattingContext::computeMargin(LayoutContext&, const Box&, Display::Box& displayBox) const
{
    displayBox.setMargin({ 0, 0, 0, 0 });
}

void FormattingContext::computeBorderAndPadding(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    displayBox.setBorder(Geometry::computedBorder(layoutContext, layoutBox));
    if (auto padding = Geometry::computedPadding(layoutContext, layoutBox))
        displayBox.setPadding(*padding);
}

void FormattingContext::placeInFlowPositionedChildren(LayoutContext& layoutContext, const Container& container) const
{
    // If this container also establishes a formatting context, then positioning already has happend in that the formatting context.
    if (container.establishesFormattingContext() && &container != &root())
        return;

    for (auto& layoutBox : childrenOfType<Box>(container)) {
        if (!layoutBox.isInFlowPositioned())
            continue;
        computeInFlowPositionedPosition(layoutContext, layoutBox, *layoutContext.displayBoxForLayoutBox(layoutBox));
    }
}

void FormattingContext::layoutOutOfFlowDescendants(LayoutContext& layoutContext) const
{
    if (!is<Container>(m_root.get()))
        return;
    for (auto& outOfFlowBox : downcast<Container>(*m_root).outOfFlowDescendants()) {
        auto& layoutBox = *outOfFlowBox;
        auto& displayBox = layoutContext.createDisplayBox(layoutBox);

        // The term "static position" (of an element) refers, roughly, to the position an element would have had in the normal flow.
        // More precisely, the static position for 'top' is the distance from the top edge of the containing block to the top margin edge
        // of a hypothetical box that would have been the first box of the element if its specified 'position' value had been 'static' and
        // its specified 'float' had been 'none' and its specified 'clear' had been 'none'.
        computeStaticPosition(layoutContext, layoutBox, displayBox);
        computeOutOfFlowWidth(layoutContext, layoutBox, displayBox);

        ASSERT(layoutBox.establishesFormattingContext());
        auto formattingContext = layoutContext.formattingContext(layoutBox);
        formattingContext->layout(layoutContext, layoutContext.establishedFormattingState(layoutBox, *formattingContext));

        computeOutOfFlowHeight(layoutContext, layoutBox, displayBox);
        computeOutOfFlowPosition(layoutContext, layoutBox, displayBox);
    }
}

#ifndef NDEBUG
void FormattingContext::validateGeometryConstraintsAfterLayout(const LayoutContext& layoutContext) const
{
    if (!is<Container>(root()))
        return;
    auto& formattingContextRoot = downcast<Container>(root());
    // FIXME: add a descendantsOfType<> flavor that stops at nested formatting contexts
    for (auto& layoutBox : descendantsOfType<Box>(formattingContextRoot)) {
        if (&layoutBox.formattingContextRoot() != &formattingContextRoot)
            continue;
        auto* containingBlock = layoutBox.containingBlock();
        ASSERT(containingBlock);
        auto containingBlockSize = layoutContext.displayBoxForLayoutBox(*containingBlock)->size();
        auto* displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
        ASSERT(displayBox);

        // 10.3.3 Block-level, non-replaced elements in normal flow
        // 10.3.7 Absolutely positioned, non-replaced elements
        if ((layoutBox.isBlockLevelBox() || layoutBox.isOutOfFlowPositioned()) && !layoutBox.replaced()) {
            // margin-left + border-left-width + padding-left + width + padding-right + border-right-width + margin-right = width of containing block
            ASSERT(displayBox->marginLeft() + displayBox->borderLeft() + displayBox->paddingLeft() + displayBox->width()
                + displayBox->paddingRight() + displayBox->borderRight() + displayBox->marginRight() == containingBlockSize.width());
        }

        // 10.6.4 Absolutely positioned, non-replaced elements
        if (layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced()) {
            // top + margin-top + border-top-width + padding-top + height + padding-bottom + border-bottom-width + margin-bottom + bottom = height of containing block
            ASSERT(displayBox->top() + displayBox->marginTop() + displayBox->borderTop() + displayBox->paddingTop()
                + displayBox->paddingBottom() + displayBox->borderBottom() + displayBox->marginBottom() == containingBlockSize.height());
        }
    }
}
#endif

}
}
#endif

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
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

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

void FormattingContext::computeFloatingHeightAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto heightAndMargin = Geometry::floatingHeightAndMargin(layoutContext, layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.moveVertically(heightAndMargin.margin.top);
    displayBox.setVerticalMargin(heightAndMargin.margin);
}

void FormattingContext::computeFloatingWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto widthAndMargin = Geometry::floatingWidthAndMargin(layoutContext, layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.moveHorizontally(widthAndMargin.margin.left);
    displayBox.setHorizontalMargin(widthAndMargin.margin);
}

void FormattingContext::computeOutOfFlowHorizontalGeometry(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto horizontalGeometry = Geometry::outOfFlowHorizontalGeometry(layoutContext, layoutBox);
    displayBox.setLeft(horizontalGeometry.left);
    displayBox.setContentBoxWidth(horizontalGeometry.widthAndMargin.width);
    displayBox.setHorizontalMargin(horizontalGeometry.widthAndMargin.margin);
}

void FormattingContext::computeOutOfFlowVerticalGeometry(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto verticalGeometry = Geometry::outOfFlowVerticalGeometry(layoutContext, layoutBox);
    displayBox.setTop(verticalGeometry.top);
    displayBox.setContentBoxHeight(verticalGeometry.heightAndMargin.height);
    displayBox.setVerticalMargin(verticalGeometry.heightAndMargin.margin);
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

    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: move in-flow positioned children -> context: " << &layoutContext << " parent: " << &container);
    for (auto& layoutBox : childrenOfType<Box>(container)) {
        if (!layoutBox.isInFlowPositioned())
            continue;
        computeInFlowPositionedPosition(layoutContext, layoutBox, *layoutContext.displayBoxForLayoutBox(layoutBox));
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: move in-flow positioned children -> context: " << &layoutContext << " parent: " << &container);
}

void FormattingContext::layoutOutOfFlowDescendants(LayoutContext& layoutContext) const
{
    if (!is<Container>(m_root.get()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: layout out-of-flow descendants -> context: " << &layoutContext << " root: " << &root());

    for (auto& outOfFlowBox : downcast<Container>(*m_root).outOfFlowDescendants()) {
        auto& layoutBox = *outOfFlowBox;
        auto& displayBox = layoutContext.createDisplayBox(layoutBox);

        // The term "static position" (of an element) refers, roughly, to the position an element would have had in the normal flow.
        // More precisely, the static position for 'top' is the distance from the top edge of the containing block to the top margin edge
        // of a hypothetical box that would have been the first box of the element if its specified 'position' value had been 'static' and
        // its specified 'float' had been 'none' and its specified 'clear' had been 'none'.
        computeStaticPosition(layoutContext, layoutBox, displayBox);
        computeBorderAndPadding(layoutContext, layoutBox, displayBox);
        computeOutOfFlowHorizontalGeometry(layoutContext, layoutBox, displayBox);

        ASSERT(layoutBox.establishesFormattingContext());
        auto formattingContext = layoutContext.formattingContext(layoutBox);
        formattingContext->layout(layoutContext, layoutContext.establishedFormattingState(layoutBox, *formattingContext));

        computeOutOfFlowVerticalGeometry(layoutContext, layoutBox, displayBox);
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: layout out-of-flow descendants -> context: " << &layoutContext << " root: " << &root());
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
        auto& containingBlockDisplayBox = *layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock());
        auto* displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
        ASSERT(displayBox);

        // 10.3.3 Block-level, non-replaced elements in normal flow
        // 10.3.7 Absolutely positioned, non-replaced elements
        if ((layoutBox.isBlockLevelBox() || layoutBox.isOutOfFlowPositioned()) && !layoutBox.replaced()) {
            // margin-left + border-left-width + padding-left + width + padding-right + border-right-width + margin-right = width of containing block
            auto containingBlockWidth = containingBlockDisplayBox.contentBoxWidth();
            ASSERT(displayBox->marginLeft() + displayBox->borderLeft() + displayBox->paddingLeft() + displayBox->contentBoxWidth()
                + displayBox->paddingRight() + displayBox->borderRight() + displayBox->marginRight() == containingBlockWidth);
        }

        // 10.6.4 Absolutely positioned, non-replaced elements
        if (layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced()) {
            // top + margin-top + border-top-width + padding-top + height + padding-bottom + border-bottom-width + margin-bottom + bottom = height of containing block
            auto containingBlockHeight = containingBlockDisplayBox.contentBoxHeight();
            ASSERT(displayBox->top() + displayBox->marginTop() + displayBox->borderTop() + displayBox->paddingTop() + displayBox->contentBoxHeight()
                + displayBox->paddingBottom() + displayBox->borderBottom() + displayBox->marginBottom() == containingBlockHeight);
        }
    }
}
#endif

}
}
#endif

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
#include "FormattingState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutDescendantIterator.h"
#include "LayoutState.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FormattingContext);

FormattingContext::FormattingContext(const Box& formattingContextRoot, FormattingState& formattingState)
    : m_root(makeWeakPtr(formattingContextRoot))
    , m_formattingState(formattingState)
{
#ifndef NDEBUG
    layoutState().registerFormattingContext(*this);
#endif
}

FormattingContext::~FormattingContext()
{
#ifndef NDEBUG
    layoutState().deregisterFormattingContext(*this);
#endif
}

LayoutState& FormattingContext::layoutState() const
{
    return m_formattingState.layoutState();
}

void FormattingContext::computeOutOfFlowHorizontalGeometry(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();
    auto containingBlockWidth = layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock()).paddingBoxWidth();

    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        auto usedValues = UsedHorizontalValues { containingBlockWidth, usedWidth, { } };
        return Geometry::outOfFlowHorizontalGeometry(layoutState, layoutBox, usedValues);
    };

    auto horizontalGeometry = compute({ });
    if (auto maxWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMaxWidth(), containingBlockWidth)) {
        auto maxHorizontalGeometry = compute(maxWidth);
        if (horizontalGeometry.widthAndMargin.width > maxHorizontalGeometry.widthAndMargin.width)
            horizontalGeometry = maxHorizontalGeometry;
    }

    if (auto minWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMinWidth(), containingBlockWidth)) {
        auto minHorizontalGeometry = compute(minWidth);
        if (horizontalGeometry.widthAndMargin.width < minHorizontalGeometry.widthAndMargin.width)
            horizontalGeometry = minHorizontalGeometry;
    }

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setLeft(horizontalGeometry.left + horizontalGeometry.widthAndMargin.usedMargin.start);
    displayBox.setContentBoxWidth(horizontalGeometry.widthAndMargin.width);
    displayBox.setHorizontalMargin(horizontalGeometry.widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(horizontalGeometry.widthAndMargin.computedMargin);
}

void FormattingContext::computeOutOfFlowVerticalGeometry(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    auto compute = [&](UsedVerticalValues usedValues) {
        return Geometry::outOfFlowVerticalGeometry(layoutState, layoutBox, usedValues);
    };

    auto verticalGeometry = compute({ });
    if (auto maxHeight = Geometry::computedMaxHeight(layoutState, layoutBox)) {
        auto maxVerticalGeometry = compute({ *maxHeight });
        if (verticalGeometry.heightAndMargin.height > maxVerticalGeometry.heightAndMargin.height)
            verticalGeometry = maxVerticalGeometry;
    }

    if (auto minHeight = Geometry::computedMinHeight(layoutState, layoutBox)) {
        auto minVerticalGeometry = compute({ *minHeight });
        if (verticalGeometry.heightAndMargin.height < minVerticalGeometry.heightAndMargin.height)
            verticalGeometry = minVerticalGeometry;
    }

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    auto nonCollapsedVerticalMargin = verticalGeometry.heightAndMargin.nonCollapsedMargin;
    displayBox.setTop(verticalGeometry.top + nonCollapsedVerticalMargin.before);
    displayBox.setContentBoxHeight(verticalGeometry.heightAndMargin.height);
    // Margins of absolutely positioned boxes do not collapse
    displayBox.setVerticalMargin({ nonCollapsedVerticalMargin, { } });
}

void FormattingContext::computeBorderAndPadding(const Box& layoutBox, Optional<UsedHorizontalValues> usedValues) const
{
    auto& layoutState = this->layoutState();
    if (!usedValues)
        usedValues = UsedHorizontalValues { layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth() };
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setBorder(Geometry::computedBorder(layoutBox));
    displayBox.setPadding(Geometry::computedPadding(layoutBox, *usedValues));
}

void FormattingContext::layoutOutOfFlowContent() const
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: layout out-of-flow content -> context: " << &layoutState() << " root: " << &root());

    for (auto& outOfFlowBox : formattingState().outOfFlowBoxes()) {
        ASSERT(outOfFlowBox->establishesFormattingContext());

        computeBorderAndPadding(*outOfFlowBox);
        computeOutOfFlowHorizontalGeometry(*outOfFlowBox);

        auto formattingContext = layoutState().createFormattingContext(*outOfFlowBox);
        formattingContext->layout();

        computeOutOfFlowVerticalGeometry(*outOfFlowBox);
        formattingContext->layoutOutOfFlowContent();
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: layout out-of-flow content -> context: " << &layoutState() << " root: " << &root());
}

static LayoutUnit mapHorizontalPositionToAncestor(const LayoutState& layoutState, LayoutUnit horizontalPosition, const Container& containingBlock, const Container& ancestor)
{
    // "horizontalPosition" is in the coordinate system of the "containingBlock". -> map from containingBlock to ancestor.
    if (&containingBlock == &ancestor)
        return horizontalPosition;
    ASSERT(containingBlock.isContainingBlockDescendantOf(ancestor));
    for (auto* container = &containingBlock; container && container != &ancestor; container = container->containingBlock())
        horizontalPosition += layoutState.displayBoxForLayoutBox(*container).left();
    return horizontalPosition;
}

// FIXME: turn these into templates.
LayoutUnit FormattingContext::mapLeftToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.containingBlock());
    return mapHorizontalPositionToAncestor(layoutState, layoutState.displayBoxForLayoutBox(layoutBox).left(), *layoutBox.containingBlock(), ancestor);
}

LayoutUnit FormattingContext::mapRightToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.containingBlock());
    return mapHorizontalPositionToAncestor(layoutState, layoutState.displayBoxForLayoutBox(layoutBox).right(), *layoutBox.containingBlock(), ancestor);
}

Display::Box FormattingContext::mapBoxToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.isContainingBlockDescendantOf(ancestor));
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    auto topLeft = displayBox.topLeft();
    for (auto* containingBlock = layoutBox.containingBlock(); containingBlock && containingBlock != &ancestor; containingBlock = containingBlock->containingBlock())
        topLeft.moveBy(layoutState.displayBoxForLayoutBox(*containingBlock).topLeft());

    auto mappedDisplayBox = Display::Box(displayBox);
    mappedDisplayBox.setTopLeft(topLeft);
    return mappedDisplayBox;
}

LayoutUnit FormattingContext::mapTopToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.isContainingBlockDescendantOf(ancestor));
    auto top = layoutState.displayBoxForLayoutBox(layoutBox).top();
    for (auto* container = layoutBox.containingBlock(); container && container != &ancestor; container = container->containingBlock())
        top += layoutState.displayBoxForLayoutBox(*container).top();
    return top;
}

Point FormattingContext::mapPointToAncestor(const LayoutState& layoutState, Point position, const Container& from, const Container& to)
{
    if (&from == &to)
        return position;
    ASSERT(from.isContainingBlockDescendantOf(to));
    auto mappedPosition = position;
    for (auto* container = &from; container && container != &to; container = container->containingBlock())
        mappedPosition.moveBy(layoutState.displayBoxForLayoutBox(*container).topLeft());
    return mappedPosition;
}

Point FormattingContext::mapPointToDescendent(const LayoutState& layoutState, Point point, const Container& from, const Container& to)
{
    // "point" is in the coordinate system of the "from" container.
    if (&from == &to)
        return point;
    ASSERT(to.isContainingBlockDescendantOf(from));
    for (auto* container = &to; container && container != &from; container = container->containingBlock())
        point.moveBy(-layoutState.displayBoxForLayoutBox(*container).topLeft());
    return point;
}

#ifndef NDEBUG
void FormattingContext::validateGeometryConstraintsAfterLayout() const
{
    if (!is<Container>(root()))
        return;
    auto& formattingContextRoot = downcast<Container>(root());
    auto& layoutState = this->layoutState();
    // FIXME: add a descendantsOfType<> flavor that stops at nested formatting contexts
    for (auto& layoutBox : descendantsOfType<Box>(formattingContextRoot)) {
        if (&layoutBox.formattingContextRoot() != &formattingContextRoot)
            continue;
        auto& containingBlockDisplayBox = layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock());
        auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

        // 10.3.3 Block-level, non-replaced elements in normal flow
        // 10.3.7 Absolutely positioned, non-replaced elements
        if ((layoutBox.isBlockLevelBox() || layoutBox.isOutOfFlowPositioned()) && !layoutBox.replaced()) {
            // margin-left + border-left-width + padding-left + width + padding-right + border-right-width + margin-right = width of containing block
            auto containingBlockWidth = containingBlockDisplayBox.contentBoxWidth();
            ASSERT(displayBox.horizontalMarginBorderAndPadding() + displayBox.contentBoxWidth() == containingBlockWidth);
        }

        // 10.6.4 Absolutely positioned, non-replaced elements
        if (layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced()) {
            // top + margin-top + border-top-width + padding-top + height + padding-bottom + border-bottom-width + margin-bottom + bottom = height of containing block
            auto containingBlockHeight = containingBlockDisplayBox.contentBoxHeight();
            ASSERT(displayBox.top() + displayBox.marginBefore() + displayBox.borderTop() + displayBox.paddingTop().valueOr(0) + displayBox.contentBoxHeight()
                + displayBox.paddingBottom().valueOr(0) + displayBox.borderBottom() + displayBox.marginAfter() == containingBlockHeight);
        }
    }
}
#endif

}
}
#endif

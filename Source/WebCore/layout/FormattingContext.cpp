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
}

FormattingContext::~FormattingContext()
{
}

FormattingState& FormattingContext::formattingState() const
{
    return m_formattingState;
}

LayoutState& FormattingContext::layoutState() const
{
    return m_formattingState.layoutState();
}

void FormattingContext::computeOutOfFlowHorizontalGeometry(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        return Geometry::outOfFlowHorizontalGeometry(layoutState, layoutBox, usedWidth);
    };

    auto horizontalGeometry = compute({ });
    auto containingBlockWidth = layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth();

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
    displayBox.setLeft(horizontalGeometry.left + horizontalGeometry.widthAndMargin.margin.start);
    displayBox.setContentBoxWidth(horizontalGeometry.widthAndMargin.width);
    displayBox.setHorizontalMargin(horizontalGeometry.widthAndMargin.margin);
    displayBox.setHorizontalNonComputedMargin(horizontalGeometry.widthAndMargin.nonComputedMargin);
}

void FormattingContext::computeOutOfFlowVerticalGeometry(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    auto compute = [&](Optional<LayoutUnit> usedHeight) {
        return Geometry::outOfFlowVerticalGeometry(layoutState, layoutBox, usedHeight);
    };

    auto verticalGeometry = compute({ });
    if (auto maxHeight = Geometry::computedMaxHeight(layoutState, layoutBox)) {
        auto maxVerticalGeometry = compute(maxHeight);
        if (verticalGeometry.heightAndMargin.height > maxVerticalGeometry.heightAndMargin.height)
            verticalGeometry = maxVerticalGeometry;
    }

    if (auto minHeight = Geometry::computedMinHeight(layoutState, layoutBox)) {
        auto minVerticalGeometry = compute(minHeight);
        if (verticalGeometry.heightAndMargin.height < minVerticalGeometry.heightAndMargin.height)
            verticalGeometry = minVerticalGeometry;
    }

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    // Margins of absolutely positioned boxes do not collapse
    ASSERT(!verticalGeometry.heightAndMargin.margin.collapsedValues());
    auto nonCollapsedVerticalMargin = verticalGeometry.heightAndMargin.margin.nonCollapsedValues();
    displayBox.setTop(verticalGeometry.top + nonCollapsedVerticalMargin.before);
    displayBox.setContentBoxHeight(verticalGeometry.heightAndMargin.height);
    displayBox.setVerticalMargin(verticalGeometry.heightAndMargin.margin);
}

void FormattingContext::computeBorderAndPadding(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setBorder(Geometry::computedBorder(layoutState, layoutBox));
    displayBox.setPadding(Geometry::computedPadding(layoutState, layoutBox));
}

void FormattingContext::layoutOutOfFlowDescendants(const Box& layoutBox) const
{
    // Initial containing block by definition is a containing block.
    if (!layoutBox.isPositioned() && !layoutBox.isInitialContainingBlock())
        return;

    if (!is<Container>(layoutBox))
        return;

    auto& container = downcast<Container>(layoutBox);
    if (!container.hasChild())
        return;

    auto& layoutState = this->layoutState();
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: layout out-of-flow descendants -> context: " << &layoutState << " root: " << &root());

    for (auto& outOfFlowBox : container.outOfFlowDescendants()) {
        auto& layoutBox = *outOfFlowBox;

        ASSERT(layoutBox.establishesFormattingContext());

        computeBorderAndPadding(layoutBox);
        computeOutOfFlowHorizontalGeometry(layoutBox);

        layoutState.createFormattingStateForFormattingRootIfNeeded(layoutBox).createFormattingContext(layoutBox)->layout();

        computeOutOfFlowVerticalGeometry(layoutBox);
        layoutOutOfFlowDescendants(layoutBox);
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: layout out-of-flow descendants -> context: " << &layoutState << " root: " << &root());
}

Display::Box FormattingContext::mapBoxToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.isDescendantOf(ancestor));

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    auto topLeft = displayBox.topLeft();

    auto* containingBlock = layoutBox.containingBlock();
    for (; containingBlock && containingBlock != &ancestor; containingBlock = containingBlock->containingBlock())
        topLeft.moveBy(layoutState.displayBoxForLayoutBox(*containingBlock).topLeft());

    if (!containingBlock) {
        ASSERT_NOT_REACHED();
        return Display::Box(displayBox);
    }

    auto mappedDisplayBox = Display::Box(displayBox);
    mappedDisplayBox.setTopLeft(topLeft);
    return mappedDisplayBox;
}

Point FormattingContext::mapTopLeftToAncestor(const LayoutState& layoutState, const Box& layoutBox, const Container& ancestor)
{
    ASSERT(layoutBox.isDescendantOf(ancestor));
    return mapCoordinateToAncestor(layoutState, layoutState.displayBoxForLayoutBox(layoutBox).topLeft(), *layoutBox.containingBlock(), ancestor);
}

Point FormattingContext::mapCoordinateToAncestor(const LayoutState& layoutState, Point position, const Container& containingBlock, const Container& ancestor)
{
    auto mappedPosition = position;
    auto* container = &containingBlock;
    for (; container && container != &ancestor; container = container->containingBlock())
        mappedPosition.moveBy(layoutState.displayBoxForLayoutBox(*container).topLeft());

    if (!container) {
        ASSERT_NOT_REACHED();
        return position;
    }

    return mappedPosition;
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
            ASSERT(displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0) + displayBox.contentBoxWidth()
                + displayBox.paddingRight().valueOr(0) + displayBox.borderRight() + displayBox.marginEnd() == containingBlockWidth);
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

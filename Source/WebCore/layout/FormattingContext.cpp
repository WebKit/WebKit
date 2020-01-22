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
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutDescendantIterator.h"
#include "LayoutState.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FormattingContext);

FormattingContext::FormattingContext(const Container& formattingContextRoot, FormattingState& formattingState)
    : m_root(makeWeakPtr(formattingContextRoot))
    , m_formattingState(formattingState)
{
    ASSERT(formattingContextRoot.hasChild());
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

void FormattingContext::computeOutOfFlowHorizontalGeometry(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    ASSERT(layoutBox.isOutOfFlowPositioned());
    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        return geometry().outOfFlowHorizontalGeometry(layoutBox, horizontalConstraints, { usedWidth, { } });
    };

    auto containingBlockWidth = horizontalConstraints.logicalWidth;
    auto horizontalGeometry = compute({ });
    if (auto maxWidth = geometry().computedMaxWidth(layoutBox, containingBlockWidth)) {
        auto maxHorizontalGeometry = compute(maxWidth);
        if (horizontalGeometry.contentWidthAndMargin.contentWidth > maxHorizontalGeometry.contentWidthAndMargin.contentWidth)
            horizontalGeometry = maxHorizontalGeometry;
    }

    if (auto minWidth = geometry().computedMinWidth(layoutBox, containingBlockWidth)) {
        auto minHorizontalGeometry = compute(minWidth);
        if (horizontalGeometry.contentWidthAndMargin.contentWidth < minHorizontalGeometry.contentWidthAndMargin.contentWidth)
            horizontalGeometry = minHorizontalGeometry;
    }

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setLeft(horizontalGeometry.left + horizontalGeometry.contentWidthAndMargin.usedMargin.start);
    displayBox.setContentBoxWidth(horizontalGeometry.contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin(horizontalGeometry.contentWidthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(horizontalGeometry.contentWidthAndMargin.computedMargin);
}

void FormattingContext::computeOutOfFlowVerticalGeometry(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints)
{
    ASSERT(layoutBox.isOutOfFlowPositioned());
    auto compute = [&](Optional<LayoutUnit> usedHeight) {
        return geometry().outOfFlowVerticalGeometry(layoutBox, horizontalConstraints, verticalConstraints, { usedHeight });
    };

    auto containingBlockHeight = *verticalConstraints.logicalHeight;
    auto verticalGeometry = compute({ });
    if (auto maxHeight = geometry().computedMaxHeight(layoutBox, containingBlockHeight)) {
        auto maxVerticalGeometry = compute(maxHeight);
        if (verticalGeometry.contentHeightAndMargin.contentHeight > maxVerticalGeometry.contentHeightAndMargin.contentHeight)
            verticalGeometry = maxVerticalGeometry;
    }

    if (auto minHeight = geometry().computedMinHeight(layoutBox, containingBlockHeight)) {
        auto minVerticalGeometry = compute(minHeight);
        if (verticalGeometry.contentHeightAndMargin.contentHeight < minVerticalGeometry.contentHeightAndMargin.contentHeight)
            verticalGeometry = minVerticalGeometry;
    }

    auto& displayBox = formattingState().displayBox(layoutBox);
    auto nonCollapsedVerticalMargin = verticalGeometry.contentHeightAndMargin.nonCollapsedMargin;
    displayBox.setTop(verticalGeometry.top + nonCollapsedVerticalMargin.before);
    displayBox.setContentBoxHeight(verticalGeometry.contentHeightAndMargin.contentHeight);
    // Margins of absolutely positioned boxes do not collapse
    displayBox.setVerticalMargin({ nonCollapsedVerticalMargin, { } });
}

void FormattingContext::computeBorderAndPadding(const Box& layoutBox, const HorizontalConstraints& horizontalConstraint)
{
    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setBorder(geometry().computedBorder(layoutBox));
    displayBox.setPadding(geometry().computedPadding(layoutBox, horizontalConstraint));
}

void FormattingContext::layoutOutOfFlowContent(InvalidationState& invalidationState, const HorizontalConstraints& rootHorizontalConstraints, const VerticalConstraints& rootVerticalConstraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: layout out-of-flow content -> context: " << &layoutState() << " root: " << &root());

    collectOutOfFlowDescendantsIfNeeded();

    auto horizontalConstraintsForOutOfFlowBox = [&] (const auto& outOfFlowBox) {
        auto* containingBlock = outOfFlowBox.containingBlock();
        ASSERT(containingBlock);
        if (containingBlock == &root())
            return rootHorizontalConstraints;
        return Geometry::horizontalConstraintsForOutOfFlow(geometryForBox(*containingBlock));
    };

    auto verticalConstraintsForOutOfFlowBox = [&] (const auto& outOfFlowBox) {
        auto* containingBlock = outOfFlowBox.containingBlock();
        ASSERT(containingBlock);
        if (containingBlock == &root())
            return rootVerticalConstraints;
        return Geometry::verticalConstraintsForOutOfFlow(geometryForBox(*containingBlock));
    };

    for (auto& outOfFlowBox : formattingState().outOfFlowBoxes()) {
        ASSERT(outOfFlowBox->establishesFormattingContext());
        if (!invalidationState.needsLayout(*outOfFlowBox))
            continue;

        auto horizontalConstraints = horizontalConstraintsForOutOfFlowBox(*outOfFlowBox);
        auto verticalConstraints = verticalConstraintsForOutOfFlowBox(*outOfFlowBox);
        // Borders and paddings are resolved against the containing block's content box like if this box was an in-flow box.
        auto& outOfFlowRootDisplayBox = geometryForBox(*outOfFlowBox);
        computeBorderAndPadding(*outOfFlowBox, Geometry::horizontalConstraintsForInFlow(outOfFlowRootDisplayBox));
        computeOutOfFlowHorizontalGeometry(*outOfFlowBox, horizontalConstraints);
        if (!is<Container>(*outOfFlowBox) || !downcast<Container>(*outOfFlowBox).hasChild()) {
            computeOutOfFlowVerticalGeometry(*outOfFlowBox, horizontalConstraints, verticalConstraints);
            continue;
        }

        auto& outOfFlowRootContainer = downcast<Container>(*outOfFlowBox);
        auto formattingContext = LayoutContext::createFormattingContext(outOfFlowRootContainer, layoutState());
        if (outOfFlowRootContainer.hasInFlowOrFloatingChild())
            formattingContext->layoutInFlowContent(invalidationState, Geometry::horizontalConstraintsForInFlow(outOfFlowRootDisplayBox), Geometry::verticalConstraintsForInFlow(outOfFlowRootDisplayBox));
        computeOutOfFlowVerticalGeometry(outOfFlowRootContainer, horizontalConstraints, verticalConstraints);
        formattingContext->layoutOutOfFlowContent(invalidationState, Geometry::horizontalConstraintsForInFlow(outOfFlowRootDisplayBox), Geometry::verticalConstraintsForInFlow(outOfFlowRootDisplayBox));
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: layout out-of-flow content -> context: " << &layoutState() << " root: " << &root());
}

static LayoutUnit mapHorizontalPositionToAncestor(const FormattingContext& formattingContext, LayoutUnit horizontalPosition, const Container& containingBlock, const Container& ancestor)
{
    // "horizontalPosition" is in the coordinate system of the "containingBlock". -> map from containingBlock to ancestor.
    if (&containingBlock == &ancestor)
        return horizontalPosition;
    ASSERT(containingBlock.isContainingBlockDescendantOf(ancestor));
    for (auto* container = &containingBlock; container && container != &ancestor; container = container->containingBlock())
        horizontalPosition += formattingContext.geometryForBox(*container).left();
    return horizontalPosition;
}

// FIXME: turn these into templates.
LayoutUnit FormattingContext::mapTopToFormattingContextRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.containingBlock());
    auto& formattingContextRoot = root();
    ASSERT(layoutBox.isContainingBlockDescendantOf(formattingContextRoot));
    auto top = geometryForBox(layoutBox).top();
    for (auto* container = layoutBox.containingBlock(); container && container != &formattingContextRoot; container = container->containingBlock())
        top += geometryForBox(*container).top();
    return top;
}

LayoutUnit FormattingContext::mapLeftToFormattingContextRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.containingBlock());
    return mapHorizontalPositionToAncestor(*this, geometryForBox(layoutBox).left(), *layoutBox.containingBlock(), root());
}

LayoutUnit FormattingContext::mapRightToFormattingContextRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.containingBlock());
    return mapHorizontalPositionToAncestor(*this, geometryForBox(layoutBox).right(), *layoutBox.containingBlock(), root());
}

const Display::Box& FormattingContext::geometryForBox(const Box& layoutBox, Optional<EscapeReason> escapeReason) const
{
    UNUSED_PARAM(escapeReason);
#if ASSERT_ENABLED
    auto isOkToAccessDisplayBox = [&] {
        if (!layoutBox.isInitialContainingBlock() && &layoutBox.formattingContextRoot() == &root()) {
            // This is the non-escape case of accessing a box's geometry information within the same formatting context.
            return true;
        }

        if (!escapeReason) {
            // Any geometry access outside of the formatting context without a valid reason is considered an escape.
            return false;
        }

        if (*escapeReason == EscapeReason::BodyStrechesToViewportQuirk) {
            ASSERT(layoutState().inQuirksMode());
            return layoutBox.isInitialContainingBlock();
        }

        if (*escapeReason == EscapeReason::StrokeOverflowNeedsViewportGeometry)
            return layoutBox.isInitialContainingBlock();

        if (*escapeReason == EscapeReason::NeedsGeometryFromEstablishedFormattingContext) {
            // This is the case when a formatting root collects geometry information from the established
            // formatting context to be able to determine width/height.
            // e.g <div>text content</div>. The <div> is a formatting root of the IFC.
            // In order to compute the height of the <div>, we need to look inside the IFC and gather geometry information.
            return &layoutBox.formattingContextRoot().formattingContextRoot() == &root();
        }

        if (*escapeReason == EscapeReason::OutOfFlowBoxNeedsInFlowGeometry) {
            // When computing the static position of an out-of-flow box, we need to gather sibling/parent geometry information
            // as if the out-of-flow box was a simple inflow box.
            // Now since the out-of-flow and the sibling/parent boxes could very well be in different containing block subtrees
            // the formatting context they live in could also be very different.
            return true;
        }

        if (*escapeReason == EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates) {
            // Float box top/left values are mapped relative to the FloatState's root. Inline formatting contexts(A) inherit floats from parent
            // block formatting contexts(B). Floats in these inline formatting contexts(A) need to be mapped to the parent, block formatting context(B).
            auto& formattingContextRoot = layoutBox.formattingContextRoot();
            return &formattingContextRoot == &root() || &formattingContextRoot == &root().formattingContextRoot();
        }

        if (*escapeReason == EscapeReason::FindFixedHeightAncestorQuirk) {
            ASSERT(layoutState().inQuirksMode());
            // Find the first containing block with fixed height quirk. See Quirks::heightValueOfNearestContainingBlockWithFixedHeight
            auto& targetFormattingRoot = layoutBox.formattingContextRoot();
            auto* ancestorFormattingContextRoot = &root().formattingContextRoot();
            while (true) {
                if (&targetFormattingRoot == ancestorFormattingContextRoot)
                    return true;
                if (ancestorFormattingContextRoot->isInitialContainingBlock())
                    return false;
                ancestorFormattingContextRoot = &ancestorFormattingContextRoot->formattingContextRoot();
            }
            return false;
        }

        if (*escapeReason == EscapeReason::TableNeedsAccessToTableWrapper) {
            // Tables are wrapped in a 2 level formatting context structure. A <table> element initiates a block formatting context for its principal table box
            // where the caption and the table content live. It also initiates a table wrapper box which establishes the table formatting context.
            // In many cases the TFC needs access to the parent (generated) BFC.
            return &layoutBox == &root().formattingContextRoot();
        }

        ASSERT_NOT_REACHED();
        return false;
    };
#endif
    ASSERT(isOkToAccessDisplayBox());
    ASSERT(layoutState().hasDisplayBox(layoutBox));
    return layoutState().displayBoxForLayoutBox(layoutBox);
}

void FormattingContext::collectOutOfFlowDescendantsIfNeeded()
{
    if (!formattingState().outOfFlowBoxes().isEmpty())
        return;
    auto& root = this->root();
    if (!root.hasChild())
        return;
    if (!root.isPositioned() && !root.isInitialContainingBlock())
        return;
    // Collect the out-of-flow descendants at the formatting root level (as opposed to at the containing block level, though they might be the same).
    // FIXME: Turn this into a register-self as boxes are being inserted.
    for (auto& descendant : descendantsOfType<Box>(root)) {
        if (!descendant.isOutOfFlowPositioned())
            continue;
        if (&descendant.formattingContextRoot() != &root)
            continue;
        formattingState().addOutOfFlowBox(descendant);
    }
}

#ifndef NDEBUG
void FormattingContext::validateGeometryConstraintsAfterLayout() const
{
    auto& formattingContextRoot = root();
    // FIXME: add a descendantsOfType<> flavor that stops at nested formatting contexts
    for (auto& layoutBox : descendantsOfType<Box>(formattingContextRoot)) {
        if (&layoutBox.formattingContextRoot() != &formattingContextRoot)
            continue;
        auto& containingBlockGeometry = geometryForBox(*layoutBox.containingBlock());
        auto& boxGeometry = geometryForBox(layoutBox);

        // 10.3.3 Block-level, non-replaced elements in normal flow
        // 10.3.7 Absolutely positioned, non-replaced elements
        if ((layoutBox.isBlockLevelBox() || layoutBox.isOutOfFlowPositioned()) && !layoutBox.replaced()) {
            // margin-left + border-left-width + padding-left + width + padding-right + border-right-width + margin-right = width of containing block
            auto containingBlockWidth = containingBlockGeometry.contentBoxWidth();
            ASSERT(boxGeometry.horizontalMarginBorderAndPadding() + boxGeometry.contentBoxWidth() == containingBlockWidth);
        }

        // 10.6.4 Absolutely positioned, non-replaced elements
        if (layoutBox.isOutOfFlowPositioned() && !layoutBox.replaced()) {
            // top + margin-top + border-top-width + padding-top + height + padding-bottom + border-bottom-width + margin-bottom + bottom = height of containing block
            auto containingBlockHeight = containingBlockGeometry.contentBoxHeight();
            ASSERT(boxGeometry.top() + boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0) + boxGeometry.contentBoxHeight()
                + boxGeometry.paddingBottom().valueOr(0) + boxGeometry.borderBottom() + boxGeometry.marginAfter() == containingBlockHeight);
        }
    }
}
#endif

}
}
#endif

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

#include "BlockFormattingState.h"
#include "DisplayBox.h"
#include "FloatingContext.h"
#include "FloatingState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const Box& formattingContextRoot)
    : FormattingContext(formattingContextRoot)
{
}

void BlockFormattingContext::layout(LayoutContext& layoutContext, FormattingState& formattingState) const
{
    // 9.4.1 Block formatting contexts
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> block formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");

    auto& formattingRoot = downcast<Container>(root());
    LayoutQueue layoutQueue;
    FloatingContext floatingContext(formattingState.floatingState());
    // This is a post-order tree traversal layout.
    // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
    if (auto* firstChild = formattingRoot.firstInFlowOrFloatingChild())
        layoutQueue.append(firstChild);
    // 1. Go all the way down to the leaf node
    // 2. Compute static position and width as we traverse down
    // 3. As we climb back on the tree, compute height and finialize position
    // (Any subtrees with new formatting contexts need to layout synchronously)
    while (!layoutQueue.isEmpty()) {
        // Traverse down on the descendants and compute width/static position until we find a leaf node.
        while (true) {
            auto& layoutBox = *layoutQueue.last();

            if (layoutBox.establishesFormattingContext()) {
                layoutFormattingContextRoot(layoutContext, floatingContext, formattingState, layoutBox);
                layoutQueue.removeLast();
                // Since this box is a formatting context root, it takes care of its entire subtree.
                // Continue with next sibling if exists.
                if (!layoutBox.nextInFlowOrFloatingSibling())
                    break;
                layoutQueue.append(layoutBox.nextInFlowOrFloatingSibling());
                continue;
            }

            LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
            computeStaticPosition(layoutContext, layoutBox);
            computeBorderAndPadding(layoutContext, layoutBox);
            computeWidthAndMargin(layoutContext, layoutBox);
            if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(downcast<Container>(layoutBox).firstInFlowOrFloatingChild());
        }

        // Climb back on the ancestors and compute height/final position.
        while (!layoutQueue.isEmpty()) {
            // All inflow descendants (if there are any) are laid out by now. Let's compute the box's height.
            auto& layoutBox = *layoutQueue.takeLast();

            LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Height][Margin] -> for layoutBox(" << &layoutBox << ")");
            // Formatting root boxes are special-cased and they don't come here.
            ASSERT(!layoutBox.establishesFormattingContext());
            computeHeightAndMargin(layoutContext, layoutBox);
            // Finalize position with clearance.
            if (layoutBox.hasFloatClear())
                computeVerticalPositionForFloatClear(layoutContext, floatingContext, layoutBox);
            if (!is<Container>(layoutBox))
                continue;
            auto& container = downcast<Container>(layoutBox);
            // Move in-flow positioned children to their final position.
            placeInFlowPositionedChildren(layoutContext, container);
            if (auto* nextSibling = container.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
    // Place the inflow positioned children.
    placeInFlowPositionedChildren(layoutContext, formattingRoot);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> block formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");
}

void BlockFormattingContext::layoutFormattingContextRoot(LayoutContext& layoutContext, FloatingContext& floatingContext, FormattingState&, const Box& layoutBox) const
{
    // Start laying out this formatting root in the formatting contenxt it lives in.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeStaticPosition(layoutContext, layoutBox);
    computeBorderAndPadding(layoutContext, layoutBox);
    computeWidthAndMargin(layoutContext, layoutBox);

    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutContext.formattingContext(layoutBox);
    auto& formattingState = layoutContext.createFormattingStateForFormattingRootIfNeeded(layoutBox);
    formattingContext->layout(layoutContext, formattingState);

    // Come back and finalize the root's geometry.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Height][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeHeightAndMargin(layoutContext, layoutBox);

    // Float related final positioning.
    if (layoutBox.isFloatingPositioned()) {
        computeFloatingPosition(layoutContext, floatingContext, layoutBox);
        floatingContext.floatingState().append(layoutBox);
    } else if (layoutBox.hasFloatClear())
        computeVerticalPositionForFloatClear(layoutContext, floatingContext, layoutBox);
    else if (layoutBox.establishesBlockFormattingContext())
        computePositionToAvoidFloats(layoutContext, floatingContext, layoutBox);

    // Now that we computed the root's height, we can go back and layout the out-of-flow descedants (if any).
    formattingContext->layoutOutOfFlowDescendants(layoutContext, layoutBox);
}

void BlockFormattingContext::computeStaticPosition(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    layoutContext.displayBoxForLayoutBox(layoutBox).setTopLeft(Geometry::staticPosition(layoutContext, layoutBox));
}

void BlockFormattingContext::computeEstimatedMarginTop(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    auto estimatedMarginTop = Geometry::estimatedMarginTop(layoutContext, layoutBox);

    auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
    displayBox.setEstimatedMarginTop(estimatedMarginTop);
    displayBox.moveVertically(estimatedMarginTop);
}

void BlockFormattingContext::computeEstimatedMarginTopForAncestors(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    // We only need to estimate margin top for float related layout (formatting context roots avoid floats).
    ASSERT(layoutBox.isFloatingPositioned() || layoutBox.hasFloatClear() || layoutBox.establishesBlockFormattingContext());

    // In order to figure out whether a box should avoid a float, we need to know the final positions of both (ignore relative positioning for now).
    // In block formatting context the final position for a normal flow box includes
    // 1. the static position and
    // 2. the corresponding (non)collapsed margins.
    // Now the vertical margins are computed when all the descendants are finalized, because the margin values might be depending on the height of the box
    // (and the height might be based on the content).
    // So when we get to the point where we intersect the box with the float to decide if the box needs to move, we don't yet have the final vertical position.
    //
    // The idea here is that as long as we don't cross the block formatting context boundary, we should be able to pre-compute the final top margin.

    for (auto* ancestor = layoutBox.containingBlock(); ancestor && !ancestor->establishesBlockFormattingContext(); ancestor = ancestor->containingBlock()) {
        auto& displayBox = layoutContext.displayBoxForLayoutBox(*ancestor);
        // FIXME: with incremental layout, we might actually have a valid (non-estimated) margin top as well.
        if (displayBox.estimatedMarginTop())
            return;

        computeEstimatedMarginTop(layoutContext, *ancestor);
    }
}

void BlockFormattingContext::computeFloatingPosition(const LayoutContext& layoutContext, const FloatingContext& floatingContext, const Box& layoutBox) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
    // 8.3.1 Collapsing margins
    // In block formatting context margins between a floated box and any other box do not collapse.
    // Adjust the static position by using the previous inflow box's non-collapsed margin.
    if (auto* previousInFlowBox = layoutBox.previousInFlowSibling()) {
        auto& previousDisplayBox = layoutContext.displayBoxForLayoutBox(*previousInFlowBox);
        displayBox.moveVertically(previousDisplayBox.nonCollapsedMarginBottom() - previousDisplayBox.marginBottom());
    }
    computeEstimatedMarginTopForAncestors(layoutContext, layoutBox);
    displayBox.setTopLeft(floatingContext.positionForFloat(layoutBox));
}

void BlockFormattingContext::computePositionToAvoidFloats(const LayoutContext& layoutContext, const FloatingContext& floatingContext, const Box& layoutBox) const
{
    // Formatting context roots avoid floats.
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());

    if (floatingContext.floatingState().isEmpty())
        return;

    computeEstimatedMarginTopForAncestors(layoutContext, layoutBox);
    if (auto adjustedPosition = floatingContext.positionForFloatAvoiding(layoutBox))
        layoutContext.displayBoxForLayoutBox(layoutBox).setTopLeft(*adjustedPosition);
}

void BlockFormattingContext::computeVerticalPositionForFloatClear(const LayoutContext& layoutContext, const FloatingContext& floatingContext, const Box& layoutBox) const
{
    ASSERT(layoutBox.hasFloatClear());
    if (floatingContext.floatingState().isEmpty())
        return;

    computeEstimatedMarginTopForAncestors(layoutContext, layoutBox);
    if (auto verticalPositionWithClearance = floatingContext.verticalPositionWithClearance(layoutBox))
        layoutContext.displayBoxForLayoutBox(layoutBox).setTop(*verticalPositionWithClearance);
}

void BlockFormattingContext::computeInFlowPositionedPosition(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    layoutContext.displayBoxForLayoutBox(layoutBox).setTopLeft(Geometry::inFlowPositionedPosition(layoutContext, layoutBox));
}

void BlockFormattingContext::computeWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox) const
{
    auto compute = [&](std::optional<LayoutUnit> usedWidth) -> WidthAndMargin {

        if (layoutBox.isInFlow())
            return Geometry::inFlowWidthAndMargin(layoutContext, layoutBox, usedWidth);

        if (layoutBox.isFloatingPositioned())
            return Geometry::floatingWidthAndMargin(layoutContext, *this, layoutBox, usedWidth);

        ASSERT_NOT_REACHED();
        return { };
    };

    auto widthAndMargin = compute({ });
    auto containingBlockWidth = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth();

    if (auto maxWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMaxWidth(), containingBlockWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (widthAndMargin.width > maxWidthAndMargin.width)
            widthAndMargin = maxWidthAndMargin;
    }

    if (auto minWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMinWidth(), containingBlockWidth)) {
        auto minWidthAndMargin = compute(minWidth);
        if (widthAndMargin.width < minWidthAndMargin.width)
            widthAndMargin = minWidthAndMargin;
    }

    auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.moveHorizontally(widthAndMargin.margin.left);
    displayBox.setHorizontalMargin(widthAndMargin.margin);
    displayBox.setHorizontalNonComputedMargin(widthAndMargin.nonComputedMargin);
}

void BlockFormattingContext::computeHeightAndMargin(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    auto compute = [&](std::optional<LayoutUnit> usedHeight) -> HeightAndMargin {

        if (layoutBox.isInFlow())
            return Geometry::inFlowHeightAndMargin(layoutContext, layoutBox, usedHeight);

        if (layoutBox.isFloatingPositioned())
            return Geometry::floatingHeightAndMargin(layoutContext, layoutBox, usedHeight);

        ASSERT_NOT_REACHED();
        return { };
    };

    auto heightAndMargin = compute({ });
    if (auto maxHeight = Geometry::computedMaxHeight(layoutContext, layoutBox)) {
        auto maxHeightAndMargin = compute(maxHeight);
        if (heightAndMargin.height > maxHeightAndMargin.height)
            heightAndMargin = maxHeightAndMargin;
    }

    if (auto minHeight = Geometry::computedMinHeight(layoutContext, layoutBox)) {
        auto minHeightAndMargin = compute(minHeight);
        if (heightAndMargin.height < minHeightAndMargin.height)
            heightAndMargin = minHeightAndMargin;
    }

    auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalNonCollapsedMargin(heightAndMargin.margin);
    displayBox.setVerticalMargin(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin));

    // If this box has already been moved by the estimated vertical margin, no need to move it again.
    if (layoutBox.isFloatingPositioned() || !displayBox.estimatedMarginTop())
        displayBox.moveVertically(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin).top);
}

FormattingContext::InstrinsicWidthConstraints BlockFormattingContext::instrinsicWidthConstraints(LayoutContext& layoutContext, const Box& layoutBox) const
{
    auto& formattingState = layoutContext.formattingStateForBox(layoutBox);
    ASSERT(formattingState.isBlockFormattingState());
    if (auto instrinsicWidthConstraints = formattingState.instrinsicWidthConstraints(layoutBox))
        return *instrinsicWidthConstraints;

    // Can we just compute them without checking the children?
    if (!Geometry::instrinsicWidthConstraintsNeedChildrenWidth(layoutBox)) {
        auto instrinsicWidthConstraints = Geometry::instrinsicWidthConstraints(layoutContext, layoutBox);
        formattingState.setInstrinsicWidthConstraints(layoutBox, instrinsicWidthConstraints);
        return instrinsicWidthConstraints;
    }

    // Visit the in-flow descendants and compute their min/max intrinsic width if needed.
    // 1. Go all the way down to the leaf node
    // 2. Check if actually need to visit all the boxes as we traverse down (already computed, container's min/max does not depend on descendants etc)
    // 3. As we climb back on the tree, compute min/max intrinsic width
    // (Any subtrees with new formatting contexts need to layout synchronously)
    Vector<const Box*> queue;
    // Non-containers early return.
    ASSERT(is<Container>(layoutBox));
    if (auto* firstChild = downcast<Container>(layoutBox).firstInFlowOrFloatingChild())
        queue.append(firstChild);

    auto& formattingStateForChildren = layoutBox.establishesFormattingContext() ? layoutContext.createFormattingStateForFormattingRootIfNeeded(layoutBox) : formattingState;
    while (!queue.isEmpty()) {
        while (true) {
            auto& childBox = *queue.last(); 
            // Already computed?
            auto instrinsicWidthConstraints = formattingStateForChildren.instrinsicWidthConstraints(childBox);
            // Can we just compute them without checking the children?
            if (!instrinsicWidthConstraints && !Geometry::instrinsicWidthConstraintsNeedChildrenWidth(childBox))
                instrinsicWidthConstraints = Geometry::instrinsicWidthConstraints(layoutContext, childBox);
            // Is it a formatting context root?
            if (!instrinsicWidthConstraints && childBox.establishesFormattingContext())
                instrinsicWidthConstraints = layoutContext.formattingContext(childBox)->instrinsicWidthConstraints(layoutContext, childBox);
            // Go to the next sibling (and skip the descendants) if this box's min/max width is computed.
            if (instrinsicWidthConstraints) {
                formattingStateForChildren.setInstrinsicWidthConstraints(childBox, *instrinsicWidthConstraints); 
                queue.removeLast();
                if (!childBox.nextInFlowOrFloatingSibling())
                    break;
                queue.append(childBox.nextInFlowOrFloatingSibling());
                continue;
            }

            if (!is<Container>(childBox) || !downcast<Container>(childBox).hasInFlowOrFloatingChild())
                break;

            queue.append(downcast<Container>(childBox).firstInFlowOrFloatingChild());
        }

        // Compute min/max intrinsic width bottom up.
        while (!queue.isEmpty()) {
            auto& childBox = *queue.takeLast();
            formattingStateForChildren.setInstrinsicWidthConstraints(childBox, Geometry::instrinsicWidthConstraints(layoutContext, childBox)); 
            // Move over to the next sibling or take the next box in the queue.
            if (!is<Container>(childBox) || !downcast<Container>(childBox).nextInFlowOrFloatingSibling())
                continue;
            queue.append(downcast<Container>(childBox).nextInFlowOrFloatingSibling());
        }
    }

    auto instrinsicWidthConstraints = Geometry::instrinsicWidthConstraints(layoutContext, layoutBox);
    formattingState.setInstrinsicWidthConstraints(layoutBox, instrinsicWidthConstraints); 
    return instrinsicWidthConstraints;
}

}
}

#endif

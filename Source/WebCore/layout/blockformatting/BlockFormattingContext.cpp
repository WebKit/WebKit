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
        layoutQueue.append(std::make_unique<LayoutPair>(LayoutPair {*firstChild, layoutContext.createDisplayBox(*firstChild)}));
    // 1. Go all the way down to the leaf node
    // 2. Compute static position and width as we traverse down
    // 3. As we climb back on the tree, compute height and finialize position
    // (Any subtrees with new formatting contexts need to layout synchronously)
    while (!layoutQueue.isEmpty()) {
        // Traverse down on the descendants and compute width/static position until we find a leaf node.
        while (true) {
            auto& layoutPair = *layoutQueue.last();
            auto& layoutBox = layoutPair.layoutBox;
            auto& displayBox = layoutPair.displayBox;

            if (layoutBox.establishesFormattingContext()) {
                layoutFormattingContextRoot(layoutContext, formattingState, layoutBox, displayBox);
                layoutQueue.removeLast();
                // Since this box is a formatting context root, it takes care of its entire subtree.
                // Continue with next sibling if exists.
                if (!layoutBox.nextInFlowOrFloatingSibling())
                    break;
                auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling();
                layoutQueue.append(std::make_unique<LayoutPair>(LayoutPair {*nextSibling, layoutContext.createDisplayBox(*nextSibling)}));
                continue;
            }

            LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
            computeStaticPosition(layoutContext, layoutBox, displayBox);
            computeBorderAndPadding(layoutContext, layoutBox, displayBox);
            computeWidthAndMargin(layoutContext, layoutBox, displayBox);
            if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
                break;
            auto& firstChild = *downcast<Container>(layoutBox).firstInFlowOrFloatingChild();
            layoutQueue.append(std::make_unique<LayoutPair>(LayoutPair {firstChild, layoutContext.createDisplayBox(firstChild)}));
        }

        // Climb back on the ancestors and compute height/final position.
        while (!layoutQueue.isEmpty()) {
            // All inflow descendants (if there are any) are laid out by now. Let's compute the box's height.
            auto layoutPair = layoutQueue.takeLast();
            auto& layoutBox = layoutPair->layoutBox;
            auto& displayBox = layoutPair->displayBox;

            LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Height][Margin] -> for layoutBox(" << &layoutBox << ")");
            // Formatting root boxes are special-cased and they don't come here.
            ASSERT(!layoutBox.establishesFormattingContext());

            computeHeightAndMargin(layoutContext, layoutBox, displayBox);
            // Adjust position now that we have all the previous floats placed in this context -if needed.
            floatingContext.computePosition(layoutBox, displayBox);
            if (!is<Container>(layoutBox))
                continue;
            auto& container = downcast<Container>(layoutBox);
            // Move in-flow positioned children to their final position.
            placeInFlowPositionedChildren(layoutContext, container);
            layoutOutOfFlowDescendants(layoutContext, container);
            if (auto* nextSibling = container.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(std::make_unique<LayoutPair>(LayoutPair {*nextSibling, layoutContext.createDisplayBox(*nextSibling)}));
                break;
            }
        }
    }
    // Place the inflow positioned children.
    placeInFlowPositionedChildren(layoutContext, formattingRoot);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> block formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");
}

void BlockFormattingContext::layoutFormattingContextRoot(LayoutContext& layoutContext, FormattingState& formattingState, const Box& layoutBox, Display::Box& displayBox) const
{
    // Start laying out this formatting root in the formatting contenxt it lives in.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeStaticPosition(layoutContext, layoutBox, displayBox);
    computeBorderAndPadding(layoutContext, layoutBox, displayBox);
    computeWidthAndMargin(layoutContext, layoutBox, displayBox);

    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutContext.formattingContext(layoutBox);
    auto& establishedFormattingState = layoutContext.establishedFormattingState(layoutBox, *formattingContext);
    formattingContext->layout(layoutContext, establishedFormattingState);

    // Come back and finalize the root's geometry.
    FloatingContext(formattingState.floatingState()).computePosition(layoutBox, displayBox);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Height][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeHeightAndMargin(layoutContext, layoutBox, displayBox);
    // Now that we computed the root's height, we can go back and layout the out-of-flow descedants (if any).
    formattingContext->layoutOutOfFlowDescendants(layoutContext, layoutBox);
}

std::unique_ptr<FormattingState> BlockFormattingContext::createFormattingState(Ref<FloatingState>&& floatingState, const LayoutContext& layoutContext) const
{
    return std::make_unique<BlockFormattingState>(WTFMove(floatingState), layoutContext);
}

Ref<FloatingState> BlockFormattingContext::createOrFindFloatingState(LayoutContext&) const
{
    // Block formatting context always establishes a new floating state.
    return FloatingState::create();
}

void BlockFormattingContext::computeStaticPosition(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    displayBox.setTopLeft(Geometry::staticPosition(layoutContext, layoutBox));
}

void BlockFormattingContext::computeInFlowPositionedPosition(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    displayBox.setTopLeft(Geometry::inFlowPositionedPosition(layoutContext, layoutBox));
}

void BlockFormattingContext::computeWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isInFlow())
        return computeInFlowWidthAndMargin(layoutContext, layoutBox, displayBox);

    if (layoutBox.isFloatingPositioned())
        return computeFloatingWidthAndMargin(layoutContext, layoutBox, displayBox);

    ASSERT_NOT_REACHED();
}

void BlockFormattingContext::computeHeightAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (layoutBox.isInFlow())
        return computeInFlowHeightAndMargin(layoutContext, layoutBox, displayBox);

    if (layoutBox.isFloatingPositioned())
        return computeFloatingHeightAndMargin(layoutContext, layoutBox, displayBox);

    ASSERT_NOT_REACHED();
}

void BlockFormattingContext::computeInFlowHeightAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto heightAndMargin = Geometry::inFlowHeightAndMargin(layoutContext, layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.moveVertically(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin).top);
    displayBox.setVerticalMargin(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin));
    displayBox.setVerticalNonCollapsedMargin(heightAndMargin.margin);
}

void BlockFormattingContext::computeInFlowWidthAndMargin(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    auto widthAndMargin = Geometry::inFlowWidthAndMargin(layoutContext, layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.moveHorizontally(widthAndMargin.margin.left);
    displayBox.setHorizontalMargin(widthAndMargin.margin);
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

    auto& formattingStateForChildren = layoutBox.establishesFormattingContext() ? layoutContext.establishedFormattingState(layoutBox, *this) : formattingState;
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

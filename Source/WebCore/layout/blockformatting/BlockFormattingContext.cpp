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
#include "BlockMarginCollapse.h"
#include "DisplayBox.h"
#include "FloatingContext.h"
#include "FloatingState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

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
            
            computeWidth(layoutContext, layoutBox, displayBox);
            computeStaticPosition(layoutContext, layoutBox, layoutPair.displayBox);
            if (layoutBox.establishesFormattingContext()) {
                auto formattingContext = layoutContext.formattingContext(layoutBox);
                formattingContext->layout(layoutContext, layoutContext.establishedFormattingState(layoutBox, *formattingContext));
                break;
            }
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

            computeHeight(layoutContext, layoutBox, displayBox);
            // Adjust position now that we have all the previous floats placed in this context -if needed.
            floatingContext.computePosition(layoutBox, displayBox);
            if (!is<Container>(layoutBox))
                continue;
            auto& container = downcast<Container>(layoutBox);
            // Move in-flow positioned children to their final position.
            placeInFlowPositionedChildren(container);
            if (auto* nextSibling = container.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(std::make_unique<LayoutPair>(LayoutPair {*nextSibling, layoutContext.createDisplayBox(*nextSibling)}));
                break;
            }
        }
    }
    // Place the inflow positioned children.
    placeInFlowPositionedChildren(formattingRoot);
    // And take care of out-of-flow boxes as the final step.
    layoutOutOfFlowDescendants(layoutContext);
}

std::unique_ptr<FormattingState> BlockFormattingContext::createFormattingState(Ref<FloatingState>&& floatingState) const
{
    return std::make_unique<BlockFormattingState>(WTFMove(floatingState));
}

Ref<FloatingState> BlockFormattingContext::createOrFindFloatingState(LayoutContext&) const
{
    // Block formatting context always establishes a new floating state.
    return FloatingState::create();
}

void BlockFormattingContext::computeStaticPosition(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    // https://www.w3.org/TR/CSS22/visuren.html#block-formatting
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    // In a block formatting context, each box's left outer edge touches the left edge of the containing block (for right-to-left formatting, right edges touch).
    auto containingBlockContentBox = layoutContext.displayBoxForLayoutBox(*layoutBox.containingBlock())->contentBox();
    // Start from the top of the container's content box.
    auto top = containingBlockContentBox.y();
    auto left = containingBlockContentBox.x();
    if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling())
        top = layoutContext.displayBoxForLayoutBox(*previousInFlowSibling)->bottom() + marginBottom(*previousInFlowSibling);
    LayoutPoint topLeft = { top, left };
    topLeft.moveBy({ marginLeft(layoutBox), marginTop(layoutBox) });
    displayBox.setTopLeft(topLeft);
}

void BlockFormattingContext::computeInFlowWidth(const Box&, Display::Box&) const
{
}

void BlockFormattingContext::computeInFlowHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    if (!layoutBox.isReplaced()) {
        computeInFlowNonReplacedHeight(layoutContext, layoutBox, displayBox);
        return;
    }
    ASSERT_NOT_REACHED();
}

LayoutUnit BlockFormattingContext::marginTop(const Box& layoutBox) const
{
    return BlockMarginCollapse::marginTop(layoutBox);
}

LayoutUnit BlockFormattingContext::marginBottom(const Box& layoutBox) const
{
    return BlockMarginCollapse::marginBottom(layoutBox);
}

void BlockFormattingContext::computeInFlowNonReplacedHeight(LayoutContext& layoutContext, const Box& layoutBox, Display::Box& displayBox) const
{
    // https://www.w3.org/TR/CSS22/visudet.html
    // If 'height' is 'auto', the height depends on whether the element has any block-level children and whether it has padding or borders:
    // The element's height is the distance from its top content edge to the first applicable of the following:
    // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
    // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin
    //    does not collapse with the element's bottom margin
    // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
    // 4. zero, otherwise
    // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
    // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.
    if (!layoutBox.style().logicalHeight().isAuto()) {
        // FIXME: Only fixed values yet.
        displayBox.setHeight(layoutBox.style().logicalHeight().value());
        return;
    }

    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild()) {
        displayBox.setHeight(0);
        return;
    }

    // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
    if (layoutBox.establishesInlineFormattingContext()) {
        // height = lastLineBox().bottom();
        displayBox.setHeight(0);
        return;
    }

    // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin...
    auto* lastInFlowChild = downcast<Container>(layoutBox).lastInFlowChild();
    ASSERT(lastInFlowChild);
    if (!BlockMarginCollapse::isMarginBottomCollapsedWithParent(*lastInFlowChild)) {
        auto* lastInFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*lastInFlowChild);
        ASSERT(lastInFlowDisplayBox);
        displayBox.setHeight(lastInFlowDisplayBox->bottom() + lastInFlowDisplayBox->marginBottom());
        return;
    }

    // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
    auto* inFlowChild = lastInFlowChild;
    while (inFlowChild && BlockMarginCollapse::isMarginTopCollapsedWithParentMarginBottom(*inFlowChild))
        inFlowChild = inFlowChild->previousInFlowSibling();
    if (inFlowChild) {
        auto* inFlowDisplayBox = layoutContext.displayBoxForLayoutBox(*inFlowChild);
        ASSERT(inFlowDisplayBox);
        displayBox.setHeight(inFlowDisplayBox->top() + inFlowDisplayBox->borderBox().height());
        return;
    }

    // 4. zero, otherwise
    displayBox.setHeight(0);
}

}
}

#endif

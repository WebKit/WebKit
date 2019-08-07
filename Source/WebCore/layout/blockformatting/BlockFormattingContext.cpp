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
#include "LayoutState.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const Box& formattingContextRoot, BlockFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void BlockFormattingContext::layout() const
{
    // 9.4.1 Block formatting contexts
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> block formatting context -> formatting root(" << &root() << ")");

    auto& formattingRoot = downcast<Container>(root());
    LayoutQueue layoutQueue;
    FloatingContext floatingContext(formattingState().floatingState());
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
                layoutFormattingContextRoot(floatingContext, layoutBox);
                layoutQueue.removeLast();
                // Since this box is a formatting context root, it takes care of its entire subtree.
                // Continue with next sibling if exists.
                if (!layoutBox.nextInFlowOrFloatingSibling())
                    break;
                layoutQueue.append(layoutBox.nextInFlowOrFloatingSibling());
                continue;
            }

            LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
            computeBorderAndPadding(layoutBox);
            computeWidthAndMargin(layoutBox);
            computeStaticPosition(floatingContext, layoutBox);
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
            computeHeightAndMargin(layoutBox);
            // Move in-flow positioned children to their final position.
            placeInFlowPositionedChildren(layoutBox);
            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
    // Place the inflow positioned children.
    placeInFlowPositionedChildren(formattingRoot);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> block formatting context -> formatting root(" << &root() << ")");
}

Optional<LayoutUnit> BlockFormattingContext::usedAvailableWidthForFloatAvoider(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    // Normally the available width for an in-flow block level box is the width of the containing block's content box.
    // However (and can't find it anywhere in the spec) non-floating positioned float avoider block level boxes are constrained by existing floats.
    if (!layoutBox.isFloatAvoider() || layoutBox.isFloatingPositioned())
        return { };
    auto& floatingState = floatingContext.floatingState();
    if (floatingState.isEmpty())
        return { };
    // Vertical static position is not computed yet, so let's just estimate it for now.
    auto& formattingRoot = downcast<Container>(root());
    auto verticalPosition = FormattingContext::mapTopToAncestor(layoutState(), layoutBox, formattingRoot);
    auto constraints = floatingState.constraints({ verticalPosition }, formattingRoot);
    if (!constraints.left && !constraints.right)
        return { };
    auto& containingBlock = downcast<Container>(*layoutBox.containingBlock());
    auto& containingBlockDisplayBox = layoutState().displayBoxForLayoutBox(containingBlock);
    auto availableWidth = containingBlockDisplayBox.contentBoxWidth();

    LayoutUnit containingBlockLeft;
    LayoutUnit containingBlockRight = containingBlockDisplayBox.right();
    if (&containingBlock != &formattingRoot) {
        // Move containing block left/right to the root's coordinate system.
        containingBlockLeft = FormattingContext::mapLeftToAncestor(layoutState(), containingBlock, formattingRoot);
        containingBlockRight = FormattingContext::mapRightToAncestor(layoutState(), containingBlock, formattingRoot);
    }
    auto containingBlockContentBoxLeft = containingBlockLeft + containingBlockDisplayBox.borderLeft() + containingBlockDisplayBox.paddingLeft().valueOr(0);
    auto containingBlockContentBoxRight = containingBlockRight - containingBlockDisplayBox.borderRight() + containingBlockDisplayBox.paddingRight().valueOr(0);

    // Shrink the available space if the floats are actually intruding at this vertical position.
    if (constraints.left)
        availableWidth -= std::max<LayoutUnit>(0, constraints.left->x - containingBlockContentBoxLeft);
    if (constraints.right)
        availableWidth -= std::max<LayoutUnit>(0, containingBlockContentBoxRight - constraints.right->x);
    return availableWidth;
}

void BlockFormattingContext::layoutFormattingContextRoot(FloatingContext& floatingContext, const Box& layoutBox) const
{
    ASSERT(layoutBox.establishesFormattingContext());
    // Start laying out this formatting root in the formatting contenxt it lives in.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeBorderAndPadding(layoutBox);
    computeStaticVerticalPosition(floatingContext, layoutBox);

    computeWidthAndMargin(layoutBox, usedAvailableWidthForFloatAvoider(floatingContext, layoutBox));
    computeStaticHorizontalPosition(layoutBox);
    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutState().createFormattingContext(layoutBox);
    formattingContext->layout();

    // Come back and finalize the root's geometry.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Height][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeHeightAndMargin(layoutBox);
    // Now that we computed the root's height, we can go back and layout the out-of-flow content.
    formattingContext->layoutOutOfFlowContent();

    // Float related final positioning.
    if (layoutBox.isFloatingPositioned()) {
        computeFloatingPosition(floatingContext, layoutBox);
        floatingContext.floatingState().append(layoutBox);
    } else if (layoutBox.establishesBlockFormattingContext())
        computePositionToAvoidFloats(floatingContext, layoutBox);
}

void BlockFormattingContext::placeInFlowPositionedChildren(const Box& layoutBox) const
{
    if (!is<Container>(layoutBox))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: move in-flow positioned children -> parent: " << &layoutBox);
    auto& container = downcast<Container>(layoutBox);
    for (auto& childBox : childrenOfType<Box>(container)) {
        if (!childBox.isInFlowPositioned())
            continue;

        auto computeInFlowPositionedPosition = [&] {
            auto& layoutState = this->layoutState();
            auto positionOffset = Geometry::inFlowPositionedPositionOffset(layoutState, childBox);

            auto& displayBox = layoutState.displayBoxForLayoutBox(childBox);
            auto topLeft = displayBox.topLeft();

            topLeft.move(positionOffset);

            displayBox.setTopLeft(topLeft);
        };

        computeInFlowPositionedPosition();
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: move in-flow positioned children -> parent: " << &layoutBox);
}

void BlockFormattingContext::computeStaticVerticalPosition(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();
    layoutState.displayBoxForLayoutBox(layoutBox).setTop(Geometry::staticVerticalPosition(layoutState, layoutBox));
    if (layoutBox.hasFloatClear())
        computeEstimatedVerticalPositionForFloatClear(floatingContext, layoutBox);
    else if (layoutBox.establishesFormattingContext())
        computeEstimatedVerticalPositionForFormattingRoot(layoutBox);
}

void BlockFormattingContext::computeStaticHorizontalPosition(const Box& layoutBox) const
{
    layoutState().displayBoxForLayoutBox(layoutBox).setLeft(Geometry::staticHorizontalPosition(layoutState(), layoutBox));
}

void BlockFormattingContext::computeStaticPosition(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    computeStaticVerticalPosition(floatingContext, layoutBox);
    computeStaticHorizontalPosition(layoutBox);
}

void BlockFormattingContext::computeEstimatedVerticalPosition(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();
    auto estimatedMarginBefore = MarginCollapse::estimatedMarginBefore(layoutState, layoutBox);
    setEstimatedMarginBefore(layoutBox, estimatedMarginBefore);

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    auto nonCollapsedValues = UsedVerticalMargin::NonCollapsedValues { estimatedMarginBefore.nonCollapsedValue, { } };
    auto collapsedValues = UsedVerticalMargin::CollapsedValues { estimatedMarginBefore.collapsedValue, { }, estimatedMarginBefore.isCollapsedThrough };
    auto verticalMargin = UsedVerticalMargin { nonCollapsedValues, collapsedValues };
    displayBox.setVerticalMargin(verticalMargin);
    displayBox.setTop(verticalPositionWithMargin(layoutBox, verticalMargin));
#if !ASSERT_DISABLED
    displayBox.setHasEstimatedMarginBefore();
#endif
}

void BlockFormattingContext::computeEstimatedVerticalPositionForAncestors(const Box& layoutBox) const
{
    // We only need to estimate margin top for float related layout (formatting context roots avoid floats).
    ASSERT(layoutBox.isFloatingPositioned() || layoutBox.hasFloatClear() || layoutBox.establishesBlockFormattingContext() || layoutBox.establishesInlineFormattingContext());

    // In order to figure out whether a box should avoid a float, we need to know the final positions of both (ignore relative positioning for now).
    // In block formatting context the final position for a normal flow box includes
    // 1. the static position and
    // 2. the corresponding (non)collapsed margins.
    // Now the vertical margins are computed when all the descendants are finalized, because the margin values might be depending on the height of the box
    // (and the height might be based on the content).
    // So when we get to the point where we intersect the box with the float to decide if the box needs to move, we don't yet have the final vertical position.
    //
    // The idea here is that as long as we don't cross the block formatting context boundary, we should be able to pre-compute the final top position.
    for (auto* ancestor = layoutBox.containingBlock(); ancestor && !ancestor->establishesBlockFormattingContext(); ancestor = ancestor->containingBlock()) {
        // FIXME: with incremental layout, we might actually have a valid (non-estimated) margin top as well.
        if (hasEstimatedMarginBefore(*ancestor))
            return;
        computeEstimatedVerticalPosition(*ancestor);
    }
}

void BlockFormattingContext::computeEstimatedVerticalPositionForFormattingRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.establishesFormattingContext());
    ASSERT(!layoutBox.hasFloatClear());

    if (layoutBox.isFloatingPositioned()) {
        computeEstimatedVerticalPositionForAncestors(layoutBox);
        return;
    }

    computeEstimatedVerticalPosition(layoutBox);
    computeEstimatedVerticalPositionForAncestors(layoutBox);

    // If the inline formatting root is also the root for the floats (happens when the root box also establishes a block formatting context)
    // the floats are in the coordinate system of this root. No need to find the final vertical position.
    auto inlineContextInheritsFloats = layoutBox.establishesInlineFormattingContextOnly();
    if (inlineContextInheritsFloats) {
        computeEstimatedVerticalPosition(layoutBox);
        computeEstimatedVerticalPositionForAncestors(layoutBox);
    }
}

void BlockFormattingContext::computeEstimatedVerticalPositionForFloatClear(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    ASSERT(layoutBox.hasFloatClear());
    if (floatingContext.floatingState().isEmpty())
        return;
    // The static position with clear requires margin esitmation to see if clearance is needed.
    computeEstimatedVerticalPosition(layoutBox);
    computeEstimatedVerticalPositionForAncestors(layoutBox);
    auto verticalPositionAndClearance = floatingContext.verticalPositionWithClearance(layoutBox);
    if (!verticalPositionAndClearance.position) {
        ASSERT(!verticalPositionAndClearance.clearance);
        return;
    }

    auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);
    ASSERT(*verticalPositionAndClearance.position >= displayBox.top());
    displayBox.setTop(*verticalPositionAndClearance.position);
    if (verticalPositionAndClearance.clearance)
        displayBox.setHasClearance();
}

#ifndef NDEBUG
bool BlockFormattingContext::hasPrecomputedMarginBefore(const Box& layoutBox) const
{
    for (auto* ancestor = layoutBox.containingBlock(); ancestor && !ancestor->establishesBlockFormattingContext(); ancestor = ancestor->containingBlock()) {
        if (hasEstimatedMarginBefore(*ancestor))
            continue;
        return false;
    }
    return true;
}
#endif

void BlockFormattingContext::computeFloatingPosition(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(hasPrecomputedMarginBefore(layoutBox));
    layoutState().displayBoxForLayoutBox(layoutBox).setTopLeft(floatingContext.positionForFloat(layoutBox));
}

void BlockFormattingContext::computePositionToAvoidFloats(const FloatingContext& floatingContext, const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();
    // Formatting context roots avoid floats.
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(hasPrecomputedMarginBefore(layoutBox));

    if (floatingContext.floatingState().isEmpty())
        return;

    if (auto adjustedPosition = floatingContext.positionForFormattingContextRoot(layoutBox))
        layoutState.displayBoxForLayoutBox(layoutBox).setTopLeft(*adjustedPosition);
}

void BlockFormattingContext::computeWidthAndMargin(const Box& layoutBox, Optional<LayoutUnit> usedAvailableWidth) const
{
    auto& layoutState = this->layoutState();

    LayoutUnit availableWidth;
    if (usedAvailableWidth)
        availableWidth = *usedAvailableWidth;
    else
        availableWidth = layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth();

    auto compute = [&](Optional<LayoutUnit> usedWidth) -> WidthAndMargin {
        auto usedValues = UsedHorizontalValues { availableWidth, usedWidth, { } };
        if (layoutBox.isInFlow())
            return Geometry::inFlowWidthAndMargin(layoutState, layoutBox, usedValues);

        if (layoutBox.isFloatingPositioned())
            return Geometry::floatingWidthAndMargin(layoutState, layoutBox, usedValues);

        ASSERT_NOT_REACHED();
        return { };
    };

    auto widthAndMargin = compute({ });

    if (auto maxWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMaxWidth(), availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (widthAndMargin.width > maxWidthAndMargin.width)
            widthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = Geometry::computedValueIfNotAuto(layoutBox.style().logicalMinWidth(), availableWidth).valueOr(0);
    auto minWidthAndMargin = compute(minWidth);
    if (widthAndMargin.width < minWidthAndMargin.width)
        widthAndMargin = minWidthAndMargin;

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(widthAndMargin.computedMargin);
}

void BlockFormattingContext::computeHeightAndMargin(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    auto compute = [&](UsedVerticalValues usedValues) -> HeightAndMargin {

        if (layoutBox.isInFlow())
            return Geometry::inFlowHeightAndMargin(layoutState, layoutBox, usedValues);

        if (layoutBox.isFloatingPositioned())
            return Geometry::floatingHeightAndMargin(layoutState, layoutBox, usedValues, UsedHorizontalValues { layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth() });

        ASSERT_NOT_REACHED();
        return { };
    };

    auto heightAndMargin = compute({ });
    if (auto maxHeight = Geometry::computedMaxHeight(layoutState, layoutBox)) {
        if (heightAndMargin.height > *maxHeight) {
            auto maxHeightAndMargin = compute({ *maxHeight });
            // Used height should remain the same.
            ASSERT((layoutState.inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || maxHeightAndMargin.height == *maxHeight);
            heightAndMargin = { *maxHeight, maxHeightAndMargin.nonCollapsedMargin };
        }
    }

    if (auto minHeight = Geometry::computedMinHeight(layoutState, layoutBox)) {
        if (heightAndMargin.height < *minHeight) {
            auto minHeightAndMargin = compute({ *minHeight });
            // Used height should remain the same.
            ASSERT((layoutState.inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || minHeightAndMargin.height == *minHeight);
            heightAndMargin = { *minHeight, minHeightAndMargin.nonCollapsedMargin };
        }
    }

    // 1. Compute collapsed margins.
    // 2. Adjust vertical position using the collapsed values
    // 3. Adjust previous in-flow sibling margin after using this margin.
    auto collapsedMargin = MarginCollapse::collapsedVerticalValues(layoutState, layoutBox, heightAndMargin.nonCollapsedMargin);
    auto verticalMargin = UsedVerticalMargin { heightAndMargin.nonCollapsedMargin, collapsedMargin };
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

    // Out of flow boxes don't need vertical adjustment after margin collapsing.
    if (layoutBox.isOutOfFlowPositioned()) {
        ASSERT(!hasEstimatedMarginBefore(layoutBox));
        displayBox.setContentBoxHeight(heightAndMargin.height);
        displayBox.setVerticalMargin(verticalMargin);
        return;
    }

    ASSERT(!hasEstimatedMarginBefore(layoutBox) || estimatedMarginBefore(layoutBox).usedValue() == verticalMargin.before());
    removeEstimatedMarginBefore(layoutBox);
    displayBox.setTop(verticalPositionWithMargin(layoutBox, verticalMargin));
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalMargin(verticalMargin);

    MarginCollapse::updatePositiveNegativeMarginValues(layoutState, layoutBox);
    // Adjust the previous sibling's margin bottom now that this box's vertical margin is computed.
    MarginCollapse::updateMarginAfterForPreviousSibling(layoutState, layoutBox);
}

FormattingContext::IntrinsicWidthConstraints BlockFormattingContext::computedIntrinsicWidthConstraints() const
{
    auto& layoutState = this->layoutState();
    auto& formattingRoot = root();
    auto& formattingState = this->formattingState();
    ASSERT(!formattingState.intrinsicWidthConstraints());

    // Visit the in-flow descendants and compute their min/max intrinsic width if needed.
    // 1. Go all the way down to the leaf node
    // 2. Check if actually need to visit all the boxes as we traverse down (already computed, container's min/max does not depend on descendants etc)
    // 3. As we climb back on the tree, compute min/max intrinsic width
    // (Any subtrees with new formatting contexts need to layout synchronously)
    Vector<const Box*> queue;
    if (is<Container>(formattingRoot) && downcast<Container>(formattingRoot).hasInFlowOrFloatingChild())
        queue.append(downcast<Container>(formattingRoot).firstInFlowOrFloatingChild());

    IntrinsicWidthConstraints constraints;
    while (!queue.isEmpty()) {
        while (true) {
            auto& layoutBox = *queue.last();
            auto hasInFlowOrFloatingChild = is<Container>(layoutBox) && downcast<Container>(layoutBox).hasInFlowOrFloatingChild();
            auto skipDescendants = formattingState.intrinsicWidthConstraintsForBox(layoutBox) || !hasInFlowOrFloatingChild || layoutBox.establishesFormattingContext() || layoutBox.style().width().isFixed();
            if (skipDescendants)
                break;
            queue.append(downcast<Container>(layoutBox).firstInFlowOrFloatingChild());
        }
        // Compute min/max intrinsic width bottom up if needed.
        while (!queue.isEmpty()) {
            auto& layoutBox = *queue.takeLast();
            auto desdendantConstraints = formattingState.intrinsicWidthConstraintsForBox(layoutBox); 
            if (!desdendantConstraints) {
                desdendantConstraints = Geometry::intrinsicWidthConstraints(layoutState, layoutBox);
                formattingState.setIntrinsicWidthConstraintsForBox(layoutBox, *desdendantConstraints);
            }
            constraints.minimum = std::max(constraints.minimum, desdendantConstraints->minimum);
            constraints.maximum = std::max(constraints.maximum, desdendantConstraints->maximum);
            // Move over to the next sibling or take the next box in the queue.
            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                queue.append(nextSibling);
                break;
            }
        }
    }
    formattingState.setIntrinsicWidthConstraints(constraints);
    return constraints;
}

LayoutUnit BlockFormattingContext::verticalPositionWithMargin(const Box& layoutBox, const UsedVerticalMargin& verticalMargin) const
{
    ASSERT(!layoutBox.isOutOfFlowPositioned());
    // Now that we've computed the final margin before, let's shift the box's vertical position if needed.
    // 1. Check if the box has clearance. If so, we've already precomputed/finalized the top value and vertical margin does not impact it anymore.
    // 2. Check if the margin before collapses with the previous box's margin after. if not -> return previous box's bottom including margin after + marginBefore
    // 3. Check if the previous box's margins collapse through. If not -> return previous box' bottom excluding margin after + marginBefore (they are supposed to be equal)
    // 4. Go to previous box and start from step #1 until we hit the parent box.
    auto& layoutState = this->layoutState();
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    if (displayBox.hasClearance())
        return displayBox.top();

    auto* currentLayoutBox = &layoutBox;
    while (currentLayoutBox) {
        if (!currentLayoutBox->previousInFlowSibling())
            break;
        auto& previousInFlowSibling = *currentLayoutBox->previousInFlowSibling();
        if (!MarginCollapse::marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutState, *currentLayoutBox)) {
            auto& previousDisplayBox = layoutState.displayBoxForLayoutBox(previousInFlowSibling);
            return previousDisplayBox.rectWithMargin().bottom() + verticalMargin.before();
        }

        if (!MarginCollapse::marginsCollapseThrough(layoutState, previousInFlowSibling)) {
            auto& previousDisplayBox = layoutState.displayBoxForLayoutBox(previousInFlowSibling);
            return previousDisplayBox.bottom() + verticalMargin.before();
        }
        currentLayoutBox = &previousInFlowSibling;
    }

    auto& containingBlock = *layoutBox.containingBlock();
    auto containingBlockContentBoxTop = layoutState.displayBoxForLayoutBox(containingBlock).contentBoxTop();
    // Adjust vertical position depending whether this box directly or indirectly adjoins with its parent.
    auto directlyAdjoinsParent = !layoutBox.previousInFlowSibling();
    if (directlyAdjoinsParent) {
        // If the top and bottom margins of a box are adjoining, then it is possible for margins to collapse through it.
        // In this case, the position of the element depends on its relationship with the other elements whose margins are being collapsed.
        if (verticalMargin.collapsedValues().isCollapsedThrough) {
            // If the element's margins are collapsed with its parent's top margin, the top border edge of the box is defined to be the same as the parent's.
            if (MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(layoutState, layoutBox))
                return containingBlockContentBoxTop;
            // Otherwise, either the element's parent is not taking part in the margin collapsing, or only the parent's bottom margin is involved.
            // The position of the element's top border edge is the same as it would have been if the element had a non-zero bottom border.
            auto beforeMarginWithBottomBorder = MarginCollapse::marginBeforeIgnoringCollapsingThrough(layoutState, layoutBox, verticalMargin.nonCollapsedValues());
            return containingBlockContentBoxTop + beforeMarginWithBottomBorder;
        }
        // Non-collapsed through box vertical position depending whether the margin collapses.
        if (MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(layoutState, layoutBox))
            return containingBlockContentBoxTop;

        return containingBlockContentBoxTop + verticalMargin.before();
    }
    // At this point this box indirectly (via collapsed through previous in-flow siblings) adjoins the parent. Let's check if it margin collapses with the parent.
    ASSERT(containingBlock.firstInFlowChild());
    ASSERT(containingBlock.firstInFlowChild() != &layoutBox);
    if (MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(layoutState, *containingBlock.firstInFlowChild()))
        return containingBlockContentBoxTop;

    return containingBlockContentBoxTop + verticalMargin.before();
}

void BlockFormattingContext::setEstimatedMarginBefore(const Box& layoutBox, const EstimatedMarginBefore& estimatedMarginBefore) const
{
    // Can't cross formatting context boundary.
    ASSERT(&layoutState().formattingStateForBox(layoutBox) == &formattingState());
    m_estimatedMarginBeforeList.set(&layoutBox, estimatedMarginBefore);
}

bool BlockFormattingContext::hasEstimatedMarginBefore(const Box& layoutBox) const
{
    // Can't cross formatting context boundary.
    ASSERT(&layoutState().formattingStateForBox(layoutBox) == &formattingState());
    return m_estimatedMarginBeforeList.contains(&layoutBox);
}

}
}

#endif

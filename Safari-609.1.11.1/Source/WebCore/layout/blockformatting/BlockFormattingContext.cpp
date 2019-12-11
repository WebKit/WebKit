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
#include "FloatingContext.h"
#include "FloatingState.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutState.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const Container& formattingContextRoot, BlockFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

enum class LayoutDirection { Child, Sibling };
void BlockFormattingContext::layoutInFlowContent(InvalidationState& invalidationState)
{
    // 9.4.1 Block formatting contexts
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> block formatting context -> formatting root(" << &root() << ")");
    auto& formattingRoot = root();
    auto floatingContext = FloatingContext { formattingRoot, *this, formattingState().floatingState() };

    LayoutQueue layoutQueue;
    auto appendNextToLayoutQueue = [&] (const auto& layoutBox, auto direction) {
        if (direction == LayoutDirection::Child) {
            if (!is<Container>(layoutBox))
                return false;
            for (auto* child = downcast<Container>(layoutBox).firstInFlowOrFloatingChild(); child; child = child->nextInFlowOrFloatingSibling()) {
                if (!invalidationState.needsLayout(*child))
                    continue;
                layoutQueue.append(child);
                return true;
            }
            return false;
        }

        if (direction == LayoutDirection::Sibling) {
            for (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling(); nextSibling; nextSibling = nextSibling->nextInFlowOrFloatingSibling()) {
                if (!invalidationState.needsLayout(*nextSibling))
                    continue;
                layoutQueue.append(nextSibling);
                return true;
            }
            return false;
        }
        ASSERT_NOT_REACHED();
        return false;
    };

    // This is a post-order tree traversal layout.
    // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
    appendNextToLayoutQueue(formattingRoot, LayoutDirection::Child);
    // 1. Go all the way down to the leaf node
    // 2. Compute static position and width as we traverse down
    // 3. As we climb back on the tree, compute height and finialize position
    // (Any subtrees with new formatting contexts need to layout synchronously)
    while (!layoutQueue.isEmpty()) {
        // Traverse down on the descendants and compute width/static position until we find a leaf node.
        while (true) {
            auto& layoutBox = *layoutQueue.last();

            if (layoutBox.establishesFormattingContext()) {
                // layoutFormattingContextRoot() takes care of the layoutBox itself and its descendants.
                layoutFormattingContextRoot(floatingContext, layoutBox, invalidationState);
                layoutQueue.removeLast();
                if (!appendNextToLayoutQueue(layoutBox, LayoutDirection::Sibling))
                    break;
                continue;
            }

            computeBorderAndPadding(layoutBox);
            computeWidthAndMargin(layoutBox);
            computeStaticPosition(floatingContext, layoutBox);

            if (!appendNextToLayoutQueue(layoutBox, LayoutDirection::Child))
                break;
        }

        // Climb back on the ancestors and compute height/final position.
        while (!layoutQueue.isEmpty()) {
            // All inflow descendants (if there are any) are laid out by now. Let's compute the box's height.
            auto& layoutBox = *layoutQueue.takeLast();
            // Formatting root boxes are special-cased and they don't come here.
            ASSERT(!layoutBox.establishesFormattingContext());
            computeHeightAndMargin(layoutBox);
            // Move in-flow positioned children to their final position.
            placeInFlowPositionedChildren(layoutBox);
            if (appendNextToLayoutQueue(layoutBox, LayoutDirection::Sibling))
                break;
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
    ASSERT(layoutBox.isFloatAvoider());
    if (floatingContext.isEmpty())
        return { };
    // Vertical static position is not computed yet, so let's just estimate it for now.
    auto verticalPosition = mapTopToFormattingContextRoot(layoutBox);
    auto constraints = floatingContext.constraints({ verticalPosition });
    if (!constraints.left && !constraints.right)
        return { };
    auto& containingBlock = *layoutBox.containingBlock();
    auto& containingBlockGeometry = geometryForBox(containingBlock);
    auto availableWidth = containingBlockGeometry.contentBoxWidth();

    LayoutUnit containingBlockLeft;
    LayoutUnit containingBlockRight = containingBlockGeometry.right();
    if (&containingBlock != &root()) {
        // Move containing block left/right to the root's coordinate system.
        containingBlockLeft = mapLeftToFormattingContextRoot(containingBlock);
        containingBlockRight = mapRightToFormattingContextRoot(containingBlock);
    }
    auto containingBlockContentBoxLeft = containingBlockLeft + containingBlockGeometry.borderLeft() + containingBlockGeometry.paddingLeft().valueOr(0);
    auto containingBlockContentBoxRight = containingBlockRight - containingBlockGeometry.borderRight() + containingBlockGeometry.paddingRight().valueOr(0);

    // Shrink the available space if the floats are actually intruding at this vertical position.
    if (constraints.left)
        availableWidth -= std::max<LayoutUnit>(0, constraints.left->x - containingBlockContentBoxLeft);
    if (constraints.right)
        availableWidth -= std::max<LayoutUnit>(0, containingBlockContentBoxRight - constraints.right->x);
    return availableWidth;
}

void BlockFormattingContext::layoutFormattingContextRoot(FloatingContext& floatingContext, const Box& layoutBox, InvalidationState& invalidationState)
{
    ASSERT(layoutBox.establishesFormattingContext());
    // Start laying out this formatting root in the formatting contenxt it lives in.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Compute] -> [Position][Border][Padding][Width][Margin] -> for layoutBox(" << &layoutBox << ")");
    computeBorderAndPadding(layoutBox);
    computeStaticVerticalPosition(floatingContext, layoutBox);

    Optional<LayoutUnit> usedAvailableWidthForFloatAvoider;
    auto horizontalAvailableSpaceIsConstrainedByExistingFloats = layoutBox.isFloatAvoider() && !layoutBox.isFloatingPositioned();
    if (horizontalAvailableSpaceIsConstrainedByExistingFloats)
        usedAvailableWidthForFloatAvoider = this->usedAvailableWidthForFloatAvoider(floatingContext, layoutBox);
    computeWidthAndMargin(layoutBox, usedAvailableWidthForFloatAvoider);
    computeStaticHorizontalPosition(layoutBox);
    if (is<Container>(layoutBox)) {
        // Swich over to the new formatting context (the one that the root creates).
        auto& rootContainer = downcast<Container>(layoutBox);
        auto formattingContext = LayoutContext::createFormattingContext(rootContainer, layoutState());
        formattingContext->layoutInFlowContent(invalidationState);
        // Come back and finalize the root's geometry.
        computeHeightAndMargin(rootContainer);
        // Now that we computed the root's height, we can go back and layout the out-of-flow content.
        formattingContext->layoutOutOfFlowContent(invalidationState);
    } else
        computeHeightAndMargin(layoutBox);
    // Float related final positioning.
    if (layoutBox.isFloatingPositioned()) {
        computeFloatingPosition(floatingContext, layoutBox);
        floatingContext.append(layoutBox);
    } else if (layoutBox.establishesBlockFormattingContext())
        computePositionToAvoidFloats(floatingContext, layoutBox);
}

void BlockFormattingContext::placeInFlowPositionedChildren(const Box& layoutBox)
{
    if (!is<Container>(layoutBox))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: move in-flow positioned children -> parent: " << &layoutBox);
    auto& container = downcast<Container>(layoutBox);
    for (auto& childBox : childrenOfType<Box>(container)) {
        if (!childBox.isInFlowPositioned())
            continue;

        auto computeInFlowPositionedPosition = [&] {
            auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { geometryForBox(*childBox.containingBlock()) } };
            auto positionOffset = geometry().inFlowPositionedPositionOffset(childBox, usedHorizontalValues);

            auto& displayBox = formattingState().displayBox(childBox);
            auto topLeft = displayBox.topLeft();

            topLeft.move(positionOffset);

            displayBox.setTopLeft(topLeft);
        };

        computeInFlowPositionedPosition();
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: move in-flow positioned children -> parent: " << &layoutBox);
}

void BlockFormattingContext::computeStaticVerticalPosition(const FloatingContext& floatingContext, const Box& layoutBox)
{
    auto usedVerticalValues = UsedVerticalValues { UsedVerticalValues::Constraints { geometryForBox(*layoutBox.containingBlock()) } };
    formattingState().displayBox(layoutBox).setTop(geometry().staticVerticalPosition(layoutBox, usedVerticalValues));
    if (layoutBox.hasFloatClear())
        computeEstimatedVerticalPositionForFloatClear(floatingContext, layoutBox);
    else if (layoutBox.establishesFormattingContext())
        computeEstimatedVerticalPositionForFormattingRoot(layoutBox);
}

void BlockFormattingContext::computeStaticHorizontalPosition(const Box& layoutBox)
{
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { geometryForBox(*layoutBox.containingBlock()) } };
    formattingState().displayBox(layoutBox).setLeft(geometry().staticHorizontalPosition(layoutBox, usedHorizontalValues));
}

void BlockFormattingContext::computeStaticPosition(const FloatingContext& floatingContext, const Box& layoutBox)
{
    computeStaticVerticalPosition(floatingContext, layoutBox);
    computeStaticHorizontalPosition(layoutBox);
}

void BlockFormattingContext::computeEstimatedVerticalPosition(const Box& layoutBox)
{
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { geometryForBox(*layoutBox.containingBlock()) } };
    auto computedVerticalMargin = geometry().computedVerticalMargin(layoutBox, usedHorizontalValues);
    auto usedNonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) };
    auto estimatedMarginBefore = marginCollapse().estimatedMarginBefore(layoutBox, usedNonCollapsedMargin);
    setEstimatedMarginBefore(layoutBox, estimatedMarginBefore);

    auto& displayBox = formattingState().displayBox(layoutBox);
    auto nonCollapsedValues = UsedVerticalMargin::NonCollapsedValues { estimatedMarginBefore.nonCollapsedValue, { } };
    auto collapsedValues = UsedVerticalMargin::CollapsedValues { estimatedMarginBefore.collapsedValue, { }, estimatedMarginBefore.isCollapsedThrough };
    auto verticalMargin = UsedVerticalMargin { nonCollapsedValues, collapsedValues };
    displayBox.setVerticalMargin(verticalMargin);
    displayBox.setTop(verticalPositionWithMargin(layoutBox, verticalMargin));
#if !ASSERT_DISABLED
    displayBox.setHasEstimatedMarginBefore();
#endif
}

void BlockFormattingContext::computeEstimatedVerticalPositionForAncestors(const Box& layoutBox)
{
    // We only need to estimate margin top for float related layout (formatting context roots avoid floats).
    ASSERT(layoutBox.isFloatAvoider() || layoutBox.establishesInlineFormattingContext());

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

void BlockFormattingContext::computeEstimatedVerticalPositionForFormattingRoot(const Box& layoutBox)
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

void BlockFormattingContext::computeEstimatedVerticalPositionForFloatClear(const FloatingContext& floatingContext, const Box& layoutBox)
{
    ASSERT(layoutBox.hasFloatClear());
    if (floatingContext.isEmpty())
        return;
    // The static position with clear requires margin esitmation to see if clearance is needed.
    computeEstimatedVerticalPosition(layoutBox);
    computeEstimatedVerticalPositionForAncestors(layoutBox);
    auto verticalPositionAndClearance = floatingContext.verticalPositionWithClearance(layoutBox);
    if (!verticalPositionAndClearance.position) {
        ASSERT(!verticalPositionAndClearance.clearance);
        return;
    }

    auto& displayBox = formattingState().displayBox(layoutBox);
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

void BlockFormattingContext::computeFloatingPosition(const FloatingContext& floatingContext, const Box& layoutBox)
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(hasPrecomputedMarginBefore(layoutBox));
    formattingState().displayBox(layoutBox).setTopLeft(floatingContext.positionForFloat(layoutBox));
}

void BlockFormattingContext::computePositionToAvoidFloats(const FloatingContext& floatingContext, const Box& layoutBox)
{
    // Formatting context roots avoid floats.
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(hasPrecomputedMarginBefore(layoutBox));

    if (floatingContext.isEmpty())
        return;

    if (auto adjustedPosition = floatingContext.positionForFormattingContextRoot(layoutBox))
        formattingState().displayBox(layoutBox).setTopLeft(*adjustedPosition);
}

void BlockFormattingContext::computeWidthAndMargin(const Box& layoutBox, Optional<LayoutUnit> usedAvailableWidth)
{
    LayoutUnit availableWidth;
    auto containingBlockGeometry = geometryForBox(*layoutBox.containingBlock());
    if (usedAvailableWidth)
        availableWidth = *usedAvailableWidth;
    else
        availableWidth = containingBlockGeometry.contentBoxWidth();
    auto constraints = UsedHorizontalValues::Constraints { containingBlockGeometry.contentBoxLeft(), availableWidth };

    auto compute = [&](Optional<LayoutUnit> usedWidth) -> ContentWidthAndMargin {
        auto usedValues = UsedHorizontalValues { constraints, usedWidth, { } };
        if (layoutBox.isInFlow())
            return geometry().inFlowWidthAndMargin(layoutBox, usedValues);

        if (layoutBox.isFloatingPositioned())
            return geometry().floatingWidthAndMargin(layoutBox, usedValues);

        ASSERT_NOT_REACHED();
        return { };
    };

    auto contentWidthAndMargin = compute({ });

    if (auto maxWidth = geometry().computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = geometry().computedMinWidth(layoutBox, availableWidth).valueOr(0);
    auto minWidthAndMargin = compute(minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin(contentWidthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(contentWidthAndMargin.computedMargin);
}

void BlockFormattingContext::computeHeightAndMargin(const Box& layoutBox)
{
    auto& containingBlockGeometry = geometryForBox(*layoutBox.containingBlock());
    auto compute = [&](auto usedVerticalValues) -> ContentHeightAndMargin {

        auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { containingBlockGeometry } };
        if (layoutBox.isInFlow())
            return geometry().inFlowHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);

        if (layoutBox.isFloatingPositioned())
            return geometry().floatingHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);

        ASSERT_NOT_REACHED();
        return { };
    };

    auto verticalConstraints = UsedVerticalValues::Constraints { containingBlockGeometry };
    auto contentHeightAndMargin = compute(UsedVerticalValues { verticalConstraints });
    if (auto maxHeight = geometry().computedMaxHeight(layoutBox)) {
        if (contentHeightAndMargin.contentHeight > *maxHeight) {
            auto maxHeightAndMargin = compute(UsedVerticalValues { verticalConstraints, maxHeight });
            // Used height should remain the same.
            ASSERT((layoutState().inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || maxHeightAndMargin.contentHeight == *maxHeight);
            contentHeightAndMargin = { *maxHeight, maxHeightAndMargin.nonCollapsedMargin };
        }
    }

    if (auto minHeight = geometry().computedMinHeight(layoutBox)) {
        if (contentHeightAndMargin.contentHeight < *minHeight) {
            auto minHeightAndMargin = compute(UsedVerticalValues { verticalConstraints, minHeight });
            // Used height should remain the same.
            ASSERT((layoutState().inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || minHeightAndMargin.contentHeight == *minHeight);
            contentHeightAndMargin = { *minHeight, minHeightAndMargin.nonCollapsedMargin };
        }
    }

    // 1. Compute collapsed margins.
    // 2. Adjust vertical position using the collapsed values
    // 3. Adjust previous in-flow sibling margin after using this margin.
    auto collapsedMargin = marginCollapse().collapsedVerticalValues(layoutBox, contentHeightAndMargin.nonCollapsedMargin);
    auto verticalMargin = UsedVerticalMargin { contentHeightAndMargin.nonCollapsedMargin, collapsedMargin };
    auto& displayBox = formattingState().displayBox(layoutBox);

    // Out of flow boxes don't need vertical adjustment after margin collapsing.
    if (layoutBox.isOutOfFlowPositioned()) {
        ASSERT(!hasEstimatedMarginBefore(layoutBox));
        displayBox.setContentBoxHeight(contentHeightAndMargin.contentHeight);
        displayBox.setVerticalMargin(verticalMargin);
        return;
    }

    ASSERT(!hasEstimatedMarginBefore(layoutBox) || estimatedMarginBefore(layoutBox).usedValue() == verticalMargin.before());
    removeEstimatedMarginBefore(layoutBox);
    displayBox.setTop(verticalPositionWithMargin(layoutBox, verticalMargin));
    displayBox.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    displayBox.setVerticalMargin(verticalMargin);

    auto marginCollapse = this->marginCollapse();
    MarginCollapse::updatePositiveNegativeMarginValues(*this, marginCollapse, layoutBox);
    // Adjust the previous sibling's margin bottom now that this box's vertical margin is computed.
    MarginCollapse::updateMarginAfterForPreviousSibling(*this, marginCollapse, layoutBox);
}

FormattingContext::IntrinsicWidthConstraints BlockFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& formattingState = this->formattingState();
    ASSERT(!formattingState.intrinsicWidthConstraints());

    // Visit the in-flow descendants and compute their min/max intrinsic width if needed.
    // 1. Go all the way down to the leaf node
    // 2. Check if actually need to visit all the boxes as we traverse down (already computed, container's min/max does not depend on descendants etc)
    // 3. As we climb back on the tree, compute min/max intrinsic width
    // (Any subtrees with new formatting contexts need to layout synchronously)
    Vector<const Box*> queue;
    if (root().hasInFlowOrFloatingChild())
        queue.append(root().firstInFlowOrFloatingChild());

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
                desdendantConstraints = geometry().intrinsicWidthConstraints(layoutBox);
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
    auto& boxGeometry = geometryForBox(layoutBox);
    if (boxGeometry.hasClearance())
        return boxGeometry.top();

    auto* currentLayoutBox = &layoutBox;
    while (currentLayoutBox) {
        if (!currentLayoutBox->previousInFlowSibling())
            break;
        auto& previousInFlowSibling = *currentLayoutBox->previousInFlowSibling();
        if (!marginCollapse().marginBeforeCollapsesWithPreviousSiblingMarginAfter(*currentLayoutBox)) {
            auto& previousBoxGeometry = geometryForBox(previousInFlowSibling);
            return previousBoxGeometry.rectWithMargin().bottom() + verticalMargin.before();
        }

        if (!marginCollapse().marginsCollapseThrough(previousInFlowSibling)) {
            auto& previousBoxGeometry = geometryForBox(previousInFlowSibling);
            return previousBoxGeometry.bottom() + verticalMargin.before();
        }
        currentLayoutBox = &previousInFlowSibling;
    }

    auto& containingBlock = *layoutBox.containingBlock();
    auto containingBlockContentBoxTop = geometryForBox(containingBlock).contentBoxTop();
    // Adjust vertical position depending whether this box directly or indirectly adjoins with its parent.
    auto directlyAdjoinsParent = !layoutBox.previousInFlowSibling();
    if (directlyAdjoinsParent) {
        // If the top and bottom margins of a box are adjoining, then it is possible for margins to collapse through it.
        // In this case, the position of the element depends on its relationship with the other elements whose margins are being collapsed.
        if (verticalMargin.collapsedValues().isCollapsedThrough) {
            // If the element's margins are collapsed with its parent's top margin, the top border edge of the box is defined to be the same as the parent's.
            if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(layoutBox))
                return containingBlockContentBoxTop;
            // Otherwise, either the element's parent is not taking part in the margin collapsing, or only the parent's bottom margin is involved.
            // The position of the element's top border edge is the same as it would have been if the element had a non-zero bottom border.
            auto beforeMarginWithBottomBorder = marginCollapse().marginBeforeIgnoringCollapsingThrough(layoutBox, verticalMargin.nonCollapsedValues());
            return containingBlockContentBoxTop + beforeMarginWithBottomBorder;
        }
        // Non-collapsed through box vertical position depending whether the margin collapses.
        if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(layoutBox))
            return containingBlockContentBoxTop;

        return containingBlockContentBoxTop + verticalMargin.before();
    }
    // At this point this box indirectly (via collapsed through previous in-flow siblings) adjoins the parent. Let's check if it margin collapses with the parent.
    ASSERT(containingBlock.firstInFlowChild());
    ASSERT(containingBlock.firstInFlowChild() != &layoutBox);
    if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(*containingBlock.firstInFlowChild()))
        return containingBlockContentBoxTop;

    return containingBlockContentBoxTop + verticalMargin.before();
}

void BlockFormattingContext::setEstimatedMarginBefore(const Box& layoutBox, const EstimatedMarginBefore& estimatedMarginBefore)
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

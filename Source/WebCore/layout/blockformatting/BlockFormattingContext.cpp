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
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutState.h"
#include "Logging.h"
#include "TableWrapperBlockFormattingContext.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const ContainerBox& formattingContextRoot, BlockFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void BlockFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const ConstraintsForInFlowContent& constraints)
{
    // 9.4.1 Block formatting contexts
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> block formatting context -> formatting root(" << &root() << ")");
    auto& formattingRoot = root();
    ASSERT(formattingRoot.hasInFlowOrFloatingChild());
    auto floatingContext = FloatingContext { formattingRoot, *this, formattingState().floatingState() };

    LayoutQueue layoutQueue;
    enum class LayoutDirection { Child, Sibling };
    auto appendNextToLayoutQueue = [&] (const auto& layoutBox, auto direction) {
        if (direction == LayoutDirection::Child) {
            if (!is<ContainerBox>(layoutBox))
                return false;
            for (auto* child = downcast<ContainerBox>(layoutBox).firstInFlowOrFloatingChild(); child; child = child->nextInFlowOrFloatingSibling()) {
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

    auto constraintsForLayoutBox = [&] (const auto& layoutBox) {
        auto& containingBlock = layoutBox.containingBlock();
        return &containingBlock == &formattingRoot ? constraints : geometry().constraintsForInFlowContent(containingBlock);
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
            auto containingBlockConstraints = constraintsForLayoutBox(layoutBox);

            computeBorderAndPadding(layoutBox, containingBlockConstraints.horizontal);
            computeStaticVerticalPosition(layoutBox, containingBlockConstraints.vertical);
            computeWidthAndMargin(floatingContext, layoutBox, { constraints, containingBlockConstraints });
            computeStaticHorizontalPosition(layoutBox, containingBlockConstraints.horizontal);
            computePositionToAvoidFloats(floatingContext, layoutBox, { constraints, containingBlockConstraints });

            if (layoutBox.establishesFormattingContext()) {
                if (is<ContainerBox>(layoutBox) && downcast<ContainerBox>(layoutBox).hasInFlowOrFloatingChild()) {
                    auto& containerBox = downcast<ContainerBox>(layoutBox);
                    if (containerBox.establishesInlineFormattingContext()) {
                        // IFCs inherit floats from parent FCs. We need final vertical position to find intruding floats.
                        precomputeVerticalPositionForBoxAndAncestors(containerBox, { constraints, containingBlockConstraints });
                    }
                    // Layout the inflow descendants of this formatting context root.
                    auto formattingContext = LayoutContext::createFormattingContext(containerBox, layoutState());
                    if (containerBox.isTableWrapperBox())
                        downcast<TableWrapperBlockFormattingContext>(*formattingContext).setHorizontalConstraintsIgnoringFloats(containingBlockConstraints.horizontal);
                    formattingContext->layoutInFlowContent(invalidationState, geometry().constraintsForInFlowContent(containerBox));
                }
                break;
            }
            if (!appendNextToLayoutQueue(layoutBox, LayoutDirection::Child))
                break;
        }

        // Climb back on the ancestors and compute height/final position.
        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            auto containingBlockConstraints = constraintsForLayoutBox(layoutBox);

            // All inflow descendants (if there are any) are laid out by now. Let's compute the box's height.
            computeHeightAndMargin(layoutBox, containingBlockConstraints);
            if (layoutBox.isFloatingPositioned())
                floatingContext.append(layoutBox);

            auto establishesFormattingContext = layoutBox.establishesFormattingContext(); 
            if (establishesFormattingContext) {
                // Now that we computed the root's height, we can layout the out-of-flow descendants.
                if (is<ContainerBox>(layoutBox) && downcast<ContainerBox>(layoutBox).hasChild()) {
                    auto& containerBox = downcast<ContainerBox>(layoutBox);
                    LayoutContext::createFormattingContext(containerBox, layoutState())->layoutOutOfFlowContent(invalidationState, geometry().constraintsForOutOfFlowContent(containerBox));
                }
            }
            if (!establishesFormattingContext && is<ContainerBox>(layoutBox))
                placeInFlowPositionedChildren(downcast<ContainerBox>(layoutBox), containingBlockConstraints.horizontal);

            if (appendNextToLayoutQueue(layoutBox, LayoutDirection::Sibling))
                break;
        }
    }
    // Place the inflow positioned children.
    placeInFlowPositionedChildren(formattingRoot, constraints.horizontal);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> block formatting context -> formatting root(" << &root() << ")");
}

Optional<LayoutUnit> BlockFormattingContext::usedAvailableWidthForFloatAvoider(const FloatingContext& floatingContext, const Box& layoutBox, const ConstraintsPair& constraintsPair)
{
    // Normally the available width for an in-flow block level box is the width of the containing block's content box.
    // However (and can't find it anywhere in the spec) non-floating positioned float avoider block level boxes are constrained by existing floats.
    ASSERT(layoutBox.isFloatAvoider());
    if (floatingContext.isEmpty())
        return { };
    // Float clear pushes the block level box either below the floats, or just one side below but the other side could overlap.
    // What it means is that the used available width always matches the containing block's constraint.
    if (layoutBox.hasFloatClear())
        return { };

    ASSERT(layoutBox.establishesFormattingContext());
    // Vertical static position is not computed yet for this formatting context root, so let's just pre-compute it for now.
    precomputeVerticalPositionForBoxAndAncestors(layoutBox, constraintsPair);

    auto logicalTopInFormattingContextRootCoordinate = [&] (auto& floatAvoider) {
        auto top = geometryForBox(floatAvoider).logicalTop();
        for (auto* ancestor = &floatAvoider.containingBlock(); ancestor != &root(); ancestor = &ancestor->containingBlock())
            top += geometryForBox(*ancestor).logicalTop();
        return top;
    };

    auto floatConstraintsInContainingBlockCoordinate = [&] (auto floatConstraints) {
        if (!floatConstraints.left && !floatConstraints.right)
            return FloatingContext::Constraints { };
        auto offset = LayoutSize { };
        for (auto* ancestor = &layoutBox.containingBlock(); ancestor != &root(); ancestor = &ancestor->containingBlock())
            offset += toLayoutSize(geometryForBox(*ancestor).logicalTopLeft());
        if (floatConstraints.left)
            floatConstraints.left = PointInContextRoot { *floatConstraints.left - offset };
        if (floatConstraints.right)
            floatConstraints.right = PointInContextRoot { *floatConstraints.right - offset };
        return floatConstraints;
    };

    // FIXME: Check if the non-yet-computed height affects this computation - and whether we have to resolve it at a later point.
    auto logicalTop = logicalTopInFormattingContextRootCoordinate(layoutBox);
    auto constraints = floatConstraintsInContainingBlockCoordinate(floatingContext.constraints(logicalTop, logicalTop));
    if (!constraints.left && !constraints.right)
        return { };
    // Shrink the available space if the floats are actually intruding at this vertical position.
    auto availableWidth = constraintsPair.containingBlock.horizontal.logicalWidth;
    if (constraints.left)
        availableWidth -= constraints.left->x;
    if (constraints.right)
        availableWidth -= std::max(0_lu, constraintsPair.containingBlock.horizontal.logicalRight() - constraints.right->x);
    return availableWidth;
}

void BlockFormattingContext::placeInFlowPositionedChildren(const ContainerBox& containerBox, const HorizontalConstraints& horizontalConstraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Start: move in-flow positioned children -> parent: " << &containerBox);
    for (auto& childBox : childrenOfType<Box>(containerBox)) {
        if (!childBox.isInFlowPositioned())
            continue;
        auto positionOffset = geometry().inFlowPositionedPositionOffset(childBox, horizontalConstraints);
        formattingState().boxGeometry(childBox).move(positionOffset);
    }
    LOG_WITH_STREAM(FormattingContextLayout, stream << "End: move in-flow positioned children -> parent: " << &containerBox);
}

void BlockFormattingContext::computeStaticVerticalPosition(const Box& layoutBox, const VerticalConstraints& verticalConstraints)
{
    formattingState().boxGeometry(layoutBox).setLogicalTop(geometry().staticVerticalPosition(layoutBox, verticalConstraints));
}

void BlockFormattingContext::computeStaticHorizontalPosition(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    formattingState().boxGeometry(layoutBox).setLogicalLeft(geometry().staticHorizontalPosition(layoutBox, horizontalConstraints));
}

void BlockFormattingContext::precomputeVerticalPositionForBoxAndAncestors(const Box& layoutBox, const ConstraintsPair& constraintsPair)
{
    // In order to figure out whether a box should avoid a float, we need to know the final positions of both (ignore relative positioning for now).
    // In block formatting context the final position for a normal flow box includes
    // 1. the static position and
    // 2. the corresponding (non)collapsed margins.
    // Now the vertical margins are computed when all the descendants are finalized, because the margin values might be depending on the height of the box
    // (and the height might be based on the content).
    // So when we get to the point where we intersect the box with the float to decide if the box needs to move, we don't yet have the final vertical position.
    //
    // The idea here is that as long as we don't cross the block formatting context boundary, we should be able to pre-compute the final top position.
    // FIXME: we currently don't account for the "clear" property when computing the final position for an ancestor.
    for (auto* ancestor = &layoutBox; ancestor && ancestor != &root(); ancestor = &ancestor->containingBlock()) {
        auto constraintsForAncestor = [&] {
            auto& containingBlock = ancestor->containingBlock();
            return &containingBlock == &root() ? constraintsPair.formattingContextRoot : geometry().constraintsForInFlowContent(containingBlock);
        }();

        auto computedVerticalMargin = geometry().computedVerticalMargin(*ancestor, constraintsForAncestor.horizontal);
        auto usedNonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) };
        auto precomputedMarginBefore = marginCollapse().precomputedMarginBefore(*ancestor, usedNonCollapsedMargin);

        auto& boxGeometry = formattingState().boxGeometry(*ancestor);
        auto nonCollapsedValues = UsedVerticalMargin::NonCollapsedValues { precomputedMarginBefore.nonCollapsedValue, { } };
        auto collapsedValues = UsedVerticalMargin::CollapsedValues { precomputedMarginBefore.collapsedValue, { }, false };
        auto verticalMargin = UsedVerticalMargin { nonCollapsedValues, collapsedValues, { precomputedMarginBefore.positiveAndNegativeMarginBefore, { } } };

        formattingState().setUsedVerticalMargin(*ancestor, verticalMargin);
        boxGeometry.setVerticalMargin({ marginBefore(verticalMargin), marginAfter(verticalMargin) });
        boxGeometry.setLogicalTop(verticalPositionWithMargin(*ancestor, verticalMargin, constraintsForAncestor.vertical));
#if ASSERT_ENABLED
        setPrecomputedMarginBefore(*ancestor, precomputedMarginBefore);
        boxGeometry.setHasPrecomputedMarginBefore();
#endif
    }
}

void BlockFormattingContext::computePositionToAvoidFloats(const FloatingContext& floatingContext, const Box& layoutBox, const ConstraintsPair& constraintsPair)
{
    if (!layoutBox.isFloatAvoider())
        return;
    // In order to position a float avoider we need to know its vertical position relative to its formatting context root (and not just its containing block),
    // because all the already-placed floats (floats that we are trying to avoid here) in this BFC might belong
    // to a different set of containing blocks (but they all descendants of the BFC root).
    // However according to the BFC rules, at this point of the layout flow we don't yet have computed vertical positions for the ancestors.
    if (layoutBox.isFloatingPositioned()) {
        precomputeVerticalPositionForBoxAndAncestors(layoutBox, constraintsPair);
        formattingState().boxGeometry(layoutBox).setLogicalTopLeft(floatingContext.positionForFloat(layoutBox, constraintsPair.containingBlock.horizontal));
        return;
    }
    // Non-float positioned float avoiders (formatting context roots and clear boxes) should be fine unless there are floats in this context.
    if (floatingContext.isEmpty())
        return;
    precomputeVerticalPositionForBoxAndAncestors(layoutBox, constraintsPair);
    if (layoutBox.hasFloatClear())
        return computeVerticalPositionForFloatClear(floatingContext, layoutBox);

    ASSERT(layoutBox.establishesFormattingContext());
    formattingState().boxGeometry(layoutBox).setLogicalTopLeft(floatingContext.positionForNonFloatingFloatAvoider(layoutBox, constraintsPair.containingBlock.horizontal));
}

void BlockFormattingContext::computeVerticalPositionForFloatClear(const FloatingContext& floatingContext, const Box& layoutBox)
{
    ASSERT(layoutBox.hasFloatClear());
    if (floatingContext.isEmpty())
        return;
    auto verticalPositionAndClearance = floatingContext.verticalPositionWithClearance(layoutBox);
    if (!verticalPositionAndClearance.position) {
        ASSERT(!verticalPositionAndClearance.clearance);
        return;
    }

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    ASSERT(*verticalPositionAndClearance.position >= boxGeometry.logicalTop());
    boxGeometry.setLogicalTop(*verticalPositionAndClearance.position);
    if (verticalPositionAndClearance.clearance)
        boxGeometry.setHasClearance();
    // FIXME: Reset the margin values on the ancestors/previous siblings now that the float avoider with clearance does not margin collapse anymore.
}

void BlockFormattingContext::computeWidthAndMargin(const FloatingContext& floatingContext, const Box& layoutBox, const ConstraintsPair& constraintsPair)
{
    auto availableWidthFloatAvoider = Optional<LayoutUnit> { };
    if (layoutBox.isFloatAvoider()) {
        // Float avoiders' available width might be shrunk by existing floats in the context.
        availableWidthFloatAvoider = usedAvailableWidthForFloatAvoider(floatingContext, layoutBox, constraintsPair);
    }
    auto contentWidthAndMargin = geometry().computedWidthAndMargin(layoutBox, constraintsPair.containingBlock.horizontal, availableWidthFloatAvoider);
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    boxGeometry.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void BlockFormattingContext::computeHeightAndMargin(const Box& layoutBox, const ConstraintsForInFlowContent& constraints)
{
    auto compute = [&](Optional<LayoutUnit> usedHeight) -> ContentHeightAndMargin {
        if (layoutBox.isInFlow())
            return geometry().inFlowHeightAndMargin(layoutBox, constraints.horizontal, { usedHeight });

        if (layoutBox.isFloatingPositioned())
            return geometry().floatingHeightAndMargin(layoutBox, constraints.horizontal, { usedHeight });

        ASSERT_NOT_REACHED();
        return { };
    };

    auto contentHeightAndMargin = compute({ });
    if (auto maxHeight = geometry().computedMaxHeight(layoutBox)) {
        if (contentHeightAndMargin.contentHeight > *maxHeight) {
            auto maxHeightAndMargin = compute(maxHeight);
            // Used height should remain the same.
            ASSERT((layoutState().inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || maxHeightAndMargin.contentHeight == *maxHeight);
            contentHeightAndMargin = { *maxHeight, maxHeightAndMargin.nonCollapsedMargin };
        }
    }

    if (auto minHeight = geometry().computedMinHeight(layoutBox)) {
        if (contentHeightAndMargin.contentHeight < *minHeight) {
            auto minHeightAndMargin = compute(minHeight);
            // Used height should remain the same.
            ASSERT((layoutState().inQuirksMode() && (layoutBox.isBodyBox() || layoutBox.isDocumentBox())) || minHeightAndMargin.contentHeight == *minHeight);
            contentHeightAndMargin = { *minHeight, minHeightAndMargin.nonCollapsedMargin };
        }
    }

    // 1. Compute collapsed margins.
    // 2. Adjust vertical position using the collapsed values.
    // 3. Adjust previous in-flow sibling margin after using this margin.
    auto marginCollapse = this->marginCollapse();
    auto verticalMargin = marginCollapse.collapsedVerticalValues(layoutBox, contentHeightAndMargin.nonCollapsedMargin);
    // Cache the computed positive and negative margin value pair.
    formattingState().setUsedVerticalMargin(layoutBox, verticalMargin);

#if ASSERT_ENABLED
    if (hasPrecomputedMarginBefore(layoutBox) && precomputedMarginBefore(layoutBox).usedValue() != marginBefore(verticalMargin)) {
        // When the pre-computed margin turns out to be incorrect, we need to re-layout this subtree with the correct margin values.
        // <div style="float: left"></div>
        // <div>
        //   <div style="margin-bottom: 200px"></div>
        // </div>
        // The float box triggers margin before computation on the ancestor chain to be able to intersect with other floats in the same floating context.
        // However in some cases the parent margin-top collapses with some next siblings (nephews) and there's no way to be able to properly
        // account for that without laying out every node in the FC (in the example, the margin-bottom pushes down the float).
        ASSERT_NOT_IMPLEMENTED_YET();
    }
#endif
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    if (!layoutBox.isFloatAvoider()) {
        // Float avoiders have pre-computed vertical margins.
        boxGeometry.setLogicalTop(verticalPositionWithMargin(layoutBox, verticalMargin, constraints.vertical));
    }
    boxGeometry.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    boxGeometry.setVerticalMargin({ marginBefore(verticalMargin), marginAfter(verticalMargin) });
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
    auto maximumHorizontalStackingWidth = LayoutUnit { };
    auto currentHorizontalStackingWidth = LayoutUnit { };
    while (!queue.isEmpty()) {
        while (true) {
            // Check if we have to deal with descendant content.
            auto& layoutBox = *queue.last();
            // Float avoiders are all establish a new formatting context. No need to look inside them.
            if (layoutBox.isFloatAvoider() && !layoutBox.hasFloatClear())
                break;
            // Non-floating block level boxes reset the current horizontal float stacking.
            // SPEC: This is a bit odd as floating positioning is a formatting context level concept:
            // e.g.
            // <div style="float: left; width: 10px;"></div>
            // <div></div>
            // <div style="float: left; width: 40px;"></div>
            // ...will produce a max width of 40px which makes the floats vertically stacked.
            // Vertically stacked floats makes me think we haven't managed to provide the maximum preferred width for the content.
            maximumHorizontalStackingWidth = std::max(currentHorizontalStackingWidth, maximumHorizontalStackingWidth);
            currentHorizontalStackingWidth = { };
            // Already has computed intrinsic constraints.
            if (formattingState.intrinsicWidthConstraintsForBox(layoutBox))
                break;
            // Box with fixed width defines their descendant content intrinsic width.
            if (layoutBox.style().width().isFixed())
                break;
            // Non-float avoider formatting context roots are opaque to intrinsic width computation.
            if (layoutBox.establishesFormattingContext())
                break;
            // No relevant child content.
            if (!is<ContainerBox>(layoutBox) || !downcast<ContainerBox>(layoutBox).hasInFlowOrFloatingChild())
                break;
            queue.append(downcast<ContainerBox>(layoutBox).firstInFlowOrFloatingChild());
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
            auto willStackHorizontally = layoutBox.isFloatAvoider() && !layoutBox.hasFloatClear();
            if (willStackHorizontally)
                currentHorizontalStackingWidth += desdendantConstraints->maximum;
            else
                constraints.maximum = std::max(constraints.maximum, desdendantConstraints->maximum);
            // Move over to the next sibling or take the next box in the queue.
            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                queue.append(nextSibling);
                break;
            }
        }
    }
    maximumHorizontalStackingWidth = std::max(currentHorizontalStackingWidth, maximumHorizontalStackingWidth);
    constraints.maximum = std::max(constraints.maximum, maximumHorizontalStackingWidth);
    formattingState.setIntrinsicWidthConstraints(constraints);
    return constraints;
}

LayoutUnit BlockFormattingContext::verticalPositionWithMargin(const Box& layoutBox, const UsedVerticalMargin& verticalMargin,  const VerticalConstraints& verticalConstraints) const
{
    ASSERT(!layoutBox.isOutOfFlowPositioned());
    // Now that we've computed the final margin before, let's shift the box's vertical position if needed.
    // 1. Check if the box has clearance. If so, we've already precomputed/finalized the top value and vertical margin does not impact it anymore.
    // 2. Check if the margin before collapses with the previous box's margin after. if not -> return previous box's bottom including margin after + marginBefore
    // 3. Check if the previous box's margins collapse through. If not -> return previous box' bottom excluding margin after + marginBefore (they are supposed to be equal)
    // 4. Go to previous box and start from step #1 until we hit the parent box.
    auto& boxGeometry = geometryForBox(layoutBox);
    if (boxGeometry.hasClearance())
        return boxGeometry.logicalTop();

    auto* currentLayoutBox = &layoutBox;
    while (currentLayoutBox) {
        if (!currentLayoutBox->previousInFlowSibling())
            break;
        auto& previousInFlowSibling = *currentLayoutBox->previousInFlowSibling();
        if (!marginCollapse().marginBeforeCollapsesWithPreviousSiblingMarginAfter(*currentLayoutBox)) {
            auto& previousBoxGeometry = geometryForBox(previousInFlowSibling);
            return previousBoxGeometry.logicalRectWithMargin().bottom() + marginBefore(verticalMargin);
        }

        if (!marginCollapse().marginsCollapseThrough(previousInFlowSibling)) {
            auto& previousBoxGeometry = geometryForBox(previousInFlowSibling);
            return previousBoxGeometry.logicalBottom() + marginBefore(verticalMargin);
        }
        currentLayoutBox = &previousInFlowSibling;
    }

    auto containingBlockContentBoxTop = verticalConstraints.logicalTop;
    // Adjust vertical position depending whether this box directly or indirectly adjoins with its parent.
    auto directlyAdjoinsParent = !layoutBox.previousInFlowSibling();
    if (directlyAdjoinsParent) {
        // If the top and bottom margins of a box are adjoining, then it is possible for margins to collapse through it.
        // In this case, the position of the element depends on its relationship with the other elements whose margins are being collapsed.
        if (verticalMargin.collapsedValues.isCollapsedThrough) {
            // If the element's margins are collapsed with its parent's top margin, the top border edge of the box is defined to be the same as the parent's.
            if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(layoutBox))
                return containingBlockContentBoxTop;
            // Otherwise, either the element's parent is not taking part in the margin collapsing, or only the parent's bottom margin is involved.
            // The position of the element's top border edge is the same as it would have been if the element had a non-zero bottom border.
            auto beforeMarginWithBottomBorder = marginCollapse().marginBeforeIgnoringCollapsingThrough(layoutBox, verticalMargin.nonCollapsedValues);
            return containingBlockContentBoxTop + beforeMarginWithBottomBorder;
        }
        // Non-collapsed through box vertical position depending whether the margin collapses.
        if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(layoutBox))
            return containingBlockContentBoxTop;

        return containingBlockContentBoxTop + marginBefore(verticalMargin);
    }
    // At this point this box indirectly (via collapsed through previous in-flow siblings) adjoins the parent. Let's check if it margin collapses with the parent.
    auto& containingBlock = layoutBox.containingBlock();
    ASSERT(containingBlock.firstInFlowChild());
    ASSERT(containingBlock.firstInFlowChild() != &layoutBox);
    if (marginCollapse().marginBeforeCollapsesWithParentMarginBefore(*containingBlock.firstInFlowChild()))
        return containingBlockContentBoxTop;

    return containingBlockContentBoxTop + marginBefore(verticalMargin);
}

}
}

#endif

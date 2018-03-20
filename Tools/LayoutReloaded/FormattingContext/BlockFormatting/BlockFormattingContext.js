/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
class BlockFormattingContext extends FormattingContext {
    constructor(root) {
        super(root);
        // New block formatting context always establishes a new floating context.
        this.m_floatingContext = new FloatingContext(this);
    }

    layout(layoutContext) {
        // 9.4.1 Block formatting contexts
        // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
        // The vertical distance between two sibling boxes is determined by the 'margin' properties.
        // Vertical margins between adjacent block-level boxes in a block formatting context collapse.

        // This is a post-order tree traversal layout.
        // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
        if (this.rootContainer().firstInFlowOrFloatChild())
            this._addToLayoutQueue(this.rootContainer().firstInFlowOrFloatChild());
        // 1. Go all the way down to the leaf node
        // 2. Compute static position and width as we travers down
        // 3. As we climb back on the tree, compute height and finialize position
        // (Any subtrees with new formatting contexts need to layout synchronously)
        while (this._descendantNeedsLayout()) {
            // Travers down on the descendants until we find a leaf node.
            while (true) {
                let layoutBox = this._nextInLayoutQueue();
                this.computeWidth(layoutBox);
                this._computeStaticPosition(layoutBox);
                if (layoutBox.establishesFormattingContext()) {
                    layoutContext.layoutFormattingContext(layoutBox.establishedFormattingContext());
                    break;
                }
                if (!layoutBox.isContainer() || !layoutBox.hasInFlowOrFloatChild())
                    break;
                this._addToLayoutQueue(layoutBox.firstInFlowOrFloatChild());
            }

            // Climb back on the ancestors and compute height/final position.
            while (this._descendantNeedsLayout()) {
                // All inflow descendants (if there are any) are laid out by now. Let's compute the box's height.
                let layoutBox = this._nextInLayoutQueue();
                this.computeHeight(layoutBox);
                // Adjust position now that we have all the previous floats placed in this context -if needed.
                this.floatingContext().computePosition(layoutBox);
                // Move in-flow positioned children to their final position.
                this._placeInFlowPositionedChildren(layoutBox);
                // We are done with laying out this box.
                this._removeFromLayoutQueue(layoutBox);
                if (layoutBox.nextInFlowOrFloatSibling()) {
                    this._addToLayoutQueue(layoutBox.nextInFlowOrFloatSibling());
                    break;
                }
            }
        }
        // Place the inflow positioned children.
        this._placeInFlowPositionedChildren(this.rootContainer());
        // And take care of out-of-flow boxes as the final step.
        this._layoutOutOfFlowDescendants(layoutContext);
   }

    computeWidth(layoutBox) {
        if (layoutBox.isOutOfFlowPositioned())
            return this._computeOutOfFlowWidth(layoutBox);
        if (layoutBox.isFloatingPositioned())
            return this._computeFloatingWidth(layoutBox);
        return this._computeInFlowWidth(layoutBox);
    }

    computeHeight(layoutBox) {
        if (layoutBox.isOutOfFlowPositioned())
            return this._computeOutOfFlowHeight(layoutBox);
        if (layoutBox.isFloatingPositioned())
            return this._computeFloatingHeight(layoutBox);
        return this._computeInFlowHeight(layoutBox);
    }

    marginTop(layoutBox) {
        return BlockMarginCollapse.marginTop(layoutBox);
    }

    marginBottom(layoutBox) {
        return BlockMarginCollapse.marginBottom(layoutBox);
    }

    _computeStaticPosition(layoutBox) {
        // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
        // The vertical distance between two sibling boxes is determined by the 'margin' properties.
        // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
        let containingBlockContentBox = this.toDisplayBox(layoutBox.containingBlock()).contentBox();
        // Start from the top of the container's content box.
        let previousInFlowSibling = layoutBox.previousInFlowSibling();
        let contentBottom = containingBlockContentBox.top()
        if (previousInFlowSibling)
            contentBottom = this.toDisplayBox(previousInFlowSibling).bottom() + this.marginBottom(previousInFlowSibling);
        let position = new LayoutPoint(contentBottom, containingBlockContentBox.left());
        position.moveBy(new LayoutSize(this.marginLeft(layoutBox), this.marginTop(layoutBox)));
        this.toDisplayBox(layoutBox).setTopLeft(position);
    }

    _placeInFlowPositionedChildren(container) {
        if (!container.isContainer())
            return;
        // If this layoutBox also establishes a formatting context, then positioning already has happend at the formatting context.
        if (container.establishesFormattingContext() && container != this.rootContainer())
            return;
        ASSERT(container.isContainer());
        for (let inFlowChild = container.firstInFlowChild(); inFlowChild; inFlowChild = inFlowChild.nextInFlowSibling()) {
            if (!inFlowChild.isInFlowPositioned())
                continue;
            this._computeInFlowPositionedPosition(inFlowChild);
        }
    }

    _layoutOutOfFlowDescendants(layoutContext) {
        // This lays out all the out-of-flow boxes that belong to this formatting context even if
        // the root container is not the containing block.
        let outOfFlowDescendants = this._outOfFlowDescendants();
        for (let outOfFlowBox of outOfFlowDescendants) {
            this._addToLayoutQueue(outOfFlowBox);
            this.computeWidth(outOfFlowBox);
            layoutContext.layoutFormattingContext(outOfFlowBox.establishedFormattingContext());
            this.computeHeight(outOfFlowBox);
            this._computeOutOfFlowPosition(outOfFlowBox);
            this._removeFromLayoutQueue(outOfFlowBox);
        }
    }

    _computeOutOfFlowWidth(layoutBox) {
        // 10.3.7 Absolutely positioned, non-replaced elements

        // 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the width is shrink-to-fit. Then solve for 'left'
        // 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if the 'direction' property of the element establishing
        //     the static-position containing block is 'ltr' set 'left' to the static position, otherwise set 'right' to the static position.
        //     Then solve for 'left' (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
        // 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the width is shrink-to-fit . Then solve for 'right'
        // 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
        // 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
        // 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'
        let width = Number.NaN;
        if (Utils.isWidthAuto(layoutBox) && Utils.isLeftAuto(layoutBox) && Utils.isRightAuto(layoutBox))
            width = this._shrinkToFitWidth(layoutBox);
        else if (Utils.isLeftAuto(layoutBox) && Utils.isWidthAuto(layoutBox) && !Utils.isRightAuto(layoutBox))
            width = this._shrinkToFitWidth(layoutBox); // 1
        else if (Utils.isLeftAuto(layoutBox) && Utils.isRightAuto(layoutBox) && !Utils.isWidthAuto(layoutBox))
            width = Utils.width(layoutBox); // 2
        else if (Utils.isWidthAuto(layoutBox) && Utils.isRightAuto(layoutBox) && !Utils.isLeftAuto(layoutBox))
            width = this._shrinkToFitWidth(layoutBox); // 3
        else if (Utils.isLeftAuto(layoutBox) && !Utils.isWidthAuto(layoutBox) && !Utils.isRightAuto(layoutBox))
            width = Utils.width(layoutBox); // 4
        else if (Utils.isWidthAuto(layoutBox) && !Utils.isLeftAuto(layoutBox) && !Utils.isRightAuto(layoutBox))
            width = Math.max(0, this.toDisplayBox(layoutBox.containingBlock()).contentBox().width() - Utils.right(layoutBox) - Utils.left(layoutBox)); // 5
        else if (Utils.isRightAuto(layoutBox) && !Utils.isLeftAuto(layoutBox) && !Utils.isWidthAuto(layoutBox))
            width = Utils.width(layoutBox); // 6
        else
            ASSERT_NOT_REACHED();
        width += Utils.computedHorizontalBorderAndPadding(layoutBox.node());
        this.toDisplayBox(layoutBox).setWidth(width);
    }

    _computeFloatingWidth(layoutBox) {
        // FIXME: missing cases
        this.toDisplayBox(layoutBox).setWidth(Utils.width(layoutBox) + Utils.computedHorizontalBorderAndPadding(layoutBox.node()));
    }

    _computeInFlowWidth(layoutBox) {
        if (Utils.isWidthAuto(layoutBox))
            return this.toDisplayBox(layoutBox).setWidth(this._horizontalConstraint(layoutBox));
        return this.toDisplayBox(layoutBox).setWidth(Utils.width(layoutBox) + Utils.computedHorizontalBorderAndPadding(layoutBox.node()));
    }

    _computeOutOfFlowHeight(layoutBox) {
        // 1. If all three of 'top', 'height', and 'bottom' are auto, set 'top' to the static position and apply rule number three below.
        // 2. If none of the three are 'auto': If both 'margin-top' and 'margin-bottom' are 'auto', solve the equation under
        //    the extra constraint that the two margins get equal values. If one of 'margin-top' or 'margin-bottom' is 'auto',
        //    solve the equation for that value. If the values are over-constrained, ignore the value for 'bottom' and solve for that value.
        // Otherwise, pick the one of the following six rules that applies.

        // 3. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then the height is based on the content per 10.6.7,
        //    set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
        // 4. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then set 'top' to the static position, set 'auto' values
        //    for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
        // 5. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then the height is based on the content per 10.6.7,
        //    set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
        // 6. 'top' is 'auto', 'height' and 'bottom' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
        // 7. 'height' is 'auto', 'top' and 'bottom' are not 'auto', then 'auto' values for 'margin-top' and 'margin-bottom' are set to 0 and solve for 'height'
        // 8. 'bottom' is 'auto', 'top' and 'height' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0 and solve for 'bottom'
        let height = Number.NaN;
        if (Utils.isHeightAuto(layoutBox) && Utils.isBottomAuto(layoutBox) && Utils.isTopAuto(layoutBox))
            height = this._contentHeight(layoutBox); // 1
        else if (Utils.isTopAuto((layoutBox)) && Utils.isHeightAuto((layoutBox)) && !Utils.isBottomAuto((layoutBox)))
            height = this._contentHeight(layoutBox); // 3
        else if (Utils.isTopAuto((layoutBox)) && Utils.isBottomAuto((layoutBox)) && !Utils.isHeightAuto((layoutBox)))
            height = Utils.height(layoutBox); // 4
        else if (Utils.isHeightAuto((layoutBox)) && Utils.isBottomAuto((layoutBox)) && !Utils.isTopAuto((layoutBox)))
            height = this._contentHeight(layoutBox); // 5
        else if (Utils.isTopAuto((layoutBox)) && !Utils.isHeightAuto((layoutBox)) && !Utils.isBottomAuto((layoutBox)))
            height = Utils.height(layoutBox); // 6
        else if (Utils.isHeightAuto((layoutBox)) && !Utils.isTopAuto((layoutBox)) && !Utils.isBottomAuto((layoutBox)))
            height = Math.max(0, this.toDisplayBox(layoutBox.containingBlock()).contentBox().height() - Utils.bottom(layoutBox) - Utils.top(layoutBox)); // 7
        else if (Utils.isBottomAuto((layoutBox)) && !Utils.isTopAuto((layoutBox)) && !Utils.isHeightAuto((layoutBox)))
            height = Utils.height(layoutBox); // 8
        else
            ASSERT_NOT_REACHED();
        height += Utils.computedVerticalBorderAndPadding(layoutBox.node());
        this.toDisplayBox(layoutBox).setHeight(height);
    }

    _computeFloatingHeight(layoutBox) {
        // FIXME: missing cases
        this.toDisplayBox(layoutBox).setHeight(Utils.height(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
    }

    _computeInFlowHeight(layoutBox) {
        if (Utils.isHeightAuto(layoutBox)) {
            // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
            // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.

            // The element's height is the distance from its top content edge to the first applicable of the following:
            // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
            return this.toDisplayBox(layoutBox).setHeight(this._contentHeight(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
        }
        return this.toDisplayBox(layoutBox).setHeight(Utils.height(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
    }

    _horizontalConstraint(layoutBox) {
        let horizontalConstraint = this.toDisplayBox(layoutBox.containingBlock()).contentBox().width();
        horizontalConstraint -= this.marginLeft(layoutBox) + this.marginRight(layoutBox);
        return horizontalConstraint;
    }

    _contentHeight(layoutBox) {
        // 10.6.3 Block-level non-replaced elements in normal flow when 'overflow' computes to 'visible'
        // The element's height is the distance from its top content edge to the first applicable of the following:
        // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
        // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin does not collapse
        //    with the element's bottom margin
        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        // 4. zero, otherwise
        // Only children in the normal flow are taken into account.
        if (!layoutBox.isContainer() || !layoutBox.hasInFlowChild())
            return 0;
        if (layoutBox.establishesInlineFormattingContext()) {
            ASSERT(layoutBox.establishedFormattingContext());
            let lines = layoutBox.establishedFormattingContext().lines();
            if (!lines.length)
                return 0;
            let lastLine = lines[lines.length - 1];
            return lastLine.rect().bottom();
        }
        let top = this.toDisplayBox(layoutBox).contentBox().top();
        let bottom = this._adjustBottomWithFIXME(layoutBox);
        return bottom - top;
    }

    _adjustBottomWithFIXME(layoutBox) {
        // FIXME: This function is a big FIXME.
        let lastInFlowChild = layoutBox.lastInFlowChild();
        let lastInFlowDisplayBox = lastInFlowChild.displayBox();
        let bottom = lastInFlowDisplayBox.bottom() + this.marginBottom(lastInFlowChild);
        // FIXME: margin for body
        if (lastInFlowChild.name() == "RenderBody" && Utils.isHeightAuto(lastInFlowChild) && !this.toDisplayBox(lastInFlowChild).contentBox().height())
            bottom -= this.marginBottom(lastInFlowChild);
        // FIXME: figure out why floatings part of the initial block formatting context get propagated to HTML
        if (layoutBox.node().tagName == "HTML") {
            let floatingBottom = this.floatingContext().bottom();
            if (!Number.isNaN(floatingBottom))
                bottom = Math.max(floatingBottom, bottom);
        }
        return bottom;
    }

    _computeInFlowPositionedPosition(layoutBox) {
        // Start with the original, static position.
        let displayBox = this.toDisplayBox(layoutBox);
        let relativePosition = displayBox.topLeft();
        // Top/bottom
        if (!Utils.isTopAuto(layoutBox))
            relativePosition.shiftTop(Utils.top(layoutBox));
        else if (!Utils.isBottomAuto(layoutBox))
            relativePosition.shiftTop(-Utils.bottom(layoutBox));
        // Left/right
        if (!Utils.isLeftAuto(layoutBox))
            relativePosition.shiftLeft(Utils.left(layoutBox));
        else if (!Utils.isRightAuto(layoutBox))
            relativePosition.shiftLeft(-Utils.right(layoutBox));
        displayBox.setTopLeft(relativePosition);
    }

    _computeOutOfFlowPosition(layoutBox) {
        let displayBox = this.toDisplayBox(layoutBox);
        let top = Number.NaN;
        let containerSize = this.toDisplayBox(layoutBox.containingBlock()).contentBox().size();
        // Top/bottom
        if (Utils.isTopAuto(layoutBox) && Utils.isBottomAuto(layoutBox)) {
            ASSERT(Utils.isStaticallyPositioned(layoutBox));
            // Vertically statically positioned.
            // FIXME: Figure out if it is actually valid that we use the parent box as the container (which is not even in this formatting context).
            let parent = layoutBox.parent();
            let parentDisplayBox = parent.displayBox();
            let previousInFlowSibling = layoutBox.previousInFlowSibling();
            let contentBottom = previousInFlowSibling ? previousInFlowSibling.displayBox().bottom() : parentDisplayBox.contentBox().top();
            top = contentBottom + this.marginTop(layoutBox);
            // Convert static position (in parent coordinate system) to absolute (in containing block coordindate system).
            if (parent != layoutBox.containingBlock())
                top += this._toAbsolutePosition(parentDisplayBox.topLeft(), parent, layoutBox.containingBlock()).top();
        } else if (!Utils.isTopAuto(layoutBox))
            top = Utils.top(layoutBox) + this.marginTop(layoutBox);
        else if (!Utils.isBottomAuto(layoutBox))
            top = containerSize.height() - Utils.bottom(layoutBox) - displayBox.height() - this.marginBottom(layoutBox);
        else
            ASSERT_NOT_REACHED();
        // Left/right
        let left = Number.NaN;
        if (Utils.isLeftAuto(layoutBox) && Utils.isRightAuto(layoutBox)) {
            ASSERT(Utils.isStaticallyPositioned(layoutBox));
            // Horizontally statically positioned.
            // FIXME: Figure out if it is actually valid that we use the parent box as the container (which is not even in this formatting context).
            let parent = layoutBox.parent();
            let parentDisplayBox = parent.displayBox();
            left = parentDisplayBox.contentBox().left() + this.marginLeft(layoutBox);
            // Convert static position (in parent coordinate system) to absolute (in containing block coordindate system).
            if (parent != layoutBox.containingBlock())
                left += this._toAbsolutePosition(parentDisplayBox.rect(), parent, layoutBox.containingBlock()).left();
        } else if (!Utils.isLeftAuto(layoutBox))
            left = Utils.left(layoutBox) + this.marginLeft(layoutBox);
        else if (!Utils.isRightAuto(layoutBox))
            left = containerSize.width() - Utils.right(layoutBox) - displayBox.width() - this.marginRight(layoutBox);
        else
            ASSERT_NOT_REACHED();
        displayBox.setTopLeft(new LayoutPoint(top, left));
    }

    _shrinkToFitWidth(layoutBox) {
        // FIXME: this is naive.
        ASSERT(Utils.isWidthAuto(layoutBox));
        if (!layoutBox.isContainer() || !layoutBox.hasChild())
            return 0;
        let width = 0;
        for (let inFlowChild = layoutBox.firstInFlowChild(); inFlowChild; inFlowChild = inFlowChild.nextInFlowSibling()) {
            let widthCandidate = Utils.isWidthAuto(inFlowChild) ? this._shrinkToFitWidth(inFlowChild) : Utils.width(inFlowChild);
            width = Math.max(width, widthCandidate + Utils.computedHorizontalBorderAndPadding(inFlowChild.node()));
        }
        return width;
    }
}

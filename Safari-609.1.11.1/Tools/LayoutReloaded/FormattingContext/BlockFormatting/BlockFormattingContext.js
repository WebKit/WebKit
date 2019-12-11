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

/*
class BlockFormattingContext : public FormattingContext {
public:
    void layout() override;

    void computeWidth(const Layout::Box&) override;
    void computeHeight(const Layout::Box&) override;
 
    void marginTop(const Layout::Box&) override;
    void marginBottom(const Layout::Box&) override;

private:
    void computeStaticPosition(const Layout::Box&);
    void computeInFlowHeight(const Layout::Box&);
    void horizontalConstraint(const Layout::Box&);
    void contentHeight(const Layout::Box&);
};
*/

class BlockFormattingContext extends FormattingContext {
    constructor(blockFormattingState) {
        super(blockFormattingState);
    }

    layout() {
        // 9.4.1 Block formatting contexts
        // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
        // The vertical distance between two sibling boxes is determined by the 'margin' properties.
        // Vertical margins between adjacent block-level boxes in a block formatting context collapse.

        // This is a post-order tree traversal layout.
        // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
        this._addToLayoutQueue(this._firstInFlowChildWithNeedsLayout(this.formattingRoot()));
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
                    this.layoutState().formattingContext(layoutBox).layout();
                    break;
                }
                let childToLayout = this._firstInFlowChildWithNeedsLayout(layoutBox);
                if (!childToLayout)
                    break;
                this._addToLayoutQueue(childToLayout);
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
                this.formattingState().clearNeedsLayout(layoutBox);
                let nextSiblingToLayout = this._nextInFlowSiblingWithNeedsLayout(layoutBox);
                if (nextSiblingToLayout) {
                    this._addToLayoutQueue(nextSiblingToLayout);
                    break;
                }
            }
        }
        // Place the inflow positioned children.
        this._placeInFlowPositionedChildren(this.formattingRoot());
        // And take care of out-of-flow boxes as the final step.
        this._layoutOutOfFlowDescendants();
        ASSERT(!this.formattingState().layoutNeeded());
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
        let containingBlockContentBox = this.displayBox(layoutBox.containingBlock()).contentBox();
        // Start from the top of the container's content box.
        let previousInFlowSibling = layoutBox.previousInFlowSibling();
        let contentBottom = containingBlockContentBox.top()
        if (previousInFlowSibling)
            contentBottom = this.displayBox(previousInFlowSibling).bottom() + this.marginBottom(previousInFlowSibling);
        let position = new LayoutPoint(contentBottom, containingBlockContentBox.left());
        position.moveBy(new LayoutSize(this.marginLeft(layoutBox), this.marginTop(layoutBox)));
        this.displayBox(layoutBox).setTopLeft(position);
    }

    _computeInFlowHeight(layoutBox) {
        if (Utils.isHeightAuto(layoutBox)) {
            // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
            // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.

            // The element's height is the distance from its top content edge to the first applicable of the following:
            // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
            return this.displayBox(layoutBox).setHeight(this._contentHeight(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
        }
        return this.displayBox(layoutBox).setHeight(Utils.height(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
    }

    _horizontalConstraint(layoutBox) {
        let horizontalConstraint = this.displayBox(layoutBox.containingBlock()).contentBox().width();
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
            let lines = this.layoutState().establishedFormattingState(layoutBox).lines();
            if (!lines.length)
                return 0;
            let lastLine = lines[lines.length - 1];
            return lastLine.rect().bottom();
        }
        let top = this.displayBox(layoutBox).contentBox().top();
        let bottom = this._adjustBottomWithFIXME(layoutBox);
        return bottom - top;
    }

    _adjustBottomWithFIXME(layoutBox) {
        // FIXME: This function is a big FIXME.
        let lastInFlowChild = layoutBox.lastInFlowChild();
        let lastInFlowDisplayBox = this.displayBox(lastInFlowChild);
        let bottom = lastInFlowDisplayBox.bottom() + this.marginBottom(lastInFlowChild);
        // FIXME: margin for body
        if (lastInFlowChild.name() == "RenderBody" && Utils.isHeightAuto(lastInFlowChild) && !this.displayBox(lastInFlowChild).contentBox().height())
            bottom -= this.marginBottom(lastInFlowChild);
        // FIXME: figure out why floatings part of the initial block formatting context get propagated to HTML
        if (layoutBox.node().tagName == "HTML") {
            let floatingBottom = this.floatingContext().bottom();
            if (!Number.isNaN(floatingBottom))
                bottom = Math.max(floatingBottom, bottom);
        }
        return bottom;
    }
}

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
class FormattingContext {
public:
    Layout::Container& formattingRoot();
    FormattingState& formattingState();
    LayoutState& layoutState();
    FloatingContext& floatingContext();

    virtual void layout();

    virtual void computeWidth(const Layout::Box&);
    virtual void computeHeight(const Layout::Box&);

    virtual LayoutUnit marginTop(const Layout::Box&);
    virtual LayoutUnit marginLeft(const Layout::Box&);
    virtual LayoutUnit marginBottom(const Layout::Box&);
    virtual LayoutUnit marginRight(const Layout::Box&);

private:
    void computeFloatingWidth(const Layout::Box&);
    void computeFloatingHeight(const Layout::Box&);

    void placeInFlowPositionedChildren(const Layout::Box&);
    void computeInFlowPositionedPosition(const Layout::Box&);
    void computeInFlowWidth(const Layout::Box&);

    void layoutOutOfFlowDescendants();

    void computeOutOfFlowWidth(const Layout::Box&);
    void computeOutOfFlowHeight(const Layout::Box&);
    void computeOutOfFlowPosition(const Layout::Box&);

    LayoutUnit shrinkToFitWidth(Layout::Box&);
};
*/
class FormattingContext {
    constructor(formattingState) {
        this.m_formattingState = formattingState;
        this.m_floatingContext = new FloatingContext(formattingState.floatingState());
        this.m_layoutStack = new Array();
    }

    formattingRoot() {
        return this.m_formattingState.formattingRoot();
    }

    formattingState() {
        return this.m_formattingState;
    }

    layoutState() {
        return this.formattingState().layoutState();
    }

    floatingContext() {
        return this.m_floatingContext;
    }

    layout() {
        ASSERT_NOT_REACHED();
    }

    computeWidth(layoutBox) {
    }

    computeHeight(layoutBox) {
    }

    marginTop(layoutBox) {
        return Utils.computedMarginTop(layoutBox.node());
    }
    
    marginLeft(layoutBox) {
        return Utils.computedMarginLeft(layoutBox.node());
    }
    
    marginBottom(layoutBox) {
        return Utils.computedMarginBottom(layoutBox.node());
    }
    
    marginRight(layoutBox) {
        return Utils.computedMarginRight(layoutBox.node());
    }

    static isInFormattingContext(layoutBox, formattingContextRoot) {
        ASSERT(formattingContextRoot.establishesFormattingContext());
        // If we hit the "this" while climbing up on the containing block chain and we don't pass a formatting context root -> box is part of this box's formatting context.
        for (let containingBlock = layoutBox.containingBlock(); containingBlock; containingBlock = containingBlock.containingBlock()) {
            if (containingBlock == formattingContextRoot)
                return true;
            if (containingBlock.establishesFormattingContext())
                return false;
        }
        return false;
    }

    _descendantNeedsLayout() {
        return this.m_layoutStack.length;
    }

    _addToLayoutQueue(layoutBox) {
        if (!layoutBox)
            return;
        // Initialize the corresponding display box.
        let displayBox = this.formattingState().createDisplayBox(layoutBox, this);
        if (layoutBox.node()) {
            displayBox.setMarginTop(this.marginTop(layoutBox));
            displayBox.setMarginLeft(this.marginLeft(layoutBox));
            displayBox.setMarginBottom(this.marginBottom(layoutBox));
            displayBox.setMarginRight(this.marginRight(layoutBox));
        }

        this.m_layoutStack.push(layoutBox);
    }

    _nextInLayoutQueue() {
        ASSERT(this.m_layoutStack.length);
        return this.m_layoutStack[this.m_layoutStack.length - 1];
    }

    _removeFromLayoutQueue(layoutBox) {
        // With the current layout logic, the layoutBox should be at the top (this.m_layoutStack.pop() should do).
        ASSERT(this.m_layoutStack.length);
        ASSERT(this.m_layoutStack[this.m_layoutStack.length - 1] == layoutBox);
        this.m_layoutStack.splice(this.m_layoutStack.indexOf(layoutBox), 1);
    }

    displayBox(layoutBox) {
        return this.formattingState().displayBox(layoutBox);
    }

    _computeFloatingWidth(layoutBox) {
        // FIXME: missing cases
        this.displayBox(layoutBox).setWidth(Utils.width(layoutBox) + Utils.computedHorizontalBorderAndPadding(layoutBox.node()));
    }

    _computeFloatingHeight(layoutBox) {
        // FIXME: missing cases
        this.displayBox(layoutBox).setHeight(Utils.height(layoutBox) + Utils.computedVerticalBorderAndPadding(layoutBox.node()));
    }

    _placeInFlowPositionedChildren(container) {
        if (!container.isContainer())
            return;
        // If this layoutBox also establishes a formatting context, then positioning already has happend at the formatting context.
        if (container.establishesFormattingContext() && container != this.formattingRoot())
            return;
        ASSERT(container.isContainer());
        for (let inFlowChild = container.firstInFlowChild(); inFlowChild; inFlowChild = inFlowChild.nextInFlowSibling()) {
            if (!inFlowChild.isInFlowPositioned())
                continue;
            this._computeInFlowPositionedPosition(inFlowChild);
        }
    }

    _computeInFlowPositionedPosition(layoutBox) {
        // Start with the original, static position.
        let displayBox = this.displayBox(layoutBox);
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

    _computeInFlowWidth(layoutBox) {
        if (Utils.isWidthAuto(layoutBox))
            return this.displayBox(layoutBox).setWidth(this._horizontalConstraint(layoutBox));
        return this.displayBox(layoutBox).setWidth(Utils.width(layoutBox) + Utils.computedHorizontalBorderAndPadding(layoutBox.node()));
    }

    _layoutOutOfFlowDescendants() {
        // This lays out all the out-of-flow boxes that belong to this formatting context even if
        // the root container is not the containing block.
        let outOfFlowDescendants = this._outOfFlowDescendants();
        for (let outOfFlowBox of outOfFlowDescendants) {
            this._addToLayoutQueue(outOfFlowBox);
            this._computeOutOfFlowWidth(outOfFlowBox);
            this.layoutState().formattingContext(outOfFlowBox).layout();
            this._computeOutOfFlowHeight(outOfFlowBox);
            this._computeOutOfFlowPosition(outOfFlowBox);
            this._removeFromLayoutQueue(outOfFlowBox);
            this.formattingState().clearNeedsLayout(outOfFlowBox);
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
            width = Math.max(0, this.displayBox(layoutBox.containingBlock()).contentBox().width() - Utils.right(layoutBox) - Utils.left(layoutBox)); // 5
        else if (Utils.isRightAuto(layoutBox) && !Utils.isLeftAuto(layoutBox) && !Utils.isWidthAuto(layoutBox))
            width = Utils.width(layoutBox); // 6
        else
            ASSERT_NOT_REACHED();
        width += Utils.computedHorizontalBorderAndPadding(layoutBox.node());
        this.displayBox(layoutBox).setWidth(width);
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
            height = Math.max(0, this.displayBox(layoutBox.containingBlock()).contentBox().height() - Utils.bottom(layoutBox) - Utils.top(layoutBox)); // 7
        else if (Utils.isBottomAuto((layoutBox)) && !Utils.isTopAuto((layoutBox)) && !Utils.isHeightAuto((layoutBox)))
            height = Utils.height(layoutBox); // 8
        else
            ASSERT_NOT_REACHED();
        height += Utils.computedVerticalBorderAndPadding(layoutBox.node());
        this.displayBox(layoutBox).setHeight(height);
    }

    _computeOutOfFlowPosition(layoutBox) {
        let displayBox = this.displayBox(layoutBox);
        let top = Number.NaN;
        let containerSize = this.displayBox(layoutBox.containingBlock()).contentBox().size();
        // Top/bottom
        if (Utils.isTopAuto(layoutBox) && Utils.isBottomAuto(layoutBox)) {
            ASSERT(Utils.isStaticallyPositioned(layoutBox));
            // Vertically statically positioned.
            // FIXME: Figure out if it is actually valid that we use the parent box as the container (which is not even in this formatting context).
            let parent = layoutBox.parent();
            let parentDisplayBox = this.displayBox(parent);
            let previousInFlowSibling = layoutBox.previousInFlowSibling();
            let contentBottom = previousInFlowSibling ? this.displayBox(previousInFlowSibling).bottom() : parentDisplayBox.contentBox().top();
            top = contentBottom + this.marginTop(layoutBox);
            // Convert static position (in parent coordinate system) to absolute (in containing block coordindate system).
            if (parent != layoutBox.containingBlock()) {
                ASSERT(displayBox.parent() == this.displayBox(layoutBox.containingBlock()));
                top += Utils.mapPosition(parentDisplayBox.topLeft(), parentDisplayBox, displayBox.parent()).top();
            }
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
            let parentDisplayBox = this.displayBox(parent);
            left = parentDisplayBox.contentBox().left() + this.marginLeft(layoutBox);
            // Convert static position (in parent coordinate system) to absolute (in containing block coordindate system).
            if (parent != layoutBox.containingBlock()) {
                ASSERT(displayBox.parent() == this.displayBox(layoutBox.containingBlock()));
                left += Utils.mapPosition(parentDisplayBox.topLeft(), parentDisplayBox, displayBox.parent()).left();
            }
        } else if (!Utils.isLeftAuto(layoutBox))
            left = Utils.left(layoutBox) + this.marginLeft(layoutBox);
        else if (!Utils.isRightAuto(layoutBox))
            left = containerSize.width() - Utils.right(layoutBox) - displayBox.width() - this.marginRight(layoutBox);
        else
            ASSERT_NOT_REACHED();
        displayBox.setTopLeft(new LayoutPoint(top, left));
    }

    _shrinkToFitWidth(layoutBox) {
        // FIXME: this is naive and missing the actual preferred width computation.
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

    _outOfFlowDescendants() {
        // FIXME: This is highly inefficient but will do for now.
        // 1. Collect all the out-of-flow descendants first.
        // 2. Check if they are all belong to this formatting context.
        //    - either the root container is the containing block.
        //    - or a descendant of the root container is the containing block
        //      and there is not other formatting context inbetween.
        let allOutOfFlowBoxes = new Array();
        let descendants = new Array();
        for (let child = this.formattingRoot().firstChild(); child; child = child.nextSibling())
            descendants.push(child);
        while (descendants.length) {
            let descendant = descendants.pop();
            if (descendant.isOutOfFlowPositioned())
                allOutOfFlowBoxes.push(descendant);
            if (!descendant.isContainer())
                continue;
            for (let child = descendant.lastChild(); child; child = child.previousSibling())
                descendants.push(child);
        }
        let outOfFlowBoxes = new Array();
        for (let outOfFlowBox of allOutOfFlowBoxes) {
            let containingBlock = outOfFlowBox.containingBlock();
            // Collect the out-of-flow descendant that belong to this formatting context.
            if (containingBlock == this.formattingRoot())
                outOfFlowBoxes.push(outOfFlowBox);
            else if (containingBlock.isDescendantOf(this.formattingRoot())) {
                if (!containingBlock.establishesFormattingContext() || !containingBlock.isPositioned())
                    outOfFlowBoxes.push(outOfFlowBox);
            }
        }
        return outOfFlowBoxes;
    }

    _firstInFlowChildWithNeedsLayout(layoutBox) {
        if (!layoutBox.isContainer())
            return null;
        for (let child = layoutBox.firstInFlowOrFloatChild(); child; child = child.nextInFlowOrFloatSibling()) {
            if (this.formattingState().needsLayout(child))
                return child;
        }
        return null;
    }

    _nextInFlowSiblingWithNeedsLayout(layoutBox) {
        for (let sibling = layoutBox.nextInFlowOrFloatSibling(); sibling; sibling = sibling.nextInFlowOrFloatSibling()) {
            if (this.formattingState().needsLayout(sibling))
                return sibling;
        }
        return null;
    }
}

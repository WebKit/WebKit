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
class FloatingContext {
public:
    void computePosition(Layout::Box&);
    LayoutUnit left(LayoutUnit verticalPosition);
    LayoutUnit right(LayoutUnit verticalPosition);
    LayoutUnit bottom();

private:
    LayoutPoint positionForFloating(const Layout::Box&);
    LayoutPoint positionForClear(const Layout::Box&);
    LayoutPoint computePositionToAvoidIntrudingFloats(const Layout::Box&);
};
*/
// All geometry here is absolute to the formatting context's root.
class FloatingContext {
    constructor(floatingState) {
        this.m_floatingState = floatingState;
    }

    computePosition(layoutBox) {
        if (layoutBox.isOutOfFlowPositioned())
            return;
        let displayBox = this._formattingState().displayBox(layoutBox);
        if (layoutBox.isFloatingPositioned()) {
            displayBox.setTopLeft(this._positionForFloating(layoutBox));
            this._addFloatingBox(layoutBox);
            return;
        }
        if (Utils.hasClear(layoutBox))
            return displayBox.setTopLeft(this._positionForClear(layoutBox));
        // Intruding floats might force this box move.
        displayBox.setTopLeft(this._computePositionToAvoidIntrudingFloats(layoutBox));
    }

    left(verticalPosition) {
        // Relative to the formatting context's root.
        let leftFloating = this._findFloatingAtVerticalPosition(verticalPosition, this._leftFloatings());
        if (!leftFloating)
            return Number.NaN;
        return this._mapDisplayMarginBoxToFormattingRoot(leftFloating).right();
    }

    right(verticalPosition) {
        // Relative to the formatting context's root.
        let rightFloating = this._findFloatingAtVerticalPosition(verticalPosition, this._rightFloatings());
        if (!rightFloating)
            return Number.NaN;
        return this._mapDisplayMarginBoxToFormattingRoot(rightFloating).left();
    }

    bottom() {
        let leftBottom = this._bottom(this._leftFloatings());
        let rightBottom = this._bottom(this._rightFloatings());
        if (Number.isNaN(leftBottom) && Number.isNaN(rightBottom))
            return Number.NaN;
        if (!Number.isNaN(leftBottom) && !Number.isNaN(rightBottom))
            return Math.max(leftBottom, rightBottom);
        if (!Number.isNaN(leftBottom))
            return leftBottom;
        return rightBottom;
    }

    _positionForFloating(floatingBox) {
        let absoluteFloatingBox = this._mapMarginBoxToFormattingRoot(floatingBox);
        if (this._isEmpty())
            return this._adjustedFloatingPosition(floatingBox, absoluteFloatingBox.top());
        let verticalPosition = Math.max(absoluteFloatingBox.top(), this._mapDisplayMarginBoxToFormattingRoot(this._lastFloating()).top());
        let spaceNeeded = absoluteFloatingBox.width();
        while (true) {
            let floatingPair = this._findInnerMostLeftAndRight(verticalPosition);
            if (this._availableSpace(floatingBox.containingBlock(), floatingPair) >= spaceNeeded)
                return this._adjustedFloatingPosition(floatingBox, verticalPosition, floatingPair);
            verticalPosition = this._moveToNextVerticalPosition(floatingPair);
        }
        return Math.Nan;
    }

    _positionForClear(layoutBox) {
        ASSERT(Utils.hasClear(layoutBox));
        let displayBox = this._formattingState().displayBox(layoutBox);
        if (this._isEmpty())
            return displayBox.topLeft();

        let leftBottom = Number.NaN;
        let rightBottom = Number.NaN;
        if (Utils.hasClearLeft(layoutBox) || Utils.hasClearBoth(layoutBox))
            leftBottom = this._bottom(this._leftFloatings());
        if (Utils.hasClearRight(layoutBox) || Utils.hasClearBoth(layoutBox))
            rightBottom = this._bottom(this._rightFloatings());

        if (!Number.isNaN(leftBottom) && !Number.isNaN(rightBottom))
            return new LayoutPoint(Math.max(leftBottom, rightBottom), displayBox.left());
        if (!Number.isNaN(leftBottom))
            return new LayoutPoint(leftBottom, displayBox.left());
        if (!Number.isNaN(rightBottom))
            return new LayoutPoint(rightBottom, displayBox.left());
        return displayBox.topLeft();
    }

    _computePositionToAvoidIntrudingFloats(layoutBox) {
        if (!layoutBox.establishesBlockFormattingContext() || this._isEmpty())
            return this._formattingState().displayBox(layoutBox).topLeft();
        // The border box of a table, a block-level replaced element, or an element in the normal flow that establishes
        // a new block formatting context (such as an element with 'overflow' other than 'visible') must not overlap the
        // margin box of any floats in the same block formatting context as the element itself.
        // For some reason, we position this as if it was floating left.
        return this._positionForFloating(layoutBox);
    }

    _findInnerMostLeftAndRight(verticalPosition) {
        let leftFloating = this._findFloatingAtVerticalPosition(verticalPosition, this._leftFloatings());
        let rightFloating = this._findFloatingAtVerticalPosition(verticalPosition, this._rightFloatings());
        return { left: leftFloating, right: rightFloating };
    }

    _moveToNextVerticalPosition(floatingPair) {
        if (!floatingPair.left && !floatingPair.right)
            return Math.NaN;
        let leftBottom = Number.POSITIVE_INFINITY;
        let rightBottom = Number.POSITIVE_INFINITY;
        if (floatingPair.left)
            leftBottom = this._mapDisplayMarginBoxToFormattingRoot(floatingPair.left).bottom();
        if (floatingPair.right)
            rightBottom = this._mapDisplayMarginBoxToFormattingRoot(floatingPair.right).bottom();
        return Math.min(leftBottom, rightBottom);
    }

    _availableSpace(containingBlock, floatingPair) {
        let containingBlockContentBox = this._formattingState().displayBox(containingBlock);
        if (floatingPair.left && floatingPair.right)
            return floatingPair.right.left() - floatingPair.left.right();
        if (floatingPair.left) {
            return containingBlockContentBox.width() - (this._mapDisplayMarginBoxToFormattingRoot(floatingPair.left).right() - this._mapBorderBoxToFormattingRoot(containingBlock).left());
        }
        if (floatingPair.right)
            return this._mapDisplayMarginBoxToFormattingRoot(floatingPair.right).left();
        return containingBlockContentBox.width();
    }

    _findFloatingAtVerticalPosition(verticalPosition, floatingStack) {
        let index = floatingStack.length;
        while (--index >= 0 && this._mapDisplayMarginBoxToFormattingRoot(floatingStack[index]).bottom() <= verticalPosition);
        return index >= 0 ? floatingStack[index] : null;
    }

    _isEmpty() {
        return !this._leftFloatings().length && !this._rightFloatings().length;
    }

    _adjustedFloatingPosition(floatingBox, verticalPosition, leftRightFloatings) {
        let containingBlock = floatingBox.containingBlock();
        // Convert all coordinates relative to formatting context's root.
        let left = this._mapContentBoxToFormattingRoot(containingBlock).left();
        let right = this._mapContentBoxToFormattingRoot(containingBlock).right();
        if (leftRightFloatings) {
            if (leftRightFloatings.left) {
                let floatingBoxRight = this._mapDisplayMarginBoxToFormattingRoot(leftRightFloatings.left).right();
                if (floatingBoxRight > left)
                    left = floatingBoxRight;
            }

            if (leftRightFloatings.right) {
                let floatingBoxLeft = this._mapDisplayMarginBoxToFormattingRoot(leftRightFloatings.right).left();
                if (floatingBoxLeft < right)
                    right = floatingBoxLeft;
            }
        }
        let floatingDisplayBox = this._formattingState().displayBox(floatingBox);
        left += floatingDisplayBox.marginLeft();
        right -= floatingDisplayBox.marginRight();
        verticalPosition += floatingDisplayBox.marginTop();
        // No convert them back relative to the floatingBox's containing block.
        let containingBlockLeft = this._mapBorderBoxToFormattingRoot(containingBlock).left();
        let containingBlockTop = this._mapBorderBoxToFormattingRoot(containingBlock).top();
        left -= containingBlockLeft;
        right -= containingBlockLeft;
        verticalPosition -= containingBlockTop;

        if (Utils.isFloatingLeft(floatingBox) || !Utils.isFloatingPositioned(floatingBox))
            return new LayoutPoint(verticalPosition, left);
        return new LayoutPoint(verticalPosition, right - floatingDisplayBox.rect().width());
    }

    _bottom(floatingStack) {
        if (!floatingStack || !floatingStack.length)
            return Number.NaN;
        let max = Number.NEGATIVE_INFINITY;
        for (let i = 0; i < floatingStack.length; ++i)
            max = Math.max(this._mapDisplayMarginBoxToFormattingRoot(floatingStack[i]).bottom(), max);
        return max;
    }

    _addFloatingBox(layoutBox) {
        this._floatingState().addFloating(this._formattingState().displayBox(layoutBox), Utils.isFloatingLeft(layoutBox));
    }

    _mapMarginBoxToFormattingRoot(layoutBox) {
        ASSERT(layoutBox instanceof Layout.Box);
        return this._mapDisplayMarginBoxToFormattingRoot(this._formattingState().displayBox(layoutBox));
    }

    _mapDisplayMarginBoxToFormattingRoot(displayBox) {
        ASSERT(displayBox instanceof Display.Box);
        return Utils.marginBox(displayBox, this._formattingState().displayBox(this.formattingRoot()));
    }

    _mapBorderBoxToFormattingRoot(layoutBox) {
        let displayBox = this._formattingState().displayBox(layoutBox);
        let rootDisplayBox = this._formattingState().displayBox(this.formattingRoot());
        return Utils.borderBox(displayBox, rootDisplayBox);
    }

    _mapContentBoxToFormattingRoot(layoutBox) {
        let displayBox = this._formattingState().displayBox(layoutBox);
        let rootDisplayBox = this._formattingState().displayBox(this.formattingRoot());
        return Utils.contentBox(displayBox, rootDisplayBox);
    }

    formattingRoot() {
        return this._formattingState().formattingRoot();
    }

    _floatingState() {
        return this.m_floatingState;
    }

    _formattingState() {
        return this._floatingState().formattingState();
    }

    _lastFloating() {
        return this._floatingState().lastFloating();
    }

    _leftFloatings() {
        return this._floatingState().leftFloatingStack();
    }

    _rightFloatings() {
        return this._floatingState().rightFloatingStack();
    }
}

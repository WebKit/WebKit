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

// All geometry here is absolute to the formatting context's root.
class FloatingContext {
    constructor(formattingContext) {
        this.m_leftFloatingBoxStack = new Array();
        this.m_rightFloatingBoxStack = new Array();
        this.m_lastFloating = null;
        this.m_formattingContext = formattingContext;
    }

    computePosition(layoutBox) {
        if (layoutBox.isOutOfFlowPositioned())
            return;
        let displayBox = this._formattingContext().displayBox(layoutBox);
        if (layoutBox.isFloatingPositioned()) {
            displayBox.setTopLeft(this._positionForFloating(layoutBox));
            this._addFloating(layoutBox);
            return;
        }
        if (Utils.hasClear(layoutBox))
            return displayBox.setTopLeft(this._positionForClear(layoutBox));
        // Intruding floats might force this box move.
        displayBox.setTopLeft(this._computePositionToAvoidIntrudingFloats(layoutBox));
    }

    bottom() {
        let leftBottom = this._bottom(this.m_leftFloatingBoxStack);
        let rightBottom = this._bottom(this.m_rightFloatingBoxStack);
        if (Number.isNaN(leftBottom) && Number.isNaN(rightBottom))
            return Number.NaN;
        if (!Number.isNaN(leftBottom) && !Number.isNaN(rightBottom))
            return Math.max(leftBottom, rightBottom);
        if (!Number.isNaN(leftBottom))
            return leftBottom;
        return rightBottom;
    }

    _positionForFloating(floatingBox) {
        let absoluteFloatingBox = this._formattingContext().absoluteMarginBox(floatingBox);
        if (this._isEmpty())
            return this._adjustedFloatingPosition(floatingBox, absoluteFloatingBox.top());
        let verticalPosition = Math.max(absoluteFloatingBox.top(), this.m_lastFloating.top());
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
        let displayBox = this._formattingContext().displayBox(layoutBox);
        if (this._isEmpty())
            return displayBox.topLeft();

        let leftBottom = Number.NaN;
        let rightBottom = Number.NaN;
        if (Utils.hasClearLeft(layoutBox) || Utils.hasClearBoth(layoutBox))
            leftBottom = this._bottom(this.m_leftFloatingBoxStack);
        if (Utils.hasClearRight(layoutBox) || Utils.hasClearBoth(layoutBox))
            rightBottom = this._bottom(this.m_rightFloatingBoxStack);

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
            return this._formattingContext().displayBox(layoutBox).topLeft();
        // The border box of a table, a block-level replaced element, or an element in the normal flow that establishes
        // a new block formatting context (such as an element with 'overflow' other than 'visible') must not overlap the
        // margin box of any floats in the same block formatting context as the element itself.
        // For some reason, we position this as if it was floating left.
        return this._positionForFloating(layoutBox);
    }

    _addFloating(floatingBox) {
        // Convert floating box to absolute.
        let floatingDisplayBox = this._formattingContext().displayBox(floatingBox).clone();
        floatingDisplayBox.setRect(this._formattingContext().absoluteMarginBox(floatingBox));
        this.m_lastFloating = floatingDisplayBox;
        if (Utils.isFloatingLeft(floatingBox)) {
            this.m_leftFloatingBoxStack.push(floatingDisplayBox);
            return;
        }
        this.m_rightFloatingBoxStack.push(floatingDisplayBox);
    }

    _findInnerMostLeftAndRight(verticalPosition) {
        let leftFloating = this._findFloatingAtVerticalPosition(verticalPosition, this.m_leftFloatingBoxStack);
        let rightFloating = this._findFloatingAtVerticalPosition(verticalPosition, this.m_rightFloatingBoxStack);
        return { left: leftFloating, right: rightFloating };
    }

    _moveToNextVerticalPosition(floatingPair) {
        if (!floatingPair.left && !floatingPair.right)
            return Math.NaN;
        let leftBottom = Number.POSITIVE_INFINITY;
        let rightBottom = Number.POSITIVE_INFINITY;
        if (floatingPair.left)
            leftBottom = floatingPair.left.bottom();
        if (floatingPair.right)
            rightBottom = floatingPair.right.bottom();
        return Math.min(leftBottom, rightBottom);
    }

    _availableSpace(containingBlock, floatingPair) {
        let containingBlockContentBox = this._formattingContext().displayBox(containingBlock);
        if (floatingPair.left && floatingPair.right)
            return floatingPair.right.left() - floatingPair.left.right();
        if (floatingPair.left)
            return containingBlockContentBox.width() - (floatingPair.left.right() - this._formattingContext().absoluteBorderBox(containingBlock).left());
        if (floatingPair.right)
            return floatingPair.right.left();
        return containingBlockContentBox.width();
    }

    _findFloatingAtVerticalPosition(verticalPosition, floatingStack) {
        let index = floatingStack.length;
        while (--index >= 0 && floatingStack[index].bottom() <= verticalPosition);
        return index >= 0 ? floatingStack[index] : null;
    }

    _isEmpty() {
        return !this.m_leftFloatingBoxStack.length && !this.m_rightFloatingBoxStack.length;
    }

    _adjustedFloatingPosition(floatingBox, verticalPosition, leftRightFloatings) {
        let containingBlock = floatingBox.containingBlock();
        // Convert all coordinates relative to formatting context's root.
        let left = this._formattingContext().absoluteContentBox(containingBlock).left();
        let right = this._formattingContext().absoluteContentBox(containingBlock).right();
        if (leftRightFloatings) {
            if (leftRightFloatings.left) {
                let floatingBoxRight = leftRightFloatings.left.right();
                if (floatingBoxRight > left)
                    left = floatingBoxRight;
            }

            if (leftRightFloatings.right) {
                let floatingBoxLeft = leftRightFloatings.right.left();
                if (floatingBoxLeft < right)
                    right = floatingBoxLeft;
            }
        }
        left += this._formattingContext().marginLeft(floatingBox);
        right -= this._formattingContext().marginRight(floatingBox);
        verticalPosition += this._formattingContext().marginTop(floatingBox);
        // No convert them back relative to the floatingBox's containing block.
        let containingBlockLeft = this._formattingContext().absoluteBorderBox(containingBlock).left();
        let containingBlockTop = this._formattingContext().absoluteBorderBox(containingBlock).top();
        left -= containingBlockLeft;
        right -= containingBlockLeft;
        verticalPosition -= containingBlockTop;

        if (Utils.isFloatingLeft(floatingBox) || !Utils.isFloatingPositioned(floatingBox))
            return new LayoutPoint(verticalPosition, left);
        return new LayoutPoint(verticalPosition, right - this._formattingContext().displayBox(floatingBox).rect().width());
    }

    _bottom(floatingStack) {
        if (!floatingStack || !floatingStack.length)
            return Number.NaN;
        let max = Number.NEGATIVE_INFINITY;
        for (let i = 0; i < floatingStack.length; ++i)
            max = Math.max(floatingStack[i].bottom(), max);
        return max;
    }

    _formattingContext() {
        return this.m_formattingContext;
    }
}

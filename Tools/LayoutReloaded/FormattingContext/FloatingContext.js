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

class FloatingContext {
    constructor(formattingContext) {
        this.m_leftFloatingBoxStack = new Array();
        this.m_rightFloatingBoxStack = new Array();
        this.m_lastFloating = null;
        this.m_formattingContext = formattingContext;
    }

    computePosition(box) {
        if (box.isOutOfFlowPositioned())
            return box.topLeft();
        if (box.isFloatingPositioned()) {
            let position = this._positionForFloating(box);
            this._addFloating(box);
            return position;
        }
        if (Utils.hasClear(box))
            return this._positionForClear(box);
        // Intruding floats might force this box move.
        return this._computePositionToAvoidIntrudingFloats(box);
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
        if (this._isEmpty())
            return this._adjustedFloatingPosition(floatingBox, this._formattingContext().absoluteMarginBox(floatingBox).top());
        let verticalPosition = Math.max(this._formattingContext().absoluteMarginBox(floatingBox).top(), this._formattingContext().absoluteMarginBox(this.m_lastFloating).top());
        let spaceNeeded = this._formattingContext().absoluteMarginBox(floatingBox).width();
        while (true) {
            let floatingPair = this._findInnerMostLeftAndRight(verticalPosition);
            if (this._availableSpace(floatingBox.containingBlock(), floatingPair) >= spaceNeeded)
                return this._adjustedFloatingPosition(floatingBox, verticalPosition, floatingPair);
            verticalPosition = this._moveToNextVerticalPosition(floatingPair);
        }
        return Math.Nan;
    }

    _positionForClear(box) {
        ASSERT(Utils.hasClear(box));
        if (this._isEmpty())
            return box.topLeft();

        let leftBottom = Number.NaN;
        let rightBottom = Number.NaN;
        if (Utils.hasClearLeft(box) || Utils.hasClearBoth(box))
            leftBottom = this._bottom(this.m_leftFloatingBoxStack);
        if (Utils.hasClearRight(box) || Utils.hasClearBoth(box))
            rightBottom = this._bottom(this.m_rightFloatingBoxStack);

        if (!Number.isNaN(leftBottom) && !Number.isNaN(rightBottom))
            return new LayoutPoint(Math.max(leftBottom, rightBottom), box.rect().left());
        if (!Number.isNaN(leftBottom))
            return new LayoutPoint(leftBottom, box.rect().left());
        if (!Number.isNaN(rightBottom))
            return new LayoutPoint(rightBottom, box.rect().left());
        return box.topLeft();
    }

    _computePositionToAvoidIntrudingFloats(box) {
        if (!box.establishesBlockFormattingContext() || this._isEmpty())
            return box.topLeft();
        // The border box of a table, a block-level replaced element, or an element in the normal flow that establishes
        // a new block formatting context (such as an element with 'overflow' other than 'visible') must not overlap the
        // margin box of any floats in the same block formatting context as the element itself.
        // For some reason, we position this as if it was floating left.
        return this._positionForFloating(box);
    }

    _addFloating(floatingBox) {
        this.m_lastFloating = floatingBox;
        if (Utils.isFloatingLeft(floatingBox)) {
            this.m_leftFloatingBoxStack.push(floatingBox);
            return;
        }
        this.m_rightFloatingBoxStack.push(floatingBox);
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
            leftBottom = this._formattingContext().absoluteMarginBox(floatingPair.left).bottom();
        if (floatingPair.right)
            rightBottom = this._formattingContext().absoluteMarginBox(floatingPair.right).bottom();
        return Math.min(leftBottom, rightBottom);
    }

    _availableSpace(containingBlock, floatingPair) {
        if (floatingPair.left && floatingPair.right)
            return this._formattingContext().absoluteMarginBox(floatingPair.right).left() - this._formattingContext().absoluteMarginBox(floatingPair.left).right();
        if (floatingPair.left)
            return containingBlock.contentBox().width() - (this._formattingContext().absoluteMarginBox(floatingPair.left).right() - this._formattingContext().absoluteBorderBox(containingBlock).left());
        if (floatingPair.right)
            return this._formattingContext().absoluteMarginBox(floatingPair.right).left();
        return containingBlock.contentBox().width();
    }

    _findFloatingAtVerticalPosition(verticalPosition, floatingStack) {
        let index = floatingStack.length;
        while (--index >= 0 && this._formattingContext().absoluteMarginBox(floatingStack[index]).bottom() <= verticalPosition);
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
                let floatingBoxRight = this._formattingContext().absoluteMarginBox(leftRightFloatings.left).right();
                if (floatingBoxRight > left)
                    left = floatingBoxRight;
            }

            if (leftRightFloatings.right) {
                let floatingBoxLeft = this._formattingContext().absoluteMarginBox(leftRightFloatings.right).left();
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
        return new LayoutPoint(verticalPosition, right - floatingBox.rect().width());
    }

    _bottom(floatingStack) {
        if (!floatingStack || !floatingStack.length)
            return Number.NaN;
        let max = Number.NEGATIVE_INFINITY;
        for (let i = 0; i < floatingStack.length; ++i)
            max = Math.max(this._formattingContext().absoluteMarginBox(floatingStack[i]).bottom(), max);
        return max;
    }

    _formattingContext() {
        return this.m_formattingContext;
    }
}

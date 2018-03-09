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
class BlockMarginCollapse {

    static marginTop(box) {
        if (box.isAnonymous())
            return 0;
        // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
        if (this._isMarginTopCollapsedWithParent(box))
            return 0;
        // Floats and out of flow positioned boxes do not collapse their margins.
        if (!this._isMarginTopCollapsedWithSibling(box))
            return this._nonCollapsedMarginTop(box);
        // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
        // unless that sibling has clearance.
        let previousInFlowSibling = box.previousInFlowSibling();
        if (previousInFlowSibling) {
            let previousSiblingMarginBottom = this._nonCollapsedMarginBottom(previousInFlowSibling);
            let marginTop = this._nonCollapsedMarginTop(box);
            return this._marginValue(marginTop, previousSiblingMarginBottom);
        }
        return this._nonCollapsedMarginTop(box);
    }

    static marginBottom(box) {
        if (box.isAnonymous())
            return 0;
        // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
        if (this._isMarginBottomCollapsedWithParent(box))
            return 0;
        // Floats and out of flow positioned boxes do not collapse their margins.
        if (!this._isMarginBottomCollapsedWithSibling(box))
            return this._nonCollapsedMarginBottom(box);
        // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
        // unless that sibling has clearance.
        if (box.nextInFlowSibling())
            return 0;
        return this._nonCollapsedMarginBottom(box);
    }

    static _isMarginTopCollapsedWithSibling(box) {
        if (box.isFloatingPositioned())
            return false;
        if (!box.isPositioned() || box.isInFlowPositioned())
            return true;
        // Out of flow positioned.
        ASSERT(box.isOutOfFlowPositioned());
        return Utils.isTopAuto(box);
    }

    static _isMarginBottomCollapsedWithSibling(box) {
        if (box.isFloatingPositioned())
            return false;
        if (!box.isPositioned() || box.isInFlowPositioned())
            return true;
        // Out of flow positioned.
        ASSERT(box.isOutOfFlowPositioned());
        return Utils.isBottomAuto(box);
    }

    static _isMarginTopCollapsedWithParent(box) {
        // The first inflow child could propagate its top margin to parent.
        // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
        if (box.isAnonymous())
            return false;
        if (box.isFloatingOrOutOfFlowPositioned())
            return false;
        let parent = box.parent();
        // Is this box the first inlflow child?
        if (parent.firstInFlowChild() != box)
            return false;
        if (parent.establishesBlockFormattingContext())
            return false;
        // Margins of the root element's box do not collapse.
        if (parent.isRootElement())
            return false;
        if (Utils.hasBorderTop(parent))
            return false;
        if (Utils.hasPaddingTop(parent))
            return false;
        return true;
    }

    static _isMarginBottomCollapsedWithParent(box) {
        // last inflow box to parent.
        // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
        if (box.isAnonymous())
            return false;
        if (box.isFloatingOrOutOfFlowPositioned())
            return false;
        let parent = box.parent();
        // Is this the last inlflow child?
        if (parent.lastInFlowChild() != box)
            return false;
        if (parent.establishesBlockFormattingContext())
            return false;
        // Margins of the root element's box do not collapse.
        if (parent.isRootElement())
            return false;
        if (Utils.hasBorderTop(parent))
            return false;
        if (Utils.hasPaddingTop(parent))
            return false;
        if (!Utils.isHeightAuto(parent))
            return false;
        return true;
    }

    static _nonCollapsedMarginTop(box) {
        // Non collapsed margin top includes collapsed margin from inflow first child.
        return this._marginValue(Utils.computedMarginTop(box), this._collapsedMarginTopFromFirstChild(box));
    }

    static _nonCollapsedMarginBottom(box) {
        // Non collapsed margin bottom includes collapsed margin from inflow last child.
        return this._marginValue(Utils.computedMarginBottom(box), this._collapsedMarginBottomFromLastChild(box));
    }

    static _collapsedMarginTopFromFirstChild(box) {
        // Check if the first child collapses its margin top.
        if (!box.isContainer() || !box.firstInFlowChild())
            return 0;
        let firstInFlowChild = box.firstInFlowChild();
        if (!this._isMarginTopCollapsedWithParent(firstInFlowChild))
            return 0;
        // Collect collapsed margin top recursively.
        return this._marginValue(Utils.computedMarginTop(firstInFlowChild), this._collapsedMarginTopFromFirstChild(firstInFlowChild));
    }

    static _collapsedMarginBottomFromLastChild(box) {
        // Check if the last child propagates its margin bottom.
        if (!box.isContainer() || !box.lastInFlowChild())
            return 0;
        let lastInFlowChild = box.lastInFlowChild();
        if (!this._isMarginBottomCollapsedWithParent(lastInFlowChild))
            return 0;
        // Collect collapsed margin bottom recursively.
        return this._marginValue(Utils.computedMarginBottom(lastInFlowChild), this._collapsedMarginBottomFromLastChild(lastInFlowChild));
    }

    static _marginValue(currentMarginValue, candidateMarginValue) {
        if (!candidateMarginValue)
            return currentMarginValue;
        if (!currentMarginValue)
            return candidateMarginValue;
        // Both margins are positive.
        if (candidateMarginValue > 0 && currentMarginValue > 0)
            return Math.max(candidateMarginValue, currentMarginValue)
        // Both margins are negative.
        if (candidateMarginValue < 0 && currentMarginValue < 0)
            return 0 - Math.max(Math.abs(candidateMarginValue), Math.abs(currentMarginValue))
        // One of the margins is negative.
        return currentMarginValue + candidateMarginValue;
    }

    static _hasAdjoiningMarginTopAndBottom(box) {
        // Two margins are adjoining if and only if:
        // 1. both belong to in-flow block-level boxes that participate in the same block formatting context
        // 2. no line boxes, no clearance, no padding and no border separate them (Note that certain zero-height line boxes (see 9.4.2) are ignored for this purpose.)
        // 3. both belong to vertically-adjacent box edges, i.e. form one of the following pairs:
        //        top margin of a box and top margin of its first in-flow child
        //        bottom margin of box and top margin of its next in-flow following sibling
        //        bottom margin of a last in-flow child and bottom margin of its parent if the parent has 'auto' computed height
        //        top and bottom margins of a box that does not establish a new block formatting context and that has zero computed 'min-height',
        //        zero or 'auto' computed 'height', and no in-flow children
        // A collapsed margin is considered adjoining to another margin if any of its component margins is adjoining to that margin.
        return false;
    }
}

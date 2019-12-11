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
class BlockMarginCollapse {
public:
    LayoutUnit marginTop(const Layout::Box&);
    LayoutUnit marginBottom(const Layout::Box&);

private:
    bool isMarginTopCollapsedWithSibling(const Layout::Box&);
    bool isMarginBottomCollapsedWithSibling(const Layout::Box&);
    bool isMarginTopCollapsedWithParent(const Layout::Box&);
    bool isMarginBottomCollapsedWithParent(const Layout::Box&);

    LayoutUnit nonCollapsedMarginTop(const Layout::Box&);
    LayoutUnit nonCollapsedMarginBottom(const Layout::Box&);
    LayoutUnit collapsedMarginTopFromFirstChild(const Layout::Box&);
    LayoutUnit collapsedMarginBottomFromLastChild(const Layout::Box&);
    LayoutUnit marginValue(currentMarginValue, candidateMarginValue);

    bool hasAdjoiningMarginTopAndBottom(const Layout::Box&);
};
*/
class BlockMarginCollapse {

    static marginTop(layoutBox) {
        if (layoutBox.isAnonymous())
            return 0;
        // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
        if (this._isMarginTopCollapsedWithParent(layoutBox))
            return 0;
        // Floats and out of flow positioned boxes do not collapse their margins.
        if (!this._isMarginTopCollapsedWithSibling(layoutBox))
            return this._nonCollapsedMarginTop(layoutBox);
        // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
        // unless that sibling has clearance.
        let previousInFlowSibling = layoutBox.previousInFlowSibling();
        if (previousInFlowSibling) {
            let previousSiblingMarginBottom = this._nonCollapsedMarginBottom(previousInFlowSibling);
            let marginTop = this._nonCollapsedMarginTop(layoutBox);
            return this._marginValue(marginTop, previousSiblingMarginBottom);
        }
        return this._nonCollapsedMarginTop(layoutBox);
    }

    static marginBottom(layoutBox) {
        if (layoutBox.isAnonymous())
            return 0;
        // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
        if (this._isMarginBottomCollapsedWithParent(layoutBox))
            return 0;
        // Floats and out of flow positioned boxes do not collapse their margins.
        if (!this._isMarginBottomCollapsedWithSibling(layoutBox))
            return this._nonCollapsedMarginBottom(layoutBox);
        // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
        // unless that sibling has clearance.
        if (layoutBox.nextInFlowSibling())
            return 0;
        return this._nonCollapsedMarginBottom(layoutBox);
    }

    static _isMarginTopCollapsedWithSibling(layoutBox) {
        if (layoutBox.isFloatingPositioned())
            return false;
        if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
            return true;
        // Out of flow positioned.
        ASSERT(layoutBox.isOutOfFlowPositioned());
        return Utils.isTopAuto(layoutBox);
    }

    static _isMarginBottomCollapsedWithSibling(layoutBox) {
        if (layoutBox.isFloatingPositioned())
            return false;
        if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
            return true;
        // Out of flow positioned.
        ASSERT(layoutBox.isOutOfFlowPositioned());
        return Utils.isBottomAuto(layoutBox);
    }

    static _isMarginTopCollapsedWithParent(layoutBox) {
        // The first inflow child could propagate its top margin to parent.
        // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
        if (layoutBox.isAnonymous())
            return false;
        if (layoutBox.isFloatingOrOutOfFlowPositioned())
            return false;
        let parent = layoutBox.parent();
        // Is this box the first inlflow child?
        if (parent.firstInFlowChild() != layoutBox)
            return false;
        if (parent.establishesBlockFormattingContext())
            return false;
        // Margins of the root element's box do not collapse.
        if (parent.isRootBox())
            return false;
        if (Utils.hasBorderTop(parent))
            return false;
        if (Utils.hasPaddingTop(parent))
            return false;
        return true;
    }

    static _isMarginBottomCollapsedWithParent(layoutBox) {
        // last inflow box to parent.
        // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
        if (layoutBox.isAnonymous())
            return false;
        if (layoutBox.isFloatingOrOutOfFlowPositioned())
            return false;
        let parent = layoutBox.parent();
        // Is this the last inlflow child?
        if (parent.lastInFlowChild() != layoutBox)
            return false;
        if (parent.establishesBlockFormattingContext())
            return false;
        // Margins of the root element's box do not collapse.
        if (parent.isRootBox())
            return false;
        if (Utils.hasBorderTop(parent))
            return false;
        if (Utils.hasPaddingTop(parent))
            return false;
        if (!Utils.isHeightAuto(parent))
            return false;
        return true;
    }

    static _nonCollapsedMarginTop(layoutBox) {
        // Non collapsed margin top includes collapsed margin from inflow first child.
        return this._marginValue(Utils.computedMarginTop(layoutBox.node()), this._collapsedMarginTopFromFirstChild(layoutBox));
    }

    static _nonCollapsedMarginBottom(layoutBox) {
        // Non collapsed margin bottom includes collapsed margin from inflow last child.
        return this._marginValue(Utils.computedMarginBottom(layoutBox.node()), this._collapsedMarginBottomFromLastChild(layoutBox));
    }

    static _collapsedMarginTopFromFirstChild(layoutBox) {
        // Check if the first child collapses its margin top.
        if (!layoutBox.isContainer() || !layoutBox.firstInFlowChild())
            return 0;
        let firstInFlowChild = layoutBox.firstInFlowChild();
        if (!this._isMarginTopCollapsedWithParent(firstInFlowChild))
            return 0;
        // Collect collapsed margin top recursively.
        return this._marginValue(Utils.computedMarginTop(firstInFlowChild.node()), this._collapsedMarginTopFromFirstChild(firstInFlowChild));
    }

    static _collapsedMarginBottomFromLastChild(layoutBox) {
        // Check if the last child propagates its margin bottom.
        if (!layoutBox.isContainer() || !layoutBox.lastInFlowChild())
            return 0;
        let lastInFlowChild = layoutBox.lastInFlowChild();
        if (!this._isMarginBottomCollapsedWithParent(lastInFlowChild))
            return 0;
        // Collect collapsed margin bottom recursively.
        return this._marginValue(Utils.computedMarginBottom(lastInFlowChild.node()), this._collapsedMarginBottomFromLastChild(lastInFlowChild));
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

    static _hasAdjoiningMarginTopAndBottom(layoutBox) {
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

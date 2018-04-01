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

    _descendantNeedsLayout() {
        return this.m_layoutStack.length;
    }

    _addToLayoutQueue(layoutBox) {
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

}

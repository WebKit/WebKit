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

class InlineFormattingContext extends FormattingContext {
    constructor(root) {
        super(root);
        // If the block container box that initiates this inline formatting contex also establishes a block context, create a new float for us.
        ASSERT(root.isBlockContainerBox());
        if (root.establishesBlockFormattingContext())
            this.m_floatingContext = new FloatingContext(this);
        this._initializeLine();
        this.m_lines = new Array();
    }

    lines() {
        // TODO: this is temporary. Line boxes will live off of the BoxTree.
        return this.m_lines;
    }

    layout(layoutContext) {
        // 9.4.2 Inline formatting contexts
        // In an inline formatting context, boxes are laid out horizontally, one after the other, beginning at the top of a containing block.
        if (!this.rootContainer().firstChild())
            return;
        // This is a post-order tree traversal layout.
        let layoutStack = new Array();
        // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
        layoutStack.push(this.rootContainer().firstChild());
        while (layoutStack.length) {
            // Travers down on the descendants until we find a leaf node.
            while (true) {
                let box = layoutStack[layoutStack.length - 1];
                if (box.establishesFormattingContext()) {
                    layoutContext.layoutFormattingContext(box.establishedFormattingContext());
                    break;
                }
                if (!box.isContainer() || !box.hasChild())
                    break;
                layoutStack.push(box.firstChild());
            }
            while (layoutStack.length) {
                let box = layoutStack.pop();
                this._handleInlineBox(box);
                if (box.nextSibling()) {
                    layoutStack.push(box.nextSibling());
                    break;
                }
            }
        }
        //this._placeOutOfFlowDescendants(this.rootContainer());
        this._commitLine();
   }

    _handleInlineBox(inlineBox) {
        if (inlineBox.text())
            return this._handleText(inlineBox);
    }

    _handleText(inlineBox) {
        // FIXME: This is a extremely simple line breaking algorithm.
        let currentPosition = 0;
        let endPosition = inlineBox.text().length() - 1;
        while (currentPosition < endPosition) {
            let breakingPosition = Utils.nextBreakingOpportunity(inlineBox.text(), currentPosition);
            if (breakingPosition == currentPosition)
                break;
            let fragmentWidth = Utils.measureText(inlineBox.text(), currentPosition, breakingPosition);
            if (this._line().availableWidth() < fragmentWidth && !this._line().isEmpty())
                this._commitLine();
            this._line().addLineBox(currentPosition, breakingPosition, new LayoutSize(fragmentWidth, Utils.textHeight(inlineBox)));
            currentPosition = breakingPosition;
        }
    }

    _commitLine() {
        this.m_lines.push(this._line());
        this._initializeLine();
    }

    _line() {
        return this.m_line;
    }

    _initializeLine() {
        // TODO: Floats need to be taken into account.
        let contentBoxRect = this.rootContainer().contentBox();
        this.m_line = new Line(contentBoxRect.topLeft(), Utils.computedLineHeight(this.rootContainer()), contentBoxRect.width());
    }
}


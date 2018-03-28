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
    constructor(inlineFormattingState) {
        super(inlineFormattingState);
        // If the block container box that initiates this inline formatting contex also establishes a block context, create a new float for us.
        ASSERT(this.formattingRoot().isBlockContainerBox());
        if (this.formattingRoot().establishesBlockFormattingContext())
            this.m_floatingContext = new FloatingContext(this);
    }

    layout() {
        // 9.4.2 Inline formatting contexts
        // In an inline formatting context, boxes are laid out horizontally, one after the other, beginning at the top of a containing block.
        if (!this.formattingRoot().firstChild())
            return;
        // This is a post-order tree traversal layout.
        // The root container layout is done in the formatting context it lives in, not that one it creates, so let's start with the first child.
        this.m_line = this._createNewLine();
        this._addToLayoutQueue(this.formattingRoot().firstChild());
        while (this._descendantNeedsLayout()) {
            // Travers down on the descendants until we find a leaf node.
            while (true) {
                let layoutBox = this._nextInLayoutQueue();
                if (layoutBox.establishesFormattingContext()) {
                    this.layoutState().layout(layoutBox);
                    break;
                }
                if (!layoutBox.isContainer() || !layoutBox.hasChild())
                    break;
                this._addToLayoutQueue(layoutBox.firstChild());
            }
            while (this._descendantNeedsLayout()) {
                let layoutBox = this._nextInLayoutQueue();
                this._handleInlineBox(layoutBox);
                // We are done with laying out this box.
                this._removeFromLayoutQueue(layoutBox);
                if (layoutBox.nextSibling()) {
                    this._addToLayoutQueue(layoutBox.nextSibling());
                    break;
                }
            }
        }
        //this._placeOutOfFlowDescendants(this.formattingRoot());
        this._commitLine();
   }

    _handleInlineBox(inlineBox) {
        if (inlineBox.text())
            return this._handleText(inlineBox);
    }

    _handleText(inlineBox) {
        // FIXME: This is a extremely simple line breaking algorithm.
        let currentPosition = 0;
        let text = inlineBox.text().content();
        while (text.length) {
            let textRuns = Utils.textRunsForLine(text, this._line().availableWidth(), this.formattingRoot());
            if (!textRuns.length)
                break;
            for (let run of textRuns)
                this._line().addTextLineBox(run.startPosition, run.endPosition, new LayoutSize(run.width, Utils.textHeight(inlineBox)));
            text = text.slice(textRuns[textRuns.length - 1].endPosition, text.length);
            this._commitLine();
        }
    }

    _commitLine() {
        if (this._line().isEmpty())
            return;
        this.formattingState().appendLine(this._line());
        this.m_line = this._createNewLine();
    }

    _line() {
        return this.m_line;
    }

    _createNewLine() {
        // TODO: Floats need to be taken into account.
        let contentBoxRect = this.displayBox(this.formattingRoot()).contentBox();
        let topLeft = contentBoxRect.topLeft();
        let lines = this.formattingState().lines();
        if (lines.length)
            topLeft.setTop(lines[lines.length - 1].rect().bottom());
        return new Line(topLeft, Utils.computedLineHeight(this.formattingRoot().node()), contentBoxRect.width());
    }
}


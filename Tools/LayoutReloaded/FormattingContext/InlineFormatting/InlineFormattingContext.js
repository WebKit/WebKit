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
class InlineFormattingContext {
public:
    void layout() override;

private:
    void handleInlineContainer(const Layout::Container&);
    void handleInlineBlockContainer(const Layout::Container&);
    void handleInlineContent(const Layout::Box&);
    void handleInlineBox(const Layout::InlineBox&);
    void handleText(const Layout::InlineBox&);
    void handleFloatingBox(const Layout::Box&);
};
*/
class InlineFormattingContext extends FormattingContext {
    constructor(inlineFormattingState) {
        super(inlineFormattingState);
        ASSERT(this.formattingRoot().isBlockContainerBox());
    }

    layout() {
        // 9.4.2 Inline formatting contexts
        // In an inline formatting context, boxes are laid out horizontally, one after the other, beginning at the top of a containing block.
        if (!this.formattingRoot().firstChild())
            return;
        this.m_line = this._createNewLine();
        let inlineContainerStack = new Array();
        this._addToLayoutQueue(this._firstInFlowChildWithNeedsLayout(this.formattingRoot()));
        while (this._descendantNeedsLayout()) {
            let layoutBox = this._nextInLayoutQueue();
            if (layoutBox.isInlineContainer()) {
                let containerStart = inlineContainerStack.indexOf(layoutBox) == -1;
                this._handleInlineContainer(layoutBox, containerStart);
                // container start.
                if (containerStart) {
                    inlineContainerStack.push(layoutBox);
                    this._addToLayoutQueue(this._firstInFlowChildWithNeedsLayout(layoutBox));
                    // Keep the inline container in the layout stack so that we can finish it when all the descendants are all set.
                    continue;
                }
                // container end.
                inlineContainerStack.pop(layoutBox);
            } else
                this._handleInlineContent(layoutBox);
            this._clearNeedsLayoutAndMoveToNextSibling(layoutBox);
        }
        // Place the inflow positioned children.
        this._placeInFlowPositionedChildren(this.formattingRoot());
        // And take care of out-of-flow boxes as the final step.
        this._layoutOutOfFlowDescendants(this.formattingRoot());
        this._commitLine();
        ASSERT(!inlineContainerStack.length);
        ASSERT(!this.formattingState().layoutNeeded());
   }

    _handleInlineContainer(inlineContainer, containerStart) {
        ASSERT(!inlineContainer.establishesFormattingContext());
        if (containerStart) {
            this._adjustLineForInlineContainerStart(inlineContainer);
            return;
        }
        this._adjustLineForInlineContainerEnd(inlineContainer);
        // Place inflow positioned children.
        this._placeInFlowPositionedChildren(inlineContainer);
    }


    _handleInlineContent(layoutBox) {
        if (layoutBox.isInlineBox())
            this._handleInlineBox(layoutBox);
        else if (layoutBox.isFloatingPositioned())
            this._handleFloatingBox(layoutBox);
        else if (layoutBox.isInlineBlockBox())
            this._handleInlineBlock(layoutBox);
        else
            ASSERT_NOT_REACHED();
    }

    _handleInlineBox(inlineBox) {
        if (inlineBox.text())
            return this._handleText(inlineBox);
        else
            return this._handleReplaced(inlineBox);
    }

    _handleInlineBlock(inlineBlockBox) {
        ASSERT(inlineBlockBox.establishesFormattingContext());
        let displayBox = this.displayBox(inlineBlockBox);

        // TODO: auto width/height and check if content actually at all.
        this._adjustLineForInlineContainerStart(inlineBlockBox);
        displayBox.setWidth(Utils.width(inlineBlockBox) + Utils.computedHorizontalBorderAndPadding(inlineBlockBox.node()));
        this.layoutState().formattingContext(inlineBlockBox).layout();
        displayBox.setHeight(Utils.height(inlineBlockBox) + Utils.computedVerticalBorderAndPadding(inlineBlockBox.node()));
        this._line().addInlineBox(displayBox.size());
        displayBox.setTopLeft(this._line().lastLineBox().lineBoxRect.topLeft());
        this._adjustLineForInlineContainerEnd(inlineBlockBox);
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
            // Commit the line unless we run out of content.
            if (text.length)
                this._commitLine();
        }
    }

    _handleFloatingBox(floatingBox) {
        this._computeFloatingWidth(floatingBox);
        this.layoutState().formattingContext(floatingBox).layout();
        this._computeFloatingHeight(floatingBox);
        let displayBox = this.displayBox(floatingBox);
        // Position this float statically first, the floating context will figure it out the final position.
        let floatingStaticPosition = this._line().rect().topLeft();
        if (displayBox.width() > this._line().availableWidth())
            floatingStaticPosition = new LayoutPoint(this._line().rect().bottom(), this.displayBox(this.formattingRoot()).contentBox().left());
        displayBox.setTopLeft(floatingStaticPosition);
        this.floatingContext().computePosition(floatingBox);
        // Check if the floating box is actually on the current line or got pushed further down.
        if (displayBox.top() >= this._line().rect().bottom())
            return;
        let floatWidth = displayBox.width();
        this._line().shrink(floatWidth);
        if (Utils.isFloatingLeft(floatingBox))
            this._line().moveContentHorizontally(floatWidth);
    }

    _handleReplaced(replacedBox) {
        // TODO: intrinsic size and check if content actually at all.
        let displayBox = this.displayBox(replacedBox);
        this._adjustLineForInlineContainerStart(replacedBox);
        displayBox.setWidth(Utils.width(replacedBox) + Utils.computedHorizontalBorderAndPadding(replacedBox.node()));

        displayBox.setHeight(Utils.height(replacedBox) + Utils.computedVerticalBorderAndPadding(replacedBox.node()));
        this._line().addInlineBox(displayBox.size());
        displayBox.setTopLeft(this._line().lastLineBox().lineBoxRect.topLeft());
        this._adjustLineForInlineContainerEnd(replacedBox);
   }

    _adjustLineForInlineContainerStart(inlineContainer) {
        let offset = this.marginLeft(inlineContainer) + Utils.computedBorderAndPaddingLeft(inlineContainer.node());
        this._line().adjustWithOffset(offset);
    }

    _adjustLineForInlineContainerEnd(inlineContainer) {
        let offset = this.marginRight(inlineContainer) + Utils.computedBorderAndPaddingRight(inlineContainer.node());
        this._line().adjustWithOffset(offset);
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
        let lineRect = this.displayBox(this.formattingRoot()).contentBox();
        let lines = this.formattingState().lines();
        if (lines.length)
            lineRect.setTop(lines[lines.length - 1].rect().bottom());
        // Find floatings on this line.
        // Offset the vertical position if the floating context belongs to the parent formatting context.
        let lineTopInFloatingPosition = this._mapFloatingVerticalPosition(lineRect.top());
        let floatingLeft = this._mapFloatingHorizontalPosition(this.floatingContext().left(lineTopInFloatingPosition));
        let floatingRight = this._mapFloatingHorizontalPosition(this.floatingContext().right(lineTopInFloatingPosition));
        if (!Number.isNaN(floatingLeft) && !Number.isNaN(floatingRight)) {
            // Floats on both sides.
            lineRect.setLeft(floatingLeft);
            lineRect.setWidth(floatingRight - floatingLeft);
        } else if (!Number.isNaN(floatingLeft)) {
            lineRect.setLeft(floatingLeft);
            lineRect.shrinkBy(new LayoutSize(floatingLeft, 0));
        } else if (!Number.isNaN(floatingRight))
            lineRect.setRight(floatingRight);

        return new Line(lineRect.topLeft(), Utils.computedLineHeight(this.formattingRoot().node()), lineRect.width());
    }

    _mapFloatingVerticalPosition(verticalPosition) {
        // Floats position are relative to their formatting root (which might not be this formatting root).
        let root = this.displayBox(this.formattingRoot());
        let floatFormattingRoot = this.displayBox(this.floatingContext().formattingRoot());
        if (root == floatFormattingRoot)
            return verticalPosition;
        let rootTop = Utils.mapPosition(root.topLeft(), root, floatFormattingRoot).top();
        return rootTop + root.contentBox().top() + verticalPosition;
    }

    _mapFloatingHorizontalPosition(horizontalPosition) {
        if (Number.isNaN(horizontalPosition))
            return horizontalPosition;
        // Floats position are relative to their formatting root (which might not be this formatting root).
        let root = this.displayBox(this.formattingRoot());
        let floatFormattingRoot = this.displayBox(this.floatingContext().formattingRoot());
        if (root == floatFormattingRoot)
            return horizontalPosition;
        let rootLeft = Utils.mapPosition(root.topLeft(), root, floatFormattingRoot).left();
        rootLeft += root.contentBox().left();
        // The left most float is to the right of the root.
        if (rootLeft >= horizontalPosition)
            return root.contentBox().left();
        return horizontalPosition - rootLeft;
     }

    _clearNeedsLayoutAndMoveToNextSibling(layoutBox) {
        this._removeFromLayoutQueue(layoutBox);
        this.formattingState().clearNeedsLayout(layoutBox);
        this._addToLayoutQueue(this._nextInFlowSiblingWithNeedsLayout(layoutBox));
    }
}


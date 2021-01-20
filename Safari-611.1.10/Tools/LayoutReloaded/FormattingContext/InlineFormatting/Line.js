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
class Line {
public:
    bool isEmpty();

    LayoutUnit availableWidth();

    LayoutRect rect();
    Vector<InlineDisplayBox> lineBoxes();

    void shrink(float width);
    void adjustWithOffset(LayoutUnit offset);
    void moveContentHorizontally(LayoutUnit offset);
    void addInlineContainerBox(LayoutSize);
    void addTextLineBox(unsigned startPosition, unsigned endPosition, LayoutSize size);
};
*/
class Line {
    constructor(topLeft, height, availableWidth) {
        this.m_availableWidth = availableWidth;
        this.m_lineRect = new LayoutRect(topLeft, new LayoutSize(0, height));
        this.m_lineBoxes = new Array();
    }

    isEmpty() {
        return !this.m_lineBoxes.length;
    }

    availableWidth() {
        return this.m_availableWidth;
    }

    rect() {
        return this.m_lineRect;
    }

    lineBoxes() {
        return this.m_lineBoxes;
    }

    lastLineBox() {
        return this.m_lineBoxes[this.m_lineBoxes.length - 1];
    }

    shrink(width) {
        this.m_availableWidth -= width;
    }

    adjustWithOffset(offset) {
        this.m_availableWidth -= offset;
        this.m_lineRect.growBy(new LayoutSize(offset, 0));
    }

    moveContentHorizontally(offset) {
        // Push non-floating boxes to the right.
        for (let lineBox of this.m_lineBoxes)
            lineBox.lineBoxRect.moveHorizontally(offset);
        this.m_lineRect.moveHorizontally(offset);
    }

    addInlineBox(size) {
        let width = size.width();
        ASSERT(width <= this.m_availableWidth);
        this.shrink(width);
        let lineBoxRect = new LayoutRect(this.rect().topRight(), size);
        this.m_lineBoxes.push({lineBoxRect});
        this.m_lineRect.growHorizontally(width);
    }

    addTextLineBox(startPosition, endPosition, size) {
        let width = size.width();
        ASSERT(width <= this.m_availableWidth);
        this.shrink(width);
        // TODO: use the actual height instead of the line height.
        let lineBoxRect = new LayoutRect(this.rect().topRight(), new LayoutSize(width, this.rect().height()));
        this.m_lineBoxes.push({startPosition, endPosition, lineBoxRect});
        this.m_lineRect.growHorizontally(width);
    }
}

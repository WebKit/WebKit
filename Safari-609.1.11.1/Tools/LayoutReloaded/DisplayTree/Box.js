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
namespace Display {
class Box {
    setRect(LayoutRect);
    setTopLeft(LayoutPoint);
    setTop(LayoutUnit);
    setLeft(LayoutUnit);
    setSize(LayoutSize);
    setWidth(LayoutUnit);
    setHeight(LayoutUnit);

    LayoutRect rect();

    LayoutUnit top();
    LayoutUnit left();
    LayoutUnit bottom();
    LayoutUnit right();

    LayoutPoint topLeft();
    LayoutPoint bottomRight();

    LayoutSize size();
    LayoutUnit width();
    LayoutUnit height();

    setMarginTop(LayoutUnit);
    setMarginLeft(LayoutUnit);
    setMarginBottom(LayoutUnit);
    setMarginRight(LayoutUnit);

    LayoutUnit marginTop();
    LayoutUnit marginLeft();
    LayoutUnit marginBottom();
    LayoutUnit marginRight();

    LayoutRect marginBox();
    LayoutRect borderBox();
    LayoutRect paddingBox();
    LayoutRect contentBox();

    void setParent(Display::Box&);
    void setNextSibling(Display::Box&);
    void setPreviousSibling(Display::Box&);
    void setFirstChild(Display::Box&);
    void setLastChild(Display::Box&);

    Display::Box* parent();
    Display::Box* nextSibling();
    Display::Box* previousSibling();
    Display::Box* firstChild();
    Display::Box* lastChild();
};
}
*/
var Display = { }

Display.Box = class Box {
    constructor(node) {
        this.m_node = node;
        this.m_rect = new LayoutRect(new LayoutPoint(0, 0), new LayoutSize(0, 0));
        this.m_marginTop = 0;
        this.m_marginLeft = 0;
        this.m_marginBottom = 0;
        this.m_marginRight = 0;
        this.m_parent = null;
        this.m_nextSibling = null;
        this.m_previousSibling = null;
        this.m_firstChild = null;
        this.m_lastChild = null;
    }

    setRect(rect) {
        return this.m_rect = rect;
    }

    setTopLeft(topLeft) {
        this.m_rect.setTopLeft(topLeft);
    }

    setTop(top) {
        this.m_rect.setTop(top);
    }

    setLeft(left) {
        this.m_rect.setLeft(left);
    }

    setSize(size) {
        this.m_rect.setSize(size);
    }

    setWidth(width) {
        this.m_rect.setWidth(width);
    }

    setHeight(height) {
        this.m_rect.setHeight(height);
    }

    rect() {
        return this.m_rect.clone();
    }

    top() {
        return this.rect().top();
    }
    
    left() {
        return this.rect().left();
    }
    
    bottom() {
        return this.rect().bottom();
    }
    
    right() {
        return this.rect().right();
    }

    topLeft() {
        return this.rect().topLeft();
    }

    bottomRight() {
        return this.rect().bottomRight();
    }

    size() {
        return this.rect().size();
    }

    width() {
        return this.rect().width();
    }

    height() {
        return this.rect().height();
    }

    setMarginTop(marginTop) {
        this.m_marginTop = marginTop;
     }

    setMarginLeft(marginLeft) {
        this.m_marginLeft = marginLeft;
     }

    setMarginBottom(marginBottom) {
        this.m_marginBottom = marginBottom;
     }

    setMarginRight(marginRight) {
        this.m_marginRight = marginRight;
    }

    marginTop() {
        return this.m_marginTop;
    }

    marginLeft() {
        return this.m_marginLeft;
    }

    marginBottom() {
        return this.m_marginBottom;
    }

    marginRight() {
        return this.m_marginRight;
    }

    marginBox() {
        let marginBox = this.rect();
        marginBox.moveBy(new LayoutSize(-this.m_marginLeft, -this.m_marginTop));
        marginBox.growBy(new LayoutSize(this.m_marginLeft + this.m_marginRight, this.m_marginTop + this.m_marginBottom));
        return marginBox;
     }

    borderBox() {
        return new LayoutRect(new LayoutPoint(0, 0), this.rect().size());
    }

    paddingBox() {
        // ICB does not have associated node.
        if (!this.m_node)
            return this.borderBox();
        let paddingBox = this.borderBox();
        let borderSize = Utils.computedBorderTopLeft(this.m_node);
        paddingBox.moveBy(borderSize);
        paddingBox.shrinkBy(borderSize);
        paddingBox.shrinkBy(Utils.computedBorderBottomRight(this.m_node));
        return paddingBox;
    }

    contentBox() {
        // ICB does not have associated node.
        if (!this.m_node)
            return this.borderBox();
        let contentBox = this.paddingBox();
        let paddingSize = Utils.computedPaddingTopLeft(this.m_node);
        contentBox.moveBy(paddingSize);
        contentBox.shrinkBy(paddingSize);
        contentBox.shrinkBy(Utils.computedPaddingBottomRight(this.m_node));
        return contentBox;
    }

    setParent(parent) {
        this.m_parent = parent;
    }

    setNextSibling(nextSibling) {
        this.m_nextSibling = nextSibling;
    }

    setPreviousSibling(previousSibling) {
        this.m_previousSibling = previousSibling;
    }

    setFirstChild(firstChild) {
        this.m_firstChild = firstChild;
    }

    setLastChild(lastChild) {
        this.m_lastChild = lastChild;
    }

    parent() {
        return this.m_parent;;
    }

    nextSibling() {
        return this.m_nextSibling;
    }

    previousSibling() {
        return this.m_previousSibling;
    }

    firstChild() {
        return this.m_firstChild;
    }

    lastChild() {
        return this.m_lastChild;
    }
}

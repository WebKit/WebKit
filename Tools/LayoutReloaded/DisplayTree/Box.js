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

var Display = { }

Display.Box = class Box {
    constructor(node) {
        this.m_node = node;
        this.m_rect = new LayoutRect(new LayoutPoint(0, 0), new LayoutSize(0, 0));
    }

    clone() {
        let cloneBox = new Display.Box(this.m_node);
        cloneBox.setRect(this.rect());
        return cloneBox;
    }

    rect() {
        return this.m_rect.clone();
    }

    setRect(rect) {
        return this.m_rect = rect;
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

    height() {
        return this.rect().height();
    }

    width() {
        return this.rect().width();
    }

    setTopLeft(topLeft) {
        this.m_rect.setTopLeft(topLeft);
    }

    setTop(top) {
        this.m_rect.setTop(top);
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
}

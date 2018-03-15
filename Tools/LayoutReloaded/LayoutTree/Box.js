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

var Layout = { }

Layout.Box = class Box {
    constructor(node, id) {
        this.m_id = id;
        this.m_rendererName = null;
        this.m_node = node;
        this.m_parent = null;
        this.m_nextSibling = null;
        this.m_previousSibling = null;
        this.m_isAnonymous = false;
        this.m_rect = new LayoutRect(new LayoutPoint(0, 0), new LayoutSize(0, 0));
        this.m_establishedFormattingContext = null;
    }

    id() {
        return this.m_id;
    }

    setRendererName(name) {
        this.m_rendererName = name;
    }

    name() {
        return this.m_rendererName;
    }

    node() {
        return this.m_node;
    }

    parent() {
        return this.m_parent;
    }

    nextSibling() {
        return this.m_nextSibling;
    }

    nextInFlowSibling() {
        let nextInFlowSibling = this.nextSibling();
        while (nextInFlowSibling) {
            if (nextInFlowSibling.isInFlow())
                return nextInFlowSibling;
            nextInFlowSibling = nextInFlowSibling.nextSibling();
        }
        return null;
    }

    previousSibling() {
        return this.m_previousSibling;
    }

    previousInFlowSibling() {
        let previousInFlowSibling = this.previousSibling();
        while (previousInFlowSibling) {
            if (previousInFlowSibling.isInFlow())
                return previousInFlowSibling;
            previousInFlowSibling = previousInFlowSibling.previousSibling();
        }
        return null;
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

    rect() {
        return this.m_rect.clone();
    }

    topLeft() {
        return this.rect().topLeft();
    }

    bottomRight() {
        return this.rect().bottomRight();
    }

    setTopLeft(topLeft) {
        this.m_rect.setTopLeft(topLeft);
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

    isContainer() {
        return false;
    }

    isBlockLevelBox() {
        return Utils.isBlockLevelElement(this.node());
    }

    isBlockContainerBox() {
        return Utils.isBlockContainerElement(this.node());
    }

    isInlineLevelBox() {
        return Utils.isInlineLevelElement(this.node());
    }

    setIsAnonymous() {
        this.m_isAnonymous = true;
    }

    isAnonymous() {
        return this.m_isAnonymous;
    }

    establishesFormattingContext() {
        if (this.isAnonymous())
            return false;
        return this.establishesBlockFormattingContext() || this.establishesInlineFormattingContext();
    }

    establishedFormattingContext() {
        if (this.establishesFormattingContext() && !this.m_establishedFormattingContext)
            this.m_establishedFormattingContext = this.establishesBlockFormattingContext() ? new BlockFormattingContext(this) : new InlineFormattingContext(this);
        return this.m_establishedFormattingContext;
    }

    establishesBlockFormattingContext() {
        // 9.4.1 Block formatting contexts
        // Floats, absolutely positioned elements, block containers (such as inline-blocks, table-cells, and table-captions)
        // that are not block boxes, and block boxes with 'overflow' other than 'visible' (except when that value has been propagated to the viewport)
        // establish new block formatting contexts for their contents.
        return this.isFloatingPositioned() || this.isAbsolutePositioned() || (this.isBlockContainerBox() && !this.isBlockLevelBox())
            || (this.isBlockLevelBox() && !Utils.isOverflowVisible(this));
    }

    establishesInlineFormattingContext() {
        return false;
    }

    isPositioned() {
        return this.isOutOfFlowPositioned() || this.isRelativePositioned();
    }

    isRelativePositioned() {
        return Utils.isRelativePositioned(this);
    }

    isAbsolutePositioned() {
        return Utils.isAbsolutePositioned(this);
    }

    isFixedPositioned() {
        return Utils.isFixedPositioned(this);
    }

    isInFlow() {
        if (this.isAnonymous())
            return true;
        return !this.isFloatingOrOutOfFlowPositioned();
    }

    isOutOfFlowPositioned() {
        return this.isAbsolutePositioned() || this.isFixedPositioned();
    }

    isInFlowPositioned() {
        return this.isPositioned() && !this.isOutOfFlowPositioned();
    }

    isFloatingPositioned() {
        return Utils.isFloatingPositioned(this);
    }

    isFloatingOrOutOfFlowPositioned() {
        return this.isFloatingPositioned() || this.isOutOfFlowPositioned();
    }

    isRootElement() {
        // FIXME: This should just be a simple instanceof check, but we are in the mainframe while the test document is in an iframe
        // Let's just return root for both the RenderView and the <html> element.
        return !this.parent() || !this.parent().parent();
    }

    containingBlock() {
        if (!this.parent())
            return null;
        if (!this.isPositioned() || this.isInFlowPositioned())
            return this.parent();
        let parent = this.parent();
        while (parent.parent()) {
            if (this.isAbsolutePositioned() && parent.isPositioned())
                return parent;
            parent = parent.parent();
        }
        return parent;
    }

    borderBox() {
        return new LayoutRect(new LayoutPoint(0, 0), this.rect().size());
    }

    paddingBox() {
        let paddingBox = this.borderBox();
        let borderSize = Utils.computedBorderTopLeft(this);
        paddingBox.moveBy(borderSize);
        paddingBox.shrinkBy(borderSize);
        paddingBox.shrinkBy(Utils.computedBorderBottomRight(this));
        return paddingBox;
    }

    contentBox() {
        let contentBox = this.paddingBox();
        let paddingSize = Utils.computedPaddingTopLeft(this);
        contentBox.moveBy(paddingSize);
        contentBox.shrinkBy(paddingSize);
        contentBox.shrinkBy(Utils.computedPaddingBottomRight(this));
        return contentBox;
    }
}

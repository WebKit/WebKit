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

class LayoutPoint {
    constructor(top, left) {
        this.m_top = top;
        this.m_left = left;
    }

    setLeft(left) {
        this.m_left = left;
    }

    setTop(top) {
        this.m_top = top;
    }

    left() {
        return this.m_left;
    }

    top() {
        return this.m_top;
    }

    shiftLeft(distance) {
        this.m_left += distance;
    }

    shiftTop(distance) {
        this.m_top += distance;
    }

    moveBy(distance) {
        if (distance.top && distance.left) {
            this.m_top += distance.top();
            this.m_left += distance.left();
        }
        else if (distance.width && distance.height) {
            this.m_top += distance.height();
            this.m_left += distance.width();
        }
    }

    equal(other) {
        return this.top() == other.top() && this.left() == other.left();
    }

    clone() {
        return new LayoutPoint(this.top(), this.left());
    }
}

class LayoutSize {
    constructor(width, height) {
        this.m_width = width;
        this.m_height = height;
    }

    setWidth(width) {
        this.m_width = width;
    }

    setHeight(height) {
        this.m_height = height;
    }

    width() {
        return this.m_width;
    }

    height() {
        return this.m_height;
    }

    growBy(distance) {
        this.m_width += distance.width();
        this.m_height += distance.height();
    }

    shrinkBy(distance) {
        this.m_width -= distance.width();
        this.m_height -= distance.height();
    }

    isEmpty() {
        return this.m_width <= 0 || this.m_height <= 0;
    }

    equal(other) {
        return this.width() == other.width() && this.height() == other.height();
    }

    clone() {
        return new LayoutSize(this.width(), this.height());
    }
}

class LayoutRect {
    constructor(topLeft, size) {
        this.m_topLeft = topLeft.clone();
        this.m_size = size.clone();
    }

    setTop(top) {
        this.m_topLeft.setTop(top);
    }

    setLeft(left) {
        this.m_topLeft.setLeft(left);
    }

    setBottom(bottom) {
        this.m_size.setHeight(bottom - this.m_topLeft.top());
    }

    setRight(right) {
        this.m_size.setWidth(right - this.m_topLeft.left());
    }

    left() {
        return this.m_topLeft.left();
    }

    top() {
        return this.m_topLeft.top();
    }

    bottom() {
        return this.m_topLeft.top() + this.m_size.height();
    }

    right() {
        return this.m_topLeft.left() + this.m_size.width();
    }

    setTopLeft(topLeft) {
        this.m_topLeft = topLeft.clone();
    }

    topLeft() {
        return this.m_topLeft.clone();
    }

    topRight() {
        return new LayoutPoint(this.top(), this.right());
    }

    bottomRight() {
        return new LayoutPoint(this.bottom(), this.right());
    }

    setWidth(width) {
        this.m_size.setWidth(width);
    }

    setHeight(height) {
        this.m_size.setHeight(height);
    }

    setSize(newSize) {
        this.m_size = newSize.clone();
    }

    size() {
        return this.m_size.clone();
    }

    width() {
        return this.m_size.width();
    }

    height() {
        return this.m_size.height();
    }

    growBy(distance) {
        this.m_size.growBy(distance);
    }

    shrinkBy(distance) {
        this.m_size.shrinkBy(distance);
    }

    moveBy(distance) {
        this.m_topLeft.moveBy(distance);
    }

    growHorizontally(distance) {
        this.m_size.setWidth(this.m_size.width() + distance);
    }

    moveHorizontally(distance) {
        this.m_topLeft.shiftLeft(distance);
    }

    moveVertically(distance) {
        this.m_topLeft.shiftTop(distance);
    }

    isEmpty() {
        return this.m_size.isEmpty();
    }

    equal(other) {
        return this.m_topLeft.equal(other.topLeft()) && this.m_size.equal(other.size());
    }

    intersects(other) {
        return !this.isEmpty() && !other.isEmpty()
        && this.left() < other.right() && other.left() < this.right()
        && this.top() < other.bottom() && other.top() < this.bottom();
    }

    contains(other) {
        return this.left() <= other.left() && this.right() >= other.right()
        && this.top() <= other.top() && this.bottom() >= other.bottom();
    }

    clone() {
        return new LayoutRect(this.topLeft().clone(), this.size().clone());
    }
}

function ASSERT_NOT_REACHED() {
    throw Error("Should not reach!");
}

function ASSERT(statement) {
    if (statement)
        return;
    throw Error("Assertion failure");
}

class Utils {
    static computedValue(strValue, baseValue) {
        if (strValue.indexOf("px") > -1)
            return parseFloat(strValue);
        if (strValue.indexOf("%") > -1)
            return parseFloat(strValue) * baseValue / 100;
        return Number.NaN;
    }

    static propertyIsAuto(propertyName, box) {
        if (box.isAnonymous())
            return true;
        return window.getComputedStyle(box.node()).isPropertyValueInitial(propertyName);
    }

    static isWidthAuto(box) {
        return Utils.propertyIsAuto("width", box);
    }

    static isHeightAuto(box) {
        return Utils.propertyIsAuto("height", box);
    }

    static isTopAuto(box) {
        return Utils.propertyIsAuto("top", box);
    }

    static isLeftAuto(box) {
        return Utils.propertyIsAuto("left", box);
    }

    static isBottomAuto(box) {
        return Utils.propertyIsAuto("bottom", box);
    }

    static isRightAuto(box) {
        return Utils.propertyIsAuto("right", box);
    }

    static width(box) {
        ASSERT(!Utils.isWidthAuto(box));
        return parseFloat(window.getComputedStyle(box.node()).width);
    }

    static height(box) {
        ASSERT(!Utils.isHeightAuto(box));
        return parseFloat(window.getComputedStyle(box.node()).height);
    }

    static top(box) {
        return parseFloat(box.node().style.top);
    }

    static bottom(box) {
        return parseFloat(box.node().style.bottom);
    }

    static left(box) {
        return parseFloat(box.node().style.left);
    }

    static right(box) {
        return parseFloat(box.node().style.right);
    }

    static hasBorderTop(box) {
        return window.getComputedStyle(box.node()).borderTopWidth != "0px";
    }

    static hasBorderBottom(box) {
        return window.getComputedStyle(box.node()).borderBottomWidth != "0px";
    }

    static hasPaddingTop(box) {
        return window.getComputedStyle(box.node()).paddingTop != "0px";
    }

    static hasPaddingBottom(box) {
        return window.getComputedStyle(box.node()).paddingBottom != "0px";
    }

    static computedMarginTop(node) {
        return Utils.computedValue(window.getComputedStyle(node).marginTop);
    }

    static computedMarginLeft(node) {
        return Utils.computedValue(window.getComputedStyle(node).marginLeft);
    }

    static computedMarginBottom(node) {
        return Utils.computedValue(window.getComputedStyle(node).marginBottom);
    }

    static computedMarginRight(node) {
        return Utils.computedValue(window.getComputedStyle(node).marginRight);
    }

    static computedBorderTopLeft(node) {
        return new LayoutSize(Utils.computedValue(window.getComputedStyle(node).borderLeftWidth), Utils.computedValue(window.getComputedStyle(node).borderTopWidth));
    }

    static computedBorderBottomRight(node) {
        return new LayoutSize(Utils.computedValue(window.getComputedStyle(node).borderRightWidth), Utils.computedValue(window.getComputedStyle(node).borderBottomWidth));
    }

    static computedPaddingTopLeft(node) {
        return new LayoutSize(Utils.computedValue(window.getComputedStyle(node).paddingLeft), Utils.computedValue(window.getComputedStyle(node).paddingTop));
    }

    static computedPaddingBottomRight(node) {
        return new LayoutSize(Utils.computedValue(window.getComputedStyle(node).paddingRight), Utils.computedValue(window.getComputedStyle(node).paddingBottom));
    }

    static computedBorderAndPaddingTop(node) {
        return Utils.computedBorderTopLeft(node).height() + Utils.computedPaddingTopLeft(node).height();
    }

    static computedBorderAndPaddingLeft(node) {
        return Utils.computedBorderTopLeft(node).width() + Utils.computedPaddingTopLeft(node).width();
    }

    static computedBorderAndPaddingTop(node) {
        return Utils.computedBorderTopLeft(node).height() + Utils.computedPaddingTopLeft(node).height();
    }

    static computedBorderAndPaddingLeft(node) {
        return Utils.computedBorderTopLeft(node).width() + Utils.computedPaddingTopLeft(node).width();
    }

    static computedBorderAndPaddingBottom(node) {
        return Utils.computedBorderBottomRight(node).height() + Utils.computedPaddingBottomRight(node).height();
    }

    static computedBorderAndPaddingRight(node) {
        return Utils.computedBorderBottomRight(node).width() + Utils.computedPaddingBottomRight(node).width();
    }

    static computedHorizontalBorderAndPadding(node) {
        return this.computedBorderAndPaddingLeft(node) + this.computedBorderAndPaddingRight(node);
    }

    static computedVerticalBorderAndPadding(node) {
        return this.computedBorderAndPaddingTop(node) + this.computedBorderAndPaddingBottom(node);
    }

    static computedLineHeight(node) {
        return Utils.computedValue(window.getComputedStyle(node).lineHeight);
    }

    static hasClear(box) {
        return Utils.hasClearLeft(box) || Utils.hasClearRight(box) || Utils.hasClearBoth(box);
    }

    static hasClearLeft(box) {
        return window.getComputedStyle(box.node()).clear == "left";
    }

    static hasClearRight(box) {
        return window.getComputedStyle(box.node()).clear == "right";
    }

    static hasClearBoth(box) {
        return window.getComputedStyle(box.node()).clear == "both";
    }

    static isBlockLevelElement(node) {
        if (!node)
            return false;
        let display = window.getComputedStyle(node).display;
        return  display == "block" || display == "list-item" || display == "table";
    }

    static isBlockContainerElement(node) {
        if (!node || node.nodeType != Node.ELEMENT_NODE)
            return false;
        let display = window.getComputedStyle(node).display;
        return  display == "block" || display == "list-item" || display == "inline-block" || display == "table-cell" || display == "table-caption"; //TODO && !replaced element
    }

    static isInlineLevelElement(node) {
        let display = window.getComputedStyle(node).display;
        return  display == "inline" || display == "inline-block" || display == "inline-table";
    }

    static isTableElement(node) {
        let display = window.getComputedStyle(node).display;
        return  display == "table" || display == "inline-table";
    }

    static isInlineBlockElement(node) {
        if (!node || node.nodeType != Node.ELEMENT_NODE)
            return false;
        let display = window.getComputedStyle(node).display;
        return  display == "inline-block";
    }

    static isRelativelyPositioned(box) {
        if (box.isAnonymous())
            return false;
        let node = box.node();
        return window.getComputedStyle(node).position == "relative";
    }

    static isAbsolutelyPositioned(box) {
        if (box.isAnonymous())
            return false;
        let node = box.node();
        return window.getComputedStyle(node).position == "absolute";
    }

    static isFixedPositioned(box) {
        if (box.isAnonymous())
            return false;
        let node = box.node();
        return window.getComputedStyle(node).position == "fixed";
    }

    static isStaticallyPositioned(box) {
        if (box.isAnonymous())
            return true;
        let node = box.node();
        return (Utils.propertyIsAuto("top", box) && Utils.propertyIsAuto("bottom", box)) || (Utils.propertyIsAuto("left", box) && Utils.propertyIsAuto("right", box));
    }

    static isOverflowVisible(box) {
        return window.getComputedStyle(box.node()).overflow == "visible";
    }

    static isFloatingPositioned(box) {
        if (box.isAnonymous())
            return false;
        let node = box.node();
        return window.getComputedStyle(node).float != "none";
    }

    static isFloatingLeft(box) {
        let node = box.node();
        return window.getComputedStyle(node).float == "left";
    }

    static mapPosition(position, box, container) {
        ASSERT(box instanceof Display.Box);
        ASSERT(container instanceof Display.Box);

        if (box == container)
            return position;
        for (let ascendant = box.parent(); ascendant && ascendant != container; ascendant = ascendant.parent())
            position.moveBy(ascendant.topLeft());
        return position;
    }

    static marginBox(box, container) {
        let marginBox = box.marginBox();
        let mappedPosition = Utils.mapPosition(marginBox.topLeft(), box, container);
        return new LayoutRect(mappedPosition, marginBox.size());
    }

    static borderBox(box, container) {
        let borderBox = box.borderBox();
        let mappedPosition = Utils.mapPosition(box.topLeft(), box, container);
        mappedPosition.moveBy(borderBox.topLeft());
        return new LayoutRect(mappedPosition, borderBox.size());
    }

    static contentBox(box, container) {
        let contentBox = box.contentBox();
        let mappedPosition = Utils.mapPosition(box.topLeft(), box, container);
        mappedPosition.moveBy(contentBox.topLeft());
        return new LayoutRect(mappedPosition, contentBox.size());
    }

    static textRuns(text, container) {
        return window.collectTextRuns(text, container.node());
    }

    static textRunsForLine(text, availableSpace, container) {
        return window.collectTextRuns(text, container.node(), availableSpace);
    }

    static nextBreakingOpportunity(textBox, currentPosition)
    {
        return window.nextBreakingOpportunity(textBox.content(), currentPosition);
    }

    static measureText(texBox, start, end)
    {
        return texBox.node().textWidth(start, end);
    }

    static textHeight(textBox)
    {
        return textBox.text().node().textHeight();
    }

    static layoutBoxById(layoutBoxId, box) {
        if (box.id() == layoutBoxId)
            return box;
        if (!box.isContainer())
            return null;
        // Super inefficient but this is all temporary anyway.
        for (let child = box.firstChild(); child; child = child.nextSibling()) {
            if (child.id() == layoutBoxId)
                return child;
            let foundIt = Utils.layoutBoxById(layoutBoxId, child);
            if (foundIt)
                return foundIt;
        }
        return null;
    }
    // "RenderView at (0,0) size 1317x366\n HTML RenderBlock at (0,0) size 1317x116\n  BODY RenderBody at (8,8) size 1301x100\n   DIV RenderBlock at (0,0) size 100x100\n";
    static layoutTreeDump(layoutState) {
        return this._dumpBox(layoutState, layoutState.rootContainer(), 1) + this._dumpTree(layoutState, layoutState.rootContainer(), 2);
    }

    static _dumpBox(layoutState, box, level) {
        // Skip anonymous boxes for now -This is the case where WebKit does not generate an anon inline container for text content where the text is a direct child
        // of a block container.
        let indentation = " ".repeat(level);
        if (box.isInlineBox()) {
            if (box.text())
                return indentation + "#text RenderText\n";
        }
        if (box.name() == "RenderInline") {
            if (box.isInFlowPositioned()) {
                let displayBox = layoutState.displayBox(box);
                let boxRect = displayBox.rect();
                return indentation + box.node().tagName + " " + box.name() + "  (" + Utils.precisionRoundWithDecimals(boxRect.left()) + ", " + Utils.precisionRoundWithDecimals(boxRect.top()) + ")\n";
            }
            return indentation + box.node().tagName + " " + box.name() + "\n";
        }
        if (box.isAnonymous())
            return "";
        let displayBox = layoutState.displayBox(box);
        let boxRect = displayBox.rect();
        return indentation + (box.node().tagName ? (box.node().tagName + " ") : "")  + box.name() + " at (" + Utils.precisionRound(boxRect.left()) + "," + Utils.precisionRound(boxRect.top()) + ") size " + Utils.precisionRound(boxRect.width()) + "x" + Utils.precisionRound(boxRect.height()) + "\n";
    }

    static _dumpLines(layoutState, root, level) {
        ASSERT(root.establishesInlineFormattingContext());
        let inlineFormattingState = layoutState.establishedFormattingState(root);
        let lines = inlineFormattingState.lines();
        let content = "";
        let indentation = " ".repeat(level);
        lines.forEach(function(line) {
            let lineRect = line.rect();
            content += indentation + "RootInlineBox at (" + lineRect.left() + "," + lineRect.top() + ") size " + Utils.precisionRound(lineRect.width()) + "x" + lineRect.height() + "\n";
            line.lineBoxes().forEach(function(lineBox) {
                let indentation = " ".repeat(level + 1);
                let inlineBoxName = lineBox.startPosition === undefined ? "InlineBox" : "InlineTextBox";
                content += indentation +  inlineBoxName + " at (" + Utils.precisionRound(lineBox.lineBoxRect.left()) + "," + Utils.precisionRound(lineBox.lineBoxRect.top()) + ") size " + Utils.precisionRound(lineBox.lineBoxRect.width()) + "x" + lineBox.lineBoxRect.height() + "\n";
            });
        });
        return content;
    }

    static _dumpTree(layoutState, root, level) {
        let content = "";
        if (root.isBlockContainerBox() && root.establishesInlineFormattingContext())
            content += this._dumpLines(layoutState, root, level);
        for (let child = root.firstChild(); child; child = child.nextSibling()) {
            content += this._dumpBox(layoutState, child, level);
            if (child.isContainer())
                content += this._dumpTree(layoutState, child, level + 1, content);
        }
        return content;
    }

    static precisionRoundWithDecimals(number) {
        return number.toFixed(2);
    }

    static precisionRound(number) {
        let factor = Math.pow(10, 2);
        return Math.round(number * factor) / factor;
    }
}


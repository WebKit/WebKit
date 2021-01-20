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
class FormattingState {
public:
    Layout::Container& formattingRoot();
    LayoutState& layoutState();
    FloatingState& floatingState();
    Display::Box& createDisplayBox(const Layout::Box&);

    Vector<Display::Box&> displayBoxes();
    Display::Box& displayBox(const Layout::Box&);

    void markNeedsLayout(const Layout::Box&);
    void clearNeedsLayout(const Layout::Box&);
    bool needsLayout(const Layout::Box&);

    bool needsLayout();
};
*/
class FormattingState {
    constructor(layoutState, formattingRoot) {
        this.m_layoutState = layoutState;
        this.m_formattingRoot = formattingRoot;
        this.m_floatingState = null;
        this.m_displayToLayout = new Map();
        this.m_needsLayoutBoxList = new Map();
        this._markSubTreeNeedsLayout(formattingRoot);
    }

    formattingRoot() {
        return this.m_formattingRoot;
    }

    layoutState() {
        return this.m_layoutState;
    }

    floatingState() {
        return this.m_floatingState;
    }

    createDisplayBox(layoutBox) {
        let displayBox = new Display.Box(layoutBox.node());
        let parentDisplayBox = this.displayBox(layoutBox.containingBlock());
        displayBox.setParent(parentDisplayBox);
        if (!parentDisplayBox.firstChild()) {
            parentDisplayBox.setFirstChild(displayBox);
            parentDisplayBox.setLastChild(displayBox);
        } else {
            let previousSibling = parentDisplayBox.lastChild();
            previousSibling.setNextSibling(displayBox);
            displayBox.setPreviousSibling(previousSibling);
            parentDisplayBox.setLastChild(displayBox);
        }
        this.m_displayToLayout.set(layoutBox, displayBox);
        return displayBox;
    }

    displayBoxes() {
        return this.m_displayToLayout;
    }

    displayBox(layoutBox) {
        ASSERT(layoutBox);
        // 1. Normally we only need to access boxes within the same formatting context
        // 2. In some cases we need size information about the root container -which is in the parent formatting context
        // 3. In inline formatting with parent floating state, we need display boxes from the parent formatting context
        // 4. In rare cases (statically positioned out-of-flow box), we need position information on sibling formatting context
        // but in all cases it needs to be a descendant of the root container (or the container itself)
        let displayBox = this.m_displayToLayout.get(layoutBox);
        if (displayBox)
            return displayBox;
        return this.layoutState().displayBox(layoutBox);
    }

    markNeedsLayout(layoutBox) {
        // Never mark the formatting root dirty. It belongs to the parent formatting context (or none if ICB).
        ASSERT(layoutBox != this.formattingRoot());
        this.m_needsLayoutBoxList.set(layoutBox);
        // FIXME: Let's just mark all the ancestors dirty in this formatting scope.
        let containingBlock = layoutBox.containingBlock();
        if (!containingBlock || containingBlock == this.formattingRoot())
            return;
        if (!FormattingContext.isInFormattingContext(containingBlock, this.formattingRoot()))
            return;
        if (this.needsLayout(containingBlock))
            return;

        this.markNeedsLayout(containingBlock);
    }

    clearNeedsLayout(layoutBox) {
        this.m_needsLayoutBoxList.delete(layoutBox);
    }

    needsLayout(layoutBox) {
        return this.m_needsLayoutBoxList.has(layoutBox);
    }

    // This should just be needsLayout()
    layoutNeeded() {
        return this.m_needsLayoutBoxList.size;
    }

    _markSubTreeNeedsLayout(subTreeRoot) {
        if (!subTreeRoot)
            return;
        // Only mark children that actually belong to this formatting context/state.
        if (FormattingContext.isInFormattingContext(subTreeRoot, this.formattingRoot()))
            this.markNeedsLayout(subTreeRoot);
        if (!subTreeRoot.isContainer() || !subTreeRoot.hasChild())
            return;
        for (let child = subTreeRoot.firstChild(); child; child = child.nextSibling())
            this._markSubTreeNeedsLayout(child);
    }

}

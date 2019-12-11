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
class LayoutState {
public:
    Container& rootContainer();

    FormattingContext& formattingContext(const Layout::Container& formattingRoot);
    FormattingState& establishedFormattingState(const Layout::Container& formattingRoot);
    FormattingState& formattingStateForBox(const Layout::Box&);

    Display::Box* displayBox(const Layout::Box&);

    void markNeedsLayout(const Layout::Box&);
    bool needsLayout();
};
*/

class LayoutState {
    constructor(rootContainer, rootDisplayBox) {
        ASSERT(rootContainer.establishesFormattingContext());
        this.m_rootContainer = rootContainer;
        this.m_rootDisplayBox = rootDisplayBox;

        this.m_formattingStates = new Map();
    }

    formattingContext(formattingRoot) {
        return this._formattingContext(this.establishedFormattingState(formattingRoot));
    }

    rootContainer() {
        return this.m_rootContainer;
    }

    establishedFormattingState(formattingRoot) {
        ASSERT(formattingRoot.establishesFormattingContext());
        let formattingState = this.m_formattingStates.get(formattingRoot);
        if (formattingState)
            return formattingState;
        formattingState = this._createFormattingState(formattingRoot);
        this.m_formattingStates.set(formattingRoot, formattingState);
        return formattingState;
    }

    formattingStateForBox(layoutBox) {
        for (let formattingEntry of this.m_formattingStates) {
            let formattingRoot = formattingEntry[0];
            let formattingState = formattingEntry[1];
            if (FormattingContext.isInFormattingContext(layoutBox, formattingRoot))
                return formattingState;
        }
        ASSERT_NOT_REACHED();
        return null;
    }

    markNeedsLayout(layoutBox) {
        let formattingState = this.formattingStateForBox(layoutBox);
        // Newly created formatting state will obviously mark all the boxes dirty.
        if (!formattingState)
            return;
        formattingState.markNeedsLayout(layoutBox);
    }

    needsLayout() {
        for (let formattingEntry of this.m_formattingStates) {
            let formattingState = formattingEntry[1];
            if (formattingState.layoutNeeded())
                return true;
        }
        return false;
    }

    displayBox(layoutBox) {
        for (let formattingEntry of this.m_formattingStates) {
            let formattingState = formattingEntry[1];
            let displayBox = formattingState.displayBoxes().get(layoutBox);
            if (displayBox)
                return displayBox;
        }
        // It must be the root container.
        ASSERT(layoutBox == this.m_rootContainer);
        return this.m_rootDisplayBox;
    }

    _createFormattingState(formattingRoot) {
        ASSERT(formattingRoot.establishesFormattingContext());
        if (formattingRoot.establishesInlineFormattingContext())
            return new InlineFormattingState(formattingRoot, this);
        if (formattingRoot.establishesBlockFormattingContext())
            return new BlockFormattingState(formattingRoot, this);
        ASSERT_NOT_REACHED();
        return null;
    }

    _formattingContext(formattingState) {
        if (formattingState instanceof BlockFormattingState)
            return new BlockFormattingContext(formattingState);
        if (formattingState instanceof InlineFormattingState)
            return new InlineFormattingContext(formattingState);
        ASSERT_NOT_REACHED();
        return null;
    }
}

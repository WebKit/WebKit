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

class LayoutState {
    constructor(initialDisplayBox) {
        this.m_formattingStates = new Map();
        this.m_initialDisplayBox = initialDisplayBox;
    }

    layout(formattingRoot) {
        let formattingState = this._createFormattingState(formattingRoot);
        this.m_formattingStates.set(formattingRoot, formattingState);
        this.formattingContext(formattingState).layout();
    }

    formattingContext(formattingState) {
        if (formattingState instanceof BlockFormattingState)
            return new BlockFormattingContext(formattingState);
        if (formattingState instanceof InlineFormattingState)
            return new InlineFormattingContext(formattingState);
        ASSERT_NOT_REACHED();
        return null;
    }

    formattingStates() {
        return this.m_formattingStates;
    }

    establishedFormattingState(formattingRoot) {
        ASSERT(formattingRoot.establishesFormattingContext());
        return this.m_formattingStates.get(formattingRoot);
    }

    formattingStateForBox(layoutBox) {
        for (let formattingEntry of this.formattingStates()) {
            let formattingRoot = formattingEntry[0];
            let formattingState = formattingEntry[1];
            if (FormattingContext.isInFormattingContext(layoutBox, formattingRoot))
                return formattingState;
        }
        ASSERT_NOT_REACHED();
        return null;
    }

    setNeedsLayout(layoutBox) {
        let formattingState = this.formattingStateForBox(layoutBox);
        // Newly created formatting state will obviously mark all the boxes dirty.
        if (!formattingState)
            return;
        formattingState.setNeedsLayout(layoutBox);
    }

    needsLayout() {
        for (let formattingEntry of this.formattingStates()) {
            let formattingState = formattingEntry[1];
            if (formattingState.layoutNeeded())
                return true;
        }
        return false;
    }

    displayBox(layoutBox) {
        for (let formattingEntry of this.formattingStates()) {
            let formattingState = formattingEntry[1];
            let displayBox = formattingState.displayBoxes().get(layoutBox);
            if (displayBox)
                return displayBox;
        }
        // It must be the ICB.
        ASSERT(!layoutBox.parent());
        return this.initialDisplayBox();
    }

    initialDisplayBox() {
        return this.m_initialDisplayBox;
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

}

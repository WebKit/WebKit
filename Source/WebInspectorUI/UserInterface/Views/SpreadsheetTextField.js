/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.SpreadsheetTextField = class SpreadsheetTextField
{
    constructor(delegate, element, completionProvider)
    {
        this._delegate = delegate;
        this._element = element;

        this._completionProvider = completionProvider || null;
        if (this._completionProvider) {
            this._suggestionHintElement = document.createElement("span");
            this._suggestionHintElement.contentEditable = false;
            this._suggestionHintElement.classList.add("completion-hint");
            this._suggestionsView = new WI.CompletionSuggestionsView(this, {preventBlur: true});
        }

        this._element.classList.add("spreadsheet-text-field");

        this._element.addEventListener("mousedown", this._handleMouseDown.bind(this), true);
        this._element.addEventListener("click", this._handleClick.bind(this));
        this._element.addEventListener("blur", this._handleBlur.bind(this));
        this._element.addEventListener("keydown", this._handleKeyDown.bind(this));
        this._element.addEventListener("input", this._handleInput.bind(this));

        this._editing = false;
        this._valueBeforeEditing = "";
        this._completionPrefix = "";
        this._controlSpaceKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Control, WI.KeyboardShortcut.Key.Space);
    }

    // Public

    get element() { return this._element; }

    get editing() { return this._editing; }

    get value() { return this._element.textContent; }
    set value(value) { this._element.textContent = value; }

    valueWithoutSuggestion()
    {
        let value = this._element.textContent;
        return value.slice(0, value.length - this.suggestionHint.length);
    }

    get suggestionHint()
    {
        return this._suggestionHintElement.textContent;
    }

    set suggestionHint(value)
    {
        this._suggestionHintElement.textContent = value;

        if (value)
            this._reAttachSuggestionHint();
        else
            this._suggestionHintElement.remove();
    }

    startEditing()
    {
        if (this._editing)
            return;

        if (this._delegate && typeof this._delegate.spreadsheetTextFieldWillStartEditing === "function")
            this._delegate.spreadsheetTextFieldWillStartEditing(this);

        this._editing = true;
        this._valueBeforeEditing = this.value;

        this._element.classList.add("editing");
        this._element.contentEditable = "plaintext-only";
        this._element.spellcheck = false;
        this._element.scrollIntoViewIfNeeded(false);

        this._element.focus();
        this._selectText();

        this._updateCompletions();
    }

    stopEditing()
    {
        if (!this._editing)
            return;

        this._editing = false;
        this._valueBeforeEditing = "";
        this._element.classList.remove("editing");
        this._element.contentEditable = false;

        this.discardCompletion();
    }

    discardCompletion()
    {
        if (!this._completionProvider)
            return;

        this._suggestionsView.hide();

        let hadSuggestionHint = !!this.suggestionHint;
        this.suggestionHint = "";
        if (hadSuggestionHint && this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
            this._delegate.spreadsheetTextFieldDidChange(this);
    }

    detached()
    {
        this.discardCompletion();
        this._element.remove();
    }

    // CompletionSuggestionsView delegate

    completionSuggestionsSelectedCompletion(suggestionsView, selectedText = "")
    {
        this.suggestionHint = selectedText.slice(this._completionPrefix.length);

        this._reAttachSuggestionHint();

        if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
            this._delegate.spreadsheetTextFieldDidChange(this);
    }

    completionSuggestionsClickedCompletion(suggestionsView, selectedText)
    {
        // Consider the following example:
        //
        //   border: 1px solid ro|
        //                     rosybrown
        //                     royalblue
        //
        // Clicking on "rosybrown" should replace "ro" with "rosybrown".
        //
        //           prefix:  1px solid ro
        // completionPrefix:            ro
        //        newPrefix:  1px solid
        //     selectedText:            rosybrown
        let prefix = this.valueWithoutSuggestion();
        let newPrefix = prefix.slice(0, -this._completionPrefix.length);

        this._element.textContent = newPrefix + selectedText;

        // Place text caret at the end.
        window.getSelection().setBaseAndExtent(this._element, selectedText.length, this._element, selectedText.length);

        this.discardCompletion();

        if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
            this._delegate.spreadsheetTextFieldDidChange(this);
    }

    // Private

    _selectText()
    {
        window.getSelection().selectAllChildren(this._element);
    }

    _discardChange()
    {
        if (this._valueBeforeEditing !== this.value) {
            this.value = this._valueBeforeEditing;

            if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                this._delegate.spreadsheetTextFieldDidChange(this);
        }
    }

    _handleClick(event)
    {
        this.startEditing();
    }

    _handleMouseDown(event)
    {
        if (this._editing)
            event.stopPropagation();
    }

    _handleBlur(event)
    {
        if (!this._editing)
            return;

        // Keep editing after tabbing out of Web Inspector window and back.
        if (document.activeElement === this._element)
            return;

        this._applyCompletionHint();
        this.discardCompletion();

        let changed = this._valueBeforeEditing !== this.value;
        this._delegate.spreadsheetTextFieldDidBlur(this, event, changed);
        this.stopEditing();
    }

    _handleKeyDown(event)
    {
        if (!this._editing)
            return;

        if (this._suggestionsView) {
            let consumed = this._handleKeyDownForSuggestionView(event);
            if (consumed)
                return;
        }

        let isEnterKey = event.key === "Enter";

        if (isEnterKey && event.shiftKey) {
            if (this._delegate && this._delegate.spreadsheetTextFieldAllowsNewlines(this))
                return;
        }

        let isTabKey = event.key === "Tab";
        if (isEnterKey || isTabKey) {
            event.stop();
            this._applyCompletionHint();

            let direction = (isTabKey && event.shiftKey) ? "backward" : "forward";

            if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidCommit === "function")
                this._delegate.spreadsheetTextFieldDidCommit(this, {direction});

            this.stopEditing();
            return;
        }

        if (event.key === "ArrowUp" || event.key === "ArrowDown") {
            let delta = 1;
            if (event.metaKey)
                delta = 100;
            else if (event.shiftKey)
                delta = 10;
            else if (event.altKey)
                delta = 0.1;

            if (event.key === "ArrowDown")
                delta = -delta;

            let didModify = WI.incrementElementValue(this._element, delta);
            if (!didModify)
                return;

            event.stop();

            if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                this._delegate.spreadsheetTextFieldDidChange(this);
        }

        if (event.key === "Backspace") {
            if (!this.value) {
                event.stop();
                this.stopEditing();

                if (this._delegate && this._delegate.spreadsheetTextFieldDidBackspace)
                    this._delegate.spreadsheetTextFieldDidBackspace(this);

                return;
            }
        }

        if (this._controlSpaceKeyboardShortcut.matchesEvent(event)) {
            event.stop();
            if (this._suggestionsView.visible)
                this._suggestionsView.hide();
            else {
                const forceCompletions = true;
                this._updateCompletions(forceCompletions);
            }
            return;
        }

        if (event.key === "Escape") {
            event.stop();
            this._discardChange();
            window.getSelection().removeAllRanges();

            if (this._delegate && this._delegate.spreadsheetTextFieldDidPressEsc)
                this._delegate.spreadsheetTextFieldDidPressEsc(this, this._valueBeforeEditing);
        }
    }

    _handleKeyDownForSuggestionView(event)
    {
        if ((event.key === "ArrowDown" || event.key === "ArrowUp") && this._suggestionsView.visible) {
            event.stop();

            if (event.key === "ArrowDown")
                this._suggestionsView.selectNext();
            else
                this._suggestionsView.selectPrevious();

            // Update popover position in case text moved, e.g. started or stopped wrapping.
            this._showSuggestionsView();

            if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                this._delegate.spreadsheetTextFieldDidChange(this);

            return true;
        }

        if (event.key === "ArrowRight" && this.suggestionHint) {
            let selection = window.getSelection();

            if (selection.isCollapsed && (selection.focusOffset === this.valueWithoutSuggestion().length || selection.focusNode === this._suggestionHintElement)) {
                event.stop();
                document.execCommand("insertText", false, this.suggestionHint);

                // When completing "background", don't hide the completion popover.
                // Continue showing the popover with properties such as "background-color" and "background-image".
                this._updateCompletions();

                if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                    this._delegate.spreadsheetTextFieldDidChange(this);

                return true;
            }
        }

        if (event.key === "Escape" && this._suggestionsView.visible) {
            event.stop();

            let willChange = !!this.suggestionHint;
            this.discardCompletion();

            if (willChange && this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                this._delegate.spreadsheetTextFieldDidChange(this);

            return true;
        }

        if (event.key === "ArrowLeft" && (this.suggestionHint || this._suggestionsView.visible)) {
            this.discardCompletion();

            if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
                this._delegate.spreadsheetTextFieldDidChange(this);
        }

        return false;
    }

    _handleInput(event)
    {
        if (!this._editing)
            return;

        this._updateCompletions();

        if (this._delegate && typeof this._delegate.spreadsheetTextFieldDidChange === "function")
            this._delegate.spreadsheetTextFieldDidChange(this);
    }

    _updateCompletions(forceCompletions = false)
    {
        if (!this._completionProvider)
            return;

        let valueWithoutSuggestion = this.valueWithoutSuggestion();
        let {completions, prefix} = this._completionProvider(valueWithoutSuggestion, {allowEmptyPrefix: forceCompletions});
        this._completionPrefix = prefix;

        if (!completions.length) {
            this.discardCompletion();
            return;
        }

        // No need to show the completion popover with only one item that matches the entered value.
        if (completions.length === 1 && completions[0] === valueWithoutSuggestion) {
            this.discardCompletion();
            return;
        }

        console.assert(this._element.isConnected, "SpreadsheetTextField already removed from the DOM.");
        if (!this._element.isConnected) {
            this._suggestionsView.hide();
            return;
        }

        this._suggestionsView.update(completions);

        if (completions.length === 1) {
            // No need to show the completion popover that matches the suggestion hint.
            this._suggestionsView.hide();
        } else
            this._showSuggestionsView();

        this._suggestionsView.selectedIndex = NaN;
        if (this._completionPrefix) {
            // Select first item and call completionSuggestionsSelectedCompletion.
            this._suggestionsView.selectNext();
        } else
            this.suggestionHint = "";
    }

    _showSuggestionsView()
    {
        let prefix = this.valueWithoutSuggestion();
        let startOffset = prefix.length - this._completionPrefix.length;
        let caretRect = this._getCaretRect(startOffset);

        // Hide completion popover when the anchor element is removed from the DOM.
        if (!caretRect)
            this._suggestionsView.hide();
        else {
            this._suggestionsView.show(caretRect);
            this._suggestionsView.hideWhenElementMoves(this._element);
        }
    }

    _getCaretRect(startOffset)
    {
        let selection = window.getSelection();

        let isHidden = (clientRect) => {
            return clientRect.x === 0 && clientRect.y === 0;
        };

        if (selection.rangeCount) {
            let range = selection.getRangeAt(0).cloneRange();
            range.setStart(range.startContainer, startOffset);
            let clientRect = range.getBoundingClientRect();

            if (!isHidden(clientRect)) {
                // This happens after deleting value. However, when focusing
                // on an empty value clientRect is visible.
                return WI.Rect.rectFromClientRect(clientRect);
            }
        }

        let clientRect = this._element.getBoundingClientRect();
        if (isHidden(clientRect))
            return null;

        const leftPadding = parseInt(getComputedStyle(this._element).paddingLeft) || 0;
        return new WI.Rect(clientRect.left + leftPadding, clientRect.top, clientRect.width, clientRect.height);
    }

    _applyCompletionHint()
    {
        if (!this._completionProvider || !this.suggestionHint)
            return;

        this._element.textContent = this._element.textContent;
    }

    _reAttachSuggestionHint()
    {
        if (this._suggestionHintElement.parentElement === this._element)
            return;

        this._element.append(this._suggestionHintElement);
    }
};

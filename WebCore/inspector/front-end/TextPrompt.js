/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TextPrompt = function(element, completions, stopCharacters)
{
    this.element = element;
    this.completions = completions;
    this.completionStopCharacters = stopCharacters;
    this.history = [];
    this.historyOffset = 0;
}

WebInspector.TextPrompt.prototype = {
    get text()
    {
        return this.element.textContent;
    },

    set text(x)
    {
        if (!x) {
            // Append a break element instead of setting textContent to make sure the selection is inside the prompt.
            this.element.removeChildren();
            this.element.appendChild(document.createElement("br"));
        } else
            this.element.textContent = x;

        this.moveCaretToEndOfPrompt();
    },

    handleKeyEvent: function(event)
    {
        switch (event.keyIdentifier) {
            case "Up":
                this._upKeyPressed(event);
                break;
            case "Down":
                this._downKeyPressed(event);
                break;
            case "U+0009": // Tab
                this._tabKeyPressed(event);
                break;
            case "Right":
                if (!this.acceptAutoComplete())
                    this.autoCompleteSoon();
                break;
            default:
                this.clearAutoComplete();
                this.autoCompleteSoon();
                break;
        }
    },

    acceptAutoComplete: function()
    {
        if (!this.autoCompleteElement || !this.autoCompleteElement.parentNode)
            return false;

        var text = this.autoCompleteElement.textContent;
        var textNode = document.createTextNode(text);
        this.autoCompleteElement.parentNode.replaceChild(textNode, this.autoCompleteElement);
        delete this.autoCompleteElement;

        var finalSelectionRange = document.createRange();
        finalSelectionRange.setStart(textNode, text.length);
        finalSelectionRange.setEnd(textNode, text.length);

        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(finalSelectionRange);

        return true;
    },

    clearAutoComplete: function(includeTimeout)
    {
        if (includeTimeout && "_completeTimeout" in this) {
            clearTimeout(this._completeTimeout);
            delete this._completeTimeout;
        }

        if (!this.autoCompleteElement)
            return;

        if (this.autoCompleteElement.parentNode)
            this.autoCompleteElement.parentNode.removeChild(this.autoCompleteElement);
        delete this.autoCompleteElement;

        if (!this._userEnteredRange || !this._userEnteredText)
            return;

        this._userEnteredRange.deleteContents();

        var userTextNode = document.createTextNode(this._userEnteredText);
        this._userEnteredRange.insertNode(userTextNode);           

        var selectionRange = document.createRange();
        selectionRange.setStart(userTextNode, this._userEnteredText.length);
        selectionRange.setEnd(userTextNode, this._userEnteredText.length);

        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(selectionRange);

        delete this._userEnteredRange;
        delete this._userEnteredText;
    },

    autoCompleteSoon: function()
    {
        if (!("_completeTimeout" in this))
            this._completeTimeout = setTimeout(this.complete.bind(this, true), 250);
    },

    complete: function(auto)
    {
        this.clearAutoComplete(true);
        var selection = window.getSelection();
        if (!selection.rangeCount)
            return;

        var selectionRange = selection.getRangeAt(0);
        if (!selectionRange.commonAncestorContainer.isDescendant(this.element))
            return;
        if (auto && !this.isCaretAtEndOfPrompt())
            return;
        var wordPrefixRange = selectionRange.startContainer.rangeOfWord(selectionRange.startOffset, this.completionStopCharacters, this.element, "backward");
        this.completions(wordPrefixRange, auto, this._completionsReady.bind(this, selection, auto, wordPrefixRange));
    },

    _completionsReady: function(selection, auto, originalWordPrefixRange, completions)
    {
        if (!completions || !completions.length)
            return;

        var selectionRange = selection.getRangeAt(0);
        var wordPrefixRange = selectionRange.startContainer.rangeOfWord(selectionRange.startOffset, this.completionStopCharacters, this.element, "backward");

        var fullWordRange = document.createRange();
        fullWordRange.setStart(wordPrefixRange.startContainer, wordPrefixRange.startOffset);
        fullWordRange.setEnd(selectionRange.endContainer, selectionRange.endOffset);

        if (originalWordPrefixRange.toString() + selectionRange.toString() != fullWordRange.toString())
            return;

        if (completions.length === 1 || selection.isCollapsed || auto) {
            var completionText = completions[0];
        } else {
            var currentText = fullWordRange.toString();

            var foundIndex = null;
            for (var i = 0; i < completions.length; ++i) {
                if (completions[i] === currentText)
                    foundIndex = i;
            }

            if (foundIndex === null || (foundIndex + 1) >= completions.length)
                var completionText = completions[0];
            else
                var completionText = completions[foundIndex + 1];
        }

        var wordPrefixLength = wordPrefixRange.toString().length;

        this._userEnteredRange = fullWordRange;
        this._userEnteredText = fullWordRange.toString();

        fullWordRange.deleteContents();

        var finalSelectionRange = document.createRange();

        if (auto) {
            var prefixText = completionText.substring(0, wordPrefixLength);
            var suffixText = completionText.substring(wordPrefixLength);

            var prefixTextNode = document.createTextNode(prefixText);
            fullWordRange.insertNode(prefixTextNode);           

            this.autoCompleteElement = document.createElement("span");
            this.autoCompleteElement.className = "auto-complete-text";
            this.autoCompleteElement.textContent = suffixText;

            prefixTextNode.parentNode.insertBefore(this.autoCompleteElement, prefixTextNode.nextSibling);

            finalSelectionRange.setStart(prefixTextNode, wordPrefixLength);
            finalSelectionRange.setEnd(prefixTextNode, wordPrefixLength);
        } else {
            var completionTextNode = document.createTextNode(completionText);
            fullWordRange.insertNode(completionTextNode);           

            if (completions.length > 1)
                finalSelectionRange.setStart(completionTextNode, wordPrefixLength);
            else
                finalSelectionRange.setStart(completionTextNode, completionText.length);

            finalSelectionRange.setEnd(completionTextNode, completionText.length);
        }

        selection.removeAllRanges();
        selection.addRange(finalSelectionRange);
    },

    isCaretInsidePrompt: function()
    {
        return this.element.isInsertionCaretInside();
    },

    isCaretAtEndOfPrompt: function()
    {
        var selection = window.getSelection();
        if (!selection.rangeCount || !selection.isCollapsed)
            return false;

        var selectionRange = selection.getRangeAt(0);
        var node = selectionRange.startContainer;
        if (node !== this.element && !node.isDescendant(this.element))
            return false;

        if (node.nodeType === Node.TEXT_NODE && selectionRange.startOffset < node.nodeValue.length)
            return false;

        var foundNextText = false;
        while (node) {
            if (node.nodeType === Node.TEXT_NODE && node.nodeValue.length) {
                if (foundNextText)
                    return false;
                foundNextText = true;
            }

            node = node.traverseNextNode(false, this.element);
        }

        return true;
    },

    moveCaretToEndOfPrompt: function()
    {
        var selection = window.getSelection();
        var selectionRange = document.createRange();

        var offset = this.element.childNodes.length;
        selectionRange.setStart(this.element, offset);
        selectionRange.setEnd(this.element, offset);

        selection.removeAllRanges();
        selection.addRange(selectionRange);
    },

    _tabKeyPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        this.complete();
    },

    _upKeyPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.historyOffset == this.history.length)
            return;

        this.clearAutoComplete(true);

        if (this.historyOffset == 0)
            this.tempSavedCommand = this.text;

        ++this.historyOffset;
        this.text = this.history[this.history.length - this.historyOffset];
    },

    _downKeyPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.historyOffset == 0)
            return;

        this.clearAutoComplete(true);

        --this.historyOffset;

        if (this.historyOffset == 0) {
            this.text = this.tempSavedCommand;
            delete this.tempSavedCommand;
            return;
        }

        this.text = this.history[this.history.length - this.historyOffset];
    }
}

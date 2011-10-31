/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

/**
 * @constructor
 * @param {function(Range, boolean, function(*))} completions
 * @param {string} stopCharacters
 */
WebInspector.TextPrompt = function(completions, stopCharacters)
{
    /**
     * @type {Element|undefined}
     */
    this._proxyElement;
    this._loadCompletions = completions;
    this._completionStopCharacters = stopCharacters;
}

WebInspector.TextPrompt.prototype = {
    get proxyElement()
    {
        return this._proxyElement;
    },

    /**
     * Clients should never attach any event listeners to the |element|. Instead,
     * they should use the result of this method to attach listeners for bubbling events.
     *
     * @param {Element} element
     */
    attach: function(element)
    {
        return this._attachInternal(element);
    },

    /**
     * Clients should never attach any event listeners to the |element|. Instead,
     * they should use the result of this method to attach listeners for bubbling events
     * or the |blurListener| parameter to register a "blur" event listener on the |element|
     * (since the "blur" event does not bubble.)
     *
     * @param {Element} element
     * @param {function(Event)} blurListener
     */
    attachAndStartEditing: function(element, blurListener)
    {
        this._attachInternal(element);
        this._startEditing(blurListener);
        return this.proxyElement;
    },

    _attachInternal: function(element)
    {
        if (this.proxyElement)
            throw "Cannot attach an attached TextPrompt";
        this._element = element;

        this._boundOnKeyDown = this.onKeyDown.bind(this);
        this._boundSelectStart = this._selectStart.bind(this);
        this._proxyElement = element.ownerDocument.createElement("span");
        this._proxyElement.style.display = "inline-block";
        element.parentElement.insertBefore(this.proxyElement, element);
        this.proxyElement.appendChild(element);
        this._element.addStyleClass("text-prompt");
        this._element.addEventListener("keydown", this._boundOnKeyDown, true);
        this._element.addEventListener("selectstart", this._selectStart.bind(this), false);
        return this.proxyElement;
    },

    detach: function()
    {
        this._removeFromElement();
        this.proxyElement.parentElement.insertBefore(this._element, this.proxyElement);
        this.proxyElement.parentElement.removeChild(this.proxyElement);
        delete this._proxyElement;
        if (this._element === WebInspector.currentFocusElement() || this._element.isAncestor(WebInspector.currentFocusElement()))
            WebInspector.setCurrentFocusElement(WebInspector.previousFocusElement());
    },

    get text()
    {
        return this._element.textContent;
    },

    set text(x)
    {
        this.clearAutoComplete(true);
        if (!x) {
            // Append a break element instead of setting textContent to make sure the selection is inside the prompt.
            this._element.removeChildren();
            this._element.appendChild(document.createElement("br"));
        } else
            this._element.textContent = x;

        this.moveCaretToEndOfPrompt();
        this._element.scrollIntoView();
    },

    _removeFromElement: function()
    {
        this.clearAutoComplete(true);
        this._element.removeEventListener("keydown", this._boundOnKeyDown, false);
        this._element.removeEventListener("selectstart", this._boundSelectStart, false);
        if (this._isEditing)
            this._stopEditing();
    },

    _startEditing: function(blurListener)
    {
        if (!WebInspector.markBeingEdited(this._element, true))
            return;
        this._isEditing = true;
        this._element.addStyleClass("editing");
        if (blurListener) {
            this._blurListener = blurListener;
            this._element.addEventListener("blur", this._blurListener, false);
        }
        this._oldTabIndex = this._element.tabIndex;
        if (this._element.tabIndex < 0)
            this._element.tabIndex = 0;
        WebInspector.setCurrentFocusElement(this._element);
    },

    _stopEditing: function()
    {
        this._element.tabIndex = this._oldTabIndex;
        if (this._blurListener)
            this._element.removeEventListener("blur", this._blurListener, false);
        this._element.removeStyleClass("editing");
        delete this._isEditing;
        WebInspector.markBeingEdited(this._element, false);
    },

    _selectStart: function(event)
    {
        if (this._selectionTimeout)
            clearTimeout(this._selectionTimeout);

        this.clearAutoComplete();

        function moveBackIfOutside()
        {
            delete this._selectionTimeout;
            if (!this.isCaretInsidePrompt() && window.getSelection().isCollapsed)
                this.moveCaretToEndOfPrompt();
            this.autoCompleteSoon();
        }

        this._selectionTimeout = setTimeout(moveBackIfOutside.bind(this), 100);
    },

    defaultKeyHandler: function(event)
    {
        this.clearAutoComplete();
        this.autoCompleteSoon();
        return false;
    },

    onKeyDown: function(event)
    {
        var handled = false;

        switch (event.keyIdentifier) {
            case "U+0009": // Tab
                this.complete(false, event.shiftKey);
                handled = true;
                break;
            case "Right":
            case "End":
                if (!this.acceptAutoComplete())
                    this.autoCompleteSoon();
                break;
            case "Alt":
            case "Meta":
            case "Shift":
            case "Control":
                break;
            default:
                handled = this.defaultKeyHandler(event)
                break;
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
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

    /**
     * @param {boolean=} includeTimeout
     */
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
        this._element.pruneEmptyTextNodes();

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

    complete: function(auto, reverse)
    {
        this.clearAutoComplete(true);
        var selection = window.getSelection();
        if (!selection.rangeCount)
            return;

        var selectionRange = selection.getRangeAt(0);
        var isEmptyInput = selectionRange.commonAncestorContainer === this._element; // this._element has no child Text nodes.

        // Do not attempt to auto-complete an empty input in the auto mode (only on demand).
        if (auto && isEmptyInput)
            return;
        if (!auto && !isEmptyInput && !selectionRange.commonAncestorContainer.isDescendant(this._element))
            return;
        if (auto && !this.isCaretAtEndOfPrompt())
            return;
        var wordPrefixRange = selectionRange.startContainer.rangeOfWord(selectionRange.startOffset, this._completionStopCharacters, this._element, "backward");
        this._loadCompletions(wordPrefixRange, auto, this._completionsReady.bind(this, selection, auto, wordPrefixRange, reverse));
    },

    _completionsReady: function(selection, auto, originalWordPrefixRange, reverse, completions)
    {
        if (!completions || !completions.length)
            return;

        var selectionRange = selection.getRangeAt(0);

        var fullWordRange = document.createRange();
        fullWordRange.setStart(originalWordPrefixRange.startContainer, originalWordPrefixRange.startOffset);
        fullWordRange.setEnd(selectionRange.endContainer, selectionRange.endOffset);

        if (originalWordPrefixRange.toString() + selectionRange.toString() != fullWordRange.toString())
            return;

        var wordPrefixLength = originalWordPrefixRange.toString().length;

        if (auto)
            var completionText = completions[0];
        else {
            if (completions.length === 1) {
                var completionText = completions[0];
                wordPrefixLength = completionText.length;
            } else {
                var commonPrefix = completions[0];
                for (var i = 0; i < completions.length; ++i) {
                    var completion = completions[i];
                    var lastIndex = Math.min(commonPrefix.length, completion.length);
                    for (var j = wordPrefixLength; j < lastIndex; ++j) {
                        if (commonPrefix[j] !== completion[j]) {
                            commonPrefix = commonPrefix.substr(0, j);
                            break;
                        }
                    }
                }
                wordPrefixLength = commonPrefix.length;

                if (selection.isCollapsed)
                    var completionText = completions[0];
                else {
                    var currentText = fullWordRange.toString();

                    var foundIndex = null;
                    for (var i = 0; i < completions.length; ++i) {
                        if (completions[i] === currentText)
                            foundIndex = i;
                    }

                    var nextIndex = foundIndex + (reverse ? -1 : 1);
                    if (foundIndex === null || nextIndex >= completions.length)
                        var completionText = completions[0];
                    else if (nextIndex < 0)
                        var completionText = completions[completions.length - 1];
                    else
                        var completionText = completions[nextIndex];
                }
            }
        }

        this._userEnteredRange = fullWordRange;
        this._userEnteredText = fullWordRange.toString();

        fullWordRange.deleteContents();
        this._element.pruneEmptyTextNodes();

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
        return this._element.isInsertionCaretInside();
    },

    isCaretAtEndOfPrompt: function()
    {
        var selection = window.getSelection();
        if (!selection.rangeCount || !selection.isCollapsed)
            return false;

        var selectionRange = selection.getRangeAt(0);
        var node = selectionRange.startContainer;
        if (node !== this._element && !node.isDescendant(this._element))
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

            node = node.traverseNextNode(this._element);
        }

        return true;
    },

    isCaretOnFirstLine: function()
    {
        var selection = window.getSelection();
        var focusNode = selection.focusNode;
        if (!focusNode || focusNode.nodeType !== Node.TEXT_NODE || focusNode.parentNode !== this._element)
            return true;

        if (focusNode.textContent.substring(0, selection.focusOffset).indexOf("\n") !== -1)
            return false;
        focusNode = focusNode.previousSibling;

        while (focusNode) {
            if (focusNode.nodeType !== Node.TEXT_NODE)
                return true;
            if (focusNode.textContent.indexOf("\n") !== -1)
                return false;
            focusNode = focusNode.previousSibling;
        }

        return true;
    },

    isCaretOnLastLine: function()
    {
        var selection = window.getSelection();
        var focusNode = selection.focusNode;
        if (!focusNode || focusNode.nodeType !== Node.TEXT_NODE || focusNode.parentNode !== this._element)
            return true;

        if (focusNode.textContent.substring(selection.focusOffset).indexOf("\n") !== -1)
            return false;
        focusNode = focusNode.nextSibling;

        while (focusNode) {
            if (focusNode.nodeType !== Node.TEXT_NODE)
                return true;
            if (focusNode.textContent.indexOf("\n") !== -1)
                return false;
            focusNode = focusNode.nextSibling;
        }

        return true;
    },

    moveCaretToEndOfPrompt: function()
    {
        var selection = window.getSelection();
        var selectionRange = document.createRange();

        var offset = this._element.childNodes.length;
        selectionRange.setStart(this._element, offset);
        selectionRange.setEnd(this._element, offset);

        selection.removeAllRanges();
        selection.addRange(selectionRange);
    }
}

/**
 * @constructor
 * @extends {WebInspector.TextPrompt}
 * @param {function(Range, boolean, function(*))} completions
 * @param {string} stopCharacters
 */
WebInspector.TextPromptWithHistory = function(completions, stopCharacters)
{
    WebInspector.TextPrompt.call(this, completions, stopCharacters);

    /**
     * @type {Array.<string>}
     */
    this._data = [];

    /**
     * 1-based entry in the history stack.
     * @type {number}
     */
    this._historyOffset = 1;

    /**
     * Whether to coalesce duplicate items in the history, default is true.
     * @type {boolean}
     */
    this._coalesceHistoryDupes = true;
}

WebInspector.TextPromptWithHistory.prototype = {
    get historyData()
    {
        // FIXME: do we need to copy this?
        return this._data;
    },

    setCoalesceHistoryDupes: function(x)
    {
        this._coalesceHistoryDupes = x;
    },

    /**
     * @param {Array.<string>} data
     */
    setHistoryData: function(data)
    {
        this._data = [].concat(data);
        this._historyOffset = 1;
    },

    /**
     * Pushes a committed text into the history.
     * @param {string} text
     */
    pushHistoryItem: function(text)
    {
        if (this._uncommittedIsTop) {
            this._data.pop();
            delete this._uncommittedIsTop;
        }

        if (this._coalesceHistoryDupes && text === this._currentHistoryItem())
            return;
        this._data.push(text);
        this._historyOffset = 1;
    },

    /**
     * Pushes the current (uncommitted) text into the history.
     */
    _pushCurrentText: function()
    {
        if (this._uncommittedIsTop)
            this._data.pop(); // Throw away obsolete uncommitted text.
        this._uncommittedIsTop = true;
        this.clearAutoComplete(true);
        this._data.push(this.text);
    },

    /**
     * @return {string|undefined}
     */
    _previous: function()
    {
        if (this._historyOffset > this._data.length)
            return undefined;
        if (this._historyOffset === 1)
            this._pushCurrentText();
        ++this._historyOffset;
        return this._currentHistoryItem();
    },

    /**
     * @return {string|undefined}
     */
    _next: function()
    {
        if (this._historyOffset === 1)
            return undefined;
        --this._historyOffset;
        return this._currentHistoryItem();
    },

    _currentHistoryItem: function()
    {
        return this._data[this._data.length - this._historyOffset];
    },

    defaultKeyHandler: function(event)
    {
        var newText;
        var isPrevious;

        switch (event.keyIdentifier) {
        case "Up":
            if (!this.isCaretOnFirstLine())
                break;
            newText = this._previous();
            isPrevious = true;
            break;
        case "Down":
            if (!this.isCaretOnLastLine())
                break;
            newText = this._next();
            break;
        case "U+0050": // Ctrl+P = Previous
            if (WebInspector.isMac() && event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey) {
                newText = this._previous();
                isPrevious = true;
            }
            break;
        case "U+004E": // Ctrl+N = Next
            if (WebInspector.isMac() && event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey)
                newText = this._next();
            break;
        }

        if (newText !== undefined) {
            event.stopPropagation();
            event.preventDefault();
            this.text = newText;

            if (isPrevious) {
                var firstNewlineIndex = this.text.indexOf("\n");
                if (firstNewlineIndex === -1)
                    this.moveCaretToEndOfPrompt();
                else {
                    var selection = window.getSelection();
                    var selectionRange = document.createRange();

                    selectionRange.setStart(this._element.firstChild, firstNewlineIndex);
                    selectionRange.setEnd(this._element.firstChild, firstNewlineIndex);

                    selection.removeAllRanges();
                    selection.addRange(selectionRange);
                }
            }

            return true;
        }

        return WebInspector.TextPrompt.prototype.defaultKeyHandler.call(this, event);
    }
}

WebInspector.TextPromptWithHistory.prototype.__proto__ = WebInspector.TextPrompt.prototype;

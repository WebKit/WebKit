/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 */
WebInspector.JavaScriptOutlineDialog = function(panel, view)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "javaScriptOutlineDialog.css", false);
    xhr.send(null);

    this._element = document.createElement("div");
    this._element.className = "js-outline-dialog";
    this._element.addEventListener("keydown", this._onKeyDown.bind(this), false);
    this._element.addEventListener("mousemove", this._onMouseMove.bind(this), false);
    this._element.addEventListener("click", this._onClick.bind(this), false);
    var styleElement = this._element.createChild("style");
    styleElement.type = "text/css";
    styleElement.textContent = xhr.responseText;

    this._closeKeys = [
        WebInspector.KeyboardShortcut.Keys.Enter.code,
        WebInspector.KeyboardShortcut.Keys.Esc.code,
    ];
    this._itemElements = [];
    this._previousInputLength = 0;
    this._visibleElements = [];

    var dialogWindow = this._element;
    this._boundBlurHandler = this._onBlur.bind(this);

    this._promptElement = dialogWindow.createChild("input", "monospace");
    this._promptElement.type = "text";
    this._promptElement.setAttribute("spellcheck", "false");
    this._promptElement.addEventListener("blur", this._boundBlurHandler, false);

    function onMouseDown(event)
    {
        if (event.target === this._promptElement)
            return;
        var itemElement = event.target.enclosingNodeOrSelfWithClass("item");
        if (!itemElement) {
            event.stopPropagation();
            event.preventDefault();
        }

        // Prevent blur listener from prematurely closing the dialog when an item is clicked.
        this._promptElement.removeEventListener("blur", this._boundBlurHandler, false);
    }
    this._element.addEventListener("mousedown", onMouseDown.bind(this), false);

    this._progressElement = dialogWindow.createChild("div", "progress");

    this._functionItemContainer = document.createElement("div");
    this._functionItemContainer.className = "container monospace";
    this._functionItemContainer.addEventListener("scroll", this._onScroll.bind(this), false);

    this._panel = panel;
    this._view = view;

    this._panel.requestVisibleScriptOutline();
    this._resizeWindow();

    this._previousFocusElement = WebInspector.currentFocusElement();
    WebInspector.setCurrentFocusElement(this._promptElement);

    this._highlighter = new WebInspector.JavaScriptOutlineDialog.MatchHighlighter(this);
}

/**
 * @param {{chunk, index, total, id}} data
 */
WebInspector.JavaScriptOutlineDialog.didAddChunk = function(data)
{
    var instance = WebInspector.JavaScriptOutlineDialog._instance;
    if (!instance)
        return;

    if (data.id !== instance._view.uiSourceCode.id)
        return;

    instance._appendItemElements(data.chunk);

    if (data.index === data.total) {
        instance._progressElement.style.backgroundImage = "";
        instance._filterFunctions();
    } else {
        const color = "rgb(66, 129, 235)";
        const percent = ((data.index / data.total) * 100) + "%";
        instance._progressElement.style.backgroundImage = "-webkit-linear-gradient(left, " + color + ", " + color + " " + percent + ",  transparent " + percent + ")";
    }
},

WebInspector.JavaScriptOutlineDialog.install = function(panel, viewGetter)
{
    function showJavaScriptOutlineDialog()
    {
         var view = viewGetter();
         if (view)
             WebInspector.JavaScriptOutlineDialog._show(panel, view);
    }

    var javaScriptOutlineShortcut = WebInspector.JavaScriptOutlineDialog.createShortcut();
    panel.registerShortcut(javaScriptOutlineShortcut.key, showJavaScriptOutlineDialog);
}

WebInspector.JavaScriptOutlineDialog._show = function(panel, sourceView)
{
    if (!sourceView || !sourceView.canHighlightLine())
        return;
    if (WebInspector.JavaScriptOutlineDialog._instance)
        return;
    WebInspector.JavaScriptOutlineDialog._instance = new WebInspector.JavaScriptOutlineDialog(panel, sourceView);
}

WebInspector.JavaScriptOutlineDialog.createShortcut = function()
{
    if (WebInspector.isMac())
        return WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.Meta);
    return WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.Ctrl);
}

WebInspector.JavaScriptOutlineDialog.prototype = {
    _resizeWindow: function()
    {
        const minWidth = 500;
        const minHeight = 204;
        const totalWidthDelta = 40;
        const totalHeightDelta = 78;
        this._element.appendChild(this._functionItemContainer);
        var width = Math.max(this._view.element.offsetWidth * 2 / 3, minWidth);
        var height = Math.max(this._view.element.offsetHeight * 2 / 3, minHeight);

        const shadowPadding = 10;
        this._element.positionAt(
                this._view.element.totalOffsetLeft() + Math.max(this._view.element.offsetWidth - width - shadowPadding, shadowPadding),
                this._view.element.totalOffsetTop() + Math.max((this._view.element.offsetHeight - height) / 2 + shadowPadding, shadowPadding));
        this._element.style.width = width + "px";
        this._element.style.height = height + "px";

        document.body.appendChild(this._element);
    },

    _appendItemElements: function(chunk)
    {
        var regExp;
        if (this._promptElement.value)
            regExp = this._createSearchRegExp(this._promptElement.value);
        var fragment = document.createDocumentFragment();
        var candidateItem = this._selectedElement;
        for (var i = 0; i < chunk.length; ++i) {
            var isVisible = true;
            var entry = chunk[i];
            var functionItem = document.createElement("div");
            functionItem.className = "item";
            functionItem._text = entry.name;
            functionItem._arguments = (entry.arguments ? entry.arguments : "");
            functionItem.textContent = functionItem._text + functionItem._arguments;
            if (regExp && !regExp.test(functionItem._text)) {
                functionItem.style.display = "none";
                isVisible = false;
            }
            functionItem._line = entry.line;
            this._itemElements.push(functionItem);
            if (!candidateItem && isVisible)
                candidateItem = functionItem;
            fragment.appendChild(functionItem);
            this._visibleElements.push(functionItem);
        }
        this._functionItemContainer.appendChild(fragment);
        this._updateSelection(candidateItem);
    },

    /**
     * @param {boolean=} isGlobal
     */
    _createSearchRegExp: function(query, isGlobal)
    {
        var trimmedQuery = query.trim();
        var regExpString = trimmedQuery.escapeForRegExp().replace(/\\\*/g, ".*").replace(/(?!^)([A-Z])/g, "[^A-Z]*$1");
        var isSuffix = (query.charAt(query.length - 1) === " ");
        if (isSuffix)
            regExpString += "$";
        return new RegExp(regExpString, (trimmedQuery === trimmedQuery.toLowerCase() ? "i" : "") + (isGlobal ? "g" : ""));
    },

    _filterFunctions: function()
    {
        delete this._filterTimer;

        var query = this._promptElement.value;
        var charsAdded = this._previousInputLength < query.length;
        this._previousInputLength = query.length;
        if (!query.trim()) {
            this._visibleElements = [];
            for (var i = 0; i < this._itemElements.length; ++i) {
                var element = this._itemElements[i];
                element.style.display = "";
                this._visibleElements.push(element);
            }
            this._updateSelection(this._functionItemContainer.firstChild);
            delete this._query;
            this._highlighter.clearHighlight();
            return;
        }

        this._query = query;
        var regExp = this._createSearchRegExp(query);
        var firstElement;
        var newVisibleElements = [];
        if (charsAdded) {
            for (var i = 0; i < this._visibleElements.length; ++i) {
                var element = this._visibleElements[i];
                if (regExp.test(element._text)) {
                    if (!firstElement)
                        firstElement = element;
                    newVisibleElements.push(element);
                } else
                    element.style.display = "none";
            }
            this._visibleElements = newVisibleElements;
            this._updateSelection(firstElement);
            this._highlighter.highlightViewportItems(query);
            return;
        }

        for (var i = 0; i < this._itemElements.length; ++i) {
            var element = this._itemElements[i];
            if (regExp.test(element._text)) {
                if (!firstElement)
                    firstElement = element;
                element.style.display = "";
                newVisibleElements.push(element);
            } else
                element.style.display = "none";
        }
        this._visibleElements = newVisibleElements;
        this._updateSelection(firstElement);
        this._highlighter.highlightViewportItems(query);
    },

    _selectFirstItem: function()
    {
        var selectedElement;
        for (var child = this._functionItemContainer.firstChild; child; child = child.nextSibling) {
            if (child.style.display !== "none") {
                selectedElement = child;
                break;
            }
        }
        this._updateSelection(selectedElement);
    },

    _hide: function()
    {
        if (this._isHiding)
            return;
        this._isHiding = true;
        if (this._filterTimer)
            clearTimeout(this._filterTimer);
        this._element.parentElement.removeChild(this._element);
        WebInspector.JavaScriptOutlineDialog._instance = null;

        WebInspector.setCurrentFocusElement(this._previousFocusElement);
    },

    _onBlur: function(event)
    {
        this._hide();
    },

    _onKeyDown: function(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Tab.code) {
            event.preventDefault();
            return;
        }

        function previousItem(itemElement, isPageScroll)
        {
            var scrollItemsLeft = isPageScroll && this._rowsPerViewport ? this._rowsPerViewport : 1;
            var candidate = itemElement;
            var lastVisibleCandidate = candidate;
            do {
                candidate = candidate.previousSibling;
                if (!candidate) {
                    if (isPageScroll)
                        return lastVisibleCandidate;
                    else
                        candidate = this._functionItemContainer.lastChild;
                }
                if (candidate.style.display === "none")
                    continue;
                lastVisibleCandidate = candidate;
                --scrollItemsLeft;
            } while (scrollItemsLeft && candidate !== this._selectedElement);

            return candidate;
        }

        function nextItem(itemElement, isPageScroll)
        {
            var scrollItemsLeft = isPageScroll && this._rowsPerViewport ? this._rowsPerViewport : 1;
            var candidate = itemElement;
            var lastVisibleCandidate = candidate;
            do {
                candidate = candidate.nextSibling;
                if (!candidate) {
                    if (isPageScroll)
                        return lastVisibleCandidate;
                    else
                        candidate = this._functionItemContainer.firstChild;
                }
                if (candidate.style.display === "none")
                    continue;
                lastVisibleCandidate = candidate;
                --scrollItemsLeft;
            } while (scrollItemsLeft && candidate !== this._selectedElement);

            return candidate;
        }

        var delta = 1;
        var isPageScroll = false;

        switch (event.keyCode) {
        case WebInspector.KeyboardShortcut.Keys.Up.code:
        case WebInspector.KeyboardShortcut.Keys.PageUp.code:
            if (!this._selectedElement)
                return;
            if (event.keyCode === WebInspector.KeyboardShortcut.Keys.PageUp.code)
                isPageScroll = true;

            var candidate = previousItem.call(this, this._selectedElement, isPageScroll);
            this._updateSelection(candidate);
            event.preventDefault();
            return;

        case WebInspector.KeyboardShortcut.Keys.Down.code:
        case WebInspector.KeyboardShortcut.Keys.PageDown.code:
            if (!this._selectedElement)
                return;
            if (event.keyCode === WebInspector.KeyboardShortcut.Keys.PageDown.code)
                isPageScroll = true;

            var candidate = nextItem.call(this, this._selectedElement, isPageScroll);
            this._updateSelection(candidate);
            event.preventDefault();
            return;
        }

        if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Enter.code)
            this._highlightFunctionLine();

        if (this._closeKeys.indexOf(event.keyCode) >= 0) {
            this._hide();
            event.preventDefault();
            event.stopPropagation();
        } else if (event.keyIdentifier !== "Shift" && event.keyIdentifier !== "Ctrl" && event.keyIdentifier !== "Meta" && event.keyIdentifier !== "Left" && event.keyIdentifier !== "Right")
            this._scheduleFilter();
    },

    _scheduleFilter: function()
    {
        if (this._filterTimer)
            return;
        this._filterTimer = setTimeout(this._filterFunctions.bind(this), 0);
    },

    _updateSelection: function(newSelectedElement)
    {
        if (this._selectedElement === newSelectedElement)
            return;
        if (this._selectedElement)
            this._selectedElement.removeStyleClass("selected");

        this._selectedElement = newSelectedElement;
        if (newSelectedElement) {
            newSelectedElement.addStyleClass("selected");
            newSelectedElement.scrollIntoViewIfNeeded(false);
            if (!this._itemHeight) {
                this._itemHeight = newSelectedElement.offsetHeight;
                this._rowsPerViewport = Math.floor(this._functionItemContainer.offsetHeight / this._itemHeight);
            }
        }
    },

    _onClick: function(event)
    {
        var itemElement = event.target.enclosingNodeOrSelfWithClass("item");
        if (!itemElement)
            return;
        this._updateSelection(itemElement);
        this._highlightFunctionLine();
        this._hide();
    },

    _onMouseMove: function(event)
    {
        var itemElement = event.target.enclosingNodeOrSelfWithClass("item");
        if (!itemElement)
            return;
        this._updateSelection(itemElement);
    },

    _onScroll: function()
    {
        if (this._query)
            this._highlighter.highlightViewportItems(this._query);
        else
            this._highlighter.clearHighlight();
    },

    _highlightFunctionLine: function()
    {
        var lineNumber = this._selectedElement._line;
        if (!isNaN(lineNumber) && lineNumber >= 0)
            this._view.highlightLine(lineNumber);
    }
}

/**
* @constructor
*/
WebInspector.JavaScriptOutlineDialog.MatchHighlighter = function(dialog)
{
    this._dialog = dialog;
}

WebInspector.JavaScriptOutlineDialog.MatchHighlighter.prototype = {
    highlightViewportItems: function(query)
    {
        var regex = this._dialog._createSearchRegExp(query, true);
        var range = this._viewportRowRange();
        var visibleElements = this._dialog._visibleElements;
        for (var i = range[0]; i < range[1]; ++i)
            this._highlightItem(visibleElements[i], regex);
    },

    clearHighlight: function()
    {
        var range = this._viewportRowRange();
        var visibleElements = this._dialog._visibleElements;
        for (var i = range[0]; i < range[1]; ++i) {
            var element = visibleElements[i];
            if (element._domChanges) {
                revertDomChanges(element._domChanges);
                delete element._domChanges;
            }
        }
    },

    _highlightItem: function(element, regex)
    {
        if (element._domChanges) {
            revertDomChanges(element._domChanges);
            delete element._domChanges;
        }

        var text = element._text;
        var ranges = [];

        var match;
        while ((match = regex.exec(text)) !== null) {
            ranges.push({ offset: match.index, length: regex.lastIndex - match.index });
        }

        var changes = [];
        highlightRangesWithStyleClass(element, ranges, "highlight", changes);

        if (changes.length)
            element._domChanges = changes;
    },

    _viewportRowRange: function()
    {
        var dialog = this._dialog;
        var startIndex = Math.floor(dialog._functionItemContainer.scrollTop / dialog._itemHeight);
        var endIndex = Math.min(dialog._visibleElements.length, startIndex + dialog._rowsPerViewport + 1);
        return [startIndex, endIndex];
    }
}

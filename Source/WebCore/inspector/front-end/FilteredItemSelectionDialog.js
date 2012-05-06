/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @extends {WebInspector.DialogDelegate}
 * @param {WebInspector.SelectionDialogContentProvider} delegate
 */
WebInspector.FilteredItemSelectionDialog = function(delegate)
{
    WebInspector.DialogDelegate.call(this);

    var xhr = new XMLHttpRequest();
    xhr.open("GET", "filteredItemSelectionDialog.css", false);
    xhr.send(null);

    this.element = document.createElement("div");
    this.element.className = "js-outline-dialog";
    this.element.addEventListener("keydown", this._onKeyDown.bind(this), false);
    this.element.addEventListener("mousemove", this._onMouseMove.bind(this), false);
    this.element.addEventListener("click", this._onClick.bind(this), false);
    var styleElement = this.element.createChild("style");
    styleElement.type = "text/css";
    styleElement.textContent = xhr.responseText;

    this._previousInputLength = 0;
    this._itemElements = [];
    this._elementIndexes = new Map();
    this._elementHighlightChanges = new Map();

    this._promptElement = this.element.createChild("input", "monospace");
    this._promptElement.type = "text";
    this._promptElement.setAttribute("spellcheck", "false");

    this._progressElement = this.element.createChild("div", "progress");

    this._itemElementsContainer = document.createElement("div");
    this._itemElementsContainer.className = "container monospace";
    this._itemElementsContainer.addEventListener("scroll", this._onScroll.bind(this), false);
    this.element.appendChild(this._itemElementsContainer);

    this._delegate = delegate;

    this._delegate.requestItems(this._itemsLoaded.bind(this));
}

WebInspector.FilteredItemSelectionDialog.prototype = {
    /**
     * @param {Element} element
     * @param {Element} relativeToElement
     */
    position: function(element, relativeToElement)
    {
        const minWidth = 500;
        const minHeight = 204;
        var width = Math.max(relativeToElement.offsetWidth * 2 / 3, minWidth);
        var height = Math.max(relativeToElement.offsetHeight * 2 / 3, minHeight);

        this.element.style.width = width + "px";
        this.element.style.height = height + "px";

        const shadowPadding = 20; // shadow + padding
        element.positionAt(
            relativeToElement.totalOffsetLeft() + Math.max((relativeToElement.offsetWidth - width - 2 * shadowPadding) / 2, shadowPadding),
            relativeToElement.totalOffsetTop() + Math.max((relativeToElement.offsetHeight - height - 2 * shadowPadding) / 2, shadowPadding));
    },

    focus: function()
    {
        WebInspector.setCurrentFocusElement(this._promptElement);
    },

    willHide: function()
    {
        if (this._isHiding)
            return;
        this._isHiding = true;
        if (this._filterTimer)
            clearTimeout(this._filterTimer);
    },

    onEnter: function()
    {
        if (!this._selectedElement)
            return;
        this._delegate.selectItem(this._elementIndexes.get(this._selectedElement));
    },

    /**
     * @param {number} index
     * @param {number} chunkLength
     * @param {number} chunkIndex
     * @param {number} chunkCount
     */
    _itemsLoaded: function(index, chunkLength, chunkIndex, chunkCount)
    {
        var fragment = document.createDocumentFragment();
        var candidateItem = this._selectedElement;
        var regex = this._createSearchRegExp(this._promptElement.value);
        for (var i = index; i < index + chunkLength; ++i) {
            var itemElement = this._createItemElement(i, this._delegate.itemTitleAt(i));
            if (regex.test(this._delegate.itemKeyAt(i))) {
                if (!candidateItem)
                    candidateItem = itemElement;
            } else
                this._hideItemElement(itemElement);
            fragment.appendChild(itemElement);
        }
        this._itemElementsContainer.appendChild(fragment);
        this._updateSelection(candidateItem);

        if (chunkIndex === chunkCount)
            this._progressElement.style.backgroundImage = "";
        else {
            const color = "rgb(66, 129, 235)";
            const percent = ((chunkIndex / chunkCount) * 100) + "%";
            this._progressElement.style.backgroundImage = "-webkit-linear-gradient(left, " + color + ", " + color + " " + percent + ",  transparent " + percent + ")";
        }
    },

    /**
     * @param {number} index
     * @param {string} title
     */
    _createItemElement: function(index, title)
    {
        if (this._itemElements[index])
            return this._itemElements[index];

        var itemElement = document.createElement("div");
        itemElement.className = "item";
        itemElement.textContent = title;
        this._elementIndexes.put(itemElement, index);
        this._itemElements.push(itemElement);

        return itemElement;
    },

    /**
     * @param {Element} itemElement
     */
    _hideItemElement: function(itemElement)
    {
        itemElement.style.display = "none";
    },

    /**
     * @param {Element} itemElement
     */
    _itemElementVisible: function(itemElement)
    {
        return itemElement.style.display !== "none";
    },

    /**
     * @param {Element} itemElement
     */
    _showItemElement: function(itemElement)
    {
        itemElement.style.display = "";
    },

    /**
     * @param {?string} query
     * @param {boolean=} isGlobal
     */
    _createSearchRegExp: function(query, isGlobal)
    {
        if (!query || !query.trim())
            return new RegExp(".*");

        var trimmedQuery = query.trim();

        var ignoreCase = (trimmedQuery === trimmedQuery.toLowerCase());

        const toEscape = "^[]{}()\\.$*+?|";

        var regExpString = "";
        for (var i = 0; i < trimmedQuery.length; ++i) {
            var c = trimmedQuery.charAt(i);
            if (toEscape.indexOf(c) !== -1)
                c = "\\" + c;
            if (i)
                regExpString += "[^" + c + "]*";
            regExpString += c;
        }

        return new RegExp(regExpString, (ignoreCase ? "i" : "") + (isGlobal ? "g" : ""));
    },

    _filterItems: function()
    {
        delete this._filterTimer;

        var query = this._promptElement.value;
        var charsAdded = this._previousInputLength < query.length;
        this._previousInputLength = query.length;
        query = query.trim();
        var regex = this._createSearchRegExp(query);

        var firstElement;
        for (var i = 0; i < this._itemElements.length; ++i) {
            var itemElement = this._itemElements[i];
            
            if (this._itemElementVisible(itemElement)) { 
                if (!regex.test(this._delegate.itemKeyAt(i)))
                    this._hideItemElement(itemElement);
            } else if (!charsAdded && regex.test(this._delegate.itemKeyAt(i)))
                this._showItemElement(itemElement);
            
            if (!firstElement && this._itemElementVisible(itemElement))
                firstElement = itemElement;
        }

        this._updateSelection(firstElement);
        if (query) {
            this._highlightItems(query);
            this._query = query;
        } else {
            this._clearHighlight();
            delete this._query;
        }
    },

    _onKeyDown: function(event)
    {
        function nextItem(itemElement, isPageScroll, forward)
        {
            var scrollItemsLeft = isPageScroll && this._rowsPerViewport ? this._rowsPerViewport : 1;
            var candidate = itemElement;
            var lastVisibleCandidate = candidate;
            do {
                candidate = forward ? candidate.nextSibling : candidate.previousSibling;
                if (!candidate) {
                    if (isPageScroll)
                        return lastVisibleCandidate;
                    else
                        candidate = forward ? this._itemElementsContainer.firstChild : this._itemElementsContainer.lastChild;
                }
                if (!this._itemElementVisible(candidate))
                    continue;
                lastVisibleCandidate = candidate;
                --scrollItemsLeft;
            } while (scrollItemsLeft && candidate !== this._selectedElement);

            return candidate;
        }

        if (this._selectedElement) {
            var candidate;
            switch (event.keyCode) {
            case WebInspector.KeyboardShortcut.Keys.Down.code:
                candidate = nextItem.call(this, this._selectedElement, false, true);
                break;
            case WebInspector.KeyboardShortcut.Keys.Up.code:
                candidate = nextItem.call(this, this._selectedElement, false, false);
                break;
            case WebInspector.KeyboardShortcut.Keys.PageDown.code:
                candidate = nextItem.call(this, this._selectedElement, true, true);
                break;
            case WebInspector.KeyboardShortcut.Keys.PageUp.code:
                candidate = nextItem.call(this, this._selectedElement, true, false);
                break;
            }

            if (candidate) {
                this._updateSelection(candidate);
                event.preventDefault();
                return;
            }
        }

        if (event.keyIdentifier !== "Shift" && event.keyIdentifier !== "Ctrl" && event.keyIdentifier !== "Meta" && event.keyIdentifier !== "Left" && event.keyIdentifier !== "Right")
            this._scheduleFilter();
    },

    _scheduleFilter: function()
    {
        if (this._filterTimer)
            return;
        this._filterTimer = setTimeout(this._filterItems.bind(this), 0);
    },

    /**
     * @param {Element} newSelectedElement
     */
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
                this._rowsPerViewport = Math.floor(this._itemElementsContainer.offsetHeight / this._itemHeight);
            }
        }
    },

    _onClick: function(event)
    {
        var itemElement = event.target.enclosingNodeOrSelfWithClass("item");
        if (!itemElement)
            return;
        this._updateSelection(itemElement);
        this._delegate.selectItem(this._elementIndexes.get(this._selectedElement));
        WebInspector.Dialog.hide();
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
            this._highlightItems(this._query);
        else
            this._clearHighlight();
    },

    /**
     * @param {!string} query
     */
    _highlightItems: function(query)
    {
        var regex = this._createSearchRegExp(query, true);
        for (var i = 0; i < this._delegate.itemsCount(); ++i) {
            var itemElement = this._itemElements[i];
            if (this._itemElementVisible(itemElement) && this._itemElementInViewport(itemElement))
                this._highlightItem(itemElement, regex);
        }
    },

    _clearHighlight: function()
    {
        for (var i = 0; i < this._delegate.itemsCount(); ++i)
            this._clearElementHighlight(this._itemElements[i]);
    },

    /**
     * @param {Element} itemElement
     */
    _clearElementHighlight: function(itemElement)
    {
        var changes = this._elementHighlightChanges.get(itemElement)
        if (changes) {
            WebInspector.revertDomChanges(changes);
            this._elementHighlightChanges.remove(itemElement);
        }
    },

    /**
     * @param {Element} itemElement
     * @param {RegExp} regex
     */
    _highlightItem: function(itemElement, regex)
    {
        this._clearElementHighlight(itemElement);

        var key = this._delegate.itemKeyAt(this._elementIndexes.get(itemElement));
        var ranges = [];

        var match;
        while ((match = regex.exec(key)) !== null) {
            ranges.push({ offset: match.index, length: regex.lastIndex - match.index });
        }

        var changes = [];
        WebInspector.highlightRangesWithStyleClass(itemElement, ranges, "highlight", changes);

        if (changes.length)
            this._elementHighlightChanges.put(itemElement, changes);
    },

    /**
     * @param {Element} itemElement
     * @return {boolean}
     */
    _itemElementInViewport: function(itemElement)
    {
        if (itemElement.offsetTop + this._itemHeight < this._itemElementsContainer.scrollTop)
            return false;
        if (itemElement.offsetTop > this._itemElementsContainer.scrollTop + this._itemHeight * (this._rowsPerViewport + 1))
            return false;
        return true;
    }
}

WebInspector.FilteredItemSelectionDialog.prototype.__proto__ = WebInspector.DialogDelegate.prototype;

/**
 * @interface
 */
WebInspector.SelectionDialogContentProvider = function()
{
}

WebInspector.SelectionDialogContentProvider.prototype = {
    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemTitleAt: function(itemIndex) { },

    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemKeyAt: function(itemIndex) { },

    /**
     * @return {number}
     */
    itemsCount: function() { },

    /**
     * @param {function(number, number, number, number)} callback
     */
    requestItems: function(callback) { },

    /**
     * @param {number} itemIndex
     */
    selectItem: function(itemIndex) { }
};

/**
 * @constructor
 * @implements {WebInspector.SelectionDialogContentProvider}
 * @param {WebInspector.View} view
 * @param {WebInspector.ContentProvider} contentProvider
 */
WebInspector.JavaScriptOutlineDialog = function(view, contentProvider)
{
    WebInspector.SelectionDialogContentProvider.call(this);

    this._functionItems = [];
    this._view = view;
    this._contentProvider = contentProvider;
}

/**
 * @param {WebInspector.View} view
 * @param {WebInspector.ContentProvider} contentProvider
 */
WebInspector.JavaScriptOutlineDialog.show = function(view, contentProvider)
{
    if (WebInspector.Dialog.currentInstance())
        return null;
    var delegate = new WebInspector.JavaScriptOutlineDialog(view, contentProvider);
    var filteredItemSelectionDialog = new WebInspector.FilteredItemSelectionDialog(delegate);
    WebInspector.Dialog.show(view.element, filteredItemSelectionDialog);
}

WebInspector.JavaScriptOutlineDialog.createShortcut = function()
{
    return WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta | WebInspector.KeyboardShortcut.Modifiers.Shift);
}

WebInspector.JavaScriptOutlineDialog.prototype = {
    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemTitleAt: function(itemIndex)
    {
        var functionItem = this._functionItems[itemIndex];
        return functionItem.name + (functionItem.arguments ? functionItem.arguments : "");
    },

    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemKeyAt: function(itemIndex)
    {
        return this._functionItems[itemIndex].name;
    },

    /**
     * @return {number}
     */
    itemsCount: function()
    {
        return this._functionItems.length;
    },

    /**
     * @param {function(number, number, number, number)} callback
     */
    requestItems: function(callback)
    {
        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function contentCallback(content, contentEncoded, mimeType)
        {
            if (this._outlineWorker)
                this._outlineWorker.terminate();
            this._outlineWorker = new Worker("ScriptFormatterWorker.js");
            this._outlineWorker.onmessage = this._didBuildOutlineChunk.bind(this, callback);
            const method = "outline";
            this._outlineWorker.postMessage({ method: method, params: { content: content } });
        }
        this._contentProvider.requestContent(contentCallback.bind(this));
    },

    _didBuildOutlineChunk: function(callback, event)
    {
        var data = event.data;

        var index = this._functionItems.length;
        var chunk = data["chunk"];
        for (var i = 0; i < chunk.length; ++i)
            this._functionItems.push(chunk[i]);
        callback(index, chunk.length, data.index, data.total);

        if (data.total === data.index && this._outlineWorker) {
            this._outlineWorker.terminate();
            delete this._outlineWorker;
        }
    },

    /**
     * @param {number} itemIndex
     */
    selectItem: function(itemIndex)
    {
        var lineNumber = this._functionItems[itemIndex].line;
        if (!isNaN(lineNumber) && lineNumber >= 0)
            this._view.highlightLine(lineNumber);
        this._view.focus();
    }
}

WebInspector.JavaScriptOutlineDialog.prototype.__proto__ = WebInspector.SelectionDialogContentProvider.prototype;

/**
 * @constructor
 * @implements {WebInspector.SelectionDialogContentProvider}
 * @param {Array.<WebInspector.Resource>} resources
 */
WebInspector.OpenResourceDialog = function(resources)
{
    // FIXME: migrate from WebInspector.Resource to WebInspector.Source (base for Resource and UISourceCode) and make this dialog OpenSource.
    WebInspector.SelectionDialogContentProvider.call(this);

    this.resources = resources;

    function filterOutEmptyURLs(resource)
    {
        return !!resource.parsedURL.lastPathComponent;
    }
    this.resources = this.resources.filter(filterOutEmptyURLs);

    function compareFunction(resource1, resource2)
    {
        return resource1.parsedURL.lastPathComponent.localeCompare(resource2.parsedURL.lastPathComponent);
    }
    this.resources.sort(compareFunction);
}

WebInspector.OpenResourceDialog.createShortcut = function()
{
    return WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta);
}

WebInspector.OpenResourceDialog.prototype = {
    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemTitleAt: function(itemIndex)
    {
        return this.resources[itemIndex].parsedURL.lastPathComponent;
    },

    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemKeyAt: function(itemIndex)
    {
        return this.resources[itemIndex].parsedURL.lastPathComponent;
    },

    /**
     * @return {number}
     */
    itemsCount: function()
    {
        return this.resources.length;
    },

    /**
     * @param {function(number, number, number, number)} callback
     */
    requestItems: function(callback)
    {
        callback(0, this.resources.length, 1, 1);
    },

    /**
     * @param {number} itemIndex
     */
    selectItem: function(itemIndex)
    {
        // Overriden by descendants;
    }
}

WebInspector.OpenResourceDialog.prototype.__proto__ = WebInspector.SelectionDialogContentProvider.prototype;

/**
 * @constructor
 * @extends {WebInspector.OpenResourceDialog}
 * @param {WebInspector.ScriptsPanel} panel
 * @param {WebInspector.DebuggerPresentationModel} presentationModel
 */
WebInspector.OpenScriptDialog = function(panel, presentationModel)
{
    WebInspector.OpenResourceDialog.call(this, presentationModel.uiSourceCodes());
    this._panel = panel;
}

/**
 * @param {WebInspector.ScriptsPanel} panel
 * @param {WebInspector.DebuggerPresentationModel} presentationModel
 */
WebInspector.OpenScriptDialog.install = function(panel, presentationModel, relativeToElement)
{
    function showOpenResourceDialog()
    {
        WebInspector.OpenScriptDialog._show(panel, presentationModel, relativeToElement);
    }

    var openResourceShortcut = WebInspector.OpenResourceDialog.createShortcut();
    panel.registerShortcut(openResourceShortcut.key, showOpenResourceDialog);
}

/**
 * @param {WebInspector.ScriptsPanel} panel
 * @param {WebInspector.DebuggerPresentationModel} presentationModel
 * @param {Element} relativeToElement
 */
WebInspector.OpenScriptDialog._show = function(panel, presentationModel, relativeToElement)
{
    if (WebInspector.Dialog.currentInstance())
        return;

    var filteredItemSelectionDialog = new WebInspector.FilteredItemSelectionDialog(new WebInspector.OpenScriptDialog(panel, presentationModel));
    WebInspector.Dialog.show(relativeToElement, filteredItemSelectionDialog);
}

WebInspector.OpenScriptDialog.prototype = {
    /**
     * @param {number} itemIndex
     */
    selectItem: function(itemIndex)
    {
        this._panel.showUISourceCode(this.resources[itemIndex]);
    }
}

WebInspector.OpenScriptDialog.prototype.__proto__ = WebInspector.OpenResourceDialog.prototype;

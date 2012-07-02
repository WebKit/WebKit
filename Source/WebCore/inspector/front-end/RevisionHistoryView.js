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
 * @extends {WebInspector.View}
 */
WebInspector.RevisionHistoryView = function()
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("revisionHistory.css");
    this.element.addStyleClass("revision-history-drawer");
    this.element.addStyleClass("fill");
    this.element.addStyleClass("outline-disclosure");
    this._resourceItems = new Map();

    var olElement = this.element.createChild("ol");
    this._treeOutline = new TreeOutline(olElement);

    function populateRevisions(resource)
    {
        if (resource.history.length)
            this._createResourceItem(resource);
    }

    WebInspector.resourceTreeModel.forAllResources(populateRevisions.bind(this));
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceContentCommitted, this._revisionAdded, this);

    this._statusElement = document.createElement("span");
    this._statusElement.textContent = WebInspector.UIString("Local modifications");

}

/**
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.RevisionHistoryView.showHistory = function(uiSourceCode)
{
    if (!WebInspector.RevisionHistoryView._view) 
        WebInspector.RevisionHistoryView._view = new WebInspector.RevisionHistoryView();
    var view = WebInspector.RevisionHistoryView._view;
    WebInspector.showViewInDrawer(view._statusElement, view);
    view._revealResource(uiSourceCode.resource());
}

WebInspector.RevisionHistoryView.reset = function()
{
    if (WebInspector.RevisionHistoryView._view)
        WebInspector.RevisionHistoryView._view._reset();
}

WebInspector.RevisionHistoryView.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     */
    _createResourceItem: function(resource)
    {
        var resourceItem = new TreeElement(resource.displayName, null, true);
        resourceItem.selectable = false;

        // Insert in sorted order
        for (var i = 0; i < this._treeOutline.children.length; ++i) {
            if (this._treeOutline.children[i].title.localeCompare(resource.displayName) > 0) {
                this._treeOutline.insertChild(resourceItem, i);
                break;
            }
        }
        if (i === this._treeOutline.children.length)
            this._treeOutline.appendChild(resourceItem);

        this._resourceItems.put(resource, resourceItem);

        var revisionCount = resource.history.length;
        for (var i = revisionCount - 1; i >= 0; --i) {
            var revision = resource.history[i];
            var historyItem = new WebInspector.RevisionHistoryTreeElement(revision, resource.history[i - 1], i !== revisionCount - 1);
            resourceItem.appendChild(historyItem);
        }

        var linkItem = new TreeElement("", null, false);
        linkItem.selectable = false;
        resourceItem.appendChild(linkItem);

        var revertToOriginal = linkItem.listItemElement.createChild("span", "revision-history-link revision-history-link-row");
        revertToOriginal.textContent = WebInspector.UIString("apply original content");
        revertToOriginal.addEventListener("click", resource.revertToOriginal.bind(resource));

        var clearHistoryElement = resourceItem.listItemElement.createChild("span", "revision-history-link");
        clearHistoryElement.textContent = WebInspector.UIString("revert");
        clearHistoryElement.addEventListener("click", this._clearHistory.bind(this, resource));
        return resourceItem;
    },

    /**
     * @param {WebInspector.Resource} resource
     */
    _clearHistory: function(resource)
    {
        resource.revertAndClearHistory(historyCleared.bind(this));

        function historyCleared()
        {
            var resourceItem = this._resourceItems.get(resource);
            this._treeOutline.removeChild(resourceItem);
            this._resourceItems.remove(resource);
        }

    },

    _revisionAdded: function(event)
    {
        var resource = /** @type {WebInspector.Resource} */ event.data.resource;
        var resourceItem = this._resourceItems.get(resource);
        if (!resourceItem) {
            resourceItem = this._createResourceItem(resource);
            return;
        }

        var historyLength = resource.history.length;
        var historyItem = new WebInspector.RevisionHistoryTreeElement(resource.history[historyLength - 1], resource.history[historyLength - 2], false);
        if (resourceItem.children.length)
            resourceItem.children[0].allowRevert();
        resourceItem.insertChild(historyItem, 0);
    },

    /**
     * @param {WebInspector.Resource} resource
     */
    _revealResource: function(resource)
    {
        var resourceItem = this._resourceItems.get(resource);
        if (resourceItem) {
            resourceItem.reveal();
            resourceItem.expand();
        }
    },

    _reset: function()
    {
        this._treeOutline.removeChildren();
        this._resourceItems.clear();
    }
}

WebInspector.RevisionHistoryView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @extends {TreeElement}
 * @param {WebInspector.ResourceRevision} revision
 * @param {WebInspector.ResourceRevision} baseRevision
 * @param {boolean} allowRevert
 */
WebInspector.RevisionHistoryTreeElement = function(revision, baseRevision, allowRevert)
{
    TreeElement.call(this, revision.timestamp.toLocaleTimeString(), null, true);
    this.selectable = false;

    this._revision = revision;
    this._baseRevision = baseRevision;

    this._revertElement = document.createElement("span");
    this._revertElement.className = "revision-history-link";
    this._revertElement.textContent = WebInspector.UIString("apply revision content");
    this._revertElement.addEventListener("click", this._revision.revertToThis.bind(this._revision), false);
    if (!allowRevert)
        this._revertElement.addStyleClass("hidden");
}

WebInspector.RevisionHistoryTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.addStyleClass("revision-history-revision");
    },

    onexpand: function()
    {
        this.listItemElement.appendChild(this._revertElement);

        if (this._wasExpandedOnce)
            return;
        this._wasExpandedOnce = true;

        this.childrenListElement.addStyleClass("source-code");
        if (this._baseRevision)
            this._baseRevision.requestContent(step1.bind(this));
        else
            this._revision.resource.requestContent(step1.bind(this));

        function step1(baseContent)
        {
            this._revision.requestContent(step2.bind(this, baseContent));
        }

        function step2(baseContent, newContent)
        {
            var baseLines = difflib.stringAsLines(baseContent);
            var newLines = difflib.stringAsLines(newContent);
            var sm = new difflib.SequenceMatcher(baseLines, newLines);
            var opcodes = sm.get_opcodes();
            var lastWasSeparator = false;

            for (var idx = 0; idx < opcodes.length; idx++) {
                var code = opcodes[idx];
                var change = code[0];
                var b = code[1];
                var be = code[2];
                var n = code[3];
                var ne = code[4];
                var rowCount = Math.max(be - b, ne - n);
                var topRows = [];
                var bottomRows = [];
                for (var i = 0; i < rowCount; i++) {
                    if (change === "delete" || (change === "replace" && b < be)) {
                        var lineNumber = b++;
                        this._createLine(lineNumber, null, baseLines[lineNumber], "removed");
                        lastWasSeparator = false;
                    }

                    if (change === "insert" || (change === "replace" && n < ne)) {
                        var lineNumber = n++;
                        this._createLine(null, lineNumber, newLines[lineNumber], "added");
                        lastWasSeparator = false;
                    }

                    if (change === "equal") {
                        b++;
                        n++;
                        if (!lastWasSeparator)
                            this._createLine(null, null, "    \u2026", "separator");
                        lastWasSeparator = true;
                    }
                }
            }
        }
    },

    oncollapse: function()
    {
        if (this._revertElement.parentElement)
            this._revertElement.parentElement.removeChild(this._revertElement);
    },

    /**
     * @param {?number} baseLineNumber
     * @param {?number} newLineNumber
     * @param {string} lineContent
     * @param {string} changeType
     */
    _createLine: function(baseLineNumber, newLineNumber, lineContent, changeType)
    {
        var child = new TreeElement("", null, false);
        child.selectable = false;
        this.appendChild(child);
        var lineElement = document.createElement("span");

        function appendLineNumber(lineNumber)
        {
            var numberString = lineNumber !== null ? numberToStringWithSpacesPadding(lineNumber + 1, 4) : "    ";
            var lineNumberSpan = document.createElement("span");
            lineNumberSpan.addStyleClass("webkit-line-number");
            lineNumberSpan.textContent = numberString;
            child.listItemElement.appendChild(lineNumberSpan);
        }

        appendLineNumber(baseLineNumber);
        appendLineNumber(newLineNumber);

        var contentSpan = document.createElement("span");
        contentSpan.textContent = lineContent;
        child.listItemElement.appendChild(contentSpan);
        child.listItemElement.addStyleClass("revision-history-line");
        child.listItemElement.addStyleClass("revision-history-line-" + changeType);
    },

    allowRevert: function()
    {
        this._revertElement.removeStyleClass("hidden");
    }
}

WebInspector.RevisionHistoryTreeElement.prototype.__proto__ = TreeElement.prototype;

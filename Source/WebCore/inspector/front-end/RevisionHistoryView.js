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
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.RevisionHistoryView = function(uiSourceCode)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("revisionHistory.css");
    this.element.addStyleClass("revision-history-drawer");
    this.element.addStyleClass("fill");
    this.element.addStyleClass("outline-disclosure");

    this._uiSourceCode = uiSourceCode;
    this._resource = uiSourceCode.resource();

    var revisionCount = 0;
    if (!this._resource || !this._resource.history.length) {
        var label = this.element.createChild("div", "storage-empty-view");
        label.textContent = WebInspector.UIString("No revisions");
    } else {
        var olElement = this.element.createChild("ol");
        this._treeOutline = new TreeOutline(olElement);
        revisionCount = this._resource.history.length;

        for (var i = revisionCount - 1; i >= 0; --i) {
            var revision = this._resource.history[i];
            var historyItem = new WebInspector.RevisionHistoryTreeElement(revision, this._resource.history[i - 1]);
            this._treeOutline.appendChild(historyItem);
        }

        var baseItem = new TreeElement("", null, false);
        baseItem.selectable = false;
        this._treeOutline.appendChild(baseItem);

        var revertToOriginal = baseItem.listItemElement.createChild("span", "revision-history-link");
        revertToOriginal.textContent = WebInspector.UIString("revert to original");
        revertToOriginal.addEventListener("click", this._resource.revertToOriginal.bind(this._resource));
    }

    this._statusElement = document.createElement("span");
    if (!revisionCount)
        this._statusElement.textContent = WebInspector.UIString("%s: no revisions", uiSourceCode.parsedURL.displayName);
    else if (revisionCount === 1)
        this._statusElement.textContent = WebInspector.UIString("%s: 1 revision", uiSourceCode.parsedURL.displayName);
    else
        this._statusElement.textContent = WebInspector.UIString("%s: %s revisions", uiSourceCode.parsedURL.displayName, revisionCount);

    if (this._resource)
        this._resource.addEventListener(WebInspector.Resource.Events.RevisionAdded, this._revisionAdded, this);
}

/**
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.RevisionHistoryView.showHistory = function(uiSourceCode)
{
    var view = new WebInspector.RevisionHistoryView(uiSourceCode);
    WebInspector.showViewInDrawer(view._statusElement, view, view._onclose.bind(view));
    WebInspector.RevisionHistoryView._revisionHistoryShowing = true;
}

/**
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.RevisionHistoryView.uiSourceCodeSelected = function(uiSourceCode)
{
    if (WebInspector.RevisionHistoryView._revisionHistoryShowing)
        WebInspector.RevisionHistoryView.showHistory(uiSourceCode);
}

WebInspector.RevisionHistoryView.prototype = {
    _revisionAdded: function(event)
    {
        if (this._resource.history.length === 1) {
            WebInspector.RevisionHistoryView.showHistory(this._uiSourceCode);
            return;
        }
        var historyLength = this._resource.history.length;
        var historyItem = new WebInspector.RevisionHistoryTreeElement(this._resource.history[historyLength - 1], this._resource.history[historyLength - 2]);
        this._treeOutline.insertChild(historyItem, 0);
    },

    _onclose: function()
    {
        if (this._resource)
            this._resource.removeEventListener(WebInspector.Resource.Events.RevisionAdded, this._revisionAdded, this);
        delete WebInspector.RevisionHistoryView._revisionHistoryShowing;
    }
}

WebInspector.RevisionHistoryView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @extends {TreeElement}
 * @param {WebInspector.ResourceRevision} revision
 * @param {WebInspector.ResourceRevision=} baseRevision
 */
WebInspector.RevisionHistoryTreeElement = function(revision, baseRevision)
{
    var titleElement = document.createElement("span");
    titleElement.textContent = revision.timestamp.toLocaleTimeString();

    TreeElement.call(this, titleElement, null, true);
    this.selectable = false;

    this._revision = revision;
    this._baseRevision = baseRevision;

    var revertElement = titleElement.createChild("span", "revision-history-link");
    revertElement.textContent = WebInspector.UIString("revert to this");
    revertElement.addEventListener("click", this._revision.revertToThis.bind(this._revision), false);
}

WebInspector.RevisionHistoryTreeElement.prototype = {
    onexpand: function()
    {
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
    }
}

WebInspector.RevisionHistoryTreeElement.prototype.__proto__ = TreeElement.prototype;

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
 * @extends {WebInspector.SourceFrame}
 * @param {WebInspector.ScriptsPanel} scriptsPanel
 * @param {WebInspector.DebuggerPresentationModel} model
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.JavaScriptSourceFrame = function(scriptsPanel, model, uiSourceCode)
{
    this._scriptsPanel = scriptsPanel;
    this._model = model;
    this._breakpointManager = WebInspector.breakpointManager;
    this._uiSourceCode = uiSourceCode;

    var locations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
    for (var i = 0; i < locations.length; ++i)
        this._breakpointAdded({data:locations[i]});

    WebInspector.SourceFrame.call(this, uiSourceCode.url);

    this._popoverHelper = new WebInspector.ObjectPopoverHelper(this.textViewer.element,
            this._getPopoverAnchor.bind(this), this._resolveObjectForPopover.bind(this), this._onHidePopover.bind(this), true);

    this.textViewer.element.addEventListener("mousedown", this._onMouseDown.bind(this), true);
    this.textViewer.element.addEventListener("keydown", this._onKeyDown.bind(this), true);

    this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded, this._breakpointAdded, this);
    this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved, this._breakpointRemoved, this);

    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._onContentChanged, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageAdded, this._consoleMessageAdded, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageRemoved, this._consoleMessageRemoved, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessagesCleared, this._consoleMessagesCleared, this);
}

WebInspector.JavaScriptSourceFrame.prototype = {
    get uiSourceCode()
    {
        return this._uiSourceCode;
    },

    // View events
    wasShown: function()
    {
        WebInspector.SourceFrame.prototype.wasShown.call(this);
        this._setScriptSourceIsDirty(this._isDirty);
    },

    willHide: function()
    {
        WebInspector.SourceFrame.prototype.willHide.call(this);
        this._popoverHelper.hidePopover();
    },

    /**
     * @param {function(?string, boolean, string)} callback
     */
    requestContent: function(callback)
    {
        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function mycallback(content, contentEncoded, mimeType)
        {
            this._originalContent = content;
            callback(content, contentEncoded, mimeType);
        }
        this._uiSourceCode.requestContent(mycallback.bind(this));
    },

    canEditSource: function()
    {
        return this._model.canEditScriptSource(this._uiSourceCode);
    },

    editContent: function(newContent, callback)
    {
        this._editingContent = true;
        this._model.setScriptSource(this._uiSourceCode, newContent, callback);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onContentChanged: function(event)
    {
        if (this._editingContent)
            return;
        var oldContent = /** @type {string} */ event.data.oldContent;
        var content = /** @type {string} */ event.data.content;

        if (this._uiSourceCode.togglingFormatter())
            this.setContent(content, false, this._uiSourceCode.mimeType());
        else {
            var breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
            for (var i = 0; i < breakpointLocations.length; ++i)
                breakpointLocations[i].breakpoint.remove();
            this.setContent(content, false, this._uiSourceCode.mimeType());
            this._updateBreakpointsAfterLiveEdit(oldContent, content, breakpointLocations);
        }
    },

    populateLineGutterContextMenu: function(contextMenu, lineNumber)
    {
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Continue to here" : "Continue to Here"), this._continueToLine.bind(this, lineNumber));

        var breakpoint = this._breakpointManager.findBreakpoint(this._uiSourceCode, lineNumber);
        if (!breakpoint) {
            // This row doesn't have a breakpoint: We want to show Add Breakpoint and Add and Edit Breakpoint.
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add breakpoint" : "Add Breakpoint"), this._setBreakpoint.bind(this, lineNumber, "", true));
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add conditional breakpoint…" : "Add Conditional Breakpoint…"), this._editBreakpointCondition.bind(this, lineNumber));
        } else {
            // This row has a breakpoint, we want to show edit and remove breakpoint, and either disable or enable.
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Remove breakpoint" : "Remove Breakpoint"), breakpoint.remove.bind(breakpoint));
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Edit breakpoint…" : "Edit Breakpoint…"), this._editBreakpointCondition.bind(this, lineNumber, breakpoint));
            if (breakpoint.enabled())
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Disable breakpoint" : "Disable Breakpoint"), breakpoint.setEnabled.bind(breakpoint, false));
            else
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Enable breakpoint" : "Enable Breakpoint"), breakpoint.setEnabled.bind(breakpoint, true));
        }
    },

    populateTextAreaContextMenu: function(contextMenu, lineNumber)
    {
        WebInspector.SourceFrame.prototype.populateTextAreaContextMenu.call(this, contextMenu, lineNumber);
        var selection = window.getSelection();
        if (selection.type === "Range" && !selection.isCollapsed) {
            var addToWatchLabel = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add to watch" : "Add to Watch");
            contextMenu.appendItem(addToWatchLabel, this._scriptsPanel.addToWatch.bind(this._scriptsPanel, selection.toString()));
            var evaluateLabel = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Evaluate in console" : "Evaluate in Console");
            contextMenu.appendItem(evaluateLabel, WebInspector.evaluateInConsole.bind(WebInspector, selection.toString()));
        }
    },

    afterTextChanged: function(oldRange, newRange)
    {
        var isDirty = this.textModel.text !== this._originalContent;
        if (isDirty)
            this._setScriptSourceIsDirty(true);
        else
            this.didEditContent(null, this._originalContent);
    },

    _setScriptSourceIsDirty: function(isDirty)
    {
        this._scriptsPanel.setScriptSourceIsDirty(this._uiSourceCode, isDirty);
        this._isDirty = isDirty;
    },

    beforeTextChanged: function()
    {
        if (!this._isDirty) {
            // Disable all breakpoints in the model, store them as muted breakpoints.
            var breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
            var lineNumbers = {};
            for (var i = 0; i < breakpointLocations.length; ++i) {
                var breakpoint = breakpointLocations[i].breakpoint;
                breakpointLocations[i].breakpoint.remove();
                // Re-adding decoration only.
                this._addBreakpointDecoration(breakpointLocations[i].uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled(), true);
            }
        }

        this._isDirty = true;
        WebInspector.SourceFrame.prototype.beforeTextChanged.call(this);
    },

    didEditContent: function(error, content)
    {
        delete this._editingContent;

        WebInspector.SourceFrame.prototype.didEditContent.call(this, error, content);
        if (error)
            return;

        this._originalContent = content;
        this._isDirty = false;

        // Restore all muted breakpoints.
        for (var lineNumber = 0; lineNumber < this.textModel.linesCount; ++lineNumber) {
            var breakpointDecoration = this.textModel.getAttribute(lineNumber, "breakpoint");
            if (breakpointDecoration) {
                // Remove fake decoration
                this._removeBreakpointDecoration(lineNumber);
                // Set new breakpoint
                this._setBreakpoint(lineNumber, breakpointDecoration.condition, breakpointDecoration.enabled);
            }
        }
        this._setScriptSourceIsDirty(false);
    },

    _getPopoverAnchor: function(element, event)
    {
        if (!WebInspector.debuggerModel.isPaused())
            return null;
        if (window.getSelection().type === "Range")
            return null;
        var lineElement = element.enclosingNodeOrSelfWithClass("webkit-line-content");
        if (!lineElement)
            return null;

        if (element.hasStyleClass("webkit-javascript-ident"))
            return element;

        if (element.hasStyleClass("source-frame-token"))
            return element;

        // We are interested in identifiers and "this" keyword.
        if (element.hasStyleClass("webkit-javascript-keyword"))
            return element.textContent === "this" ? element : null;

        if (element !== lineElement || lineElement.childElementCount)
            return null;

        // Handle non-highlighted case
        // 1. Collect ranges of identifier suspects
        var lineContent = lineElement.textContent;
        var ranges = [];
        var regex = new RegExp("[a-zA-Z_\$0-9]+", "g");
        var match;
        while (regex.lastIndex < lineContent.length && (match = regex.exec(lineContent)))
            ranges.push({offset: match.index, length: regex.lastIndex - match.index});

        // 2. 'highlight' them with artificial style to detect word boundaries
        var changes = [];
        WebInspector.highlightRangesWithStyleClass(lineElement, ranges, "source-frame-token", changes);
        var lineOffsetLeft = lineElement.totalOffsetLeft();
        for (var child = lineElement.firstChild; child; child = child.nextSibling) {
            if (child.nodeType !== Node.ELEMENT_NODE || !child.hasStyleClass("source-frame-token"))
                continue;
            if (event.x > lineOffsetLeft + child.offsetLeft && event.x < lineOffsetLeft + child.offsetLeft + child.offsetWidth) {
                var text = child.textContent;
                return (text === "this" || !WebInspector.SourceJavaScriptTokenizer.Keywords[text]) ? child : null;
            }
        }
        return null;
    },

    _resolveObjectForPopover: function(element, showCallback, objectGroupName)
    {
        this._highlightElement = this._highlightExpression(element);

        /**
         * @param {?RuntimeAgent.RemoteObject} result
         * @param {boolean=} wasThrown
         */
        function showObjectPopover(result, wasThrown)
        {
            if (!WebInspector.debuggerModel.isPaused()) {
                this._popoverHelper.hidePopover();
                return;
            }
            showCallback(WebInspector.RemoteObject.fromPayload(result), wasThrown, this._highlightElement);
            // Popover may have been removed by showCallback().
            if (this._highlightElement)
                this._highlightElement.addStyleClass("source-frame-eval-expression");
        }

        if (!WebInspector.debuggerModel.isPaused()) {
            this._popoverHelper.hidePopover();
            return;
        }
        var selectedCallFrame = WebInspector.debuggerModel.selectedCallFrame();
        selectedCallFrame.evaluate(this._highlightElement.textContent, objectGroupName, false, true, false, showObjectPopover.bind(this));
    },

    _onHidePopover: function()
    {
        // Replace higlight element with its contents inplace.
        var highlightElement = this._highlightElement;
        if (!highlightElement)
            return;
        // FIXME: the text editor should maintain highlight on its own. The check below is a workaround for
        // the case when highlight element is detached from DOM by the TextViewer when re-building the DOM.
        var parentElement = highlightElement.parentElement;
        if (parentElement) {
            var child = highlightElement.firstChild;
            while (child) {
                var nextSibling = child.nextSibling;
                parentElement.insertBefore(child, highlightElement);
                child = nextSibling;
            }
            parentElement.removeChild(highlightElement);
        }
        delete this._highlightElement;
    },

    _highlightExpression: function(element)
    {
        // Collect tokens belonging to evaluated expression.
        var tokens = [ element ];
        var token = element.previousSibling;
        while (token && (token.className === "webkit-javascript-ident" || token.className === "source-frame-token" || token.className === "webkit-javascript-keyword" || token.textContent.trim() === ".")) {
            tokens.push(token);
            token = token.previousSibling;
        }
        tokens.reverse();

        // Wrap them with highlight element.
        var parentElement = element.parentElement;
        var nextElement = element.nextSibling;
        var container = document.createElement("span");
        for (var i = 0; i < tokens.length; ++i)
            container.appendChild(tokens[i]);
        parentElement.insertBefore(container, nextElement);
        return container;
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     * @param {boolean} mutedWhileEditing
     */
    _addBreakpointDecoration: function(lineNumber, condition, enabled, mutedWhileEditing)
    {
        var breakpoint = {
            condition: condition,
            enabled: enabled
        };
        this.textModel.setAttribute(lineNumber, "breakpoint", breakpoint);

        this.textViewer.beginUpdates();
        this.textViewer.addDecoration(lineNumber, "webkit-breakpoint");
        if (!enabled || mutedWhileEditing)
            this.textViewer.addDecoration(lineNumber, "webkit-breakpoint-disabled");
        else
            this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        if (!!condition)
            this.textViewer.addDecoration(lineNumber, "webkit-breakpoint-conditional");
        else
            this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this.textViewer.endUpdates();
    },

    _removeBreakpointDecoration: function(lineNumber)
    {
        this.textModel.removeAttribute(lineNumber, "breakpoint");
        this.textViewer.beginUpdates();
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint");
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this.textViewer.endUpdates();
    },

    _onMouseDown: function(event)
    {
        if (this._isDirty)
            return;

        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey)
            return;
        var target = event.target.enclosingNodeOrSelfWithClass("webkit-line-number");
        if (!target)
            return;
        var lineNumber = target.lineNumber;

        var breakpoint = this._breakpointManager.findBreakpoint(this._uiSourceCode, lineNumber);
        if (breakpoint) {
            if (event.shiftKey)
                breakpoint.setEnabled(!breakpoint.enabled());
            else
                breakpoint.remove();
        } else
            this._setBreakpoint(lineNumber, "", true);
        event.preventDefault();
    },

    _onKeyDown: function(event)
    {
        if (event.keyIdentifier === "U+001B") { // Escape key
            if (this._popoverHelper.isPopoverVisible()) {
                this._popoverHelper.hidePopover();
                event.consume();
            }
        }
    },

    /**
     * @param {number} lineNumber
     * @param {WebInspector.BreakpointManager.Breakpoint=} breakpoint
     */
    _editBreakpointCondition: function(lineNumber, breakpoint)
    {
        this._conditionElement = this._createConditionElement(lineNumber);
        this.textViewer.addDecoration(lineNumber, this._conditionElement);

        function finishEditing(committed, element, newText)
        {
            this.textViewer.removeDecoration(lineNumber, this._conditionElement);
            delete this._conditionEditorElement;
            delete this._conditionElement;
            if (breakpoint)
                breakpoint.setCondition(newText);
            else
                this._setBreakpoint(lineNumber, newText, true);
        }

        var config = new WebInspector.EditingConfig(finishEditing.bind(this, true), finishEditing.bind(this, false));
        WebInspector.startEditing(this._conditionEditorElement, config);
        this._conditionEditorElement.value = breakpoint ? breakpoint.condition() : "";
        this._conditionEditorElement.select();
    },

    _createConditionElement: function(lineNumber)
    {
        var conditionElement = document.createElement("div");
        conditionElement.className = "source-frame-breakpoint-condition";

        var labelElement = document.createElement("label");
        labelElement.className = "source-frame-breakpoint-message";
        labelElement.htmlFor = "source-frame-breakpoint-condition";
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("The breakpoint on line %d will stop only if this expression is true:", lineNumber)));
        conditionElement.appendChild(labelElement);

        var editorElement = document.createElement("input");
        editorElement.id = "source-frame-breakpoint-condition";
        editorElement.className = "monospace";
        editorElement.type = "text";
        conditionElement.appendChild(editorElement);
        this._conditionEditorElement = editorElement;

        return conditionElement;
    },

    /**
     * @param {number} lineNumber
     */
    setExecutionLine: function(lineNumber)
    {
        this._executionLineNumber = lineNumber;
        if (this.loaded) {
            this.textViewer.addDecoration(lineNumber, "webkit-execution-line");
            this.revealLine(this._executionLineNumber);
        }
    },

    clearExecutionLine: function()
    {
        if (this.loaded)
            this.textViewer.removeDecoration(this._executionLineNumber, "webkit-execution-line");
        delete this._executionLineNumber;
    },

    _lineNumberAfterEditing: function(lineNumber, oldRange, newRange)
    {
        var shiftOffset = lineNumber <= oldRange.startLine ? 0 : newRange.linesCount - oldRange.linesCount;

        // Special case of editing the line itself. We should decide whether the line number should move below or not.
        if (lineNumber === oldRange.startLine) {
            var whiteSpacesRegex = /^[\s\xA0]*$/;
            for (var i = 0; lineNumber + i <= newRange.endLine; ++i) {
                if (!whiteSpacesRegex.test(this.textModel.line(lineNumber + i))) {
                    shiftOffset = i;
                    break;
                }
            }
        }

        var newLineNumber = Math.max(0, lineNumber + shiftOffset);
        if (oldRange.startLine < lineNumber && lineNumber < oldRange.endLine)
            newLineNumber = oldRange.startLine;
        return newLineNumber;
    },

    _breakpointAdded: function(event)
    {
        var uiLocation = /** @type {WebInspector.UILocation} */ event.data.uiLocation;

        if (uiLocation.uiSourceCode !== this._uiSourceCode)
            return;

        var breakpoint = /** @type {WebInspector.BreakpointManager.Breakpoint} */ event.data.breakpoint;
        if (this.loaded)
            this._addBreakpointDecoration(uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled(), false);
    },

    _breakpointRemoved: function(event)
    {
        var uiLocation = /** @type {WebInspector.UILocation} */ event.data.uiLocation;
        if (uiLocation.uiSourceCode !== this._uiSourceCode)
            return;

        var breakpoint = /** @type {WebInspector.BreakpointManager.Breakpoint} */ event.data.breakpoint;
        var remainingBreakpoint = this._breakpointManager.findBreakpoint(this._uiSourceCode, uiLocation.lineNumber);
        if (!remainingBreakpoint && this.loaded)
            this._removeBreakpointDecoration(uiLocation.lineNumber);
    },

    _consoleMessageAdded: function(event)
    {
        var message = /** @type {WebInspector.PresentationConsoleMessage} */ event.data;
        if (this.loaded)
            this.addMessageToSource(message.lineNumber, message.originalMessage);
    },

    _consoleMessageRemoved: function(event)
    {
        var message = /** @type {WebInspector.PresentationConsoleMessage} */ event.data;
        if (this.loaded)
            this.removeMessageFromSource(message.lineNumber, message.originalMessage);
    },

    _consoleMessagesCleared: function(event)
    {
        this.clearMessages();
    },

    onTextViewerContentLoaded: function()
    {
        if (typeof this._executionLineNumber === "number")
            this.setExecutionLine(this._executionLineNumber);

        var breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
        for (var i = 0; i < breakpointLocations.length; ++i) {
            var breakpoint = breakpointLocations[i].breakpoint;
            this._addBreakpointDecoration(breakpointLocations[i].uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled(), false);
        }

        var messages = this._uiSourceCode.consoleMessages();
        for (var i = 0; i < messages.length; ++i) {
            var message = messages[i];
            this.addMessageToSource(message.lineNumber, message.originalMessage);
        }
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    _setBreakpoint: function(lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(this._uiSourceCode, lineNumber, condition, enabled);
    },

    /**
     * @param {number} lineNumber
     */
    _continueToLine: function(lineNumber)
    {
        var rawLocation = this._uiSourceCode.uiLocationToRawLocation(lineNumber, 0);
        WebInspector.debuggerModel.continueToLocation(rawLocation);
    },

    /**
     * @param {string} oldSource
     * @param {string} newSource
     * @param {Array.<Object>} breakpointLocations
     */
    _updateBreakpointsAfterLiveEdit: function(oldSource, newSource, breakpointLocations)
    {
        // Clear and re-create breakpoints according to text diff.
        var diff = Array.diff(oldSource.split("\n"), newSource.split("\n"));
        for (var i = 0; i < breakpointLocations.length; ++i) {
            var lineNumber = breakpointLocations[i].uiLocation.lineNumber;
            var newLineNumber = diff.left[lineNumber].row;
            if (newLineNumber === undefined) {
                for (var i = lineNumber - 1; i >= 0; --i) {
                    if (diff.left[i].row === undefined)
                        continue;
                    var shiftedLineNumber = diff.left[i].row + lineNumber - i;
                    if (shiftedLineNumber < diff.right.length) {
                        var originalLineNumber = diff.right[shiftedLineNumber].row;
                        if (originalLineNumber === lineNumber || originalLineNumber === undefined)
                            newLineNumber = shiftedLineNumber;
                    }
                    break;
                }
            }
            if (newLineNumber !== undefined) {
                var breakpoint = breakpointLocations[i].breakpoint;
                this._setBreakpoint(newLineNumber, breakpoint.condition(), breakpoint.enabled());
            }
        }
    }
}

WebInspector.JavaScriptSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;

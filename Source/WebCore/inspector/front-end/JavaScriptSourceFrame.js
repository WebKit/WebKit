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
 * @extends {WebInspector.UISourceCodeFrame}
 * @param {WebInspector.ScriptsPanel} scriptsPanel
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.JavaScriptSourceFrame = function(scriptsPanel, uiSourceCode)
{
    this._scriptsPanel = scriptsPanel;
    this._breakpointManager = WebInspector.breakpointManager;
    this._uiSourceCode = uiSourceCode;

    WebInspector.UISourceCodeFrame.call(this, uiSourceCode);
    if (uiSourceCode.project().type() === WebInspector.projectTypes.Debugger)
        this.element.addStyleClass("source-frame-debugger-script");

    this._popoverHelper = new WebInspector.ObjectPopoverHelper(this.textEditor.element,
            this._getPopoverAnchor.bind(this), this._resolveObjectForPopover.bind(this), this._onHidePopover.bind(this), true);

    this.textEditor.element.addEventListener("keydown", this._onKeyDown.bind(this), true);

    this.textEditor.addEventListener(WebInspector.TextEditor.Events.GutterClick, this._handleGutterClick.bind(this), this);

    this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded, this._breakpointAdded, this);
    this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved, this._breakpointRemoved, this);

    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageAdded, this._consoleMessageAdded, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageRemoved, this._consoleMessageRemoved, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessagesCleared, this._consoleMessagesCleared, this);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.SourceMappingChanged, this._onSourceMappingChanged, this);
    
    this._updateScriptFile();
}

WebInspector.JavaScriptSourceFrame.prototype = {
    // View events
    wasShown: function()
    {
        WebInspector.UISourceCodeFrame.prototype.wasShown.call(this);
    },

    willHide: function()
    {
        WebInspector.UISourceCodeFrame.prototype.willHide.call(this);
        this._popoverHelper.hidePopover();
    },

    /**
     * @param {string} content
     */
    onUISourceCodeContentChanged: function(content, contentEncoded, mimeType)
    {
        var breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
        for (var i = 0; i < breakpointLocations.length; ++i)
            breakpointLocations[i].breakpoint.remove();
        WebInspector.UISourceCodeFrame.prototype.onUISourceCodeContentChanged.call(this, content);
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
        var selection = window.getSelection();
        if (selection.type === "Range" && !selection.isCollapsed) {
            var addToWatchLabel = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add to watch" : "Add to Watch");
            contextMenu.appendItem(addToWatchLabel, this._scriptsPanel.addToWatch.bind(this._scriptsPanel, selection.toString()));
            var evaluateLabel = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Evaluate in console" : "Evaluate in Console");
            contextMenu.appendItem(evaluateLabel, WebInspector.evaluateInConsole.bind(WebInspector, selection.toString()));
            contextMenu.appendSeparator();
        } else if (!this._uiSourceCode.isEditable() && this._uiSourceCode.contentType() === WebInspector.resourceTypes.Script) {
            function liveEdit(event)
            {
                var liveEditUISourceCode = WebInspector.liveEditSupport.uiSourceCodeForLiveEdit(this._uiSourceCode);
                this._scriptsPanel.showUISourceCode(liveEditUISourceCode, lineNumber)
            }

            // FIXME: Change condition above to explicitly check that current uiSourceCode is created by default debugger mapping
            // and move the code adding this menu item to generic context menu provider for UISourceCode.
            var liveEditLabel = WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Live edit" : "Live Edit");
            contextMenu.appendItem(liveEditLabel, liveEdit.bind(this));
            contextMenu.appendSeparator();
        }
        WebInspector.UISourceCodeFrame.prototype.populateTextAreaContextMenu.call(this, contextMenu, lineNumber);
    },

    _willMergeToVM: function()
    {
        if (this._supportsEnabledBreakpointsWhileEditing())
            return;
        this._preserveDecorations = true;
    },

    _didMergeToVM: function()
    {
        if (this._supportsEnabledBreakpointsWhileEditing())
            return;
        delete this._preserveDecorations;
        this._restoreBreakpointsAfterEditing();
    },

    _willDivergeFromVM: function()
    {
        if (this._supportsEnabledBreakpointsWhileEditing())
            return;
        this._preserveDecorations = true;
    },

    _didDivergeFromVM: function()
    {
        if (this._supportsEnabledBreakpointsWhileEditing())
            return;
        delete this._preserveDecorations;
        this._muteBreakpointsWhileEditing();
    },

    _muteBreakpointsWhileEditing: function()
    {
        for (var lineNumber = 0; lineNumber < this._textEditor.linesCount; ++lineNumber) {
            var breakpointDecoration = this._textEditor.getAttribute(lineNumber, "breakpoint");
            if (!breakpointDecoration)
                continue;
            this._removeBreakpointDecoration(lineNumber);
            this._addBreakpointDecoration(lineNumber, breakpointDecoration.condition, breakpointDecoration.enabled, true);
        }
    },

    _supportsEnabledBreakpointsWhileEditing: function()
    {
        return this._uiSourceCode.project().type() === WebInspector.projectTypes.Snippets;
    },

    _restoreBreakpointsAfterEditing: function()
    {
        var breakpoints = {};
        // Save and remove muted breakpoint decorations.
        for (var lineNumber = 0; lineNumber < this._textEditor.linesCount; ++lineNumber) {
            var breakpointDecoration = this._textEditor.getAttribute(lineNumber, "breakpoint");
            if (breakpointDecoration) {
                breakpoints[lineNumber] = breakpointDecoration;
                this._removeBreakpointDecoration(lineNumber);
            }
        }

        // Remove all breakpoints.
        var breakpointLocations = this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);
        var lineNumbers = {};
        for (var i = 0; i < breakpointLocations.length; ++i) {
            var breakpoint = breakpointLocations[i].breakpoint;
            breakpointLocations[i].breakpoint.remove();
        }

        // Restore all breakpoints from saved decorations.
        for (var lineNumberString in breakpoints) {
            var lineNumber = parseInt(lineNumberString, 10);
            if (isNaN(lineNumber))
                continue;
            var breakpointDecoration = breakpoints[lineNumberString];
            this._setBreakpoint(lineNumber, breakpointDecoration.condition, breakpointDecoration.enabled);
        }
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
        this.textEditor.highlightRangesWithStyleClass(lineElement, ranges, "source-frame-token", changes);
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
        selectedCallFrame.evaluate(this._highlightElement.textContent, objectGroupName, false, true, false, false, showObjectPopover.bind(this));
    },

    _onHidePopover: function()
    {
        // Replace higlight element with its contents inplace.
        var highlightElement = this._highlightElement;
        if (!highlightElement)
            return;
        // FIXME: the text editor should maintain highlight on its own. The check below is a workaround for
        // the case when highlight element is detached from DOM by the TextEditor when re-building the DOM.
        this.textEditor.hideHighlightedExpression(highlightElement);
        delete this._highlightElement;
    },

    /**
     * @param {Element} element
     * @return {Element}
     */
    _highlightExpression: function(element)
    {
        return this.textEditor.highlightExpression(element, { "webkit-javascript-ident": true, "source-frame-token": true, "webkit-javascript-keyword": true }, { ".": true });
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     * @param {boolean} mutedWhileEditing
     */
    _addBreakpointDecoration: function(lineNumber, condition, enabled, mutedWhileEditing)
    {
        if (this._preserveDecorations)
            return;
        var breakpoint = {
            condition: condition,
            enabled: enabled
        };
        this.textEditor.setAttribute(lineNumber, "breakpoint", breakpoint);

        var disabled = !enabled || mutedWhileEditing;
        this.textEditor.addBreakpoint(lineNumber, disabled, !!condition);
    },

    _removeBreakpointDecoration: function(lineNumber)
    {
        if (this._preserveDecorations)
            return;
        this.textEditor.removeAttribute(lineNumber, "breakpoint");
        this.textEditor.removeBreakpoint(lineNumber);
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
        this.textEditor.addDecoration(lineNumber, this._conditionElement);

        function finishEditing(committed, element, newText)
        {
            this.textEditor.removeDecoration(lineNumber, this._conditionElement);
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
            this.textEditor.setExecutionLine(lineNumber);
            this.revealLine(this._executionLineNumber);
            if (this.canEditSource())
                this.setSelection(WebInspector.TextRange.createFromLocation(lineNumber, 0));
        }
    },

    clearExecutionLine: function()
    {
        if (this.loaded && typeof this._executionLineNumber === "number")
            this.textEditor.clearExecutionLine();
        delete this._executionLineNumber;
    },

    _lineNumberAfterEditing: function(lineNumber, oldRange, newRange)
    {
        var shiftOffset = lineNumber <= oldRange.startLine ? 0 : newRange.linesCount - oldRange.linesCount;

        // Special case of editing the line itself. We should decide whether the line number should move below or not.
        if (lineNumber === oldRange.startLine) {
            var whiteSpacesRegex = /^[\s\xA0]*$/;
            for (var i = 0; lineNumber + i <= newRange.endLine; ++i) {
                if (!whiteSpacesRegex.test(this.textEditor.line(lineNumber + i))) {
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
        var uiLocation = /** @type {WebInspector.UILocation} */ (event.data.uiLocation);

        if (uiLocation.uiSourceCode !== this._uiSourceCode)
            return;

        var breakpoint = /** @type {WebInspector.BreakpointManager.Breakpoint} */ (event.data.breakpoint);
        if (this.loaded)
            this._addBreakpointDecoration(uiLocation.lineNumber, breakpoint.condition(), breakpoint.enabled(), false);
    },

    _breakpointRemoved: function(event)
    {
        var uiLocation = /** @type {WebInspector.UILocation} */ (event.data.uiLocation);
        if (uiLocation.uiSourceCode !== this._uiSourceCode)
            return;

        var breakpoint = /** @type {WebInspector.BreakpointManager.Breakpoint} */ (event.data.breakpoint);
        var remainingBreakpoint = this._breakpointManager.findBreakpoint(this._uiSourceCode, uiLocation.lineNumber);
        if (!remainingBreakpoint && this.loaded)
            this._removeBreakpointDecoration(uiLocation.lineNumber);
    },

    _consoleMessageAdded: function(event)
    {
        var message = /** @type {WebInspector.PresentationConsoleMessage} */ (event.data);
        if (this.loaded)
            this.addMessageToSource(message.lineNumber, message.originalMessage);
    },

    _consoleMessageRemoved: function(event)
    {
        var message = /** @type {WebInspector.PresentationConsoleMessage} */ (event.data);
        if (this.loaded)
            this.removeMessageFromSource(message.lineNumber, message.originalMessage);
    },

    _consoleMessagesCleared: function(event)
    {
        this.clearMessages();
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onSourceMappingChanged: function(event)
    {
        this._updateScriptFile();
    },

    _updateScriptFile: function()
    {
        if (this._scriptFile) {
            this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.WillMergeToVM, this._willMergeToVM, this);
            this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.DidMergeToVM, this._didMergeToVM, this);
            this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.WillDivergeFromVM, this._willDivergeFromVM, this);
            this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.DidDivergeFromVM, this._didDivergeFromVM, this);
        }
        this._scriptFile = this._uiSourceCode.scriptFile();
        if (this._scriptFile) {
            this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.WillMergeToVM, this._willMergeToVM, this);
            this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.DidMergeToVM, this._didMergeToVM, this);
            this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.WillDivergeFromVM, this._willDivergeFromVM, this);
            this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.DidDivergeFromVM, this._didDivergeFromVM, this);
        }
    },

    onTextEditorContentLoaded: function()
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
     * @param {Event} event
     */
    _handleGutterClick: function(event)
    {
        if (this._uiSourceCode.isDirty() && !this._supportsEnabledBreakpointsWhileEditing())
            return;

        var lineNumber = event.data.lineNumber;
        var eventObject = /** @type {Event} */ (event.data.event);

        if (eventObject.button != 0 || eventObject.altKey || eventObject.ctrlKey || eventObject.metaKey)
            return;

        this._toggleBreakpoint(lineNumber, eventObject.shiftKey);
        eventObject.consume(true);
    },

    /**
     * @param {number} lineNumber
     * @param {boolean} onlyDisable
     */
    _toggleBreakpoint: function(lineNumber, onlyDisable)
    {
        var breakpoint = this._breakpointManager.findBreakpoint(this._uiSourceCode, lineNumber);
        if (breakpoint) {
            if (onlyDisable)
                breakpoint.setEnabled(!breakpoint.enabled());
            else
                breakpoint.remove();
        } else
            this._setBreakpoint(lineNumber, "", true);
    },

    toggleBreakpointOnCurrentLine: function()
    {
        if (this._uiSourceCode.isDirty() && !this._supportsEnabledBreakpointsWhileEditing())
            return;

        var selection = this.textEditor.selection();
        if (!selection)
            return;
        this._toggleBreakpoint(selection.startLine, false);
    },

    /**
     * @param {number} lineNumber
     * @param {string} condition
     * @param {boolean} enabled
     */
    _setBreakpoint: function(lineNumber, condition, enabled)
    {
        this._breakpointManager.setBreakpoint(this._uiSourceCode, lineNumber, condition, enabled);

        WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction, {
            action: WebInspector.UserMetrics.UserActionNames.SetBreakpoint,
            url: this._uiSourceCode.originURL(),
            line: lineNumber,
            enabled: enabled
        });
    },

    /**
     * @param {number} lineNumber
     */
    _continueToLine: function(lineNumber)
    {
        var rawLocation = /** @type {WebInspector.DebuggerModel.Location} */ (this._uiSourceCode.uiLocationToRawLocation(lineNumber, 0));
        WebInspector.debuggerModel.continueToLocation(rawLocation);
    },

    dispose: function()
    {
        this._breakpointManager.removeEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded, this._breakpointAdded, this);
        this._breakpointManager.removeEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved, this._breakpointRemoved, this);
        WebInspector.UISourceCodeFrame.prototype.dispose.call(this);
    },

    __proto__: WebInspector.UISourceCodeFrame.prototype
}

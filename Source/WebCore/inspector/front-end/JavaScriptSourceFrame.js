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
    WebInspector.SourceFrame.call(this, uiSourceCode.url);

    this._scriptsPanel = scriptsPanel;
    this._model = model;
    this._uiSourceCode = uiSourceCode;
    this._popoverObjectGroup = "popover";
    this._breakpoints = {};
    this._popoverHelper = new WebInspector.ObjectPopoverHelper(this.textViewer.element,
            this._getPopoverAnchor.bind(this), this._onShowPopover.bind(this), this._onHidePopover.bind(this), true);

    this.textViewer.element.addEventListener("mousedown", this._onMouseDown.bind(this), true);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._onContentChanged, this);
    this.addEventListener(WebInspector.SourceFrame.Events.Loaded, this._onTextViewerContentLoaded, this);
}

WebInspector.JavaScriptSourceFrame.prototype = {
    get uiSourceCode()
    {
        return this._uiSourceCode;
    },

    // View events
    willHide: function()
    {
        WebInspector.SourceFrame.prototype.willHide.call(this);
        this._popoverHelper.hidePopover();
    },

    // SourceFrame overrides
    requestContent: function(callback)
    {
        this._uiSourceCode.requestContent(callback);
    },

    canEditSource: function()
    {
        return this._model.canEditScriptSource(this._uiSourceCode);
    },

    suggestedFileName: function()
    {
        return this._uiSourceCode.fileName || "untitled.js";
    },

    editContent: function(newContent, callback)
    {
        this._model.setScriptSource(this._uiSourceCode, newContent, callback);
    },

    _onContentChanged: function()
    {
        if (!this.textViewer.readOnly)
            return;
        this.requestContent(this.setContent.bind(this));
    },

    setReadOnly: function(readOnly)
    {
        if (!readOnly)
            this._popoverHelper.hidePopover();
        WebInspector.SourceFrame.prototype.setReadOnly.call(this, readOnly);
        if (readOnly)
            this._scriptsPanel.setScriptSourceIsBeingEdited(this._uiSourceCode, false);
    },

    populateLineGutterContextMenu: function(contextMenu, lineNumber)
    {
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Continue to here" : "Continue to Here"), this._model.continueToLine.bind(this._model, this._uiSourceCode, lineNumber));

        var breakpoint = this._model.findBreakpoint(this._uiSourceCode, lineNumber);
        if (!breakpoint) {
            // This row doesn't have a breakpoint: We want to show Add Breakpoint and Add and Edit Breakpoint.
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add breakpoint" : "Add Breakpoint"), this._setBreakpoint.bind(this, lineNumber, "", true));

            function addConditionalBreakpoint()
            {
                this.addBreakpoint(lineNumber, true, true, true);
                function didEditBreakpointCondition(committed, condition)
                {
                    this.removeBreakpoint(lineNumber);
                    if (committed)
                        this._setBreakpoint(lineNumber, condition, true);
                }
                this._editBreakpointCondition(lineNumber, "", didEditBreakpointCondition.bind(this));
            }
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add conditional breakpoint…" : "Add Conditional Breakpoint…"), addConditionalBreakpoint.bind(this));
        } else {
            // This row has a breakpoint, we want to show edit and remove breakpoint, and either disable or enable.
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Remove breakpoint" : "Remove Breakpoint"), this._model.removeBreakpoint.bind(this._model, this._uiSourceCode, lineNumber));

            function editBreakpointCondition()
            {
                function didEditBreakpointCondition(committed, condition)
                {
                    if (committed)
                        this._model.updateBreakpoint(this._uiSourceCode, lineNumber, condition, breakpoint.enabled);
                }
                this._editBreakpointCondition(lineNumber, breakpoint.condition, didEditBreakpointCondition.bind(this));
            }
            contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Edit breakpoint…" : "Edit Breakpoint…"), editBreakpointCondition.bind(this));
            function setBreakpointEnabled(enabled)
            {
                this._model.updateBreakpoint(this._uiSourceCode, lineNumber, breakpoint.condition, enabled);
            }
            if (breakpoint.enabled)
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Disable breakpoint" : "Disable Breakpoint"), setBreakpointEnabled.bind(this, false));
            else
                contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Enable breakpoint" : "Enable Breakpoint"), setBreakpointEnabled.bind(this, true));
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
        if (!oldRange || !newRange)
            return;

        // Adjust execution line number.
        if (typeof this._executionLineNumber === "number") {
            var newExecutionLineNumber = this._lineNumberAfterEditing(this._executionLineNumber, oldRange, newRange);
            this.clearExecutionLine();
            this.setExecutionLine(newExecutionLineNumber, true);
        }

        // Adjust breakpoints.
        var oldBreakpoints = this._breakpoints;
        this._breakpoints = {};
        for (var lineNumber in oldBreakpoints) {
            lineNumber = Number(lineNumber);
            var breakpoint = oldBreakpoints[lineNumber];
            var newLineNumber = this._lineNumberAfterEditing(lineNumber, oldRange, newRange);
            if (lineNumber === newLineNumber)
                this._breakpoints[lineNumber] = breakpoint;
            else {
                this.removeBreakpoint(lineNumber);
                this.addBreakpoint(newLineNumber, breakpoint.resolved, breakpoint.conditional, breakpoint.enabled);
            }
        }
    },

    beforeTextChanged: function()
    {
        if (!this._javaScriptSourceFrameState) {
            this._javaScriptSourceFrameState = {
                executionLineNumber: this._executionLineNumber,
                breakpoints: this._breakpoints
            }
            this._scriptsPanel.setScriptSourceIsBeingEdited(this._uiSourceCode, true);
        }
        WebInspector.SourceFrame.prototype.beforeTextChanged.call(this);
    },

    cancelEditing: function()
    {
        WebInspector.SourceFrame.prototype.cancelEditing.call(this);

        if (!this._javaScriptSourceFrameState)
            return;

        if (typeof this._javaScriptSourceFrameState.executionLineNumber === "number") {
            this.clearExecutionLine();
            this.setExecutionLine(this._javaScriptSourceFrameState.executionLineNumber);
        }

        var oldBreakpoints = this._breakpoints;
        this._breakpoints = {};
        for (var lineNumber in oldBreakpoints)
            this.removeBreakpoint(Number(lineNumber));

        var newBreakpoints = this._javaScriptSourceFrameState.breakpoints;
        for (var lineNumber in newBreakpoints) {
            lineNumber = Number(lineNumber);
            var breakpoint = newBreakpoints[lineNumber];
            this.addBreakpoint(lineNumber, breakpoint.resolved, breakpoint.conditional, breakpoint.enabled);
        }

        delete this._javaScriptSourceFrameState;
    },

    didEditContent: function(error)
    {
        WebInspector.SourceFrame.prototype.didEditContent.call(this, error);
        if (error)
            return;

        var newBreakpoints = {};
        for (var lineNumber in this._breakpoints) {
            newBreakpoints[lineNumber] = this._breakpoints[lineNumber];
            this.removeBreakpoint(Number(lineNumber));
        }

        for (var lineNumber in this._javaScriptSourceFrameState.breakpoints)
            this._model.removeBreakpoint(this._uiSourceCode, Number(lineNumber));

        for (var lineNumber in newBreakpoints) {
            var breakpoint = newBreakpoints[lineNumber];
            this._setBreakpoint(Number(lineNumber), breakpoint.condition, breakpoint.enabled);
        }
        this._scriptsPanel.setScriptSourceIsBeingEdited(this._uiSourceCode, false);
        delete this._javaScriptSourceFrameState;
    },

    // Popover callbacks
    _shouldShowPopover: function(element)
    {
        if (!this._model.paused)
            return false;
        if (!element.enclosingNodeOrSelfWithClass("webkit-line-content"))
            return false;
        if (window.getSelection().type === "Range")
            return false;

        // We are interested in identifiers and "this" keyword.
        if (element.hasStyleClass("webkit-javascript-keyword"))
            return element.textContent === "this";

        return element.hasStyleClass("webkit-javascript-ident");
    },

    _getPopoverAnchor: function(element)
    {
        if (!this._shouldShowPopover(element))
            return;
        return element;
    },

    _onShowPopover: function(element, showCallback)
    {
        if (!this.readOnly) {
            this._popoverHelper.hidePopover();
            return;
        }
        this._highlightElement = this._highlightExpression(element);

        function showObjectPopover(result, wasThrown)
        {
            if (!this._model.paused) {
                this._popoverHelper.hidePopover();
                return;
            }
            showCallback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
            // Popover may have been removed by showCallback().
            if (this._highlightElement)
                this._highlightElement.addStyleClass("source-frame-eval-expression");
        }

        var selectedCallFrame = this._model.selectedCallFrame;
        selectedCallFrame.evaluate(this._highlightElement.textContent, this._popoverObjectGroup, false, false, showObjectPopover.bind(this));
    },

    _onHidePopover: function()
    {
        // Replace higlight element with its contents inplace.
        var highlightElement = this._highlightElement;
        if (!highlightElement)
            return;
        var parentElement = highlightElement.parentElement;
        var child = highlightElement.firstChild;
        while (child) {
            var nextSibling = child.nextSibling;
            parentElement.insertBefore(child, highlightElement);
            child = nextSibling;
        }
        parentElement.removeChild(highlightElement);
        delete this._highlightElement;
        RuntimeAgent.releaseObjectGroup(this._popoverObjectGroup);
    },

    _highlightExpression: function(element)
    {
        // Collect tokens belonging to evaluated expression.
        var tokens = [ element ];
        var token = element.previousSibling;
        while (token && (token.className === "webkit-javascript-ident" || token.className === "webkit-javascript-keyword" || token.textContent.trim() === ".")) {
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

    addBreakpoint: function(lineNumber, resolved, conditional, enabled)
    {
        this._breakpoints[lineNumber] = {
            resolved: resolved,
            conditional: conditional,
            enabled: enabled
        };
        this.textViewer.beginUpdates();
        this.textViewer.addDecoration(lineNumber, "webkit-breakpoint");
        if (!enabled)
            this.textViewer.addDecoration(lineNumber, "webkit-breakpoint-disabled");
        if (conditional)
            this.textViewer.addDecoration(lineNumber, "webkit-breakpoint-conditional");
        this.textViewer.endUpdates();
    },

    removeBreakpoint: function(lineNumber)
    {
        delete this._breakpoints[lineNumber];
        this.textViewer.beginUpdates();
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint");
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        this.textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this.textViewer.endUpdates();
    },

    _setBreakpoint: function(lineNumber, condition, enabled)
    {
        this._model.setBreakpoint(this._uiSourceCode, lineNumber, condition, enabled);
        this._scriptsPanel.activateBreakpoints();
    },

    _onMouseDown: function(event)
    {
        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey)
            return;
        var target = event.target.enclosingNodeOrSelfWithClass("webkit-line-number");
        if (!target)
            return;
        var lineNumber = target.lineNumber;

        var breakpoint = this._model.findBreakpoint(this._uiSourceCode, lineNumber);
        if (breakpoint) {
            if (event.shiftKey)
                this._model.updateBreakpoint(this._uiSourceCode, lineNumber, breakpoint.condition, !breakpoint.enabled);
            else
                this._model.removeBreakpoint(this._uiSourceCode, lineNumber);
        } else
            this._setBreakpoint(lineNumber, "", true);
        event.preventDefault();
    },

    _editBreakpointCondition: function(lineNumber, condition, callback)
    {
        this._conditionElement = this._createConditionElement(lineNumber);
        this.textViewer.addDecoration(lineNumber, this._conditionElement);

        function finishEditing(committed, element, newText)
        {
            this.textViewer.removeDecoration(lineNumber, this._conditionElement);
            delete this._conditionEditorElement;
            delete this._conditionElement;
            callback(committed, newText);
        }

        var config = new WebInspector.EditingConfig(finishEditing.bind(this, true), finishEditing.bind(this, false));
        WebInspector.startEditing(this._conditionEditorElement, config);
        this._conditionEditorElement.value = condition;
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
     * @param {boolean=} skipRevealLine
     */
    setExecutionLine: function(lineNumber, skipRevealLine)
    {
        this._executionLineNumber = lineNumber;
        if (this.loaded) {
            this.textViewer.addDecoration(lineNumber, "webkit-execution-line");
            if (!skipRevealLine)
                this.textViewer.revealLine(lineNumber);
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
                if (!whiteSpacesRegex.test(this._textModel.line(lineNumber + i))) {
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

    _onTextViewerContentLoaded: function()
    {
        if (typeof this._executionLineNumber === "number")
            this.setExecutionLine(this._executionLineNumber);
    }
}

WebInspector.JavaScriptSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;

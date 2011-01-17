/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

WebInspector.SourceFrame = function(parentElement, contentProvider, url, canEditScripts)
{
    this._parentElement = parentElement;
    this._contentProvider = contentProvider;
    this._url = url;
    this._canEditScripts = canEditScripts;

    this._textModel = new WebInspector.TextEditorModel();
    this._textModel.replaceTabsWithSpaces = true;

    this._messages = [];
    this._rowMessages = {};
    this._messageBubbles = {};

    this._popoverObjectGroup = "popover";
}

WebInspector.SourceFrame.prototype = {

    set visible(visible)
    {
        if (!this._contentRequested) {
            this._contentRequested = true;
            this._contentProvider.requestContent(this._createTextViewer.bind(this));
        }

        if (visible) {
            if (this._textViewer && this._scrollTop)
                this._textViewer.element.scrollTop = this._scrollTop;
            if (this._textViewer && this._scrollLeft)
                this._textViewer.element.scrollLeft = this._scrollLeft;
            if (this._textViewer)
                this._textViewer.resize();
        } else {
            this._hidePopup();
            if (this._textViewer) {
                this._scrollTop = this._textViewer.element.scrollTop;
                this._scrollLeft = this._textViewer.element.scrollLeft;
                this._textViewer.freeCachedElements();
            }
        }
    },

    get executionLine()
    {
        return this._executionLine;
    },

    set executionLine(x)
    {
        if (this._executionLine === x)
            return;

        var previousLine = this._executionLine;
        this._executionLine = x;

        if (this._textViewer)
            this._updateExecutionLine(previousLine);
    },

    markDiff: function(diffData)
    {
        if (this._diffLines && this._textViewer)
            this._removeDiffDecorations();

        this._diffLines = diffData;
        if (this._textViewer)
            this._updateDiffDecorations();
    },

    revealLine: function(lineNumber)
    {
        if (this._textViewer)
            this._textViewer.revealLine(lineNumber - 1, 0);
        else
            this._lineNumberToReveal = lineNumber;
    },

    addMessage: function(msg)
    {
        // Don't add the message if there is no message or valid line or if the msg isn't an error or warning.
        if (!msg.message || msg.line <= 0 || !msg.isErrorOrWarning())
            return;
        this._messages.push(msg)
        if (this._textViewer)
            this._addMessageToSource(msg);
    },

    clearMessages: function()
    {
        for (var line in this._messageBubbles) {
            var bubble = this._messageBubbles[line];
            bubble.parentNode.removeChild(bubble);
        }

        this._messages = [];
        this._rowMessages = {};
        this._messageBubbles = {};
        if (this._textViewer)
            this._textViewer.resize();
    },

    sizeToFitContentHeight: function()
    {
        if (this._textViewer)
            this._textViewer.revalidateDecorationsAndPaint();
    },

    get textModel()
    {
        return this._textModel;
    },

    get scrollTop()
    {
        return this._textViewer ? this._textViewer.element.scrollTop : 0;
    },

    set scrollTop(scrollTop)
    {
        if (this._textViewer)
            this._textViewer.element.scrollTop = scrollTop;
    },

    highlightLine: function(line)
    {
        if (this._textViewer)
            this._textViewer.highlightLine(line - 1);
        else
            this._lineToHighlight = line;
    },

    clearLineHighlight: function()
    {
        if (this._textViewer)
            this._textViewer.clearLineHighlight();
        else
            delete this._lineToHighlight;
    },

    _createTextViewer: function(mimeType, content)
    {
        this._content = content;
        this._textModel.setText(null, content);

        this._textViewer = new WebInspector.TextViewer(this._textModel, WebInspector.platform, this._url);
        var element = this._textViewer.element;
        element.addEventListener("contextmenu", this._contextMenu.bind(this), true);
        element.addEventListener("mousedown", this._mouseDown.bind(this), true);
        element.addEventListener("mousemove", this._mouseMove.bind(this), true);
        element.addEventListener("scroll", this._scroll.bind(this), true);
        this._parentElement.appendChild(element);

        this._textViewer.beginUpdates();

        this._textViewer.mimeType = mimeType;
        this._addExistingMessagesToSource();
        this._updateExecutionLine();
        this._updateDiffDecorations();
        this._textViewer.resize();

        if (this._lineNumberToReveal) {
            this.revealLine(this._lineNumberToReveal);
            delete this._lineNumberToReveal;
        }

        if (this._pendingMarkRange) {
            var range = this._pendingMarkRange;
            this.markAndRevealRange(range);
            delete this._pendingMarkRange;
        }

        if (this._lineToHighlight) {
            this.highlightLine(this._lineToHighlight);
            delete this._lineToHighlight;
        }

        if (this._delayedFindSearchMatches) {
            this._delayedFindSearchMatches();
            delete this._delayedFindSearchMatches;
        }

        var breakpoints = this._breakpoints();
        for (var i = 0; i < breakpoints.length; ++i)
            this._addBreakpoint(breakpoints[i]);
        WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointAdded, this._breakpointAdded, this);

        this._textViewer.endUpdates();

        if (this._canEditScripts)
            this._textViewer.editCallback = this._editLine.bind(this);
    },

    findSearchMatches: function(query, finishedCallback)
    {
        function doFindSearchMatches()
        {
            var ranges = [];

            // First do case-insensitive search.
            var regexObject = createSearchRegex(query);
            this._collectRegexMatches(regexObject, ranges);

            // Then try regex search if user knows the / / hint.
            try {
                if (/^\/.*\/$/.test(query))
                    this._collectRegexMatches(new RegExp(query.substring(1, query.length - 1)), ranges);
            } catch (e) {
                // Silent catch.
            }
            finishedCallback(ranges);
        }

        if (this._textViewer)
            doFindSearchMatches.call(this);
        else
            this._delayedFindSearchMatches = doFindSearchMatches.bind(this);
    },

    cancelFindSearchMatches: function()
    {
        delete this._delayedFindSearchMatches;
    },

    _collectRegexMatches: function(regexObject, ranges)
    {
        for (var i = 0; i < this._textModel.linesCount; ++i) {
            var line = this._textModel.line(i);
            var offset = 0;
            do {
                var match = regexObject.exec(line);
                if (match) {
                    ranges.push(new WebInspector.TextRange(i, offset + match.index, i, offset + match.index + match[0].length));
                    offset += match.index + 1;
                    line = line.substring(match.index + 1);
                }
            } while (match)
        }
        return ranges;
    },

    markAndRevealRange: function(range)
    {
        if (this._textViewer)
            this._textViewer.markAndRevealRange(range);
        else
            this._pendingMarkRange = range;
    },

    clearMarkedRange: function()
    {
        if (this._textViewer) {
            this._textViewer.markAndRevealRange(null);
        } else
            delete this._pendingMarkRange;
    },

    _incrementMessageRepeatCount: function(msg, repeatDelta)
    {
        if (!msg._resourceMessageLineElement)
            return;

        if (!msg._resourceMessageRepeatCountElement) {
            var repeatedElement = document.createElement("span");
            msg._resourceMessageLineElement.appendChild(repeatedElement);
            msg._resourceMessageRepeatCountElement = repeatedElement;
        }

        msg.repeatCount += repeatDelta;
        msg._resourceMessageRepeatCountElement.textContent = WebInspector.UIString(" (repeated %d times)", msg.repeatCount);
    },

    _updateExecutionLine: function(previousLine)
    {
        if (previousLine) {
            if (previousLine - 1 < this._textModel.linesCount)
                this._textViewer.removeDecoration(previousLine - 1, "webkit-execution-line");
        }

        if (!this._executionLine)
            return;

        if (this._executionLine < this._textModel.linesCount)
            this._textViewer.addDecoration(this._executionLine - 1, "webkit-execution-line");
    },

    _updateDiffDecorations: function()
    {
        if (!this._diffLines)
            return;

        function addDecorations(textViewer, lines, className)
        {
            for (var i = 0; i < lines.length; ++i)
                textViewer.addDecoration(lines[i], className);
        }
        addDecorations(this._textViewer, this._diffLines.added, "webkit-added-line");
        addDecorations(this._textViewer, this._diffLines.removed, "webkit-removed-line");
        addDecorations(this._textViewer, this._diffLines.changed, "webkit-changed-line");
    },

    _removeDiffDecorations: function()
    {
        function removeDecorations(textViewer, lines, className)
        {
            for (var i = 0; i < lines.length; ++i)
                textViewer.removeDecoration(lines[i], className);
        }
        removeDecorations(this._textViewer, this._diffLines.added, "webkit-added-line");
        removeDecorations(this._textViewer, this._diffLines.removed, "webkit-removed-line");
        removeDecorations(this._textViewer, this._diffLines.changed, "webkit-changed-line");
    },

    _addExistingMessagesToSource: function()
    {
        var length = this._messages.length;
        for (var i = 0; i < length; ++i)
            this._addMessageToSource(this._messages[i]);
    },

    _addMessageToSource: function(msg)
    {
        if (msg.line >= this._textModel.linesCount)
            return;

        var messageBubbleElement = this._messageBubbles[msg.line];
        if (!messageBubbleElement || messageBubbleElement.nodeType !== Node.ELEMENT_NODE || !messageBubbleElement.hasStyleClass("webkit-html-message-bubble")) {
            messageBubbleElement = document.createElement("div");
            messageBubbleElement.className = "webkit-html-message-bubble";
            this._messageBubbles[msg.line] = messageBubbleElement;
            this._textViewer.addDecoration(msg.line - 1, messageBubbleElement);
        }

        var rowMessages = this._rowMessages[msg.line];
        if (!rowMessages) {
            rowMessages = [];
            this._rowMessages[msg.line] = rowMessages;
        }

        for (var i = 0; i < rowMessages.length; ++i) {
            if (rowMessages[i].isEqual(msg)) {
                this._incrementMessageRepeatCount(rowMessages[i], msg.repeatDelta);
                return;
            }
        }

        rowMessages.push(msg);

        var imageURL;
        switch (msg.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                messageBubbleElement.addStyleClass("webkit-html-error-message");
                imageURL = "Images/errorIcon.png";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                messageBubbleElement.addStyleClass("webkit-html-warning-message");
                imageURL = "Images/warningIcon.png";
                break;
        }

        var messageLineElement = document.createElement("div");
        messageLineElement.className = "webkit-html-message-line";
        messageBubbleElement.appendChild(messageLineElement);

        // Create the image element in the Inspector's document so we can use relative image URLs.
        var image = document.createElement("img");
        image.src = imageURL;
        image.className = "webkit-html-message-icon";
        messageLineElement.appendChild(image);
        messageLineElement.appendChild(document.createTextNode(msg.message));

        msg._resourceMessageLineElement = messageLineElement;
    },

    _breakpointAdded: function(event)
    {
        var breakpoint = event.data;

        if (breakpoint.sourceID in this._sourceIDSet())
            this._addBreakpoint(breakpoint);
    },

    _addBreakpoint: function(breakpoint)
    {
        if (breakpoint.line > this._textModel.linesCount)
            return;

        breakpoint.addEventListener("enable-changed", this._breakpointChanged, this);
        breakpoint.addEventListener("condition-changed", this._breakpointChanged, this);
        breakpoint.addEventListener("removed", this._breakpointRemoved, this);

        this._setBreakpointDecoration(breakpoint.line, breakpoint.enabled, !!breakpoint.condition);
    },

    _breakpointRemoved: function(event)
    {
        var breakpoint = event.target;

        breakpoint.removeEventListener("enable-changed", null, this);
        breakpoint.removeEventListener("condition-changed", null, this);
        breakpoint.removeEventListener("removed", null, this);

        this._removeBreakpointDecoration(breakpoint.line);
    },

    _breakpointChanged: function(event)
    {
        var breakpoint = event.target;
        this._setBreakpointDecoration(breakpoint.line, breakpoint.enabled, !!breakpoint.condition);
    },

    _setBreakpointDecoration: function(lineNumber, enabled, hasCondition)
    {
        lineNumber -= 1;
        this._textViewer.beginUpdates();
        this._textViewer.addDecoration(lineNumber, "webkit-breakpoint");
        if (enabled)
            this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        else
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-disabled");
        if (hasCondition)
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-conditional");
        else
            this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this._textViewer.endUpdates();
    },

    _removeBreakpointDecoration: function(lineNumber)
    {
        lineNumber -= 1;
        this._textViewer.beginUpdates();
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint");
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this._textViewer.endUpdates();
    },

    _contextMenu: function(event)
    {
        if (!WebInspector.panels.scripts)
            return;

        var target = event.target.enclosingNodeOrSelfWithClass("webkit-line-number");
        if (!target)
            return;
        var lineNumber = target.parentElement.lineNumber + 1;

        var contextMenu = new WebInspector.ContextMenu();

        contextMenu.appendItem(WebInspector.UIString("Continue to Here"), this._continueToLine.bind(this, lineNumber));

        var breakpoint = this._findBreakpoint(lineNumber);
        if (!breakpoint) {
            // This row doesn't have a breakpoint: We want to show Add Breakpoint and Add and Edit Breakpoint.
            contextMenu.appendItem(WebInspector.UIString("Add Breakpoint"), this._setBreakpoint.bind(this, lineNumber, "", true));

            function addConditionalBreakpoint()
            {
                this._setBreakpointDecoration(lineNumber, true, true);
                function didEditBreakpointCondition(committed, condition)
                {
                    this._removeBreakpointDecoration(lineNumber);
                    if (committed)
                        this._setBreakpoint(lineNumber, true, condition);
                }
                this._editBreakpointCondition(lineNumber, "", didEditBreakpointCondition.bind(this));
            }
            contextMenu.appendItem(WebInspector.UIString("Add Conditional Breakpoint…"), addConditionalBreakpoint.bind(this));
        } else {
            // This row has a breakpoint, we want to show edit and remove breakpoint, and either disable or enable.
            contextMenu.appendItem(WebInspector.UIString("Remove Breakpoint"), breakpoint.remove.bind(breakpoint));
            function editBreakpointCondition()
            {
                function didEditBreakpointCondition(committed, condition)
                {
                    if (committed) {
                        breakpoint.remove();
                        this._setBreakpoint(breakpoint.line, breakpoint.enabled, condition);
                    }
                }
                this._editBreakpointCondition(lineNumber, breakpoint.condition, didEditBreakpointCondition.bind(this));
            }
            contextMenu.appendItem(WebInspector.UIString("Edit Breakpoint…"), editBreakpointCondition.bind(this));
            function setBreakpointEnabled(enabled)
            {
                breakpoint.remove();
                this._setBreakpoint(breakpoint.line, enabled, breakpoint.condition);
            }
            if (breakpoint.enabled)
                contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), setBreakpointEnabled.bind(this, false));
            else
                contextMenu.appendItem(WebInspector.UIString("Enable Breakpoint"), setBreakpointEnabled.bind(this, true));
        }
        contextMenu.show(event);
    },

    _scroll: function(event)
    {
        this._hidePopup();
    },

    _mouseDown: function(event)
    {
        this._resetHoverTimer();
        this._hidePopup();
        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey)
            return;
        var target = event.target.enclosingNodeOrSelfWithClass("webkit-line-number");
        if (!target)
            return;
        var lineNumber = target.parentElement.lineNumber + 1;

        var breakpoint = this._findBreakpoint(lineNumber);
        if (breakpoint) {
            breakpoint.remove();
            if (event.shiftKey)
                this._setBreakpoint(breakpoint.line, !breakpoint.enabled, breakpoint.condition);
        } else
            this._setBreakpoint(lineNumber, true, "");
        event.preventDefault();
    },

    _mouseMove: function(event)
    {
        // Pretend that nothing has happened.
        if (this._hoverElement === event.target || event.target.hasStyleClass("source-frame-eval-expression"))
            return;

        this._resetHoverTimer();
        // User has 500ms to reach the popup.
        if (this._popup) {
            var self = this;
            function doHide()
            {
                self._hidePopup();
                delete self._hidePopupTimer;
            }
            if (!("_hidePopupTimer" in this))
                this._hidePopupTimer = setTimeout(doHide, 500);
        }

        this._hoverElement = event.target;

        // Now that cleanup routines are set up above, leave this in case we are not on a break.
        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        // We are interested in identifiers and "this" keyword.
        if (this._hoverElement.hasStyleClass("webkit-javascript-keyword")) {
            if (this._hoverElement.textContent !== "this")
                return;
        } else if (!this._hoverElement.hasStyleClass("webkit-javascript-ident"))
            return;

        const toolTipDelay = this._popup ? 600 : 1000;
        this._hoverTimer = setTimeout(this._mouseHover.bind(this, this._hoverElement), toolTipDelay);
    },

    _resetHoverTimer: function()
    {
        if (this._hoverTimer) {
            clearTimeout(this._hoverTimer);
            delete this._hoverTimer;
        }
    },

    _hidePopup: function()
    {
        if (!this._popup)
            return;

        // Replace higlight element with its contents inplace.
        var parentElement = this._popup.highlightElement.parentElement;
        var child = this._popup.highlightElement.firstChild;
        while (child) {
            var nextSibling = child.nextSibling;
            parentElement.insertBefore(child, this._popup.highlightElement);
            child = nextSibling;
        }
        parentElement.removeChild(this._popup.highlightElement);

        this._popup.hide();
        delete this._popup;
        InspectorBackend.releaseWrapperObjectGroup(0, this._popoverObjectGroup);
    },

    _mouseHover: function(element)
    {
        delete this._hoverTimer;

        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        var lineRow = element.enclosingNodeOrSelfWithNodeName("tr");
        if (!lineRow)
            return;

        // Collect tokens belonging to evaluated exression.
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
        this._showPopup(container);
    },

    _showPopup: function(element)
    {
        function killHidePopupTimer()
        {
            if (this._hidePopupTimer) {
                clearTimeout(this._hidePopupTimer);
                delete this._hidePopupTimer;

                // We know that we reached the popup, but we might have moved over other elements.
                // Discard pending command.
                this._resetHoverTimer();
            }
        }

        function showObjectPopup(result)
        {
            if (!WebInspector.panels.scripts.paused)
                return;

            var popupContentElement = null;
            if (result.type !== "object" && result.type !== "node" && result.type !== "array") {
                popupContentElement = document.createElement("span");
                popupContentElement.className = "monospace console-formatted-" + result.type;
                popupContentElement.style.whiteSpace = "pre";
                popupContentElement.textContent = result.description;
                if (result.type === "string")
                    popupContentElement.textContent = "\"" + popupContentElement.textContent + "\"";
                this._popup = new WebInspector.Popover(popupContentElement);
                this._popup.show(element);
            } else {
                var popupContentElement = document.createElement("div");

                var titleElement = document.createElement("div");
                titleElement.className = "source-frame-popover-title monospace";
                titleElement.textContent = result.description;
                popupContentElement.appendChild(titleElement);

                var section = new WebInspector.ObjectPropertiesSection(result, "", null, false);
                section.expanded = true;
                section.element.addStyleClass("source-frame-popover-tree");
                section.headerElement.addStyleClass("hidden");
                popupContentElement.appendChild(section.element);

                this._popup = new WebInspector.Popover(popupContentElement);
                const popupWidth = 300;
                const popupHeight = 250;
                this._popup.show(element, popupWidth, popupHeight);
            }
            this._popup.highlightElement = element;
            this._popup.highlightElement.addStyleClass("source-frame-eval-expression");
            popupContentElement.addEventListener("mousemove", killHidePopupTimer.bind(this), true);
        }

        function evaluateCallback(result)
        {
            if (result.isError())
                return;
            if (!WebInspector.panels.scripts.paused)
                return;
            showObjectPopup.call(this, result);
        }
        WebInspector.panels.scripts.evaluateInSelectedCallFrame(element.textContent, false, this._popoverObjectGroup, evaluateCallback.bind(this));
    },

    _editBreakpointCondition: function(lineNumber, condition, callback)
    {
        lineNumber -= 1;
        this._conditionElement = this._createConditionElement(lineNumber);
        this._textViewer.addDecoration(lineNumber, this._conditionElement);

        function finishEditing(committed, element, newText)
        {
            this._textViewer.removeDecoration(lineNumber, this._conditionElement);
            delete this._conditionEditorElement;
            delete this._conditionElement;
            callback(committed, newText);
        }

        WebInspector.startEditing(this._conditionEditorElement, {
            context: null,
            commitHandler: finishEditing.bind(this, true),
            cancelHandler: finishEditing.bind(this, false)
        });
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
        editorElement.type = "text"
        conditionElement.appendChild(editorElement);
        this._conditionEditorElement = editorElement;

        return conditionElement;
    },

    _evalSelectionInCallFrame: function(event)
    {
        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        var selection = this.element.contentWindow.getSelection();
        if (!selection.rangeCount)
            return;

        var expression = selection.getRangeAt(0).toString().trim();
        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, "console", function(result) {
            WebInspector.showConsole();
            var commandMessage = new WebInspector.ConsoleCommand(expression);
            WebInspector.console.addMessage(commandMessage);
            WebInspector.console.addMessage(new WebInspector.ConsoleCommandResult(result, commandMessage));
        });
    },

    resize: function()
    {
        if (this._textViewer)
            this._textViewer.resize();
    },

    _continueToLine: function(lineNumber)
    {
        var sourceID = this._sourceIDForLine(lineNumber);
        if (!sourceID)
            return;
        WebInspector.debuggerModel.continueToLine(sourceID, lineNumber);
    },

    _editLine: function(lineNumber, newContent, cancelEditingCallback)
    {
        lineNumber += 1;

        var lines = [];
        var oldLines = this._content.split('\n');
        for (var i = 0; i < oldLines.length; ++i) {
            if (i === lineNumber - 1)
                lines.push(newContent);
            else
                lines.push(oldLines[i]);
        }

        var editData = {};
        editData.sourceID = this._sourceIDForLine(lineNumber);
        editData.content = lines.join("\n");
        editData.line = lineNumber;
        editData.linesCountToShift = newContent.split("\n").length - 1;
        this._doEditLine(editData, cancelEditingCallback);
    },

    _revertEditLine: function(editData, contentToRevertTo)
    {
        var newEditData = {};
        newEditData.sourceID = editData.sourceID;
        newEditData.content = contentToRevertTo;
        newEditData.line = editData.line;
        newEditData.linesCountToShift = -editData.linesCountToShift;
        this._doEditLine(newEditData);
    },

    _doEditLine: function(editData, cancelEditingCallback)
    {
        var revertEditingCallback = this._revertEditLine.bind(this, editData);
        WebInspector.panels.scripts.editScriptSource(editData, revertEditingCallback, cancelEditingCallback);
    },

    _setBreakpoint: function(lineNumber, enabled, condition)
    {
        var sourceID = this._sourceIDForLine(lineNumber);
        if (!sourceID)
            return;
        WebInspector.debuggerModel.setBreakpoint(sourceID, lineNumber, enabled, condition);
        if (!WebInspector.panels.scripts.breakpointsActivated)
            WebInspector.panels.scripts.toggleBreakpointsClicked();
    },

    _breakpoints: function()
    {
        var sourceIDSet = this._sourceIDSet();
        return WebInspector.debuggerModel.queryBreakpoints(function(b) { return b.sourceID in sourceIDSet; });
    },

    _findBreakpoint: function(lineNumber)
    {
        var sourceID = this._sourceIDForLine(lineNumber);
        return WebInspector.debuggerModel.findBreakpoint(sourceID, lineNumber);
    },

    _sourceIDForLine: function(lineNumber)
    {
        var sourceIDForLine = null;
        var closestStartingLine = 0;
        var scripts = this._contentProvider.scripts();
        for (var i = 0; i < scripts.length; ++i) {
            var startingLine = scripts[i].startingLine;
            if (startingLine <= lineNumber && startingLine >= closestStartingLine) {
                closestStartingLine = startingLine;
                sourceIDForLine = scripts[i].sourceID;
            }
        }
        return sourceIDForLine;
    },

    _sourceIDSet: function()
    {
        var scripts = this._contentProvider.scripts();
        var sourceIDSet = {};
        for (var i = 0; i < scripts.length; ++i)
            sourceIDSet[scripts[i].sourceID] = true;
        return sourceIDSet;
    }
}

WebInspector.SourceFrame.prototype.__proto__ = WebInspector.Object.prototype;


WebInspector.SourceFrameContentProvider = function()
{
}

WebInspector.SourceFrameContentProvider.prototype = {
    requestContent: function(callback)
    {
        // Should be implemented by subclasses.
    },

    scripts: function()
    {
        // Should be implemented by subclasses.
    }
}

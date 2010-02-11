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

WebInspector.SourceFrame = function(parentElement, addBreakpointDelegate, removeBreakpointDelegate)
{
    this._parentElement = parentElement;

    this._textModel = new WebInspector.TextEditorModel();
    this._textModel.replaceTabsWithSpaces = true;

    this._messages = [];
    this._rowMessages = {};
    this._messageBubbles = {};
    this.breakpoints = [];
    this._shortcuts = {};

    this._loaded = false;

    this._addBreakpointDelegate = addBreakpointDelegate;
    this._removeBreakpointDelegate = removeBreakpointDelegate;
}

WebInspector.SourceFrame.prototype = {

    set visible(visible)
    {
        this._visible = visible;
        this._createViewerIfNeeded();
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

    revealLine: function(lineNumber)
    {
        if (this._textViewer)
            this._textViewer.revealLine(lineNumber - 1, 0);
        else
            this._lineNumberToReveal = lineNumber;
    },

    addBreakpoint: function(breakpoint)
    {
        this.breakpoints.push(breakpoint);
        breakpoint.addEventListener("enabled", this._breakpointChanged, this);
        breakpoint.addEventListener("disabled", this._breakpointChanged, this);
        breakpoint.addEventListener("condition-changed", this._breakpointChanged, this);
        if (this._textViewer)
            this._addBreakpointToSource(breakpoint);
    },

    removeBreakpoint: function(breakpoint)
    {
        this.breakpoints.remove(breakpoint);
        breakpoint.removeEventListener("enabled", null, this);
        breakpoint.removeEventListener("disabled", null, this);
        breakpoint.removeEventListener("condition-changed", null, this);
        if (this._textViewer)
            this._removeBreakpointFromSource(breakpoint);
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

    setContent: function(mimeType, content, url)
    {
        this._loaded = true;
        this._textModel.setText(null, content);
        this._mimeType = mimeType;
        this._url = url;
        this._createViewerIfNeeded();
    },

    highlightLine: function(line)
    {
        if (this._textViewer)
            this._textViewer.highlightLine(line - 1);
        else
            this._lineToHighlight = line;
    },

    _createViewerIfNeeded: function()
    {
        if (!this._visible || !this._loaded || this._textViewer)
            return;

        this._textViewer = new WebInspector.TextViewer(this._textModel, WebInspector.platform, this._url);
        var element = this._textViewer.element;
        element.addEventListener("keydown", this._keyDown.bind(this), true);
        element.addEventListener("contextmenu", this._contextMenu.bind(this), true);
        element.addEventListener("mousedown", this._mouseDown.bind(this), true);
        this._parentElement.appendChild(element);

        this._needsProgramCounterImage = true;
        this._needsBreakpointImages = true;

        this._textViewer.beginUpdates();

        this._textViewer.mimeType = this._mimeType;
        this._addExistingMessagesToSource();
        this._addExistingBreakpointsToSource();
        this._updateExecutionLine();
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
        this._textViewer.endUpdates();
    },

    findSearchMatches: function(query)
    {
        var ranges = [];

        // First do case-insensitive search.
        var regex = "";
        for (var i = 0; i < query.length; ++i) {
            var char = query.charAt(i);
            if (char === "]")
                char = "\\]";
            regex += "[" + char + "]";
        }
        var regexObject = new RegExp(regex, "i");
        this._collectRegexMatches(regexObject, ranges);

        // Then try regex search if user knows the / / hint.
        try {
            if (/^\/.*\/$/.test(query))
                this._collectRegexMatches(new RegExp(query.substring(1, query.length - 1)), ranges);
        } catch (e) {
            // Silent catch.
        }
        return ranges;
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

    _breakpointChanged: function(event)
    {
        var breakpoint = event.target;
        var lineNumber = breakpoint.line - 1;
        if (lineNumber >= this._textModel.linesCount)
            return;

        if (breakpoint.enabled)
            this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        else
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-disabled");

        if (breakpoint.condition)
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-conditional");
        else
            this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
    },

    _updateExecutionLine: function(previousLine)
    {
        if (previousLine) {
            if (previousLine - 1 < this._textModel.linesCount)
                this._textViewer.removeDecoration(previousLine - 1, "webkit-execution-line");
        }

        if (!this._executionLine)
            return;

        this._drawProgramCounterImageIfNeeded();

        if (this._executionLine < this._textModel.linesCount)
            this._textViewer.addDecoration(this._executionLine - 1, "webkit-execution-line");
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
            if (rowMessages[i].isEqual(msg, true)) {
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

    _addExistingBreakpointsToSource: function()
    {
        for (var i = 0; i < this.breakpoints.length; ++i)
            this._addBreakpointToSource(this.breakpoints[i]);
    },

    _addBreakpointToSource: function(breakpoint)
    {
        var lineNumber = breakpoint.line - 1;
        if (lineNumber >= this._textModel.linesCount)
            return;

        this._textModel.setAttribute(lineNumber, "breakpoint", breakpoint);
        breakpoint.sourceText = this._textModel.line(breakpoint.line - 1);
        this._drawBreakpointImagesIfNeeded();

        this._textViewer.beginUpdates();
        this._textViewer.addDecoration(lineNumber, "webkit-breakpoint");
        if (!breakpoint.enabled)
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-disabled");
        if (breakpoint.condition)
            this._textViewer.addDecoration(lineNumber, "webkit-breakpoint-conditional");
        this._textViewer.endUpdates();
    },

    _removeBreakpointFromSource: function(breakpoint)
    {
        var lineNumber = breakpoint.line - 1;
        this._textViewer.beginUpdates();
        this._textModel.removeAttribute(lineNumber, "breakpoint");
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint");
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-disabled");
        this._textViewer.removeDecoration(lineNumber, "webkit-breakpoint-conditional");
        this._textViewer.endUpdates();
    },

    _contextMenu: function(event)
    {
        if (event.target.className !== "webkit-line-number")
            return;
        var row = event.target.parentElement;

        var lineNumber = row.lineNumber;
        var contextMenu = new WebInspector.ContextMenu();

        var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
        if (!breakpoint) {
            // This row doesn't have a breakpoint: We want to show Add Breakpoint and Add and Edit Breakpoint.
            contextMenu.appendItem(WebInspector.UIString("Add Breakpoint"), this._addBreakpointDelegate.bind(this, lineNumber + 1));

            function addConditionalBreakpoint() 
            {
                this._addBreakpointDelegate(lineNumber + 1);
                var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
                if (breakpoint)
                    this._editBreakpointCondition(breakpoint);
            }

            contextMenu.appendItem(WebInspector.UIString("Add Conditional Breakpoint..."), addConditionalBreakpoint.bind(this));
        } else {
            // This row has a breakpoint, we want to show edit and remove breakpoint, and either disable or enable.
            contextMenu.appendItem(WebInspector.UIString("Remove Breakpoint"), WebInspector.panels.scripts.removeBreakpoint.bind(WebInspector.panels.scripts, breakpoint));
            contextMenu.appendItem(WebInspector.UIString("Edit Breakpoint..."), this._editBreakpointCondition.bind(this, breakpoint));
            if (breakpoint.enabled)
                contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), function() { breakpoint.enabled = false; });
            else
                contextMenu.appendItem(WebInspector.UIString("Enable Breakpoint"), function() { breakpoint.enabled = true; });
        }
        contextMenu.show(event);
    },

    _mouseDown: function(event)
    {
        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey || event.shiftKey)
            return;
        if (event.target.className !== "webkit-line-number")
            return;
        var row = event.target.parentElement;

        var lineNumber = row.lineNumber;

        var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
        if (breakpoint)
            this._removeBreakpointDelegate(breakpoint);
        else if (this._addBreakpointDelegate)
            this._addBreakpointDelegate(lineNumber + 1);
        event.preventDefault();
    },

    _editBreakpointCondition: function(breakpoint)
    {
        this._showBreakpointConditionPopup(breakpoint.line);

        function committed(element, newText)
        {
            breakpoint.condition = newText;
            dismissed.call(this);
        }

        function dismissed()
        {
            if (this._conditionElement)
                this._textViewer.removeDecoration(breakpoint.line - 1, this._conditionElement);
            delete this._conditionEditorElement;
            delete this._conditionElement;
        }

        var dismissedHandler = dismissed.bind(this);
        this._conditionEditorElement.addEventListener("blur", dismissedHandler, false);

        WebInspector.startEditing(this._conditionEditorElement, committed.bind(this), dismissedHandler);
        this._conditionEditorElement.value = breakpoint.condition;
        this._conditionEditorElement.select();
    },

    _showBreakpointConditionPopup: function(lineNumber)
    {
        this._conditionElement = this._createConditionElement(lineNumber);
        this._textViewer.addDecoration(lineNumber - 1, this._conditionElement);
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

    _keyDown: function(event)
    {
        var shortcut = WebInspector.KeyboardShortcut.makeKeyFromEvent(event);
        var handler = this._shortcuts[shortcut];
        if (handler) {
            handler(event);
            event.preventDefault();
        } else
            WebInspector.documentKeyDown(event);
    },

    _evalSelectionInCallFrame: function(event)
    {
        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        var selection = this.element.contentWindow.getSelection();
        if (!selection.rangeCount)
            return;

        var expression = selection.getRangeAt(0).toString().trimWhitespace();
        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, "console", function(result, exception) {
            WebInspector.showConsole();
            var commandMessage = new WebInspector.ConsoleCommand(expression);
            WebInspector.console.addMessage(commandMessage);
            WebInspector.console.addMessage(new WebInspector.ConsoleCommandResult(result, exception, commandMessage));
        });
    },

    resize: function()
    {
        if (this._textViewer)
            this._textViewer.resize();
    },

    _drawProgramCounterInContext: function(ctx, glow)
    {
        if (glow)
            ctx.save();

        ctx.beginPath();
        ctx.moveTo(17, 2);
        ctx.lineTo(19, 2);
        ctx.lineTo(19, 0);
        ctx.lineTo(21, 0);
        ctx.lineTo(26, 5.5);
        ctx.lineTo(21, 11);
        ctx.lineTo(19, 11);
        ctx.lineTo(19, 9);
        ctx.lineTo(17, 9);
        ctx.closePath();
        ctx.fillStyle = "rgb(142, 5, 4)";

        if (glow) {
            ctx.shadowBlur = 4;
            ctx.shadowColor = "rgb(255, 255, 255)";
            ctx.shadowOffsetX = -1;
            ctx.shadowOffsetY = 0;
        }

        ctx.fill();
        ctx.fill(); // Fill twice to get a good shadow and darker anti-aliased pixels.

        if (glow)
            ctx.restore();
    },

    _drawProgramCounterImageIfNeeded: function()
    {
        if (!this._needsProgramCounterImage)
            return;

        var ctx = document.getCSSCanvasContext("2d", "program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        this._drawProgramCounterInContext(ctx, true);

        delete this._needsProgramCounterImage;
    },

    _drawBreakpointImagesIfNeeded: function(conditional)
    {
        if (!this._needsBreakpointImages)
            return;

        function drawBreakpoint(ctx, disabled, conditional)
        {
            ctx.beginPath();
            ctx.moveTo(0, 2);
            ctx.lineTo(2, 0);
            ctx.lineTo(21, 0);
            ctx.lineTo(26, 5.5);
            ctx.lineTo(21, 11);
            ctx.lineTo(2, 11);
            ctx.lineTo(0, 9);
            ctx.closePath();
            ctx.fillStyle = conditional ? "rgb(217, 142, 1)" : "rgb(1, 142, 217)";
            ctx.strokeStyle = conditional ? "rgb(205, 103, 0)" : "rgb(0, 103, 205)";
            ctx.lineWidth = 3;
            ctx.fill();
            ctx.save();
            ctx.clip();
            ctx.stroke();
            ctx.restore();

            if (!disabled)
                return;

            ctx.save();
            ctx.globalCompositeOperation = "destination-out";
            ctx.fillStyle = "rgba(0, 0, 0, 0.5)";
            ctx.fillRect(0, 0, 26, 11);
            ctx.restore();
        }


        // Unconditional breakpoints.

        var ctx = document.getCSSCanvasContext("2d", "breakpoint", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-disabled", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-disabled-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);


        // Conditional breakpoints.

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-conditional", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, false, true);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-conditional-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, false, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-disabled-conditional", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true, true);

        var ctx = document.getCSSCanvasContext("2d", "breakpoint-disabled-conditional-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        delete this._needsBreakpointImages;
    }
}

WebInspector.SourceFrame.prototype.__proto__ = WebInspector.Object.prototype;

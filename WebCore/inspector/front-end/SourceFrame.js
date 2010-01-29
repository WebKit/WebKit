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

WebInspector.SourceFrame = function(parentElement, addBreakpointDelegate)
{
    this._parentElement = parentElement;

    this._textModel = new WebInspector.TextEditorModel();

    this._messages = [];
    this._rowMessages = {};
    this._messageBubbles = {};
    this.breakpoints = [];
    this._shortcuts = {};

    this._loaded = false;

    this._addBreakpointDelegate = addBreakpointDelegate;
}

WebInspector.SourceFrame.prototype = {

    set visible(visible)
    {
        this._visible = visible;
        this._createEditorIfNeeded();
    },

    get executionLine()
    {
        return this._executionLine;
    },

    set executionLine(x)
    {
        if (this._executionLine === x)
            return;
        this._executionLine = x;
        if (this._editor)
            this._editor.repaintAll();
    },

    revealLine: function(lineNumber)
    {
        if (this._editor)
            this._editor.reveal(lineNumber - 1, 0);
        else
            this._lineNumberToReveal = lineNumber;
    },

    addBreakpoint: function(breakpoint)
    {
        this.breakpoints.push(breakpoint);
        breakpoint.addEventListener("enabled", this._breakpointChanged, this);
        breakpoint.addEventListener("disabled", this._breakpointChanged, this);
        breakpoint.addEventListener("condition-changed", this._breakpointChanged, this);
        this._addBreakpointToSource(breakpoint);
    },

    removeBreakpoint: function(breakpoint)
    {
        this.breakpoints.remove(breakpoint);
        breakpoint.removeEventListener("enabled", null, this);
        breakpoint.removeEventListener("disabled", null, this);
        breakpoint.removeEventListener("condition-changed", null, this);
        this._removeBreakpointFromSource(breakpoint);
    },

    addMessage: function(msg)
    {
        // Don't add the message if there is no message or valid line or if the msg isn't an error or warning.
        if (!msg.message || msg.line <= 0 || !msg.isErrorOrWarning())
            return;
        this._messages.push(msg)
        if (this._editor)
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
        if (this._editor)
            this._editor.packAndRepaintAll();
    },

    sizeToFitContentHeight: function()
    {
        if (this._editor)
            this._editor.packAndRepaintAll();
    },

    setContent: function(mimeType, content)
    {
        this._loaded = true;
        this._textModel.setText(null, content);
        this._mimeType = mimeType;
        this._createEditorIfNeeded();
    },

    _createEditorIfNeeded: function()
    {
        if (!this._visible || !this._loaded || this._editor)
            return;

        this._editor = new WebInspector.TextEditor(this._textModel, WebInspector.platform);
        this._editor.lineNumberDecorator = new WebInspector.BreakpointLineNumberDecorator(this, this._editor.textModel);
        this._editor.lineDecorator = new WebInspector.ExecutionLineDecorator(this);
        this._editor.readOnly = true;
        this._element = this._editor.element;
        this._element.addEventListener("keydown", this._keyDown.bind(this), true);
        this._parentElement.appendChild(this._element);

        this._editor.mimeType = this._mimeType;

        this._addExistingMessagesToSource();
        this._addExistingBreakpointsToSource();

        this._editor.setCoalescingUpdate(true);
        this._editor.updateCanvasSize();
        this._editor.packAndRepaintAll();

        if (this._executionLine)
            this.revealLine(this._executionLine);

        if (this._lineNumberToReveal) {
            this.revealLine(this._lineNumberToReveal);
            delete this._lineNumberToReveal;
        }

        if (this._pendingSelectionRange) {
            var range = this._pendingSelectionRange;
            this._editor.setSelection(range.startLine, range.startColumn, range.endLine, range.endColumn);
            delete this._pendingSelectionRange;
        }
        this._editor.setCoalescingUpdate(false);
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

    setSelection: function(range)
    {
        if (this._editor)
            this._editor.setSelection(range.startLine, range.startColumn, range.endLine, range.endColumn);
        else
            this._pendingSelectionRange = range;
    },

    clearSelection: function()
    {
        if (this._editor) {
            var range = this._editor.selection;
            this._editor.setSelection(range.endLine, range.endColumn, range.endLine, range.endColumn);
        } else
            delete this._pendingSelectionRange;
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
            this._editor.setDivDecoration(msg.line - 1, messageBubbleElement);
        }

        var rowMessages = this._rowMessages[msg.line];
        if (!rowMessages) {
            rowMessages = [];
            this._rowMessages[msg.line] = rowMessages;
        }

        for (var i = 0; i < rowMessages.length; ++i) {
            if (rowMessages[i].isEqual(msg, true)) {
                this._incrementMessageRepeatCount(rowMessages[i], msg.repeatDelta);
                this._editor.packAndRepaintAll();
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

        this._editor.packAndRepaintAll();
    },

    _addExistingBreakpointsToSource: function()
    {
        var length = this.breakpoints.length;
        for (var i = 0; i < length; ++i)
            this._addBreakpointToSource(this.breakpoints[i]);
    },

    _addBreakpointToSource: function(breakpoint)
    {
        this._textModel.setAttribute(breakpoint.line - 1, "breakpoint", breakpoint);
        this._editor.paintLineNumbers();
    },

    _removeBreakpointFromSource: function(breakpoint)
    {
        this._textModel.removeAttribute(breakpoint.line - 1, "breakpoint");
        this._editor.paintLineNumbers();
    },

    _contextMenu: function(lineNumber, event)
    {
        if (!this._addBreakpointDelegate)
            return;

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

    _toggleBreakpoint: function(lineNumber, event)
    {
        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey || event.shiftKey)
            return;
        var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
        if (breakpoint)
            WebInspector.panels.scripts.removeBreakpoint(breakpoint);
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
            this._editor.paintLineNumbers();
            dismissed.call(this);
        }

        function dismissed()
        {
            this._editor.setDivDecoration(breakpoint.line - 1, null);
            delete this._conditionEditorElement;
        }

        var dismissedHandler = dismissed.bind(this);
        this._conditionEditorElement.addEventListener("blur", dismissedHandler, false);

        WebInspector.startEditing(this._conditionEditorElement, committed.bind(this), dismissedHandler);
        this._conditionEditorElement.value = breakpoint.condition;
        this._conditionEditorElement.select();
    },

    _showBreakpointConditionPopup: function(lineNumber)
    {
        var conditionElement = this._createConditionElement(lineNumber);
        this._editor.setDivDecoration(lineNumber - 1, conditionElement);
    },

    _createConditionElement: function(lineNumber)
    {
        var conditionElement = document.createElement("div");
        conditionElement.className = "source-breakpoint-condition";

        var labelElement = document.createElement("label");
        labelElement.className = "source-breakpoint-message";
        labelElement.htmlFor = "source-breakpoint-condition";
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("The breakpoint on line %d will stop only if this expression is true:", lineNumber)));
        conditionElement.appendChild(labelElement);

        var editorElement = document.createElement("input");
        editorElement.id = "source-breakpoint-condition";
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
        } else {
            WebInspector.documentKeyDown(event);
        }
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

    _breakpointChanged: function(event)
    {
        this._editor.paintLineNumbers();
    },

    resize: function()
    {
        if (this._editor)
            this._editor.updateCanvasSize();
    }
}

WebInspector.SourceFrame.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.BreakpointLineNumberDecorator = function(sourceFrame, textModel)
{
    this._sourceFrame = sourceFrame;
    this._textModel = textModel;
}

WebInspector.BreakpointLineNumberDecorator.prototype = {
    decorate: function(lineNumber, ctx, x, y, width, height, lineHeight)
    {
        var breakpoint = this._textModel.getAttribute(lineNumber, "breakpoint");
        var isExecutionLine = lineNumber + 1 === this._sourceFrame._executionLine;
        if (breakpoint || isExecutionLine) {
            ctx.save();
            ctx.translate(x + 4, y + 2);
            var breakpointWidth = width - 6;
            var breakpointHeight = lineHeight - 4;
    
            if (breakpoint)
                this._paintBreakpoint(ctx, breakpointWidth, breakpointHeight, breakpoint);
    
            if (isExecutionLine)
                this._paintProgramCounter(ctx, breakpointWidth, breakpointHeight, false);

            ctx.restore();
        }

        if (isExecutionLine) {
            // Override default behavior.
            return true;
        }

        ctx.fillStyle = breakpoint ? "rgb(255,255,255)" : "rgb(155,155,155)";
        return false;
    },

    _paintBreakpoint: function(ctx, width, height, breakpoint)
    {
        ctx.beginPath();
        ctx.moveTo(0, 2);
        ctx.lineTo(2, 0);
        ctx.lineTo(width - 5, 0);
        ctx.lineTo(width, height / 2);
        ctx.lineTo(width - 5, height);
        ctx.lineTo(2, height);
        ctx.lineTo(0, height - 2);
        ctx.closePath();
        ctx.fillStyle = breakpoint.condition ? "rgb(217, 142, 1)" : "rgb(1, 142, 217)";
        ctx.strokeStyle = breakpoint.condition ? "rgb(205, 103, 0)" : "rgb(0, 103, 205)";
        ctx.lineWidth = 3;
        ctx.fill();

        ctx.save();
        ctx.clip();
        ctx.stroke();
        ctx.restore();

        if (!breakpoint.enabled) {
            ctx.save();
            ctx.globalCompositeOperation = "destination-out";
            ctx.fillStyle = "rgba(0, 0, 0, 0.5)";
            ctx.fillRect(0, 0, width, height);
            ctx.restore();
        }
    },

    _paintProgramCounter: function(ctx, width, height)
    {
        ctx.save();

        ctx.beginPath();
        ctx.moveTo(width - 9, 2);
        ctx.lineTo(width - 7, 2);
        ctx.lineTo(width - 7, 0);
        ctx.lineTo(width - 5, 0);
        ctx.lineTo(width, height / 2);
        ctx.lineTo(width - 5, height);
        ctx.lineTo(width - 7, height);
        ctx.lineTo(width - 7, height - 2);
        ctx.lineTo(width - 9, height - 2);
        ctx.closePath();
        ctx.fillStyle = "rgb(142, 5, 4)";

        ctx.shadowBlur = 4;
        ctx.shadowColor = "rgb(255, 255, 255)";
        ctx.shadowOffsetX = -1;
        ctx.shadowOffsetY = 0;

        ctx.fill();
        ctx.fill(); // Fill twice to get a good shadow and darker anti-aliased pixels.

        ctx.restore();
    },

    mouseDown: function(lineNumber, e)
    {
        this._sourceFrame._toggleBreakpoint(lineNumber, e);
        return true;
    },

    contextMenu: function(lineNumber, e)
    {
        this._sourceFrame._contextMenu(lineNumber, e);
        return true;
    }
}

WebInspector.ExecutionLineDecorator = function(sourceFrame)
{
    this._sourceFrame = sourceFrame;
}

WebInspector.ExecutionLineDecorator.prototype = {
    decorate: function(lineNumber, ctx, x, y, width, height, lineHeight)
    {
        if (this._sourceFrame._executionLine !== lineNumber + 1)
            return;
        ctx.save();
        ctx.fillStyle = "rgb(171, 191, 254)";
        ctx.fillRect(x, y, width, height);
        
        ctx.beginPath();
        ctx.rect(x - 1, y, width + 2, height);
        ctx.clip();
        ctx.strokeStyle = "rgb(64, 115, 244)";
        ctx.stroke();

        ctx.restore();
    }
}

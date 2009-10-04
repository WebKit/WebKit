/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SourceFrame = function(element, addBreakpointDelegate)
{
    this.messages = [];
    this.breakpoints = [];
    this._shortcuts = {};

    this.addBreakpointDelegate = addBreakpointDelegate;

    this.element = element || document.createElement("iframe");
    this.element.addStyleClass("source-view-frame");
    this.element.setAttribute("viewsource", "true");

    this.element.addEventListener("load", this._loaded.bind(this), false);
}

WebInspector.SourceFrame.prototype = {
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

        this._updateExecutionLine(previousLine);
    },

    get autoSizesToFitContentHeight()
    {
        return this._autoSizesToFitContentHeight;
    },

    set autoSizesToFitContentHeight(x)
    {
        if (this._autoSizesToFitContentHeight === x)
            return;

        this._autoSizesToFitContentHeight = x;

        if (this._autoSizesToFitContentHeight) {
            this._windowResizeListener = this._windowResized.bind(this);
            window.addEventListener("resize", this._windowResizeListener, false);
            this.sizeToFitContentHeight();
        } else {
            this.element.style.removeProperty("height");
            if (this.element.contentDocument)
                this.element.contentDocument.body.removeStyleClass("webkit-height-sized-to-fit");
            window.removeEventListener("resize", this._windowResizeListener, false);
            delete this._windowResizeListener;
        }
    },

    sourceRow: function(lineNumber)
    {
        if (!lineNumber || !this.element.contentDocument)
            return;

        var table = this.element.contentDocument.getElementsByTagName("table")[0];
        if (!table)
            return;

        var rows = table.rows;

        // Line numbers are a 1-based index, but the rows collection is 0-based.
        --lineNumber;

        return rows[lineNumber];
    },

    lineNumberForSourceRow: function(sourceRow)
    {
        // Line numbers are a 1-based index, but the rows collection is 0-based.
        var lineNumber = 0;
        while (sourceRow) {
            ++lineNumber;
            sourceRow = sourceRow.previousSibling;
        }

        return lineNumber;
    },

    revealLine: function(lineNumber)
    {
        if (!this._isContentLoaded()) {
            this._lineNumberToReveal = lineNumber;
            return;
        }

        var row = this.sourceRow(lineNumber);
        if (row)
            row.scrollIntoViewIfNeeded(true);
    },

    addBreakpoint: function(breakpoint)
    {
        this.breakpoints.push(breakpoint);
        breakpoint.addEventListener("enabled", this._breakpointEnableChanged, this);
        breakpoint.addEventListener("disabled", this._breakpointEnableChanged, this);
        this._addBreakpointToSource(breakpoint);
    },

    removeBreakpoint: function(breakpoint)
    {
        this.breakpoints.remove(breakpoint);
        breakpoint.removeEventListener("enabled", null, this);
        breakpoint.removeEventListener("disabled", null, this);
        this._removeBreakpointFromSource(breakpoint);
    },

    addMessage: function(msg)
    {
        // Don't add the message if there is no message or valid line or if the msg isn't an error or warning.
        if (!msg.message || msg.line <= 0 || !msg.isErrorOrWarning())
            return;
        this.messages.push(msg);
        this._addMessageToSource(msg);
    },

    clearMessages: function()
    {
        this.messages = [];

        if (!this.element.contentDocument)
            return;

        var bubbles = this.element.contentDocument.querySelectorAll(".webkit-html-message-bubble");
        if (!bubbles)
            return;

        for (var i = 0; i < bubbles.length; ++i) {
            var bubble = bubbles[i];
            bubble.parentNode.removeChild(bubble);
        }
    },

    sizeToFitContentHeight: function()
    {
        if (this.element.contentDocument) {
            this.element.style.setProperty("height", this.element.contentDocument.body.offsetHeight + "px");
            this.element.contentDocument.body.addStyleClass("webkit-height-sized-to-fit");
        }
    },

    _highlightLineEnds: function(event)
    {
        event.target.parentNode.removeStyleClass("webkit-highlighted-line");
    },

    highlightLine: function(lineNumber)
    {
        if (!this._isContentLoaded()) {
            this._lineNumberToHighlight = lineNumber;
            return;
        }

        var sourceRow = this.sourceRow(lineNumber);
        if (!sourceRow)
            return;
        var line = sourceRow.getElementsByClassName('webkit-line-content')[0];
        // Trick to reset the animation if the user clicks on the same link
        // Using a timeout to avoid coalesced style updates
        line.style.setProperty("-webkit-animation-name", "none");
        setTimeout(function () {
            line.style.removeProperty("-webkit-animation-name");
            sourceRow.addStyleClass("webkit-highlighted-line");
        }, 0);
    },

    _loaded: function()
    {
        WebInspector.addMainEventListeners(this.element.contentDocument);
        this.element.contentDocument.addEventListener("contextmenu", this._documentContextMenu.bind(this), true);
        this.element.contentDocument.addEventListener("mousedown", this._documentMouseDown.bind(this), true);
        this.element.contentDocument.addEventListener("keydown", this._documentKeyDown.bind(this), true);
        this.element.contentDocument.addEventListener("keyup", WebInspector.documentKeyUp.bind(WebInspector), true);
        this.element.contentDocument.addEventListener("webkitAnimationEnd", this._highlightLineEnds.bind(this), false);

        // Register 'eval' shortcut.
        var isMac = InspectorController.platform().indexOf("mac-") === 0;
        var platformSpecificModifier = isMac ? WebInspector.KeyboardShortcut.Modifiers.Meta : WebInspector.KeyboardShortcut.Modifiers.Ctrl;
        var shortcut = WebInspector.KeyboardShortcut.makeKey(69 /* 'E' */, platformSpecificModifier | WebInspector.KeyboardShortcut.Modifiers.Shift);
        this._shortcuts[shortcut] = this._evalSelectionInCallFrame.bind(this);

        var headElement = this.element.contentDocument.getElementsByTagName("head")[0];
        if (!headElement) {
            headElement = this.element.contentDocument.createElement("head");
            this.element.contentDocument.documentElement.insertBefore(headElement, this.element.contentDocument.documentElement.firstChild);
        }

        var styleElement = this.element.contentDocument.createElement("style");
        headElement.appendChild(styleElement);

        // Add these style rules here since they are specific to the Inspector. They also behave oddly and not
        // all properties apply if added to view-source.css (becuase it is a user agent sheet.)
        var styleText = ".webkit-line-number { background-repeat: no-repeat; background-position: right 1px; }\n";
        styleText += ".webkit-execution-line .webkit-line-number { color: transparent; background-image: -webkit-canvas(program-counter); }\n";

        styleText += ".webkit-breakpoint .webkit-line-number { color: white; background-image: -webkit-canvas(breakpoint); }\n";
        styleText += ".webkit-breakpoint-disabled .webkit-line-number { color: white; background-image: -webkit-canvas(breakpoint-disabled); }\n";
        styleText += ".webkit-breakpoint.webkit-execution-line .webkit-line-number { color: transparent; background-image: -webkit-canvas(breakpoint-program-counter); }\n";
        styleText += ".webkit-breakpoint-disabled.webkit-execution-line .webkit-line-number { color: transparent; background-image: -webkit-canvas(breakpoint-disabled-program-counter); }\n";

        styleText += ".webkit-breakpoint.webkit-breakpoint-conditional .webkit-line-number { color: white; background-image: -webkit-canvas(breakpoint-conditional); }\n";
        styleText += ".webkit-breakpoint-disabled.webkit-breakpoint-conditional .webkit-line-number { color: white; background-image: -webkit-canvas(breakpoint-disabled-conditional); }\n";
        styleText += ".webkit-breakpoint.webkit-breakpoint-conditional.webkit-execution-line .webkit-line-number { color: transparent; background-image: -webkit-canvas(breakpoint-conditional-program-counter); }\n";
        styleText += ".webkit-breakpoint-disabled.webkit-breakpoint-conditional.webkit-execution-line .webkit-line-number { color: transparent; background-image: -webkit-canvas(breakpoint-disabled-conditional-program-counter); }\n";

        styleText += ".webkit-execution-line .webkit-line-content { background-color: rgb(171, 191, 254); outline: 1px solid rgb(64, 115, 244); }\n";
        styleText += ".webkit-height-sized-to-fit { overflow-y: hidden }\n";
        styleText += ".webkit-line-content { background-color: white; }\n";
        styleText += "@-webkit-keyframes fadeout {from {background-color: rgb(255, 255, 120);} to { background-color: white;}}\n";
        styleText += ".webkit-highlighted-line .webkit-line-content { background-color: rgb(255, 255, 120); -webkit-animation: 'fadeout' 2s 500ms}\n";
        styleText += ".webkit-javascript-comment { color: rgb(0, 116, 0); }\n";
        styleText += ".webkit-javascript-keyword { color: rgb(170, 13, 145); }\n";
        styleText += ".webkit-javascript-number { color: rgb(28, 0, 207); }\n";
        styleText += ".webkit-javascript-string, .webkit-javascript-regexp { color: rgb(196, 26, 22); }\n";

        styleText += ".webkit-css-comment { color: rgb(0, 116, 0); }\n";
        styleText += ".webkit-css-string, .webkit-css-keyword, .webkit-css-unit { color: rgb(7, 144, 154); }\n";
        styleText += ".webkit-css-number { color: rgb(50, 0, 255); }\n";
        styleText += ".webkit-css-property, .webkit-css-at-rule { color: rgb(200, 0, 0); }\n";
        styleText += ".webkit-css-url { color: rgb(0, 0, 0); }\n";
        styleText += ".webkit-css-selector { color: rgb(0, 0, 0); }\n";
        styleText += ".webkit-css-pseudo-class { color: rgb(128, 128, 128); }\n";

        // TODO: Move these styles into inspector.css once https://bugs.webkit.org/show_bug.cgi?id=28913 is fixed and popup moved into the top frame.
        styleText += ".popup-content { position: absolute; z-index: 10000; padding: 4px; background-color: rgb(203, 226, 255); -webkit-border-radius: 7px; border: 2px solid rgb(169, 172, 203); }";
        styleText += ".popup-glasspane { position: absolute; top: 0; left: 0; height: 100%; width: 100%; opacity: 0; z-index: 9900; }";
        styleText += ".popup-message { background-color: transparent; font-family: Lucida Grande, sans-serif; font-weight: normal; font-size: 11px; text-align: left; text-shadow: none; color: rgb(85, 85, 85); cursor: default; margin: 0 0 2px 0; }";
        styleText += ".popup-content.breakpoint-condition { width: 90%; }";
        styleText += ".popup-content input#bp-condition { font-family: monospace; margin: 0; border: 1px inset rgb(190, 190, 190) !important; width: 100%; box-shadow: none !important; outline: none !important; -webkit-user-modify: read-write; }";
        // This class is already in inspector.css
        styleText += ".hidden { display: none !important; }";

        styleElement.textContent = styleText;

        this._needsProgramCounterImage = true;
        this._needsBreakpointImages = true;

        this.element.contentWindow.Element.prototype.addStyleClass = Element.prototype.addStyleClass;
        this.element.contentWindow.Element.prototype.removeStyleClass = Element.prototype.removeStyleClass;
        this.element.contentWindow.Element.prototype.positionAt = Element.prototype.positionAt;
        this.element.contentWindow.Element.prototype.removeMatchingStyleClasses = Element.prototype.removeMatchingStyleClasses;
        this.element.contentWindow.Element.prototype.hasStyleClass = Element.prototype.hasStyleClass;
        this.element.contentWindow.Element.prototype.pageOffsetRelativeToWindow = Element.prototype.pageOffsetRelativeToWindow;
        this.element.contentWindow.Element.prototype.__defineGetter__("totalOffsetLeft", Element.prototype.__lookupGetter__("totalOffsetLeft"));
        this.element.contentWindow.Element.prototype.__defineGetter__("totalOffsetTop", Element.prototype.__lookupGetter__("totalOffsetTop"));
        this.element.contentWindow.Node.prototype.enclosingNodeOrSelfWithNodeName = Node.prototype.enclosingNodeOrSelfWithNodeName;
        this.element.contentWindow.Node.prototype.enclosingNodeOrSelfWithNodeNameInArray = Node.prototype.enclosingNodeOrSelfWithNodeNameInArray;

        this._addExistingMessagesToSource();
        this._addExistingBreakpointsToSource();
        this._updateExecutionLine();
        if (this._executionLine)
            this.revealLine(this._executionLine);

        if (this.autoSizesToFitContentHeight)
            this.sizeToFitContentHeight();

        if (this._lineNumberToReveal) {
            this.revealLine(this._lineNumberToReveal);
            delete this._lineNumberToReveal;
        }

        if (this._lineNumberToHighlight) {
            this.highlightLine(this._lineNumberToHighlight);
            delete this._lineNumberToHighlight;
        }

        this.dispatchEventToListeners("content loaded");
    },

    _isContentLoaded: function() {
        var doc = this.element.contentDocument;
        return doc && doc.getElementsByTagName("table")[0];
    },

    _windowResized: function(event)
    {
        if (!this._autoSizesToFitContentHeight)
            return;
        this.sizeToFitContentHeight();
    },

    _documentContextMenu: function(event)
    {
        if (!event.target.hasStyleClass("webkit-line-number"))
            return;
        var sourceRow = event.target.enclosingNodeOrSelfWithNodeName("tr");
        if (!sourceRow._breakpointObject && this.addBreakpointDelegate)
            this.addBreakpointDelegate(this.lineNumberForSourceRow(sourceRow));

        var breakpoint = sourceRow._breakpointObject;
        if (!breakpoint)
            return;

        this._editBreakpointCondition(event.target, sourceRow, breakpoint);
        event.preventDefault();
    },

    _documentMouseDown: function(event)
    {
        if (!event.target.hasStyleClass("webkit-line-number"))
            return;
        if (event.button != 0 || event.altKey || event.ctrlKey || event.metaKey || event.shiftKey)
            return;
        var sourceRow = event.target.enclosingNodeOrSelfWithNodeName("tr");
        if (sourceRow._breakpointObject && sourceRow._breakpointObject.enabled)
            sourceRow._breakpointObject.enabled = false;
        else if (sourceRow._breakpointObject)
            WebInspector.panels.scripts.removeBreakpoint(sourceRow._breakpointObject);
        else if (this.addBreakpointDelegate)
            this.addBreakpointDelegate(this.lineNumberForSourceRow(sourceRow));
    },

    _editBreakpointCondition: function(eventTarget, sourceRow, breakpoint)
    {
        // TODO: Migrate the popup to the top-level document and remove the blur listener from conditionElement once https://bugs.webkit.org/show_bug.cgi?id=28913 is fixed.
        var popupDocument = this.element.contentDocument;
        this._showBreakpointConditionPopup(eventTarget, breakpoint.line, popupDocument);

        function committed(element, newText)
        {
            breakpoint.condition = newText;
            if (breakpoint.condition)
                sourceRow.addStyleClass("webkit-breakpoint-conditional");
            else
                sourceRow.removeStyleClass("webkit-breakpoint-conditional");
            dismissed.call(this);
        }

        function dismissed()
        {
            this._popup.hide();
            delete this._conditionEditorElement;
        }

        var dismissedHandler = dismissed.bind(this);
        this._conditionEditorElement.addEventListener("blur", dismissedHandler, false);

        WebInspector.startEditing(this._conditionEditorElement, committed.bind(this), dismissedHandler);
        this._conditionEditorElement.value = breakpoint.condition;
        this._conditionEditorElement.select();
    },

    _showBreakpointConditionPopup: function(clickedElement, lineNumber, popupDocument)
    {
        var popupContentElement = this._createPopupElement(lineNumber, popupDocument);
        var lineElement = clickedElement.enclosingNodeOrSelfWithNodeName("td").nextSibling;
        if (this._popup) {
            this._popup.hide();
            this._popup.element = popupContentElement;
        } else {
            this._popup = new WebInspector.Popup(popupContentElement);
            this._popup.autoHide = true;
        }
        this._popup.anchor = lineElement;
        this._popup.show();
    },

    _createPopupElement: function(lineNumber, popupDocument)
    {
        var popupContentElement = popupDocument.createElement("div");
        popupContentElement.className = "popup-content breakpoint-condition";

        var labelElement = document.createElement("label");
        labelElement.className = "popup-message";
        labelElement.htmlFor = "bp-condition";
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("The breakpoint on line %d will stop only if this expression is true:", lineNumber)));
        popupContentElement.appendChild(labelElement);

        var editorElement = document.createElement("input");
        editorElement.id = "bp-condition";
        editorElement.type = "text"
        popupContentElement.appendChild(editorElement);
        this._conditionEditorElement = editorElement;

        return popupContentElement;
    },

    _documentKeyDown: function(event)
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
        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, function(result, exception) {
            WebInspector.showConsole();
            var commandMessage = new WebInspector.ConsoleCommand(expression);
            WebInspector.console.addMessage(commandMessage);
            WebInspector.console.addMessage(new WebInspector.ConsoleCommandResult(result, exception, commandMessage));
        });
    },

    _breakpointEnableChanged: function(event)
    {
        var breakpoint = event.target;
        var sourceRow = this.sourceRow(breakpoint.line);
        if (!sourceRow)
            return;

        sourceRow.addStyleClass("webkit-breakpoint");

        if (breakpoint.enabled)
            sourceRow.removeStyleClass("webkit-breakpoint-disabled");
        else
            sourceRow.addStyleClass("webkit-breakpoint-disabled");
    },

    _updateExecutionLine: function(previousLine)
    {
        if (previousLine) {
            var sourceRow = this.sourceRow(previousLine);
            if (sourceRow)
                sourceRow.removeStyleClass("webkit-execution-line");
        }

        if (!this._executionLine)
            return;

        this._drawProgramCounterImageIfNeeded();

        var sourceRow = this.sourceRow(this._executionLine);
        if (sourceRow)
            sourceRow.addStyleClass("webkit-execution-line");
    },

    _addExistingBreakpointsToSource: function()
    {
        var length = this.breakpoints.length;
        for (var i = 0; i < length; ++i)
            this._addBreakpointToSource(this.breakpoints[i]);
    },

    _addBreakpointToSource: function(breakpoint)
    {
        var sourceRow = this.sourceRow(breakpoint.line);
        if (!sourceRow)
            return;

        breakpoint.sourceText = sourceRow.getElementsByClassName('webkit-line-content')[0].textContent;

        this._drawBreakpointImagesIfNeeded();

        sourceRow._breakpointObject = breakpoint;

        sourceRow.addStyleClass("webkit-breakpoint");
        if (!breakpoint.enabled)
            sourceRow.addStyleClass("webkit-breakpoint-disabled");
        if (breakpoint.condition)
            sourceRow.addStyleClass("webkit-breakpoint-conditional");
    },

    _removeBreakpointFromSource: function(breakpoint)
    {
        var sourceRow = this.sourceRow(breakpoint.line);
        if (!sourceRow)
            return;

        delete sourceRow._breakpointObject;

        sourceRow.removeStyleClass("webkit-breakpoint");
        sourceRow.removeStyleClass("webkit-breakpoint-disabled");
        sourceRow.removeStyleClass("webkit-breakpoint-conditional");
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
        var length = this.messages.length;
        for (var i = 0; i < length; ++i)
            this._addMessageToSource(this.messages[i]);
    },

    _addMessageToSource: function(msg)
    {
        var row = this.sourceRow(msg.line);
        if (!row)
            return;

        var cell = row.cells[1];
        if (!cell)
            return;

        var messageBubbleElement = cell.lastChild;
        if (!messageBubbleElement || messageBubbleElement.nodeType !== Node.ELEMENT_NODE || !messageBubbleElement.hasStyleClass("webkit-html-message-bubble")) {
            messageBubbleElement = this.element.contentDocument.createElement("div");
            messageBubbleElement.className = "webkit-html-message-bubble";
            cell.appendChild(messageBubbleElement);
        }

        if (!row.messages)
            row.messages = [];

        for (var i = 0; i < row.messages.length; ++i) {
            if (row.messages[i].isEqual(msg, true)) {
                this._incrementMessageRepeatCount(row.messages[i], msg.repeatDelta);
                return;
            }
        }

        row.messages.push(msg);

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

        var messageLineElement = this.element.contentDocument.createElement("div");
        messageLineElement.className = "webkit-html-message-line";
        messageBubbleElement.appendChild(messageLineElement);

        // Create the image element in the Inspector's document so we can use relative image URLs.
        var image = document.createElement("img");
        image.src = imageURL;
        image.className = "webkit-html-message-icon";

        // Adopt the image element since it wasn't created in element's contentDocument.
        image = this.element.contentDocument.adoptNode(image);
        messageLineElement.appendChild(image);
        messageLineElement.appendChild(this.element.contentDocument.createTextNode(msg.message));

        msg._resourceMessageLineElement = messageLineElement;
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
        if (!this._needsProgramCounterImage || !this.element.contentDocument)
            return;

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        this._drawProgramCounterInContext(ctx, true);

        delete this._needsProgramCounterImage;
    },

    _drawBreakpointImagesIfNeeded: function(conditional)
    {
        if (!this._needsBreakpointImages || !this.element.contentDocument)
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

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-disabled", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-disabled-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);


        // Conditional breakpoints.

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-conditional", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, false, true);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-conditional-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, false, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-disabled-conditional", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true, true);

        var ctx = this.element.contentDocument.getCSSCanvasContext("2d", "breakpoint-disabled-conditional-program-counter", 26, 11);
        ctx.clearRect(0, 0, 26, 11);
        drawBreakpoint(ctx, true, true);
        ctx.clearRect(20, 0, 6, 11);
        this._drawProgramCounterInContext(ctx, true);

        delete this._needsBreakpointImages;
    },

    syntaxHighlightJavascript: function()
    {
        var table = this.element.contentDocument.getElementsByTagName("table")[0];
        if (!table)
            return;

        var jsSyntaxHighlighter = new WebInspector.JavaScriptSourceSyntaxHighlighter(table, this);
        jsSyntaxHighlighter.process();
    },

    syntaxHighlightCSS: function()
    {
        var table = this.element.contentDocument.getElementsByTagName("table")[0];
        if (!table)
            return;

        var cssSyntaxHighlighter = new WebInspector.CSSSourceSyntaxHighligher(table, this);
        cssSyntaxHighlighter.process();
    }
}

WebInspector.SourceFrame.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.SourceSyntaxHighligher = function(table, sourceFrame)
{
    this.table = table;
    this.sourceFrame = sourceFrame;
}

WebInspector.SourceSyntaxHighligher.prototype = {
    createSpan: function(content, className)
    {
        var span = document.createElement("span");
        span.className = className;
        span.appendChild(document.createTextNode(content));
        return span;
    },

    generateFinder: function(regex, matchNumber, className)
    {
        return function(str) {
            var match = regex.exec(str);
            if (!match)
                return null;
            this.previousMatchLength = match[matchNumber].length;
            return this.createSpan(match[matchNumber], className);
        };
    },

    process: function()
    {
        // Split up the work into chunks so we don't block the
        // UI thread while processing.

        var i = 0;
        var rows = this.table.rows;
        var rowsLength = rows.length;
        var previousCell = null;
        const linesPerChunk = 10;

        function processChunk()
        {
            for (var end = Math.min(i + linesPerChunk, rowsLength); i < end; ++i) {
                var row = rows[i];
                if (!row)
                    continue;
                var cell = row.cells[1];
                if (!cell)
                    continue;
                this.syntaxHighlightLine(cell, previousCell);
                if (i < (end - 1))
                    this.deleteContinueFlags(previousCell);
                previousCell = cell;
            }

            if (i >= rowsLength && processChunkInterval) {
                this.deleteContinueFlags(previousCell);
                delete this.previousMatchLength;
                clearInterval(processChunkInterval);

                this.sourceFrame.dispatchEventToListeners("syntax highlighting complete");
            }
        }

        var boundProcessChunk = processChunk.bind(this);
        var processChunkInterval = setInterval(boundProcessChunk, 25);
        boundProcessChunk();
    }
}

WebInspector.CSSSourceSyntaxHighligher = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighligher.call(this, table, sourceFrame);

    this.findNumber = this.generateFinder(/^((-?(\d+|\d*\.\d+))|^(#[a-fA-F0-9]{3,6}))(?:\D|$)/, 1, "webkit-css-number");
    this.findUnits = this.generateFinder(/^(px|em|pt|in|cm|mm|pc|ex)(?:\W|$)/, 1, "webkit-css-unit");
    this.findKeyword = this.generateFinder(/^(rgba?|hsla?|var)(?:\W|$)/, 1, "webkit-css-keyword");
    this.findSingleLineString = this.generateFinder(/^"(?:[^"\\]|\\.)*"|^'([^'\\]|\\.)*'/, 0, "webkit-css-string"); // " this quote keeps Xcode happy
    this.findSingleLineComment = this.generateFinder(/^\/\*.*?\*\//, 0, "webkit-css-comment");
    this.findMultilineCommentStart = this.generateFinder(/^\/\*.*$/, 0, "webkit-css-comment");
    this.findMultilineCommentEnd = this.generateFinder(/^.*?\*\//, 0, "webkit-css-comment");
    this.findSelector = this.generateFinder(/^([#\.]?[_a-zA-Z].*?)(?:\W|$)/, 1, "webkit-css-selector");
    this.findProperty = this.generateFinder(/^(-?[_a-z0-9][_a-z0-9-]*\s*)(?:\:)/, 1, "webkit-css-property");
    this.findGenericIdent = this.generateFinder(/^([@-]?[_a-z0-9][_a-z0-9-]*)(?:\W|$)/, 1, "webkit-css-string");
}

WebInspector.CSSSourceSyntaxHighligher.prototype = {
    deleteContinueFlags: function(cell)
    {
        if (!cell)
            return;
        delete cell._commentContinues;
        delete cell._inSelector;
    },

    findPseudoClass: function(str)
    {
        var match = /^(::?)([_a-z0-9][_a-z0-9-]*)/.exec(str);
        if (!match)
            return null;
        this.previousMatchLength = match[0].length;
        var span = document.createElement("span");
        span.appendChild(document.createTextNode(match[1]));
        span.appendChild(this.createSpan(match[2], "webkit-css-pseudo-class"));
        return span;
    },

    findURL: function(str)
    {
        var match = /^(?:local|url)\(([^\)]*?)\)/.exec(str);
        if (!match)
            return null;
        this.previousMatchLength = match[0].length;
        var innerUrlSpan = this.createSpan(match[1], "webkit-css-url");
        var outerSpan = document.createElement("span");
        outerSpan.appendChild(this.createSpan("url", "webkit-css-keyword"));
        outerSpan.appendChild(document.createTextNode("("));
        outerSpan.appendChild(innerUrlSpan);
        outerSpan.appendChild(document.createTextNode(")"));
        return outerSpan;
    },

    findAtRule: function(str)
    {
        var match = /^@[_a-z0-9][_a-z0-9-]*(?:\W|$)/.exec(str);
        if (!match)
            return null;
        this.previousMatchLength = match[0].length;
        return this.createSpan(match[0], "webkit-css-at-rule");
    },

    syntaxHighlightLine: function(line, prevLine)
    {
        var code = line.textContent;
        while (line.firstChild)
            line.removeChild(line.firstChild);

        var token;
        var tmp = 0;
        var i = 0;
        this.previousMatchLength = 0;

        if (prevLine) {
            if (prevLine._commentContinues) {
                if (!(token = this.findMultilineCommentEnd(code))) {
                    token = this.createSpan(code, "webkit-javascript-comment");
                    line._commentContinues = true;
                }
            }
            if (token) {
                i += this.previousMatchLength ? this.previousMatchLength : code.length;
                tmp = i;
                line.appendChild(token);
            }
        }

        var inSelector = (prevLine && prevLine._inSelector); // inside a selector, we can now parse properties and values
        var inAtRuleBlock = (prevLine && prevLine._inAtRuleBlock); // inside an @rule block, but not necessarily inside a selector yet
        var atRuleStarted = (prevLine && prevLine._atRuleStarted); // we received an @rule, we may stop the @rule at a semicolon or open a block and become inAtRuleBlock
        var atRuleIsSelector = (prevLine && prevLine._atRuleIsSelector); // when this @rule opens a block it immediately goes into parsing properties and values instead of selectors

        for ( ; i < code.length; ++i) {
            var codeFragment = code.substr(i);
            var prevChar = code[i - 1];
            var currChar = codeFragment[0];
            token = this.findSingleLineComment(codeFragment);
            if (!token) {
                if ((token = this.findMultilineCommentStart(codeFragment)))
                    line._commentContinues = true;
                else if (currChar === ";" && !inAtRuleBlock)
                    atRuleStarted = false;
                else if (currChar === "}") {
                    if (inSelector && inAtRuleBlock && atRuleIsSelector) {
                        inSelector = false;
                        inAtRuleBlock = false;
                        atRuleStarted = false;
                    } else if (inSelector) {
                        inSelector = false;
                    } else if (inAtRuleBlock) {
                        inAtRuleBlock = false;
                        atRuleStarted = false;
                    }
                } else if (currChar === "{") {
                    if (!atRuleStarted || inAtRuleBlock) {
                        inSelector = true;
                    } else if (!inAtRuleBlock && atRuleIsSelector) {
                        inAtRuleBlock = true;
                        inSelector = true;
                    } else if (!inAtRuleBlock) {
                        inAtRuleBlock = true;
                        inSelector = false;
                    }
                } else if (inSelector) {
                    if (!prevChar || /^\d/.test(prevChar)) {
                        token = this.findUnits(codeFragment);
                    } else if (!prevChar || /^\W/.test(prevChar)) {
                        token = this.findNumber(codeFragment) ||
                                this.findKeyword(codeFragment) ||
                                this.findURL(codeFragment) ||
                                this.findProperty(codeFragment) ||
                                this.findAtRule(codeFragment) ||
                                this.findGenericIdent(codeFragment) ||
                                this.findSingleLineString(codeFragment);
                    }
                } else if (!inSelector) {
                    if (atRuleStarted && !inAtRuleBlock)
                        token = this.findURL(codeFragment); // for @import
                    if (!token) {
                        token = this.findSelector(codeFragment) ||
                                this.findPseudoClass(codeFragment) ||
                                this.findAtRule(codeFragment);
                    }
                }
            }

            if (token) {
                if (currChar === "@") {
                    atRuleStarted = true;

                    // The @font-face, @page, and @variables at-rules do not contain selectors like other at-rules
                    // instead it acts as a selector and contains properties and values.
                    var text = token.textContent;
                    atRuleIsSelector = /font-face/.test(text) || /page/.test(text) || /variables/.test(text);
                }

                if (tmp !== i)
                    line.appendChild(document.createTextNode(code.substring(tmp, i)));
                line.appendChild(token);
                i += this.previousMatchLength - 1;
                tmp = i + 1;
            }
        }

        line._inSelector = inSelector;
        line._inAtRuleBlock = inAtRuleBlock;
        line._atRuleStarted = atRuleStarted;
        line._atRuleIsSelector = atRuleIsSelector;

        if (tmp < code.length)
            line.appendChild(document.createTextNode(code.substring(tmp, i)));
    }
}

WebInspector.CSSSourceSyntaxHighligher.prototype.__proto__ = WebInspector.SourceSyntaxHighligher.prototype;

WebInspector.JavaScriptSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighligher.call(this, table, sourceFrame);

    this.findNumber = this.generateFinder(/^(-?(\d+\.?\d*([eE][+-]\d+)?|0[xX]\h+|Infinity)|NaN)(?:\W|$)/, 1, "webkit-javascript-number");
    this.findKeyword = this.generateFinder(/^(null|true|false|break|case|catch|const|default|finally|for|instanceof|new|var|continue|function|return|void|delete|if|this|do|while|else|in|switch|throw|try|typeof|with|debugger|class|enum|export|extends|import|super|get|set)(?:\W|$)/, 1, "webkit-javascript-keyword");
    this.findSingleLineString = this.generateFinder(/^"(?:[^"\\]|\\.)*"|^'([^'\\]|\\.)*'/, 0, "webkit-javascript-string"); // " this quote keeps Xcode happy
    this.findMultilineCommentStart = this.generateFinder(/^\/\*.*$/, 0, "webkit-javascript-comment");
    this.findMultilineCommentEnd = this.generateFinder(/^.*?\*\//, 0, "webkit-javascript-comment");
    this.findMultilineSingleQuoteStringStart = this.generateFinder(/^'(?:[^'\\]|\\.)*\\$/, 0, "webkit-javascript-string");
    this.findMultilineSingleQuoteStringEnd = this.generateFinder(/^(?:[^'\\]|\\.)*?'/, 0, "webkit-javascript-string");
    this.findMultilineDoubleQuoteStringStart = this.generateFinder(/^"(?:[^"\\]|\\.)*\\$/, 0, "webkit-javascript-string");
    this.findMultilineDoubleQuoteStringEnd = this.generateFinder(/^(?:[^"\\]|\\.)*?"/, 0, "webkit-javascript-string");
    this.findMultilineRegExpEnd = this.generateFinder(/^(?:[^\/\\]|\\.)*?\/([gim]{0,3})/, 0, "webkit-javascript-regexp");
    this.findSingleLineComment = this.generateFinder(/^\/\/.*|^\/\*.*?\*\//, 0, "webkit-javascript-comment");
}

WebInspector.JavaScriptSourceSyntaxHighlighter.prototype = {
    deleteContinueFlags: function(cell)
    {
        if (!cell)
            return;
        delete cell._commentContinues;
        delete cell._singleQuoteStringContinues;
        delete cell._doubleQuoteStringContinues;
        delete cell._regexpContinues;
    },

    findMultilineRegExpStart: function(str)
    {
        var match = /^\/(?:[^\/\\]|\\.)*\\$/.exec(str);
        if (!match || !/\\|\$|\.[\?\*\+]|[^\|]\|[^\|]/.test(match[0]))
            return null;
        this.previousMatchLength = match[0].length;
        return this.createSpan(match[0], "webkit-javascript-regexp");
    },

    findSingleLineRegExp: function(str)
    {
        var match = /^(\/(?:[^\/\\]|\\.)*\/([gim]{0,3}))(.?)/.exec(str);
        if (!match || !(match[2].length > 0 || /\\|\$|\.[\?\*\+]|[^\|]\|[^\|]/.test(match[1]) || /\.|;|,/.test(match[3])))
            return null;
        this.previousMatchLength = match[1].length;
        return this.createSpan(match[1], "webkit-javascript-regexp");
    },

    syntaxHighlightLine: function(line, prevLine)
    {
        var messageBubble = line.lastChild;
        if (messageBubble && messageBubble.nodeType === Node.ELEMENT_NODE && messageBubble.hasStyleClass("webkit-html-message-bubble"))
            line.removeChild(messageBubble);
        else
            messageBubble = null;

        var code = line.textContent;

        while (line.firstChild)
            line.removeChild(line.firstChild);

        var token;
        var tmp = 0;
        var i = 0;
        this.previousMatchLength = 0;

        if (prevLine) {
            if (prevLine._commentContinues) {
                if (!(token = this.findMultilineCommentEnd(code))) {
                    token = this.createSpan(code, "webkit-javascript-comment");
                    line._commentContinues = true;
                }
            } else if (prevLine._singleQuoteStringContinues) {
                if (!(token = this.findMultilineSingleQuoteStringEnd(code))) {
                    token = this.createSpan(code, "webkit-javascript-string");
                    line._singleQuoteStringContinues = true;
                }
            } else if (prevLine._doubleQuoteStringContinues) {
                if (!(token = this.findMultilineDoubleQuoteStringEnd(code))) {
                    token = this.createSpan(code, "webkit-javascript-string");
                    line._doubleQuoteStringContinues = true;
                }
            } else if (prevLine._regexpContinues) {
                if (!(token = this.findMultilineRegExpEnd(code))) {
                    token = this.createSpan(code, "webkit-javascript-regexp");
                    line._regexpContinues = true;
                }
            }
            if (token) {
                i += this.previousMatchLength ? this.previousMatchLength : code.length;
                tmp = i;
                line.appendChild(token);
            }
        }

        for ( ; i < code.length; ++i) {
            var codeFragment = code.substr(i);
            var prevChar = code[i - 1];
            token = this.findSingleLineComment(codeFragment);
            if (!token) {
                if ((token = this.findMultilineCommentStart(codeFragment)))
                    line._commentContinues = true;
                else if (!prevChar || /^\W/.test(prevChar)) {
                    token = this.findNumber(codeFragment) ||
                            this.findKeyword(codeFragment) ||
                            this.findSingleLineString(codeFragment) ||
                            this.findSingleLineRegExp(codeFragment);
                    if (!token) {
                        if (token = this.findMultilineSingleQuoteStringStart(codeFragment))
                            line._singleQuoteStringContinues = true;
                        else if (token = this.findMultilineDoubleQuoteStringStart(codeFragment))
                            line._doubleQuoteStringContinues = true;
                        else if (token = this.findMultilineRegExpStart(codeFragment))
                            line._regexpContinues = true;
                    }
                }
            }

            if (token) {
                if (tmp !== i)
                    line.appendChild(document.createTextNode(code.substring(tmp, i)));
                line.appendChild(token);
                i += this.previousMatchLength - 1;
                tmp = i + 1;
            }
        }

        if (tmp < code.length)
            line.appendChild(document.createTextNode(code.substring(tmp, i)));

        if (messageBubble)
            line.appendChild(messageBubble);
    }
}

WebInspector.JavaScriptSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighligher.prototype;

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
        
        var linkElement = this.element.contentDocument.createElement("link");
        linkElement.type = "text/css";
        linkElement.rel = "stylesheet";
        linkElement.href = "inspectorSyntaxHighlight.css";
        headElement.appendChild(linkElement);

        var styleElement = this.element.contentDocument.createElement("style");
        headElement.appendChild(styleElement);

        // Add these style rules here since they are specific to the Inspector. They also behave oddly and not
        // all properties apply if added to view-source.css (because it is a user agent sheet.)
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
        this.element.contentWindow.Element.prototype.removeChildren = Element.prototype.removeChildren;
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

        event.preventDefault();
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
        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, "console", function(result, exception) {
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

        var cssSyntaxHighlighter = new WebInspector.CSSSourceSyntaxHighlighter(table, this);
        cssSyntaxHighlighter.process();
    }
}

WebInspector.SourceFrame.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.SourceSyntaxHighlighter = function(table, sourceFrame)
{
    this.table = table;
    this.sourceFrame = sourceFrame;
}

WebInspector.SourceSyntaxHighlighter.prototype = {
    createSpan: function(content, className)
    {
        var span = document.createElement("span");
        span.className = className;
        span.appendChild(document.createTextNode(content));
        return span;
    },

    process: function()
    {
        // Split up the work into chunks so we don't block the
        // UI thread while processing.

        var rows = this.table.rows;
        var rowsLength = rows.length;
        const tokensPerChunk = 100;
        const lineLengthLimit = 20000;
        
        var boundProcessChunk = processChunk.bind(this);
        var processChunkInterval = setInterval(boundProcessChunk, 25);
        boundProcessChunk();
        
        function processChunk()
        {
            for (var i = 0; i < tokensPerChunk; i++) {
                if (this.cursor >= this.lineCode.length)
                    moveToNextLine.call(this);
                if (this.lineIndex >= rowsLength) {
                    this.sourceFrame.dispatchEventToListeners("syntax highlighting complete");
                    return;
                }
                if (this.cursor > lineLengthLimit) {
                    var codeFragment = this.lineCode.substring(this.cursor);
                    this.nonToken += codeFragment;
                    this.cursor += codeFragment.length;
                }

                this.lex();
            }
        }
        
        function moveToNextLine()
        {
            this.appendNonToken();
            
            var row = rows[this.lineIndex];
            var line = row ? row.cells[1] : null;
            if (line && this.newLine) {
                line.removeChildren();
                
                if (this.messageBubble)
                    this.newLine.appendChild(this.messageBubble);
                
                line.parentNode.insertBefore(this.newLine, line);
                line.parentNode.removeChild(line);
                
                this.newLine = null;
            }
            this.lineIndex++;
            if (this.lineIndex >= rowsLength && processChunkInterval) {
                clearInterval(processChunkInterval);
                this.sourceFrame.dispatchEventToListeners("syntax highlighting complete");
                return;
            }
            row = rows[this.lineIndex];
            line = row ? row.cells[1] : null;
            
            this.messageBubble = null;
            if (line.lastChild && line.lastChild.nodeType === Node.ELEMENT_NODE && line.lastChild.hasStyleClass("webkit-html-message-bubble")) {
                this.messageBubble = line.lastChild;
                line.removeChild(this.messageBubble);
            }

            this.lineCode = line.textContent;
            this.newLine = line.cloneNode(false);
            this.cursor = 0;
            if (!line)
                moveToNextLine();
        }
    },
    
    lex: function()
    {
        var token = null;
        var codeFragment = this.lineCode.substring(this.cursor);
        
        for (var i = 0; i < this.rules.length; i++) {
            var rule = this.rules[i];
            var ruleContinueStateCondition = typeof rule.continueStateCondition === "undefined" ? this.ContinueState.None : rule.continueStateCondition;
            if (this.continueState === ruleContinueStateCondition) {
                if (typeof rule.lexStateCondition !== "undefined" && this.lexState !== rule.lexStateCondition)
                    continue;
                var match = rule.pattern.exec(codeFragment);
                if (match) {
                    token = match[0];
                    if (token) {
                        if (!rule.dontAppendNonToken)
                            this.appendNonToken();
                        return rule.action.call(this, token);
                    }
                }
            }
        }
        this.nonToken += codeFragment[0];
        this.cursor++;
    },
    
    appendNonToken: function ()
    {
        if (this.nonToken.length > 0) {
            this.newLine.appendChild(document.createTextNode(this.nonToken));
            this.nonToken = "";
        }
    },
    
    syntaxHighlightNode: function(node)
    {
        this.lineCode = node.textContent;
        node.removeChildren();
        this.newLine = node;
        this.cursor = 0;
        while (true) {
            if (this.cursor >= this.lineCode.length) {
                var codeFragment = this.lineCode.substring(this.cursor);
                this.nonToken += codeFragment;
                this.cursor += codeFragment.length;
                this.appendNonToken();
                this.newLine = null;
                return;
            }

            this.lex();
        }
    }
}

WebInspector.CSSSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighlighter.call(this, table, sourceFrame);

    this.LexState = {
        Initial: 1,
        DeclarationProperty: 2,
        DeclarationValue: 3,
        AtMedia: 4,
        AtRule: 5,
        AtKeyframes: 6
    };
    this.ContinueState = {
        None: 0,
        Comment: 1
    };
    
    this.nonToken = "";
    this.cursor = 0;
    this.lineIndex = -1;
    this.lineCode = "";
    this.newLine = null;
    this.lexState = this.LexState.Initial;
    this.continueState = this.ContinueState.None;
    
    const urlPattern = /^url\(\s*(?:(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')|(?:[!#$%&*-~\w]|(?:\\[\da-f]{1,6}\s?|\.))*)\s*\)/i;
    const stringPattern = /^(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*')/i;
    const identPattern = /^-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i;
    const startBlockPattern = /^{/i;
    const endBlockPattern = /^}/i;
    this.rules = [{
        pattern: /^\/\*[^\*]*\*+([^\/*][^*]*\*+)*\//i,
        action: commentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/i,
        action: commentStartAction
    }, {
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/i,
        action: commentEndAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^.*/i,
        action: commentMiddleAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^(?:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\*)(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))*|(?:#-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\.-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|\[\s*-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*(?:(?:=|~=|\|=)\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|(?:"(?:[^\\\"]|(?:\\[\da-f]{1,6}\s?|\.))*"|'(?:[^\\\']|(?:\\[\da-f]{1,6}\s?|\.))*'))\s*)?\]|:(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*|-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\(\s*(?:-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*\s*)?\)))+)/i,
        action: selectorAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startRulesetBlockAction,
        lexStateCondition: this.LexState.Initial,
        dontAppendNonToken: true
    }, {
        pattern: identPattern,
        action: propertyAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: /^:/i,
        action: declarationColonAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:#(?:[\da-f]{6}|[\da-f]{3})|rgba\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsla\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|rgb\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\)|hsl\(\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*,\s*(?:\d+|\d*\.\d+)%?\s*\))/i,
        action: colorAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: /^(?:-?(?:\d+|\d*\.\d+)(?:em|rem|__qem|ex|px|cm|mm|in|pt|pc|deg|rad|grad|turn|ms|s|Hz|kHz|%)?)/i,
        action: numvalueAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: urlPattern,
        action: urlAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: /^!\s*important/i,
        action: importantAction,
        lexStateCondition: this.LexState.DeclarationValue
    }, {
        pattern: identPattern,
        action: valueIdentAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: /^;/i,
        action: declarationSemicolonAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: endBlockPattern,
        action: endRulesetBlockAction,
        lexStateCondition: this.LexState.DeclarationProperty,
        dontAppendNonToken: true
    }, {
        pattern: endBlockPattern,
        action: endRulesetBlockAction,
        lexStateCondition: this.LexState.DeclarationValue,
        dontAppendNonToken: true
    }, {
        pattern: /^@media/i,
        action: atMediaAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startAtMediaBlockAction,
        lexStateCondition: this.LexState.AtMedia,
        dontAppendNonToken: true
    }, {
        pattern: /^@-webkit-keyframes/i,
        action: atKeyframesAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: startBlockPattern,
        action: startAtMediaBlockAction,
        lexStateCondition: this.LexState.AtKeyframes,
        dontAppendNonToken: true
    }, {
        pattern: /^@-?(?:\w|(?:\\[\da-f]{1,6}\s?|\.))(?:[-\w]|(?:\\[\da-f]{1,6}\s?|\.))*/i,
        action: atRuleAction,
        lexStateCondition: this.LexState.Initial
    }, {
        pattern: /^;/i,
        action: endAtRuleAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: urlPattern,
        action: urlAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.AtRule
    }, {
        pattern: stringPattern,
        action: stringAction,
        lexStateCondition: this.LexState.AtKeyframes
    }, {
        pattern: identPattern,
        action: atRuleIdentAction,
        lexStateCondition: this.LexState.AtRule,
        dontAppendNonToken: true
    }, {
        pattern: identPattern,
        action: atRuleIdentAction,
        lexStateCondition: this.LexState.AtMedia,
        dontAppendNonToken: true
    }, {
        pattern: startBlockPattern,
        action: startAtRuleBlockAction,
        lexStateCondition: this.LexState.AtRule,
        dontAppendNonToken: true
    }];
    
    function commentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
    }
    
    function commentStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
        this.continueState = this.ContinueState.Comment;
    }
    
    function commentEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
        this.continueState = this.ContinueState.None;
    }

    function commentMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-comment"));
    }
    
    function selectorAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-selector"));
    }
    
    function startRulesetBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    function endRulesetBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    const propertyKeywords = {
        "background": true,
        "background-attachment": true,
        "background-clip": true,
        "background-color": true,
        "background-image": true,
        "background-origin": true,
        "background-position": true,
        "background-position-x": true,
        "background-position-y": true,
        "background-repeat": true,
        "background-repeat-x": true,
        "background-repeat-y": true,
        "background-size": true,
        "border": true,
        "border-bottom": true,
        "border-bottom-color": true,
        "border-bottom-left-radius": true,
        "border-bottom-right-radius": true,
        "border-bottom-style": true,
        "border-bottom-width": true,
        "border-collapse": true,
        "border-color": true,
        "border-left": true,
        "border-left-color": true,
        "border-left-style": true,
        "border-left-width": true,
        "border-radius": true,
        "border-right": true,
        "border-right-color": true,
        "border-right-style": true,
        "border-right-width": true,
        "border-spacing": true,
        "border-style": true,
        "border-top": true,
        "border-top-color": true,
        "border-top-left-radius": true,
        "border-top-right-radius": true,
        "border-top-style": true,
        "border-top-width": true,
        "border-width": true,
        "bottom": true,
        "caption-side": true,
        "clear": true,
        "clip": true,
        "color": true,
        "content": true,
        "counter-increment": true,
        "counter-reset": true,
        "cursor": true,
        "direction": true,
        "display": true,
        "empty-cells": true,
        "float": true,
        "font": true,
        "font-family": true,
        "font-size": true,
        "font-stretch": true,
        "font-style": true,
        "font-variant": true,
        "font-weight": true,
        "height": true,
        "left": true,
        "letter-spacing": true,
        "line-height": true,
        "list-style": true,
        "list-style-image": true,
        "list-style-position": true,
        "list-style-type": true,
        "margin": true,
        "margin-bottom": true,
        "margin-left": true,
        "margin-right": true,
        "margin-top": true,
        "max-height": true,
        "max-width": true,
        "min-height": true,
        "min-width": true,
        "opacity": true,
        "orphans": true,
        "outline": true,
        "outline-color": true,
        "outline-offset": true,
        "outline-style": true,
        "outline-width": true,
        "overflow": true,
        "overflow-x": true,
        "overflow-y": true,
        "padding": true,
        "padding-bottom": true,
        "padding-left": true,
        "padding-right": true,
        "padding-top": true,
        "page": true,
        "page-break-after": true,
        "page-break-before": true,
        "page-break-inside": true,
        "pointer-events": true,
        "position": true,
        "quotes": true,
        "resize": true,
        "right": true,
        "size": true,
        "src": true,
        "table-layout": true,
        "text-align": true,
        "text-decoration": true,
        "text-indent": true,
        "text-line-through": true,
        "text-line-through-color": true,
        "text-line-through-mode": true,
        "text-line-through-style": true,
        "text-line-through-width": true,
        "text-overflow": true,
        "text-overline": true,
        "text-overline-color": true,
        "text-overline-mode": true,
        "text-overline-style": true,
        "text-overline-width": true,
        "text-rendering": true,
        "text-shadow": true,
        "text-transform": true,
        "text-underline": true,
        "text-underline-color": true,
        "text-underline-mode": true,
        "text-underline-style": true,
        "text-underline-width": true,
        "top": true,
        "unicode-bidi": true,
        "unicode-range": true,
        "vertical-align": true,
        "visibility": true,
        "white-space": true,
        "widows": true,
        "width": true,
        "word-break": true,
        "word-spacing": true,
        "word-wrap": true,
        "z-index": true,
        "zoom": true,
        "-webkit-animation": true,
        "-webkit-animation-delay": true,
        "-webkit-animation-direction": true,
        "-webkit-animation-duration": true,
        "-webkit-animation-iteration-count": true,
        "-webkit-animation-name": true,
        "-webkit-animation-play-state": true,
        "-webkit-animation-timing-function": true,
        "-webkit-appearance": true,
        "-webkit-backface-visibility": true,
        "-webkit-background-clip": true,
        "-webkit-background-composite": true,
        "-webkit-background-origin": true,
        "-webkit-background-size": true,
        "-webkit-binding": true,
        "-webkit-border-fit": true,
        "-webkit-border-horizontal-spacing": true,
        "-webkit-border-image": true,
        "-webkit-border-radius": true,
        "-webkit-border-vertical-spacing": true,
        "-webkit-box-align": true,
        "-webkit-box-direction": true,
        "-webkit-box-flex": true,
        "-webkit-box-flex-group": true,
        "-webkit-box-lines": true,
        "-webkit-box-ordinal-group": true,
        "-webkit-box-orient": true,
        "-webkit-box-pack": true,
        "-webkit-box-reflect": true,
        "-webkit-box-shadow": true,
        "-webkit-box-sizing": true,
        "-webkit-column-break-after": true,
        "-webkit-column-break-before": true,
        "-webkit-column-break-inside": true,
        "-webkit-column-count": true,
        "-webkit-column-gap": true,
        "-webkit-column-rule": true,
        "-webkit-column-rule-color": true,
        "-webkit-column-rule-style": true,
        "-webkit-column-rule-width": true,
        "-webkit-column-width": true,
        "-webkit-columns": true,
        "-webkit-font-size-delta": true,
        "-webkit-font-smoothing": true,
        "-webkit-highlight": true,
        "-webkit-line-break": true,
        "-webkit-line-clamp": true,
        "-webkit-margin-bottom-collapse": true,
        "-webkit-margin-collapse": true,
        "-webkit-margin-start": true,
        "-webkit-margin-top-collapse": true,
        "-webkit-marquee": true,
        "-webkit-marquee-direction": true,
        "-webkit-marquee-increment": true,
        "-webkit-marquee-repetition": true,
        "-webkit-marquee-speed": true,
        "-webkit-marquee-style": true,
        "-webkit-mask": true,
        "-webkit-mask-attachment": true,
        "-webkit-mask-box-image": true,
        "-webkit-mask-clip": true,
        "-webkit-mask-composite": true,
        "-webkit-mask-image": true,
        "-webkit-mask-origin": true,
        "-webkit-mask-position": true,
        "-webkit-mask-position-x": true,
        "-webkit-mask-position-y": true,
        "-webkit-mask-repeat": true,
        "-webkit-mask-repeat-x": true,
        "-webkit-mask-repeat-y": true,
        "-webkit-mask-size": true,
        "-webkit-match-nearest-mail-blockquote-color": true,
        "-webkit-nbsp-mode": true,
        "-webkit-padding-start": true,
        "-webkit-perspective": true,
        "-webkit-perspective-origin": true,
        "-webkit-perspective-origin-x": true,
        "-webkit-perspective-origin-y": true,
        "-webkit-rtl-ordering": true,
        "-webkit-text-decorations-in-effect": true,
        "-webkit-text-fill-color": true,
        "-webkit-text-security": true,
        "-webkit-text-size-adjust": true,
        "-webkit-text-stroke": true,
        "-webkit-text-stroke-color": true,
        "-webkit-text-stroke-width": true,
        "-webkit-transform": true,
        "-webkit-transform-origin": true,
        "-webkit-transform-origin-x": true,
        "-webkit-transform-origin-y": true,
        "-webkit-transform-origin-z": true,
        "-webkit-transform-style": true,
        "-webkit-transition": true,
        "-webkit-transition-delay": true,
        "-webkit-transition-duration": true,
        "-webkit-transition-property": true,
        "-webkit-transition-timing-function": true,
        "-webkit-user-drag": true,
        "-webkit-user-modify": true,
        "-webkit-user-select": true,
        "-webkit-variable-declaration-block": true
    };
    function propertyAction(token)
    {
        this.cursor += token.length;
        if (token in propertyKeywords) {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-property"));
        } else
            this.nonToken += token;
    }
    
    function declarationColonAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationValue;
    }

    const valueKeywords = {
        "inherit": true,
        "initial": true,
        "none": true,
        "hidden": true,
        "inset": true,
        "groove": true,
        "ridge": true,
        "outset": true,
        "dotted": true,
        "dashed": true,
        "solid": true,
        "double": true,
        "caption": true,
        "icon": true,
        "menu": true,
        "message-box": true,
        "small-caption": true,
        "-webkit-mini-control": true,
        "-webkit-small-control": true,
        "-webkit-control": true,
        "status-bar": true,
        "italic": true,
        "oblique": true,
        "all": true,
        "small-caps": true,
        "normal": true,
        "bold": true,
        "bolder": true,
        "lighter": true,
        "xx-small": true,
        "x-small": true,
        "small": true,
        "medium": true,
        "large": true,
        "x-large": true,
        "xx-large": true,
        "-webkit-xxx-large": true,
        "smaller": true,
        "larger": true,
        "wider": true,
        "narrower": true,
        "ultra-condensed": true,
        "extra-condensed": true,
        "condensed": true,
        "semi-condensed": true,
        "semi-expanded": true,
        "expanded": true,
        "extra-expanded": true,
        "ultra-expanded": true,
        "serif": true,
        "sans-serif": true,
        "cursive": true,
        "fantasy": true,
        "monospace": true,
        "-webkit-body": true,
        "aqua": true,
        "black": true,
        "blue": true,
        "fuchsia": true,
        "gray": true,
        "green": true,
        "lime": true,
        "maroon": true,
        "navy": true,
        "olive": true,
        "orange": true,
        "purple": true,
        "red": true,
        "silver": true,
        "teal": true,
        "white": true,
        "yellow": true,
        "transparent": true,
        "-webkit-link": true,
        "-webkit-activelink": true,
        "activeborder": true,
        "activecaption": true,
        "appworkspace": true,
        "background": true,
        "buttonface": true,
        "buttonhighlight": true,
        "buttonshadow": true,
        "buttontext": true,
        "captiontext": true,
        "graytext": true,
        "highlight": true,
        "highlighttext": true,
        "inactiveborder": true,
        "inactivecaption": true,
        "inactivecaptiontext": true,
        "infobackground": true,
        "infotext": true,
        "match": true,
        "menutext": true,
        "scrollbar": true,
        "threeddarkshadow": true,
        "threedface": true,
        "threedhighlight": true,
        "threedlightshadow": true,
        "threedshadow": true,
        "window": true,
        "windowframe": true,
        "windowtext": true,
        "-webkit-focus-ring-color": true,
        "currentcolor": true,
        "grey": true,
        "-webkit-text": true,
        "repeat": true,
        "repeat-x": true,
        "repeat-y": true,
        "no-repeat": true,
        "clear": true,
        "copy": true,
        "source-over": true,
        "source-in": true,
        "source-out": true,
        "source-atop": true,
        "destination-over": true,
        "destination-in": true,
        "destination-out": true,
        "destination-atop": true,
        "xor": true,
        "plus-darker": true,
        "plus-lighter": true,
        "baseline": true,
        "middle": true,
        "sub": true,
        "super": true,
        "text-top": true,
        "text-bottom": true,
        "top": true,
        "bottom": true,
        "-webkit-baseline-middle": true,
        "-webkit-auto": true,
        "left": true,
        "right": true,
        "center": true,
        "justify": true,
        "-webkit-left": true,
        "-webkit-right": true,
        "-webkit-center": true,
        "outside": true,
        "inside": true,
        "disc": true,
        "circle": true,
        "square": true,
        "decimal": true,
        "decimal-leading-zero": true,
        "lower-roman": true,
        "upper-roman": true,
        "lower-greek": true,
        "lower-alpha": true,
        "lower-latin": true,
        "upper-alpha": true,
        "upper-latin": true,
        "hebrew": true,
        "armenian": true,
        "georgian": true,
        "cjk-ideographic": true,
        "hiragana": true,
        "katakana": true,
        "hiragana-iroha": true,
        "katakana-iroha": true,
        "inline": true,
        "block": true,
        "list-item": true,
        "run-in": true,
        "compact": true,
        "inline-block": true,
        "table": true,
        "inline-table": true,
        "table-row-group": true,
        "table-header-group": true,
        "table-footer-group": true,
        "table-row": true,
        "table-column-group": true,
        "table-column": true,
        "table-cell": true,
        "table-caption": true,
        "-webkit-box": true,
        "-webkit-inline-box": true,
        "-wap-marquee": true,
        "auto": true,
        "crosshair": true,
        "default": true,
        "pointer": true,
        "move": true,
        "vertical-text": true,
        "cell": true,
        "context-menu": true,
        "alias": true,
        "progress": true,
        "no-drop": true,
        "not-allowed": true,
        "-webkit-zoom-in": true,
        "-webkit-zoom-out": true,
        "e-resize": true,
        "ne-resize": true,
        "nw-resize": true,
        "n-resize": true,
        "se-resize": true,
        "sw-resize": true,
        "s-resize": true,
        "w-resize": true,
        "ew-resize": true,
        "ns-resize": true,
        "nesw-resize": true,
        "nwse-resize": true,
        "col-resize": true,
        "row-resize": true,
        "text": true,
        "wait": true,
        "help": true,
        "all-scroll": true,
        "-webkit-grab": true,
        "-webkit-grabbing": true,
        "ltr": true,
        "rtl": true,
        "capitalize": true,
        "uppercase": true,
        "lowercase": true,
        "visible": true,
        "collapse": true,
        "above": true,
        "absolute": true,
        "always": true,
        "avoid": true,
        "below": true,
        "bidi-override": true,
        "blink": true,
        "both": true,
        "close-quote": true,
        "crop": true,
        "cross": true,
        "embed": true,
        "fixed": true,
        "hand": true,
        "hide": true,
        "higher": true,
        "invert": true,
        "landscape": true,
        "level": true,
        "line-through": true,
        "local": true,
        "loud": true,
        "lower": true,
        "-webkit-marquee": true,
        "mix": true,
        "no-close-quote": true,
        "no-open-quote": true,
        "nowrap": true,
        "open-quote": true,
        "overlay": true,
        "overline": true,
        "portrait": true,
        "pre": true,
        "pre-line": true,
        "pre-wrap": true,
        "relative": true,
        "scroll": true,
        "separate": true,
        "show": true,
        "static": true,
        "thick": true,
        "thin": true,
        "underline": true,
        "-webkit-nowrap": true,
        "stretch": true,
        "start": true,
        "end": true,
        "reverse": true,
        "horizontal": true,
        "vertical": true,
        "inline-axis": true,
        "block-axis": true,
        "single": true,
        "multiple": true,
        "forwards": true,
        "backwards": true,
        "ahead": true,
        "up": true,
        "down": true,
        "slow": true,
        "fast": true,
        "infinite": true,
        "slide": true,
        "alternate": true,
        "read-only": true,
        "read-write": true,
        "read-write-plaintext-only": true,
        "element": true,
        "ignore": true,
        "intrinsic": true,
        "min-intrinsic": true,
        "clip": true,
        "ellipsis": true,
        "discard": true,
        "dot-dash": true,
        "dot-dot-dash": true,
        "wave": true,
        "continuous": true,
        "skip-white-space": true,
        "break-all": true,
        "break-word": true,
        "space": true,
        "after-white-space": true,
        "checkbox": true,
        "radio": true,
        "push-button": true,
        "square-button": true,
        "button": true,
        "button-bevel": true,
        "default-button": true,
        "list-button": true,
        "listbox": true,
        "listitem": true,
        "media-fullscreen-button": true,
        "media-mute-button": true,
        "media-play-button": true,
        "media-seek-back-button": true,
        "media-seek-forward-button": true,
        "media-rewind-button": true,
        "media-return-to-realtime-button": true,
        "media-slider": true,
        "media-sliderthumb": true,
        "media-volume-slider-container": true,
        "media-volume-slider": true,
        "media-volume-sliderthumb": true,
        "media-controls-background": true,
        "media-current-time-display": true,
        "media-time-remaining-display": true,
        "menulist": true,
        "menulist-button": true,
        "menulist-text": true,
        "menulist-textfield": true,
        "slider-horizontal": true,
        "slider-vertical": true,
        "sliderthumb-horizontal": true,
        "sliderthumb-vertical": true,
        "caret": true,
        "searchfield": true,
        "searchfield-decoration": true,
        "searchfield-results-decoration": true,
        "searchfield-results-button": true,
        "searchfield-cancel-button": true,
        "textfield": true,
        "textarea": true,
        "caps-lock-indicator": true,
        "round": true,
        "border": true,
        "border-box": true,
        "content": true,
        "content-box": true,
        "padding": true,
        "padding-box": true,
        "contain": true,
        "cover": true,
        "logical": true,
        "visual": true,
        "lines": true,
        "running": true,
        "paused": true,
        "flat": true,
        "preserve-3d": true,
        "ease": true,
        "linear": true,
        "ease-in": true,
        "ease-out": true,
        "ease-in-out": true,
        "document": true,
        "reset": true,
        "visiblePainted": true,
        "visibleFill": true,
        "visibleStroke": true,
        "painted": true,
        "fill": true,
        "stroke": true,
        "antialiased": true,
        "subpixel-antialiased": true,
        "optimizeSpeed": true,
        "optimizeLegibility": true,
        "geometricPrecision": true
    };
    function valueIdentAction(token) {
        this.cursor += token.length;
        if (token in valueKeywords) {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-keyword"));
        } else
            this.nonToken += token;
    }

    function numvalueAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-number"));
    }
    
    function declarationSemicolonAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    function urlAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-url"));
    }
    
    function stringAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-string"));
    }
    
    function colorAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-color"));
    }
    
    function importantAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-important"));
    }
    
    function atMediaAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtMedia;
    }
    
    function startAtMediaBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function atKeyframesAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtKeyframes;
    }
    
    function startAtKeyframesBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function atRuleAction(token) {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-css-at-rule"));
        this.lexState = this.LexState.AtRule;
    }
    
    function endAtRuleAction(token) {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function startAtRuleBlockAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DeclarationProperty;
    }
    
    const mediaTypes = ["all", "aural", "braille", "embossed", "handheld", "print", "projection", "screen", "tty", "tv"];
    function atRuleIdentAction(token) {
        this.cursor += token.length;
        if (mediaTypes.indexOf(token) === -1)
            this.nonToken += token;
        else {
            this.appendNonToken.call(this);
            this.newLine.appendChild(this.createSpan(token, "webkit-css-keyword"));
        }
    }
}

WebInspector.CSSSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;

WebInspector.JavaScriptSourceSyntaxHighlighter = function(table, sourceFrame) {
    WebInspector.SourceSyntaxHighlighter.call(this, table, sourceFrame);

    this.LexState = {
        Initial: 1,
        DivisionAllowed: 2,
    };
    this.ContinueState = {
        None: 0,
        Comment: 1,
        SingleQuoteString: 2,
        DoubleQuoteString: 3,
        RegExp: 4
    };
    
    this.nonToken = "";
    this.cursor = 0;
    this.lineIndex = -1;
    this.lineCode = "";
    this.newLine = null;
    this.lexState = this.LexState.Initial;
    this.continueState = this.ContinueState.None;
    
    this.rules = [{
        pattern: /^(?:\/\/.*)/,
        action: singleLineCommentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*\*+\/)/,
        action: multiLineSingleLineCommentAction
    }, {
        pattern: /^(?:\/\*(?:[^\*]|\*[^\/])*)/,
        action: multiLineCommentStartAction
    }, {
        pattern: /^(?:(?:[^\*]|\*[^\/])*\*+\/)/,
        action: multiLineCommentEndAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^.*/,
        action: multiLineCommentMiddleAction,
        continueStateCondition: this.ContinueState.Comment
    }, {
        pattern: /^(?:(?:0|[1-9]\d*)\.\d+?(?:[eE](?:\d+|\+\d+|-\d+))?|\.\d+(?:[eE](?:\d+|\+\d+|-\d+))?|(?:0|[1-9]\d*)(?:[eE](?:\d+|\+\d+|-\d+))?|0x[0-9a-fA-F]+|0X[0-9a-fA-F]+)/,
        action: numericLiteralAction
    }, {
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*"|'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        action: stringLiteralAction
    }, {
        pattern: /^(?:'(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: singleQuoteStringStartAction
    }, {
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*')/,
        action: singleQuoteStringEndAction,
        continueStateCondition: this.ContinueState.SingleQuoteString
    }, {
        pattern: /^(?:(?:[^'\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: singleQuoteStringMiddleAction,
        continueStateCondition: this.ContinueState.SingleQuoteString
    }, {
        pattern: /^(?:"(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: doubleQuoteStringStartAction
    }, {
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*")/,
        action: doubleQuoteStringEndAction,
        continueStateCondition: this.ContinueState.DoubleQuoteString
    }, {
        pattern: /^(?:(?:[^"\\]|\\(?:['"\bfnrtv]|[^'"\bfnrtv0-9xu]|0|x[0-9a-fA-F][0-9a-fA-F]|(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])))*)\\$/,
        action: doubleQuoteStringMiddleAction,
        continueStateCondition: this.ContinueState.DoubleQuoteString
    }, {
        pattern: /^(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: identOrKeywordAction
    }, {
        pattern: /^\)/,
        action: rightParenAction,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:<=|>=|===|==|!=|!==|\+\+|\-\-|<<|>>|>>>|&&|\|\||\+=|\-=|\*=|%=|<<=|>>=|>>>=|&=|\|=|^=|[{}\(\[\]\.;,<>\+\-\*%&\|\^!~\?:=])/,
        action: punctuatorAction,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:\/=?)/,
        action: divPunctuatorAction,
        lexStateCondition: this.LexState.DivisionAllowed,
        dontAppendNonToken: true
    }, {
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: regExpLiteralAction
    }, {
        pattern: /^(?:\/(?:(?:\\.)|[^\\*\/])(?:(?:\\.)|[^\\/])*)\\$/,
        action: regExpStartAction
    }, {
        pattern: /^(?:(?:(?:\\.)|[^\\/])*\/(?:(?:[a-zA-Z]|[$_]|\\(?:u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))|[0-9])*)/,
        action: regExpEndAction,
        continueStateCondition: this.ContinueState.RegExp
    }, {
        pattern: /^(?:(?:(?:\\.)|[^\\/])*)\\$/,
        action: regExpMiddleAction,
        continueStateCondition: this.ContinueState.RegExp
    }];
    
    function singleLineCommentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function multiLineSingleLineCommentAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function multiLineCommentStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
        this.continueState = this.ContinueState.Comment;
    }
    
    function multiLineCommentEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
        this.continueState = this.ContinueState.None;
    }
    
    function multiLineCommentMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-comment"));
    }
    
    function numericLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-number"));
        this.lexState = this.LexState.DivisionAllowed;
    }
    
    function stringLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.lexState = this.LexState.Initial;
    }
    
    function singleQuoteStringStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.SingleQuoteString;
    }
    
    function singleQuoteStringEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.None;
    }
    
    function singleQuoteStringMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
    }
    
    function doubleQuoteStringStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.DoubleQuoteString;
    }
    
    function doubleQuoteStringEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
        this.continueState = this.ContinueState.None;
    }
    
    function doubleQuoteStringMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-string"));
    }
    
    function regExpLiteralAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.lexState = this.LexState.Initial;
    }

    function regExpStartAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.continueState = this.ContinueState.RegExp;
    }

    function regExpEndAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
        this.continueState = this.ContinueState.None;
    }

    function regExpMiddleAction(token)
    {
        this.cursor += token.length;
        this.newLine.appendChild(this.createSpan(token, "webkit-javascript-regexp"));
    }
    
    const keywords = {
        "null": true,
        "true": true,
        "false": true,
        "break": true,
        "case": true,
        "catch": true,
        "const": true,
        "default": true,
        "finally": true,
        "for": true,
        "instanceof": true,
        "new": true,
        "var": true,
        "continue": true,
        "function": true,
        "return": true,
        "void": true,
        "delete": true,
        "if": true,
        "this": true,
        "do": true,
        "while": true,
        "else": true,
        "in": true,
        "switch": true,
        "throw": true,
        "try": true,
        "typeof": true,
        "debugger": true,
        "class": true,
        "enum": true,
        "export": true,
        "extends": true,
        "import": true,
        "super": true,
        "get": true,
        "set": true
    };
    function identOrKeywordAction(token)
    {
        this.cursor += token.length;
        
        if (token in keywords) {
            this.newLine.appendChild(this.createSpan(token, "webkit-javascript-keyword"));
            this.lexState = this.LexState.Initial;
        } else {
            var identElement = this.createSpan(token, "webkit-javascript-ident");
            identElement.addEventListener("mouseover", showDatatip, false);
            this.newLine.appendChild(identElement);
            this.lexState = this.LexState.DivisionAllowed;
        }
    }
    
    function showDatatip(event) {
        if (!WebInspector.panels.scripts || !WebInspector.panels.scripts.paused)
            return;

        var node = event.target;
        var parts = [node.textContent];
        while (node.previousSibling && node.previousSibling.textContent === ".") {
            node = node.previousSibling.previousSibling;
            if (!node || !node.hasStyleClass("webkit-javascript-ident"))
                break;
            parts.unshift(node.textContent);
        }

        var expression = parts.join(".");

        WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, "console", callback);
        function callback(result, exception)
        {
            if (exception)
                return;
            event.target.setAttribute("title", result.description);
            event.target.addEventListener("mouseout", onmouseout, false);
            
            function onmouseout(event)
            {
                event.target.removeAttribute("title");
                event.target.removeEventListener("mouseout", onmouseout, false);
            }
        }
    }
    
    function divPunctuatorAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
    
    function rightParenAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.DivisionAllowed;
    }
    
    function punctuatorAction(token)
    {
        this.cursor += token.length;
        this.nonToken += token;
        this.lexState = this.LexState.Initial;
    }
}

WebInspector.JavaScriptSourceSyntaxHighlighter.prototype.__proto__ = WebInspector.SourceSyntaxHighlighter.prototype;

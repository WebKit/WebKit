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

WebInspector.TextEditor = function(textModel, platform)
{
    this._textModel = textModel;
    this._textModel.changeListener = this._textChanged.bind(this);
    this._highlighter = new WebInspector.TextEditorHighlighter(this._textModel, this._highlightChanged.bind(this));

    this.element = document.createElement("div");
    this.element.className = "text-editor monospace";
    this.element.tabIndex = 0;

    this._canvas = document.createElement("canvas");
    this._canvas.className = "text-editor-canvas";
    this.element.appendChild(this._canvas);

    this._container = document.createElement("div");
    this._container.className = "text-editor-container";
    this.element.appendChild(this._container);

    this._sheet = document.createElement("div");
    this._container.appendChild(this._sheet);

    var cursorElement = document.createElement("div");
    cursorElement.className = "text-editor-cursor";
    this._container.appendChild(cursorElement);
    this._cursor = new WebInspector.TextCursor(cursorElement);

    this._container.addEventListener("scroll", this._scroll.bind(this), false);

    this._registerMouseListeners();
    this._registerKeyboardListeners();
    this._registerClipboardListeners();

    this._desiredCaretColumn = 0;
    this._scrollLeft = 0;
    this._scrollTop = 0;

    this._ctx = this._canvas.getContext("2d");
    this._selection = new WebInspector.TextSelectionModel(this._selectionChanged.bind(this));

    this._isMac = platform && (platform.indexOf("mac") === 0);
    this._paintCoalescingLevel = 0;

    this._registerShortcuts();
    // Debugging flags, allow disabling / enabling highlights and track repaints.
    this._highlightingEnabled = true;
    this._debugMode = false;

    this._textWidth = 0;
    this._longestLineNumber = 0;

    this._lineOffsetsCache = [0];
    this._readOnly = false;
    this._selectionColor = "rgb(181, 213, 255)";
}

WebInspector.TextEditor.prototype = {
    set text(text)
    {
        var lastLine = this._textModel.linesCount - 1;
        this._textModel.setText(null, text);
        this._textModel.resetUndoStack();
        this._setCaretLocation(0, 0);
    },

    set mimeType(mimeType)
    {
        this._highlighter.mimeType = mimeType;
    },

    get textModel()
    {
        return this._textModel;
    },

    set readOnly(readOnly)
    {
        this._readOnly = readOnly;
        if (readOnly)
            this.element.addStyleClass("text-editor-readonly")
        else
            this.element.removeStyleClass("text-editor-readonly")
    },

    set lineNumberDecorator(lineNumberDecorator)
    {
        this._lineNumberDecorator = lineNumberDecorator;
    },

    set lineDecorator(lineDecorator)
    {
        this._lineDecorator = lineDecorator;
    },

    get selection()
    {
        return this._selection.range();
    },

    setSelection: function(startLine, startColumn, endLine, endColumn)
    {
        var start = this._fit(startLine, startColumn);
        this._selection.setStart(start.line, start.column);
        this._setSelectionEnd(endLine, endColumn);
    },

    setDivDecoration: function(lineNumber, element)
    {
        var existingElement = this._textModel.getAttribute(lineNumber, "div-decoration");
        if (existingElement && existingElement.parentNode)
            existingElement.parentNode.removeChild(existingElement);
        this._textModel.removeAttribute(lineNumber, "div-decoration");

        if (element) {
            this.element.appendChild(element);
            this._textModel.setAttribute(lineNumber, "div-decoration", element);
        }
        this.revalidateDecorationsAndPaint();
    },

    _registerMouseListeners: function()
    {
        this.element.addEventListener("contextmenu", this._contextMenu.bind(this), false);
        this.element.addEventListener("mouseup", this._mouseUp.bind(this), false);
        this.element.addEventListener("mousedown", this._mouseDown.bind(this), false);
        this.element.addEventListener("mousemove", this._mouseMove.bind(this), false);
        this.element.addEventListener("mouseout", this._mouseOut.bind(this), false);
        this.element.addEventListener("dblclick", this._dblClick.bind(this), false);
    },

    _registerKeyboardListeners: function()
    {
        this._container.addEventListener("keydown", this._keyDown.bind(this), false);
        this._container.addEventListener("textInput", this._textInput.bind(this), false);
    },

    _registerClipboardListeners: function()
    {
        this._container.addEventListener("beforecopy", this._beforeCopy.bind(this), false);
        this._container.addEventListener("copy", this._copy.bind(this), false);
        this._container.addEventListener("beforecut", this._beforeCut.bind(this), false);
        this._container.addEventListener("cut", this._cut.bind(this), false);
        this._container.addEventListener("beforepaste", this._beforePaste.bind(this), false);
        this._container.addEventListener("paste", this._paste.bind(this), false);
    },

    _offsetToLine: function(offset)
    {
        if (offset > this._lineOffsetsCache[this._lineOffsetsCache.length - 1]) {
            // Seeking outside cached area. Fill the cache.
            var lineNumber = this._lineOffsetsCache.length;
            while (lineNumber < this._textModel.linesCount && this._lineToOffset(lineNumber) < offset)
                lineNumber++;
            return lineNumber;
        }

        // Bisect.
        var from = 0;
        var to = this._lineOffsetsCache.length;
        while (to > from + 1) {
            var mid = Math.floor((from + to) / 2);
            if (this._lineOffsetsCache[mid] > offset)
                to = mid;
            else
                from = mid;
        }
        return to;
    },

    _lineToOffset: function(lineNumber)
    {
        var offset = this._lineOffsetsCache[lineNumber];
        if (offset)
            return offset;
        for (var line = lineNumber; line > 0; --line) {
            if (this._lineOffsetsCache[line])
                break;
        }
        offset = this._lineOffsetsCache[line];
        for (var i = line + 1; i <= lineNumber; ++i) {
            offset += this._lineHeight(i - 1);
            this._lineOffsetsCache[i] = offset;
        }
        return offset;
    },

    _lineHeight: function(lineNumber)
    {
        // Use cached value first.
        if (this._lineOffsetsCache[lineNumber + 1])
            return this._lineOffsetsCache[lineNumber + 1] - this._lineOffsetsCache[lineNumber];

        var element = this._textModel.getAttribute(lineNumber, "div-decoration");
        if (element)
            return 2 * this._textLineHeight + element.clientHeight;
        return this._textLineHeight;
    },

    reveal: function(line, column)
    {
        this._scrollTop = this._container.scrollTop;
        this._scrollLeft = this._container.scrollLeft;

        var maxScrollTop = this._lineToOffset(line);
        var minScrollTop = maxScrollTop + this._lineHeight(line) - this._canvas.height;
        if (this._scrollTop > maxScrollTop)
            this._container.scrollTop = maxScrollTop - this._textLineHeight * 2;
        else if (this._scrollTop < minScrollTop)
            this._container.scrollTop = minScrollTop + this._textLineHeight * 2;

        var firstColumn = this._columnForOffset(line, this._scrollLeft);
        var maxScrollLeft = this._columnToOffset(line, column);
        var minScrollLeft = maxScrollLeft - this._container.clientWidth + this._lineNumberWidth;
        if (this._scrollLeft < minScrollLeft)
            this._container.scrollLeft = minScrollLeft + 100;
        else if (this._scrollLeft > maxScrollLeft)
            this._container.scrollLeft = maxScrollLeft;
        else if (minScrollLeft < 0 && maxScrollLeft > 0)
            this._container.scrollLeft = 0;
    },

    // WebInspector.TextModel listener
    _textChanged: function(oldRange, newRange, oldText, newText)
    {
        if (newRange.linesCount == oldRange.linesCount)
            this._invalidateLines(newRange.startLine, newRange.endLine + 1);
        else
            // Lines shifted, invalidate all under start line. Also clear lines that now are outside model range.
            this._invalidateLines(newRange.startLine, this._textModel.linesCount + Math.max(0, oldRange.endLine - newRange.endLine));

        if (this._highlightingEnabled) {
            var lastVisibleLine = Math.min(this._textModel.linesCount, this._offsetToLine(this._scrollTop + this._canvas.height) + 1);
            this._highlighter.updateHighlight(newRange.startLine, lastVisibleLine);
        }

        this._updatePreferredSize(newRange.startLine, Math.max(newRange.endLine, oldRange.endLine));
        if (oldRange.linesCount !== newRange.linesCount) {
            // Invalidate offset cache.
            this._lineOffsetsCache.length = oldRange.startLine + 1;
            // Force linenumber cache to be continuous.
            this._lineToOffset(oldRange.startLine);
            this.paintLineNumbers();
        }
        this._paint();
    },

    // WebInspector.TextSelectionModel listener
    _selectionChanged: function(oldRange, newRange)
    {
        if (oldRange.isEmpty() && newRange.isEmpty() && oldRange.startLine === newRange.startLine) {
            // Nothing to repaint.
            return;
        }

        this._invalidateLines(oldRange.startLine, oldRange.endLine + 1);
        this._invalidateLines(newRange.startLine, newRange.endLine + 1);
        this._paint();
    },

    _highlightChanged: function(fromLine, toLine)
    {
        if (this._muteHighlightListener)
            return;

        this._invalidateLines(fromLine, toLine);
        this._paint();
    },

    revalidateDecorationsAndPaint: function()
    {
        this.setCoalescingUpdate(true);
        this._lineOffsetsCache = [0];
        this._updatePreferredSize(0, this._textModel.linesCount);
        this.repaintAll();
        this.setCoalescingUpdate(false);
    },

    _updatePreferredSize: function(startLine, endLine)
    {
        this._ctx.font = this._font;
        this.setCoalescingUpdate(true);
        var guardedEndLine = Math.min(this._textModel.linesCount, endLine + 1);
        var newMaximum = false;
        for (var i = startLine; i < guardedEndLine; ++i) {
            var lineWidth = this._ctx.measureText(this._textModel.line(i)).width;
            if (lineWidth > this._textWidth) {
                this._textWidth = lineWidth;
                this._longestLineNumber = i;
                newMaximum = true;
            }
        }

        if (!newMaximum && startLine <= this._longestLineNumber && this._longestLineNumber <= endLine) {
            this._textWidth = 0;
            this._longestLineNumber = 0;
            for (var i = 0; i < this._textModel.linesCount; ++i) {
                var lineWidth = this._ctx.measureText(this._textModel.line(i)).width;
                if (lineWidth > this._textWidth) {
                    this._textWidth = lineWidth;
                    this._longestLineNumber = i;
                }
            }
        }

        var newLineNumberDigits = this._decimalDigits(this._textModel.linesCount);
        this._lineNumberWidth = (newLineNumberDigits + 2) * this._digitWidth;
        this._container.style.left = this._lineNumberWidth + "px";

        var newWidth = this._textWidth + "px";
        var newHeight = this._lineToOffset(this._textModel.linesCount) + "px";
        this._sheet.style.width = newWidth;
        this._sheet.style.height = newHeight;

        if (newLineNumberDigits !== this._lineNumberDigits) {
            this._lineNumberDigits = newLineNumberDigits;
            this.repaintAll();
        }

        // Changes to size can change the client area (scrollers can appear/disappear)
        this.resize();
        this.setCoalescingUpdate(false);
    },

    resize: function()
    {
        if (this._canvas.width !== this._container.clientWidth || this._canvas.height !== this._container.clientHeight) {
            this._canvas.width = this._container.clientWidth + this._lineNumberWidth;
            this._canvas.height = this._container.clientHeight;
            this.repaintAll();
        }
    },

    repaintAll: function()
    {
        this._invalidateLines(0, this._textModel.linesCount);
        this._paint();
    },

    _invalidateLines: function(startLine, endLine)
    {
        if (!this._damage)
            this._damage = [ { startLine: startLine, endLine: endLine } ];
        else {
            for (var i = 0; i < this._damage.length; ++i) {
                var chunk = this._damage[i];
                if (chunk.startLine <= endLine && chunk.endLine >= startLine) {
                    chunk.startLine = Math.min(chunk.startLine, startLine);
                    chunk.endLine = Math.max(chunk.endLine, endLine);
                    return;
                }
            }
            this._damage.push({ startLine: startLine, endLine: endLine });
        }
    },

    _paint: function()
    {
        this._scrollTop = this._container.scrollTop;
        this._scrollLeft = this._container.scrollLeft;

        if (this._paintCoalescingLevel)
            return;

        this._updateDivDecorations();

        this.paintLineNumbers();

        for (var i = 0; this._damage && i < this._damage.length; ++i)
            this._paintLines(this._damage[i].startLine, this._damage[i].endLine);
        delete this._damage;

        this._updateCursor(this._selection.endLine, this._selection.endColumn);
    },

    _paintLines: function(firstLine, lastLine)
    {
        this._ctx.font = this._font;
        this._ctx.textBaseline = "bottom";

        firstLine = Math.max(firstLine, this._offsetToLine(this._scrollTop) - 1);
        lastLine = Math.min(lastLine, this._offsetToLine(this._scrollTop + this._canvas.height) + 1);
        if (firstLine > lastLine)
            return;

        if (this._debugMode) {
            WebInspector.log("Repaint %d:%d", firstLine, lastLine);
            this._ctx.fillStyle = "rgb(255,255,0)";
            var fromOffset = this._lineToOffset(firstLine);
            var toOffset = this._lineToOffset(lastLine);
            this._ctx.fillRect(this._lineNumberWidth - 1, fromOffset - this._scrollTop, this._canvas.width - this._lineNumberWidth + 1, toOffset - fromOffset);
            setTimeout(this._paintLinesContinuation.bind(this, firstLine, lastLine), 100);
        } else
            this._paintLinesContinuation(firstLine, lastLine);
    },

    _paintLinesContinuation: function(firstLine, lastLine) {
        // Clip editor area.
        this._ctx.save();
        this._ctx.beginPath();
        this._ctx.rect(this._lineNumberWidth - 1, 0, this._canvas.width - this._lineNumberWidth + 1, this._canvas.height);
        this._ctx.clip();

        // First clear the region, then update last line to fit model (this clears removed lines from the end of the document).
        var fromOffset = this._lineToOffset(firstLine);
        var toOffset = lastLine < this._textModel.linesCount ? this._lineToOffset(lastLine) : this._canvas.height + this._scrollTop;

        // Do not clear region when paintCurrentLine is likely to do all the necessary work.
        if (this._readOnly || firstLine + 1 != lastLine || this._selection.endLine != firstLine) {
            this._ctx.fillStyle = "rgb(255,255,255)";
            this._ctx.fillRect(0, fromOffset - this._scrollTop, this._canvas.width, toOffset - fromOffset);
        }
        lastLine = Math.min(lastLine, this._textModel.linesCount);

        // Paint current line for editable mode only.
        if (!this._readOnly && this._selection.startLine === this._selection.endLine && firstLine <= this._selection.startLine && this._selection.startLine < lastLine)
            this._paintCurrentLine(this._selection.startLine);

        this._paintSelection(firstLine, lastLine);

        if (this._highlightingEnabled) {
            this._muteHighlightListener = true;
            this._highlighter.highlight(lastLine);
            delete this._muteHighlightListener;
        }
        for (var i = firstLine; i < lastLine; ++i) {
            var lineOffset = this._lineToOffset(i) - this._scrollTop;

            if (this._lineDecorator)
                this._lineDecorator.decorate(i, this._ctx, this._lineNumberWidth - 1, lineOffset, this._canvas.width - this._lineNumberWidth + 1, this._lineHeight(i), this._textLineHeight);

            var element = this._textModel.getAttribute(i, "div-decoration");
            if (element)
                this._positionDivDecoration(i, element, true);

            this._paintLine(i, lineOffset);
        }
        this._ctx.restore();
    },

    _paintLine: function(lineNumber, lineOffset)
    {
        var line = this._textModel.line(lineNumber);
        if (!this._highlightingEnabled) {
            this._ctx.fillStyle = "rgb(0,0,0)";
            this._ctx.fillText(line, this._lineNumberWidth - this._scrollLeft, lineOffset + this._textLineHeight);
            return;
        }

        if (line.length > 1000) {
            // Optimization: no need to paint decorations outside visible area.
            var firstColumn = this._columnForOffset(lineNumber, this._scrollLeft);
            var lastColumn = this._columnForOffset(lineNumber, this._scrollLeft + this._canvas.width);
        }
        var highlighterState = this._textModel.getAttribute(lineNumber, "highlighter-state");
        var plainTextStart = -1;
        for (var j = 0; j < line.length;) {
            var attribute = highlighterState && highlighterState.attributes[j];
            if (attribute && firstColumn && j + attribute.length < firstColumn) {
                j += attribute.length;
                continue;
            }
            if (attribute && lastColumn && j > lastColumn)
                break;
            if (!attribute || !attribute.style) {
                if (plainTextStart === -1)
                    plainTextStart = j;
                j++;
            } else {
                if (plainTextStart !== -1) {
                    this._ctx.fillStyle = "rgb(0,0,0)";
                    this._ctx.fillText(line.substring(plainTextStart, j), this._lineNumberWidth - this._scrollLeft + this._columnToOffset(lineNumber, plainTextStart), lineOffset + this._textLineHeight);
                    plainTextStart = -1;
                }
                this._ctx.fillStyle = attribute.style;
                this._ctx.fillText(line.substring(j, j + attribute.length), this._lineNumberWidth - this._scrollLeft + this._columnToOffset(lineNumber, j), lineOffset + this._textLineHeight);
                j += attribute.length;
            }
        }
        if (plainTextStart !== -1) {
            this._ctx.fillStyle = "rgb(0,0,0)";
            this._ctx.fillText(line.substring(plainTextStart, j), this._lineNumberWidth - this._scrollLeft + this._columnToOffset(lineNumber, plainTextStart), lineOffset + this._textLineHeight);
        }
    },

    paintLineNumbers: function()
    {
        this._ctx.font = this._font;
        this._ctx.textBaseline = "bottom";

        this._ctx.fillStyle = "rgb(255,255,255)";
        this._ctx.fillRect(0, 0, this._lineNumberWidth - 2, this._canvas.height);

        this._ctx.fillStyle = "rgb(235,235,235)";
        this._ctx.fillRect(this._lineNumberWidth - 2, 0, 1, this._canvas.height);

        var firstLine = Math.max(0, this._offsetToLine(this._scrollTop) - 1);
        var lastLine = Math.min(this._textModel.linesCount, this._offsetToLine(this._scrollTop + this._canvas.height) + 1);

        for (var i = firstLine; i < lastLine; ++i) {
            var lineOffset = this._lineToOffset(i) - this._scrollTop;
            this._ctx.fillStyle = "rgb(155,155,155)";
            if (this._lineNumberDecorator && this._lineNumberDecorator.decorate(i, this._ctx, 0, lineOffset, this._lineNumberWidth, this._lineHeight(i), this._textLineHeight))
                continue;
            this._ctx.fillText(i + 1, (this._lineNumberDigits - this._decimalDigits(i + 1) + 1) * this._digitWidth, lineOffset + this._textLineHeight);
        }
    },

    _paintCurrentLine: function(line)
    {
        this._ctx.fillStyle = "rgb(232, 242, 254)";
        this._ctx.fillRect(0, this._lineToOffset(line) - this._scrollTop, this._canvas.width, this._lineHeight(line));
    },

    _scroll: function(e)
    {
        // Hide div-based cursor first.
        this._cursor._cursorElement.style.display = "none";
        setTimeout(this._repaintOnScroll.bind(this), 10);
    },

    _repaintOnScroll: function()
    {
        if (this._scrollTop !== this._container.scrollTop || this._scrollLeft !== this._container.scrollLeft) {
            this._scrollTop = this._container.scrollTop;
            this._scrollLeft = this._container.scrollLeft;
            this.repaintAll();
        }
    },

    _mouseUp: function(e)
    {
        this._isDragging = false;
    },

    _mouseDown: function(e)
    {
        if (e.button === 2 || (this._isMac && e.ctrlKey))
            return;

        var location = this._caretForMouseEvent(e);

        if (e.target === this.element && this._lineNumberDecorator) {
            if (this._lineNumberDecorator.mouseDown(location.line, e))            
                return;
        }

        if (e.shiftKey)
            this._setSelectionEnd(location.line, location.column);
        else
            this._setCaretLocation(location.line, location.column);
        this._isDragging = true;
        this._textModel.markUndoableState();
    },

    _mouseMove: function(e)
    {
        if (!this._isDragging)
            return;
        var location = this._caretForMouseEvent(e);
        this._setSelectionEnd(location.line, location.column)
    },

    _mouseOut: function(e)
    {
    },

    _dblClick: function(e)
    {
        var location = this._caretForMouseEvent(e);
        var range = this._textModel.wordRange(location.line, location.column);
        this.setSelection(range.startLine, range.startColumn, range.endLine, range.endColumn);
    },

    _contextMenu: function(e)
    {
        if (e.target === this.element && this._lineNumberDecorator) {
            var location = this._caretForMouseEvent(e);
            if (this._lineNumberDecorator.contextMenu(location.line, e))
                return;
        } else {
            var range = this._selection.range();
            if (!range.isEmpty()) {
                var text = this._textModel.copyRange(range);
                var contextMenu = new WebInspector.ContextMenu();
                contextMenu.appendItem(WebInspector.UIString("Copy"), this._copy.bind(this));
                contextMenu.show(event);
            }
        }
    },

    _caretForMouseEvent: function(e)
    {
        var lineNumber = Math.max(0, this._offsetToLine(e.offsetY + (e.target === this.element ? this._scrollTop : 0)) - 1);
        var offset = e.offsetX + this._scrollLeft;
        return { line: lineNumber, column: this._columnForOffset(lineNumber, offset) };
    },

    _columnForOffset: function(lineNumber, offset)
    {
        var length = 0;
        var line = this._textModel.line(lineNumber);

        // First pretend it is monospace to get a quick guess.
        var charWidth = this._ctx.measureText("a").width;
        var index = Math.floor(offset / charWidth);
        var indexOffset = this._ctx.measureText(line.substring(0, index)).width;
        if (offset >= indexOffset && index < line.length && offset < indexOffset + this._ctx.measureText(line.charAt(index)).width)
            return index;

        // Fallback to non-monospace.
        var delta = indexOffset < offset ? 1 : -1;
        while (index >=0 && index < line.length) {
            index += delta;
            indexOffset += delta * this._ctx.measureText(line.charAt(index)).width;
            if (offset >= indexOffset && offset < indexOffset + charWidth)
                return index;
        }
        return line.length;
    },

    _columnToOffset: function(lineNumber, column)
    {
        var line = this._textModel.line(lineNumber);
        return this._ctx.measureText(line.substring(0, column)).width;
    },

    _keyDown: function(e)
    {
        var shortcutKey = WebInspector.KeyboardShortcut.makeKeyFromEvent(e);
        var handler = this._shortcuts[shortcutKey];
        if (handler) {
            handler.call(this);
            e.preventDefault();
            e.stopPropagation();
            return;
        }

        if (this._handleNavigationKey(e)) {
            e.preventDefault();
            e.stopPropagation();
            return;
        }

        if (this._readOnly)
            return;

        var keyCodes = WebInspector.KeyboardShortcut.KeyCodes;
        switch (e.keyCode) {
            case keyCodes.Backspace:
                this._handleBackspaceKey();
                break;
            case keyCodes.Delete:
                this._handleDeleteKey();
                break;
            case keyCodes.Tab:
                this._replaceSelectionWith("\t");
                break;
            case keyCodes.Enter:
                this._replaceSelectionWith("\n");
                break;
            default:
                return;
        }

        e.preventDefault();
        e.stopPropagation();
    },

    _handleNavigationKey: function(e)
    {
        var caretLine = this._selection.endLine;
        var caretColumn = this._selection.endColumn;
        var arrowAction = e.shiftKey ? this._setSelectionEnd : this._setCaretLocation;

        var keyCodes = WebInspector.KeyboardShortcut.KeyCodes;
        switch (e.keyCode) {
            case keyCodes.Up:
            case keyCodes.PageUp:
                if (e.metaKey)
                    arrowAction.call(this, 0, 0, true);
                else if (e.ctrlKey)
                    this._container.scrollTop -= this._lineHeight(caretLine);
                else {
                    if (e.keyCode === keyCodes.Up)
                        arrowAction.call(this, caretLine - 1, this._desiredCaretColumn, true);
                    else {
                        var offset = Math.max(0, this._lineToOffset(caretLine) - this._canvas.height);
                        arrowAction.call(this, this._offsetToLine(offset), this._desiredCaretColumn, true);
                    }
                }
                break;
            case keyCodes.Down:
            case keyCodes.PageDown:
                if (e.metaKey)
                    arrowAction.call(this, this._textModel.linesCount - 1, this._textModel.lineLength(this._textModel.linesCount - 1), true);
                else if (e.ctrlKey)
                    this._container.scrollTop += this._lineHeight(caretLine);
                else {
                    if (e.keyCode === keyCodes.Down)
                        arrowAction.call(this, caretLine + 1, this._desiredCaretColumn, true);
                    else {
                        var offset = this._lineToOffset(caretLine) + this._canvas.height;
                        arrowAction.call(this, this._offsetToLine(offset), this._desiredCaretColumn, true);
                    }
                }
                break;
            case keyCodes.Home:
                if (this._isMetaCtrl(e))
                    arrowAction.call(this, 0, 0, true);
                else
                    arrowAction.call(this, this._selection.endLine, 0);
                break;
            case keyCodes.End:
                if (this._isMetaCtrl(e))
                    arrowAction.call(this, this._textModel.linesCount - 1, this._textModel.lineLength(this._textModel.linesCount - 1), true);
                else
                    arrowAction.call(this, this._selection.endLine, this._textModel.lineLength(this._selection.endLine));
                break;
            case keyCodes.Left:
                if (!e.shiftKey && !e.metaKey && !this._isAltCtrl(e) && !this._selection.isEmpty()) {
                    // Reset selection
                    var range = this._selection.range();
                    this._setCaretLocation(range.startLine, range.startColumn);
                } else if (e.metaKey)
                    arrowAction.call(this, this._selection.endLine, 0);
                else if (caretColumn === 0 && caretLine > 0)
                    arrowAction.call(this, caretLine - 1, this._textModel.lineLength(caretLine - 1));
                else if (this._isAltCtrl(e)) {
                    caretColumn = this._textModel.wordStart(this._selection.endLine, this._selection.endColumn);
                    if (caretColumn === this._selection.endColumn)
                        caretColumn = 0;
                    arrowAction.call(this, caretLine, caretColumn);
                } else
                    arrowAction.call(this, caretLine, caretColumn - 1);
                break;
            case keyCodes.Right:
                var line = this._textModel.line(caretLine);
                if (!e.shiftKey && !e.metaKey && !this._isAltCtrl(e) && !this._selection.isEmpty()) {
                    // Reset selection
                    var range = this._selection.range();
                    this._setCaretLocation(range.endLine, range.endColumn);
                } else if (e.metaKey)
                    arrowAction.call(this, this._selection.endLine, this._textModel.lineLength(this._selection.endLine));
                else if (caretColumn === line.length && caretLine < this._textModel.linesCount - 1)
                    arrowAction.call(this, caretLine + 1, 0);
                else if (this._isAltCtrl(e)) {
                    caretColumn = this._textModel.wordEnd(this._selection.endLine, this._selection.endColumn);
                    if (caretColumn === this._selection.endColumn)
                        caretColumn = line.length;
                    arrowAction.call(this, caretLine, caretColumn);
                } else
                    arrowAction.call(this, caretLine, caretColumn + 1);
                break;
            default:
                return false;
        }
        this._textModel.markUndoableState();
        return true;
    },

    _textInput: function(e)
    {
        if (this._readOnly)
            return;

        if (e.data && !e.altKey && !e.ctrlKey && !e.metaKey) {
            this._replaceSelectionWith(e.data);
            e.preventDefault();
            e.stopPropagation();
        }
    },

    _setCaretLocation: function(line, column, updown)
    {
        this.setSelection(line, column, line, column, updown);
    },

    _setSelectionEnd: function(line, column, updown)
    {
        if (!updown)
            this._desiredCaretColumn = column;

        var end = this._fit(line, column);
        this._selection.setEnd(end.line, end.column);
        this.reveal(this._selection.endLine, this._selection.endColumn);
        this._updateCursor(end.line, end.column);
    },

    _updateDivDecorations: function()
    {
        var firstLine = this._offsetToLine(this._scrollTop) - 1;
        var lastLine = this._offsetToLine(this._scrollTop + this._canvas.height) + 1;

        var linesCount = this._textModel.linesCount;
        for (var i = 0; i < linesCount; ++i) {
            var element = this._textModel.getAttribute(i, "div-decoration");
            if (element) {
                this._lineOffsetsCache.length = Math.min(this._lineOffsetsCache.length, i + 1);
                this._positionDivDecoration(i, element, i > firstLine && i < lastLine);
            }
        }
    },

    _positionDivDecoration: function(lineNumber, element, visible)
    {
        element.style.position = "absolute";
        element.style.top = this._lineToOffset(lineNumber) - this._scrollTop + this._textLineHeight + "px";
        element.style.left = this._lineNumberWidth + "px";
        element.style.setProperty("max-width", this._canvas.width + "px");
    },

    _updateCursor: function(line, column)
    {
        if (line >= this._textModel.linesCount)
            return;
        var offset = this._columnToOffset(line, column);
        if (offset >= this._container.scrollLeft && !this._readOnly)
            this._cursor.setLocation(this._lineNumberWidth + offset - 1, this._lineToOffset(line));
        else
            this._cursor.hide();
    },

    _fit: function(line, column)
    {
        line = Math.max(0, Math.min(line, this._textModel.linesCount - 1));
        var lineLength = this._textModel.lineLength(line);
        column = Math.max(0, Math.min(column, lineLength));
        return { line: line, column: column };
    },

    _paintSelection: function(firstLine, lastLine)
    {
        if (this._selection.isEmpty())
            return;
        var range = this._selection.range();
        this._ctx.fillStyle = this._selectionColor;

        firstLine = Math.max(firstLine, range.startLine);
        endLine = Math.min(lastLine, range.endLine + 1);

        for (var i = firstLine; i < endLine; ++i) {
            var line = this._textModel.line(i);
            var from, to;

            if (i === range.startLine) {
                var offset = this._columnToOffset(range.startLine, range.startColumn);
                from = offset - this._scrollLeft + this._lineNumberWidth - 1;
            } else
                from = 0;

            if (i === range.endLine) {
                var offset = this._columnToOffset(range.endLine, range.endColumn);
                to = offset - this._scrollLeft + this._lineNumberWidth - 1;
            } else
                to = this._canvas.width;

            this._ctx.fillRect(from, this._lineToOffset(i) - this._scrollTop, to - from, this._lineHeight(i));
        }
        this._ctx.fillStyle = "rgb(0, 0, 0)";
    },

    _beforeCopy: function(e)
    {
        if (!this._selection.isEmpty())
            e.preventDefault();
    },

    _copy: function(e)
    {
        var range = this._selection.range();
        var text = this._textModel.copyRange(range);

        function delayCopy()
        {
            InspectorFrontendHost.copyText(text);
        }

        setTimeout(delayCopy);
        if (e)
            e.preventDefault();
    },

    _beforeCut: function(e)
    {
        if (!this._selection.isEmpty())
            e.preventDefault();
    },

    _cut: function(e)
    {
        if (this._readOnly) {
            e.preventDefault();
            return;
        }

        this._textModel.markUndoableState();
        this._copy(e);
        this._replaceSelectionWith("");
    },

    _beforePaste: function(e)
    {
        e.preventDefault();
    },

    _paste: function(e)
    {
        if (this._readOnly) {
            e.preventDefault();
            return;
        }

        var text = e.clipboardData.getData("Text");
        if (!text)
            return;

        this._textModel.markUndoableState();
        this._replaceSelectionWith(text);
        e.preventDefault();
    },

    _replaceSelectionWith: function(newText, overrideRange)
    {
        var range = overrideRange || this._selection.range();
        this.setCoalescingUpdate(true);
        var newRange = this._textModel.setText(range, newText);
        this._setCaretLocation(newRange.endLine, newRange.endColumn);
        this.setCoalescingUpdate(false);
    },

    setCoalescingUpdate: function(enabled)
    {
        if (enabled)
            this._paintCoalescingLevel++;
        else
            this._paintCoalescingLevel--;
        if (!this._paintCoalescingLevel)
            this._paint();
    },

    _selectAll: function()
    {
        // No need to reveal last selection line in select all.
        this._selection.setStart(0, 0);
        var lastLineNum = this._textModel.linesCount - 1;
        this._selection.setEnd(lastLineNum, this._textModel.lineLength(lastLineNum));
        this._updateCursor(this._selection.endLine, this._selection.endColumn);
    },

    initFontMetrics: function()
    {
        var computedStyle = window.getComputedStyle(this.element);
        this._font = computedStyle.fontSize + " " + computedStyle.fontFamily;
        this._ctx.font = this._font;
        this._digitWidth = this._ctx.measureText("0").width;
        this._textLineHeight = Math.floor(parseInt(this._ctx.font) * 1.4);
        this._cursor.setTextLineHeight(this._textLineHeight);
    },

    _registerShortcuts: function()
    {
        var modifiers = WebInspector.KeyboardShortcut.Modifiers;
        this._shortcuts = {};
        this._shortcuts[WebInspector.KeyboardShortcut.makeKey("z", this._isMac ? modifiers.Meta : modifiers.Ctrl)] = this._handleUndo.bind(this);
        this._shortcuts[WebInspector.KeyboardShortcut.makeKey("z", modifiers.Shift | (this._isMac ? modifiers.Meta : modifiers.Ctrl))] = this._handleRedo.bind(this);
        this._shortcuts[WebInspector.KeyboardShortcut.makeKey("a", this._isMac ? modifiers.Meta : modifiers.Ctrl)] = this._selectAll.bind(this);
        if (this._isMac)
            this._shortcuts[WebInspector.KeyboardShortcut.makeKey("d", modifiers.Ctrl)] = this._handleDeleteKey.bind(this);

        this._shortcuts[WebInspector.KeyboardShortcut.makeKey("d", modifiers.Ctrl | modifiers.Alt)] = this._handleToggleDebugMode.bind(this);
        this._shortcuts[WebInspector.KeyboardShortcut.makeKey("h", modifiers.Ctrl | modifiers.Alt)] = this._handleToggleHighlightMode.bind(this);
    },

    _handleUndo: function()
    {
        this.setCoalescingUpdate(true);
        var range = this._textModel.undo();
        if (range)
            this._setCaretLocation(range.endLine, range.endColumn);
        this.setCoalescingUpdate(false);
    },

    _handleRedo: function()
    {
        this.setCoalescingUpdate(true);
        var range = this._textModel.redo();
        if (range)
            this._setCaretLocation(range.endLine, range.endColumn);
        this.setCoalescingUpdate(false);
    },

    _handleDeleteKey: function()
    {
        var range = this._selection.range();
        if (range.isEmpty()) {
            if (range.endColumn < this._textModel.lineLength(range.startLine))
                range.endColumn++;
            else if (range.endLine < this._textModel.linesCount) {
                range.endLine++;
                range.endColumn = 0;
            } else
                return;
        } else
            this._textModel.markUndoableState();
        this._replaceSelectionWith("", range);
    },

    _handleBackspaceKey: function()
    {
        var range = this._selection.range();
        if (range.isEmpty()) {
            if (range.startColumn > 0)
                range.startColumn--;
            else if (range.startLine > 0) {
                range.startLine--;
                range.startColumn = this._textModel.lineLength(range.startLine);
            } else
                return;
        } else
            this._textModel.markUndoableState();
        this._replaceSelectionWith("", range);
    },

    _handleToggleDebugMode: function()
    {
        this._debugMode = !this._debugMode;
    },

    _handleToggleHighlightMode: function()
    {
        this._highlightingEnabled = !this._highlightingEnabled;
    },

    _isMetaCtrl: function(e)
    {
        return this._isMac ? e.metaKey : e.ctrlKey;
    },

    _isAltCtrl: function(e)
    {
        return this._isMac ? e.altKey : e.ctrlKey;
    },

    _decimalDigits: function(number)
    {
        return Math.ceil(Math.log(number + 1) / Math.log(10));
    }
}

WebInspector.TextSelectionModel = function(changeListener)
{
    this.startLine = 0;
    this.startColumn = 0;
    this.endLine = 0;
    this.endColumn = 0;
    this._changeListener = changeListener;
}

WebInspector.TextSelectionModel.prototype = {
    setStart: function(line, column)
    {
        var oldRange = this.range();

        this.startLine = line;
        this.startColumn = column;
        this.endLine = line;
        this.endColumn = column;

        this._changeListener(oldRange, this.range());
    },

    setEnd: function(line, column)
    {
        var oldRange = this.range();

        this.endLine = line;
        this.endColumn = column;

        this._changeListener(oldRange, this.range(), this.endLine, this.endColumn);
    },

    range: function()
    {
        if (this.startLine < this.endLine || (this.startLine === this.endLine && this.startColumn <= this.endColumn))
            return new WebInspector.TextRange(this.startLine, this.startColumn, this.endLine, this.endColumn);
        else
            return new WebInspector.TextRange(this.endLine, this.endColumn, this.startLine, this.startColumn);
    },

    isEmpty: function()
    {
        return this.startLine === this.endLine && this.startColumn === this.endColumn;
    }
}

WebInspector.TextCursor = function(cursorElement)
{
    this._visible = false;
    this._cursorElement = cursorElement;
}

WebInspector.TextCursor.prototype = {
    setLocation: function(x, y)
    {
        this._x = x;
        this._y = y;
        if (this._paintInterval) {
            window.clearInterval(this._paintInterval);
            delete this._paintInterval;
        }
        this._paintInterval = window.setInterval(this._paint.bind(this, false), 500);
        this._paint(true);
    },

    hide: function()
    {
        if (this._paintInterval) {
            window.clearInterval(this._paintInterval);
            delete this._paintInterval;
        }
        this._cursorElement.style.display = "none";
    },

    setTextLineHeight: function(textLineHeight)
    {
        this._cursorElement.style.height = textLineHeight + "px";
    },

    _paint: function(force)
    {
        if (force)
            this._visible = true;
        else
            this._visible = !this._visible;
        this._cursorElement.style.left = this._x + "px";
        this._cursorElement.style.top = this._y + "px";
        this._cursorElement.style.display = this._visible ? "block" : "none";
    }
}

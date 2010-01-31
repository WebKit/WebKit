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

WebInspector.NativeTextViewer = function(textModel, platform)
{
    WebInspector.TextEditor.call(this, textModel, platform);
    this._sheet.className = "monospace";
    this._sheet.tabIndex = 0;
    this._canvas.style.zIndex = 0;
    this._createLineDivs();
}

WebInspector.NativeTextViewer.prototype = {
    // WebInspector.TextModel listener
    _textChanged: function(oldRange, newRange, oldText, newText)
    {
        this._createLineDivs();
        WebInspector.TextEditor.prototype._textChanged.call(this, oldRange, newRange, oldText, newText);
    },

    _createLineDivs: function()
    {
        this._container.removeChild(this._sheet);
        this._sheet.removeChildren();
        for (var i = 0; i < this._textModel.linesCount; ++i) {
            var lineDiv = document.createElement("div");
            lineDiv.className = "native-text-editor-line";
            lineDiv.textContent = this._textModel.line(i);
            this._sheet.appendChild(lineDiv);
            this._textModel.setAttribute(i, "line-div", lineDiv);
        }
        this._container.appendChild(this._sheet);
    },

    _updatePreferredSize: function(startLine, endLine)
    {
        // Preferred size is automatically calculated based on the line divs.
        // Only handle line numbers here.
        
        this.setCoalescingUpdate(true);
        var newLineNumberDigits = this._decimalDigits(this._textModel.linesCount);
        this._lineNumberWidth = (newLineNumberDigits + 2) * this._digitWidth;

        this._sheet.style.paddingLeft = this._textWidth + this._lineNumberWidth + "px";

        this._lineNumberDigits = newLineNumberDigits;
        this.repaintAll();

        // Changes to size can change the client area (scrollers can appear/disappear)
        this.resize();
        this.setCoalescingUpdate(false);
    },

    _scroll: function(e)
    {
        // Do instant repaint so that offset of canvas was in sync with the sheet.
        this._repaintOnScroll();
    },

    _registerMouseListeners: function()
    {
        this._sheet.addEventListener("mousedown", this._mouseDown.bind(this), false);
    },

    _registerKeyboardListeners: function()
    {
        // Noop - let browser take care of this.
    },

    _registerClipboardListeners: function()
    {
        // Noop - let browser take care of this.
    },

    _paintSelection: function()
    {
        // Noop - let browser take care of this.
    },

    _positionDivDecoration: function()
    {
        // Div decorations have fixed positions in our case.
    },

    _mouseDown: function(e)
    {
        if (e.offsetX + e.target.offsetTop >= this._lineNumberWidth && this._lineNumberDecorator)
            return;

        if (e.button === 2 || (this._isMac && e.ctrlKey))
            return;

        var location = this._caretForMouseEvent(e);
        this._lineNumberDecorator.mouseDown(location.line, e);
    },

    _contextMenu: function(e)
    {
        // Override editor's implementation to add the line's offsets.
        if (e.offsetX + e.target.offsetTop >= this._lineNumberWidth && this._lineNumberDecorator)
            return;

        var location = this._caretForMouseEvent(e);
        this._lineNumberDecorator.contextMenu(location.line, e);
    },

    _caretForMouseEvent: function(e)
    {
        // Override editor's implementation to add the line's offsets.
        var lineNumber = Math.max(0, this._offsetToLine(e.offsetY + e.target.offsetTop) - 1);
        var offset = e.offsetX + e.target.offsetLeft + this._scrollLeft - this._lineNumberWidth;
        return { line: lineNumber, column: this._columnForOffset(lineNumber, offset) };
    },

    _paintLine: function(lineNumber, lineOffset)
    {
        var divHighlighted = this._textModel.getAttribute(lineNumber, "div-highlighted");
        if (divHighlighted)
            return;

        var highlighterState = this._textModel.getAttribute(lineNumber, "highlighter-state");
        if (!highlighterState)
            return;

        var line = this._textModel.line(lineNumber);
        var element = this._textModel.getAttribute(lineNumber, "line-div");
        element.removeChildren();
 
        var plainTextStart = -1;
        for (var j = 0; j < line.length;) {
            if (j > 1000) {
                // This line is too long - do not waste cycles on minified js highlighting.
                break;
            }
            var attribute = highlighterState && highlighterState.attributes[j];
            if (!attribute || !attribute.style) {
                if (plainTextStart === -1)
                    plainTextStart = j;
                j++;
            } else {
                if (plainTextStart !== -1) {
                    element.appendChild(document.createTextNode(line.substring(plainTextStart, j)));
                    plainTextStart = -1;
                }
                element.appendChild(this._createSpan(line.substring(j, j + attribute.length), attribute.tokenType));
                j += attribute.length;
            }
        }
        if (plainTextStart !== -1)
            element.appendChild(document.createTextNode(line.substring(plainTextStart, line.length)));

        this._textModel.setAttribute(lineNumber, "div-highlighted", true);
    },

    _createSpan: function(content, className)
    {
        var span = document.createElement("span");
        span.className = "webkit-" + className;
        span.appendChild(document.createTextNode(content));
        return span;
    },

    setDivDecoration: function(lineNumber, element)
    {
        var existingElement = this._textModel.getAttribute(lineNumber, "div-decoration");
        if (existingElement && existingElement.parentNode)
            existingElement.parentNode.removeChild(existingElement);
        this._textModel.removeAttribute(lineNumber, "div-decoration");

        if (element) {
            if (lineNumber < this._textModel.linesCount - 1) {
                var lineDiv = this._textModel.getAttribute(lineNumber + 1, "line-div");
                this._sheet.insertBefore(element, lineDiv);
            } else
                this._sheet.appendChild(element);
            this._textModel.setAttribute(lineNumber, "div-decoration", element);
        }
        this.revalidateDecorationsAndPaint();
    }
}

WebInspector.NativeTextViewer.prototype.__proto__ = WebInspector.TextEditor.prototype;

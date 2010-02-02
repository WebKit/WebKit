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

WebInspector.NativeTextViewer = function(textModel, platform, url)
{
    WebInspector.TextEditor.call(this, textModel, platform);
    this._sheet.tabIndex = 0;
    this._canvas.style.zIndex = 0;
    this._createLineDivs();
    this._url = url;
    this._selectionColor = "rgb(241, 234, 0)";
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
            var text = this._textModel.line(i);
            lineDiv.textContent = text;
            if (!text)
                lineDiv.style.minHeight = this._textLineHeight + "px";
            this._sheet.appendChild(lineDiv);
            this._textModel.setAttribute(i, "line-div", lineDiv);
            this._textModel.removeAttribute(i, "div-highlighted");
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

        this._container.style.left = this._lineNumberWidth + "px";

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
        this.element.addEventListener("contextmenu", this._contextMenu.bind(this), false);
        this.element.addEventListener("mousedown", this._mouseDown.bind(this), false);
    },

    _registerKeyboardListeners: function()
    {
        // Noop - let browser take care of this.
    },

    _registerClipboardListeners: function()
    {
        // Noop - let browser take care of this.
    },

    _positionDivDecoration: function()
    {
        // Div decorations have fixed positions in our case.
    },

    _registerShortcuts: function()
    {
        // Noop.
    },

    _mouseDown: function(e)
    {
        if (e.target !== this.element || e.button === 2 || (this._isMac && e.ctrlKey))
            return;
        this._lineNumberDecorator.mouseDown(this._lineForMouseEvent(e), e);
    },

    _contextMenu: function(e)
    {
        if (e.target !== this.element)
            return;
        this._lineNumberDecorator.contextMenu(this._lineForMouseEvent(e), e);
    },

    _lineForMouseEvent: function(e)
    {
        return Math.max(0, this._offsetToLine(e.offsetY + this._scrollTop) - 1);
    },

    _lineHeight: function(lineNumber)
    {
        // Use cached value first.
        if (this._lineOffsetsCache[lineNumber + 1])
            return this._lineOffsetsCache[lineNumber + 1] - this._lineOffsetsCache[lineNumber];

        // Get metrics from the browser.
        var element = this._textModel.getAttribute(lineNumber, "line-div");
        if (lineNumber + 1 < this._textModel.linesCount) {
            var nextElement = this._textModel.getAttribute(lineNumber + 1, "line-div");
            return nextElement.offsetTop - element.offsetTop;
        }
        return element.parentElement.offsetHeight - element.offsetTop;
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
        if (className === "html-resource-link" || className === "html-external-link")
            return this._createLink(content, className === "html-external-link");

        var span = document.createElement("span");
        span.className = "webkit-" + className;
        span.appendChild(document.createTextNode(content));
        return span;
    },

    _createLink: function(content, isExternal)
    {
        var quote = content.charAt(0);
        if (content.length > 1 && (quote === "\"" ||   quote === "'"))
            content = content.substring(1, content.length - 1);
        else
            quote = null;

        var a = WebInspector.linkifyURLAsNode(this._rewriteHref(content), content, null, isExternal);
        var span = document.createElement("span");
        span.className = "webkit-html-attribute-value";
        if (quote)
            span.appendChild(document.createTextNode(quote));
        span.appendChild(a);
        if (quote)
            span.appendChild(document.createTextNode(quote));
        return span;
    },

    _rewriteHref: function(hrefValue, isExternal)
    {
        if (!this._url || !hrefValue || hrefValue.indexOf("://") > 0)
            return hrefValue;
        return WebInspector.completeURL(this._url, hrefValue);
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
    },

    initFontMetrics: function()
    {
        WebInspector.TextEditor.prototype.initFontMetrics.call(this);
        for (var i = 0; i < this._textModel.linesCount; ++i) {
            var lineDiv = this._textModel.getAttribute(i, "line-div");
            if (!this._textModel.line(i))
                lineDiv.style.minHeight = this._textLineHeight + "px";
        }
    }
}

WebInspector.NativeTextViewer.prototype.__proto__ = WebInspector.TextEditor.prototype;

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

importScript("cm/codemirror.js");
importScript("cm/css.js");
importScript("cm/javascript.js");
importScript("cm/xml.js");
importScript("cm/htmlmixed.js");

/**
 * @constructor
 * @extends {WebInspector.View}
 * @implements {WebInspector.TextEditor}
 * @param {?string} url
 * @param {WebInspector.TextEditorDelegate} delegate
 */
WebInspector.CodeMirrorTextEditor = function(url, delegate)
{
    WebInspector.View.call(this);
    this._delegate = delegate;
    this._url = url;

    this.registerRequiredCSS("codemirror.css");
    this.registerRequiredCSS("cmdevtools.css");

    this._codeMirror = window.CodeMirror(this.element, {
        lineNumbers: true,
        fixedGutter: true,
        onChange: this._onChange.bind(this),
        onGutterClick: this._onGutterClick.bind(this)
    });

    this._lastRange = this.range();

    this.element.firstChild.addStyleClass("source-code");
    this.element.firstChild.addStyleClass("fill");
}

WebInspector.CodeMirrorTextEditor.prototype = {
    /**
     * @param {string} mimeType
     */
    set mimeType(mimeType)
    {
        this._codeMirror.setOption("mode", mimeType);
        switch(mimeType) {
            case "text/html": this._codeMirror.setOption("theme", "web-inspector-html"); break;
            case "text/css": this._codeMirror.setOption("theme", "web-inspector-css"); break;
            case "text/javascript": this._codeMirror.setOption("theme", "web-inspector-js"); break;
        }
    },

    /**
     * @param {boolean} readOnly
     */
    setReadOnly: function(readOnly)
    {
        this._codeMirror.setOption("readOnly", readOnly);
    },

    /**
     * @return {boolean}
     */
    readOnly: function()
    {
        return !!this._codeMirror.getOption("readOnly");
    },

    /**
     * @return {Element}
     */
    defaultFocusedElement: function()
    {
        return this.element.firstChild;
    },

    focus: function()
    {
        this._codeMirror.focus();
    },

    beginUpdates: function() { },

    endUpdates: function() { },

    /**
     * @param {number} lineNumber
     */
    revealLine: function(lineNumber)
    {
        this._codeMirror.setCursor({ line: lineNumber, ch: 0 });
        var coords = this._codeMirror.cursorCoords();
        this._codeMirror.scrollTo(coords.x, coords.y);
    },

    _onGutterClick: function(instance, lineNumber, event)
    {
        this.dispatchEventToListeners(WebInspector.TextEditor.Events.GutterClick, { lineNumber: lineNumber, event: event });
    },

    /**
     * @param {number} lineNumber
     * @param {boolean} disabled
     * @param {boolean} conditional
     */
    addBreakpoint: function(lineNumber, disabled, conditional)
    {
        var className = "cm-breakpoint" + (disabled ? " cm-breakpoint-disabled" : "") + (conditional ? " cm-breakpoint-conditional" : "");
        this._codeMirror.setMarker(lineNumber, null, className);
    },

    /**
     * @param {number} lineNumber
     */
    removeBreakpoint: function(lineNumber)
    {
        this._codeMirror.clearMarker(lineNumber);
    },

    /**
     * @param {number} lineNumber
     */
    setExecutionLine: function(lineNumber)
    {
        this._executionLine = this._codeMirror.getLineHandle(lineNumber)
        this._codeMirror.setLineClass(this._executionLine, null, "cm-execution-line");
    },

    clearExecutionLine: function()
    {
        if (this._executionLine)
            this._codeMirror.setLineClass(this._executionLine, null, null);
        delete this._executionLine;
    },

    /**
     * @param {number} lineNumber
     * @param {Element} element
     */
    addDecoration: function(lineNumber, element)
    {
        // TODO implement so that it doesn't hide context code
    },

    /**
     * @param {number} lineNumber
     * @param {Element} element
     */
    removeDecoration: function(lineNumber, element)
    {
        // TODO implement so that it doesn't hide context code
    },

    /**
     * @param {WebInspector.TextRange} range
     */
    markAndRevealRange: function(range)
    {
        if (range)
            this.setSelection(range);
    },

    /**
     * @param {number} lineNumber
     */
    highlightLine: function(lineNumber)
    {
        this.clearLineHighlight();
        this._highlightedLine = this._codeMirror.getLineHandle(lineNumber);
        this._codeMirror.setLineClass(this._highlightedLine, null, "cm-highlight");
        this._clearHighlightTimeout = setTimeout(this.clearLineHighlight.bind(this), 2000);
    },

    clearLineHighlight: function()
    {
        if (this._clearHighlightTimeout)
            clearTimeout(this._clearHighlightTimeout);
        delete this._clearHighlightTimeout;

        if (this._highlightedLine)
            this._codeMirror.setLineClass(this._highlightedLine, null, null);
        delete this._highlightedLine;
    },

    /**
     * @return {Array.<Element>}
     */
    elementsToRestoreScrollPositionsFor: function()
    {
        return [];
    },

    /**
     * @param {WebInspector.TextEditor} textEditor
     */
    inheritScrollPositions: function(textEditor)
    {
    },

    onResize: function()
    {
        this._codeMirror.refresh();
    },

    /**
     * @param {WebInspector.TextRange} range
     * @param {string} text
     * @return {WebInspector.TextRange}
     */
    editRange: function(range, text)
    {
        var pos = this._toPos(range);
        this._codeMirror.replaceRange(text, pos.start, pos.end);
        var newRange = this._toRange(pos.start, this._codeMirror.posFromIndex(this._codeMirror.indexFromPos(pos.start) + text.length));
        this._delegate.onTextChanged(range, newRange);
        return newRange;
    },

    _onChange: function()
    {
        var newRange = this.range();
        this._delegate.onTextChanged(this._lastRange, newRange);
        this._lastRange = newRange;
    },

    /**
     * @param {number} lineNumber
     */
    scrollToLine: function(lineNumber)
    {
        this._codeMirror.setCursor({line:lineNumber, ch:0});
    },

    /**
     * @return {WebInspector.TextRange}
     */
    selection: function(textRange)
    {
        var start = this._codeMirror.cursorCoords(true);
        var end = this._codeMirror.cursorCoords(false);

        if (start.line > end.line || (start.line == end.line && start.ch > end.ch))
            return this._toRange(end, start);

        return this._toRange(start, end);
    },

    /**
     * @return {WebInspector.TextRange?}
     */
    lastSelection: function()
    {
        return this._lastSelection;
    },

    /**
     * @param {WebInspector.TextRange} textRange
     */
    setSelection: function(textRange)
    {
        this._lastSelection = textRange;
        var pos = this._toPos(textRange);
        this._codeMirror.setSelection(pos.start, pos.end);
    },

    /**
     * @param {string} text
     */
    setText: function(text)
    {
        this._codeMirror.setValue(text);
    },

    /**
     * @return {string}
     */
    text: function()
    {
        return this._codeMirror.getValue();
    },

    /**
     * @return {WebInspector.TextRange}
     */
    range: function()
    {
        var lineCount = this.linesCount;
        var lastLine = this._codeMirror.getLine(lineCount - 1);
        return this._toRange({ line: 0, ch: 0 }, { line: lineCount - 1, ch: lastLine.length });
    },

    /**
     * @param {number} lineNumber
     * @return {string}
     */
    line: function(lineNumber)
    {
        return this._codeMirror.getLine(lineNumber);
    },

    /**
     * @return {number}
     */
    get linesCount()
    {
        return this._codeMirror.lineCount();
    },

    /**
     * @param {number} line
     * @param {string} name
     * @param {Object?} value
     */
    setAttribute: function(line, name, value)
    {
        var handle = this._codeMirror.getLineHandle(line);
        if (handle.attributes === undefined) handle.attributes = {};
        handle.attributes[name] = value;
    },

    /**
     * @param {number} line
     * @param {string} name
     * @return {Object|null} value
     */
    getAttribute: function(line, name)
    {
        var handle = this._codeMirror.getLineHandle(line);
        return handle.attributes && handle.attributes[name] !== undefined ? handle.attributes[name] : null;
    },

    /**
     * @param {number} line
     * @param {string} name
     */
    removeAttribute: function(line, name)
    {
        var handle = this._codeMirror.getLineHandle(line);
        if (handle && handle.attributes)
            delete handle.attributes[name];
    },

    _toPos: function(range)
    {
        return {
            start: {line: range.startLine, ch: range.startColumn},
            end: {line: range.endLine, ch: range.endColumn}
        }
    },

    _toRange: function(start, end)
    {
        return new WebInspector.TextRange(start.line, start.ch, end.line, end.ch);
    }
}

WebInspector.CodeMirrorTextEditor.prototype.__proto__ = WebInspector.View.prototype;

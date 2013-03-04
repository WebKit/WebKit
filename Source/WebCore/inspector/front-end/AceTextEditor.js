/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

importScript("ace/ace.js");


/**
 * @constructor
 * @extends {WebInspector.View}
 * @implements {WebInspector.TextEditor}
 * @param {?string} url
 * @param {WebInspector.TextEditorDelegate} delegate
 */

WebInspector.AceTextEditor = function(url, delegate)
{
    WebInspector.View.call(this);
    this._delegate = delegate;
    this._url = url;
    this.element.className = "ace-editor-container";

    var prefix = window.flattenImports ? "" : "ace/";
    ace.config.setModuleUrl("ace/mode/javascript", prefix + "mode_javascript.js");
    ace.config.setModuleUrl("ace/mode/javascript", prefix + "mode_css.js");
    ace.config.setModuleUrl("ace/mode/javascript", prefix + "mode_html.js");
    ace.config.setModuleUrl("ace/theme/textmate", prefix + "theme_textmate.js");
    this._aceEditor = window.ace.edit(this.element);

    this._aceEditor.setShowFoldWidgets(false);
    this._aceEditor.on("gutterclick", this._gutterClick.bind(this));
    this._aceEditor.on("change", this._onTextChange.bind(this));
    this._aceEditor.setHighlightActiveLine(false);
    this._aceEditor.session.setUseWorker(false);
    this.registerRequiredCSS("ace/acedevtools.css");
}

WebInspector.AceTextEditor.prototype = {

    _onTextChange: function(event)
    {
        this._delegate.onTextChanged(null, null);
    },

    _gutterClick: function(event)
    {
        var lineNumber = parseInt(event.domEvent.target.textContent) - 1;
        this.dispatchEventToListeners(WebInspector.TextEditor.Events.GutterClick, { lineNumber: lineNumber, event: event.domEvent });
    },

    /**
     * @param {string} mimeType
     */
    set mimeType(mimeType)
    {
        switch(mimeType) {
        case "text/html":
            this._aceEditor.getSession().setMode("ace/mode/html");
            break;
        case "text/css":
            this._aceEditor.getSession().setMode("ace/mode/css");
            break;
        case "text/javascript":
            this._aceEditor.getSession().setMode("ace/mode/javascript");
            break;
        }
    },

    /**
     * @param {boolean} readOnly
     */
    setReadOnly: function(readOnly)
    {
        this._aceEditor.setReadOnly(readOnly);
    },

    /**
     * @return {boolean}
     */
    readOnly: function()
    {
        return this._aceEditor.getReadOnly();
    },

    focus: function()
    {
        this._aceEditor.focus();
    },

    /**
     * @return {Element}
     */
    defaultFocusedElement: function()
    {
        return this.element.firstChild;
    },

    /**
     * @param {string} regex
     * @param {string} cssClass
     * @return {WebInspector.TextEditorMainPanel.HighlightDescriptor}
     */
    highlightRegex: function(regex, cssClass)
    {
        console.log("aceEditor.highlightRegex not implemented");
    },

    /**
     * @param {WebInspector.TextRange} range
     * @param {string} cssClass
     */
    highlightRange: function(range, cssClass)
    {
        console.log("aceEditor.highlightRange not implemented");
    },

    /**
     * @param {WebInspector.TextEditorMainPanel.HighlightDescriptor} highlightDescriptor
     */
    removeHighlight: function(highlightDescriptor)
    {
        console.log("aceEditor.removeHighlight not implemented");
    },

    /**
     * @param {number} lineNumber
     */
    revealLine: function(lineNumber) {
        this._aceEditor.scrollToLine(lineNumber, false, true);
    },

    /**
     * @param {number} lineNumber
     * @param {boolean} disabled
     * @param {boolean} conditional
     */
    addBreakpoint: function(lineNumber, disabled, conditional)
    {
        this._aceEditor.getSession().setBreakpoint(lineNumber, "webkit-breakpoint");
    },

    /**
     * @param {number} lineNumber
     */
    removeBreakpoint: function(lineNumber)
    {
        this._aceEditor.getSession().clearBreakpoint(lineNumber);
    },

    /**
     * @param {number} lineNumber
     */
    setExecutionLine: function(lineNumber)
    {
        console.log("aceEditor.setExecutionLine not implemented");
    },

    /**
     * @param {WebInspector.TextRange} range
     * @return {string}
     */
    copyRange: function(range)
    {
        console.log("aceEditor.copyRange not implemented");
        return "";
    },

    clearExecutionLine: function()
    {
        console.log("aceEditor.clearExecutionLine not implemented");
    },

    /**
     * @param {number} lineNumber
     * @param {Element} element
     */
    addDecoration: function(lineNumber, element)
    {
        console.log("aceEditor.addDecoration not implemented");
    },

    /**
     * @param {number} lineNumber
     * @param {Element} element
     */
    removeDecoration: function(lineNumber, element)
    {
        console.log("aceEditor.removeDecoration not implemented");
    },

    /**
     * @param {WebInspector.TextRange} range
     */
    markAndRevealRange: function(range)
    {
        console.log("aceEditor.markAndRevealRange not implemented");
    },

    /**
     * @param {number} lineNumber
     */
    highlightLine: function(lineNumber)
    {
        console.log("aceEditor.highlightLine not implemented");
    },

    clearLineHighlight: function() {
        console.log("aceEditor.clearLineHighlight not implemented");
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
        console.log("aceEditor.inheritScrollPositions not implemented");
    },

    beginUpdates: function() { },

    endUpdates: function() { },

    onResize: function() { },

    /**
     * @param {WebInspector.TextRange} range
     * @param {string} text
     * @return {WebInspector.TextRange}
     */
    editRange: function(range, text)
    {
        console.log("aceEditor.editRange not implemented");
    },

    /**
     * @param {number} lineNumber
     */
    scrollToLine: function(lineNumber)
    {
        this._aceEditor.scrollToLine(lineNumber, false, true);
    },

    /**
     * @return {WebInspector.TextRange}
     */
    selection: function()
    {
        console.log("aceEditor.selection not implemented");
    },

    /**
     * @return {WebInspector.TextRange?}
     */
    lastSelection: function()
    {
        console.log("aceEditor.lastSelection not implemented");
    },

    /**
     * @param {WebInspector.TextRange} textRange
     */
    setSelection: function(textRange)
    {
        console.log("aceEditor.setSelection not implemented");
    },

    /**
     * @param {string} text
     */
    setText: function(text)
    {
        this._aceEditor.getSession().setValue(text);
    },

    /**
     * @return {string}
     */
    text: function()
    {
        return this._aceEditor.getSession().getValue();
    },

    /**
     * @return {WebInspector.TextRange}
     */
    range: function()
    {
        console.log("aceEditor.range not implemented");
    },

    /**
     * @param {number} lineNumber
     * @return {string}
     */
    line: function(lineNumber)
    {
        return this._aceEditor.getSession().getLine(lineNumber);
    },

    /**
     * @return {number}
     */
    get linesCount() {
        return this._aceEditor.getSession().getLength();
    },

    /**
     * @param {number} line
     * @param {string} name
     * @param {Object?} value
     */
    setAttribute: function(line, name, value)
    {
        console.log("aceEditor.setAttribute not implemented");
    },

    /**
     * @param {number} line
     * @param {string} name
     * @return {Object|null} value
     */
    getAttribute: function(line, name)
    {
        console.log("aceEditor.getAttribute not implemented");
    },

    /**
     * @param {number} line
     * @param {string} name
     */
    removeAttribute: function(line, name)
    {
        console.log("aceEditor.removeAttribute not implemented");
    },

    wasShown: function() { },

    __proto__: WebInspector.View.prototype
}

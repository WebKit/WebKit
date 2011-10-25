/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.SourceFrame}
 * @param {WebInspector.SourceFrameDelegate} delegate
 * @param {WebInspector.DebuggerPresentationModel} model
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.JavaScriptSourceFrame = function(delegate, model, uiSourceCode)
{
    // FIXME: move all SourceFrame methods related to JavaScript debugging here and
    // get rid of SourceFrame._delegate.
    WebInspector.SourceFrame.call(this, delegate, uiSourceCode.url);

    this._model = model;
    this._popoverObjectGroup = "popover";

    uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this.contentChanged, this);
}

WebInspector.JavaScriptSourceFrame.prototype = {
    shouldShowPopover: function(element)
    {
        if (!this._model.paused)
            return false;
        if (!element.enclosingNodeOrSelfWithClass("webkit-line-content"))
            return false;

        // We are interested in identifiers and "this" keyword.
        if (element.hasStyleClass("webkit-javascript-keyword"))
            return element.textContent === "this";

        return element.hasStyleClass("webkit-javascript-ident");
    },

    onShowPopover: function(element, showCallback)
    {
        if (!this.readOnly) {
            this.popoverHelper.hidePopover();
            return;
        }
        this._highlightElement = this._highlightExpression(element);

        function showObjectPopover(result, wasThrown)
        {
            if (!this._model.paused) {
                this.popoverHelper.hidePopover();
                return;
            }
            showCallback(WebInspector.RemoteObject.fromPayload(result), wasThrown);
            // Popover may have been removed by showCallback().
            if (this._highlightElement)
                this._highlightElement.addStyleClass("source-frame-eval-expression");
        }

        var selectedCallFrame = this._model.selectedCallFrame;
        selectedCallFrame.evaluate(this._highlightElement.textContent, this._popoverObjectGroup, false, false, showObjectPopover.bind(this));
    },

    onHidePopover: function()
    {
        // Replace higlight element with its contents inplace.
        var highlightElement = this._highlightElement;
        if (!highlightElement)
            return;
        var parentElement = highlightElement.parentElement;
        var child = highlightElement.firstChild;
        while (child) {
            var nextSibling = child.nextSibling;
            parentElement.insertBefore(child, highlightElement);
            child = nextSibling;
        }
        parentElement.removeChild(highlightElement);
        delete this._highlightElement;
        RuntimeAgent.releaseObjectGroup(this._popoverObjectGroup);
    },

    _highlightExpression: function(element)
    {
        // Collect tokens belonging to evaluated expression.
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
        return container;
    },

    populateTextAreaContextMenu: function(contextMenu)
    {
        var selection = window.getSelection();
        if (selection.type !== "Range" || selection.isCollapsed)
            return;
        contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles() ? "Add to watch" : "Add to Watch"), this._delegate.addToWatch.bind(this._delegate, selection.toString()));
    }
}

WebInspector.JavaScriptSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;

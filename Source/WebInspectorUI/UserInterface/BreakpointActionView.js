/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.BreakpointActionView = function(action, delegate, omitFocus)
{
    WebInspector.Object.call(this);

    console.assert(action);
    console.assert(delegate);
    console.assert(DebuggerAgent.BreakpointActionType);

    this._action = action;
    this._delegate = delegate;

    this._element = document.createElement("div");
    this._element.className = "breakpoint-action-block";

    var header = this._element.appendChild(document.createElement("div"));
    header.className = "breakpoint-action-block-header";

    var picker = header.appendChild(document.createElement("select"));
    picker.addEventListener("change", this._pickerChanged.bind(this));

    for (var key in WebInspector.BreakpointAction.Type) {
        var type = WebInspector.BreakpointAction.Type[key];
        var option = document.createElement("option");
        option.textContent = WebInspector.BreakpointActionView.displayStringForType(type);
        option.selected = this._action.type === type;
        option.value = type;
        picker.add(option);
    }

    var appendActionButton = header.appendChild(document.createElement("button"));
    appendActionButton.className = "breakpoint-action-append-button";
    appendActionButton.addEventListener("click", this._appendActionButtonClicked.bind(this));
    appendActionButton.title = WebInspector.UIString("Add new breakpoint action after this action");

    var removeActionButton = header.appendChild(document.createElement("button"));
    removeActionButton.className = "breakpoint-action-remove-button";
    removeActionButton.addEventListener("click", this._removeActionButtonClicked.bind(this));
    removeActionButton.title = WebInspector.UIString("Remove this breakpoint action");

    this._bodyElement = this._element.appendChild(document.createElement("div"));
    this._bodyElement.className = "breakpoint-action-block-body";

    this._updateBody(omitFocus);
};

WebInspector.BreakpointActionView.displayStringForType = function(type)
{
    switch (type) {
    case WebInspector.BreakpointAction.Type.Log:
        return WebInspector.UIString("Log Message");
    case WebInspector.BreakpointAction.Type.Evaluate:
        return WebInspector.UIString("Evaluate JavaScript");
    case WebInspector.BreakpointAction.Type.Sound:
        return WebInspector.UIString("Play Sound");
    case WebInspector.BreakpointAction.Type.Probe:
        return WebInspector.UIString("Probe Expression");
    default:
        console.assert(false);
        return "";
    }
}

WebInspector.BreakpointActionView.prototype = {
    constructor: WebInspector.BreakpointActionView,

    // Public

    get action()
    {
        return this._action;
    },

    get element()
    {
        return this._element;
    },

    // Private

    _pickerChanged: function(event)
    {
        var newType = event.target.value;
        this._action = this._action.breakpoint.recreateAction(newType, this._action);
        this._updateBody();
        this._delegate.breakpointActionViewResized(this);
    },

    _appendActionButtonClicked: function(event)
    {
        var newAction = this._action.breakpoint.createAction(WebInspector.Breakpoint.DefaultBreakpointActionType, this._action);
        this._delegate.breakpointActionViewAppendActionView(this, newAction);
    },

    _removeActionButtonClicked: function(event)
    {
        this._action.breakpoint.removeAction(this._action);
        this._delegate.breakpointActionViewRemoveActionView(this);
    },

    _updateBody: function(omitFocus)
    {
        this._bodyElement.removeChildren();

        switch (this._action.type) {
        case WebInspector.BreakpointAction.Type.Log:
            this._bodyElement.hidden = false;

            var input = this._bodyElement.appendChild(document.createElement("input"));
            input.placeholder = WebInspector.UIString("Message");
            input.addEventListener("change", this._logInputChanged.bind(this));
            input.value = this._action.data || "";
            input.spellcheck = false;
            if (!omitFocus)
                setTimeout(function() { input.focus(); }, 0);

            break;

        case WebInspector.BreakpointAction.Type.Evaluate:
        case WebInspector.BreakpointAction.Type.Probe:
            this._bodyElement.hidden = false;

            var editorElement = this._bodyElement.appendChild(document.createElement("div"));
            editorElement.classList.add("breakpoint-action-eval-editor");
            editorElement.classList.add(WebInspector.SyntaxHighlightedStyleClassName);

            this._codeMirror = CodeMirror(editorElement, {
                lineWrapping: true,
                mode: "text/javascript",
                indentWithTabs: true,
                indentUnit: 4,
                matchBrackets: true,
                value: this._action.data || "",
            });

            this._codeMirror.on("viewportChange", this._codeMirrorViewportChanged.bind(this));
            this._codeMirror.on("blur", this._codeMirrorBlurred.bind(this));

            var completionController = new WebInspector.CodeMirrorCompletionController(this._codeMirror);
            completionController.addExtendedCompletionProvider("javascript", WebInspector.javaScriptRuntimeCompletionProvider);

            // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
            setTimeout(function() {
                this._codeMirror.refresh();
                if (!omitFocus)
                    this._codeMirror.focus();
            }.bind(this), 0);

            break;

        case WebInspector.BreakpointAction.Type.Sound:
            this._bodyElement.hidden = true;
            break;

        default:
            console.assert(false);
            this._bodyElement.hidden = true;
            break;
        }
    },

    _logInputChanged: function(event)
    {
        this._action.data = event.target.value;
    },

    _codeMirrorBlurred: function(event)
    {
        this._action.data = this._codeMirror.getValue();
    },

    _codeMirrorViewportChanged: function(event)
    {
        this._delegate.breakpointActionViewResized(this);
    }
};

WebInspector.BreakpointActionView.prototype.__proto__ = WebInspector.Object.prototype;

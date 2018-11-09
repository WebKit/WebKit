/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.BreakpointActionView = class BreakpointActionView extends WI.Object
{
    constructor(action, delegate, omitFocus)
    {
        super();

        console.assert(action);
        console.assert(delegate);

        this._action = action;
        this._delegate = delegate;

        this._element = document.createElement("div");
        this._element.className = "breakpoint-action-block";

        var header = this._element.appendChild(document.createElement("div"));
        header.className = "breakpoint-action-block-header";

        var picker = header.appendChild(document.createElement("select"));
        picker.addEventListener("change", this._pickerChanged.bind(this));

        for (var key in WI.BreakpointAction.Type) {
            var type = WI.BreakpointAction.Type[key];
            var option = document.createElement("option");
            option.textContent = WI.BreakpointActionView.displayStringForType(type);
            option.selected = this._action.type === type;
            option.value = type;
            picker.add(option);
        }

        let buttonContainerElement = header.appendChild(document.createElement("div"));
        buttonContainerElement.classList.add("breakpoint-action-button-container");

        let appendActionButton = buttonContainerElement.appendChild(document.createElement("button"));
        appendActionButton.className = "breakpoint-action-append-button";
        appendActionButton.addEventListener("click", this._appendActionButtonClicked.bind(this));
        appendActionButton.title = WI.UIString("Add new breakpoint action after this action");

        let removeActionButton = buttonContainerElement.appendChild(document.createElement("button"));
        removeActionButton.className = "breakpoint-action-remove-button";
        removeActionButton.addEventListener("click", this._removeAction.bind(this));
        removeActionButton.title = WI.UIString("Remove this breakpoint action");

        this._bodyElement = this._element.appendChild(document.createElement("div"));
        this._bodyElement.className = "breakpoint-action-block-body";

        this._updateBody(omitFocus);
    }

    // Static

    static displayStringForType(type)
    {
        switch (type) {
        case WI.BreakpointAction.Type.Log:
            return WI.UIString("Log Message");
        case WI.BreakpointAction.Type.Evaluate:
            return WI.UIString("Evaluate JavaScript");
        case WI.BreakpointAction.Type.Sound:
            return WI.UIString("Play Sound");
        case WI.BreakpointAction.Type.Probe:
            return WI.UIString("Probe Expression");
        default:
            console.assert(false);
            return "";
        }
    }

    // Public

    get action()
    {
        return this._action;
    }

    get element()
    {
        return this._element;
    }

    // Private

    _pickerChanged(event)
    {
        var newType = event.target.value;
        this._action = this._action.breakpoint.recreateAction(newType, this._action);
        this._updateBody();
        this._delegate.breakpointActionViewResized(this);
    }

    _appendActionButtonClicked(event)
    {
        var newAction = this._action.breakpoint.createAction(this._action.type, this._action);
        this._delegate.breakpointActionViewAppendActionView(this, newAction);
    }

    _removeAction()
    {
        this._action.breakpoint.removeAction(this._action);
        this._delegate.breakpointActionViewRemoveActionView(this);
    }

    _updateBody(omitFocus)
    {
        this._bodyElement.removeChildren();

        switch (this._action.type) {
        case WI.BreakpointAction.Type.Log:
            this._bodyElement.hidden = false;

            var input = this._bodyElement.appendChild(document.createElement("input"));
            input.placeholder = WI.UIString("Message");
            input.addEventListener("change", this._logInputChanged.bind(this));
            input.value = this._action.data || "";
            input.spellcheck = false;
            if (!omitFocus)
                setTimeout(function() { input.focus(); }, 0);

            var descriptionElement = this._bodyElement.appendChild(document.createElement("div"));
            descriptionElement.classList.add("description");
            descriptionElement.setAttribute("dir", "ltr");
            descriptionElement.textContent = WI.UIString("${expr} = expression");
            break;

        case WI.BreakpointAction.Type.Evaluate:
        case WI.BreakpointAction.Type.Probe:
            this._bodyElement.hidden = false;

            var editorElement = this._bodyElement.appendChild(document.createElement("div"));
            editorElement.classList.add("breakpoint-action-eval-editor");
            editorElement.classList.add(WI.SyntaxHighlightedStyleClassName);

            this._codeMirror = WI.CodeMirrorEditor.create(editorElement, {
                lineWrapping: true,
                mode: "text/javascript",
                indentWithTabs: true,
                indentUnit: 4,
                matchBrackets: true,
                value: this._action.data || "",
            });

            this._codeMirror.on("viewportChange", this._codeMirrorViewportChanged.bind(this));
            this._codeMirror.on("blur", this._codeMirrorBlurred.bind(this));

            this._codeMirrorViewport = {from: null, to: null};

            var completionController = new WI.CodeMirrorCompletionController(this._codeMirror);
            completionController.addExtendedCompletionProvider("javascript", WI.javaScriptRuntimeCompletionProvider);

            // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
            setTimeout(() => {
                this._codeMirror.refresh();
                if (!omitFocus)
                    this._codeMirror.focus();
            }, 0);

            break;

        case WI.BreakpointAction.Type.Sound:
            this._bodyElement.hidden = true;
            break;

        default:
            console.assert(false);
            this._bodyElement.hidden = true;
            break;
        }
    }

    _logInputChanged(event)
    {
        this._action.data = event.target.value;
    }

    _codeMirrorBlurred(event)
    {
        // Throw away the expression if it's just whitespace.
        this._action.data = (this._codeMirror.getValue() || "").trim();
    }

    _codeMirrorViewportChanged(event, from, to)
    {
        if (this._codeMirrorViewport.from === from && this._codeMirrorViewport.to === to)
            return;

        this._codeMirrorViewport.from = from;
        this._codeMirrorViewport.to = to;
        this._delegate.breakpointActionViewResized(this);
    }
};

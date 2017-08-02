/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.XHRBreakpointPopover = class XHRBreakpointPopover extends WI.Popover
{
    constructor(delegate)
    {
        super(delegate);

        this._result = WI.InputPopover.Result.None;
        this._type = WI.XHRBreakpoint.Type.Text;
        this._value = null;

        this._codeMirror = null;
        this._targetElement = null;
        this._preferredEdges = null;

        this.windowResizeHandler = this._presentOverTargetElement.bind(this);
    }

    // Public

    get result() { return this._result; }
    get type() { return this._type; }
    get value() { return this._value; }

    show(targetElement, preferredEdges)
    {
        this._targetElement = targetElement;
        this._preferredEdges = preferredEdges;

        let contentElement = document.createElement("div");
        contentElement.classList.add("xhr-breakpoint-content");

        let label = document.createElement("div");
        label.classList.add("label");
        label.textContent = WI.UIString("Break on request with URL:");

        let editorWrapper = document.createElement("div");
        editorWrapper.classList.add("editor-wrapper");

        let selectElement = document.createElement("select");

        function addOption(text, value)
        {
            let optionElement = document.createElement("option");
            optionElement.textContent = text;
            optionElement.value = value;
            selectElement.append(optionElement);
        }

        addOption(WI.UIString("Containing"), WI.XHRBreakpoint.Type.Text);
        addOption(WI.UIString("Matching"), WI.XHRBreakpoint.Type.RegularExpression);

        selectElement.value = this._type;
        selectElement.addEventListener("change", (event) => {
            this._type = event.target.value;
            this._updateEditor();
            this._codeMirror.focus();
        });

        editorWrapper.append(selectElement, this._createEditor());
        contentElement.append(label, editorWrapper);

        this.content = contentElement;

        this._presentOverTargetElement();
    }

    // Private

    _createEditor()
    {
        let editorElement = document.createElement("div");
        editorElement.classList.add("editor");

        this._codeMirror = WI.CodeMirrorEditor.create(editorElement, {
            lineWrapping: false,
            matchBrackets: false,
            scrollbarStyle: null,
            value: "",
        });

        this._codeMirror.addKeyMap({
            "Enter": () => {
                this._result = WI.InputPopover.Result.Committed;
                this._value = this._codeMirror.getValue().trim();
                this.dismiss();
            },
        });

        this._updateEditor();

        return editorElement;
    }

    _updateEditor()
    {
        let placeholder;
        let mimeType;
        if (this._type === WI.XHRBreakpoint.Type.Text) {
            placeholder = WI.UIString("Text");
            mimeType = "text/plain";
        } else {
            placeholder = WI.UIString("Regular Expression");
            mimeType = "text/x-regex";
        }

        this._codeMirror.setOption("mode", mimeType);
        this._codeMirror.setOption("placeholder", placeholder);
    }

    _presentOverTargetElement()
    {
        if (!this._targetElement)
            return;

        let targetFrame = WI.Rect.rectFromClientRect(this._targetElement.getBoundingClientRect());
        this.present(targetFrame, this._preferredEdges);

        // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
        setTimeout(() => {
            this._codeMirror.refresh();
            this._codeMirror.focus();
            this.update();
        }, 0);
    }
};

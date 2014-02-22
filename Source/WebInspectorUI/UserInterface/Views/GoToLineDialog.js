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

WebInspector.GoToLineDialog = function()
{
    WebInspector.Object.call(this);

    this._element = document.createElement("div");
    this._element.className = WebInspector.GoToLineDialog.StyleClassName;

    var field = this._element.appendChild(document.createElement("div"));

    this._input = field.appendChild(document.createElement("input"));
    this._input.type = "text";
    this._input.placeholder = WebInspector.UIString("Line Number");
    this._input.spellcheck = false;

    this._clearIcon = field.appendChild(document.createElement("img"));

    this._input.addEventListener("input", this);
    this._input.addEventListener("keydown", this);
    this._input.addEventListener("blur", this);
    this._clearIcon.addEventListener("mousedown", this);
    this._clearIcon.addEventListener("click", this);
}

WebInspector.GoToLineDialog.StyleClassName = "go-to-line-dialog";
WebInspector.GoToLineDialog.NonEmptyClassName = "non-empty";

WebInspector.GoToLineDialog.prototype = {
    constructor: WebInspector.GoToLineDialog,

    __proto__: WebInspector.Object.prototype,

    // Public

    present: function(parent)
    {
        parent.appendChild(this._element);
        this._input.focus();
        this._clear();
    },

    dismiss: function()
    {
        var parent = this._element.parentNode;
        if (!parent)
            return;

        parent.removeChild(this._element);

        if (this.delegate && typeof this.delegate.goToLineDialogWasDismissed === "function")
            this.delegate.goToLineDialogWasDismissed(this);
    },

    // Protected

    handleEvent: function(event)
    {
        switch (event.type) {
        case "input":
            this._handleInputEvent(event);
            break;
        case "keydown":
            this._handleKeydownEvent(event);
            break;
        case "blur":
            this._handleBlurEvent(event);
            break;
        case "mousedown":
            this._handleMousedownEvent(event);
            break;
        case "click":
            this._handleClickEvent(event);
            break;
        }
    },

    // Private

    _handleInputEvent: function(event)
    {
        if (this._input.value === "")
            this._element.classList.remove(WebInspector.GoToLineDialog.NonEmptyClassName);
        else
            this._element.classList.add(WebInspector.GoToLineDialog.NonEmptyClassName);
    },

    _handleKeydownEvent: function(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Key.Escape.keyCode) {
            if (this._input.value === "")
                this.dismiss();
            else
                this._clear();
        } else if (event.keyCode === WebInspector.KeyboardShortcut.Key.Enter.keyCode) {
            var value = parseInt(this._input.value, 10);

            var valueIsValid = false;
            if (this.delegate && typeof this.delegate.isGoToLineDialogValueValid === "function")
                valueIsValid = this.delegate.isGoToLineDialogValueValid(this, value);
            
            if (valueIsValid && this.delegate && typeof this.delegate.goToLineDialogValueWasValidated === "function") {
                this.delegate.goToLineDialogValueWasValidated(this, value);
                this.dismiss();
                return;
            }

            this._input.select();
            InspectorFrontendHost.beep();
        }
    },

    _handleBlurEvent: function(event)
    {
        this.dismiss();
    },

    _handleMousedownEvent: function(event)
    {
        this._input.select();
        // This ensures we don't get a "blur" event triggered for the text field
        // which would end up dimissing the dialog, which is not the intent.
        event.preventDefault();
    },

    _handleClickEvent: function(event)
    {
        this._clear();
    },

    _clear: function()
    {
        this._input.value = "";
        this._element.classList.remove(WebInspector.GoToLineDialog.NonEmptyClassName);
    }
}

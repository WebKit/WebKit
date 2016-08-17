/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.VisualStylePropertyNameInput = class VisualStylePropertyNameInput extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, text, layoutReversed)
    {
        super(propertyNames, text, null, null, "property-name-input", layoutReversed);

        this._propertyNameInputElement = document.createElement("input");
        this._propertyNameInputElement.placeholder = WebInspector.UIString("Enter a name.");
        this._propertyNameInputElement.addEventListener("keydown", this._inputKeyDown.bind(this));
        this._propertyNameInputElement.addEventListener("keyup", this.debounce(250)._inputKeyUp);
        this._propertyNameInputElement.addEventListener("blur", this._hideCompletions.bind(this));
        this.contentElement.appendChild(this._propertyNameInputElement);

        this._completionController = new WebInspector.VisualStyleCompletionsController(this);
        this._completionController.addEventListener(WebInspector.VisualStyleCompletionsController.Event.CompletionSelected, this._completionClicked, this);
    }

    // Public

    get value()
    {
        return this._propertyNameInputElement.value;
    }

    set value(value)
    {
        if (value && value === this.value)
            return;

        this._propertyNameInputElement.value = value;
    }

    get synthesizedValue()
    {
        return this.value || null;
    }

    get hasCompletions()
    {
        return this._completionController.hasCompletions;
    }

    set completions(completions)
    {
        this._completionController.completions = completions;
    }

    // Private

    _completionClicked(event)
    {
        this.value = event.data.text;
        this._valueDidChange();
    }

    _inputKeyDown(event)
    {
        if (!this._completionController.visible)
            return;

        let keyCode = event.keyCode;
        let enterKeyCode = WebInspector.KeyboardShortcut.Key.Enter.keyCode;
        let tabKeyCode = WebInspector.KeyboardShortcut.Key.Tab.keyCode;
        if (keyCode === enterKeyCode || keyCode === tabKeyCode) {
            this.value = this._completionController.currentCompletion;
            this._hideCompletions();
            this._valueDidChange();
            return;
        }

        let escapeKeyCode = WebInspector.KeyboardShortcut.Key.Escape.keyCode;
        if (keyCode === escapeKeyCode) {
            this._hideCompletions();
            return;
        }

        let key = event.keyIdentifier;
        if (key === "Up") {
            this._completionController.previous();
            return;
        }

        if (key === "Down") {
            this._completionController.next();
            return;
        }
    }

    _inputKeyUp()
    {
        if (!this.hasCompletions)
            return;

        let result = this._valueDidChange();
        if (!result)
            return;

        if (this._completionController.update(this.value)) {
            let bounds = WebInspector.Rect.rectFromClientRect(this._propertyNameInputElement.getBoundingClientRect());
            if (!bounds)
                return;

            this._completionController.show(bounds, 2);
        }
    }

    _hideCompletions()
    {
        this._completionController.hide();
    }

    _toggleTabbingOfSelectableElements(disabled)
    {
        this._propertyNameInputElement.tabIndex = disabled ? "-1" : null;
    }
};

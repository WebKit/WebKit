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

WebInspector.VisualStyleColorPicker = class VisualStyleColorPicker extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, text, layoutReversed)
    {
        super(propertyNames, text, null, null, "input-color-picker", layoutReversed);

        this._colorSwatch = new WebInspector.ColorSwatch;
        this._colorSwatch.addEventListener(WebInspector.ColorSwatch.Event.ColorChanged, this._colorSwatchColorChanged, this);
        this.contentElement.appendChild(this._colorSwatch.element);

        this._textInputElement = document.createElement("input");
        this._textInputElement.spellcheck = false;
        this._textInputElement.addEventListener("keydown", this._textInputKeyDown.bind(this));
        this._textInputElement.addEventListener("keyup", this._textInputKeyUp.bind(this));
        this._textInputElement.addEventListener("blur", this._hideCompletions.bind(this));
        this.contentElement.appendChild(this._textInputElement);

        this._completionController = new WebInspector.VisualStyleCompletionsController(this);
        this._completionController.addEventListener(WebInspector.VisualStyleCompletionsController.Event.CompletionSelected, this._completionClicked, this);

        this._formatChanged = false;
        this._updateColorSwatch();
        this._colorProperty = true;
    }

    // Public

    get value()
    {
        return this._textInputElement.value;
    }

    set value(value)
    {
        if (value && value === this.value)
            return;

        this._textInputElement.value = this._hasMultipleConflictingValues() ? null : value;
        this._updateColorSwatch();
    }

    get placeholder()
    {
        return this._textInputElement.getAttribute("placeholder");
    }

    set placeholder(text)
    {
        if (text && text === this.placeholder)
            return;

        if (this._hasMultipleConflictingValues())
            text = this.specialPropertyPlaceholderElement.textContent;

        this._textInputElement.setAttribute("placeholder", text || "transparent");
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

    _colorSwatchColorChanged(event)
    {
        let colorString = event && event.data && event.data.color && event.data.color.toString();
        if (!colorString)
            return;

        this.value = colorString;
        this._valueDidChange();
    }

    _updateColorSwatch()
    {
        let value = this._textInputElement.value;
        this._colorSwatch.color = WebInspector.Color.fromString(value || "transparent");
    }

    _completionClicked(event)
    {
        this.value = event.data.text;
        this._valueDidChange();
    }

    _textInputKeyDown(event)
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

    _textInputKeyUp()
    {
        this._showCompletionsIfAble();
        this._updateColorSwatch();
        this._valueDidChange();
    }

    _showCompletionsIfAble()
    {
        if (!this.hasCompletions)
            return;

        let result = this._valueDidChange();
        if (!result)
            return;

        if (this._completionController.update(this.value)) {
            let bounds = WebInspector.Rect.rectFromClientRect(this._textInputElement.getBoundingClientRect());
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
        this._textInputElement.tabIndex = disabled ? "-1" : null;
    }
};

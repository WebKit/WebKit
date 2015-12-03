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

        this._swatchElement = document.createElement("span");
        this._swatchElement.classList.add("color-swatch");
        this._swatchElement.title = WebInspector.UIString("Click to select a color. Shift-click to switch color formats.");
        this._swatchElement.addEventListener("click", this._colorSwatchClicked.bind(this));

        this._swatchInnerElement = document.createElement("span");
        this._swatchElement.appendChild(this._swatchInnerElement);

        this.contentElement.appendChild(this._swatchElement);

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

    _updateColorSwatch()
    {
        var value = this._textInputElement.value;
        this._color = WebInspector.Color.fromString(value || "transparent");
        this._swatchInnerElement.style.backgroundColor = this._color ? value : null;
    }

    _colorSwatchClicked(event)
    {
        var color = this._color;
        if (event.shiftKey) {
            var nextFormat = color.nextFormat();

            console.assert(nextFormat);
            if (!nextFormat)
                return;

            color.format = nextFormat;
            this.value = color.toString();

            this._formatChanged = true;
            this._valueDidChange();
            return;
        }

        var bounds = WebInspector.Rect.rectFromClientRect(this._swatchElement.getBoundingClientRect());

        var colorPicker = new WebInspector.ColorPicker;
        colorPicker.addEventListener(WebInspector.ColorPicker.Event.ColorChanged, this._colorPickerColorDidChange, this);

        var popover = new WebInspector.Popover(this);
        popover.content = colorPicker.element;
        popover.present(bounds.pad(2), [WebInspector.RectEdge.MIN_X]);

        colorPicker.color = color;
    }

    _colorPickerColorDidChange(event)
    {
        var format = !this._formatChanged ? WebInspector.Color.Format.HEX : null;
        this.value = event.data.color.toString(format);
        this._valueDidChange();
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

        var keyCode = event.keyCode;
        var enterKeyCode = WebInspector.KeyboardShortcut.Key.Enter.keyCode;
        var tabKeyCode = WebInspector.KeyboardShortcut.Key.Tab.keyCode;
        if (keyCode === enterKeyCode || keyCode === tabKeyCode) {
            this.value = this._completionController.currentCompletion;
            this._hideCompletions();
            this._valueDidChange();
            return;
        }

        var escapeKeyCode = WebInspector.KeyboardShortcut.Key.Escape.keyCode;
        if (keyCode === escapeKeyCode) {
            this._hideCompletions();
            return;
        }

        var key = event.keyIdentifier;
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

        var result = this._valueDidChange();
        if (!result)
            return;

        if (this._completionController.update(this.value)) {
            var bounds = WebInspector.Rect.rectFromClientRect(this._textInputElement.getBoundingClientRect());
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

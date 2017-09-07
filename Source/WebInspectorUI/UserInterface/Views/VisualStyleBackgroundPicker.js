/*
 * Copyright (C) 2015 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.VisualStyleBackgroundPicker = class VisualStyleBackgroundPicker extends WI.VisualStylePropertyEditor
{
    constructor(propertyNames, text, possibleValues, layoutReversed)
    {
        super(propertyNames, text, possibleValues, null, "background-picker", layoutReversed);

        this._gradientSwatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Gradient);
        this._gradientSwatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, this._gradientSwatchColorChanged, this);
        this.contentElement.appendChild(this._gradientSwatch.element);

        this._valueInputElement = document.createElement("input");
        this._valueInputElement.classList.add("value-input");
        this._valueInputElement.type = "url";
        this._valueInputElement.placeholder = WI.UIString("Enter a URL");
        this._valueInputElement.addEventListener("input", this.debounce(250)._valueInputValueChanged);
        this.contentElement.appendChild(this._valueInputElement);

        this._valueTypePickerElement = document.createElement("select");
        this._valueTypePickerElement.classList.add("value-type-picker-select");
        if (this._possibleValues.advanced)
            this._valueTypePickerElement.title = WI.UIString("Option-click to show all values");

        let imageOption = document.createElement("option");
        imageOption.value = "url";
        imageOption.text = WI.UIString("Image");
        this._valueTypePickerElement.appendChild(imageOption);

        let linearGradientOption = document.createElement("option");
        linearGradientOption.value = "linear-gradient";
        linearGradientOption.text = WI.UIString("Linear Gradient");
        this._valueTypePickerElement.appendChild(linearGradientOption);

        let radialGradientOption = document.createElement("option");
        radialGradientOption.value = "radial-gradient";
        radialGradientOption.text = WI.UIString("Radial Gradient");
        this._valueTypePickerElement.appendChild(radialGradientOption);

        let repeatingLinearGradientOption = document.createElement("option");
        repeatingLinearGradientOption.value = "repeating-linear-gradient";
        repeatingLinearGradientOption.text = WI.UIString("Repeating Linear Gradient");
        this._valueTypePickerElement.appendChild(repeatingLinearGradientOption);

        let repeatingRadialGradientOption = document.createElement("option");
        repeatingRadialGradientOption.value = "repeating-radial-gradient";
        repeatingRadialGradientOption.text = WI.UIString("Repeating Radial Gradient");
        this._valueTypePickerElement.appendChild(repeatingRadialGradientOption);

        this._valueTypePickerElement.appendChild(document.createElement("hr"));

        this._createValueOptions(this._possibleValues.basic);
        this._advancedValuesElements = null;

        this._valueTypePickerElement.addEventListener("mousedown", this._keywordSelectMouseDown.bind(this));
        this._valueTypePickerElement.addEventListener("change", this._handleKeywordChanged.bind(this));
        this.contentElement.appendChild(this._valueTypePickerElement);

        this._currentType = "url";
    }

    // Public

    get value()
    {
        return this._valueInputElement.value;
    }

    set value(value)
    {
        if (!value || !value.length || value === this.value)
            return;

        const isKeyword = this.valueIsSupportedKeyword(value);
        this._currentType = isKeyword ? value : value.substring(0, value.indexOf("("));
        this._updateValueInput();
        if (!isKeyword)
            this._valueInputElement.value = value.substring(value.indexOf("(") + 1, value.length - 1);

        this._valueTypePickerElement.value = this._currentType;

        if (!this._currentType.includes("gradient"))
            return;

        this._updateGradient();
    }

    get synthesizedValue()
    {
        if (this.valueIsSupportedKeyword(this._currentType))
            return this._currentType;

        return this._currentType + "(" + this.value + ")";
    }

    // Protected

    parseValue(text)
    {
        const validPrefixes = ["url", "linear-gradient", "radial-gradient", "repeating-linear-gradient", "repeating-radial-gradient"];
        return validPrefixes.some((item) => text.startsWith(item)) ? [text, text] : null;
    }

    // Private

    _updateValueInput()
    {
        const supportedKeyword = this.valueIsSupportedKeyword(this._currentType);
        const gradientValue = this._currentType.includes("gradient");
        this.contentElement.classList.toggle("gradient-value", !supportedKeyword && gradientValue);
        this._valueInputElement.disabled = supportedKeyword;
        if (supportedKeyword) {
            this._valueInputElement.value = "";
            this._valueInputElement.placeholder = WI.UIString("Using Keyword Value");
        } else {
            if (this._currentType === "image") {
                this._valueInputElement.type = "url";
                this._valueInputElement.placeholder = WI.UIString("Enter a URL");
            } else if (gradientValue) {
                this._valueInputElement.type = "text";
                this._valueInputElement.placeholder = WI.UIString("Enter a Gradient");
            }
        }
    }

    _updateGradient()
    {
        const value = this.synthesizedValue;
        if (!value || value === this._currentType)
            return;

        this._gradientSwatch.value = WI.Gradient.fromString(value);
    }

    _gradientSwatchColorChanged(event)
    {
        this.value = event.data.value.toString();
        this._valueDidChange();
    }

    _valueInputValueChanged(event)
    {
        this._updateGradient();
        this._valueDidChange();
    }

    _keywordSelectMouseDown(event)
    {
        if (event.altKey)
            this._addAdvancedValues();
        else if (!this._valueIsSupportedAdvancedKeyword(this.value))
            this._removeAdvancedValues();
    }

    _handleKeywordChanged(event)
    {
        this._currentType = this._valueTypePickerElement.value;
        this._updateValueInput();
        this._updateGradient();
        this._valueDidChange();
    }

    _createValueOptions(values)
    {
        let addedElements = [];
        for (let key in values) {
            let option = document.createElement("option");
            option.value = key;
            option.text = values[key];
            this._valueTypePickerElement.appendChild(option);
            addedElements.push(option);
        }
        return addedElements;
    }

    _addAdvancedValues()
    {
        if (this._advancedValuesElements)
            return;

        this._valueTypePickerElement.appendChild(document.createElement("hr"));
        this._advancedValuesElements = this._createValueOptions(this._possibleValues.advanced);
    }

    _removeAdvancedValues()
    {
        if (!this._advancedValuesElements)
            return;

        this._valueTypePickerElement.removeChild(this._advancedValuesElements[0].previousSibling);
        for (let element of this._advancedValuesElements)
            this._valueTypePickerElement.removeChild(element);

        this._advancedValuesElements = null;
    }

    _toggleTabbingOfSelectableElements(disabled)
    {
        let tabIndex = disabled ? "-1" : null;
        this._valueInputElement.tabIndex = tabIndex;
        this._valueTypePickerElement.tabIndex = tabIndex;
    }
};

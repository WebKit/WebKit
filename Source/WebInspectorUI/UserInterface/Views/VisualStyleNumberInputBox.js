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

WebInspector.VisualStyleNumberInputBox = class VisualStyleNumberInputBox extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, text, possibleValues, possibleUnits, allowNegativeValues, layoutReversed)
    {
        super(propertyNames, text, possibleValues, possibleUnits || [WebInspector.UIString("Number")], "number-input-box", layoutReversed);

        this._hasUnits = !!possibleUnits;
        this._allowNegativeValues = !!allowNegativeValues || false;

        this.contentElement.classList.toggle("no-values", !possibleValues || !possibleValues.length);
        this.contentElement.classList.toggle("no-units", !this._hasUnits);

        let focusRingElement = document.createElement("div");
        focusRingElement.classList.add("focus-ring");
        this.contentElement.appendChild(focusRingElement);

        this._keywordSelectElement = document.createElement("select");
        this._keywordSelectElement.classList.add("number-input-keyword-select");
        if (this._possibleUnits.advanced)
            this._keywordSelectElement.title = WebInspector.UIString("Option-click to show all units");

        if (this._possibleValues) {
            this._createValueOptions(this._possibleValues.basic);
            this._keywordSelectElement.appendChild(document.createElement("hr"));
        }

        if (this._possibleUnits)
            this._createUnitOptions(this._possibleUnits.basic);

        this._advancedUnitsElements = null;

        this._keywordSelectElement.addEventListener("focus", this._focusContentElement.bind(this));
        this._keywordSelectElement.addEventListener("mousedown", this._keywordSelectMouseDown.bind(this));
        this._keywordSelectElement.addEventListener("change", this._keywordChanged.bind(this));
        this._keywordSelectElement.addEventListener("blur", this._blurContentElement.bind(this));
        this.contentElement.appendChild(this._keywordSelectElement);

        this._numberUnitsContainer = document.createElement("div");
        this._numberUnitsContainer.classList.add("number-input-container");

        this._valueNumberInputElement = document.createElement("input");
        this._valueNumberInputElement.classList.add("number-input-value");
        this._valueNumberInputElement.spellcheck = false;
        this._valueNumberInputElement.addEventListener("focus", this._focusContentElement.bind(this));
        this._valueNumberInputElement.addEventListener("keydown", this._valueNumberInputKeyDown.bind(this));
        this._valueNumberInputElement.addEventListener("keyup", this._numberInputChanged.bind(this));
        this._valueNumberInputElement.addEventListener("blur", this._blurContentElement.bind(this));
        this._numberUnitsContainer.appendChild(this._valueNumberInputElement);

        this._unitsElement = document.createElement("span");
        this._numberUnitsContainer.appendChild(this._unitsElement);
        this.contentElement.appendChild(this._numberUnitsContainer);

        this._numberInputIsEditable = true;
        this.contentElement.classList.add("number-input-editable");
        this._valueNumberInputElement.value = null;
        this._valueNumberInputElement.setAttribute("placeholder", 0);
        if (this._hasUnits && this.valueIsSupportedUnit("px"))
            this._unitsElement.textContent = this._keywordSelectElement.value = "px";
    }

    // Public

    get value()
    {
        if (this._numberInputIsEditable)
            return parseFloat(this._valueNumberInputElement.value);

        if (!this._numberInputIsEditable)
            return this._keywordSelectElement.value;

        return null;
    }

    set value(value)
    {
        if (value && value === this.value)
            return;

        if (!isNaN(value)) {
            this._numberInputIsEditable = true;
            this.contentElement.classList.add("number-input-editable");
            this._valueNumberInputElement.value = value;
            return;
        }

        if (!value) {
            this._valueNumberInputElement.value = null;
            return;
        }

        if (this.valueIsSupportedKeyword(value)) {
            this._numberInputIsEditable = false;
            this.contentElement.classList.remove("number-input-editable");
            this._keywordSelectElement.value = value;
            return;
        }
    }

    get units()
    {
        let keyword = this._keywordSelectElement.value;
        if (!this.valueIsSupportedUnit(keyword))
            return;

        return keyword;
    }

    set units(unit)
    {
        if (!unit || unit === this.units)
            return;

        if (!this.valueIsSupportedUnit(unit))
            return;

        if (this._valueIsSupportedAdvancedUnit(unit))
            this._addAdvancedUnits();

        this._numberInputIsEditable = true;
        this.contentElement.classList.add("number-input-editable");
        this._keywordSelectElement.value = unit;

        if (this._hasUnits)
            this._unitsElement.textContent = unit;
    }

    get placeholder()
    {
        return this._valueNumberInputElement.getAttribute("placeholder");
    }

    set placeholder(text)
    {
        if (text === this.placeholder)
            return;

        let onlyNumericalText = !isNaN(text) && text;
        this._valueNumberInputElement.setAttribute("placeholder", onlyNumericalText || 0);
    }

    get synthesizedValue()
    {
        let value = this._valueNumberInputElement.value;
        if (this._numberInputIsEditable && !value)
            return null;

        let keyword = this._keywordSelectElement.value;
        return this.valueIsSupportedUnit(keyword) ? value + (this._hasUnits ? keyword : "") : keyword;
    }

    updateValueFromText(text, value)
    {
        let match = this.parseValue(value);
        this.value = match ? match[1] : value;
        this.units = match ? match[2] : null;
        return this.modifyPropertyText(text, value);
    }

    // Protected

    parseValue(text)
    {
        return /^(-?[\d.]+)([^\s\d]{0,4})(?:\s*;?)$/.exec(text);
    }

    // Private

    _keywordChanged()
    {
        let selectedKeywordIsUnit = this.valueIsSupportedUnit(this._keywordSelectElement.value);
        if (!this._numberInputIsEditable && selectedKeywordIsUnit)
            this._valueNumberInputElement.value = null;

        if (this._hasUnits)
            this._unitsElement.textContent = this._keywordSelectElement.value;

        this._numberInputIsEditable = selectedKeywordIsUnit;
        this.contentElement.classList.toggle("number-input-editable", selectedKeywordIsUnit);
        this._valueDidChange();
    }

    _valueNumberInputKeyDown(event)
    {
        if (!this._numberInputIsEditable)
            return;

        function adjustValue(delta)
        {
            let newValue;
            let value = this.value;
            if (!value && isNaN(value)) {
                let placeholderValue = this.placeholder && !isNaN(this.placeholder) ? parseFloat(this.placeholder) : 0;
                newValue = placeholderValue + delta;
            } else
                newValue = value + delta;

            if (!this._allowNegativeValues && newValue < 0)
                newValue = 0;

            this.value = Math.round(newValue * 100) / 100;
            this._valueDidChange();
        }

        let shift = 1;
        if (event.ctrlKey)
            shift /= 10;
        else if (event.shiftKey)
            shift *= 10;

        let key = event.keyIdentifier;
        if (key.startsWith("Page"))
            shift *= 10;

        if (key === "Up" || key === "PageUp") {
            event.preventDefault();
            adjustValue.call(this, shift);
            return;
        }

        if (key === "Down" || key === "PageDown") {
            event.preventDefault();
            adjustValue.call(this, -shift);
            return;
        }

        this._valueDidChange();
    }

    _numberInputChanged()
    {
        if (!this._numberInputIsEditable)
            return;

        this._valueDidChange();
    }

    _keywordSelectMouseDown(event)
    {
        if (event.altKey)
            this._addAdvancedUnits();
        else if (!this._valueIsSupportedAdvancedUnit())
            this._removeAdvancedUnits();
    }

    _createValueOptions(values)
    {
        let addedElements = [];
        for (let key in values) {
            let option = document.createElement("option");
            option.value = key;
            option.text = values[key];
            this._keywordSelectElement.appendChild(option);
            addedElements.push(option);
        }

        return addedElements;
    }

    _createUnitOptions(units)
    {
        let addedElements = [];
        for (let unit of units) {
            let option = document.createElement("option");
            option.text = unit;
            this._keywordSelectElement.appendChild(option);
            addedElements.push(option);
        }

        return addedElements;
    }

    _addAdvancedUnits()
    {
        if (this._advancedUnitsElements)
            return;

        this._keywordSelectElement.appendChild(document.createElement("hr"));
        this._advancedUnitsElements = this._createUnitOptions(this._possibleUnits.advanced);
    }

    _removeAdvancedUnits()
    {
        if (!this._advancedUnitsElements)
            return;

        this._keywordSelectElement.removeChild(this._advancedUnitsElements[0].previousSibling);
        for (let element of this._advancedUnitsElements)
            this._keywordSelectElement.removeChild(element);

        this._advancedUnitsElements = null;
    }

    _focusContentElement(event)
    {
        this.contentElement.classList.add("focused");
    }

    _blurContentElement(event)
    {
        this.contentElement.classList.remove("focused");
    }

    _toggleTabbingOfSelectableElements(disabled)
    {
        this._keywordSelectElement.tabIndex = disabled ? "-1" : null;
        this._valueNumberInputElement.tabIndex = disabled ? "-1" : null;
    }
};

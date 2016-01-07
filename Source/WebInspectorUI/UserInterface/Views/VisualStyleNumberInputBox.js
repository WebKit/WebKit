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
        let unitlessNumberUnit = WebInspector.UIString("Number");

        super(propertyNames, text, possibleValues, possibleUnits || [unitlessNumberUnit], "number-input-box", layoutReversed);

        this._unitlessNumberUnit = unitlessNumberUnit;

        this._hasUnits = this._possibleUnits.basic.some((unit) => unit !== unitlessNumberUnit);
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

        this._unchangedOptionElement = document.createElement("option");
        this._unchangedOptionElement.value = "";
        this._unchangedOptionElement.text = WebInspector.UIString("Unchanged");
        this._keywordSelectElement.appendChild(this._unchangedOptionElement);

        this._keywordSelectElement.appendChild(document.createElement("hr"));

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
        this._valueNumberInputElement.addEventListener("keyup", this._valueNumberInputKeyUp.bind(this));
        this._valueNumberInputElement.addEventListener("blur", this._blurContentElement.bind(this));
        this._valueNumberInputElement.addEventListener("change", this._valueNumberInputChanged.bind(this));
        this._numberUnitsContainer.appendChild(this._valueNumberInputElement);

        this._unitsElement = document.createElement("span");
        this._numberUnitsContainer.appendChild(this._unitsElement);

        this.contentElement.appendChild(this._numberUnitsContainer);

        this._numberInputIsEditable = true;
        this.contentElement.classList.add("number-input-editable");
        this._valueNumberInputElement.value = null;
        this._valueNumberInputElement.setAttribute("placeholder", 0);
        if (this._hasUnits)
            this._unitsElementTextContent = this._keywordSelectElement.value = this.valueIsSupportedUnit("px") ? "px" : this._possibleUnits.basic[0];
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

        if (this._updatedValues.propertyMissing) {
            if (value || this._updatedValues.placeholder)
                this.specialPropertyPlaceholderElement.textContent = (value || this._updatedValues.placeholder) + (this._updatedValues.units || "");

            if (isNaN(value)) {
                this._unchangedOptionElement.selected = true;
                this._setNumberInputIsEditable();
                this.specialPropertyPlaceholderElement.hidden = false;
                return;
            }
        }

        this.specialPropertyPlaceholderElement.hidden = true;

        if (!value) {
            this._valueNumberInputElement.value = null;
            this._markUnitsContainerIfInputHasValue();
            return;
        }

        if (!isNaN(value)) {
            this._setNumberInputIsEditable(true);
            this._valueNumberInputElement.value = Math.round(value * 100) / 100;
            this._markUnitsContainerIfInputHasValue();
            return;
        }

        if (this.valueIsSupportedKeyword(value)) {
            this._setNumberInputIsEditable();
            this._keywordSelectElement.value = value;
            return;
        }
    }

    get units()
    {
        if (this._unchangedOptionElement.selected)
            return null;

        let keyword = this._keywordSelectElement.value;
        if (!this.valueIsSupportedUnit(keyword))
            return null;

        return keyword;
    }

    set units(unit)
    {
        if (this._unchangedOptionElement.selected || unit === this.units)
            return;

        if (!unit && !this._possibleUnits.basic.includes(this._unitlessNumberUnit) && !this.valueIsSupportedUnit(unit))
            return;

        if (this._valueIsSupportedAdvancedUnit(unit))
            this._addAdvancedUnits();

        this._setNumberInputIsEditable(true);
        this._keywordSelectElement.value = unit || this._unitlessNumberUnit;
        this._unitsElementTextContent = unit;
    }

    get placeholder()
    {
        return this._valueNumberInputElement.getAttribute("placeholder");
    }

    set placeholder(text)
    {
        if (text === this.placeholder)
            return;

        let onlyNumericalText = text && !isNaN(text) && (Math.round(text * 100) / 100);
        this._valueNumberInputElement.setAttribute("placeholder", onlyNumericalText || 0);

        if (!onlyNumericalText)
            this.specialPropertyPlaceholderElement.textContent = this._canonicalizedKeywordForKey(text) || text;
    }

    get synthesizedValue()
    {
        if (this._unchangedOptionElement.selected)
            return null;

        let value = this._valueNumberInputElement.value;
        if (this._numberInputIsEditable && !value)
            return null;

        let keyword = this._keywordSelectElement.value;
        return this.valueIsSupportedUnit(keyword) ? value + (keyword === this._unitlessNumberUnit ? "" : keyword) : keyword;
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

    set _unitsElementTextContent(text)
    {
        if (!this._hasUnits)
            return;

        this._unitsElement.textContent = text === this._unitlessNumberUnit ? "" : text;
        this._markUnitsContainerIfInputHasValue();
    }

    _setNumberInputIsEditable(flag)
    {
        this._numberInputIsEditable = flag || false;
        this.contentElement.classList.toggle("number-input-editable", this._numberInputIsEditable);
    }

    _markUnitsContainerIfInputHasValue()
    {
        let numberInputValue = this._valueNumberInputElement.value;
        this._numberUnitsContainer.classList.toggle("has-value", numberInputValue && numberInputValue.length);
    }

    _keywordChanged()
    {
        let unchangedOptionSelected = this._unchangedOptionElement.selected;
        if (!unchangedOptionSelected) {
            let selectedKeywordIsUnit = this.valueIsSupportedUnit(this._keywordSelectElement.value);
            if (!this._numberInputIsEditable && selectedKeywordIsUnit)
                this._valueNumberInputElement.value = null;

            this._unitsElementTextContent = this._keywordSelectElement.value;
            this._setNumberInputIsEditable(selectedKeywordIsUnit);
        } else
            this._setNumberInputIsEditable(false);

        this._valueDidChange();
        this.specialPropertyPlaceholderElement.hidden = !unchangedOptionSelected;
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

            this._updatedValues.propertyMissing = false;
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

        this._markUnitsContainerIfInputHasValue();
        this._valueDidChange();
    }

    _valueNumberInputKeyUp(event)
    {
        if (!this._numberInputIsEditable)
            return;

        this._markUnitsContainerIfInputHasValue();
        this._valueDidChange();
    }

    _keywordSelectMouseDown(event)
    {
        if (event.altKey)
            this._addAdvancedUnits();
        else if (!this._valueIsSupportedAdvancedUnit(this._keywordSelectElement.value))
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

    _valueNumberInputChanged(event)
    {
        let newValue = this.value;
        if (!newValue && isNaN(newValue))
            newValue = this.placeholder && !isNaN(this.placeholder) ? parseFloat(this.placeholder) : 0;

        if (!this._allowNegativeValues && newValue < 0)
            newValue = 0;

        this.value = Math.round(newValue * 100) / 100;
        this._valueDidChange();
    }

    _toggleTabbingOfSelectableElements(disabled)
    {
        this._keywordSelectElement.tabIndex = disabled ? "-1" : null;
        this._valueNumberInputElement.tabIndex = disabled ? "-1" : null;
    }
};

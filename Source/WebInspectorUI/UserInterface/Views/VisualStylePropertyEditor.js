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

WebInspector.VisualStylePropertyEditor = class VisualStylePropertyEditor extends WebInspector.Object
{
    constructor(propertyNames, label, possibleValues, possibleUnits, className, layoutReversed)
    {
        super();

        this._propertyInfoList = [];
        this._style = null;

        function canonicalizeValues(values)
        {
            if (!values)
                return;

            let canonicalizedValues = {};
            for (let value of values)
                canonicalizedValues[value.toLowerCase().replace(/\s/g, "-")] = value;

            return canonicalizedValues;
        }

        this._possibleValues = null;
        if (possibleValues) {
            this._possibleValues = {};
            if (Array.isArray(possibleValues))
                this._possibleValues.basic = canonicalizeValues(possibleValues);
            else {
                this._possibleValues.basic = canonicalizeValues(possibleValues.basic);
                this._possibleValues.advanced = canonicalizeValues(possibleValues.advanced);
            } 
        }
        this._possibleUnits = null;
        if (possibleUnits) {
            this._possibleUnits = {};
            if (Array.isArray(possibleUnits))
                this._possibleUnits.basic = possibleUnits;
            else
                this._possibleUnits = possibleUnits;
        }

        this._element = document.createElement("div");
        this._element.classList.add("visual-style-property-container", className);
        this._element.classList.toggle("layout-reversed", !!layoutReversed);

        if (label && label.length) {
            let titleContainer = document.createElement("div");
            titleContainer.classList.add("visual-style-property-title");

            this._titleElement = document.createElement("span");
            this._titleElement.appendChild(document.createTextNode(label));
            this._titleElement.addEventListener("mouseover", this._titleElementMouseOver.bind(this));
            this._titleElement.addEventListener("mouseout", this._titleElementMouseOut.bind(this));
            this._titleElement.addEventListener("click", this._titleElementClick.bind(this));
            titleContainer.appendChild(this._titleElement);

            this._element.appendChild(titleContainer);

            this._boundTitleElementPrepareForClick = this._titleElementPrepareForClick.bind(this);
        }

        this._contentElement = document.createElement("div");
        this._contentElement.classList.add("visual-style-property-value-container");

        this._specialPropertyPlaceholderElement = document.createElement("span");
        this._specialPropertyPlaceholderElement.classList.add("visual-style-special-property-placeholder");
        this._specialPropertyPlaceholderElement.hidden = true;
        this._contentElement.appendChild(this._specialPropertyPlaceholderElement);

        this._element.appendChild(this._contentElement);

        this._updatedValues = {};
        this._lastValue = null;
        this._propertyMissing = false;

        if (typeof propertyNames === "string")
            propertyNames = [propertyNames];
        else {
            this._hasMultipleProperties = true;
            this._element.classList.add("multiple");
        }

        for (let name of propertyNames) {
            this._element.classList.add(name);
            this._propertyInfoList.push({
                name,
                textContainsNameRegExp: new RegExp("(?:(?:^|;)\\s*" + name + "\\s*:)"),
                replacementRegExp: new RegExp("((?:^|;)\\s*)(" + name + ")(.+?(?:;|$))")
            })
        }

        this._propertyReferenceName = propertyNames[0];
        this._propertyReferenceText = WebInspector.VisualStyleDetailsPanel.propertyReferenceInfo[this._propertyReferenceName];
        this._hasPropertyReference = this._propertyReferenceText && !!this._propertyReferenceText.trim().length;
        this._representedProperty = null;
    }

    // Static

    static generateFormattedTextForNewProperty(styleText, propertyName, propertyValue) {
        if (!propertyName || !propertyValue)
            return "";

        styleText = styleText || "";

        // FIXME: <rdar://problem/10593948> Provide a way to change the tab width in the Web Inspector
        let linePrefixText = "    ";
        let lineSuffixWhitespace = "\n";
        let trimmedText = styleText.trimRight();
        let textHasNewlines = trimmedText.includes("\n");

        if (trimmedText.trimLeft().length) {
            let styleTextPrefixWhitespace = trimmedText.match(/^\s*/);
            if (styleTextPrefixWhitespace) {
                let linePrefixWhitespaceMatch = styleTextPrefixWhitespace[0].match(/[^\S\n]+$/);
                if (linePrefixWhitespaceMatch && textHasNewlines)
                    linePrefixText = linePrefixWhitespaceMatch[0];
                else {
                    linePrefixText = "";
                    lineSuffixWhitespace = styleTextPrefixWhitespace[0];
                }
            }

            if (!trimmedText.endsWith(";"))
                linePrefixText = ";" + linePrefixText;
        } else
            linePrefixText = "\n" + linePrefixText;

        return linePrefixText + propertyName + ": " + propertyValue + ";" + lineSuffixWhitespace;
    }

    // Public

    get element()
    {
        return this._element;
    }

    get style()
    {
        return this._style;
    }

    get value()
    {
        // Implemented by subclass.
    }

    set value(value)
    {
        // Implemented by subclass.
    }

    get units()
    {
        // Implemented by subclass.
    }

    set units(unit)
    {
        // Implemented by subclass.
    }

    get placeholder()
    {
        // Implemented by subclass.
    }

    set placeholder(text)
    {
        // Implemented by subclass.
    }

    get synthesizedValue()
    {
        // Implemented by subclass.
    }

    set suppressStyleTextUpdate(flag)
    {
        this._suppressStyleTextUpdate = flag;
    }

    set masterProperty(flag)
    {
        this._masterProperty = flag;
    }

    get masterProperty()
    {
        return this._masterProperty;
    }

    set optionalProperty(flag)
    {
        this._optionalProperty = flag;
    }

    get optionalProperty()
    {
        return this._optionalProperty;
    }

    set colorProperty(flag)
    {
        this._colorProperty = flag;
    }

    get colorProperty()
    {
        return this._colorProperty;
    }

    get propertyReferenceName()
    {
        return this._propertyReferenceName;
    }

    set propertyReferenceName(name)
    {
        if (!name || !name.length)
            return;

        this._propertyReferenceName = name;
    }

    set disabled(flag)
    {
        this._disabled = flag;
        this._element.classList.toggle("disabled", this._disabled);
        this._toggleTabbingOfSelectableElements(this._disabled);
    }

    get disabled()
    {
        return this._disabled;
    }

    update(style)
    {
        if (style)
            this._style = style;
        else if (this._ignoreNextUpdate) {
            this._ignoreNextUpdate = false;
            return;
        }

        if (!this._style)
            return;

        this._updatedValues = {};
        let propertyValuesConflict = false;
        let propertyMissing = false;
        for (let propertyInfo of this._propertyInfoList) {
            let property = this._style.propertyForName(propertyInfo.name, true);
            propertyMissing = !property;
            if (propertyMissing && this._style.nodeStyles)
                property = this._style.nodeStyles.computedStyle.propertyForName(propertyInfo.name);

            let longhandPropertyValue = null;
            if (typeof this._generateTextFromLonghandProperties === "function")
                longhandPropertyValue = this._generateTextFromLonghandProperties();

            if (longhandPropertyValue)
                propertyMissing = false;

            let propertyText = (property && property.value) || longhandPropertyValue;
            if (!propertyText || !propertyText.length)
                continue;

            if (!propertyMissing && property && property.anonymous)
                this._representedProperty = property;

            let newValues = this.getValuesFromText(propertyText, propertyMissing);
            if (this._updatedValues.placeholder && this._updatedValues.placeholder !== newValues.placeholder)
                propertyValuesConflict = true;

            if (!this._updatedValues.placeholder)
                this._updatedValues = newValues;

            if (propertyValuesConflict) {
                this._updatedValues.conflictingValues = true;
                this._specialPropertyPlaceholderElement.textContent = WebInspector.UIString("(multiple)");
                break;
            }
        }

        if (this._hasMultipleProperties)
            this._specialPropertyPlaceholderElement.hidden = !propertyValuesConflict;

        this.updateEditorValues(this._updatedValues);
    }

    updateEditorValues(updatedValues)
    {
        this.value = updatedValues.value;
        this.units = updatedValues.units;
        this.placeholder = updatedValues.placeholder;

        this._lastValue = this.synthesizedValue;
        this.disabled = false;
    }

    resetEditorValues(value)
    {
        this._ignoreNextUpdate = false;
        if (!value || !value.length) {
            this.value = null;
            this._specialPropertyPlaceholderElement.hidden = false;
            return;
        }

        let updatedValues = this.getValuesFromText(value);
        this.updateEditorValues(updatedValues);
    }

    modifyPropertyText(text, value)
    {
        for (let property of this._propertyInfoList) {
            if (property.textContainsNameRegExp.test(text))
                text = text.replace(property.replacementRegExp, value !== null ? "$1$2: " + value + ";" : "$1");
            else if (value !== null)
                text += WebInspector.VisualStylePropertyEditor.generateFormattedTextForNewProperty(text, property.name, value);
        }
        return text;
    }

    getValuesFromText(text, propertyMissing)
    {
        let match = this.parseValue(text);
        let placeholder = match ? match[1] : text;
        let units = match ? match[2] : null;
        let value = placeholder;
        if (propertyMissing)
            value = this.valueIsSupportedKeyword(text) ? text : null;

        this._propertyMissing = propertyMissing || false;
        return {value, units, placeholder};
    }

    get propertyMissing()
    {
        return this._updatedValues && this._propertyMissing;
    }

    valueIsCompatible(value)
    {
        if (!value || !value.length)
            return false;

        return this.valueIsSupportedKeyword(value) || !!this.parseValue(value);
    }

    valueIsSupportedKeyword(value) {
        if (!this._possibleValues)
            return false;

        if (Object.keys(this._possibleValues.basic).includes(value))
            return true;

        return this._valueIsSupportedAdvancedKeyword(value);
    }

    valueIsSupportedUnit(unit)
    {
        if (!this._possibleUnits)
            return false;

        if (this._possibleUnits.basic.includes(unit))
            return true;

        return this._valueIsSupportedAdvancedUnit(unit);
    }

    // Protected

    get contentElement()
    {
        return this._contentElement;
    }

    get specialPropertyPlaceholderElement()
    {
        return this._specialPropertyPlaceholderElement;
    }

    parseValue(text)
    {
        return /^([^;]+)\s*;?$/.exec(text);
    }

    // Private

    _valueIsSupportedAdvancedKeyword(value)
    {
        return this._possibleValues.advanced && Object.keys(this._possibleValues.advanced).includes(value);
    }

    _valueIsSupportedAdvancedUnit(unit)
    {
        return this._possibleUnits.advanced && this._possibleUnits.advanced.includes(unit);
    }

    _canonicalizedKeywordForKey(value)
    {
        if (!value || !this._possibleValues)
            return null;

        return this._possibleValues.basic[value] || (this._possibleValues.advanced && this._possibleValues.advanced[value]) || null;
    }

    _keyForKeyword(keyword)
    {
        if (!keyword || !keyword.length || !this._possibleValues)
            return null;

        for (let basicKey in this._possibleValues.basic) {
            if (this._possibleValues.basic[basicKey] === keyword)
                return basicKey;
        }

        if (!this._possibleValues.advanced)
            return null;

        for (let advancedKey in this._possibleValues.advanced) {
            if (this._possibleValues.advanced[advancedKey] === keyword)
                return advancedKey;
        }

        return null;
    }

    _valueDidChange()
    {
        let value = this.synthesizedValue;
        if (value === this._lastValue)
            return false;

        if (this._style && !this._suppressStyleTextUpdate) {
            let newText = this._style.text;
            newText = this._replaceShorthandPropertyWithLonghandProperties(newText);
            newText = this.modifyPropertyText(newText, value);
            this._style.text = newText;
            if (!newText.length)
                this._style.update(null, null, this._style.styleSheetTextRange);
        }

        this._lastValue = value;
        this._propertyMissing = !value;
        this._ignoreNextUpdate = true;
        this._specialPropertyPlaceholderElement.hidden = true;

        this.dispatchEventToListeners(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange);
        return true;
    }

    _replaceShorthandPropertyWithLonghandProperties(text)
    {
        if (!this._representedProperty)
            return text;

        let shorthand = this._representedProperty.relatedShorthandProperty;
        if (!shorthand)
            return text;

        let longhandText = "";
        for (let longhandProperty of shorthand.relatedLonghandProperties) {
            if (longhandProperty.anonymous)
                longhandText += longhandProperty.synthesizedText;
        }
        return longhandText ? text.replace(shorthand.text, longhandText) : text;
    }

    _titleElementPrepareForClick(event)
    {
        this._titleElement.classList.toggle("property-reference-info", event.type === "keydown" && event.altKey);
    }

    _titleElementMouseOver(event)
    {
        if (!this._hasPropertyReference)
            return;

        this._titleElement.classList.toggle("property-reference-info", event.altKey);
        document.addEventListener("keydown", this._boundTitleElementPrepareForClick);
        document.addEventListener("keyup", this._boundTitleElementPrepareForClick);
    }

    _titleElementMouseOut()
    {
        if (!this._hasPropertyReference)
            return;

        this._titleElement.classList.remove("property-reference-info");
        document.removeEventListener("keydown", this._boundTitleElementPrepareForClick);
        document.removeEventListener("keyup", this._boundTitleElementPrepareForClick);
    }

    _titleElementClick(event)
    {
        if (event.altKey)
            this._showPropertyInfoPopover();
    }

    _hasMultipleConflictingValues()
    {
        return this._hasMultipleProperties && !this._specialPropertyPlaceholderElement.hidden;
    }

    _showPropertyInfoPopover()
    {
        if (!this._hasPropertyReference)
            return;

        let propertyInfoElement = document.createElement("p");
        propertyInfoElement.classList.add("visual-style-property-info-popover");

        let propertyInfoTitleElement = document.createElement("h3");
        propertyInfoTitleElement.appendChild(document.createTextNode(this._propertyReferenceName));
        propertyInfoElement.appendChild(propertyInfoTitleElement);

        propertyInfoElement.appendChild(document.createTextNode(this._propertyReferenceText));

        let bounds = WebInspector.Rect.rectFromClientRect(this._titleElement.getBoundingClientRect());
        let popover = new WebInspector.Popover(this);
        popover.content = propertyInfoElement;
        popover.present(bounds.pad(2), [WebInspector.RectEdge.MIN_Y]);
    }

    _toggleTabbingOfSelectableElements(disabled)
    {
        // Implemented by subclass.
    }
};

WebInspector.VisualStylePropertyEditor.Event = {
    ValueDidChange: "visual-style-property-editor-value-changed"
}

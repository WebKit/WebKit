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

WI.SpreadsheetStyleProperty = class SpreadsheetStyleProperty extends WI.Object
{
    constructor(delegate, property, newlyAdded = false)
    {
        super();

        console.assert(property instanceof WI.CSSProperty);

        this._delegate = delegate || null;
        this._property = property;
        this._newlyAdded = newlyAdded;
        this._element = document.createElement("div");

        this._nameElement = null;
        this._valueElement = null;

        this._nameTextField = null;
        this._valueTextField = null;

        this._property.__propertyView = this;

        this._update();
        property.addEventListener(WI.CSSProperty.Event.OverriddenStatusChanged, this._update, this);
        property.addEventListener(WI.CSSProperty.Event.Changed, this.updateClassNames, this);
    }

    // Public

    get element() { return this._element; }
    get nameTextField() { return this._nameTextField; }
    get valueTextField() { return this._valueTextField; }

    detached()
    {
        this._property.__propertyView = null;

        if (this._nameTextField)
            this._nameTextField.detached();

        if (this._valueTextField)
            this._valueTextField.detached();
    }

    highlight()
    {
        this._element.classList.add("highlighted");
    }

    updateClassNames()
    {
        let duplicatePropertyExistsBelow = (cssProperty) => {
            let propertyFound = false;

            for (let property of this._property.ownerStyle.properties) {
                if (property === cssProperty)
                    propertyFound = true;
                else if (property.name === cssProperty.name && propertyFound)
                    return true;
            }

            return false;
        };

        let classNames = ["property"];

        if (this._property.overridden)
            classNames.push("overridden");

        if (this._property.implicit)
            classNames.push("implicit");

        if (this._property.ownerStyle.inherited && !this._property.inherited)
            classNames.push("not-inherited");

        if (!this._property.valid && this._property.hasOtherVendorNameOrKeyword())
            classNames.push("other-vendor");
        else if (!this._property.valid && this._property.value !== "") {
            let propertyNameIsValid = false;
            if (WI.CSSCompletions.cssNameCompletions)
                propertyNameIsValid = WI.CSSCompletions.cssNameCompletions.isValidPropertyName(this._property.name);

            if (!propertyNameIsValid || duplicatePropertyExistsBelow(this._property))
                classNames.push("invalid-name");
            else
                classNames.push("invalid-value");
        }

        if (!this._property.enabled)
            classNames.push("disabled");

        this._element.className = classNames.join(" ");
    }

    // Private

    _remove()
    {
        this.element.remove();
        this._property.remove();
        this.detached();

        if (this._delegate && typeof this._delegate.spreadsheetStylePropertyRemoved === "function")
            this._delegate.spreadsheetStylePropertyRemoved(this);
    }

    _update()
    {
        this.element.removeChildren();
        this.updateClassNames();

        if (this._property.editable) {
            this._checkboxElement = this.element.appendChild(document.createElement("input"));
            this._checkboxElement.classList.add("property-toggle");
            this._checkboxElement.type = "checkbox";
            this._checkboxElement.checked = this._property.enabled;
            this._checkboxElement.tabIndex = -1;
            this._checkboxElement.addEventListener("change", () => {
                let disabled = !this._checkboxElement.checked;
                this._property.commentOut(disabled);
                this._update();
            });
        }

        if (!this._property.enabled)
            this.element.append("/* ");

        this._nameElement = this.element.appendChild(document.createElement("span"));
        this._nameElement.classList.add("name");
        this._nameElement.textContent = this._property.name;

        this.element.append(": ");

        this._valueElement = this.element.appendChild(document.createElement("span"));
        this._valueElement.classList.add("value");
        this._renderValue(this._property.rawValue);

        if (this._property.editable && this._property.enabled) {
            this._nameElement.tabIndex = 0;
            this._nameTextField = new WI.SpreadsheetTextField(this, this._nameElement, this._nameCompletionDataProvider.bind(this));

            this._valueElement.tabIndex = 0;
            this._valueTextField = new WI.SpreadsheetTextField(this, this._valueElement, this._valueCompletionDataProvider.bind(this));
        }

        if (this._property.editable) {
            this._setupJumpToSymbol(this._nameElement);
            this._setupJumpToSymbol(this._valueElement);
        }

        this.element.append(";");

        if (!this._property.enabled)
            this.element.append(" */");
    }

    // SpreadsheetTextField delegate

    spreadsheetTextFieldWillStartEditing(textField)
    {
        let isEditingName = textField === this._nameTextField;
        textField.value = isEditingName ? this._property.name : this._property.rawValue;
    }

    spreadsheetTextFieldDidChange(textField)
    {
        if (textField === this._valueTextField)
            this.debounce(WI.SpreadsheetStyleProperty.CommitCoalesceDelay)._handleValueChange();
        else if (textField === this._nameTextField)
            this.debounce(WI.SpreadsheetStyleProperty.CommitCoalesceDelay)._handleNameChange();
    }

    spreadsheetTextFieldDidCommit(textField, {direction})
    {
        let propertyName = this._nameTextField.value.trim();
        let propertyValue = this._valueTextField.value.trim();
        let willRemoveProperty = false;

        // Remove a property with an empty name or value. However, a newly added property
        // has an empty name and value at first. Don't remove it when moving focus from
        // the name to the value for the first time.
        if (!propertyName || (!this._newlyAdded && !propertyValue))
            willRemoveProperty = true;

        let isEditingName = textField === this._nameTextField;

        if (!isEditingName && !willRemoveProperty)
            this._renderValue(propertyValue);

        if (propertyName && isEditingName)
            this._newlyAdded = false;

        if (direction === "forward") {
            if (isEditingName && !willRemoveProperty) {
                // Move focus from the name to the value.
                this._valueTextField.startEditing();
                return;
            }
        } else {
            if (!isEditingName) {
                // Move focus from the value to the name.
                this._nameTextField.startEditing();
                return;
            }
        }

        if (typeof this._delegate.spreadsheetCSSStyleDeclarationEditorFocusMoved === "function") {
            // Move focus away from the current property, to the next or previous one, if exists, or to the next or previous rule, if exists.
            this._delegate.spreadsheetCSSStyleDeclarationEditorFocusMoved({direction, willRemoveProperty, movedFromProperty: this});
        }

        if (willRemoveProperty)
            this._remove();
    }

    spreadsheetTextFieldDidBlur(textField)
    {
        if (textField.value.trim() === "")
            this._remove();
        else if (textField === this._valueTextField)
            this._renderValue(this._valueElement.textContent);
    }

    // Private

    _renderValue(value)
    {
        const maxValueLength = 150;
        let tokens = WI.tokenizeCSSValue(value);

        if (this._property.enabled) {
            // Don't show color widgets for CSS gradients, show dedicated gradient widgets instead.
            // FIXME: <https://webkit.org/b/178404> Web Inspector: [PARITY] Styles Redesign: Add bezier curve, color gradient, and CSS variable inline widgets
            tokens = this._addColorTokens(tokens);
        }

        tokens = tokens.map((token) => {
            if (token instanceof Element)
                return token;

            let className = "";

            if (token.type) {
                if (token.type.includes("string"))
                    className = "token-string";
                else if (token.type.includes("link"))
                    className = "token-link";
                else if (token.type.includes("comment"))
                    className = "token-comment";
            }

            if (className) {
                let span = document.createElement("span");
                span.classList.add(className);
                span.textContent = token.value.trimMiddle(maxValueLength);
                return span;
            }

            return token.value;
        });

        this._valueElement.removeChildren();
        this._valueElement.append(...tokens);
    }

    _addColorTokens(tokens)
    {
        let newTokens = [];

        let createColorTokenElement = (colorString, color) => {
            let colorTokenElement = document.createElement("span");
            colorTokenElement.className = "token-color";

            let innerElement = document.createElement("span");
            innerElement.className = "token-color-value";
            innerElement.textContent = colorString;

            if (color) {
                let readOnly = !this._property.editable;
                let swatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Color, color, readOnly);

                swatch.addEventListener(WI.InlineSwatch.Event.ValueChanged, (event) => {
                    let value = event.data && event.data.value && event.data.value.toString();
                    console.assert(value, "Color value is empty.");
                    if (!value)
                        return;

                    innerElement.textContent = value;
                    this._handleValueChange();
                }, this);

                colorTokenElement.append(swatch.element);

                // Prevent the value from editing when clicking on the swatch.
                swatch.element.addEventListener("mousedown", (event) => { event.stop(); });
            }

            colorTokenElement.append(innerElement);
            return colorTokenElement;
        };

        let pushPossibleColorToken = (text, ...tokens) => {
            let color = WI.Color.fromString(text);
            if (color)
                newTokens.push(createColorTokenElement(text, color));
            else
                newTokens.push(...tokens);
        };

        let colorFunctionStartIndex = NaN;

        for (let i = 0; i < tokens.length; i++) {
            let token = tokens[i];
            if (token.type && token.type.includes("hex-color")) {
                // Hex
                pushPossibleColorToken(token.value, token);
            } else if (WI.Color.FunctionNames.has(token.value) && token.type && (token.type.includes("atom") || token.type.includes("keyword"))) {
                // Color Function start
                colorFunctionStartIndex = i;
            } else if (isNaN(colorFunctionStartIndex) && token.type && token.type.includes("keyword")) {
                // Color keyword
                pushPossibleColorToken(token.value, token);
            } else if (!isNaN(colorFunctionStartIndex)) {
                // Color Function end
                if (token.value !== ")")
                    continue;

                let rawTokens = tokens.slice(colorFunctionStartIndex, i + 1);
                let text = rawTokens.map((token) => token.value).join("");
                pushPossibleColorToken(text, ...rawTokens);
                colorFunctionStartIndex = NaN;
            } else
                newTokens.push(token);
        }

        return newTokens;
    }

    _handleNameChange()
    {
        this._property.name = this._nameElement.textContent.trim();
    }

    _handleValueChange()
    {
        this._property.rawValue = this._valueElement.textContent.trim();
    }

    _nameCompletionDataProvider(prefix)
    {
        return WI.CSSCompletions.cssNameCompletions.startsWith(prefix);
    }

    _valueCompletionDataProvider(prefix)
    {
        return WI.CSSKeywordCompletions.forProperty(this._property.name).startsWith(prefix);
    }

    _setupJumpToSymbol(element)
    {
        element.addEventListener("mousedown", (event) => {
            if (event.button !== 0)
                return;

            if (!WI.modifierKeys.metaKey)
                return;

            if (element.isContentEditable)
                return;

            let sourceCodeLocation = null;
            if (this._property.ownerStyle.ownerRule)
                sourceCodeLocation = this._property.ownerStyle.ownerRule.sourceCodeLocation;

            if (!sourceCodeLocation)
                return;

            let range = this._property.styleSheetTextRange;
            const options = {
                ignoreNetworkTab: true,
                ignoreSearchTab: true,
            };
            let sourceCode = sourceCodeLocation.sourceCode;
            WI.showSourceCodeLocation(sourceCode.createSourceCodeLocation(range.startLine, range.startColumn), options);
        });
    }
};

WI.SpreadsheetStyleProperty.CommitCoalesceDelay = 250;

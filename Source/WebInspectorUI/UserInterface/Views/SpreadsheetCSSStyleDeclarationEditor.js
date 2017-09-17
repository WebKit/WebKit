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

WI.SpreadsheetCSSStyleDeclarationEditor = class SpreadsheetCSSStyleDeclarationEditor extends WI.View
{
    constructor(delegate, style)
    {
        super();

        this.element.classList.add(WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName);

        this._style = style;
    }

    // Public

    layout()
    {
        super.layout();

        this.element.removeChildren();

        let properties = this._propertiesToRender;
        this.element.classList.toggle("no-properties", !properties.length);

        for (let property of properties)
            this.element.append(new WI.SpreadsheetStyleProperty(property).element);
    }

    get style()
    {
        return this._style;
    }

    set style(style)
    {
        if (this._style === style)
            return;

        this._style = style || null;

        this.needsLayout();
    }

    // Private

    get _propertiesToRender()
    {
        if (this._style._styleSheetTextRange)
            return this._style.allVisibleProperties;

        return this._style.allProperties;
    }
};

WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName = "spreadsheet-style-declaration-editor";

WI.SpreadsheetStyleProperty = class SpreadsheetStyleProperty extends WI.Object
{
    constructor(property)
    {
        super();

        this._property = property;
        this._element = document.createElement("div");
        this._element.classList.add("property");

        this._update();
    }

    // Public

    get element() { return this._element; }

    // Private

    _update()
    {
        this.element.removeChildren();
        this.element.classList.toggle("disabled", !this._property.enabled);

        if (this._property.editable) {
            this._checkboxElement = this.element.appendChild(document.createElement("input"));
            this._checkboxElement.classList.add("property-toggle");
            this._checkboxElement.type = "checkbox";
            this._checkboxElement.checked = this._property.enabled;
            this._checkboxElement.addEventListener("change", () => {
                let disabled = !this._checkboxElement.checked;
                this._property.commentOut(disabled);
                this._update();
            });
        }

        if (!this._property.enabled)
            this.element.append("/* ");

        let nameElement = this.element.appendChild(document.createElement("span"));
        nameElement.classList.add("name");
        nameElement.textContent = this._property.name;

        this.element.append(": ");

        let valueElement = this.element.appendChild(document.createElement("span"));
        valueElement.classList.add("value");
        valueElement.textContent = this._property.value;

        this.element.append(";");

        if (!this._property.enabled)
            this.element.append(" */");
    }
};

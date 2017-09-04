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
            this.element.append(this._renderProperty(property));
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

        this.needsLayout(WI.View.LayoutReason.Dirty);
    }

    // Private

    get _propertiesToRender()
    {
        if (this._style._styleSheetTextRange)
            return this._style.visibleProperties;

        return this._style.properties;
    }

    _renderProperty(cssProperty)
    {
        let propertyElement = document.createElement("div");
        propertyElement.classList.add("property");

        let nameElement = propertyElement.appendChild(document.createElement("span"));
        nameElement.classList.add("name");
        nameElement.textContent = cssProperty.name;

        propertyElement.append(": ");

        let valueElement = propertyElement.appendChild(document.createElement("span"));
        valueElement.classList.add("value");
        valueElement.textContent = cssProperty.value;

        propertyElement.append(";");

        return propertyElement;
    }
};

WI.SpreadsheetCSSStyleDeclarationEditor.StyleClassName = "spreadsheet-style-declaration-editor";

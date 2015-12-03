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

WebInspector.VisualStyleKeywordCheckbox = class VisualStyleKeywordCheckbox extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, text, value, layoutReversed)
    {
        super(propertyNames, text, null, null, "keyword-checkbox", layoutReversed);

        this._checkboxElement = document.createElement("input");
        this._checkboxElement.type = "checkbox";
        this._checkboxElement.addEventListener("change", this._valueDidChange.bind(this));
        this.contentElement.appendChild(this._checkboxElement);

        this._value = value.toLowerCase().replace(/\s/g, "-") || null;
    }

    // Public

    get value()
    {
        return this._checkboxElement.checked ? this._value : null;
    }

    set value(value)
    {
        this._checkboxElement.checked = value === this._value;
    }

    get synthesizedValue()
    {
        return this.value;
    }

    // Private

    _toggleTabbingOfSelectableElements(disabled)
    {
        this._checkboxElement.tabIndex = disabled ? "-1" : null;
    }
};

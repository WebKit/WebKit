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

WI.VisualStyleURLInput = class VisualStyleURLInput extends WI.VisualStylePropertyEditor
{
    constructor(propertyNames, text, possibleValues, layoutReversed)
    {
        super(propertyNames, text, possibleValues, null, "url-input", layoutReversed);

        this._urlInputElement = document.createElement("input");
        this._urlInputElement.type = "url";
        this._urlInputElement.placeholder = WI.UIString("Enter a URL");
        this._urlInputElement.addEventListener("keyup", this.debounce(250)._valueDidChange);
        this.contentElement.appendChild(this._urlInputElement);
    }

    // Public

    get value()
    {
        return this._urlInputElement.value;
    }

    set value(value)
    {
        if ((value && value === this.value) || this._propertyMissing)
            return;

        this._urlInputElement.value = value;
    }

    get synthesizedValue()
    {
        let value = this.value;
        if (!value || !value.length)
            return null;

        if (this.valueIsSupportedKeyword(value))
            return value;

        return "url(" + value + ")";
    }

    // Protected

    parseValue(text)
    {
        if (this.valueIsSupportedKeyword(text))
            return [text, text];

        return /^(?:url\(\s*)([^\)]*)(?:\s*\)\s*;?)$/.exec(text);
    }
};

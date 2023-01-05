/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

WI.FontVariationDetailsSectionRow = class FontVariationDetailsSectionRow extends WI.DetailsSectionRow
{
    constructor(fontVariationAxis, abortSignal)
    {
        super();

        this.element.classList.add("font-variation");

        let {name, tag, value, defaultValue, minimumValue, maximumValue} = fontVariationAxis;

        this._labelElement = this.element.appendChild(document.createElement("div"));
        this._labelElement.className = "label";

        this._tagElement = this.element.appendChild(document.createElement("div"));
        this._tagElement.className = "tag";

        this._variationRangeElement = this.element.appendChild(document.createElement("div"));
        this._variationRangeElement.className = "variation-range";

        this._inputRangeElement = this._variationRangeElement.appendChild(document.createElement("input"));
        this._inputRangeElement.type = "range";
        this._inputRangeElement.name = tag;
        this._inputRangeElement.min = minimumValue;
        this._inputRangeElement.max = maximumValue;
        this._inputRangeElement.value = value ?? defaultValue;
        this._inputRangeElement.step = this._getAxisResolution(minimumValue, maximumValue);

        this._variationMinValueElement = this._variationRangeElement.appendChild(document.createElement("div"));
        this._variationMinValueElement.className = "variation-minvalue";
        this._variationMinValueElement.textContent = this._formatAxisValueAsString(minimumValue);

        this._variationMaxValueElement = this._variationRangeElement.appendChild(document.createElement("div"));
        this._variationMaxValueElement.className = "variation-maxvalue";
        this._variationMaxValueElement.textContent = this._formatAxisValueAsString(maximumValue);

        this._inputRangeElement.addEventListener("input", (event) => {
            this.dispatchEventToListeners(WI.FontVariationDetailsSectionRow.Event.VariationValueChanged, {tag: event.target.name, value: event.target.value});
        }, {signal: abortSignal});

        this._valueElement = this.element.appendChild(document.createElement("div"));
        this._valueElement.className = "value";

        this.tag = tag;
        this.label = name;
        this.value = value ?? defaultValue;

        this._defaultValue = defaultValue;
    }

    // Public

    get tag()
    {
        return this._tag;
    }

    set tag(value)
    {
        this._tag = value;
        this._tagElement.textContent = value;
    }

    get label()
    {
        return this._label;
    }

    set label(label)
    {
        this._label = label || "";

        if (this._label instanceof Node) {
            this._labelElement.removeChildren();
            this._labelElement.appendChild(this._label);
        } else
            this._labelElement.textContent = this._label;
    }

    get value()
    {
        return this._value;
    }

    set value(value)
    {
        this._value = value ?? this._defaultValue;

        this._inputRangeElement.value = this._value;
        this._valueElement.textContent = this._formatAxisValueAsString(this._value);
    }

    // Private

    _formatAxisValueAsString(value)
    {
        const options = {
            minimumFractionDigits: 0,
            maximumFractionDigits: 2,
            useGrouping: false,
        }
        return value.toLocaleString(undefined, options);
    }

    _getAxisResolution(min, max)
    {
        let delta = max - min;
        if (delta <= 1)
            return 0.01;
        
        if (delta <= 10)
            return 0.1;

        return 1;
    }
};

WI.FontVariationDetailsSectionRow.Event = {
    VariationValueChanged: "font-variation-value-changed",
};

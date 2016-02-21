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

WebInspector.VisualStyleUnitSlider = class VisualStyleUnitSlider extends WebInspector.VisualStylePropertyEditor
{
    constructor(propertyNames, label, layoutReversed)
    {
        super(propertyNames, label, null, null, "unit-slider", layoutReversed);

        this._slider = new WebInspector.Slider;
        this._slider.delegate = this;
        this.contentElement.appendChild(this._slider.element);
    }

    // Public

    set value(value)
    {
        let knobX = parseFloat(value);
        if (isNaN(knobX))
            knobX = parseFloat(this._updatedValues.placeholder) || 0;

        this._slider.knobX = knobX;
    }

    get value()
    {
        return Math.round(this._slider.value * 100) / 100;
    }

    get synthesizedValue()
    {
        return this.value;
    }

    recalculateWidth(value)
    {
        this._slider.recalculateKnobX();
    }

    sliderValueDidChange(slider, value)
    {
        this._valueDidChange();
    }
};

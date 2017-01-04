/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.ColorPicker = class ColorPicker extends WebInspector.Object
{
    constructor()
    {
        super();

        this._colorWheel = new WebInspector.ColorWheel;
        this._colorWheel.delegate = this;
        this._colorWheel.dimension = 200;

        this._brightnessSlider = new WebInspector.Slider;
        this._brightnessSlider.delegate = this;
        this._brightnessSlider.element.classList.add("brightness");

        this._opacitySlider = new WebInspector.Slider;
        this._opacitySlider.delegate = this;
        this._opacitySlider.element.classList.add("opacity");

        let colorInputsContainerElement = document.createElement("div");
        colorInputsContainerElement.classList.add("color-inputs");

        function createColorInput(label, {min, max, step, units} = {min: 0, max: 1, step: 0.01}) {
            let containerElement = colorInputsContainerElement.createChild("div");

            containerElement.append(label);

            let numberInputElement = containerElement.createChild("input");
            numberInputElement.type = "number";
            numberInputElement.min = min;
            numberInputElement.max = max;
            numberInputElement.step = step;
            numberInputElement.addEventListener("input", this._handleColorInputInput.bind(this));

            if (units && units.length)
                containerElement.append(units);

            return {containerElement, numberInputElement};
        }

        this._colorInputs = new Map([
            ["R", createColorInput.call(this, "R", {max: 255, step: 1})],
            ["G", createColorInput.call(this, "G", {max: 255, step: 1})],
            ["B", createColorInput.call(this, "B", {max: 255, step: 1})],
            ["H", createColorInput.call(this, "H", {max: 360, step: 1})],
            ["S", createColorInput.call(this, "S", {units: "%"})],
            ["L", createColorInput.call(this, "L", {units: "%"})],
            ["A", createColorInput.call(this, "A")]
        ]);

        this._element = document.createElement("div");
        this._element.classList.add("color-picker");

        this._element.appendChild(this._colorWheel.element);
        this._element.appendChild(this._brightnessSlider.element);
        this._element.appendChild(this._opacitySlider.element);
        this._element.appendChild(colorInputsContainerElement);

        this._opacity = 0;
        this._opacityPattern = "url(Images/Checkers.svg)";

        this._color = WebInspector.Color.fromString("white");

        this._dontUpdateColor = false;
    }

    // Public

    get element()
    {
        return this._element;
    }

    set brightness(brightness)
    {
        if (brightness === this._brightness)
            return;

        this._colorWheel.brightness = brightness;

        this._updateColor();
        this._updateSliders(this._colorWheel.rawColor, this._colorWheel.tintedColor);
    }

    set opacity(opacity)
    {
        if (opacity === this._opacity)
            return;

        this._opacity = opacity;
        this._updateColor();
    }

    get colorWheel()
    {
        return this._colorWheel;
    }

    get color()
    {
        return this._color;
    }

    set color(color)
    {
        if (!color)
            return;

        this._dontUpdateColor = true;

        this._color = color;

        this._colorWheel.tintedColor = this._color;
        this._brightnessSlider.value = this._colorWheel.brightness;

        this._opacitySlider.value = this._color.alpha;
        this._updateSliders(this._colorWheel.rawColor, this._color);

        this._showColorComponentInputs();

        this._dontUpdateColor = false;
    }

    colorWheelColorDidChange(colorWheel)
    {
        this._updateColor();
        this._updateSliders(this._colorWheel.rawColor, this._colorWheel.tintedColor);
    }

    sliderValueDidChange(slider, value)
    {
        if (slider === this._opacitySlider)
            this.opacity = value;
        else if (slider === this._brightnessSlider)
            this.brightness = value;

        this._updateColor();
    }

    // Private

    _updateColor()
    {
        if (this._dontUpdateColor)
            return;

        let opacity = Math.round(this._opacity * 100) / 100;

        let format = this._color.format;
        let components = null;
        if (format === WebInspector.Color.Format.HSL || format === WebInspector.Color.Format.HSLA) {
            components = this._colorWheel.tintedColor.hsl.concat(opacity);
            if (opacity !== 1)
                format = WebInspector.Color.Format.HSLA;
        } else {
            components = this._colorWheel.tintedColor.rgb.concat(opacity);
            if (opacity !== 1 && format === WebInspector.Color.Format.RGB)
                format = WebInspector.Color.Format.RGBA;
        }

        this._color = new WebInspector.Color(format, components);

        this._showColorComponentInputs();

        this.dispatchEventToListeners(WebInspector.ColorPicker.Event.ColorChanged, {color: this._color});
    }

    _updateSliders(rawColor, tintedColor)
    {
        var rgb = this._colorWheel.tintedColor.rgb;
        var opaque = new WebInspector.Color(WebInspector.Color.Format.RGBA, rgb.concat(1)).toString();
        var transparent = new WebInspector.Color(WebInspector.Color.Format.RGBA, rgb.concat(0)).toString();

        this._opacitySlider.element.style.backgroundImage = "linear-gradient(90deg, " + transparent + ", " + opaque + "), " + this._opacityPattern;
        this._brightnessSlider.element.style.backgroundImage = "linear-gradient(90deg, black, " + rawColor + ")";
    }

    _showColorComponentInputs()
    {
        function updateColorInput(key, value) {
            let {containerElement, numberInputElement} = this._colorInputs.get(key);
            numberInputElement.value = value;
            containerElement.hidden = false;
        }

        for (let {containerElement} of this._colorInputs.values())
            containerElement.hidden = true;

        switch (this._color.format) {
        case WebInspector.Color.Format.RGB:
        case WebInspector.Color.Format.RGBA:
            var [r, g, b] = this._color.rgb;
            updateColorInput.call(this, "R", r);
            updateColorInput.call(this, "G", g);
            updateColorInput.call(this, "B", b);
            break;

        case WebInspector.Color.Format.HSL:
        case WebInspector.Color.Format.HSLA:
            var [h, s, l] = this._color.hsl;
            updateColorInput.call(this, "H", h);
            updateColorInput.call(this, "S", s);
            updateColorInput.call(this, "L", l);
            break;

        default:
            return;
        }

        if (this._color.format === WebInspector.Color.Format.RGBA || this._color.format === WebInspector.Color.Format.HSLA)
            updateColorInput.call(this, "A", this._color.alpha);
    }

    _handleColorInputInput(event)
    {
        let r = this._colorInputs.get("R").numberInputElement.value;
        let g = this._colorInputs.get("G").numberInputElement.value;
        let b = this._colorInputs.get("B").numberInputElement.value;
        let h = this._colorInputs.get("H").numberInputElement.value;
        let s = this._colorInputs.get("S").numberInputElement.value;
        let l = this._colorInputs.get("L").numberInputElement.value;
        let a = this._colorInputs.get("A").numberInputElement.value;

        let colorString = "";

        switch (this._color.format) {
        case WebInspector.Color.Format.RGB:
            colorString = `rgb(${r}, ${g}, ${b})`;
            break;

        case WebInspector.Color.Format.RGBA:
            colorString = `rgba(${r}, ${g}, ${b}, ${a})`;
            break;

        case WebInspector.Color.Format.HSL:
            colorString = `hsl(${h}, ${s}%, ${l}%)`;
            break;

        case WebInspector.Color.Format.HSLA:
            colorString = `hsla(${h}, ${s}%, ${l}%, ${a})`;
            break;

        default:
            WebInspector.reportInternalError(`Input event fired for invalid color format "${this._color.format}"`);
            return;
        }

        this.color = WebInspector.Color.fromString(colorString);

        this.dispatchEventToListeners(WebInspector.ColorPicker.Event.ColorChanged, {color: this._color});
    }
};

WebInspector.ColorPicker.Event = {
    ColorChanged: "css-color-picker-color-changed"
};

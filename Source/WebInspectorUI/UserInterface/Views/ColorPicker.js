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

WI.ColorPicker = class ColorPicker extends WI.Object
{
    constructor()
    {
        super();

        this._colorWheel = new WI.ColorWheel(this, 200);

        this._brightnessSlider = new WI.Slider;
        this._brightnessSlider.delegate = this;
        this._brightnessSlider.element.classList.add("brightness");

        this._opacitySlider = new WI.Slider;
        this._opacitySlider.delegate = this;
        this._opacitySlider.element.classList.add("opacity");

        let colorInputsContainerElement = document.createElement("div");
        colorInputsContainerElement.classList.add("color-inputs");

        let createColorInput = (label, {min, max, step, units} = {}) => {
            let containerElement = colorInputsContainerElement.createChild("div");

            containerElement.append(label);

            let numberInputElement = containerElement.createChild("input");
            numberInputElement.type = "number";
            numberInputElement.min = min || 0;
            numberInputElement.max = max || 100;
            numberInputElement.step = step || 1;
            numberInputElement.addEventListener("input", this._handleColorInputInput.bind(this));

            if (units && units.length)
                containerElement.append(units);

            return {containerElement, numberInputElement};
        };

        this._colorInputs = new Map([
            ["R", createColorInput("R", {max: 255})],
            ["G", createColorInput("G", {max: 255})],
            ["B", createColorInput("B", {max: 255})],
            ["H", createColorInput("H", {max: 360})],
            ["S", createColorInput("S", {units: "%"})],
            ["L", createColorInput("L", {units: "%"})],
            ["A", createColorInput("A", {max: 1, step: 0.01})]
        ]);

        this._element = document.createElement("div");
        this._element.classList.add("color-picker");

        this._element.appendChild(this._colorWheel.element);
        this._element.appendChild(this._brightnessSlider.element);
        this._element.appendChild(this._opacitySlider.element);
        this._element.appendChild(colorInputsContainerElement);

        this._opacity = 0;
        this._opacityPattern = "url(Images/Checkers.svg)";

        this._color = WI.Color.fromString("white");

        this._dontUpdateColor = false;

        this._enableColorComponentInputs = true;
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
        console.assert(color instanceof WI.Color);

        this._dontUpdateColor = true;

        let formatChanged = !this._color || this._color.format !== color.format;

        this._color = color;

        this._colorWheel.tintedColor = this._color;
        this._brightnessSlider.value = this._colorWheel.brightness / 100;

        this._opacitySlider.value = this._color.alpha;
        this._updateSliders(this._colorWheel.rawColor, this._color);

        this._showColorComponentInputs();

        if (formatChanged)
            this._handleFormatChange();

        this._dontUpdateColor = false;
    }

    set enableColorComponentInputs(value)
    {
        this._enableColorComponentInputs = value;

        this._showColorComponentInputs();
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
            this.brightness = value * 100;
    }

    // Private

    _updateColor()
    {
        if (this._dontUpdateColor)
            return;

        let opacity = Math.round(this._opacity * 100) / 100;

        let format = this._color.format;
        let components = null;
        if (format === WI.Color.Format.HSL || format === WI.Color.Format.HSLA) {
            components = this._colorWheel.tintedColor.hsl.concat(opacity);
            if (opacity !== 1)
                format = WI.Color.Format.HSLA;
        } else {
            components = this._colorWheel.tintedColor.rgb.concat(opacity);
            if (opacity !== 1 && format === WI.Color.Format.RGB)
                format = WI.Color.Format.RGBA;
        }

        let formatChanged = this._color.format === format;

        this._color = new WI.Color(format, components);

        this._showColorComponentInputs();

        this.dispatchEventToListeners(WI.ColorPicker.Event.ColorChanged, {color: this._color});

        if (formatChanged)
            this._handleFormatChange();
    }

    _updateSliders(rawColor, tintedColor)
    {
        var rgb = this._colorWheel.tintedColor.rgb;
        var opaque = new WI.Color(WI.Color.Format.RGBA, rgb.concat(1)).toString();
        var transparent = new WI.Color(WI.Color.Format.RGBA, rgb.concat(0)).toString();

        this._brightnessSlider.element.style.setProperty("background-image", `linear-gradient(90deg, black, ${rawColor}, white)`);

        this._opacitySlider.element.style.setProperty("background-image", "linear-gradient(90deg, " + transparent + ", " + opaque + "), " + this._opacityPattern);
    }

    _handleFormatChange()
    {
        this._element.classList.toggle("hide-inputs", this._color.format !== WI.Color.Format.Keyword
            && this._color.format !== WI.Color.Format.RGB
            && this._color.format !== WI.Color.Format.RGBA
            && this._color.format !== WI.Color.Format.HEX
            && this._color.format !== WI.Color.Format.ShortHEX
            && this._color.format !== WI.Color.Format.HEXAlpha
            && this._color.format !== WI.Color.Format.ShortHEXAlpha
            && this._color.format !== WI.Color.Format.HSL
            && this._color.format !== WI.Color.Format.HSLA);

        this.dispatchEventToListeners(WI.ColorPicker.Event.FormatChanged);
    }

    _showColorComponentInputs()
    {
        for (let {containerElement} of this._colorInputs.values())
            containerElement.hidden = true;

        if (!this._enableColorComponentInputs)
            return;

        function updateColorInput(key, value) {
            let {containerElement, numberInputElement} = this._colorInputs.get(key);
            numberInputElement.value = value;
            containerElement.hidden = false;
        }

        switch (this._color.format) {
        case WI.Color.Format.RGB:
        case WI.Color.Format.RGBA:
        case WI.Color.Format.HEX:
        case WI.Color.Format.ShortHEX:
        case WI.Color.Format.HEXAlpha:
        case WI.Color.Format.ShortHEXAlpha:
        case WI.Color.Format.Keyword:
            var [r, g, b] = this._color.rgb;
            updateColorInput.call(this, "R", Math.round(r));
            updateColorInput.call(this, "G", Math.round(g));
            updateColorInput.call(this, "B", Math.round(b));
            break;

        case WI.Color.Format.HSL:
        case WI.Color.Format.HSLA:
            var [h, s, l] = this._color.hsl;
            updateColorInput.call(this, "H", h.maxDecimals(2));
            updateColorInput.call(this, "S", s.maxDecimals(2));
            updateColorInput.call(this, "L", l.maxDecimals(2));
            break;

        default:
            return;
        }

        if ((this._color.format === WI.Color.Format.Keyword && this._color.alpha !== 1)
            || this._color.format === WI.Color.Format.RGBA
            || this._color.format === WI.Color.Format.HSLA
            || this._color.format === WI.Color.Format.HEXAlpha
            || this._color.format === WI.Color.Format.ShortHEXAlpha) {
            updateColorInput.call(this, "A", this._color.alpha);
        }
    }

    _handleColorInputInput(event)
    {
        if (!this._enableColorComponentInputs) {
            WI.reportInternalError("Input event fired for disabled color component input");
            return;
        }

        let r = this._colorInputs.get("R").numberInputElement.value;
        let g = this._colorInputs.get("G").numberInputElement.value;
        let b = this._colorInputs.get("B").numberInputElement.value;
        let h = this._colorInputs.get("H").numberInputElement.value;
        let s = this._colorInputs.get("S").numberInputElement.value;
        let l = this._colorInputs.get("L").numberInputElement.value;
        let a = this._colorInputs.get("A").numberInputElement.value;

        let colorString = "";
        let oldFormat = this._color.format;

        switch (oldFormat) {
        case WI.Color.Format.RGB:
        case WI.Color.Format.HEX:
        case WI.Color.Format.ShortHEX:
        case WI.Color.Format.Keyword:
            colorString = `rgb(${r}, ${g}, ${b})`;
            break;

        case WI.Color.Format.RGBA:
        case WI.Color.Format.HEXAlpha:
        case WI.Color.Format.ShortHEXAlpha:
            colorString = `rgba(${r}, ${g}, ${b}, ${a})`;
            break;

        case WI.Color.Format.HSL:
            colorString = `hsl(${h}, ${s}%, ${l}%)`;
            break;

        case WI.Color.Format.HSLA:
            colorString = `hsla(${h}, ${s}%, ${l}%, ${a})`;
            break;

        default:
            WI.reportInternalError(`Input event fired for invalid color format "${this._color.format}"`);
            return;
        }

        let newColor = WI.Color.fromString(colorString);
        if (newColor.toString() === this._color.toString())
            return;

        this.color = newColor;
        this._color.format = oldFormat;

        this.dispatchEventToListeners(WI.ColorPicker.Event.ColorChanged, {color: this._color});
    }
};

WI.ColorPicker.Event = {
    ColorChanged: "css-color-picker-color-changed",
    FormatChanged: "css-color-picker-format-changed",
};

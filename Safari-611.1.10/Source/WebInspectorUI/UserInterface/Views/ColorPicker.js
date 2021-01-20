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

        this._colorSquare = new WI.ColorSquare(this, 200);

        this._hueSlider = new WI.Slider;
        this._hueSlider.delegate = this;
        this._hueSlider.element.classList.add("hue");

        this._opacitySlider = new WI.Slider;
        this._opacitySlider.delegate = this;
        this._opacitySlider.element.classList.add("opacity");

        this._colorInputs = [];
        this._colorInputsFormat = null;
        this._colorInputsHasAlpha = null;

        this._colorInputsContainerElement = document.createElement("div");
        this._colorInputsContainerElement.classList.add("color-inputs");
        this._colorInputsContainerElement.addEventListener("input", this._handleColorInputsContainerInput.bind(this));

        this._element = document.createElement("div");
        this._element.classList.add("color-picker");

        let wrapper = this._element.appendChild(document.createElement("div"));
        wrapper.className = "wrapper";
        wrapper.appendChild(this._colorSquare.element);
        wrapper.appendChild(this._hueSlider.element);
        wrapper.appendChild(this._opacitySlider.element);

        this._element.appendChild(this._colorInputsContainerElement);

        this._opacity = 0;
        this._opacityPattern = "url(Images/Checkers.svg)";

        this._color = WI.Color.fromString("white");

        this._dontUpdateColor = false;

        this._enableColorComponentInputs = true;
    }

    // Public

    get element() { return this._element; }
    get colorSquare() { return this._colorSquare; }

    set opacity(opacity)
    {
        if (opacity === this._opacity)
            return;

        this._opacity = opacity;
        this._updateColor();
    }

    get color()
    {
        return this._color;
    }

    set color(color)
    {
        console.assert(color instanceof WI.Color);

        this._dontUpdateColor = true;

        this._color = color;

        this._colorSquare.tintedColor = this._color;

        this._hueSlider.value = this._color.hsl[0] / 360;

        this._opacitySlider.value = this._color.alpha;
        this._updateOpacitySlider();

        this._showColorComponentInputs();
        this._updateColorGamut();

        this._dontUpdateColor = false;
    }

    set enableColorComponentInputs(value)
    {
        this._enableColorComponentInputs = value;
        this._element.classList.toggle("hide-inputs", !this._enableColorComponentInputs);
    }

    focus()
    {
        this._colorSquare.element.focus();
    }

    colorSquareColorDidChange(colorSquare)
    {
        this._updateColor();
        this._updateOpacitySlider();
    }

    sliderValueDidChange(slider, value)
    {
        if (slider === this._opacitySlider)
            this.opacity = value;
        else if (slider === this._hueSlider) {
            this._colorSquare.hue = value * 360;
            this._updateColor();
            this._updateOpacitySlider();
        }
    }

    // Private

    _updateColor()
    {
        if (this._dontUpdateColor)
            return;

        let opacity = Math.round(this._opacity * 100) / 100;

        let format = this._color.format;
        let gamut = this._color.gamut;
        let components = null;
        if (format === WI.Color.Format.HSL || format === WI.Color.Format.HSLA) {
            components = this._colorSquare.tintedColor.hsl.concat(opacity);
            if (opacity !== 1)
                format = WI.Color.Format.HSLA;
        } else if (format === WI.Color.Format.ColorFunction)
            components = this._colorSquare.tintedColor.normalizedRGB.concat(opacity);
        else {
            components = this._colorSquare.tintedColor.rgb.concat(opacity);
            if (opacity !== 1 && format === WI.Color.Format.RGB)
                format = WI.Color.Format.RGBA;
        }

        this._color = new WI.Color(format, components, gamut);

        this._showColorComponentInputs();

        this.dispatchEventToListeners(WI.ColorPicker.Event.ColorChanged, {color: this._color});

        this._updateColorGamut();
    }

    _updateOpacitySlider()
    {
        let color = this._colorSquare.tintedColor;

        let rgb = color.format === WI.Color.Format.ColorFunction ? color.normalizedRGB : color.rgb;
        let gamut = color.gamut;
        let format = gamut === WI.Color.Gamut.DisplayP3 ? WI.Color.Format.ColorFunction : WI.Color.Format.RGBA;
        let opaque = new WI.Color(format, rgb.concat(1), gamut).toString();
        let transparent = new WI.Color(format, rgb.concat(0), gamut).toString();
        this._opacitySlider.element.style.setProperty("background-image", "linear-gradient(0deg, " + transparent + ", " + opaque + "), " + this._opacityPattern);
    }

    _updateColorGamut()
    {
        this._element.classList.toggle("gamut-p3", this._color.gamut === WI.Color.Gamut.DisplayP3);
    }

    _createColorInputsIfNeeded()
    {
        let hasAlpha = this._color.alpha !== 1;
        if ((this._color.format === this._colorInputsFormat) && (hasAlpha === this._colorInputsHasAlpha))
            return;

        this._colorInputsFormat = this._color.format;
        this._colorInputsHasAlpha = hasAlpha;

        this._colorInputs = [];
        this._colorInputsContainerElement.removeChildren();

        let createColorInput = (label, {max, step, units} = {}) => {
            let containerElement = this._colorInputsContainerElement.createChild("div");
            containerElement.append(label);

            let numberInputElement = containerElement.createChild("input");
            numberInputElement.type = "number";
            numberInputElement.min = 0;
            numberInputElement.max = max;
            numberInputElement.step = step || 1;
            this._colorInputs.push(numberInputElement);

            if (units && units.length)
                containerElement.append(units);
        };

        switch (this._color.format) {
        case WI.Color.Format.RGB:
        case WI.Color.Format.RGBA:
        case WI.Color.Format.HEX:
        case WI.Color.Format.ShortHEX:
        case WI.Color.Format.HEXAlpha:
        case WI.Color.Format.ShortHEXAlpha:
        case WI.Color.Format.Keyword:
            createColorInput("R", {max: 255});
            createColorInput("G", {max: 255});
            createColorInput("B", {max: 255});
            break;

        case WI.Color.Format.HSL:
        case WI.Color.Format.HSLA:
            createColorInput("H", {max: 360});
            createColorInput("S", {max: 100, units: "%"});
            createColorInput("L", {max: 100, units: "%"});
            break;

        case WI.Color.Format.ColorFunction:
            createColorInput("R", {max: 1, step: 0.01});
            createColorInput("G", {max: 1, step: 0.01});
            createColorInput("B", {max: 1, step: 0.01});
            break;

        default:
            console.error(`Unknown color format: ${this._color.format}`);
            return;
        }

        if (this._color.alpha !== 1
            || this._color.format === WI.Color.Format.RGBA
            || this._color.format === WI.Color.Format.HSLA
            || this._color.format === WI.Color.Format.HEXAlpha
            || this._color.format === WI.Color.Format.ShortHEXAlpha) {
            createColorInput("A", {max: 1, step: 0.01});
        }
    }

    _showColorComponentInputs()
    {
        if (!this._enableColorComponentInputs)
            return;

        this._createColorInputsIfNeeded();

        let components = [];
        switch (this._color.format) {
        case WI.Color.Format.RGB:
        case WI.Color.Format.RGBA:
        case WI.Color.Format.HEX:
        case WI.Color.Format.ShortHEX:
        case WI.Color.Format.HEXAlpha:
        case WI.Color.Format.ShortHEXAlpha:
        case WI.Color.Format.Keyword:
            components = this._color.rgb.map((value) => Math.round(value));
            break;

        case WI.Color.Format.HSL:
        case WI.Color.Format.HSLA:
            components = this._color.hsl.map((value) => value.maxDecimals(2));
            break;

        case WI.Color.Format.ColorFunction:
            components = this._color.normalizedRGB.map((value) => value.maxDecimals(4));
            break;

        default:
            console.error(`Unknown color format: ${this._color.format}`);
        }

        if (this._color.alpha !== 1)
            components.push(this._color.alpha);

        console.assert(this._colorInputs.length === components.length);
        for (let i = 0; i < components.length; ++i)
            this._colorInputs[i].value = components[i];
    }

    _handleColorInputsContainerInput(event)
    {
        let components = this._colorInputs.map((input) => {
            return isNaN(input.valueAsNumber) ? 0 : input.valueAsNumber;
        });
        this.color = new WI.Color(this._color.format, components, this._color.gamut);
        this.dispatchEventToListeners(WI.ColorPicker.Event.ColorChanged, {color: this._color});
    }
};

WI.ColorPicker.Event = {
    ColorChanged: "css-color-picker-color-changed",
};

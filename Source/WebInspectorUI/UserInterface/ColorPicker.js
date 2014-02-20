/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.ColorPicker = function()
{
    WebInspector.Object.call(this);

    this._colorWheel = new WebInspector.ColorWheel();
    this._colorWheel.delegate = this;
    this._colorWheel.dimension = 200;

    this._brightnessSlider = new WebInspector.Slider();
    this._brightnessSlider.delegate = this;
    this._brightnessSlider.element.classList.add("brightness");

    this._opacitySlider = new WebInspector.Slider();
    this._opacitySlider.delegate = this;
    this._opacitySlider.element.classList.add("opacity");

    this._element = document.createElement("div");
    this._element.className = "color-picker";

    this._element.appendChild(this._colorWheel.element);
    this._element.appendChild(this._brightnessSlider.element);
    this._element.appendChild(this._opacitySlider.element);

    this._opacity = 0;
    this._opacityPattern = 'url(Images/Checkers.svg)';

    this._color = "white";
};

WebInspector.ColorPicker.Event = {
    ColorChanged: "css-color-picker-color-changed"
};

WebInspector.ColorPicker.prototype = {
    contructor: WebInspector.ColorPicker,
    __proto__: WebInspector.Object.prototype,

    // Public
    
    get element()
    {
        return this._element;
    },
    
    set brightness(brightness)
    {
        if (brightness === this._brightness)
            return;

        this._colorWheel.brightness = brightness;

        this._updateColor();
        this._updateSliders(this._colorWheel.rawColor, this._colorWheel.tintedColor);
    },
    
    set opacity(opacity)
    {
        if (opacity === this._opacity)
            return;

        this._opacity = opacity;
        this._updateColor();
    },

    get colorWheel()
    {
        return this._colorWheel;
    },

    get color()
    {
        return this._color;
    },

    set color(color)
    {
        this._dontUpdateColor = true;

        this._colorFormat = color.format;

        this._colorWheel.tintedColor = color;
        this._brightnessSlider.value = this._colorWheel.brightness;

        this._opacitySlider.value = color.alpha;
        this._updateSliders(this._colorWheel.rawColor, color);

        delete this._dontUpdateColor;
    },

    colorWheelColorDidChange: function(colorWheel)
    {
        this._updateColor();
        this._updateSliders(this._colorWheel.rawColor, this._colorWheel.tintedColor);
    },

    sliderValueDidChange: function(slider, value)
    {
        if (slider === this._opacitySlider)
            this.opacity = value;
        else if (slider === this._brightnessSlider)
            this.brightness = value;
    },
    
    // Private
    
    _updateColor: function()
    {
        if (this._dontUpdateColor)
            return;

        var opacity = Math.round(this._opacity * 100) / 100;

        var components;
        if (this._colorFormat === WebInspector.Color.Format.HSL || this._colorFormat === WebInspector.Color.Format.HSLA)
            components = this._colorWheel.tintedColor.hsl.concat(opacity);
        else
            components = this._colorWheel.tintedColor.rgb.concat(opacity);

        this._color = new WebInspector.Color(this._colorFormat, components);
        this.dispatchEventToListeners(WebInspector.ColorPicker.Event.ColorChanged, {color: this._color});
    },

    _updateSliders: function(rawColor, tintedColor)
    {
        var rgb = this._colorWheel.tintedColor.rgb;
        var opaque = new WebInspector.Color(WebInspector.Color.Format.RGBA, rgb.concat(1)).toString();
        var transparent = new WebInspector.Color(WebInspector.Color.Format.RGBA, rgb.concat(0)).toString();

        this._opacitySlider.element.style.backgroundImage = "linear-gradient(90deg, " + transparent + ", " + opaque + "), " + this._opacityPattern;
        this._brightnessSlider.element.style.backgroundImage = "linear-gradient(90deg, black, " + rawColor + ")";
    }
};

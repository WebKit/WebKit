/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.ColorSquare = class ColorSquare
{
    constructor(delegate, dimension)
    {
        this._delegate = delegate;

        this._hue = 0;
        this._x = 0;
        this._y = 0;
        this._crosshairPosition = null;

        this._element = document.createElement("div");
        this._element.className = "color-square";

        let saturationGradientElement = this._element.appendChild(document.createElement("div"));
        saturationGradientElement.className = "saturation-gradient fill";

        let lightnessGradientElement = this._element.appendChild(document.createElement("div"));
        lightnessGradientElement.className = "lightness-gradient fill";

        this._element.addEventListener("mousedown", this);

        this._crosshairElement = this._element.appendChild(document.createElement("div"));
        this._crosshairElement.className = "crosshair";

        this.dimension = dimension;
    }

    // Public

    get element() { return this._element; }

    set dimension(dimension)
    {
        console.assert(!isNaN(dimension));
        if (dimension === this._dimension)
            return;

        this._dimension = dimension;
        this._element.style.width = this.element.style.height = `${this._dimension}px`;
        this._updateBaseColor();
    }

    get hue()
    {
        return this._hue;
    }

    set hue(hue)
    {
        this._hue = hue;
        this._updateBaseColor();
    }

    get tintedColor()
    {
        if (this._crosshairPosition)
            return new WI.Color(WI.Color.Format.HSL, [this._hue, this._saturation, this._lightness]);
        return new WI.Color(WI.Color.Format.HSLA, [0, 0, 0, 0]);
    }

    set tintedColor(tintedColor)
    {
        console.assert(tintedColor instanceof WI.Color);
        let hsl = tintedColor.hsl;
        let saturation = Number.constrain(hsl[1], 0, 100);
        let x = saturation / 100 * this._dimension;

        let lightness = hsl[2];

        // The color picker is HSB-based. (HSB is also known as HSV.)
        // Derive lightness by using HSB to HSL equation.
        let y = 2 * lightness / (200 - saturation);
        y = -1 * (y - 1) * this._dimension;

        this._setCrosshairPosition(new WI.Point(x, y));
    }

    get rawColor()
    {
        if (this._crosshairPosition)
            return new WI.Color(WI.Color.Format.HSL, [this._hue, this._saturation, 50]);

        return new WI.Color(WI.Color.Format.HSLA, WI.Color.Keywords.transparent);
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "mousedown":
            this._handleMousedown(event);
            break;
        case "mousemove":
            this._handleMousemove(event);
            break;
        case "mouseup":
            this._handleMouseup(event);
            break;
        }
    }

    // Private

    get _saturation()
    {
        let saturation = this._x / this._dimension;
        return Number.constrain(saturation, 0, 1) * 100;
    }

    get _brightness()
    {
        let brightness = 1 - (this._y / this._dimension);
        return Number.constrain(brightness, 0, 1) * 100;
    }

    get _lightness()
    {
        // The color picker is HSB-based. (HSB is also known as HSV.)
        // Derive lightness by using HSB to HSL equation.
        return (200 - this._saturation) * (this._brightness / 100) / 2;
    }

    _handleMousedown(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        window.addEventListener("mousemove", this, true);
        window.addEventListener("mouseup", this, true);
        this._updateColorForMouseEvent(event);

        // Prevent text selection.
        event.stop();
    }

    _handleMousemove(event)
    {
        this._updateColorForMouseEvent(event);
    }

    _handleMouseup(event)
    {
        window.removeEventListener("mousemove", this, true);
        window.removeEventListener("mouseup", this, true);
    }

    _updateColorForMouseEvent(event)
    {
        let point = window.webkitConvertPointFromPageToNode(this._element, new WebKitPoint(event.pageX, event.pageY));
        this._setCrosshairPosition(point);

        if (this._delegate && this._delegate.colorSquareColorDidChange)
            this._delegate.colorSquareColorDidChange(this);
    }

    _setCrosshairPosition(point)
    {
        this._crosshairPosition = point;
        this._x = Number.constrain(Math.round(point.x), 0, this._dimension);
        this._y = Number.constrain(Math.round(point.y), 0, this._dimension);
        this._crosshairElement.style.setProperty("transform", `translate(${this._x}px, ${this._y}px)`);
    }

    _updateBaseColor()
    {
        this._element.style.backgroundColor = `hsl(${this._hue}, 100%, 50%)`;
    }
};

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

WI.ColorWheel = class ColorWheel extends WI.Object
{
    constructor(delegate, dimension)
    {
        console.assert(!isNaN(dimension));

        super();

        this._delegate = delegate;

        this._brightness = 50;

        this._element = document.createElement("div");
        this._element.className = "color-wheel";

        this._canvasElement = this._element.appendChild(document.createElement("canvas"));
        this._canvasElement.addEventListener("mousedown", this);

        this._canvasContext = this._canvasElement.getContext("2d");

        this._crosshairElement = this._element.appendChild(document.createElement("div"));
        this._crosshairElement.className = "crosshair";

        this.dimension = dimension;
    }

    // Public

    get element() { return this._element; }

    set dimension(dimension)
    {
        if (dimension === this._dimension)
            return;

        this._dimension = dimension;

        this._element.style.width = this.element.style.height = `${this._dimension}px`;
        this._canvasElement.width = this._canvasElement.height = this._dimension * window.devicePixelRatio;

        let center = this._dimension / 2;
        this._setCrosshairPosition(new WI.Point(center, center));

        this._updateCanvas();
    }

    get brightness()
    {
        return this._brightness;
    }

    set brightness(brightness)
    {
        if (brightness === this._brightness)
            return;

        this._brightness = brightness;

        this._updateCanvas();
    }

    get tintedColor()
    {
        if (this._crosshairPosition)
            return new WI.Color(WI.Color.Format.HSL, [this._hue, this._saturation, this._brightness]);
        return new WI.Color(WI.Color.Format.HSLA, [0, 0, 0, 0]);
    }

    set tintedColor(tintedColor)
    {
        let hsl = tintedColor.hsl;

        let cosHue = Math.cos(hsl[0] * Math.PI / 180);
        let sinHue = Math.sin(hsl[0] * Math.PI / 180);
        let center = this._dimension / 2;
        let x = center + (sinHue * hsl[1]);
        let y = center - (cosHue * hsl[1]);
        this._setCrosshairPosition(new WI.Point(x, y));

        this.brightness = hsl[2];
    }

    get rawColor()
    {
        if (this._crosshairPosition)
            return new WI.Color(WI.Color.Format.HSL, [this._hue, this._saturation, 50]);
        return new WI.Color(WI.Color.Format.HSLA, [0, 0, 0, 0]);
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

    get _hue()
    {
        let center = this._dimension / 2;
        let hue = Math.atan2(this._crosshairPosition.x - center, center - this._crosshairPosition.y) * 180 / Math.PI;
        if (hue < 0)
            hue += 360;
        return hue;
    }

    get _saturation()
    {
        let center = this._dimension / 2;
        let xDis = (this._crosshairPosition.x - center) / center;
        let yDis = (this._crosshairPosition.y - center) / center;
        return Math.sqrt(Math.pow(xDis, 2) + Math.pow(yDis, 2)) * 100;
    }

    _handleMousedown(event)
    {
        window.addEventListener("mousemove", this, true);
        window.addEventListener("mouseup", this, true);

        this._updateColorForMouseEvent(event);
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
        var point = window.webkitConvertPointFromPageToNode(this._canvasElement, new WebKitPoint(event.pageX, event.pageY));

        this._setCrosshairPosition(point);

        if (this._delegate && typeof this._delegate.colorWheelColorDidChange === "function")
            this._delegate.colorWheelColorDidChange(this);
    }

    _setCrosshairPosition(point)
    {
        let radius = this._dimension / 2;
        let center = new WI.Point(radius, radius);

        // Prevents the crosshair from being dragged outside the wheel.
        if (center.distance(point) > radius) {
            let angle = Math.atan2(point.y - center.y, point.x - center.x);
            point = new WI.Point(center.x + radius * Math.cos(angle), center.y + radius * Math.sin(angle));
        }

        this._crosshairPosition = point;
        this._crosshairElement.style.setProperty("transform", "translate(" + Math.round(point.x) + "px, " + Math.round(point.y) + "px)");
    }

    _updateCanvas()
    {
        let dimension = this._dimension * window.devicePixelRatio;
        let center = dimension / 2;

        let imageData = this._canvasContext.createImageData(dimension, dimension);
        for (let y = 0; y < dimension; ++y) {
            for (let x = 0; x < dimension; ++x) {
                let xDis = (x - center) / center;
                let yDis = (y - center) / center;
                let saturation = Math.sqrt(Math.pow(xDis, 2) + Math.pow(yDis, 2)) * 100;
                if (saturation > 100)
                    continue;

                let hue = Math.atan2(x - center, center - y) * 180 / Math.PI;
                if (hue < 0)
                    hue += 360;

                let rgb = WI.Color.hsl2rgb(hue, saturation, this._brightness);

                let index = ((y * dimension) + x) * 4;
                imageData.data[index] = rgb[0];
                imageData.data[index + 1] = rgb[1];
                imageData.data[index + 2] = rgb[2];
                imageData.data[index + 3] = 255;
            }
        }

        this._canvasContext.putImageData(imageData, 0, 0);
    }
};

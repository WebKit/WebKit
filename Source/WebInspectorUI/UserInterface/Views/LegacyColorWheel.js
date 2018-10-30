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

WI.LegacyColorWheel = class LegacyColorWheel extends WI.Object
{
    constructor()
    {
        super();

        this._rawCanvas = document.createElement("canvas");
        this._tintedCanvas = document.createElement("canvas");
        this._finalCanvas = document.createElement("canvas");

        this._crosshair = document.createElement("div");
        this._crosshair.className = "crosshair";

        this._element = document.createElement("div");
        this._element.className = "color-wheel";

        this._element.appendChild(this._finalCanvas);
        this._element.appendChild(this._crosshair);

        this._finalCanvas.addEventListener("mousedown", this);
    }

    // Public

    set dimension(dimension)
    {
        this._element.style.width = this.element.style.height = `${dimension}px`;

        this._finalCanvas.width = this._tintedCanvas.width = this._rawCanvas.width = dimension * window.devicePixelRatio;
        this._finalCanvas.height = this._tintedCanvas.height = this._rawCanvas.height = dimension * window.devicePixelRatio;

        this._finalCanvas.style.width = this._finalCanvas.style.height = dimension + "px";

        this._dimension = dimension;
        // We shrink the radius a bit for better anti-aliasing.
        this._radius = dimension / 2 - 2;

        this._setCrosshairPosition(new WI.Point(dimension / 2, dimension / 2));

        this._drawRawCanvas();
        this._draw();
    }

    get element()
    {
        return this._element;
    }

    get brightness()
    {
        return this._brightness;
    }

    set brightness(brightness)
    {
        this._brightness = brightness;
        this._draw();
    }

    get tintedColor()
    {
        if (this._crosshairPosition)
            return this._colorAtPointWithBrightness(this._crosshairPosition.x * window.devicePixelRatio, this._crosshairPosition.y * window.devicePixelRatio, this._brightness);

        return new WI.Color(WI.Color.Format.RGBA, [0, 0, 0, 0]);
    }

    set tintedColor(tintedColor)
    {
        var data = this._tintedColorToPointAndBrightness(tintedColor);
        this._setCrosshairPosition(data.point);
        this.brightness = data.brightness;
    }

    get rawColor()
    {
        if (this._crosshairPosition)
            return this._colorAtPointWithBrightness(this._crosshairPosition.x * window.devicePixelRatio, this._crosshairPosition.y * window.devicePixelRatio, 1);

        return new WI.Color(WI.Color.Format.RGBA, [0, 0, 0, 0]);
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

    _pointInCircleForEvent(event)
    {
        function distance(a, b)
        {
            return Math.sqrt(Math.pow(a.x - b.x, 2) + Math.pow(a.y - b.y, 2));
        }

        function angleFromCenterToPoint(center, point)
        {
            return Math.atan2(point.y - center.y, point.x - center.x);
        }

        function pointOnCircumference(c, r, a)
        {
            return new WI.Point(c.x + r * Math.cos(a), c.y + r * Math.sin(a));
        }

        var dimension = this._dimension;
        var point = window.webkitConvertPointFromPageToNode(this._finalCanvas, new WebKitPoint(event.pageX, event.pageY));
        var center = new WI.Point(dimension / 2, dimension / 2);
        if (distance(point, center) > this._radius) {
            var angle = angleFromCenterToPoint(center, point);
            point = pointOnCircumference(center, this._radius, angle);
        }
        return point;
    }

    _updateColorForMouseEvent(event)
    {
        var point = this._pointInCircleForEvent(event);

        this._setCrosshairPosition(point);

        if (this.delegate && typeof this.delegate.colorWheelColorDidChange === "function")
            this.delegate.colorWheelColorDidChange(this);
    }

    _setCrosshairPosition(point)
    {
        this._crosshairPosition = point;
        this._crosshair.style.webkitTransform = "translate(" + Math.round(point.x) + "px, " + Math.round(point.y) + "px)";
    }

    _tintedColorToPointAndBrightness(color)
    {
        var rgb = color.rgb;
        var hsv = WI.Color.rgb2hsv(rgb[0], rgb[1], rgb[2]);
        var cosHue = Math.cos(hsv[0] * Math.PI / 180);
        var sinHue = Math.sin(hsv[0] * Math.PI / 180);
        var center = this._dimension / 2;
        var x = center + (center * cosHue * hsv[1]);
        var y = center - (center * sinHue * hsv[1]);
        return {
            point: new WI.Point(x, y),
            brightness: hsv[2]
        };
    }

    _drawRawCanvas() {
        var ctx = this._rawCanvas.getContext("2d");

        var dimension = this._dimension * window.devicePixelRatio;

        ctx.fillStyle = "white";
        ctx.fillRect(0, 0, dimension, dimension);

        var imageData = ctx.getImageData(0, 0, dimension, dimension);
        var data = imageData.data;
        for (var j = 0; j < dimension; ++j) {
            for (var i = 0; i < dimension; ++i) {
                var color = this._colorAtPointWithBrightness(i, j, 1);
                if (!color)
                    continue;
                var pos = (j * dimension + i) * 4;
                data[pos] = color.rgb[0];
                data[pos + 1] = color.rgb[1];
                data[pos + 2] = color.rgb[2];
            }
        }
        ctx.putImageData(imageData, 0, 0);
    }

    _colorAtPointWithBrightness(x, y, brightness)
    {
        var center = this._dimension / 2 * window.devicePixelRatio;
        var xDis = x - center;
        var yDis = y - center;
        var distance = Math.sqrt(xDis * xDis + yDis * yDis);

        if (distance - center > 0.001)
            return new WI.Color(WI.Color.Format.RGBA, [0, 0, 0, 0]);

        var h = Math.atan2(y - center, center - x) * 180 / Math.PI;
        h = (h + 180) % 360;
        var v = brightness;
        var s = Math.max(0, distance) / center;

        var rgb = WI.Color.hsv2rgb(h, s, v);
        return new WI.Color(WI.Color.Format.RGBA, [
            Math.round(rgb[0] * 255),
            Math.round(rgb[1] * 255),
            Math.round(rgb[2] * 255),
            1
        ]);
    }

    _drawTintedCanvas()
    {
        var ctx = this._tintedCanvas.getContext("2d");
        var dimension = this._dimension * window.devicePixelRatio;

        ctx.save();
        ctx.drawImage(this._rawCanvas, 0, 0, dimension, dimension);
        if (this._brightness !== 1) {
            ctx.globalAlpha = 1 - this._brightness;
            ctx.fillStyle = "black";
            ctx.fillRect(0, 0, dimension, dimension);
        }
        ctx.restore();
    }

    _draw()
    {
        this._drawTintedCanvas();

        var ctx = this._finalCanvas.getContext("2d");
        var dimension = this._dimension * window.devicePixelRatio;
        var radius = this._radius * window.devicePixelRatio;

        ctx.save();
        ctx.clearRect(0, 0, dimension, dimension);
        ctx.beginPath();
        ctx.arc(dimension / 2, dimension / 2, radius + 1, 0, Math.PI * 2, true);
        ctx.closePath();
        ctx.clip();
        ctx.drawImage(this._tintedCanvas, 0, 0, dimension, dimension);
        ctx.restore();
    }
};

/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WI.Gradient = class Gradient
{
    constructor(type, stops)
    {
        this.type = type;
        this.stops = stops;
    }

    // Static

    static fromString(cssString)
    {
        var type;
        var openingParenthesisIndex = cssString.indexOf("(");
        var typeString = cssString.substring(0, openingParenthesisIndex);
        if (typeString.indexOf(WI.Gradient.Types.Linear) !== -1)
            type = WI.Gradient.Types.Linear;
        else if (typeString.indexOf(WI.Gradient.Types.Radial) !== -1)
            type = WI.Gradient.Types.Radial;
        else
            return null;

        var components = [];
        var currentParams = [];
        var currentParam = "";
        var openParentheses = 0;
        var ch = openingParenthesisIndex + 1;
        var c = null;
        while (c = cssString[ch]) {
            if (c === "(")
                openParentheses++;
            if (c === ")")
                openParentheses--;

            var isComma = c === ",";
            var isSpace = /\s/.test(c);

            if (openParentheses === 0) {
                if (isSpace) {
                    if (currentParam !== "")
                        currentParams.push(currentParam);
                    currentParam = "";
                } else if (isComma) {
                    currentParams.push(currentParam);
                    components.push(currentParams);
                    currentParams = [];
                    currentParam = "";
                }
            }

            if (openParentheses === -1) {
                currentParams.push(currentParam);
                components.push(currentParams);
                break;
            }

            if (openParentheses > 0 || (!isComma && !isSpace))
                currentParam += c;

            ch++;
        }

        if (openParentheses !== -1)
            return null;

        var gradient;
        if (type === WI.Gradient.Types.Linear)
            gradient = WI.LinearGradient.fromComponents(components);
        else
            gradient = WI.RadialGradient.fromComponents(components);

        if (gradient)
            gradient.repeats = typeString.startsWith("repeating");

        return gradient;
    }

    static stopsWithComponents(components)
    {
        // FIXME: handle lengths.
        var stops = components.map(function(component) {
            while (component.length) {
                var color = WI.Color.fromString(component.shift());
                if (!color)
                    continue;

                var stop = {color};
                if (component.length && component[0].substr(-1) === "%")
                    stop.offset = parseFloat(component.shift()) / 100;
                return stop;
            }
        });

        if (!stops.length)
            return null;

        for (var i = 0, count = stops.length; i < count; ++i) {
            var stop = stops[i];

            // If one of the stops failed to parse, then this is not a valid
            // set of components for a gradient. So the whole thing is invalid.
            if (!stop)
                return null;

            if (!stop.offset)
                stop.offset = i / (count - 1);
        }

        return stops;
    }

    // Public

    stringFromStops(stops)
    {
        var count = stops.length - 1;
        return stops.map(function(stop, index) {
            var str = stop.color;
            if (stop.offset !== index / count)
                str += " " + Math.round(stop.offset * 10000) / 100 + "%";
            return str;
        }).join(", ");
    }

    // Public

    copy()
    {
        // Implemented by subclasses.
    }

    toString()
    {
        // Implemented by subclasses.
    }
};

WI.Gradient.Types = {
    Linear: "linear-gradient",
    Radial: "radial-gradient"
};

WI.LinearGradient = class LinearGradient extends WI.Gradient
{
    constructor(angle, stops)
    {
        super(WI.Gradient.Types.Linear, stops);
        this._angle = angle;
    }

    // Static

    static fromComponents(components)
    {
        let angle = {value: 180, units: WI.LinearGradient.AngleUnits.DEG};

        if (components[0].length === 1 && !WI.Color.fromString(components[0][0])) {
            let match = components[0][0].match(/([-\d\.]+)(\w+)/);
            if (!match || !Object.values(WI.LinearGradient.AngleUnits).includes(match[2]))
                return null;

            angle.value = parseFloat(match[1]);
            angle.units = match[2];

            components.shift();
        } else if (components[0][0] === "to") {
            components[0].shift();
            switch (components[0].sort().join(" ")) {
            case "top":
                angle.value = 0;
                break;
            case "right top":
                angle.value = 45;
                break;
            case "right":
                angle.value = 90;
                break;
            case "bottom right":
                angle.value = 135;
                break;
            case "bottom":
                angle.value = 180;
                break;
            case "bottom left":
                angle.value = 225;
                break;
            case "left":
                angle.value = 270;
                break;
            case "left top":
                angle.value = 315;
                break;
            default:
                return null;
            }

            components.shift();
        } else if (components[0].length !== 1 && !WI.Color.fromString(components[0][0])) {
            // If the first component is not a color, then we're dealing with a
            // legacy linear gradient format that we don't support.
            return null;
        }

        var stops = WI.Gradient.stopsWithComponents(components);
        if (!stops)
            return null;

        return new WI.LinearGradient(angle, stops);
    }

    // Public

    set angleValue(value) { this._angle.value = value; }

    get angleValue()
    {
        return this._angle.value.maxDecimals(2);
    }

    set angleUnits(units)
    {
        if (units === this._angle.units)
            return;

        this._angle.value = this._angleValueForUnits(units);
        this._angle.units = units;
    }

    get angleUnits() { return this._angle.units; }

    copy()
    {
        return new WI.LinearGradient(this._angle, this.stops.concat());
    }

    toString()
    {
        let str = "";

        let deg = this._angleValueForUnits(WI.LinearGradient.AngleUnits.DEG);
        if (deg === 0)
            str += "to top";
        else if (deg === 45)
            str += "to top right";
        else if (deg === 90)
            str += "to right";
        else if (deg === 135)
            str += "to bottom right";
        else if (deg === 225)
            str += "to bottom left";
        else if (deg === 270)
            str += "to left";
        else if (deg === 315)
            str += "to top left";
        else if (deg !== 180)
            str += this.angleValue + this.angleUnits;

        if (str !== "")
            str += ", ";

        str += this.stringFromStops(this.stops);

        return (this.repeats ? "repeating-" : "") + this.type + "(" + str + ")";
    }

    // Private

    _angleValueForUnits(units)
    {
        if (units === this._angle.units)
            return this._angle.value;

        let deg = 0;

        switch (this._angle.units) {
        case WI.LinearGradient.AngleUnits.DEG:
            deg = this._angle.value;
            break;

        case WI.LinearGradient.AngleUnits.RAD:
            deg = this._angle.value * 180 / Math.PI;
            break;

        case WI.LinearGradient.AngleUnits.GRAD:
            deg = this._angle.value / 400 * 360;
            break;

        case WI.LinearGradient.AngleUnits.TURN:
            deg = this._angle.value * 360;
            break;

        default:
            WI.reportInternalError(`Unknown angle units "${this._angle.units}"`);
            return 0;
        }

        let value = 0;

        switch (units) {
        case WI.LinearGradient.AngleUnits.DEG:
            value = deg;
            break;

        case WI.LinearGradient.AngleUnits.RAD:
            value = deg * Math.PI / 180;
            break;

        case WI.LinearGradient.AngleUnits.GRAD:
            value = deg / 360 * 400;
            break;

        case WI.LinearGradient.AngleUnits.TURN:
            value = deg / 360;
            break;
        }

        return value;
    }
};

WI.LinearGradient.AngleUnits = {
    DEG: "deg",
    RAD: "rad",
    GRAD: "grad",
    TURN: "turn",
};

WI.RadialGradient = class RadialGradient extends WI.Gradient
{
    constructor(sizing, stops)
    {
        super(WI.Gradient.Types.Radial, stops);
        this.sizing = sizing;
    }

    // Static

    static fromComponents(components)
    {
        var sizing = !WI.Color.fromString(components[0].join(" ")) ? components.shift().join(" ") : "";

        var stops = WI.Gradient.stopsWithComponents(components);
        if (!stops)
            return null;

        return new WI.RadialGradient(sizing, stops);
    }

    // Public

    copy()
    {
        return new WI.RadialGradient(this.sizing, this.stops.concat());
    }

    toString()
    {
        var str = this.sizing;

        if (str !== "")
            str += ", ";

        str += this.stringFromStops(this.stops);

        return (this.repeats ? "repeating-" : "") + this.type + "(" + str + ")";
    }
};

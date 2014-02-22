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

WebInspector.Gradient = {
    Types : {
        Linear: "linear-gradient",
        Radial: "radial-gradient"
    },
    
    fromString: function(cssString)
    {
        var type;
        var openingParenthesisIndex = cssString.indexOf("(");
        var typeString = cssString.substring(0, openingParenthesisIndex);
        if (typeString.indexOf(WebInspector.Gradient.Types.Linear) !== -1)
            type = WebInspector.Gradient.Types.Linear;
        else if (typeString.indexOf(WebInspector.Gradient.Types.Radial) !== -1)
            type = WebInspector.Gradient.Types.Radial;
        else {
            console.error("Couldn't parse angle \"" + typeString + "\"");
            return null;
        }

        var components = [];
        var currentParams = [];
        var currentParam = "";
        var openParentheses = 0;
        var ch = openingParenthesisIndex + 1;
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

        var gradient;
        if (type === WebInspector.Gradient.Types.Linear)
            gradient = WebInspector.LinearGradient.linearGradientWithComponents(components);
        else
            gradient = WebInspector.RadialGradient.radialGradientWithComponents(components);
    
        if (gradient)
            gradient.repeats = typeString.indexOf("repeating") === 0;
    
        return gradient;
    },
    
    stopsWithComponents: function(components)
    {
        // FIXME: handle lengths.
        var stops = components.map(function(component) {
            while (component.length) {
                var color = WebInspector.Color.fromString(component.shift());
                if (!color)
                    continue;

                var stop = {color: color};
                if (component.length && component[0].substr(-1) === "%")
                    stop.offset = parseFloat(component.shift()) / 100;
                return stop;
            }
        });

        if (!stops.length) {
            console.error("Couldn't parse any stops");
            return null;
        }

        for (var i = 0, count = stops.length; i < count; ++i) {
            var stop = stops[i];
            if (!stop.offset)
                stop.offset = i / (count - 1);
        }

        return stops;
    },
    
    stringFromStops: function(stops)
    {
        var count = stops.length - 1;
        return stops.map(function(stop, index) {
            var str = stop.color;
            if (stop.offset !== index / count)
                str += " " + Math.round(stop.offset * 10000) / 100 + "%";
            return str;
        }).join(", ");
    }
};

WebInspector.LinearGradient = function(angle, stops)
{
    this.type = WebInspector.Gradient.Types.Linear;
    this.angle = angle;
    this.stops = stops;
}

WebInspector.LinearGradient.linearGradientWithComponents = function(components)
{
    var angle = 180;

    if (components[0].length === 1 && components[0][0].substr(-3) === "deg") {
        angle = (parseFloat(components[0][0]) % 360 + 360) % 360;
        components.shift();
    } else if (components[0][0] === "to") {
        components[0].shift();
        switch (components[0].sort().join(" ")) {
        case "top":
            angle = 0;
            break;
        case "right top":
            angle = 45;
            break;
        case "right":
            angle = 90;
            break;
        case "bottom right":
            angle = 135;
            break;
        case "bottom":
            angle = 180;
            break;
        case "bottom left":
            angle = 225;
            break;
        case "left":
            angle = 270;
            break;
        case "left top":
            angle = 315;
            break;
        default:
            console.error("Couldn't parse angle \"to " + components[0].join(" ") + "\"");
            return null;
        }
        components.shift();
    }

    var stops = WebInspector.Gradient.stopsWithComponents(components);
    if (!stops)
        return null;

    return new WebInspector.LinearGradient(angle, stops);
}

WebInspector.LinearGradient.prototype = {
    constructor: WebInspector.LinearGradient,

    copy: function()
    {
        return new WebInspector.LinearGradient(this.angle, this.stops.concat());
    },

    toString: function()
    {
        var str = "";

        if (this.angle === 0)
            str += "to top";
        else if (this.angle === 45)
            str += "to top right";
        else if (this.angle === 90)
            str += "to right";
        else if (this.angle === 135)
            str += "to bottom right";
        else if (this.angle === 225)
            str += "to bottom left";
        else if (this.angle === 270)
            str += "to left";
        else if (this.angle === 315)
            str += "to top left";
        else if (this.angle !== 180)
            str += this.angle + "deg";

        if (str !== "")
            str += ", ";

        str += WebInspector.Gradient.stringFromStops(this.stops);

        return (this.repeats ? "repeating-" : "") + this.type + "(" + str + ")";
    }
}

WebInspector.RadialGradient = function(sizing, stops)
{
    this.type = WebInspector.Gradient.Types.Radial;
    this.sizing = sizing;
    this.stops = stops;
}

WebInspector.RadialGradient.radialGradientWithComponents = function(components)
{
    var sizing = !WebInspector.Color.fromString(components[0].join(" ")) ? components.shift().join(" ") : "";

    var stops = WebInspector.Gradient.stopsWithComponents(components);
    if (!stops)
        return null;

    return new WebInspector.RadialGradient(sizing, stops);
}

WebInspector.RadialGradient.prototype = {
    constructor: WebInspector.RadialGradient,

    copy: function()
    {
        return new WebInspector.RadialGradient(this.sizing, this.stops.concat());
    },

    toString: function()
    {
        var str = this.sizing;

        if (str !== "")
            str += ", ";

        str += WebInspector.Gradient.stringFromStops(this.stops);
        
        return (this.repeats ? "repeating-" : "") + this.type + "(" + str + ")";
    }
}

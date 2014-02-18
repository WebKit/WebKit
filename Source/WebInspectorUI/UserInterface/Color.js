/*
 * Copyright (C) 2009, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.Color = function(format, components)
{
    this.format = format;
    if (format === WebInspector.Color.Format.HSL || format === WebInspector.Color.Format.HSLA)
        this._hsla = components;
    else
        this._rgba = components;

    this.valid = !components.some(function(component) {
        return isNaN(component);
    });
}

WebInspector.Color.Format = {
    Original: "color-format-original",
    Nickname: "color-format-nickname",
    HEX: "color-format-hex",
    ShortHEX: "color-format-short-hex",
    RGB: "color-format-rgb",
    RGBA: "color-format-rgba",
    HSL: "color-format-hsl",
    HSLA: "color-format-hsla"
};

WebInspector.Color.fromString = function(colorString)
{
    var value = colorString.toLowerCase().replace(/%|\s+/g, "");
    const transparentNicknames = ["transparent", "rgba(0,0,0,0)", "hsla(0,0,0,0)"];
    if (transparentNicknames.contains(value)) {
        var color = new WebInspector.Color(WebInspector.Color.Format.Nickname, [0, 0, 0, 0]);
        color.nickname = "transparent";
        color.original = colorString;
        return color;
    }

    // Simple - #hex, rgb(), nickname, hsl()
    var simple = /^(?:#([0-9a-f]{3,6})|rgb\(([^)]+)\)|(\w+)|hsl\(([^)]+)\))$/i;
    var match = colorString.match(simple);
    if (match) {
        if (match[1]) { // hex
            var hex = match[1].toUpperCase();
            if (hex.length === 3) {
                return new WebInspector.Color(WebInspector.Color.Format.ShortHEX, [
                    parseInt(hex.charAt(0) + hex.charAt(0), 16),
                    parseInt(hex.charAt(1) + hex.charAt(1), 16),
                    parseInt(hex.charAt(2) + hex.charAt(2), 16),
                    1
                ]);
            } else {
                return new WebInspector.Color(WebInspector.Color.Format.HEX, [
                    parseInt(hex.substring(0, 2), 16),
                    parseInt(hex.substring(2, 4), 16),
                    parseInt(hex.substring(4, 6), 16),
                    1
                ]);
            }
        } else if (match[2]) { // rgb
            var rgb = match[2].split(/\s*,\s*/);
            return new WebInspector.Color(WebInspector.Color.Format.RGB, [
                parseInt(rgb[0]),
                parseInt(rgb[1]),
                parseInt(rgb[2]),
                1
            ]);
        } else if (match[3]) { // nickname
            var nickname = match[3].toLowerCase();
            if (WebInspector.Color.Nicknames.hasOwnProperty(nickname)) {
                var color = new WebInspector.Color(WebInspector.Color.Format.Nickname, WebInspector.Color.Nicknames[nickname].concat(1));
                color.nickname = nickname;
                color.original = colorString;
                return color;
            } else
                return null;
        } else if (match[4]) { // hsl
            var hsl = match[4].replace(/%/g, "").split(/\s*,\s*/);
            return new WebInspector.Color(WebInspector.Color.Format.HSL, [
                parseInt(hsl[0]),
                parseInt(hsl[1]),
                parseInt(hsl[2]),
                1
            ]);
        }
    }

    // Advanced - rgba(), hsla()
    var advanced = /^(?:rgba\(([^)]+)\)|hsla\(([^)]+)\))$/;
    match = colorString.match(advanced);
    if (match) {
        if (match[1]) { // rgba
            var rgba = match[1].split(/\s*,\s*/);
            return new WebInspector.Color(WebInspector.Color.Format.RGBA, [
                parseInt(rgba[0]),
                parseInt(rgba[1]),
                parseInt(rgba[2]),
                Number.constrain(parseFloat(rgba[3]), 0, 1)
            ]);
        } else if (match[2]) { // hsla
            var hsla = match[2].replace(/%/g, "").split(/\s*,\s*/);
            return new WebInspector.Color(WebInspector.Color.Format.HSLA, [
                parseInt(hsla[0]),
                parseInt(hsla[1]),
                parseInt(hsla[2]),
                Number.constrain(parseFloat(hsla[3]), 0, 1)
            ]);
        }
    }

    return null;
}

WebInspector.Color.prototype = {
    nextFormat: function(format)
    {
        format = format || this.format;

        switch (format) {
        case WebInspector.Color.Format.Original:
            return this.simple ? WebInspector.Color.Format.RGB : WebInspector.Color.Format.RGBA;

        case WebInspector.Color.Format.RGB:
        case WebInspector.Color.Format.RGBA:
            return this.simple ? WebInspector.Color.Format.HSL : WebInspector.Color.Format.HSLA;

        case WebInspector.Color.Format.HSL:
        case WebInspector.Color.Format.HSLA:
            if (this.nickname)
                return WebInspector.Color.Format.Nickname;
            if (this.simple)
                return this._canBeSerializedAsShortHEX() ? WebInspector.Color.Format.ShortHEX : WebInspector.Color.Format.HEX;
            else
                return WebInspector.Color.Format.Original;

        case WebInspector.Color.Format.ShortHEX:
            return WebInspector.Color.Format.HEX;

        case WebInspector.Color.Format.HEX:
            return WebInspector.Color.Format.Original;

        case WebInspector.Color.Format.Nickname:
            if (this.simple)
                return this._canBeSerializedAsShortHEX() ? WebInspector.Color.Format.ShortHEX : WebInspector.Color.Format.HEX;
            else
                return WebInspector.Color.Format.Original;

        default:
            console.error("Unknown color format.");
            return null;
        }
    },

    get alpha()
    {
        return this._rgba ? this._rgba[3] : this._hsla[3];
    },

    get simple()
    {
        return this.alpha === 1;
    },

    get rgb()
    {
        var rgb = this.rgba.slice();
        rgb.pop();
        return rgb;
    },

    get hsl()
    {
        var hsl = this.hsla.slice();
        hsl.pop();
        return hsl;
    },

    get rgba()
    {
        if (!this._rgba)
            this._rgba = this._hslaToRGBA(this._hsla);
        return this._rgba;
    },

    get hsla()
    {
        if (!this._hsla)
            this._hsla = this._rgbaToHSLA(this.rgba);
        return this._hsla;
    },

    copy: function()
    {
        switch (this.format) {
        case WebInspector.Color.Format.RGB:
        case WebInspector.Color.Format.HEX:
        case WebInspector.Color.Format.ShortHEX:
        case WebInspector.Color.Format.Nickname:
        case WebInspector.Color.Format.RGBA:
            return new WebInspector.Color(this.format, this.rgba);
        case WebInspector.Color.Format.HSL:
        case WebInspector.Color.Format.HSLA:
            return new WebInspector.Color(this.format, this.hsla);
        }
    },

    toString: function(format)
    {
        if (!format)
            format = this.format;

        switch (format) {
        case WebInspector.Color.Format.Original:
            return this._toOriginalString();
        case WebInspector.Color.Format.RGB:
            return this._toRGBString();
        case WebInspector.Color.Format.RGBA:
            return this._toRGBAString();
        case WebInspector.Color.Format.HSL:
            return this._toHSLString();
        case WebInspector.Color.Format.HSLA:
            return this._toHSLAString();
        case WebInspector.Color.Format.HEX:
            return this._toHEXString();
        case WebInspector.Color.Format.ShortHEX:
            return this._toShortHEXString();
        case WebInspector.Color.Format.Nickname:
            return this._toNicknameString();
        }

        throw "invalid color format";
    },

    _toOriginalString: function()
    {
        return this.original || this._toNicknameString();
    },

    _toNicknameString: function()
    {
        if (this.nickname)
            return this.nickname;

        var rgba = this.rgba;
        if (!this.simple) {
            if (rgba[0] === 0 && rgba[1] === 0 && rgba[2] === 0 && rgba[3] === 0)
                return "transparent";
            return this._toRGBAString();
        }

        var nicknames = WebInspector.Color.Nicknames;
        for (var nickname in nicknames) {
            if (!nicknames.hasOwnProperty(nickname))
                continue;

            var nicknameRGB = nicknames[nickname];
            if (nicknameRGB[0] === rgba[0] && nicknameRGB[1] === rgba[1] && nicknameRGB[2] === rgba[2])
                return nickname;
        }

        return this._toRGBString();
    },

    _toShortHEXString: function()
    {
        if (!this.simple)
            return this._toRGBAString();

        var rgba = this.rgba;
        var r = this._componentToHexValue(rgba[0]);
        var g = this._componentToHexValue(rgba[1]);
        var b = this._componentToHexValue(rgba[2]);

        if (r[0] === r[1] && g[0] === g[1] && b[0] === b[1])
            return "#" + r[0] + g[0] + b[0];
        else
            return "#" + r + g + b;
    },

    _toHEXString: function()
    {
        if (!this.simple)
            return this._toRGBAString();

        var rgba = this.rgba;
        var r = this._componentToHexValue(rgba[0]);
        var g = this._componentToHexValue(rgba[1]);
        var b = this._componentToHexValue(rgba[2]);

        return "#" + r + g + b;
    },

    _toRGBString: function()
    {
        if (!this.simple)
            return this._toRGBAString();

        var rgba = this.rgba;
        return "rgb(" + [rgba[0], rgba[1], rgba[2]].join(", ") + ")";
    },

    _toRGBAString: function()
    {
        return "rgba(" + this.rgba.join(", ") + ")";
    },

    _toHSLString: function()
    {
        if (!this.simple)
            return this._toHSLAString();

        var hsla = this.hsla;
        return "hsl(" + hsla[0] + ", " + hsla[1] + "%, " + hsla[2] + "%)";
    },

    _toHSLAString: function()
    {
        var hsla = this.hsla;
        return "hsla(" + hsla[0] + ", " + hsla[1] + "%, " + hsla[2] + "%, " + hsla[3] + ")";
    },

    _canBeSerializedAsShortHEX: function()
    {
        var rgba = this.rgba;

        var r = this._componentToHexValue(rgba[0]);
        if (r[0] !== r[1])
            return false;

        var g = this._componentToHexValue(rgba[1]);
        if (g[0] !== g[1])
            return false;

        var b = this._componentToHexValue(rgba[2]);
        if (b[0] !== b[1])
            return false;

        return true;
    },

    _componentToNumber: function(value)
    {
        return Number.constrain(value, 0, 255);
    },

    _componentToHexValue: function(value)
    {
        var hex = this._componentToNumber(value).toString(16);
        if (hex.length === 1)
            hex = "0" + hex;
        return hex;
    },

    _rgbToHSL: function(rgb)
    {
        var r = this._componentToNumber(rgb[0]) / 255;
        var g = this._componentToNumber(rgb[1]) / 255;
        var b = this._componentToNumber(rgb[2]) / 255;
        var max = Math.max(r, g, b);
        var min = Math.min(r, g, b);
        var diff = max - min;
        var add = max + min;

        if (min === max)
            var h = 0;
        else if (r === max)
            var h = ((60 * (g - b) / diff) + 360) % 360;
        else if (g === max)
            var h = (60 * (b - r) / diff) + 120;
        else
            var h = (60 * (r - g) / diff) + 240;

        var l = 0.5 * add;

        if (l === 0)
            var s = 0;
        else if (l === 1)
            var s = 1;
        else if (l <= 0.5)
            var s = diff / add;
        else
            var s = diff / (2 - add);

        h = Math.round(h);
        s = Math.round(s * 100);
        l = Math.round(l * 100);

        return [h, s, l];
    },

    _hslToRGB: function(hsl)
    {
        var h = parseFloat(hsl[0]) / 360;
        var s = parseFloat(hsl[1]) / 100;
        var l = parseFloat(hsl[2]) / 100;

        h *= 6;
        var sArray = [
            l += s *= l < .5 ? l : 1 - l,
            l - h % 1 * s * 2,
            l -= s *= 2,
            l,
            l + h % 1 * s,
            l + s
        ];
        return [
            Math.round(sArray[ ~~h    % 6 ] * 255),
            Math.round(sArray[ (h|16) % 6 ] * 255),
            Math.round(sArray[ (h|8)  % 6 ] * 255)
        ];
    },

    _rgbaToHSLA: function(rgba)
    {
        var hsl = this._rgbToHSL(rgba);
        hsl.push(rgba[3]);
        return hsl;
    },

    _hslaToRGBA: function(hsla)
    {
        var rgba = this._hslToRGB(hsla);
        rgba.push(hsla[3]);
        return rgba;
    }
}

WebInspector.Color.Nicknames = {
    "aliceblue": [240, 248, 255],
    "antiquewhite": [250, 235, 215],
    "aquamarine": [127, 255, 212],
    "azure": [240, 255, 255],
    "beige": [245, 245, 220],
    "bisque": [255, 228, 196],
    "black": [0, 0, 0],
    "blanchedalmond": [255, 235, 205],
    "blue": [0, 0, 255],
    "blueviolet": [138, 43, 226],
    "brown": [165, 42, 42],
    "burlywood": [222, 184, 135],
    "cadetblue": [95, 158, 160],
    "chartreuse": [127, 255, 0],
    "chocolate": [210, 105, 30],
    "coral": [255, 127, 80],
    "cornflowerblue": [100, 149, 237],
    "cornsilk": [255, 248, 220],
    "crimson": [237, 164, 61],
    "cyan": [0, 255, 255],
    "darkblue": [0, 0, 139],
    "darkcyan": [0, 139, 139],
    "darkgoldenrod": [184, 134, 11],
    "darkgray": [169, 169, 169],
    "darkgreen": [0, 100, 0],
    "darkkhaki": [189, 183, 107],
    "darkmagenta": [139, 0, 139],
    "darkolivegreen": [85, 107, 47],
    "darkorange": [255, 140, 0],
    "darkorchid": [153, 50, 204],
    "darkred": [139, 0, 0],
    "darksalmon": [233, 150, 122],
    "darkseagreen": [143, 188, 143],
    "darkslateblue": [72, 61, 139],
    "darkslategray": [47, 79, 79],
    "darkturquoise": [0, 206, 209],
    "darkviolet": [148, 0, 211],
    "deeppink": [255, 20, 147],
    "deepskyblue": [0, 191, 255],
    "dimgray": [105, 105, 105],
    "dodgerblue": [30, 144, 255],
    "firebrick": [178, 34, 34],
    "floralwhite": [255, 250, 240],
    "forestgreen": [34, 139, 34],
    "gainsboro": [220, 220, 220],
    "ghostwhite": [248, 248, 255],
    "gold": [255, 215, 0],
    "goldenrod": [218, 165, 32],
    "gray": [128, 128, 128],
    "green": [0, 128, 0],
    "greenyellow": [173, 255, 47],
    "honeydew": [240, 255, 240],
    "hotpink": [255, 105, 180],
    "indianred": [205, 92, 92],
    "indigo": [75, 0, 130],
    "ivory": [255, 255, 240],
    "khaki": [240, 230, 140],
    "lavender": [230, 230, 250],
    "lavenderblush": [255, 240, 245],
    "lawngreen": [124, 252, 0],
    "lemonchiffon": [255, 250, 205],
    "lightblue": [173, 216, 230],
    "lightcoral": [240, 128, 128],
    "lightcyan": [224, 255, 255],
    "lightgoldenrodyellow": [250, 250, 210],
    "lightgreen": [144, 238, 144],
    "lightgrey": [211, 211, 211],
    "lightpink": [255, 182, 193],
    "lightsalmon": [255, 160, 122],
    "lightseagreen": [32, 178, 170],
    "lightskyblue": [135, 206, 250],
    "lightslategray": [119, 136, 153],
    "lightsteelblue": [176, 196, 222],
    "lightyellow": [255, 255, 224],
    "lime": [0, 255, 0],
    "limegreen": [50, 205, 50],
    "linen": [250, 240, 230],
    "magenta": [255, 0, 255],
    "maroon": [128, 0, 0],
    "mediumaquamarine": [102, 205, 170],
    "mediumblue": [0, 0, 205],
    "mediumorchid": [186, 85, 211],
    "mediumpurple": [147, 112, 219],
    "mediumseagreen": [60, 179, 113],
    "mediumslateblue": [123, 104, 238],
    "mediumspringgreen": [0, 250, 154],
    "mediumturquoise": [72, 209, 204],
    "mediumvioletred": [199, 21, 133],
    "midnightblue": [25, 25, 112],
    "mintcream": [245, 255, 250],
    "mistyrose": [255, 228, 225],
    "moccasin": [255, 228, 181],
    "navajowhite": [255, 222, 173],
    "navy": [0, 0, 128],
    "oldlace": [253, 245, 230],
    "olive": [128, 128, 0],
    "olivedrab": [107, 142, 35],
    "orange": [255, 165, 0],
    "orangered": [255, 69, 0],
    "orchid": [218, 112, 214],
    "palegoldenrod": [238, 232, 170],
    "palegreen": [152, 251, 152],
    "paleturquoise": [175, 238, 238],
    "palevioletred": [219, 112, 147],
    "papayawhip": [255, 239, 213],
    "peachpuff": [255, 218, 185],
    "peru": [205, 133, 63],
    "pink": [255, 192, 203],
    "plum": [221, 160, 221],
    "powderblue": [176, 224, 230],
    "purple": [128, 0, 128],
    "red": [255, 0, 0],
    "rosybrown": [188, 143, 143],
    "royalblue": [65, 105, 225],
    "saddlebrown": [139, 69, 19],
    "salmon": [250, 128, 114],
    "sandybrown": [244, 164, 96],
    "seagreen": [46, 139, 87],
    "seashell": [255, 245, 238],
    "sienna": [160, 82, 45],
    "silver": [192, 192, 192],
    "skyblue": [135, 206, 235],
    "slateblue": [106, 90, 205],
    "slategray": [112, 128, 144],
    "snow": [255, 250, 250],
    "springgreen": [0, 255, 127],
    "steelblue": [70, 130, 180],
    "tan": [210, 180, 140],
    "teal": [0, 128, 128],
    "thistle": [216, 191, 216],
    "tomato": [255, 99, 71],
    "turquoise": [64, 224, 208],
    "violet": [238, 130, 238],
    "wheat": [245, 222, 179],
    "white": [255, 255, 255],
    "whitesmoke": [245, 245, 245],
    "yellow": [255, 255, 0],
    "yellowgreen": [154, 205, 50]
};

WebInspector.Color.rgb2hsv = function(r, g, b)
{
    r /= 255;
    g /= 255;
    b /= 255;

    var min = Math.min(Math.min(r, g), b);
    var max = Math.max(Math.max(r, g), b);
    var delta = max - min;

    var v = max;
    var s, h;

    if (max === min)
        h = 0;
    else if (max === r)
        h = (60 * ((g - b) / delta)) % 360;
    else if (max === g)
        h = 60 * ((b - r) / delta) + 120;
    else if (max === b)
        h = 60 * ((r - g) / delta) + 240;

    if (h < 0)
        h += 360;

    // Saturation
    if (max === 0)
        s = 0;
    else
        s = 1 - (min/max);

    return [h, s, v];
}

WebInspector.Color.hsv2rgb = function(h, s, v)
{
    if (s === 0)
        return [v, v, v];

    h /= 60;
    var i = Math.floor(h);
    var data = [
        v * (1 - s),
        v * (1 - s * (h - i)),
        v * (1 - s * (1 - (h - i)))
    ];
    var rgb;

    switch (i) {
    case 0:
        rgb = [v, data[2], data[0]];
        break;
    case 1:
        rgb = [data[1], v, data[0]];
        break;
    case 2:
        rgb = [data[0], v, data[2]];
        break;
    case 3:
        rgb = [data[0], data[1], v];
        break;
    case 4:
        rgb = [data[2], data[0], v];
        break;
    default:
        rgb = [v, data[0], data[1]];
        break;
    }

    return rgb;
}

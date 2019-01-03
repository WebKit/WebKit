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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

WI.Color = class Color
{
    constructor(format, components)
    {
        this.format = format;

        if (components.length === 3)
            components.push(1);

        if (format === WI.Color.Format.HSL || format === WI.Color.Format.HSLA)
            this._hsla = components;
        else
            this._rgba = components;

        this.valid = !components.some(isNaN);
    }

    // Static

    static fromString(colorString)
    {
        let value = colorString.toLowerCase().replace(/%|\s+/g, "");
        let transparentKeywords = ["transparent", "rgba(0,0,0,0)", "hsla(0,0,0,0)"];
        if (transparentKeywords.includes(value)) {
            let color = new WI.Color(WI.Color.Format.Keyword, [0, 0, 0, 0]);
            color.keyword = "transparent";
            color.original = colorString;
            return color;
        }

        // Simple - #hex, rgb(), keyword, hsl()
        let simple = /^(?:#(?<hex>[0-9a-f]{3,8})|rgb\((?<rgb>[^)]+)\)|(?<keyword>\w+)|hsl\((?<hsl>[^)]+)\))$/i;
        let match = colorString.match(simple);
        if (match) {
            if (match.groups.hex) {
                let hex = match.groups.hex.toUpperCase();
                let len = hex.length;
                if (len === 3) {
                    return new WI.Color(WI.Color.Format.ShortHEX, [
                        parseInt(hex.charAt(0) + hex.charAt(0), 16),
                        parseInt(hex.charAt(1) + hex.charAt(1), 16),
                        parseInt(hex.charAt(2) + hex.charAt(2), 16),
                        1
                    ]);
                } else if (len === 6) {
                    return new WI.Color(WI.Color.Format.HEX, [
                        parseInt(hex.substring(0, 2), 16),
                        parseInt(hex.substring(2, 4), 16),
                        parseInt(hex.substring(4, 6), 16),
                        1
                    ]);
                } else if (len === 4) {
                    return new WI.Color(WI.Color.Format.ShortHEXAlpha, [
                        parseInt(hex.charAt(0) + hex.charAt(0), 16),
                        parseInt(hex.charAt(1) + hex.charAt(1), 16),
                        parseInt(hex.charAt(2) + hex.charAt(2), 16),
                        parseInt(hex.charAt(3) + hex.charAt(3), 16) / 255
                    ]);
                } else if (len === 8) {
                    return new WI.Color(WI.Color.Format.HEXAlpha, [
                        parseInt(hex.substring(0, 2), 16),
                        parseInt(hex.substring(2, 4), 16),
                        parseInt(hex.substring(4, 6), 16),
                        parseInt(hex.substring(6, 8), 16) / 255
                    ]);
                } else
                    return null;
            } else if (match.groups.rgb) {
                let rgb = match.groups.rgb.split(/\s*,\s*/);
                if (rgb.length !== 3)
                    return null;
                return new WI.Color(WI.Color.Format.RGB, [
                    parseInt(rgb[0]),
                    parseInt(rgb[1]),
                    parseInt(rgb[2]),
                    1
                ]);
            } else if (match.groups.keyword) {
                let keyword = match.groups.keyword.toLowerCase();
                if (!WI.Color.Keywords.hasOwnProperty(keyword))
                    return null;
                let color = new WI.Color(WI.Color.Format.Keyword, WI.Color.Keywords[keyword].concat(1));
                color.keyword = keyword;
                color.original = colorString;
                return color;
            } else if (match.groups.hsl) {
                let hsl = match.groups.hsl.replace(/%/g, "").split(/\s*,\s*/);
                if (hsl.length !== 3)
                    return null;
                return new WI.Color(WI.Color.Format.HSL, [
                    parseInt(hsl[0]),
                    parseInt(hsl[1]),
                    parseInt(hsl[2]),
                    1
                ]);
            }
        }

        // Advanced - rgba(), hsla()
        let advanced = /^(?:rgba\((?<rgba>[^)]+)\)|hsla\((?<hsla>[^)]+)\))$/i;
        match = colorString.match(advanced);
        if (match) {
            if (match.groups.rgba) {
                let rgba = match.groups.rgba.split(/\s*,\s*/);
                if (rgba.length !== 4)
                    return null;
                return new WI.Color(WI.Color.Format.RGBA, [
                    parseInt(rgba[0]),
                    parseInt(rgba[1]),
                    parseInt(rgba[2]),
                    Number.constrain(parseFloat(rgba[3]), 0, 1)
                ]);
            } else if (match.groups.hsla) {
                let hsla = match.groups.hsla.replace(/%/g, "").split(/\s*,\s*/);
                if (hsla.length !== 4)
                    return null;
                return new WI.Color(WI.Color.Format.HSLA, [
                    parseInt(hsla[0]),
                    parseInt(hsla[1]),
                    parseInt(hsla[2]),
                    Number.constrain(parseFloat(hsla[3]), 0, 1)
                ]);
            }
        }

        return null;
    }

    static rgb2hsl(r, g, b)
    {
        r = WI.Color._eightBitChannel(r) / 255;
        g = WI.Color._eightBitChannel(g) / 255;
        b = WI.Color._eightBitChannel(b) / 255;

        let min = Math.min(r, g, b);
        let max = Math.max(r, g, b);
        let delta = max - min;

        let h = 0;
        let s = 0;
        let l = (max + min) / 2;

        if (delta === 0)
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
        if (delta === 0)
            s = 0;
        else
            s = delta / (1 - Math.abs((2 * l) - 1));

        return [
            h,
            s * 100,
            l * 100,
        ];
    }

    static hsl2rgb(h, s, l)
    {
        h = Number.constrain(h, 0, 360) % 360;
        s = Number.constrain(s, 0, 100) / 100;
        l = Number.constrain(l, 0, 100) / 100;

        let c = (1 - Math.abs((2 * l) - 1)) * s;
        let x = c * (1 - Math.abs(((h / 60) % 2) - 1));
        let m = l - (c / 2);

        let r = 0;
        let g = 0;
        let b = 0;

        if (h < 60) {
            r = c;
            g = x;
        } else if (h < 120) {
            r = x;
            g = c;
        } else if (h < 180) {
            g = c;
            b = x;
        } else if (h < 240) {
            g = x;
            b = c;
        } else if (h < 300) {
            r = x;
            b = c;
        } else if (h < 360) {
            r = c;
            b = x;
        }

        return [
            (r + m) * 255,
            (g + m) * 255,
            (b + m) * 255,
        ];
    }

    static cmyk2rgb(c, m, y, k)
    {
        c = Number.constrain(c, 0, 1);
        m = Number.constrain(m, 0, 1);
        y = Number.constrain(y, 0, 1);
        k = Number.constrain(k, 0, 1);
        return [
            255 * (1 - c) * (1 - k),
            255 * (1 - m) * (1 - k),
            255 * (1 - y) * (1 - k),
        ];
    }

    static normalized2rgb(r, g, b)
    {
        return [
            WI.Color._eightBitChannel(r * 255),
            WI.Color._eightBitChannel(g * 255),
            WI.Color._eightBitChannel(b * 255)
        ];
    }

    static _eightBitChannel(value)
    {
        return Number.constrain(Math.round(value), 0, 255);
    }

    // Public

    nextFormat(format)
    {
        format = format || this.format;

        switch (format) {
        case WI.Color.Format.Original:
        case WI.Color.Format.HEX:
        case WI.Color.Format.HEXAlpha:
            return this.simple ? WI.Color.Format.RGB : WI.Color.Format.RGBA;

        case WI.Color.Format.RGB:
        case WI.Color.Format.RGBA:
            return this.simple ? WI.Color.Format.HSL : WI.Color.Format.HSLA;

        case WI.Color.Format.HSL:
        case WI.Color.Format.HSLA:
            if (this.isKeyword())
                return WI.Color.Format.Keyword;
            if (this.simple)
                return this.canBeSerializedAsShortHEX() ? WI.Color.Format.ShortHEX : WI.Color.Format.HEX;
            return this.canBeSerializedAsShortHEX() ? WI.Color.Format.ShortHEXAlpha : WI.Color.Format.HEXAlpha;

        case WI.Color.Format.ShortHEX:
            return WI.Color.Format.HEX;

        case WI.Color.Format.ShortHEXAlpha:
            return WI.Color.Format.HEXAlpha;

        case WI.Color.Format.Keyword:
            if (this.simple)
                return this.canBeSerializedAsShortHEX() ? WI.Color.Format.ShortHEX : WI.Color.Format.HEX;
            return this.canBeSerializedAsShortHEX() ? WI.Color.Format.ShortHEXAlpha : WI.Color.Format.HEXAlpha;

        default:
            console.error("Unknown color format.");
            return null;
        }
    }

    get alpha()
    {
        return this._rgba ? this._rgba[3] : this._hsla[3];
    }

    get simple()
    {
        return this.alpha === 1;
    }

    get rgb()
    {
        let rgb = this.rgba.slice();
        rgb.pop();
        return rgb;
    }

    get hsl()
    {
        let hsl = this.hsla.slice();
        hsl.pop();
        return hsl;
    }

    get rgba()
    {
        if (!this._rgba)
            this._rgba = this._hslaToRGBA(this._hsla);
        return this._rgba;
    }

    get hsla()
    {
        if (!this._hsla)
            this._hsla = this._rgbaToHSLA(this.rgba);
        return this._hsla;
    }

    copy()
    {
        switch (this.format) {
        case WI.Color.Format.RGB:
        case WI.Color.Format.HEX:
        case WI.Color.Format.ShortHEX:
        case WI.Color.Format.HEXAlpha:
        case WI.Color.Format.ShortHEXAlpha:
        case WI.Color.Format.Keyword:
        case WI.Color.Format.RGBA:
            return new WI.Color(this.format, this.rgba);
        case WI.Color.Format.HSL:
        case WI.Color.Format.HSLA:
            return new WI.Color(this.format, this.hsla);
        }
    }

    toString(format)
    {
        if (!format)
            format = this.format;

        switch (format) {
        case WI.Color.Format.Original:
            return this._toOriginalString();
        case WI.Color.Format.RGB:
            return this._toRGBString();
        case WI.Color.Format.RGBA:
            return this._toRGBAString();
        case WI.Color.Format.HSL:
            return this._toHSLString();
        case WI.Color.Format.HSLA:
            return this._toHSLAString();
        case WI.Color.Format.HEX:
            return this._toHEXString();
        case WI.Color.Format.ShortHEX:
            return this._toShortHEXString();
        case WI.Color.Format.HEXAlpha:
            return this._toHEXAlphaString();
        case WI.Color.Format.ShortHEXAlpha:
            return this._toShortHEXAlphaString();
        case WI.Color.Format.Keyword:
            return this._toKeywordString();
        }

        console.error("Invalid color format: " + format);
        return "";
    }

    isKeyword()
    {
        if (this.keyword)
            return true;

        if (!this.simple)
            return Array.shallowEqual(this._rgba, [0, 0, 0, 0]) || Array.shallowEqual(this._hsla, [0, 0, 0, 0]);

        let rgb = (this._rgba && this._rgba.slice(0, 3)) || WI.Color.hsl2rgb(...this._hsla);
        return Object.keys(WI.Color.Keywords).some(key => Array.shallowEqual(WI.Color.Keywords[key], rgb));
    }

    canBeSerializedAsShortHEX()
    {
        let rgba = this.rgba || this._hslaToRGBA(this._hsla);

        let r = this._componentToHexValue(rgba[0]);
        if (r[0] !== r[1])
            return false;

        let g = this._componentToHexValue(rgba[1]);
        if (g[0] !== g[1])
            return false;

        let b = this._componentToHexValue(rgba[2]);
        if (b[0] !== b[1])
            return false;

        if (!this.simple) {
            let a = this._componentToHexValue(Math.round(rgba[3] * 255));
            if (a[0] !== a[1])
                return false;
        }

        return true;
    }

    // Private

    _toOriginalString()
    {
        return this.original || this._toKeywordString();
    }

    _toKeywordString()
    {
        if (this.keyword)
            return this.keyword;

        let rgba = this.rgba;
        if (!this.simple) {
            if (rgba[0] === 0 && rgba[1] === 0 && rgba[2] === 0 && rgba[3] === 0)
                return "transparent";
            return this._toRGBAString();
        }

        let keywords = WI.Color.Keywords;
        for (let keyword in keywords) {
            if (!keywords.hasOwnProperty(keyword))
                continue;

            let keywordRGB = keywords[keyword];
            if (keywordRGB[0] === rgba[0] && keywordRGB[1] === rgba[1] && keywordRGB[2] === rgba[2])
                return keyword;
        }

        return this._toRGBString();
    }

    _toShortHEXString()
    {
        if (!this.simple)
            return this._toRGBAString();

        let rgba = this.rgba;
        let r = this._componentToHexValue(rgba[0]);
        let g = this._componentToHexValue(rgba[1]);
        let b = this._componentToHexValue(rgba[2]);

        if (r[0] === r[1] && g[0] === g[1] && b[0] === b[1])
            return "#" + r[0] + g[0] + b[0];
        else
            return "#" + r + g + b;
    }

    _toHEXString()
    {
        if (!this.simple)
            return this._toRGBAString();

        let rgba = this.rgba;
        let r = this._componentToHexValue(rgba[0]);
        let g = this._componentToHexValue(rgba[1]);
        let b = this._componentToHexValue(rgba[2]);

        return "#" + r + g + b;
    }

    _toShortHEXAlphaString()
    {
        let rgba = this.rgba;
        let r = this._componentToHexValue(rgba[0]);
        let g = this._componentToHexValue(rgba[1]);
        let b = this._componentToHexValue(rgba[2]);
        let a = this._componentToHexValue(Math.round(rgba[3] * 255));

        if (r[0] === r[1] && g[0] === g[1] && b[0] === b[1] && a[0] === a[1])
            return "#" + r[0] + g[0] + b[0] + a[0];
        else
            return "#" + r + g + b + a;
    }

    _toHEXAlphaString()
    {
        let rgba = this.rgba;
        let r = this._componentToHexValue(rgba[0]);
        let g = this._componentToHexValue(rgba[1]);
        let b = this._componentToHexValue(rgba[2]);
        let a = this._componentToHexValue(Math.round(rgba[3] * 255));

        return "#" + r + g + b + a;
    }

    _toRGBString()
    {
        if (!this.simple)
            return this._toRGBAString();

        let r = WI.Color._eightBitChannel(Math.round(this.rgba[0]));
        let g = WI.Color._eightBitChannel(Math.round(this.rgba[1]));
        let b = WI.Color._eightBitChannel(Math.round(this.rgba[2]));
        return `rgb(${r}, ${g}, ${b})`;
    }

    _toRGBAString()
    {
        let r = WI.Color._eightBitChannel(Math.round(this.rgba[0]));
        let g = WI.Color._eightBitChannel(Math.round(this.rgba[1]));
        let b = WI.Color._eightBitChannel(Math.round(this.rgba[2]));
        return `rgba(${r}, ${g}, ${b}, ${this.alpha})`;
    }

    _toHSLString()
    {
        if (!this.simple)
            return this._toHSLAString();

        let h = this.hsla[0].maxDecimals(2);
        let s = this.hsla[1].maxDecimals(2);
        let l = this.hsla[2].maxDecimals(2);
        return `hsl(${h}, ${s}%, ${l}%)`;
    }

    _toHSLAString()
    {
        let h = this.hsla[0].maxDecimals(2);
        let s = this.hsla[1].maxDecimals(2);
        let l = this.hsla[2].maxDecimals(2);
        return `hsla(${h}, ${s}%, ${l}%, ${this.alpha})`;
    }

    _componentToHexValue(value)
    {
        let hex = WI.Color._eightBitChannel(value).toString(16);
        if (hex.length === 1)
            hex = "0" + hex;
        return hex;
    }

    _rgbaToHSLA(rgba)
    {
        let hsla = WI.Color.rgb2hsl(...rgba);
        hsla.push(rgba[3]);
        return hsla;
    }

    _hslaToRGBA(hsla)
    {
        let rgba = WI.Color.hsl2rgb(...hsla);
        rgba.push(hsla[3]);
        return rgba;
    }
};

WI.Color.Format = {
    Original: "color-format-original",
    Keyword: "color-format-keyword",
    HEX: "color-format-hex",
    ShortHEX: "color-format-short-hex",
    HEXAlpha: "color-format-hex-alpha",
    ShortHEXAlpha: "color-format-short-hex-alpha",
    RGB: "color-format-rgb",
    RGBA: "color-format-rgba",
    HSL: "color-format-hsl",
    HSLA: "color-format-hsla"
};

WI.Color.FunctionNames = new Set([
    "rgb",
    "rgba",
    "hsl",
    "hsla",
]);

WI.Color.Keywords = {
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
    "darkgrey": [169, 169, 169],
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
    "darkslategrey": [47, 79, 79],
    "darkturquoise": [0, 206, 209],
    "darkviolet": [148, 0, 211],
    "deeppink": [255, 20, 147],
    "deepskyblue": [0, 191, 255],
    "dimgray": [105, 105, 105],
    "dimgrey": [105, 105, 105],
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
    "grey": [128, 128, 128],
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
    "lightgray": [211, 211, 211],
    "lightgreen": [144, 238, 144],
    "lightgrey": [211, 211, 211],
    "lightpink": [255, 182, 193],
    "lightsalmon": [255, 160, 122],
    "lightseagreen": [32, 178, 170],
    "lightskyblue": [135, 206, 250],
    "lightslategray": [119, 136, 153],
    "lightslategrey": [119, 136, 153],
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
    "rebeccapurple": [102, 51, 153],
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
    "slategrey": [112, 128, 144],
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

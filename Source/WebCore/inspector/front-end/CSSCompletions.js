/*
 * Copyright (C) 2010 Nikita Vasilyev. All rights reserved.
 * Copyright (C) 2010 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @param {Array.<CSSAgent.CSSPropertyInfo|string>} properties
 */
WebInspector.CSSCompletions = function(properties, acceptEmptyPrefix)
{
    this._values = [];
    this._longhands = {};
    this._shorthands = {};
    for (var i = 0; i < properties.length; ++i) {
        var property = properties[i];
        if (typeof property === "string") {
            this._values.push(property);
            continue;
        }

        var propertyName = property.name;
        this._values.push(propertyName);

        var longhands = properties[i].longhands;
        if (longhands) {
            this._longhands[propertyName] = longhands;
            for (var j = 0; j < longhands.length; ++j) {
                var longhandName = longhands[j];
                var shorthands = this._shorthands[longhandName];
                if (!shorthands) {
                    shorthands = [];
                    this._shorthands[longhandName] = shorthands;
                }
                shorthands.push(propertyName);
            }
        }
    }
    this._values.sort();
    this._acceptEmptyPrefix = acceptEmptyPrefix;
}


/**
 * @type {WebInspector.CSSCompletions}
 */
WebInspector.CSSCompletions.cssPropertiesMetainfo = null;

WebInspector.CSSCompletions.requestCSSNameCompletions = function()
{
    function propertyNamesCallback(error, properties)
    {
        if (!error)
            WebInspector.CSSCompletions.cssPropertiesMetainfo = new WebInspector.CSSCompletions(properties, false);
    }
    CSSAgent.getSupportedCSSProperties(propertyNamesCallback);
}

WebInspector.CSSCompletions.cssPropertiesMetainfoKeySet = function()
{
    if (!WebInspector.CSSCompletions._cssPropertiesMetainfoKeySet)
        WebInspector.CSSCompletions._cssPropertiesMetainfoKeySet = WebInspector.CSSCompletions.cssPropertiesMetainfo.keySet();
    return WebInspector.CSSCompletions._cssPropertiesMetainfoKeySet;
}

// Weight of CSS properties based their usage on few popular websites https://gist.github.com/3751436
WebInspector.CSSCompletions.Weight = {
    "-webkit-animation": 1,
    "-webkit-animation-duration": 1,
    "-webkit-animation-iteration-count": 1,
    "-webkit-animation-name": 1,
    "-webkit-animation-timing-function": 1,
    "-webkit-appearance": 1,
    "-webkit-background-clip": 2,
    "-webkit-border-horizontal-spacing": 1,
    "-webkit-border-vertical-spacing": 1,
    "-webkit-box-shadow": 24,
    "-webkit-font-smoothing": 2,
    "-webkit-transform": 1,
    "-webkit-transition": 8,
    "-webkit-transition-delay": 7,
    "-webkit-transition-duration": 7,
    "-webkit-transition-property": 7,
    "-webkit-transition-timing-function": 6,
    "-webkit-user-select": 1,
    "background": 222,
    "background-attachment": 144,
    "background-clip": 143,
    "background-color": 222,
    "background-image": 201,
    "background-origin": 142,
    "background-size": 25,
    "border": 121,
    "border-bottom": 121,
    "border-bottom-color": 121,
    "border-bottom-left-radius": 50,
    "border-bottom-right-radius": 50,
    "border-bottom-style": 114,
    "border-bottom-width": 120,
    "border-collapse": 3,
    "border-left": 95,
    "border-left-color": 95,
    "border-left-style": 89,
    "border-left-width": 94,
    "border-radius": 50,
    "border-right": 93,
    "border-right-color": 93,
    "border-right-style": 88,
    "border-right-width": 93,
    "border-top": 111,
    "border-top-color": 111,
    "border-top-left-radius": 49,
    "border-top-right-radius": 49,
    "border-top-style": 104,
    "border-top-width": 109,
    "bottom": 16,
    "box-shadow": 25,
    "box-sizing": 2,
    "clear": 23,
    "color": 237,
    "cursor": 34,
    "direction": 4,
    "display": 210,
    "fill": 2,
    "filter": 1,
    "float": 105,
    "font": 174,
    "font-family": 25,
    "font-size": 174,
    "font-style": 9,
    "font-weight": 89,
    "height": 161,
    "left": 54,
    "letter-spacing": 3,
    "line-height": 75,
    "list-style": 17,
    "list-style-image": 8,
    "list-style-position": 8,
    "list-style-type": 17,
    "margin": 241,
    "margin-bottom": 226,
    "margin-left": 225,
    "margin-right": 213,
    "margin-top": 241,
    "max-height": 5,
    "max-width": 11,
    "min-height": 9,
    "min-width": 6,
    "opacity": 24,
    "outline": 10,
    "outline-color": 10,
    "outline-style": 10,
    "outline-width": 10,
    "overflow": 57,
    "overflow-x": 56,
    "overflow-y": 57,
    "padding": 216,
    "padding-bottom": 208,
    "padding-left": 216,
    "padding-right": 206,
    "padding-top": 216,
    "position": 136,
    "resize": 1,
    "right": 29,
    "stroke": 1,
    "stroke-width": 1,
    "table-layout": 1,
    "text-align": 66,
    "text-decoration": 53,
    "text-indent": 9,
    "text-overflow": 8,
    "text-shadow": 19,
    "text-transform": 5,
    "top": 71,
    "unicode-bidi": 1,
    "vertical-align": 37,
    "visibility": 11,
    "white-space": 24,
    "width": 255,
    "word-wrap": 6,
    "z-index": 32,
    "zoom": 10
};


WebInspector.CSSCompletions.prototype = {
    startsWith: function(prefix)
    {
        var firstIndex = this._firstIndexOfPrefix(prefix);
        if (firstIndex === -1)
            return [];

        var results = [];
        while (firstIndex < this._values.length && this._values[firstIndex].startsWith(prefix))
            results.push(this._values[firstIndex++]);
        return results;
    },

    /**
     * @param {Array.<string>} properties
     * @return {number}
     */
    mostUsedOf: function(properties)
    {
        var maxWeight = 0;
        var index = 0;
        for (var i = 0; i < properties.length; i++) {
            var weight = WebInspector.CSSCompletions.Weight[properties[i]];
            if (weight > maxWeight) {
                maxWeight = weight;
                index = i;
            }
        }
        return index;
    },

    _firstIndexOfPrefix: function(prefix)
    {
        if (!this._values.length)
            return -1;
        if (!prefix)
            return this._acceptEmptyPrefix ? 0 : -1;

        var maxIndex = this._values.length - 1;
        var minIndex = 0;
        var foundIndex;

        do {
            var middleIndex = (maxIndex + minIndex) >> 1;
            if (this._values[middleIndex].startsWith(prefix)) {
                foundIndex = middleIndex;
                break;
            }
            if (this._values[middleIndex] < prefix)
                minIndex = middleIndex + 1;
            else
                maxIndex = middleIndex - 1;
        } while (minIndex <= maxIndex);

        if (foundIndex === undefined)
            return -1;

        while (foundIndex && this._values[foundIndex - 1].startsWith(prefix))
            foundIndex--;

        return foundIndex;
    },

    keySet: function()
    {
        if (!this._keySet)
            this._keySet = this._values.keySet();
        return this._keySet;
    },

    next: function(str, prefix)
    {
        return this._closest(str, prefix, 1);
    },

    previous: function(str, prefix)
    {
        return this._closest(str, prefix, -1);
    },

    _closest: function(str, prefix, shift)
    {
        if (!str)
            return "";

        var index = this._values.indexOf(str);
        if (index === -1)
            return "";

        if (!prefix) {
            index = (index + this._values.length + shift) % this._values.length;
            return this._values[index];
        }

        var propertiesWithPrefix = this.startsWith(prefix);
        var j = propertiesWithPrefix.indexOf(str);
        j = (j + propertiesWithPrefix.length + shift) % propertiesWithPrefix.length;
        return propertiesWithPrefix[j];
    },

    /**
     * @param {string} shorthand
     * @return {?Array.<string>}
     */
    longhands: function(shorthand)
    {
        return this._longhands[shorthand];
    },

    /**
     * @param {string} longhand
     * @return {?Array.<string>}
     */
    shorthands: function(longhand)
    {
        return this._shorthands[longhand];
    }
}

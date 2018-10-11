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

var emDash = "\u2014";
var enDash = "\u2013";
var figureDash = "\u2012";
var ellipsis = "\u2026";
var zeroWidthSpace = "\u200b";
var multiplicationSign = "\u00d7";

Object.defineProperty(Object, "shallowCopy",
{
    value(object)
    {
        // Make a new object and copy all the key/values. The values are not copied.
        var copy = {};
        var keys = Object.keys(object);
        for (var i = 0; i < keys.length; ++i)
            copy[keys[i]] = object[keys[i]];
        return copy;
    }
});

Object.defineProperty(Object, "shallowEqual",
{
    value(a, b)
    {
        // Checks if two objects have the same top-level properties.

        if (!(a instanceof Object) || !(b instanceof Object))
            return false;

        if (a === b)
            return true;

        if (Array.shallowEqual(a, b))
            return true;

        if (a.constructor !== b.constructor)
            return false;

        let aKeys = Object.keys(a);
        let bKeys = Object.keys(b);
        if (aKeys.length !== bKeys.length)
            return false;

        for (let aKey of aKeys) {
            if (!(aKey in b))
                return false;

            let aValue = a[aKey];
            let bValue = b[aKey];
            if (aValue !== bValue && !Array.shallowEqual(aValue, bValue))
                return false;
        }

        return true;
    }
});

Object.defineProperty(Object.prototype, "valueForCaseInsensitiveKey",
{
    value(key)
    {
        if (this.hasOwnProperty(key))
            return this[key];

        var lowerCaseKey = key.toLowerCase();
        for (var currentKey in this) {
            if (currentKey.toLowerCase() === lowerCaseKey)
                return this[currentKey];
        }

        return undefined;
    }
});

Object.defineProperty(Map, "fromObject",
{
    value(object)
    {
        let map = new Map;
        for (let key in object)
            map.set(key, object[key]);
        return map;
    }
});

Object.defineProperty(Map.prototype, "take",
{
    value(key)
    {
        var deletedValue = this.get(key);
        this.delete(key);
        return deletedValue;
    }
});

Object.defineProperty(Node.prototype, "enclosingNodeOrSelfWithClass",
{
    value(className)
    {
        for (let node = this; node; node = node.parentElement) {
            if (node.nodeType === Node.ELEMENT_NODE && node.classList.contains(className))
                return node;
        }

        return null;
    }
});

Object.defineProperty(Node.prototype, "enclosingNodeOrSelfWithNodeNameInArray",
{
    value(nodeNames)
    {
        let upperCaseNodeNames = nodeNames.map((name) => name.toUpperCase());

        for (let node = this; node; node = node.parentElement) {
            for (let nodeName of upperCaseNodeNames) {
                if (node.nodeName === nodeName)
                    return node;
            }
        }

        return null;
    }
});

Object.defineProperty(Node.prototype, "enclosingNodeOrSelfWithNodeName",
{
    value(nodeName)
    {
        return this.enclosingNodeOrSelfWithNodeNameInArray([nodeName]);
    }
});

Object.defineProperty(Node.prototype, "traverseNextNode",
{
    value(stayWithin)
    {
        var node = this.firstChild;
        if (node)
            return node;

        if (stayWithin && this === stayWithin)
            return null;

        node = this.nextSibling;
        if (node)
            return node;

        node = this;
        while (node && !node.nextSibling && (!stayWithin || !node.parentNode || node.parentNode !== stayWithin))
            node = node.parentNode;
        if (!node)
            return null;

        return node.nextSibling;
    }
});

Object.defineProperty(Node.prototype, "traversePreviousNode",
{
    value(stayWithin)
    {
       if (stayWithin && this === stayWithin)
            return null;
        var node = this.previousSibling;
        while (node && node.lastChild)
            node = node.lastChild;
        if (node)
            return node;
        return this.parentNode;
    }
});


Object.defineProperty(Node.prototype, "rangeOfWord",
{
    value(offset, stopCharacters, stayWithinNode, direction)
    {
        var startNode;
        var startOffset = 0;
        var endNode;
        var endOffset = 0;

        if (!stayWithinNode)
            stayWithinNode = this;

        if (!direction || direction === "backward" || direction === "both") {
            var node = this;
            while (node) {
                if (node === stayWithinNode) {
                    if (!startNode)
                        startNode = stayWithinNode;
                    break;
                }

                if (node.nodeType === Node.TEXT_NODE) {
                    let start = node === this ? (offset - 1) : (node.nodeValue.length - 1);
                    for (var i = start; i >= 0; --i) {
                        if (stopCharacters.indexOf(node.nodeValue[i]) !== -1) {
                            startNode = node;
                            startOffset = i + 1;
                            break;
                        }
                    }
                }

                if (startNode)
                    break;

                node = node.traversePreviousNode(stayWithinNode);
            }

            if (!startNode) {
                startNode = stayWithinNode;
                startOffset = 0;
            }
        } else {
            startNode = this;
            startOffset = offset;
        }

        if (!direction || direction === "forward" || direction === "both") {
            node = this;
            while (node) {
                if (node === stayWithinNode) {
                    if (!endNode)
                        endNode = stayWithinNode;
                    break;
                }

                if (node.nodeType === Node.TEXT_NODE) {
                    let start = node === this ? offset : 0;
                    for (var i = start; i < node.nodeValue.length; ++i) {
                        if (stopCharacters.indexOf(node.nodeValue[i]) !== -1) {
                            endNode = node;
                            endOffset = i;
                            break;
                        }
                    }
                }

                if (endNode)
                    break;

                node = node.traverseNextNode(stayWithinNode);
            }

            if (!endNode) {
                endNode = stayWithinNode;
                endOffset = stayWithinNode.nodeType === Node.TEXT_NODE ? stayWithinNode.nodeValue.length : stayWithinNode.childNodes.length;
            }
        } else {
            endNode = this;
            endOffset = offset;
        }

        var result = this.ownerDocument.createRange();
        result.setStart(startNode, startOffset);
        result.setEnd(endNode, endOffset);

        return result;

    }
});

Object.defineProperty(Element.prototype, "realOffsetWidth",
{
    get()
    {
        return this.getBoundingClientRect().width;
    }
});

Object.defineProperty(Element.prototype, "realOffsetHeight",
{
    get()
    {
        return this.getBoundingClientRect().height;
    }
});

Object.defineProperty(Element.prototype, "totalOffsetLeft",
{
    get()
    {
        return this.getBoundingClientRect().left;
    }
});

Object.defineProperty(Element.prototype, "totalOffsetRight",
{
    get()
    {
        return this.getBoundingClientRect().right;
    }
});

Object.defineProperty(Element.prototype, "totalOffsetTop",
{
    get()
    {
        return this.getBoundingClientRect().top;
    }
});

Object.defineProperty(Element.prototype, "removeChildren",
{
    value()
    {
        // This has been tested to be the fastest removal method.
        if (this.firstChild)
            this.textContent = "";
    }
});

Object.defineProperty(Element.prototype, "isInsertionCaretInside",
{
    value()
    {
        var selection = window.getSelection();
        if (!selection.rangeCount || !selection.isCollapsed)
            return false;
        var selectionRange = selection.getRangeAt(0);
        return selectionRange.startContainer === this || this.contains(selectionRange.startContainer);
    }
});

Object.defineProperty(Element.prototype, "removeMatchingStyleClasses",
{
    value(classNameRegex)
    {
        var regex = new RegExp("(^|\\s+)" + classNameRegex + "($|\\s+)");
        if (regex.test(this.className))
            this.className = this.className.replace(regex, " ");
    }
});

Object.defineProperty(Element.prototype, "createChild",
{
    value(elementName, className)
    {
        var element = this.ownerDocument.createElement(elementName);
        if (className)
            element.className = className;
        this.appendChild(element);
        return element;
    }
});

Object.defineProperty(Element.prototype, "isScrolledToBottom",
{
    value()
    {
        // This code works only for 0-width border
        return this.scrollTop + this.clientHeight === this.scrollHeight;
    }
});

Object.defineProperty(Element.prototype, "recalculateStyles",
{
    value()
    {
        this.ownerDocument.defaultView.getComputedStyle(this);
    }
});

Object.defineProperty(DocumentFragment.prototype, "createChild",
{
    value: Element.prototype.createChild
});

Object.defineProperty(Event.prototype, "stop",
{
    value()
    {
        this.stopImmediatePropagation();
        this.preventDefault();
    }
});

Object.defineProperty(Array, "isTypedArray",
{
    value(array)
    {
        if (!array)
            return false;

        let constructor = array.constructor;
        return constructor === Int8Array
            || constructor === Int16Array
            || constructor === Int32Array
            || constructor === Uint8Array
            || constructor === Uint8ClampedArray
            || constructor === Uint16Array
            || constructor === Uint32Array
            || constructor === Float32Array
            || constructor === Float64Array;
    }
});

Object.defineProperty(Array, "shallowEqual",
{
    value(a, b)
    {
        function isArrayLike(x) {
            return Array.isArray(x) || Array.isTypedArray(x);
        }

        if (!isArrayLike(a) || !isArrayLike(b))
            return false;

        if (a === b)
            return true;

        let length = a.length;

        if (length !== b.length)
            return false;

        for (let i = 0; i < length; ++i) {
            if (a[i] === b[i])
                continue;

            if (!Object.shallowEqual(a[i], b[i]))
                return false;
        }

        return true;
    }
});

Object.defineProperty(Array.prototype, "lastValue",
{
    get()
    {
        if (!this.length)
            return undefined;
        return this[this.length - 1];
    }
});

Object.defineProperty(Array.prototype, "adjacencies",
{
    value: function*() {
        for (let i = 1; i < this.length; ++i)
            yield [this[i - 1], this[i]];
    }
});

Object.defineProperty(Array.prototype, "remove",
{
    value(value)
    {
        for (let i = 0; i < this.length; ++i) {
            if (this[i] === value) {
                this.splice(i, 1);
                return;
            }
        }
    }
});

Object.defineProperty(Array.prototype, "removeAll",
{
    value(value)
    {
        for (let i = this.length - 1; i >= 0; --i) {
            if (this[i] === value)
                this.splice(i, 1);
        }
    }
});

Object.defineProperty(Array.prototype, "toggleIncludes",
{
    value(value, force)
    {
        let exists = this.includes(value);
        if (exists === !!force)
            return;

        if (exists)
            this.remove(value);
        else
            this.push(value);
    }
});

Object.defineProperty(Array.prototype, "insertAtIndex",
{
    value(value, index)
    {
        this.splice(index, 0, value);
    }
});

Object.defineProperty(Array.prototype, "keySet",
{
    value()
    {
        let keys = Object.create(null);
        for (var i = 0; i < this.length; ++i)
            keys[this[i]] = true;
        return keys;
    }
});

Object.defineProperty(Array.prototype, "partition",
{
    value(callback)
    {
        let positive = [];
        let negative = [];
        for (let i = 0; i < this.length; ++i) {
            let value = this[i];
            if (callback(value))
                positive.push(value);
            else
                negative.push(value);
        }
        return [positive, negative];
    }
});

Object.defineProperty(String.prototype, "isLowerCase",
{
    value()
    {
        return String(this) === this.toLowerCase();
    }
});

Object.defineProperty(String.prototype, "isUpperCase",
{
    value()
    {
        return String(this) === this.toUpperCase();
    }
});

Object.defineProperty(String.prototype, "truncateMiddle",
{
    value(maxLength)
    {
        "use strict";

        if (this.length <= maxLength)
            return this;
        var leftHalf = maxLength >> 1;
        var rightHalf = maxLength - leftHalf - 1;
        return this.substr(0, leftHalf) + ellipsis + this.substr(this.length - rightHalf, rightHalf);
    }
});

Object.defineProperty(String.prototype, "truncateEnd",
{
    value(maxLength)
    {
        "use strict";

        if (this.length <= maxLength)
            return this;
        return this.substr(0, maxLength - 1) + ellipsis;
    }
});

Object.defineProperty(String.prototype, "truncate",
{
    value(maxLength)
    {
        "use strict";

        if (this.length <= maxLength)
            return this;

        let clipped = this.slice(0, maxLength);
        let indexOfLastWhitespace = clipped.search(/\s\S*$/);
        if (indexOfLastWhitespace > Math.floor(maxLength / 2))
            clipped = clipped.slice(0, indexOfLastWhitespace - 1);

        return clipped + ellipsis;
    }
});

Object.defineProperty(String.prototype, "collapseWhitespace",
{
    value()
    {
        return this.replace(/[\s\xA0]+/g, " ");
    }
});

Object.defineProperty(String.prototype, "removeWhitespace",
{
    value()
    {
        return this.replace(/[\s\xA0]+/g, "");
    }
});

Object.defineProperty(String.prototype, "escapeCharacters",
{
    value(charactersToEscape)
    {
        if (!charactersToEscape)
            return this.valueOf();

        let charactersToEscapeSet = new Set(charactersToEscape);

        let foundCharacter = false;
        for (let c of this) {
            if (!charactersToEscapeSet.has(c))
                continue;
            foundCharacter = true;
            break;
        }

        if (!foundCharacter)
            return this.valueOf();

        let result = "";
        for (let c of this) {
            if (charactersToEscapeSet.has(c))
                result += "\\";
            result += c;
        }

        return result.valueOf();
    }
});

Object.defineProperty(String.prototype, "escapeForRegExp",
{
    value()
    {
        return this.escapeCharacters("^[]{}()\\.$*+?|");
    }
});

Object.defineProperty(String.prototype, "capitalize",
{
    value()
    {
        return this.charAt(0).toUpperCase() + this.slice(1);
    }
});

Object.defineProperty(String.prototype, "extendedLocaleCompare",
{
    value(other)
    {
        return this.localeCompare(other, undefined, {numeric: true});
    }
});

Object.defineProperty(String, "tokenizeFormatString",
{
    value(format)
    {
        var tokens = [];
        var substitutionIndex = 0;

        function addStringToken(str)
        {
            tokens.push({type: "string", value: str});
        }

        function addSpecifierToken(specifier, precision, substitutionIndex)
        {
            tokens.push({type: "specifier", specifier, precision, substitutionIndex});
        }

        var index = 0;
        for (var precentIndex = format.indexOf("%", index); precentIndex !== -1; precentIndex = format.indexOf("%", index)) {
            addStringToken(format.substring(index, precentIndex));
            index = precentIndex + 1;

            if (format[index] === "%") {
                addStringToken("%");
                ++index;
                continue;
            }

            if (!isNaN(format[index])) {
                // The first character is a number, it might be a substitution index.
                var number = parseInt(format.substring(index), 10);
                while (!isNaN(format[index]))
                    ++index;

                // If the number is greater than zero and ends with a "$",
                // then this is a substitution index.
                if (number > 0 && format[index] === "$") {
                    substitutionIndex = (number - 1);
                    ++index;
                }
            }

            const defaultPrecision = 6;

            let precision = defaultPrecision;
            if (format[index] === ".") {
                // This is a precision specifier. If no digit follows the ".",
                // then use the default precision of six digits (ISO C99 specification).
                ++index;

                precision = parseInt(format.substring(index), 10);
                if (isNaN(precision))
                    precision = defaultPrecision;

                while (!isNaN(format[index]))
                    ++index;
            }

            addSpecifierToken(format[index], precision, substitutionIndex);

            ++substitutionIndex;
            ++index;
        }

        addStringToken(format.substring(index));

        return tokens;
    }
});

Object.defineProperty(String.prototype, "lineCount",
{
    get()
    {
        "use strict";

        let lineCount = 1;
        let index = 0;
        while (true) {
            index = this.indexOf("\n", index);
            if (index === -1)
                return lineCount;

            index += "\n".length;
            lineCount++;
        }
    }
});

Object.defineProperty(String.prototype, "lastLine",
{
    get()
    {
        "use strict";

        let index = this.lastIndexOf("\n");
        if (index === -1)
            return this;

        return this.slice(index + "\n".length);
    }
});

Object.defineProperty(String.prototype, "hash",
{
    get()
    {
        // Matches the wtf/Hasher.h (SuperFastHash) algorithm.

        // Arbitrary start value to avoid mapping all 0's to all 0's.
        const stringHashingStartValue = 0x9e3779b9;

        var result = stringHashingStartValue;
        var pendingCharacter = null;
        for (var i = 0; i < this.length; ++i) {
            var currentCharacter = this[i].charCodeAt(0);
            if (pendingCharacter === null) {
                pendingCharacter = currentCharacter;
                continue;
            }

            result += pendingCharacter;
            result = (result << 16) ^ ((currentCharacter << 11) ^ result);
            result += result >> 11;

            pendingCharacter = null;
        }

        // Handle the last character in odd length strings.
        if (pendingCharacter !== null) {
            result += pendingCharacter;
            result ^= result << 11;
            result += result >> 17;
        }

        // Force "avalanching" of final 31 bits.
        result ^= result << 3;
        result += result >> 5;
        result ^= result << 2;
        result += result >> 15;
        result ^= result << 10;

        // Prevent 0 and negative results.
        return (0xffffffff + result + 1).toString(36);
    }
});

Object.defineProperty(String, "standardFormatters",
{
    value: {
        d: function(substitution)
        {
            return parseInt(substitution).toLocaleString();
        },

        f: function(substitution, token)
        {
            let value = parseFloat(substitution);
            if (isNaN(value))
                return NaN;

            let options = {
                minimumFractionDigits: token.precision,
                maximumFractionDigits: token.precision,
                useGrouping: false
            };
            return value.toLocaleString(undefined, options);
        },

        s: function(substitution)
        {
            return substitution;
        }
    }
});

Object.defineProperty(String, "format",
{
    value(format, substitutions, formatters, initialValue, append)
    {
        if (!format || !substitutions || !substitutions.length)
            return {formattedResult: append(initialValue, format), unusedSubstitutions: substitutions};

        function prettyFunctionName()
        {
            return "String.format(\"" + format + "\", \"" + Array.from(substitutions).join("\", \"") + "\")";
        }

        function warn(msg)
        {
            console.warn(prettyFunctionName() + ": " + msg);
        }

        function error(msg)
        {
            console.error(prettyFunctionName() + ": " + msg);
        }

        var result = initialValue;
        var tokens = String.tokenizeFormatString(format);
        var usedSubstitutionIndexes = {};

        for (var i = 0; i < tokens.length; ++i) {
            var token = tokens[i];

            if (token.type === "string") {
                result = append(result, token.value);
                continue;
            }

            if (token.type !== "specifier") {
                error("Unknown token type \"" + token.type + "\" found.");
                continue;
            }

            if (token.substitutionIndex >= substitutions.length) {
                // If there are not enough substitutions for the current substitutionIndex
                // just output the format specifier literally and move on.
                error("not enough substitution arguments. Had " + substitutions.length + " but needed " + (token.substitutionIndex + 1) + ", so substitution was skipped.");
                result = append(result, "%" + (token.precision > -1 ? token.precision : "") + token.specifier);
                continue;
            }

            usedSubstitutionIndexes[token.substitutionIndex] = true;

            if (!(token.specifier in formatters)) {
                // Encountered an unsupported format character, treat as a string.
                warn("unsupported format character \u201C" + token.specifier + "\u201D. Treating as a string.");
                result = append(result, substitutions[token.substitutionIndex]);
                continue;
            }

            result = append(result, formatters[token.specifier](substitutions[token.substitutionIndex], token));
        }

        var unusedSubstitutions = [];
        for (var i = 0; i < substitutions.length; ++i) {
            if (i in usedSubstitutionIndexes)
                continue;
            unusedSubstitutions.push(substitutions[i]);
        }

        return {formattedResult: result, unusedSubstitutions};
    }
});

Object.defineProperty(String.prototype, "format",
{
    value()
    {
        return String.format(this, arguments, String.standardFormatters, "", function(a, b) { return a + b; }).formattedResult;
    }
});

Object.defineProperty(String.prototype, "insertWordBreakCharacters",
{
    value()
    {
        // Add zero width spaces after characters that are good to break after.
        // Otherwise a string with no spaces will not break and overflow its container.
        // This is mainly used on URL strings, so the characters are tailored for URLs.
        return this.replace(/([\/;:\)\]\}&?])/g, "$1\u200b");
    }
});

Object.defineProperty(String.prototype, "removeWordBreakCharacters",
{
    value()
    {
        // Undoes what insertWordBreakCharacters did.
        return this.replace(/\u200b/g, "");
    }
});

Object.defineProperty(String.prototype, "levenshteinDistance",
{
    value(s)
    {
        var m = this.length;
        var n = s.length;
        var d = new Array(m + 1);

        for (var i = 0; i <= m; ++i) {
            d[i] = new Array(n + 1);
            d[i][0] = i;
        }

        for (var j = 0; j <= n; ++j)
            d[0][j] = j;

        for (var j = 1; j <= n; ++j) {
            for (var i = 1; i <= m; ++i) {
                if (this[i - 1] === s[j - 1])
                    d[i][j] = d[i - 1][j - 1];
                else {
                    var deletion = d[i - 1][j] + 1;
                    var insertion = d[i][j - 1] + 1;
                    var substitution = d[i - 1][j - 1] + 1;
                    d[i][j] = Math.min(deletion, insertion, substitution);
                }
            }
        }

        return d[m][n];
    }
});

Object.defineProperty(String.prototype, "toCamelCase",
{
    value()
    {
        return this.toLowerCase().replace(/[^\w]+(\w)/g, (match, group) => group.toUpperCase());
    }
});

Object.defineProperty(String.prototype, "hasMatchingEscapedQuotes",
{
    value()
    {
        return /^\"(?:[^\"\\]|\\.)*\"$/.test(this) || /^\'(?:[^\'\\]|\\.)*\'$/.test(this);
    }
});

Object.defineProperty(Math, "roundTo",
{
    value(num, step)
    {
        return Math.round(num / step) * step;
    }
});

Object.defineProperty(Number, "constrain",
{
    value(num, min, max)
    {
        if (isNaN(num) || max < min)
            return min;

        if (num < min)
            num = min;
        else if (num > max)
            num = max;
        return num;
    }
});

Object.defineProperty(Number, "percentageString",
{
    value(fraction, precision = 1)
    {
        return fraction.toLocaleString(undefined, {minimumFractionDigits: precision, style: "percent"});
    }
});

Object.defineProperty(Number, "secondsToMillisecondsString",
{
    value(seconds, higherResolution)
    {
        let ms = seconds * 1000;

        if (higherResolution)
            return WI.UIString("%.2fms").format(ms);
        return WI.UIString("%.1fms").format(ms);
    }
});

Object.defineProperty(Number, "secondsToString",
{
    value(seconds, higherResolution)
    {
        let ms = seconds * 1000;
        if (!ms)
            return WI.UIString("%.0fms").format(0);

        const epsilon = 0.0001;

        if (Math.abs(ms) < (10 + epsilon)) {
            if (higherResolution)
                return WI.UIString("%.3fms").format(ms);
            return WI.UIString("%.2fms").format(ms);
        }

        if (Math.abs(ms) < (100 + epsilon)) {
            if (higherResolution)
                return WI.UIString("%.2fms").format(ms);
            return WI.UIString("%.1fms").format(ms);
        }

        if (Math.abs(ms) < (1000 + epsilon)) {
            if (higherResolution)
                return WI.UIString("%.1fms").format(ms);
            return WI.UIString("%.0fms").format(ms);
        }

        // Do not go over seconds when in high resolution mode.
        if (higherResolution || Math.abs(seconds) < 60)
            return WI.UIString("%.2fs").format(seconds);

        let minutes = seconds / 60;
        if (Math.abs(minutes) < 60)
            return WI.UIString("%.1fmin").format(minutes);

        let hours = minutes / 60;
        if (Math.abs(hours) < 24)
            return WI.UIString("%.1fhrs").format(hours);

        let days = hours / 24;
        return WI.UIString("%.1f days").format(days);
    }
});

Object.defineProperty(Number, "bytesToString",
{
    value(bytes, higherResolution)
    {
        if (higherResolution === undefined)
            higherResolution = true;

        if (Math.abs(bytes) < 1024)
            return WI.UIString("%.0f B").format(bytes);

        let kilobytes = bytes / 1024;
        if (Math.abs(kilobytes) < 1024) {
            if (higherResolution || Math.abs(kilobytes) < 10)
                return WI.UIString("%.2f KB").format(kilobytes);
            return WI.UIString("%.1f KB").format(kilobytes);
        }

        let megabytes = kilobytes / 1024;
        if (Math.abs(megabytes) < 1024) {
            if (higherResolution || Math.abs(megabytes) < 10)
                return WI.UIString("%.2f MB").format(megabytes);
            return WI.UIString("%.1f MB").format(megabytes);
        }

        let gigabytes = megabytes / 1024;
        if (higherResolution || Math.abs(gigabytes) < 10)
            return WI.UIString("%.2f GB").format(gigabytes);
        return WI.UIString("%.1f GB").format(gigabytes);
    }
});

Object.defineProperty(Number, "abbreviate",
{
    value(num)
    {
        if (num < 1000)
            return num.toLocaleString();

        if (num < 1000000)
            return WI.UIString("%.1fK").format(Math.round(num / 100) / 10);

        if (num < 1000000000)
            return WI.UIString("%.1fM").format(Math.round(num / 100000) / 10);

        return WI.UIString("%.1fB").format(Math.round(num / 100000000) / 10);
    }
});

Object.defineProperty(Number, "zeroPad",
{
    value(num, length)
    {
        let string = num.toLocaleString();
        return string.padStart(length, "0");
    },
});

Object.defineProperty(Number, "countDigits",
{
    value(num)
    {
        if (num === 0)
            return 1;

        num = Math.abs(num);
        return Math.floor(Math.log(num) * Math.LOG10E) + 1;
    }
});

Object.defineProperty(Number.prototype, "maxDecimals",
{
    value(decimals)
    {
        let power = 10 ** decimals;
        return Math.round(this * power) / power;
    }
});

Object.defineProperty(Uint32Array, "isLittleEndian",
{
    value()
    {
        if ("_isLittleEndian" in this)
            return this._isLittleEndian;

        var buffer = new ArrayBuffer(4);
        var longData = new Uint32Array(buffer);
        var data = new Uint8Array(buffer);

        longData[0] = 0x0a0b0c0d;

        this._isLittleEndian = data[0] === 0x0d && data[1] === 0x0c && data[2] === 0x0b && data[3] === 0x0a;

        return this._isLittleEndian;
    }
});

function isEmptyObject(object)
{
    for (var property in object)
        return false;
    return true;
}

function isEnterKey(event)
{
    // Check if this is an IME event.
    return event.keyCode !== 229 && event.keyIdentifier === "Enter";
}

function resolveDotsInPath(path)
{
    if (!path)
        return path;

    if (path.indexOf("./") === -1)
        return path;

    console.assert(path.charAt(0) === "/");

    var result = [];

    var components = path.split("/");
    for (var i = 0; i < components.length; ++i) {
        var component = components[i];

        // Skip over "./".
        if (component === ".")
            continue;

        // Rewind one component for "../".
        if (component === "..") {
            if (result.length === 1)
                continue;
            result.pop();
            continue;
        }

        result.push(component);
    }

    return result.join("/");
}

function parseMIMEType(fullMimeType)
{
    if (!fullMimeType)
        return {type: fullMimeType, boundary: null, encoding: null};

    var typeParts = fullMimeType.split(/\s*;\s*/);
    console.assert(typeParts.length >= 1);

    var type = typeParts[0];
    var boundary = null;
    var encoding = null;

    for (var i = 1; i < typeParts.length; ++i) {
        var subparts = typeParts[i].split(/\s*=\s*/);
        if (subparts.length !== 2)
            continue;

        if (subparts[0].toLowerCase() === "boundary")
            boundary = subparts[1];
        else if (subparts[0].toLowerCase() === "charset")
            encoding = subparts[1].replace("^\"|\"$", ""); // Trim quotes.
    }

    return {type, boundary: boundary || null, encoding: encoding || null};
}

function simpleGlobStringToRegExp(globString, regExpFlags)
{
    // Only supports "*" globs.

    if (!globString)
        return null;

    // Escape everything from String.prototype.escapeForRegExp except "*".
    var regexString = globString.escapeCharacters("^[]{}()\\.$+?|");

    // Unescape all doubly escaped backslashes in front of escaped asterisks.
    // So "\\*" will become "\*" again, undoing escapeCharacters escaping of "\".
    // This makes "\*" match a literal "*" instead of using the "*" for globbing.
    regexString = regexString.replace(/\\\\\*/g, "\\*");

    // The following regex doesn't match an asterisk that has a backslash in front.
    // It also catches consecutive asterisks so they collapse down when replaced.
    var unescapedAsteriskRegex = /(^|[^\\])\*+/g;
    if (unescapedAsteriskRegex.test(globString)) {
        // Replace all unescaped asterisks with ".*".
        regexString = regexString.replace(unescapedAsteriskRegex, "$1.*");

        // Match edge boundaries when there is an asterisk to better meet the expectations
        // of the user. When someone types "*.js" they don't expect "foo.json" to match. They
        // would only expect that if they type "*.js*". We use \b (instead of ^ and $) to allow
        // matches inside paths or URLs, so "ba*.js" will match "foo/bar.js" but not "boo/bbar.js".
        // When there isn't an asterisk the regexString is just a substring search.
        regexString = "\\b" + regexString + "\\b";
    }

    return new RegExp(regexString, regExpFlags);
}

Object.defineProperty(Array.prototype, "lowerBound",
{
    // Return index of the leftmost element that is equal or greater
    // than the specimen object. If there's no such element (i.e. all
    // elements are smaller than the specimen) returns array.length.
    // The function works for sorted array.
    value(object, comparator)
    {
        function defaultComparator(a, b)
        {
            return a - b;
        }
        comparator = comparator || defaultComparator;
        var l = 0;
        var r = this.length;
        while (l < r) {
            var m = (l + r) >> 1;
            if (comparator(object, this[m]) > 0)
                l = m + 1;
            else
                r = m;
        }
        return r;
    }
});

Object.defineProperty(Array.prototype, "upperBound",
{
    // Return index of the leftmost element that is greater
    // than the specimen object. If there's no such element (i.e. all
    // elements are smaller than the specimen) returns array.length.
    // The function works for sorted array.
    value(object, comparator)
    {
        function defaultComparator(a, b)
        {
            return a - b;
        }
        comparator = comparator || defaultComparator;
        var l = 0;
        var r = this.length;
        while (l < r) {
            var m = (l + r) >> 1;
            if (comparator(object, this[m]) >= 0)
                l = m + 1;
            else
                r = m;
        }
        return r;
    }
});

Object.defineProperty(Array.prototype, "binaryIndexOf",
{
    value(value, comparator)
    {
        var index = this.lowerBound(value, comparator);
        return index < this.length && comparator(value, this[index]) === 0 ? index : -1;
    }
});

Object.defineProperty(Promise, "delay",
{
    value(delay)
    {
        return new Promise((resolve) => setTimeout(resolve, delay || 0));
    }
});

(function() {
    // The `debounce` function lets you call any function on an object with a delay
    // and if the function keeps getting called, the delay gets reset. Since `debounce`
    // returns a Proxy, you can cache it and call multiple functions with the same delay.

    // Use: object.debounce(200).foo("Argument 1", "Argument 2")
    // Note: The last call's arguments get used for the delayed call.

    const debounceTimeoutSymbol = Symbol("debounce-timeout");
    const debounceSoonProxySymbol = Symbol("debounce-soon-proxy");

    Object.defineProperty(Object.prototype, "soon",
    {
        get()
        {
            if (!this[debounceSoonProxySymbol])
                this[debounceSoonProxySymbol] = this.debounce(0);
            return this[debounceSoonProxySymbol];
        }
    });

    Object.defineProperty(Object.prototype, "debounce",
    {
        value(delay)
        {
            console.assert(delay >= 0);

            return new Proxy(this, {
                get(target, property, receiver) {
                    return (...args) => {
                        let original = target[property];
                        console.assert(typeof original === "function");

                        if (original[debounceTimeoutSymbol])
                            clearTimeout(original[debounceTimeoutSymbol]);

                        let performWork = () => {
                            original[debounceTimeoutSymbol] = undefined;
                            original.apply(target, args);
                        };

                        original[debounceTimeoutSymbol] = setTimeout(performWork, delay);
                    };
                }
            });
        }
    });

    Object.defineProperty(Function.prototype, "cancelDebounce",
    {
        value()
        {
            if (!this[debounceTimeoutSymbol])
                return;

            clearTimeout(this[debounceTimeoutSymbol]);
            this[debounceTimeoutSymbol] = undefined;
        }
    });

    const requestAnimationFrameSymbol = Symbol("peform-on-animation-frame");
    const requestAnimationFrameProxySymbol = Symbol("perform-on-animation-frame-proxy");

    Object.defineProperty(Object.prototype, "onNextFrame",
    {
        get()
        {
            if (!this[requestAnimationFrameProxySymbol]) {
                this[requestAnimationFrameProxySymbol] = new Proxy(this, {
                    get(target, property, receiver) {
                        return (...args) => {
                            let original = target[property];
                            console.assert(typeof original === "function");

                            if (original[requestAnimationFrameSymbol])
                                return;

                            let performWork = () => {
                                original[requestAnimationFrameSymbol] = undefined;
                                original.apply(target, args);
                            };

                            original[requestAnimationFrameSymbol] = requestAnimationFrame(performWork);
                        };
                    }
                });
            }

            return this[requestAnimationFrameProxySymbol];
        }
    });

    const throttleTimeoutSymbol = Symbol("throttle-timeout");

    Object.defineProperty(Object.prototype, "throttle",
    {
        value(delay)
        {
            console.assert(delay >= 0);

            let lastFireTime = NaN;
            let mostRecentArguments = null;

            return new Proxy(this, {
                get(target, property, receiver) {
                    return (...args) => {
                        let original = target[property];
                        console.assert(typeof original === "function");
                        mostRecentArguments = args;

                        function performWork() {
                            lastFireTime = Date.now();
                            original[throttleTimeoutSymbol] = undefined;
                            original.apply(target, mostRecentArguments);
                        }

                        if (isNaN(lastFireTime)) {
                            performWork();
                            return;
                        }

                        let remaining = delay - (Date.now() - lastFireTime);
                        if (remaining <= 0) {
                            original.cancelThrottle();
                            performWork();
                            return;
                        }

                        if (!original[throttleTimeoutSymbol])
                            original[throttleTimeoutSymbol] = setTimeout(performWork, remaining);
                    };
                }
            });
        }
    });

    Object.defineProperty(Function.prototype, "cancelThrottle",
    {
        value()
        {
            if (!this[throttleTimeoutSymbol])
                return;

            clearTimeout(this[throttleTimeoutSymbol]);
            this[throttleTimeoutSymbol] = undefined;
        }
    });
})();

function appendWebInspectorSourceURL(string)
{
    if (string.includes("//# sourceURL"))
        return string;
    return "\n//# sourceURL=__WebInspectorInternal__\n" + string;
}

function appendWebInspectorConsoleEvaluationSourceURL(string)
{
    if (string.includes("//# sourceURL"))
        return string;
    return "\n//# sourceURL=__WebInspectorConsoleEvaluation__\n" + string;
}

function isWebInspectorInternalScript(url)
{
    return url === "__WebInspectorInternal__";
}

function isWebInspectorConsoleEvaluationScript(url)
{
    return url === "__WebInspectorConsoleEvaluation__";
}

function isWebKitInjectedScript(url)
{
    return url && url.startsWith("__InjectedScript_") && url.endsWith(".js");
}

function isWebKitInternalScript(url)
{
    if (isWebInspectorConsoleEvaluationScript(url))
        return false;

    if (isWebKitInjectedScript(url))
        return true;

    return url && url.startsWith("__Web") && url.endsWith("__");
}

function isFunctionStringNativeCode(str)
{
    return str.endsWith("{\n    [native code]\n}");
}

function isTextLikelyMinified(content)
{
    const autoFormatMaxCharactersToCheck = 5000;
    const autoFormatWhitespaceRatio = 0.2;

    let whitespaceScore = 0;
    let size = Math.min(autoFormatMaxCharactersToCheck, content.length);

    for (let i = 0; i < size; i++) {
        let char = content[i];

        if (char === " ")
            whitespaceScore++;
        else if (char === "\t")
            whitespaceScore += 4;
        else if (char === "\n")
            whitespaceScore += 8;
    }

    let ratio = whitespaceScore / size;
    return ratio < autoFormatWhitespaceRatio;
}

function doubleQuotedString(str)
{
    return "\"" + str.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"";
}

function insertionIndexForObjectInListSortedByFunction(object, list, comparator, insertionIndexAfter)
{
    if (insertionIndexAfter) {
        return list.upperBound(object, comparator);
    } else {
        return list.lowerBound(object, comparator);
    }
}

function insertObjectIntoSortedArray(object, array, comparator)
{
    array.splice(insertionIndexForObjectInListSortedByFunction(object, array, comparator), 0, object);
}

function decodeBase64ToBlob(base64Data, mimeType)
{
    mimeType = mimeType || "";

    const sliceSize = 1024;
    var byteCharacters = atob(base64Data);
    var bytesLength = byteCharacters.length;
    var slicesCount = Math.ceil(bytesLength / sliceSize);
    var byteArrays = new Array(slicesCount);

    for (var sliceIndex = 0; sliceIndex < slicesCount; ++sliceIndex) {
        var begin = sliceIndex * sliceSize;
        var end = Math.min(begin + sliceSize, bytesLength);

        var bytes = new Array(end - begin);
        for (var offset = begin, i = 0; offset < end; ++i, ++offset)
            bytes[i] = byteCharacters[offset].charCodeAt(0);

        byteArrays[sliceIndex] = new Uint8Array(bytes);
    }

    return new Blob(byteArrays, {type: mimeType});
}

function textToBlob(text, mimeType)
{
    return new Blob([text], {type: mimeType});
}

function blobAsText(blob, callback)
{
    console.assert(blob instanceof Blob);
    let fileReader = new FileReader;
    fileReader.addEventListener("loadend", () => { callback(fileReader.result); });
    fileReader.readAsText(blob);
}

if (!window.handlePromiseException) {
    window.handlePromiseException = function handlePromiseException(error)
    {
        console.error("Uncaught exception in Promise", error);
    };
}

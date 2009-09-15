/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

Object.proxyType = function(objectProxy)
{
    if (objectProxy === null)
        return "null";

    var type = typeof objectProxy;
    if (type !== "object" && type !== "function")
        return type;

    return objectProxy.type;
}

Object.properties = function(obj)
{
    var properties = [];
    for (var prop in obj)
        properties.push(prop);
    return properties;
}

Object.sortedProperties = function(obj, sortFunc)
{
    return Object.properties(obj).sort(sortFunc);
}

Function.prototype.bind = function(thisObject)
{
    var func = this;
    var args = Array.prototype.slice.call(arguments, 1);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))) };
}

Node.prototype.rangeOfWord = function(offset, stopCharacters, stayWithinNode, direction)
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
                var start = (node === this ? (offset - 1) : (node.nodeValue.length - 1));
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

            node = node.traversePreviousNode(false, stayWithinNode);
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
                var start = (node === this ? offset : 0);
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

            node = node.traverseNextNode(false, stayWithinNode);
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

Element.prototype.removeStyleClass = function(className) 
{
    // Test for the simple case before using a RegExp.
    if (this.className === className) {
        this.className = "";
        return;
    }

    this.removeMatchingStyleClasses(className.escapeForRegExp());
}

Element.prototype.removeMatchingStyleClasses = function(classNameRegex)
{
    var regex = new RegExp("(^|\\s+)" + classNameRegex + "($|\\s+)");
    if (regex.test(this.className))
        this.className = this.className.replace(regex, " ");
}

Element.prototype.addStyleClass = function(className) 
{
    if (className && !this.hasStyleClass(className))
        this.className += (this.className.length ? " " + className : className);
}

Element.prototype.hasStyleClass = function(className) 
{
    if (!className)
        return false;
    // Test for the simple case before using a RegExp.
    if (this.className === className)
        return true;
    var regex = new RegExp("(^|\\s)" + className.escapeForRegExp() + "($|\\s)");
    return regex.test(this.className);
}

Element.prototype.positionAt = function(x, y)
{
    this.style.left = x + "px";
    this.style.top = y + "px";
}

Node.prototype.enclosingNodeOrSelfWithNodeNameInArray = function(nameArray)
{
    for (var node = this; node && node !== this.ownerDocument; node = node.parentNode)
        for (var i = 0; i < nameArray.length; ++i)
            if (node.nodeName.toLowerCase() === nameArray[i].toLowerCase())
                return node;
    return null;
}

Node.prototype.enclosingNodeOrSelfWithNodeName = function(nodeName)
{
    return this.enclosingNodeOrSelfWithNodeNameInArray([nodeName]);
}

Node.prototype.enclosingNodeOrSelfWithClass = function(className)
{
    for (var node = this; node && node !== this.ownerDocument; node = node.parentNode)
        if (node.nodeType === Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;
    return null;
}

Node.prototype.enclosingNodeWithClass = function(className)
{
    if (!this.parentNode)
        return null;
    return this.parentNode.enclosingNodeOrSelfWithClass(className);
}

Element.prototype.query = function(query) 
{
    return this.ownerDocument.evaluate(query, this, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.isInsertionCaretInside = function()
{
    var selection = window.getSelection();
    if (!selection.rangeCount || !selection.isCollapsed)
        return false;
    var selectionRange = selection.getRangeAt(0);
    return selectionRange.startContainer === this || selectionRange.startContainer.isDescendant(this);
}

Element.prototype.__defineGetter__("totalOffsetLeft", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetLeft;
    return total;
});

Element.prototype.__defineGetter__("totalOffsetTop", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetTop;
    return total;
});

Element.prototype.offsetRelativeToWindow = function(targetWindow)
{
    var elementOffset = {x: 0, y: 0};
    var curElement = this;
    var curWindow = this.ownerDocument.defaultView;
    while (curWindow && curElement) {
        elementOffset.x += curElement.totalOffsetLeft;
        elementOffset.y += curElement.totalOffsetTop;
        if (curWindow === targetWindow)
            break;

        curElement = curWindow.frameElement;
        curWindow = curWindow.parent;
    }

    return elementOffset;
}

Element.prototype.firstChildSkippingWhitespace = firstChildSkippingWhitespace;
Element.prototype.lastChildSkippingWhitespace = lastChildSkippingWhitespace;

Node.prototype.isWhitespace = isNodeWhitespace;
Node.prototype.displayName = nodeDisplayName;
Node.prototype.isAncestor = function(node)
{
    return isAncestorNode(this, node);
};
Node.prototype.isDescendant = isDescendantNode;
Node.prototype.nextSiblingSkippingWhitespace = nextSiblingSkippingWhitespace;
Node.prototype.previousSiblingSkippingWhitespace = previousSiblingSkippingWhitespace;
Node.prototype.traverseNextNode = traverseNextNode;
Node.prototype.traversePreviousNode = traversePreviousNode;
Node.prototype.onlyTextChild = onlyTextChild;

String.prototype.hasSubstring = function(string, caseInsensitive)
{
    if (!caseInsensitive)
        return this.indexOf(string) !== -1;
    return this.match(new RegExp(string.escapeForRegExp(), "i"));
}

String.prototype.escapeCharacters = function(chars)
{
    var foundChar = false;
    for (var i = 0; i < chars.length; ++i) {
        if (this.indexOf(chars.charAt(i)) !== -1) {
            foundChar = true;
            break;
        }
    }

    if (!foundChar)
        return this;

    var result = "";
    for (var i = 0; i < this.length; ++i) {
        if (chars.indexOf(this.charAt(i)) !== -1)
            result += "\\";
        result += this.charAt(i);
    }

    return result;
}

String.prototype.escapeForRegExp = function()
{
    return this.escapeCharacters("^[]{}()\\.$*+?|");
}

String.prototype.escapeHTML = function()
{
    return this.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

String.prototype.collapseWhitespace = function()
{
    return this.replace(/[\s\xA0]+/g, " ");
}

String.prototype.trimLeadingWhitespace = function()
{
    return this.replace(/^[\s\xA0]+/g, "");
}

String.prototype.trimTrailingWhitespace = function()
{
    return this.replace(/[\s\xA0]+$/g, "");
}

String.prototype.trimWhitespace = function()
{
    return this.replace(/^[\s\xA0]+|[\s\xA0]+$/g, "");
}

String.prototype.trimURL = function(baseURLDomain)
{
    var result = this.replace(new RegExp("^http[s]?:\/\/", "i"), "");
    if (baseURLDomain)
        result = result.replace(new RegExp("^" + baseURLDomain.escapeForRegExp(), "i"), "");
    return result;
}

function isNodeWhitespace()
{
    if (!this || this.nodeType !== Node.TEXT_NODE)
        return false;
    if (!this.nodeValue.length)
        return true;
    return this.nodeValue.match(/^[\s\xA0]+$/);
}

function nodeDisplayName()
{
    if (!this)
        return "";

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            return "Document";

        case Node.ELEMENT_NODE:
            var name = "<" + this.nodeName.toLowerCase();

            if (this.hasAttributes()) {
                var value = this.getAttribute("id");
                if (value)
                    name += " id=\"" + value + "\"";
                value = this.getAttribute("class");
                if (value)
                    name += " class=\"" + value + "\"";
                if (this.nodeName.toLowerCase() === "a") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("href");
                    if (value)
                        name += " href=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "img") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "iframe") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "input") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("type");
                    if (value)
                        name += " type=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() === "form") {
                    value = this.getAttribute("action");
                    if (value)
                        name += " action=\"" + value + "\"";
                }
            }

            return name + ">";

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                return "(whitespace)";
            return "\"" + this.nodeValue + "\"";

        case Node.COMMENT_NODE:
            return "<!--" + this.nodeValue + "-->";
            
        case Node.DOCUMENT_TYPE_NODE:
            var docType = "<!DOCTYPE " + this.nodeName;
            if (this.publicId) {
                docType += " PUBLIC \"" + this.publicId + "\"";
                if (this.systemId)
                    docType += " \"" + this.systemId + "\"";
            } else if (this.systemId)
                docType += " SYSTEM \"" + this.systemId + "\"";
            if (this.internalSubset)
                docType += " [" + this.internalSubset + "]";
            return docType + ">";
    }

    return this.nodeName.toLowerCase().collapseWhitespace();
}

function isAncestorNode(ancestor, node)
{
    if (!node || !ancestor)
        return false;

    var currentNode = node.parentNode;
    while (currentNode) {
        if (ancestor === currentNode)
            return true;
        currentNode = currentNode.parentNode;
    }
    return false;
}

function isDescendantNode(descendant)
{
    return isAncestorNode(descendant, this);
}

function nextSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.nextSibling;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.nextSibling;
    return node;
}

function previousSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.previousSibling;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.previousSibling;
    return node;
}

function firstChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.firstChild;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = nextSiblingSkippingWhitespace.call(node);
    return node;
}

function lastChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.lastChild;
    while (node && node.nodeType === Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = previousSiblingSkippingWhitespace.call(node);
    return node;
}

function traverseNextNode(skipWhitespace, stayWithin)
{
    if (!this)
        return;

    var node = skipWhitespace ? firstChildSkippingWhitespace.call(this) : this.firstChild;
    if (node)
        return node;

    if (stayWithin && this === stayWithin)
        return null;

    node = skipWhitespace ? nextSiblingSkippingWhitespace.call(this) : this.nextSibling;
    if (node)
        return node;

    node = this;
    while (node && !(skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling) && (!stayWithin || !node.parentNode || node.parentNode !== stayWithin))
        node = node.parentNode;
    if (!node)
        return null;

    return skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
}

function traversePreviousNode(skipWhitespace, stayWithin)
{
    if (!this)
        return;
    if (stayWithin && this === stayWithin)
        return null;
    var node = skipWhitespace ? previousSiblingSkippingWhitespace.call(this) : this.previousSibling;
    while (node && (skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild) )
        node = skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild;
    if (node)
        return node;
    return this.parentNode;
}

function onlyTextChild(ignoreWhitespace)
{
    if (!this)
        return null;

    var firstChild = ignoreWhitespace ? firstChildSkippingWhitespace.call(this) : this.firstChild;
    if (!firstChild || firstChild.nodeType !== Node.TEXT_NODE)
        return null;

    var sibling = ignoreWhitespace ? nextSiblingSkippingWhitespace.call(firstChild) : firstChild.nextSibling;
    return sibling ? null : firstChild;
}

function nodeTitleInfo(hasChildren, linkify)
{
    var info = {title: "", hasChildren: hasChildren};

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            info.title = "Document";
            break;

        case Node.ELEMENT_NODE:
            info.title = "<span class=\"webkit-html-tag\">&lt;" + this.nodeName.toLowerCase().escapeHTML();

            if (this.hasAttributes()) {
                for (var i = 0; i < this.attributes.length; ++i) {
                    var attr = this.attributes[i];
                    info.title += " <span class=\"webkit-html-attribute\"><span class=\"webkit-html-attribute-name\">" + attr.name.escapeHTML() + "</span>=&#8203;\"";

                    var value = attr.value;
                    if (linkify && (attr.name === "src" || attr.name === "href")) {
                        var value = value.replace(/([\/;:\)\]\}])/g, "$1\u200B");
                        info.title += linkify(attr.value, value, "webkit-html-attribute-value", this.nodeName.toLowerCase() == "a");
                    } else {
                        var value = value.escapeHTML();
                        value = value.replace(/([\/;:\)\]\}])/g, "$1&#8203;");
                        info.title += "<span class=\"webkit-html-attribute-value\">" + value + "</span>";
                    }
                    info.title += "\"</span>";
                }
            }
            info.title += "&gt;</span>&#8203;";

            // If this element only has a single child that is a text node,
            // just show that text and the closing tag inline rather than
            // create a subtree for them

            var textChild = onlyTextChild.call(this, Preferences.ignoreWhitespace);
            var showInlineText = textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength;

            if (showInlineText) {
                info.title += "<span class=\"webkit-html-text-node\">" + textChild.nodeValue.escapeHTML() + "</span>&#8203;<span class=\"webkit-html-tag\">&lt;/" + this.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
                info.hasChildren = false;
            }
            break;

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                info.title = "(whitespace)";
            else
                info.title = "\"<span class=\"webkit-html-text-node\">" + this.nodeValue.escapeHTML() + "</span>\"";
            break

        case Node.COMMENT_NODE:
            info.title = "<span class=\"webkit-html-comment\">&lt;!--" + this.nodeValue.escapeHTML() + "--&gt;</span>";
            break;

        case Node.DOCUMENT_TYPE_NODE:
            info.title = "<span class=\"webkit-html-doctype\">&lt;!DOCTYPE " + this.nodeName;
            if (this.publicId) {
                info.title += " PUBLIC \"" + this.publicId + "\"";
                if (this.systemId)
                    info.title += " \"" + this.systemId + "\"";
            } else if (this.systemId)
                info.title += " SYSTEM \"" + this.systemId + "\"";
            if (this.internalSubset)
                info.title += " [" + this.internalSubset + "]";
            info.title += "&gt;</span>";
            break;
        default:
            info.title = this.nodeName.toLowerCase().collapseWhitespace().escapeHTML();
    }

    return info;
}

function getDocumentForNode(node) {
    return node.nodeType == Node.DOCUMENT_NODE ? node : node.ownerDocument;
}

function parentNode(node) {
    return node.parentNode;
}

Number.secondsToString = function(seconds, formatterFunction, higherResolution)
{
    if (!formatterFunction)
        formatterFunction = String.sprintf;

    var ms = seconds * 1000;
    if (higherResolution && ms < 1000)
        return formatterFunction("%.3fms", ms);
    else if (ms < 1000)
        return formatterFunction("%.0fms", ms);

    if (seconds < 60)
        return formatterFunction("%.2fs", seconds);

    var minutes = seconds / 60;
    if (minutes < 60)
        return formatterFunction("%.1fmin", minutes);

    var hours = minutes / 60;
    if (hours < 24)
        return formatterFunction("%.1fhrs", hours);

    var days = hours / 24;
    return formatterFunction("%.1f days", days);
}

Number.bytesToString = function(bytes, formatterFunction, higherResolution)
{
    if (!formatterFunction)
        formatterFunction = String.sprintf;
    if (typeof higherResolution === "undefined")
        higherResolution = true;

    if (bytes < 1024)
        return formatterFunction("%.0fB", bytes);

    var kilobytes = bytes / 1024;
    if (higherResolution && kilobytes < 1024)
        return formatterFunction("%.2fKB", kilobytes);
    else if (kilobytes < 1024)
        return formatterFunction("%.0fKB", kilobytes);

    var megabytes = kilobytes / 1024;
    if (higherResolution)
        return formatterFunction("%.3fMB", megabytes);
    else
        return formatterFunction("%.0fMB", megabytes);
}

Number.constrain = function(num, min, max)
{
    if (num < min)
        num = min;
    else if (num > max)
        num = max;
    return num;
}

HTMLTextAreaElement.prototype.moveCursorToEnd = function()
{
    var length = this.value.length;
    this.setSelectionRange(length, length);
}

Array.prototype.remove = function(value, onlyFirst)
{
    if (onlyFirst) {
        var index = this.indexOf(value);
        if (index !== -1)
            this.splice(index, 1);
        return;
    }

    var length = this.length;
    for (var i = 0; i < length; ++i) {
        if (this[i] === value)
            this.splice(i, 1);
    }
}

function insertionIndexForObjectInListSortedByFunction(anObject, aList, aFunction)
{
    // indexOf returns (-lowerBound - 1). Taking (-result - 1) works out to lowerBound.
    return (-indexOfObjectInListSortedByFunction(anObject, aList, aFunction) - 1);
}

function indexOfObjectInListSortedByFunction(anObject, aList, aFunction)
{
    var first = 0;
    var last = aList.length - 1;
    var floor = Math.floor;
    var mid, c;

    while (first <= last) {
        mid = floor((first + last) / 2);
        c = aFunction(anObject, aList[mid]);

        if (c > 0)
            first = mid + 1;
        else if (c < 0)
            last = mid - 1;
        else {
            // Return the first occurance of an item in the list.
            while (mid > 0 && aFunction(anObject, aList[mid - 1]) === 0)
                mid--;
            first = mid;
            break;
        }
    }

    // By returning 1 less than the negative lower search bound, we can reuse this function
    // for both indexOf and insertionIndexFor, with some simple arithmetic.
    return (-first - 1);
}

String.sprintf = function(format)
{
    return String.vsprintf(format, Array.prototype.slice.call(arguments, 1));
}

String.tokenizeFormatString = function(format)
{
    var tokens = [];
    var substitutionIndex = 0;

    function addStringToken(str)
    {
        tokens.push({ type: "string", value: str });
    }

    function addSpecifierToken(specifier, precision, substitutionIndex)
    {
        tokens.push({ type: "specifier", specifier: specifier, precision: precision, substitutionIndex: substitutionIndex });
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
            var number = parseInt(format.substring(index));
            while (!isNaN(format[index]))
                ++index;
            // If the number is greater than zero and ends with a "$",
            // then this is a substitution index.
            if (number > 0 && format[index] === "$") {
                substitutionIndex = (number - 1);
                ++index;
            }
        }

        var precision = -1;
        if (format[index] === ".") {
            // This is a precision specifier. If no digit follows the ".",
            // then the precision should be zero.
            ++index;
            precision = parseInt(format.substring(index));
            if (isNaN(precision))
                precision = 0;
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

String.standardFormatters = {
    d: function(substitution)
    {
        substitution = parseInt(substitution);
        return !isNaN(substitution) ? substitution : 0;
    },

    f: function(substitution, token)
    {
        substitution = parseFloat(substitution);
        if (substitution && token.precision > -1)
            substitution = substitution.toFixed(token.precision);
        return !isNaN(substitution) ? substitution : (token.precision > -1 ? Number(0).toFixed(token.precision) : 0);
    },

    s: function(substitution)
    {
        return substitution;
    },
};

String.vsprintf = function(format, substitutions)
{
    return String.format(format, substitutions, String.standardFormatters, "", function(a, b) { return a + b; }).formattedResult;
}

String.format = function(format, substitutions, formatters, initialValue, append)
{
    if (!format || !substitutions || !substitutions.length)
        return { formattedResult: append(initialValue, format), unusedSubstitutions: substitutions };

    function prettyFunctionName()
    {
        return "String.format(\"" + format + "\", \"" + substitutions.join("\", \"") + "\")";
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

    return { formattedResult: result, unusedSubstitutions: unusedSubstitutions };
}

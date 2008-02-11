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

Object.type = function(obj)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function")
        return type;

    if (obj instanceof String)
        return "string";
    if (obj instanceof Array)
        return "array";
    if (obj instanceof Boolean)
        return "boolean";
    if (obj instanceof Number)
        return "number";
    if (obj instanceof Date)
        return "date";
    if (obj instanceof RegExp)
        return "regexp";
    if (obj instanceof Error)
        return "error";
    return type;
}

Object.describe = function(obj, abbreviated)
{
    switch (Object.type(obj)) {
    case "object":
        return Object.prototype.toString.call(obj).replace(/^\[object (.*)\]$/i, "$1");
    case "array":
        return "[" + obj.toString() + "]";
    case "string":
        if (obj.length > 100)
            return "\"" + obj.substring(0, 100) + "\u2026\"";
        return "\"" + obj + "\"";
    case "function":
        var objectText = String(obj);
        if (!/^function /.test(objectText))
            objectText = (type2 == "object") ? type1 : type2;
        else if (abbreviated)
            objectText = /.*/.exec(obj)[0].replace(/ +$/g, "");
        return objectText;
    case "regexp":
        return String(obj).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
    default:
        return String(obj);
    }
}

Object.sortedProperties = function(obj)
{
    var properties = [];
    for (var prop in obj)
        properties.push(prop);
    properties.sort();
    return properties;
}

Function.prototype.bind = function(thisObject)
{
    var func = this;
    var args = Array.prototype.slice.call(arguments, 1);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))) };
}

Element.prototype.removeStyleClass = function(className) 
{
    // Test for the simple case before using a RegExp.
    if (this.className === className) {
        this.className = "";
        return;
    }

    var regex = new RegExp("(^|\\s+)" + className.escapeForRegExp() + "($|\\s+)");
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

Element.prototype.scrollToElement = function(element)
{
    if (!element || !this.isAncestor(element))
        return;

    var offsetTop = 0;
    var current = element
    while (current && current !== this) {
        offsetTop += current.offsetTop;
        current = current.offsetParent;
    }

    if (this.scrollTop > offsetTop)
        this.scrollTop = offsetTop;
    else if ((this.scrollTop + this.offsetHeight) < (offsetTop + element.offsetHeight))
        this.scrollTop = offsetTop - this.offsetHeight + element.offsetHeight;
}

Node.prototype.firstParentOrSelfWithNodeName = function(nodeName)
{
    for (var node = this; node && (node !== document); node = node.parentNode)
        if (node.nodeName.toLowerCase() === nodeName.toLowerCase())
            return node;
    return null;
}

Node.prototype.firstParentOrSelfWithClass = function(className) 
{
    for (var node = this; node && (node !== document); node = node.parentNode)
        if (node.nodeType === Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;
    return null;
}

Node.prototype.firstParentWithClass = function(className)
{
    if (!this.parentNode)
        return null;
    return this.parentNode.firstParentOrSelfWithClass(className);
}

Element.prototype.query = function(query) 
{
    return document.evaluate(query, this, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.firstChildSkippingWhitespace = firstChildSkippingWhitespace;
Element.prototype.lastChildSkippingWhitespace = lastChildSkippingWhitespace;

Node.prototype.isWhitespace = isNodeWhitespace;
Node.prototype.nodeTypeName = nodeTypeName;
Node.prototype.displayName = nodeDisplayName;
Node.prototype.contentPreview = nodeContentPreview;
Node.prototype.isAncestor = isAncestorNode;
Node.prototype.isDescendant = isDescendantNode;
Node.prototype.firstCommonAncestor = firstCommonNodeAncestor;
Node.prototype.nextSiblingSkippingWhitespace = nextSiblingSkippingWhitespace;
Node.prototype.previousSiblingSkippingWhitespace = previousSiblingSkippingWhitespace;
Node.prototype.traverseNextNode = traverseNextNode;
Node.prototype.traversePreviousNode = traversePreviousNode;
Node.prototype.onlyTextChild = onlyTextChild;
Node.prototype.titleInfo = nodeTitleInfo;

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

CSSStyleDeclaration.prototype.getShorthandValue = function(shorthandProperty)
{
    var value = this.getPropertyValue(shorthandProperty);
    if (!value) {
        // Some shorthands (like border) return a null value, so compute a shorthand value.
        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823 is fixed.

        var foundProperties = {};
        for (var i = 0; i < this.length; ++i) {
            var individualProperty = this[i];
            if (individualProperty in foundProperties || this.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;

            var individualValue = this.getPropertyValue(individualProperty);
            if (this.isPropertyImplicit(individualProperty) || individualValue === "initial")
                continue;

            foundProperties[individualProperty] = true;

            if (!value)
                value = "";
            else if (value.length)
                value += " ";
            value += individualValue;
        }
    }
    return value;
}

CSSStyleDeclaration.prototype.getShorthandPriority = function(shorthandProperty)
{
    var priority = this.getPropertyPriority(shorthandProperty);
    if (!priority) {
        for (var i = 0; i < this.length; ++i) {
            var individualProperty = this[i];
            if (this.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;
            priority = this.getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}

CSSStyleDeclaration.prototype.getLonghandProperties = function(shorthandProperty)
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < this.length; ++i) {
        var individualProperty = this[i];
        if (individualProperty in foundProperties || this.getPropertyShorthand(individualProperty) !== shorthandProperty)
            continue;
        foundProperties[individualProperty] = true;
        properties.push(individualProperty);
    }

    return properties;
}

CSSStyleDeclaration.prototype.getUniqueProperties = function()
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < this.length; ++i) {
        var property = this[i];
        if (property in foundProperties)
            continue;
        foundProperties[property] = true;
        properties.push(property);
    }

    return properties;
}

function isNodeWhitespace()
{
    if (!this || this.nodeType !== Node.TEXT_NODE)
        return false;
    if (!this.nodeValue.length)
        return true;
    return this.nodeValue.match(/^[\s\xA0]+$/);
}

function nodeTypeName()
{
    if (!this)
        return "(unknown)";

    switch (this.nodeType) {
        case Node.ELEMENT_NODE: return "Element";
        case Node.ATTRIBUTE_NODE: return "Attribute";
        case Node.TEXT_NODE: return "Text";
        case Node.CDATA_SECTION_NODE: return "Character Data";
        case Node.ENTITY_REFERENCE_NODE: return "Entity Reference";
        case Node.ENTITY_NODE: return "Entity";
        case Node.PROCESSING_INSTRUCTION_NODE: return "Processing Instruction";
        case Node.COMMENT_NODE: return "Comment";
        case Node.DOCUMENT_NODE: return "Document";
        case Node.DOCUMENT_TYPE_NODE: return "Document Type";
        case Node.DOCUMENT_FRAGMENT_NODE: return "Document Fragment";
        case Node.NOTATION_NODE: return "Notation";
    }

    return "(unknown)";
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
    }

    return this.nodeName.toLowerCase().collapseWhitespace();
}

function nodeContentPreview()
{
    if (!this || !this.hasChildNodes || !this.hasChildNodes())
        return "";

    var limit = 0;
    var preview = "";

    // always skip whitespace here
    var currentNode = traverseNextNode.call(this, true, this);
    while (currentNode) {
        if (currentNode.nodeType === Node.TEXT_NODE)
            preview += currentNode.nodeValue.escapeHTML();
        else
            preview += nodeDisplayName.call(currentNode).escapeHTML();

        currentNode = traverseNextNode.call(currentNode, true, this);

        if (++limit > 4) {
            preview += "&#x2026;"; // ellipsis
            break;
        }
    }

    return preview.collapseWhitespace();
}

function isAncestorNode(ancestor)
{
    if (!this || !ancestor)
        return false;

    var currentNode = ancestor.parentNode;
    while (currentNode) {
        if (this === currentNode)
            return true;
        currentNode = currentNode.parentNode;
    }

    return false;
}

function isDescendantNode(descendant)
{
    return isAncestorNode.call(descendant, this);
}

function firstCommonNodeAncestor(node)
{
    if (!this || !node)
        return;

    var node1 = this.parentNode;
    var node2 = node.parentNode;

    if ((!node1 || !node2) || node1 !== node2)
        return null;

    while (node1 && node2) {
        if (!node1.parentNode || !node2.parentNode)
            break;
        if (node1 !== node2)
            break;

        node1 = node1.parentNode;
        node2 = node2.parentNode;
    }

    return node1;
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

function traversePreviousNode(skipWhitespace)
{
    if (!this)
        return;
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
                    var value = attr.value.escapeHTML();
                    value = value.replace(/([\/;:\)\]\}])/g, "$1&#8203;");

                    info.title += " <span class=\"webkit-html-attribute-name\">" + attr.name.escapeHTML() + "</span>=&#8203;";

                    if (linkify && (attr.name === "src" || attr.name === "href"))
                        info.title += linkify(attr.value, value, "webkit-html-attribute-value", this.nodeName.toLowerCase() == "a");
                    else
                        info.title += "<span class=\"webkit-html-attribute-value\">\"" + value + "\"</span>";
                }
            }
            info.title += "&gt;</span>&#8203;";

            // If this element only has a single child that is a text node,
            // just show that text and the closing tag inline rather than
            // create a subtree for them

            var textChild = onlyTextChild.call(this, Preferences.ignoreWhitespace);
            var showInlineText = textChild && textChild.textContent.length < Preferences.maxInlineTextChildLength;

            if (showInlineText) {
                info.title += textChild.nodeValue.escapeHTML() + "&#8203;<span class=\"webkit-html-tag\">&lt;/" + this.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
                info.hasChildren = false;
            }
            break;

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                info.title = "(whitespace)";
            else
                info.title = "\"" + this.nodeValue.escapeHTML() + "\"";
            break

        case Node.COMMENT_NODE:
            info.title = "<span class=\"webkit-html-comment\">&lt;!--" + this.nodeValue.escapeHTML() + "--&gt;</span>";
            break;

        default:
            info.title = this.nodeName.toLowerCase().collapseWhitespace().escapeHTML();
    }

    return info;
}

Number.secondsToString = function(seconds)
{
    var ms = seconds * 1000;
    if (ms < 1000)
        return Math.round(ms) + "ms";

    if (seconds < 60)
        return (Math.round(seconds * 100) / 100) + "s";

    var minutes = seconds / 60;
    if (minutes < 60)
        return (Math.round(minutes * 10) / 10) + "min";

    var hours = minutes / 60;
    if (hours < 24)
        return (Math.round(hours * 10) / 10) + "hrs";

    var days = hours / 24;
    return (Math.round(days * 10) / 10) + " days";
}

Number.bytesToString = function(bytes)
{
    if (bytes < 1024)
        return bytes + "B";

    var kilobytes = bytes / 1024;
    if (kilobytes < 1024)
        return (Math.round(kilobytes * 100) / 100) + "KB";

    var megabytes = kilobytes / 1024;
    return (Math.round(megabytes * 1000) / 1000) + "MB";
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

String.sprintf = function(format)
{
    return String.vsprintf(format, Array.prototype.slice.call(arguments, 1));
}

String.vsprintf = function(format, substitutions)
{
    if (!format || !substitutions || !substitutions.length)
        return format;

    var result = "";
    var substitutionIndex = 0;

    var index = 0;
    for (var precentIndex = format.indexOf("%", index); precentIndex !== -1; precentIndex = format.indexOf("%", index)) {
        result += format.substring(index, precentIndex);
        index = precentIndex + 1;

        if (format[index] === "%") {
            result += "%";
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

        if (substitutionIndex >= substitutions.length) {
            // If there are not enough substitutions for the current substitutionIndex
            // just output the format specifier literally and move on.
            console.error("String.vsprintf(\"" + format + "\", \"" + substitutions.join("\", \"") + "\"): not enough substitution arguments. Had " + substitutions.length + " but needed " + (substitutionIndex + 1) + ", so substitution was skipped.");
            index = precentIndex + 1;
            result += "%";
            continue;
        }

        switch (format[index]) {
        case "d":
            var substitution = parseInt(substitutions[substitutionIndex]);
            result += (!isNaN(substitution) ? substitution : 0);
            break;
        case "f":
            var substitution = parseFloat(substitutions[substitutionIndex]);
            if (substitution && precision > -1)
                substitution = substitution.toFixed(precision);
            result += (!isNaN(substitution) ? substitution : (precision > -1 ? Number(0).toFixed(precision) : 0));
            break;
        default:
            // Encountered an unsupported format character, treat as a string.
            console.warn("String.vsprintf(\"" + format + "\", \"" + substitutions.join("\", \"") + "\"): unsupported format character \u201C" + format[index] + "\u201D. Treating as a string.");
            // Fall through to treat this like a string.
        case "s":
            result += substitutions[substitutionIndex];
            break;
        }

        ++substitutionIndex;
        ++index;
    }

    result += format.substring(index);

    return result;
}
